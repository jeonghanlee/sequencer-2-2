/*	Interface functions from state program to run-time sequencer.
 *	Note: To prevent global name conflicts "seq_" is added by the SNC, e.g.
 *	pvPut() becomes seq_pvPut().
 *
 *	Author:  Andy Kozubal
 *	Date:    1 March, 1994
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-1994, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
 *		und Energie GmbH, Germany (HZB)
 *		(see file Copyright.HZB included in this distribution)
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *	  The Controls and Automation Group (AT-8)
 *	  Ground Test Accelerator
 *	  Accelerator Technology Division
 *	  Los Alamos National Laboratory
 *
 *	Co-developed with
 *	  The Controls and Computing Group
 *	  Accelerator Systems Division
 *	  Advanced Photon Source
 *	  Argonne National Laboratory
 */

#include <stdlib.h>
#include <string.h>

#include "seq.h"

/* Flush outstanding PV requests */
epicsShareFunc void epicsShareAPI seq_pvFlush(void)
{
	pvSysFlush(pvSys);
}	

/*
 * seq_pvGet() - Get DB value.
 * TODO: add optional timeout argument.
 */
epicsShareFunc long epicsShareAPI seq_pvGet(SS_ID pSS, long pvId, long compType)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	int	sync;	/* whether synchronous get */
	int	status;
	PVREQ	*req;

	/* Determine whether performing asynchronous or synchronous i/o */
	switch (compType)
	{
	case HONOR_OPTION:
		sync = !(pSP->options & OPT_ASYNC);
		break;
	case ASYNCHRONOUS:
		sync = FALSE;
		break;
	case SYNCHRONOUS:
		sync = TRUE;
		break;
	default:
		errlogSevPrintf(errlogFatal, "pvGet: user error (bad completion type)\n");
		return pvStatERROR;
	}

	/* Flag this pvGet() as not completed */
	pDB->getComplete = FALSE;

	/* Check for channel connected */
	if (!pDB->connected)
	{
		pDB->status = pvStatDISCONN;
		pDB->severity = pvSevrINVALID;
		pDB->message = Strdcpy(pDB->message, "disconnected");
		return pDB->status;
	}

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if (sync)
	{
		epicsEventTryWait(pSS->getSemId);
	}

	/* Allocate and initialize a pv request */
	req = (PVREQ *)freeListMalloc(pSP->pvReqPool);
	req->pSS = pSS;
	req->pDB = pDB;

	/* Perform the PV get operation with a callback routine specified */
	status = pvVarGetCallback(
			pDB->pvid,		/* PV id */
			pDB->getType,		/* request type */
			pDB->count,		/* element count */
			seq_get_handler,	/* callback handler */
			req);			/* user arg */
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvGet: pvVarGetCallback() %s failure: %s\n",
			pDB->dbName, pvVarGetMess(pDB->pvid));
		pDB->getComplete[ssNum(pSS)] = TRUE;
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (sync)
	{
		epicsEventWaitStatus sem_status;

		pvSysFlush(pvSys);
		sem_status = epicsEventWaitWithTimeout(pSS->getSemId, 10.0);
		if (sem_status != epicsEventWaitOK)
		{
			pDB->status = pvStatTIMEOUT;
			pDB->severity = pvSevrMAJOR;
			pDB->message = Strdcpy(pDB->message,
						"get completion timeout");
			return pDB->status;
		}
	}
        
	return pvStatOK;
}

/*
 * seq_pvGetComplete() - returns TRUE if the last get completed.
 */
epicsShareFunc long epicsShareAPI seq_pvGetComplete(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->getComplete[ssNum(pSS)];
}

/*
 * seq_pvPut() - Put DB value.
 */
epicsShareFunc long epicsShareAPI seq_pvPut(SS_ID pSS, long pvId, long compType)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	int	nonb;	/* whether to call pvVarPutNoBlock (=fire&forget) */
	int	sync;	/* whether to wait for completion */
	int	status;
	int	count;
	void	*pVar = valPtr(pDB,pSS);	/* ptr to value */
	PVREQ	*req;

#ifdef DEBUG
	errlogPrintf("seq_pvPut: pv name=%s, pVar=%p\n", pDB->dbName, pVar);
