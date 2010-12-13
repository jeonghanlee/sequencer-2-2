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

#define varPtr(sp,ss)	(((sp)->options & OPT_SAFE) ? (ss)->pVar : (sp)->pVar)

/* Function declarations */
static boolean seq_waitConnect(SPROG *pSP, SSCB *pSS);
static void ss_entry(SSCB *pSS);
static void ss_thread_init(SPROG *, SSCB *);
static void ss_thread_uninit(SPROG *, SSCB *,int);
static void seq_clearDelay(SSCB *,STATE *);
static int seq_getTimeout(SSCB *, double *);

/*
 * sequencer() - Sequencer main thread entry point.
 */
void sequencer (SPROG *pSP)	/* ptr to original (global) state program table */
{
	SSCB		*pSS = pSP->pSS;
	unsigned	nss;
	epicsThreadId	tid;
	size_t		threadLen;
	char		threadName[THREAD_NAME_SIZE+10];

	/* Retrieve info about this thread */
	pSP->threadId = epicsThreadGetIdSelf();
	epicsThreadGetName(pSP->threadId, threadName, sizeof(threadName));
	pSS->threadId = pSP->threadId;

	/* Add the program to the state program list */
	seqAddProg(pSP);

	/* Note that the program init, entry, and exit functions
	   get the global var buffer pSP->pVar passed,
	   not the state set local one, even in safe mode. */
	/* TODO: document this */

	/* Call sequencer init function to initialize variables. */
	pSP->initFunc((USER_VAR *)pSP->pVar);

	/* Initialize state set variables. In safe mode, copy variable
	   block to state set buffers.
	   Must do all this before connecting. */
	for (nss = 0; nss < pSP->numSS; nss++)
	{
		SSCB	*pSS = pSP->pSS + nss;

		if (pSP->options & OPT_SAFE)
			memcpy(pSS->pVar, pSP->pVar, pSP->varSize);
	}

	/* Attach to PV context of pvSys creator (auxiliary thread) */
	pvSysAttach(pvSys);

	/* Initiate connect & monitor requests to database channels */
	seq_connect(pSP);

	/* If "+c" option, wait for all channels to connect (a failure
	 * return means that we have been asked to exit) */
	if (pSP->options & OPT_CONN)
	{
		if (!seq_waitConnect(pSP, pSS)) return;
	}

	/* Call program entry function if defined. */
	if (pSP->entryFunc) pSP->entryFunc(pSS, (USER_VAR *)pSP->pVar);

	/* Create each additional state-set task (additional state-set thread
	   names are derived from the first ss) */
	threadLen = strlen(threadName);
	for (nss = 1, pSS = pSP->pSS + 1; nss < pSP->numSS; nss++, pSS++)
	{
		/* Form thread name from program name + state-set number */
		sprintf(threadName+threadLen, "_%d", nss);

		/* Spawn the task */
		tid = epicsThreadCreate(
			threadName,			/* thread name */
			pSP->threadPriority,		/* priority */
			pSP->stackSize,			/* stack size */
			(EPICSTHREADFUNC)ss_entry,	/* entry point */
			pSS);				/* parameter */

		DEBUG("Spawning additional state set thread %p: \"%s\"\n", tid, threadName);
	}

	/* First state-set jumps directly to entry point */
	ss_entry(pSP->pSS);

	/* Call program exit function if defined */
	if (pSP->exitFunc) pSP->exitFunc(pSS, (USER_VAR *)pSP->pVar);
}

/*
 * ss_read_buffer() - Only used in safe mode.
 * Lock-free reading of variable buffer.
 * See also ss_write_buffer().
 */
static void ss_read_buffer(SSCB *pSS)
{
	SPROG		*pSP = pSS->sprog;
	ptrdiff_t	ss_num = pSS - pSP->pSS;
	unsigned	var;

	for (var = 0; var < pSP->numChans; var++)
	{
		CHAN	*pDB = pSP->pChan + var;
		char	*pVal = (char*)pSS->pVar + pDB->offset;
		char	*pBuf = (char*)pSP->pVar + pDB->offset;
		boolean *dirty = pDB->dirty;
		size_t	var_size = (unsigned)pDB->size * pDB->dbCount;

		if (!dirty[ss_num])
			continue;
		do {
			dirty[ss_num] = FALSE;
			memcpy(pVal, pBuf, var_size);
		} while ((pDB->wr_active || dirty[ss_num])
			&& (epicsThreadSleep(0),TRUE));
	}
}

