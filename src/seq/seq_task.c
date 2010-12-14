/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994
		The Regents of the University of California and
		the University of Chicago.
		Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)

	Thread creation and control for sequencer state sets.
***************************************************************************/

#define DECLARE_PV_SYS
#include "seq.h"

/* #define DEBUG errlogPrintf */
#define DEBUG nothing

#define varPtr(sp,ss)	(((sp)->options & OPT_SAFE) ? (ss)->var : (sp)->var)

/* Function declarations */
static boolean seq_waitConnect(SPROG *sp, SSCB *ss);
static void ss_entry(SSCB *ss);
static void ss_thread_init(SPROG *, SSCB *);
static void ss_thread_uninit(SPROG *, SSCB *,int);
static void seq_clearDelay(SSCB *,STATE *);
static int seq_getTimeout(SSCB *, double *);

/*
 * sequencer() - Sequencer main thread entry point.
 */
void sequencer (SPROG *sp)	/* ptr to original (global) state program table */
{
	SSCB		*ss = sp->ss;
	unsigned	nss;
	epicsThreadId	tid;
	size_t		threadLen;
	char		threadName[THREAD_NAME_SIZE+10];

	/* Retrieve info about this thread */
	sp->threadId = epicsThreadGetIdSelf();
	epicsThreadGetName(sp->threadId, threadName, sizeof(threadName));
	ss->threadId = sp->threadId;

	/* Add the program to the state program list */
	seqAddProg(sp);

	/* Note that the program init, entry, and exit functions
	   get the global var buffer sp->var passed,
	   not the state set local one, even in safe mode. */
	/* TODO: document this */

	/* Call sequencer init function to initialize variables. */
	sp->initFunc((USER_VAR *)sp->var);

	/* Initialize state set variables. In safe mode, copy variable
	   block to state set buffers.
	   Must do all this before connecting. */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB	*ss = sp->ss + nss;

		if (sp->options & OPT_SAFE)
			memcpy(ss->var, sp->var, sp->varSize);
	}

	/* Attach to PV context of pvSys creator (auxiliary thread) */
	pvSysAttach(pvSys);

	/* Initiate connect & monitor requests to database channels */
	seq_connect(sp);

	/* If "+c" option, wait for all channels to connect (a failure
	 * return means that we have been asked to exit) */
	if (sp->options & OPT_CONN)
	{
		if (!seq_waitConnect(sp, ss)) return;
	}

	/* Call program entry function if defined. */
	if (sp->entryFunc) sp->entryFunc(ss, (USER_VAR *)sp->var);

	/* Create each additional state-set task (additional state-set thread
	   names are derived from the first ss) */
	threadLen = strlen(threadName);
	for (nss = 1, ss = sp->ss + 1; nss < sp->numSS; nss++, ss++)
	{
		/* Form thread name from program name + state-set number */
		sprintf(threadName+threadLen, "_%d", nss);

		/* Spawn the task */
		tid = epicsThreadCreate(
			threadName,			/* thread name */
			sp->threadPriority,		/* priority */
			sp->stackSize,			/* stack size */
			(EPICSTHREADFUNC)ss_entry,	/* entry point */
			ss);				/* parameter */

		DEBUG("Spawning additional state set thread %p: \"%s\"\n", tid, threadName);
	}

	/* First state-set jumps directly to entry point */
	ss_entry(sp->ss);

	/* Call program exit function if defined */
	if (sp->exitFunc) sp->exitFunc(ss, (USER_VAR *)sp->var);
}

/*
 * ss_read_buffer() - Only used in safe mode.
 * Lock-free reading of variable buffer.
 * See also ss_write_buffer().
 */
static void ss_read_buffer(SSCB *ss)
{
	SPROG		*sp = ss->sprog;
	ptrdiff_t	ss_num = ss - sp->ss;
	unsigned	var;

	for (var = 0; var < sp->numChans; var++)
	{
		CHAN	*ch = sp->chan + var;
		char	*val = (char*)ss->var + ch->offset;
		char	*buf = (char*)sp->var + ch->offset;
		boolean *dirty = ch->dirty;
		size_t	var_size = ch->type->size * ch->dbCount;

		if (!dirty[ss_num])
			continue;
		do {
			dirty[ss_num] = FALSE;
			memcpy(val, buf, var_size);
		} while ((ch->wr_active || dirty[ss_num])
			&& (epicsThreadSleep(0),TRUE));
	}
}

/*
 * ss_write_buffer() - Only used in safe mode.
 * Lock-free writing of variable buffer.
 * See also ss_read_buffer().
 */