#endif

	/* Determine whether performing asynchronous or synchronous i/o
	   ((+a) option was never honored for put, so HONOR_OPTION
	   means non-blocking and therefore implicitly asynchronous) */
	switch (compType)
	{
	case HONOR_OPTION:
		nonb = TRUE;
		sync = FALSE;
		break;
	case ASYNCHRONOUS:
		nonb = FALSE;
		sync = FALSE;
		break;
	case SYNCHRONOUS:
		nonb = FALSE;
		sync = TRUE;
		break;
	default:
		errlogSevPrintf(errlogFatal, "pvPut: user error (bad completion type)\n");
		return pvStatERROR;
	}

	if (!nonb)
	{
		/* Must wait for active put to complete first */
		epicsEventWait(pDB->putSemId);
	}

	/* Check for channel connected */
	/* TODO: is this needed? */
	if (!pDB->connected)
	{
		pDB->status = pvStatDISCONN;
		pDB->severity = pvSevrINVALID;
		pDB->message = Strdcpy(pDB->message, "disconnected");
		return pDB->status;
	}

	/* Determine number of elements to put (don't try to put more
	   than db count) */
	count = min(pDB->count, pDB->dbCount);

	/* Perform the PV put operation (either non-blocking or with a
	   callback routine specified) */
	if (nonb)
	{
		status = pvVarPutNoBlock(
				pDB->pvid,		/* PV id */
				pDB->putType,		/* data type */
				count,			/* element count */
				(pvValue *)pVar);	/* data value */
	}
	else
	{
		/* Allocate and initialize a pv request */
		req = (PVREQ *)freeListMalloc(pSP->pvReqPool);
		req->pSS = pSS;
		req->pDB = pDB;

		status = pvVarPutCallback(
				pDB->pvid,		/* PV id */
				pDB->putType,		/* data type */
				count,			/* element count */
				(pvValue *)pVar,	/* data value */
				seq_put_handler,	/* callback handler */
				req);			/* user arg */
	}
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvPut: pvVarPut%s() %s failure: %s\n",
			nonb ? "NoBlock" : "Callback", pDB->dbName,
			pvVarGetMess(pDB->pvid));
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (sync) /* => !nonb */
	{
		epicsEventWaitStatus sem_status;

		pvSysFlush(pvSys);
		sem_status = epicsEventWaitWithTimeout(pDB->putSemId, 10.0);
		if (sem_status != epicsEventWaitOK)
		{
			pDB->status = pvStatTIMEOUT;
			pDB->severity = pvSevrMAJOR;
			pDB->message = Strdcpy(pDB->message,
				"put completion timeout");
			return pDB->status;
		}
	}

#ifdef	DEBUG
	errlogPrintf("seq_pvPut: status=%d, mess=%s\n", status,
		pvVarGetMess(pDB->pvid));
	if (status != pvStatOK)
	{
		errlogPrintf("pvPut on \"%s\" failed (%d)\n", pDB->dbName,
			     status);
		errlogPrintf("  putType=%d\n", pDB->putType);
		errlogPrintf("  size=%d, count=%d\n", pDB->size, count);
	}
#endif	/*DEBUG*/

	return pvStatOK;
}

/*
 * seq_pvPutComplete() - returns TRUE if the last put completed.
 */
epicsShareFunc long epicsShareAPI seq_pvPutComplete(
	SS_ID	pSS,
	long	pvId,
	long	length,
	long	any,
	long	*pComplete)
{
	SPROG	*pSP = pSS->sprog;
	long	anyDone = FALSE, allDone = TRUE;
	int	i;

	for (i=0; i<length; i++)
	{
		CHAN *pDB = pSP->pChan + pvId + i;

		long done = epicsEventTryWait(pDB->putSemId);

		anyDone = anyDone || done;
		allDone = allDone && done;

		if (pComplete != NULL)
		{
			pComplete[i] = done;
		}
		else if (any && done)
		{
			break;
		}
	}

#ifdef DEBUG
	errlogPrintf("pvPutComplete: pvId=%ld, length=%ld, anyDone=%ld, "
		"allDone=%ld\n", pvId, length, anyDone, allDone);
#endif

	return any?anyDone:allDone;
}

