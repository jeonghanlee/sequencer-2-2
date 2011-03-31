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
#include "seq.h"

/* #define DEBUG errlogPrintf */
#define DEBUG nothing

static void ss_entry(void *arg);
static void seq_clearDelay(SSCB *,STATE *);
static boolean seq_getTimeout(SSCB *, double *);

/*
 * sequencer() - Sequencer main thread entry point.
 */
void sequencer (void *arg)	/* ptr to original (global) state program table */
{
	SPROG		*sp = (SPROG *)arg;
	unsigned	nss;
	size_t		threadLen;
	char		threadName[THREAD_NAME_SIZE+10];

	/* Get this thread's id */
	sp->ss->threadId = epicsThreadGetIdSelf();

	/* Add the program to the state program list
	   and if necessary create pvSys */
	seqAddProg(sp);

	if (!sp->pvSys)
	{
		sp->die = TRUE;
		goto exit;
	}

	/* Note that the program init, entry, and exit functions
	   get the global var buffer sp->var passed,
	   not the state set local one, even in safe mode. */
	/* TODO: document this */

	/* Call sequencer init function to initialize variables. */
	sp->initFunc(sp->var);

	/* Initialize state set variables. In safe mode, copy variable
	   block to state set buffers. Must do all this before connecting. */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB	*ss = sp->ss + nss;

		if (sp->options & OPT_SAFE)
			memcpy(ss->var, sp->var, sp->varSize);
	}

	/* Attach to PV system */
	pvSysAttach(sp->pvSys);

	/* Initiate connect & monitor requests to database channels, waiting
	   for all connections to be established if the option is set. */
	if (seq_connect(sp, ((sp->options & OPT_CONN) != 0)) != pvStatOK)
		goto exit;

	/* Call program entry function if defined. */
	if (sp->entryFunc) sp->entryFunc(sp->ss, sp->var);

	/* Create each additional state set task (additional state set thread
	   names are derived from the first ss) */
	epicsThreadGetName(sp->ss->threadId, threadName, sizeof(threadName));
	threadLen = strlen(threadName);
	for (nss = 1; nss < sp->numSS; nss++)
	{
		SSCB		*ss = sp->ss + nss;
		epicsThreadId	tid;

		/* Form thread name from program name + state set number */
		sprintf(threadName+threadLen, "_%d", nss);

		/* Spawn the task */
		tid = epicsThreadCreate(
			threadName,			/* thread name */
			sp->threadPriority,		/* priority */
			sp->stackSize,			/* stack size */
			ss_entry,			/* entry point */
			ss);				/* parameter */

		DEBUG("Spawning additional state set thread %p: \"%s\"\n", tid, threadName);
	}

	/* First state set jumps directly to entry point */
	ss_entry(sp->ss);

	/* Call program exit function if defined */
	if (sp->exitFunc) sp->exitFunc(sp->ss, sp->var);

	DEBUG("   Wait for other state sets to exit\n");
	for (nss = 1; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;
		epicsEventMustWait(ss->dead);
	}

exit:
	DEBUG("   Disconnect all channels\n");
	seq_disconnect(sp);
	DEBUG("   Remove program instance from list\n");
	seqDelProg(sp);

	printf("Instance %d of sequencer program \"%s\" terminated\n",
		sp->instance, sp->progName);

	/* Free all allocated memory */
	seq_free(sp);
}

/*
 * ss_read_buffer_static() - static version of ss_read_buffer.
 * This is to enable inlining in the for loop in ss_read_all_buffer.
 */
static void ss_read_buffer_static(SSCB *ss, CHAN *ch)
{
	char *val = valPtr(ch,ss);
	char *buf = bufPtr(ch);
	ptrdiff_t nch = chNum(ch);
	size_t var_size = ch->type->size * ch->count;

	if (!ss->dirty[nch])
		return;

	epicsMutexMustLock(ch->varLock);

	DEBUG("ss %s: before read %s", ss->ssName, ch->varName);
	print_channel_value(DEBUG, ch, val);

	memcpy(val, buf, var_size);
	if (ch->dbch) {
		int nss = (int)ssNum(ss);
		/* structure copy */
		ch->dbch->ssMetaData[nss] = ch->dbch->metaData;
	}

	DEBUG("ss %s: after read %s", ss->ssName, ch->varName);
	print_channel_value(DEBUG, ch, val);

	ss->dirty[nch] = FALSE;

	epicsMutexUnlock(ch->varLock);
}

/*
 * ss_read_buffer() - Copy value and meta data
 * from shared buffer to state set local buffer
 * and reset corresponding dirty flag.
 */
void ss_read_buffer(SSCB *ss, CHAN *ch)
{
	return ss_read_buffer_static(ss, ch);
}

/*
 * ss_read_all_buffer() - Call ss_read_buffer_static
 * for all channels.
 */
static void ss_read_all_buffer(SPROG *sp, SSCB *ss)
{
	unsigned nch;

	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN *ch = sp->chan + nch;
		/* Call static version so it gets inlined */
		ss_read_buffer_static(ss, ch);
	}
}

/*
 * ss_write_buffer() - Copy given value and meta data
 * to shared buffer and set dirty flag for each state set.
 */