void ss_write_buffer(CHAN *ch, void *val)
{
	SPROG	*sp = ch->sprog;
	char	*buf = (char*)sp->var + ch->offset;
	boolean *dirty = ch->dirty;
	size_t	var_size = ch->type->size * ch->dbCount;
	unsigned ss_num;

	ch->wr_active = TRUE;
	memcpy(buf, val, var_size);
	for (ss_num = 0; ss_num < sp->numSS; ss_num++)
	{
		dirty[ss_num] = TRUE;
	}
	ch->wr_active = FALSE;
}

/*
 * ss_entry() - Thread entry point for all state-sets.
 * Provides the main loop for state-set processing.
 */
static void ss_entry(SSCB *ss)
{
	SPROG		*sp = ss->sprog;
	unsigned	nWords = (sp->numEvents + NBITS - 1) / NBITS;
	USER_VAR	*var = (USER_VAR *)varPtr(sp,ss);

	/* Initialize this state-set thread */
	ss_thread_init(sp, ss);

	/* Attach to PV context of pvSys creator (auxiliary thread); was
	   already done for the first state set */
	if (ss != sp->ss)
	{
		ss->threadId = epicsThreadGetIdSelf();
		pvSysAttach(pvSys);
	}


	/* Initial state is the first one */
	ss->currentState = 0;
	ss->nextState = -1;
	ss->prevState = -1;

	/*
	 * ============= Main loop ==============
	 */
	while (TRUE)
	{
		boolean ev_trig;

		/* Set state to current state */
		STATE *st = ss->states + ss->currentState;

		/* Set state set event mask to this state's event mask */
		ss->mask = st->eventMask;

		/* If we've changed state, do any entry actions. Also do these
		 * even if it's the same state if option to do so is enabled.
		 */
		if (st->entryFunc && (ss->prevState != ss->currentState
			|| (st->options & OPT_DOENTRYFROMSELF)))
		{
			st->entryFunc(ss, var);
		}

		seq_clearDelay(ss, st); /* Clear delay list */
		st->delayFunc(ss, var); /* Set up new delay list */

		/* Setting this semaphore here guarantees that a when() is
		 * always executed at least once when a state is first
		 * entered.
		 */
		epicsEventSignal(ss->syncSemId);

		/* Loop until an event is triggered, i.e. when() returns TRUE
		 */
		do {
			double delay = 0.0;

			/* Wake up on PV event, event flag, or expired delay */
			if (seq_getTimeout(ss, &delay) && delay > 0.0)
			{
				epicsEventWaitWithTimeout(ss->syncSemId, delay);
			}
			else
			{
				epicsEventWait(ss->syncSemId);
			}

			/* Check whether we have been asked to exit */
			if (epicsEventTryWait(ss->death1SemId) == epicsEventWaitOK)
			{
				goto exit;
			}

			/* Copy dirty variable values from CA buffer
			 * to user (safe mode only).
			 */
			if (sp->options & OPT_SAFE)
			{
				ss_read_buffer(ss);
			}

			/* Check state change conditions */
			ev_trig = st->eventFunc(ss, var,
				&ss->transNum, &ss->nextState);

			/* Clear all event flags (old ef mode only) */
			if (ev_trig && !(sp->options & OPT_NEWEF))
			{
				unsigned i;
				for (i = 0; i < nWords; i++)
				{
					sp->events[i] &= ~ss->mask[i];
				}
			}
		} while (!ev_trig);

		/* Execute the state change action */
		st->actionFunc(ss, var, ss->transNum, &ss->nextState);

		/* If changing state, do exit actions */
		if (st->exitFunc && (ss->currentState != ss->nextState
			|| (st->options & OPT_DOEXITTOSELF)))
		{
			st->exitFunc(ss, var);
		}

		/* Flush any outstanding DB requests */
		pvSysFlush(pvSys);

		/* Change to next state */
		ss->prevState = ss->currentState;
		ss->currentState = ss->nextState;
	}

	/* Thread exit has been requested */
exit:

	/* Uninitialize this state-set thread (phase 1) */
	ss_thread_uninit(sp, ss, 1);

	/* Pass control back (so all state-set threads can complete phase 1
	 * before embarking on phase 2) */
	epicsEventSignal(ss->death2SemId);

	/* Wait for request to perform uninitialization (phase 2) */
	epicsEventMustWait(ss->death3SemId);
	ss_thread_uninit(sp, ss, 2);

	/* Pass control back and die (i.e. exit) */
	epicsEventSignal(ss->death4SemId);
}