/*
 * seq_pvAssign() - Assign/Connect to a channel.
 * Assign to a zero-length string ("") disconnects/de-assigns.
 */
epicsShareFunc long epicsShareAPI seq_pvAssign(SS_ID pSS, long pvId, char *pvName)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	int	status, nchar;

#ifdef	DEBUG
	errlogPrintf("Assign %s to \"%s\"\n", pDB->pVarName, pvName);
#endif	/*DEBUG*/
	if (pDB->assigned)
	{	/* Disconnect this PV */
		status = pvVarDestroy(pDB->pvid);
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarDestroy() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
		}
		free(pDB->dbName);
		pDB->assigned = FALSE;
		pSP->assignCount -= 1;
	}

	if (pDB->connected)
	{
		pDB->connected = FALSE;
		pSP->connCount -= 1;
	}
	pDB->monitored = FALSE;
	nchar = strlen(pvName);
	pDB->dbName = (char *)calloc(1, nchar + 1);
	strcpy(pDB->dbName, pvName);

	/* Connect */
	if (nchar > 0)
	{
		pDB->assigned = TRUE;
		pSP->assignCount += 1;
		status = pvVarCreate(
			pvSys,			/* PV system context */
			pDB->dbName,		/* DB channel name */
			seq_conn_handler,	/* connection handler routine */
			pDB,			/* user ptr is CHAN structure */
			0,			/* debug level (inherited) */
			&pDB->pvid);		/* ptr to pvid */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarCreate() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
			return status;
		}

		if (pDB->monFlag)
		{
			status = seq_pvMonitor(pSS, pvId);
			if (status != pvStatOK)
				return status;
		}
	}

	pvSysFlush(pvSys);
	
	return pvStatOK;
}

/*
 * seq_pvMonitor() - Initiate a monitor on a channel.
 */
epicsShareFunc long epicsShareAPI seq_pvMonitor(SS_ID pSS, long pvId)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	int	status;

#ifdef	DEBUG
	errlogPrintf("monitor \"%s\"\n", pDB->dbName);
#endif	/*DEBUG*/

/*	if (pDB->monitored || !pDB->assigned)	*/
/*	WFL, 96/08/07, don't check monitored because it can get set TRUE */
/*	in the connection handler before this routine is executed; this */
/*	fix pending a proper fix */
	if (!pDB->assigned)
		return pvStatOK;

	status = pvVarMonitorOn(
		pDB->pvid,		/* pvid */
		pDB->getType,		/* requested type */
		pDB->count,		/* element count */
		seq_mon_handler,	/* function to call */
		pDB,			/* user arg (db struct) */
		&pDB->evid);		/* where to put event id */

	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvMonitor: pvVarMonitorOn() %s failure: %s\n",
			pDB->dbName, pvVarGetMess(pDB->pvid));
		return status;
	}
	pvSysFlush(pvSys);

	pDB->monitored = TRUE;
	return pvStatOK;
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
epicsShareFunc long epicsShareAPI seq_pvStopMonitor(SS_ID pSS, long pvId)
{
	CHAN	*pDB = pSS->sprog->pChan + pvId;
	int	status;

	if (!pDB->monitored)
		return -1;

	status = pvVarMonitorOff(pDB->pvid,pDB->evid);
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvStopMonitor: pvVarMonitorOff() %s failure: "
			"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
		return status;
	}

	pDB->monitored = FALSE;

	return status;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
epicsShareFunc long epicsShareAPI seq_pvChannelCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
epicsShareFunc long epicsShareAPI seq_pvConnectCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->connCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
epicsShareFunc long epicsShareAPI seq_pvAssignCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->assignCount;
}

/*
 * seq_pvConnected() - returns TRUE if database channel is connected.
 */
epicsShareFunc long epicsShareAPI seq_pvConnected(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->connected;
}

/*
 * seq_pvAssigned() - returns TRUE if database channel is assigned.
 */