void ss_write_buffer(CHAN *ch, void *val, PVMETA *meta)
{
	SPROG *sp = ch->sprog;
	char *buf = bufPtr(ch);	/* shared buffer */
	size_t var_size = ch->type->size * ch->count;
	ptrdiff_t nch = chNum(ch);
	unsigned nss;

	epicsMutexMustLock(ch->varLock);

	DEBUG("ss_write_buffer: before write %s", ch->varName);
	print_channel_value(DEBUG, ch, buf);

	memcpy(buf, val, var_size);
	if (ch->dbch && meta)
		/* structure copy */
		ch->dbch->metaData = *meta;

	DEBUG("ss_write_buffer: after write %s", ch->varName);
	print_channel_value(DEBUG, ch, buf);

	if (sp->options & OPT_SAFE)
		for (nss = 0; nss < sp->numSS; nss++)
			sp->ss[nss].dirty[nch] = TRUE;

	epicsMutexUnlock(ch->varLock);
}

/*
 * ss_entry() - Thread entry point for all state sets.
 * Provides the main loop for state set processing.
 */
static void ss_entry(void *arg)
{
	SSCB		*ss = (SSCB *)arg;
	SPROG		*sp = ss->sprog;
	USER_VAR	*var;

	if (sp->options & OPT_SAFE)
		var = ss->var;
	else
		var = sp->var;

	/* Attach to PV system; was already done for the first state set */
	if (ss != sp->ss)
	{
		ss->threadId = epicsThreadGetIdSelf();
		pvSysAttach(sp->pvSys);
	}

	/* Register this thread with the EPICS watchdog (no callback func) */
	taskwdInsert(ss->threadId, 0, 0);

	/* In safe mode, update local var buffer with global one before
	   entering the event loop. Must do this using
	   ss_read_all_buffer since CA and other state sets could
	   already post events resp. pvPut. */
	if (sp->options & OPT_SAFE)
		ss_read_all_buffer(sp, ss);

	/* Initial state is the first one */
	ss->currentState = 0;
	ss->nextState = -1;
	ss->prevState = -1;

	DEBUG("ss %s: entering main loop\n", ss->ssName);

	/*
	 * ============= Main loop ==============
	 */
	while (TRUE)
	{
		boolean	ev_trig;
		int	transNum = 0;	/* highest prio trans. # triggered */

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

		/* Flush any outstanding DB requests */
		pvSysFlush(sp->pvSys);

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
				epicsEventWaitWithTimeout(ss->syncSemId, delay);
			else
				epicsEventWait(ss->syncSemId);

			/* Check whether we have been asked to exit */
			if (sp->die)
				goto exit;

			/* Copy dirty variable values from CA buffer
			 * to user (safe mode only).
			 */
			if (sp->options & OPT_SAFE)
				ss_read_all_buffer(sp, ss);

			/* Check state change conditions */
			ev_trig = st->eventFunc(ss, var,
				&transNum, &ss->nextState);

			/* Clear all event flags (old ef mode only) */
			if (ev_trig && !(sp->options & OPT_NEWEF))
			{
				unsigned i;
				for (i = 0; i < NWORDS(sp->numEvFlags); i++)
				{
					sp->evFlags[i] &= ~ss->mask[i];
				}
			}
		} while (!ev_trig);

		/* Execute the state change action */
		st->actionFunc(ss, var, transNum, &ss->nextState);

		/* If changing state, do exit actions */
		if (st->exitFunc && (ss->currentState != ss->nextState
			|| (st->options & OPT_DOEXITTOSELF)))
		{
			st->exitFunc(ss, var);
		}

		/* Change to next state */
		ss->prevState = ss->currentState;
		ss->currentState = ss->nextState;
	}

	/* Thread exit has been requested */
exit:
	taskwdRemove(ss->threadId);
	/* Declare ourselves dead */
	if (ss != sp->ss)
		epicsEventSignal(ss->dead);
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
static boolean seq_getTimeout(SSCB *ss, double *pdelay)
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
 * Delete all state set threads and do general clean-up.
 */
void epicsShareAPI seqStop(epicsThreadId tid)
{
	SPROG	*sp;

	/* Check that this is indeed a state program thread */
	sp = seqFindProg(tid);
	if (sp == NULL)
		return;

	DEBUG("Stop %s: sp=%p, tid=%p\n", sp->progName, sp,tid);

	/* Ask all state set threads to exit */
	DEBUG("   Asking state set threads to exit\n");
	sp->die = TRUE;

	/* Take care that we die even if waiting for initial connect */
	epicsEventSignal(sp->ready);

	DEBUG("   Waking up all state sets\n");
	seqWakeup (sp, 0);
}

void seqCreatePvSys(SPROG *sp)
{
	int debug = sp->debug;
	pvStat status = pvSysCreate(sp->pvSysName,
		max(0, debug-1), &sp->pvSys);
	if (status != pvStatOK)
		errlogPrintf("pvSysCreate(\"%s\") failure\n", sp->pvSysName);
}

/*
 * seqWakeup() -- wake up each state set that is waiting on this event
 * based on the current event mask; eventNum = 0 means wake all state sets.
 */
void seqWakeup(SPROG *sp, unsigned eventNum)
{
	unsigned nss;

	/* Check event number against mask for all state sets: */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;

		epicsMutexMustLock(sp->programLock);
		/* If event bit in mask is set, wake that state set */
		DEBUG("seqWakeup: eventNum=%d, mask=%u, state set=%d\n", eventNum, 
			ss->mask? *ss->mask : 0, (int)ssNum(ss));
		if (eventNum == 0 || 
			(ss->mask && bitTest(ss->mask, eventNum)))
		{
			DEBUG("seqWakeup: waking up state set=%d\n", (int)ssNum(ss));
			epicsEventSignal(ss->syncSemId); /* wake up ss thread */
		}
		epicsMutexUnlock(sp->programLock);
	}
}