/*
 * ss_write_buffer() - Only used in safe mode.
 * Lock-free writing of variable buffer.
 * See also ss_read_buffer().
 */
void ss_write_buffer(CHAN *pDB, void *pVal)
{
	SPROG	*pSP = pDB->sprog;
	char	*pBuf = (char*)pSP->pVar + pDB->offset;
	boolean *dirty = pDB->dirty;
	size_t	var_size = (unsigned)pDB->size * pDB->dbCount;
	unsigned ss_num;

	pDB->wr_active = TRUE;
	memcpy(pBuf, pVal, var_size);
	for (ss_num = 0; ss_num < pSP->numSS; ss_num++)
	{
		dirty[ss_num] = TRUE;
	}
	pDB->wr_active = FALSE;
}

/*
 * ss_entry() - Thread entry point for all state-sets.
 * Provides the main loop for state-set processing.
 */
static void ss_entry(SSCB *pSS)
{
	SPROG		*pSP = pSS->sprog;
	unsigned	nWords = (pSP->numEvents + NBITS - 1) / NBITS;
	USER_VAR	*pVar = (USER_VAR *)varPtr(pSP,pSS);

	/* Initialize this state-set thread */
	ss_thread_init(pSP, pSS);

	/* Attach to PV context of pvSys creator (auxiliary thread); was
	   already done for the first state set */
	if (pSS != pSP->pSS)
	{
		pSS->threadId = epicsThreadGetIdSelf();
		pvSysAttach(pvSys);
	}


	/* Initial state is the first one */
	pSS->currentState = 0;
	pSS->nextState = -1;
	pSS->prevState = -1;

	/*
	 * ============= Main loop ==============
	 */
	while (TRUE)
	{
		boolean ev_trig;

		/* Set state to current state */
		STATE *pST = pSS->pStates + pSS->currentState;

		/* Set state set event mask to this state's event mask */
		pSS->pMask = pST->pEventMask;

		/* If we've changed state, do any entry actions. Also do these
		 * even if it's the same state if option to do so is enabled.
		 */
		if (pST->entryFunc && (pSS->prevState != pSS->currentState
			|| (pST->options & OPT_DOENTRYFROMSELF)))
		{
			pST->entryFunc(pSS, pVar);
		}

		seq_clearDelay(pSS, pST); /* Clear delay list */
		pST->delayFunc(pSS, pVar); /* Set up new delay list */

		/* Setting this semaphore here guarantees that a when() is
		 * always executed at least once when a state is first
		 * entered.
		 */
		epicsEventSignal(pSS->syncSemId);

		/* Loop until an event is triggered, i.e. when() returns TRUE
		 */
		do {
			double delay = 0.0;

			/* Wake up on PV event, event flag, or expired delay */
			if (seq_getTimeout(pSS, &delay) && delay > 0.0)
			{
				epicsEventWaitWithTimeout(pSS->syncSemId, delay);
			}
			else
			{
				epicsEventWait(pSS->syncSemId);
			}

			/* Check whether we have been asked to exit */
			if (epicsEventTryWait(pSS->death1SemId) == epicsEventWaitOK)
			{
				goto exit;
			}

			/* Copy dirty variable values from CA buffer
			 * to user (safe mode only).
			 */
			if (pSP->options & OPT_SAFE)
			{
				ss_read_buffer(pSS);
			}

			/* Check state change conditions */
			ev_trig = pST->eventFunc(pSS, pVar,
				&pSS->transNum, &pSS->nextState);

			/* Clear all event flags (old ef mode only) */
			if (ev_trig && !(pSP->options & OPT_NEWEF))
			{
				unsigned i;
				for (i = 0; i < nWords; i++)
				{
					pSP->pEvents[i] &= ~pSS->pMask[i];
				}
			}
		} while (!ev_trig);

		/* Execute the state change action */
		pST->actionFunc(pSS, pVar, pSS->transNum, &pSS->nextState);

		/* If changing state, do exit actions */
		if (pST->exitFunc && (pSS->currentState != pSS->nextState
			|| (pST->options & OPT_DOEXITTOSELF)))
		{
			pST->exitFunc(pSS, pVar);
		}

		/* Flush any outstanding DB requests */
		pvSysFlush(pvSys);

		/* Change to next state */
		pSS->prevState = pSS->currentState;
		pSS->currentState = pSS->nextState;
	}

	/* Thread exit has been requested */
exit:

	/* Uninitialize this state-set thread (phase 1) */
	ss_thread_uninit(pSP, pSS, 1);

	/* Pass control back (so all state-set threads can complete phase 1
	 * before embarking on phase 2) */
	epicsEventSignal(pSS->death2SemId);

	/* Wait for request to perform uninitialization (phase 2) */
	epicsEventMustWait(pSS->death3SemId);
	ss_thread_uninit(pSP, pSS, 2);

	/* Pass control back and die (i.e. exit) */
	epicsEventSignal(pSS->death4SemId);
}