epicsShareFunc long epicsShareAPI seq_pvAssigned(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->assigned;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * (1) the array size and (2) the element count returned by the PV layer.
 */
epicsShareFunc long epicsShareAPI seq_pvCount(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->dbCount;
}

/*
 * seq_pvName() - returns channel name.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->dbName;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
epicsShareFunc long epicsShareAPI seq_pvStatus(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->status;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
epicsShareFunc long epicsShareAPI seq_pvSeverity(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->severity;
}

/*
 * seq_pvMessage() - returns channel error message.
 */
epicsShareFunc char *epicsShareAPI seq_pvMessage(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->message;
}

/*
 * seq_pvIndex() - returns index of database variable.
 */
epicsShareFunc long epicsShareAPI seq_pvIndex(SS_ID pSS, long pvId)
{
	return pvId; /* index is same as pvId */
}

/*
 * seq_pvTimeStamp() - returns channel time stamp.
 */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID pSS, long pvId)
{
	CHAN *pDB = pSS->sprog->pChan + pvId;

	return pDB->timeStamp;
}
/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID pSS, long ev_flag)
{
	SPROG	*pSP = pSS->sprog;

	epicsMutexMustLock(pSP->caSemId);

#ifdef	DEBUG
	errlogPrintf("seq_efSet: pSP=%p, pSS=%p, ev_flag=%p\n", pSP, pSS,
		ev_flag);
#endif	/*DEBUG*/

	/* Set this bit */
	bitSet(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	epicsMutexUnlock(pSP->caSemId);
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
epicsShareFunc long epicsShareAPI seq_efTest(SS_ID pSS, long ev_flag)
/* event flag */
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	epicsMutexMustLock(pSP->caSemId);

	isSet = bitTest(pSP->pEvents, ev_flag);

#ifdef	DEBUG
	errlogPrintf("seq_efTest: ev_flag=%d, event=%p, isSet=%d\n",
		ev_flag, pSP->pEvents[0], isSet);
#endif	/*DEBUG*/

	epicsMutexUnlock(pSP->caSemId);

	return isSet;
}

/*
 * seq_efClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc long epicsShareAPI seq_efClear(SS_ID pSS, long ev_flag)
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	isSet = bitTest(pSP->pEvents, ev_flag);

	epicsMutexMustLock(pSP->caSemId);

	/* Clear this bit */
	bitClear(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	epicsMutexUnlock(pSP->caSemId);

	return isSet;
}

/*
 * seq_efTestAndClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc long epicsShareAPI seq_efTestAndClear(SS_ID pSS, long ev_flag)
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	epicsMutexMustLock(pSP->caSemId);

	isSet = bitTest(pSP->pEvents, ev_flag);
	bitClear(pSP->pEvents, ev_flag);

	epicsMutexUnlock(pSP->caSemId);

	return isSet;
}

/*
 * seq_pvGetQ() - Get queued DB value (looks like pvGet() but really more
 *		  like efTestAndClear()).
 */
epicsShareFunc int epicsShareAPI seq_pvGetQ(SS_ID pSS, int pvId)

{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	void	*pVar = valPtr(pDB,pSS);
	int	ev_flag;
	int	isSet;

	epicsMutexMustLock(pSP->caSemId);

	/* Determine event flag number and whether set */
	ev_flag = pDB->efId;
	isSet = bitTest(pSP->pEvents, ev_flag);

#ifdef	DEBUG
	errlogPrintf("seq_pvGetQ: pv name=%s, isSet=%d\n", pDB->dbName, isSet);
#endif	/*DEBUG*/

	/* If set, queue should be non empty */
	if (isSet)
	{
		QENTRY	*pEntry;
		pvValue	*pAccess;
		void	*pVal;

		/* Dequeue first entry */
		pEntry = (QENTRY *) ellGet(&pSP->pQueues[pDB->queueIndex]);

		/* If none, "impossible" */
		if (pEntry == NULL)
		{
			errlogPrintf("seq_pvGetQ: evflag set but queue empty "
				"(impossible)\n");
			isSet = FALSE;
		}

		/* Extract information from entry (code from seq_ca.c)
		   (pDB changed to refer to channel for which monitor
		   was posted) */
		else
		{
			pDB = pEntry->pDB;
			pAccess = &pEntry->value;

			/* Copy value returned into user variable */
			/* For now, can only return _one_ array element */
			pVal = (char *)pAccess + pDB->dbOffset;

			epicsMutexLock(pDB->varLock);
			memcpy(pVar, pVal, pDB->size * 1 );
							/* was pDB->dbCount */

			/* Copy status & severity */
			pDB->status = pAccess->timeStringVal.status;
			pDB->severity = pAccess->timeStringVal.severity;

			/* Copy time stamp */
			pDB->timeStamp = pAccess->timeStringVal.stamp;
			epicsMutexUnlock(pDB->varLock);

			/* Free queue entry */
			free(pEntry);
		}
	}

	/* If queue is now empty, clear the event flag */
	if (ellCount(&pSP->pQueues[pDB->queueIndex]) == 0)
	{
		bitClear(pSP->pEvents, ev_flag);
	}

	epicsMutexUnlock(pSP->caSemId);

	/* return TRUE iff event flag was set on entry */
	return isSet;
}

/*
 * seq_pvFreeQ() - Free elements on syncQ queue and clear event flag.
 *		   Intended to be called from action code.
 */
epicsShareFunc int epicsShareAPI seq_pvFreeQ(SS_ID pSS, int pvId)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + pvId;
	QENTRY	*pEntry;
	int	ev_flag;

	epicsMutexMustLock(pSP->caSemId);

#ifdef	DEBUG
	errlogPrintf("seq_pvFreeQ: pv name=%s, count=%d\n", pDB->dbName,
		ellCount(&pSP->pQueues[pDB->queueIndex]));
#endif	/*DEBUG*/

	/* Determine event flag number */
	ev_flag = pDB->efId;

	/* Free queue elements */
	while((pEntry = (QENTRY *)ellGet(&pSP->pQueues[pDB->queueIndex])) != NULL)
		free(pEntry);

	/* Clear event flag */
	bitClear(pSP->pEvents, ev_flag);

	epicsMutexUnlock(pSP->caSemId);

	return 0;
}