/* Initialize a state-set thread */
static void ss_thread_init(SPROG *sp, SSCB *ss)
{
	/* Get this thread's id */
	ss->threadId = epicsThreadGetIdSelf();

	/* Attach to PV context of pvSys creator (auxiliary thread); was
	   already done for the first state-set */
	if (sp->threadId != ss->threadId)
		pvSysAttach(pvSys);

	/* Register this thread with the EPICS watchdog (no callback func) */
	taskwdInsert(ss->threadId, 0, (void *)0);
}

/* Uninitialize a state-set thread */
static void ss_thread_uninit(SPROG *sp, SSCB *ss, int phase)
{
	/* Phase 1: if this is the first state-set, call user exit routine
	   and disconnect all channels */
	if (phase == 1 && ss->threadId == sp->threadId)
	{
	    USER_VAR *var = (USER_VAR *)varPtr(sp,ss);
	    DEBUG("   Call exit function\n");
	    sp->exitFunc(ss, var);

	    DEBUG("   Disconnect all channels\n");
	    seq_disconnect(sp);
	}

	/* Phase 2: unregister the thread with the EPICS watchdog */
	else if (phase == 2)
	{
	    DEBUG("   taskwdRemove(%p)\n", ss->threadId);
	    taskwdRemove(ss->threadId);
	}
}

/* Wait for all channels to connect */
static boolean seq_waitConnect(SPROG *sp, SSCB *ss)
{
	epicsStatus	status;
	double		delay;

	if (sp->numChans == 0)
		return TRUE;
	delay = 10.0; /* 10, 20, 30, 40, 40,... sec */
	while (1)
	{
		status = epicsEventWaitWithTimeout(
			ss->allFirstConnectAndMonitorSemId, delay);
		if(status==epicsEventWaitOK) break;
		if (delay < 40.0)
		{
			delay += 10.0;
			errlogPrintf("numMonitoredChans %u firstMonitorCount %u",
				sp->numMonitoredChans,sp->firstMonitorCount);
			errlogPrintf(" assignCount %u firstConnectCount %u\n",
				sp->assignCount,sp->firstConnectCount);
		}
		/* Check whether we have been asked to exit */
		if (epicsEventTryWait(ss->death1SemId) == epicsEventWaitOK)
			return FALSE;
	}
	return TRUE;
}

/*
 * seq_clearDelay() - clear the time delay list.
 */
static void seq_clearDelay(SSCB *ss, STATE *st)
{
	unsigned ndelay;

	/* On state change set time we entered this state; or if transition from
	 * same state if option to do so is on for this state.
	 */
	if ((ss->currentState != ss->prevState) ||
		!(st->options & OPT_NORESETTIMERS))
	{
		pvTimeGetCurrentDouble(&ss->timeEntered);
	}

	for (ndelay = 0; ndelay < ss->maxNumDelays; ndelay++)
	{
		ss->delay[ndelay] = 0;
	 	ss->delayExpired[ndelay] = FALSE;
	}

	ss->numDelays = 0;
}

/*
 * seq_getTimeout() - return time-out for pending on events.
 * Return whether to time out when waiting for events.
 * If yes, set *pdelay to the timout (in seconds).
 */
static int seq_getTimeout(SSCB *ss, double *pdelay)
{
	unsigned ndelay;
	boolean	do_timeout = FALSE;
	double	cur, delay;
	double	delayMin = 0;
	/* not really necessary to initialize delayMin,
	   but tell that to the compiler...
	 */

	if (ss->numDelays == 0)
		return do_timeout;
	/*
	 * Calculate the delay since this state was entered.
	 */
	pvTimeGetCurrentDouble(&cur);
	delay = cur - ss->timeEntered;
	/*
	 * Find the minimum delay among all unexpired timeouts if
	 * one exists, and set do_timeout in this case.
	 */
	for (ndelay = 0; ndelay < ss->numDelays; ndelay++)
	{
		double delayN;

		if (ss->delayExpired[ndelay])
			continue; /* skip if this delay entry already expired */
		delayN = ss->delay[ndelay];
		if (delay >= delayN)
		{	/* just expired */
			ss->delayExpired[ndelay] = TRUE; /* mark as expired */
			*pdelay = 0.0;
			return TRUE;
		}
		if (!do_timeout || delayN<=delayMin)
		{
			do_timeout = TRUE;
			delayMin = delayN;  /* this is the min. delay so far */
		}
	}
	if (do_timeout)
	{
		*pdelay = max(0.0, delayMin - delay);
	}
	return do_timeout;
}

/*
 * Delete all state-set threads and do general clean-up.
 */