/* Initialize a state-set thread */
static void ss_thread_init(SPROG *pSP, SSCB *pSS)
{
	/* Get this thread's id */
	pSS->threadId = epicsThreadGetIdSelf();

	/* Attach to PV context of pvSys creator (auxiliary thread); was
	   already done for the first state-set */
	if (pSP->threadId != pSS->threadId)
		pvSysAttach(pvSys);

	/* Register this thread with the EPICS watchdog (no callback func) */
	taskwdInsert(pSS->threadId, 0, (void *)0);
}

/* Uninitialize a state-set thread */
static void ss_thread_uninit(SPROG *pSP, SSCB *pSS, int phase)
{
	/* Phase 1: if this is the first state-set, call user exit routine
	   and disconnect all channels */
	if (phase == 1 && pSS->threadId == pSP->threadId)
	{
	    USER_VAR *pVar = (USER_VAR *)varPtr(pSP,pSS);
	    DEBUG("   Call exit function\n");
	    pSP->exitFunc(pSS, pVar);

	    DEBUG("   Disconnect all channels\n");
	    seq_disconnect(pSP);
	}

	/* Phase 2: unregister the thread with the EPICS watchdog */
	else if (phase == 2)
	{
	    DEBUG("   taskwdRemove(%p)\n", pSS->threadId);
	    taskwdRemove(pSS->threadId);
	}
}

/* Wait for all channels to connect */
static boolean seq_waitConnect(SPROG *pSP, SSCB *pSS)
{
	epicsStatus	status;
	double		delay;

	if (pSP->numChans == 0)
		return TRUE;
	delay = 10.0; /* 10, 20, 30, 40, 40,... sec */
	while (1)
	{
		status = epicsEventWaitWithTimeout(
			pSS->allFirstConnectAndMonitorSemId, delay);
		if(status==epicsEventWaitOK) break;
		if (delay < 40.0)
		{
			delay += 10.0;
			errlogPrintf("numMonitoredChans %u firstMonitorCount %u",
				pSP->numMonitoredChans,pSP->firstMonitorCount);
			errlogPrintf(" assignCount %u firstConnectCount %u\n",
				pSP->assignCount,pSP->firstConnectCount);
		}
		/* Check whether we have been asked to exit */
		if (epicsEventTryWait(pSS->death1SemId) == epicsEventWaitOK)
			return FALSE;
	}
	return TRUE;
}

/*
 * seq_clearDelay() - clear the time delay list.
 */
static void seq_clearDelay(SSCB *pSS, STATE *pST)
{
	unsigned ndelay;

	/* On state change set time we entered this state; or if transition from
	 * same state if option to do so is on for this state.
	 */
	if ((pSS->currentState != pSS->prevState) ||
		!(pST->options & OPT_NORESETTIMERS))
	{
		pvTimeGetCurrentDouble(&pSS->timeEntered);
	}

	for (ndelay = 0; ndelay < pSS->maxNumDelays; ndelay++)
	{
		pSS->delay[ndelay] = 0;
	 	pSS->delayExpired[ndelay] = FALSE;
	}

	pSS->numDelays = 0;
}

/*
 * seq_getTimeout() - return time-out for pending on events.
 * Return whether to time out when waiting for events.
 * If yes, set *pdelay to the timout (in seconds).
 */