/* seq_delay() - test for delay() time-out expired */
epicsShareFunc long epicsShareAPI seq_delay(SS_ID pSS, long delayId)
{
	double	timeNow, timeElapsed;
	long	expired = FALSE;

	/* Calc. elapsed time since state was entered */
	pvTimeGetCurrentDouble( &timeNow );
	timeElapsed = timeNow - pSS->timeEntered;

	/* Check for delay timeout */
	if ( (timeElapsed >= pSS->delay[delayId]) )
	{
		pSS->delayExpired[delayId] = TRUE; /* mark as expired */
		expired = TRUE;
	}
#ifdef DEBUG
	errlogPrintf("Delay %ld : %ld ticks, %s\n",delayId,pSS->delay[delayId],
		expired ? "expired": "unexpired");
#endif
	return expired;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID pSS, long delayId, double delay)
{
#ifdef DEBUG
	printf("seq_delayInit: delayId=%ld, numDelays=%ld, maxNumDelays=%ld\n",
		delayId, pSS->numDelays, pSS->maxNumDelays);
#endif
	assert(delayId <= pSS->numDelays);
	assert(pSS->numDelays < pSS->maxNumDelays);

	pSS->delay[delayId] = delay;
	pSS->numDelays = max(pSS->numDelays, delayId + 1);
}

/*
 * seq_optGet: return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
epicsShareFunc unsigned epicsShareAPI seq_optGet(SS_ID pSS, char *opt)
{
	SPROG	*pSP = pSS->sprog;

	switch (opt[0])
	{
	case 'a': return ( (pSP->options & OPT_ASYNC) != 0);
	case 'c': return ( (pSP->options & OPT_CONN)  != 0);
	case 'd': return ( (pSP->options & OPT_DEBUG) != 0);
	case 'e': return ( (pSP->options & OPT_NEWEF) != 0);
	case 'm': return ( (pSP->options & OPT_MAIN)  != 0);
	case 'r': return ( (pSP->options & OPT_REENT) != 0);
	case 's': return ( (pSP->options & OPT_SAFE)  != 0);
	default:  return FALSE;
	}
}