void epicsShareAPI seqStop(epicsThreadId tid)
{
	SPROG		*sp;
	SSCB		*ss;
	unsigned	nss;

	/* Check that this is indeed a state program thread */
	sp = seqFindProg(tid);
	if (sp == NULL)
		return;

	DEBUG("Stop %s: sp=%p, tid=%p\n", sp->progName, sp,tid);

	/* Ask all state-set threads to exit (phase 1) */
	DEBUG("   Asking state-set threads to exit (phase 1):\n");
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		/* Just possibly hasn't started yet, so check... */
		if (ss->threadId == 0)
			continue;

		/* Ask the thread to exit */
		DEBUG("      tid=%p\n", ss->threadId);
		epicsEventSignal(ss->death1SemId);
	}

	/* Wake up all state-sets */
	DEBUG("   Waking up all state-sets\n");
	seqWakeup (sp, 0);

	/* Wait for them all to complete phase 1 of their deaths */
	DEBUG("   Waiting for state-set threads phase 1 death:\n");
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		if (ss->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(ss->death2SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 1 (ignored)\n",
				     ss->threadId, ss->ssName);
		}
		else
		{
			DEBUG("      tid=%p\n", ss->threadId);
		}
	}

	/* Ask all state-set threads to exit (phase 2) */
	DEBUG("   Asking state-set threads to exit (phase 2):\n");
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		if (ss->threadId == 0)
			continue;

		DEBUG("      tid=%p\n", ss->threadId);
		epicsEventSignal(ss->death3SemId);
	}

	/* Wait for them all to complete phase 2 of their deaths */
	DEBUG("   Waiting for state-set threads phase 2 death:\n");
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		if (ss->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(ss->death4SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 2 (ignored)\n",
				     ss->threadId, ss->ssName);
		}
		else
		{
			DEBUG("      tid=%p\n", ss->threadId);
		}
	}

	/* Remove the state program from the state program list */
	seqDelProg(sp);

	/* Delete state-set semaphores */
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		if (ss->allFirstConnectAndMonitorSemId != NULL)
			epicsEventDestroy(ss->allFirstConnectAndMonitorSemId);
		if (ss->syncSemId != NULL)
			epicsEventDestroy(ss->syncSemId);
		if (ss->getSemId != NULL)
			epicsEventDestroy(ss->getSemId);
		if (ss->death1SemId != NULL)
			epicsEventDestroy(ss->death1SemId);
		if (ss->death2SemId != NULL)
			epicsEventDestroy(ss->death2SemId);
		if (ss->death3SemId != NULL)
			epicsEventDestroy(ss->death3SemId);
		if (ss->death4SemId != NULL)
			epicsEventDestroy(ss->death4SemId);
	}

	/* Delete program-wide semaphores */
	epicsMutexDestroy(sp->programLock);

	/* Free all allocated memory */
	seqFree(sp);

	DEBUG("   Done\n");
}

/* seqFree()--free all allocated memory */
void seqFree(SPROG *sp)
{
	SSCB		*ss;
	CHAN		*ch;
	unsigned	nch;

	seqMacFree(sp);
	for (nch = 0; nch < sp->numChans; nch++)
	{
		ch = sp->chan + nch;

		if (ch->dbName != NULL)
			free(ch->dbName);
		if (ch->putSemId != NULL)
			epicsEventDestroy(ch->putSemId);
	}

	/* Free channel structures */
	free(sp->chan);

	ss = sp->ss;

	/* Free event words */
	free(sp->events);

	/* Free SSCBs */
	free(sp->ss);

	/* Free SPROG */
	free(sp);
}

/* 
 * Sequencer auxiliary thread -- loops on pvSysPend().
 */
void *seqAuxThread(void *tArgs)
{
	AUXARGS		*args = (AUXARGS *)tArgs;
	char		*pvSysName = args->pvSysName;
	long		debug = args->debug;
	int		status;

	/* Register this thread with the EPICS watchdog */
	taskwdInsert(epicsThreadGetIdSelf(), 0, 0);

	/* All state program threads will use a common PV context (subtract
	   1 from debug level for PV debugging) */
	status = pvSysCreate(pvSysName, debug>0?debug-1:0, &pvSys);
	if (status != pvStatOK)
	{
		errlogPrintf("seqAuxThread: pvSysCreate() %s failure: %s\n",
			pvSysName, pvSysGetMess(pvSys));
		seqAuxThreadId = (epicsThreadId) -1;
		return NULL;
	}
	seqAuxThreadId = epicsThreadGetIdSelf(); /* AFTER pvSysCreate() */

	/* This loop allows for check for connect/disconnect on PVs */
	for (;;)
	{
		pvSysPend(pvSys, 10.0, TRUE); /* returns every 10 sec. */
	}

	/* Return no result (never exit in any case) */
	return NULL;
}