static int seq_getTimeout(SSCB *pSS, double *pdelay)
{
	unsigned ndelay;
	boolean	do_timeout = FALSE;
	double	cur, delay;
	double	delayMin = 0;
	/* not really necessary to initialize delayMin,
	   but tell that to the compiler...
	 */

	if (pSS->numDelays == 0)
		return do_timeout;
	/*
	 * Calculate the delay since this state was entered.
	 */
	pvTimeGetCurrentDouble(&cur);
	delay = cur - pSS->timeEntered;
	/*
	 * Find the minimum delay among all unexpired timeouts if
	 * one exists, and set do_timeout in this case.
	 */
	for (ndelay = 0; ndelay < pSS->numDelays; ndelay++)
	{
		double delayN;

		if (pSS->delayExpired[ndelay])
			continue; /* skip if this delay entry already expired */
		delayN = pSS->delay[ndelay];
		if (delay >= delayN)
		{	/* just expired */
			pSS->delayExpired[ndelay] = TRUE; /* mark as expired */
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
	SPROG		*pSP;
	SSCB		*pSS;
	unsigned	nss;

	/* Check that this is indeed a state program thread */
	pSP = seqFindProg(tid);
	if (pSP == NULL)
		return;

	DEBUG("Stop %s: pSP=%p, tid=%p\n", pSP->pProgName, pSP,tid);

	/* Ask all state-set threads to exit (phase 1) */
	DEBUG("   Asking state-set threads to exit (phase 1):\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		/* Just possibly hasn't started yet, so check... */
		if (pSS->threadId == 0)
			continue;

		/* Ask the thread to exit */
		DEBUG("      tid=%p\n", pSS->threadId);
		epicsEventSignal(pSS->death1SemId);
	}

	/* Wake up all state-sets */
	DEBUG("   Waking up all state-sets\n");
	seqWakeup (pSP, 0);

	/* Wait for them all to complete phase 1 of their deaths */
	DEBUG("   Waiting for state-set threads phase 1 death:\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(pSS->death2SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 1 (ignored)\n",
				     pSS->threadId, pSS->pSSName);
		}
		else
		{
			DEBUG("      tid=%p\n", pSS->threadId);
		}
	}

	/* Ask all state-set threads to exit (phase 2) */
	DEBUG("   Asking state-set threads to exit (phase 2):\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		DEBUG("      tid=%p\n", pSS->threadId);
		epicsEventSignal(pSS->death3SemId);
	}

	/* Wait for them all to complete phase 2 of their deaths */
	DEBUG("   Waiting for state-set threads phase 2 death:\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(pSS->death4SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 2 (ignored)\n",
				     pSS->threadId, pSS->pSSName);
		}
		else
		{
			DEBUG("      tid=%p\n", pSS->threadId);
		}
	}

	/* Remove the state program from the state program list */
	seqDelProg(pSP);

	/* Delete state-set semaphores */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->allFirstConnectAndMonitorSemId != NULL)
			epicsEventDestroy(pSS->allFirstConnectAndMonitorSemId);
		if (pSS->syncSemId != NULL)
			epicsEventDestroy(pSS->syncSemId);
		if (pSS->getSemId != NULL)
			epicsEventDestroy(pSS->getSemId);
		if (pSS->death1SemId != NULL)
			epicsEventDestroy(pSS->death1SemId);
		if (pSS->death2SemId != NULL)
			epicsEventDestroy(pSS->death2SemId);
		if (pSS->death3SemId != NULL)
			epicsEventDestroy(pSS->death3SemId);
		if (pSS->death4SemId != NULL)
			epicsEventDestroy(pSS->death4SemId);
	}

	/* Delete program-wide semaphores */
	epicsMutexDestroy(pSP->programLock);

	/* Free all allocated memory */
	seqFree(pSP);

	DEBUG("   Done\n");
}

/* seqFree()--free all allocated memory */
void seqFree(SPROG *pSP)
{
	SSCB		*pSS;
	CHAN		*pDB;
	unsigned	nch;

	seqMacFree(pSP);
	for (nch = 0; nch < pSP->numChans; nch++)
	{
		pDB = pSP->pChan + nch;

		if (pDB->dbName != NULL)
			free(pDB->dbName);
		if (pDB->putSemId != NULL)
			epicsEventDestroy(pDB->putSemId);
	}

	/* Free channel structures */
	free(pSP->pChan);

	pSS = pSP->pSS;

	/* Free event words */
	free(pSP->pEvents);

	/* Free SSCBs */
	free(pSP->pSS);

	/* Free SPROG */
	free(pSP);
}

/* 
 * Sequencer auxiliary thread -- loops on pvSysPend().
 */
void *seqAuxThread(void *tArgs)
{
	AUXARGS		*pArgs = (AUXARGS *)tArgs;
	char		*pPvSysName = pArgs->pPvSysName;
	long		debug = pArgs->debug;
	int		status;

	/* Register this thread with the EPICS watchdog */
	taskwdInsert(epicsThreadGetIdSelf(), 0, 0);

	/* All state program threads will use a common PV context (subtract
	   1 from debug level for PV debugging) */
	status = pvSysCreate(pPvSysName, debug>0?debug-1:0, &pvSys);
	if (status != pvStatOK)
	{
		errlogPrintf("seqAuxThread: pvSysCreate() %s failure: %s\n",
			pPvSysName, pvSysGetMess(pvSys));
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

