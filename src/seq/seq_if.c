/*	Interface functions from state program to run-time sequencer.
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
#include "seq.h"

/* #define DEBUG errlogPrintf */
#define DEBUG nothing

/* Flush outstanding PV requests */
epicsShareFunc void epicsShareAPI seq_pvFlush(SS_ID pSS)
{
	pvSysFlush(pvSys);
}	

/*
 * seq_pvGet() - Get DB value.
 * TODO: add optional timeout argument.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvGet(SS_ID pSS, VAR_ID varId, enum compType compType)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	int	sync;	/* whether synchronous get */
	pvStat	status;
	PVREQ	*req;

	/* Determine whether performing asynchronous or synchronous i/o */
	switch (compType)
	{
	case DEFAULT:
		sync = !(pSP->options & OPT_ASYNC);
		break;
	case ASYNC:
		sync = FALSE;
		break;
	case SYNC:
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
		pDB->message = "disconnected";
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
			pDB->type->getType,	/* request type */
			(int)pDB->count,	/* element count */
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
			pDB->message = "get completion timeout";
			return pDB->status;
		}
	}
        
	return pvStatOK;
}

/*
 * seq_pvGetComplete() - returns whether the last get completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetComplete(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->getComplete[ssNum(pSS)];
}

/*
 * seq_pvPut() - Put DB value.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvPut(SS_ID pSS, VAR_ID varId, enum compType compType)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	int	nonb;	/* whether to call pvVarPutNoBlock (=fire&forget) */
	int	sync;	/* whether to wait for completion */
	int	status;
	unsigned count;
	char	*pVar = valPtr(pDB,pSS);	/* ptr to value */
	PVREQ	*req;

	DEBUG("seq_pvPut: pv name=%s, pVar=%p\n", pDB->dbName, pVar);

	/* Determine whether performing asynchronous or synchronous i/o
	   ((+a) option was never honored for put, so DEFAULT
	   means non-blocking and therefore implicitly asynchronous) */
	switch (compType)
	{
	case DEFAULT:
		nonb = TRUE;
		sync = FALSE;
		break;
	case ASYNC:
		nonb = FALSE;
		sync = FALSE;
		break;
	case SYNC:
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
		pDB->message = "disconnected";
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
				pDB->type->putType,	/* data type */
				(int)count,		/* element count */
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
				pDB->type->putType,	/* data type */
				(int)count,		/* element count */
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
			pDB->message = "put completion timeout";
			return pDB->status;
		}
	}

	DEBUG("seq_pvPut: status=%d, mess=%s\n", status,
		pvVarGetMess(pDB->pvid));
	if (status != pvStatOK)
	{
		DEBUG("pvPut on \"%s\" failed (%d)\n", pDB->dbName, status);
		DEBUG("  putType=%d\n", pDB->type->putType);
		DEBUG("  size=%d, count=%d\n", pDB->type->size, count);
	}

	return pvStatOK;
}

/*
 * seq_pvPutComplete() - returns whether the last put completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvPutComplete(
	SS_ID		pSS,
	VAR_ID		varId,
	unsigned	length,
	boolean		any,
	boolean		*pComplete)
{
	SPROG	*pSP = pSS->sprog;
	long	anyDone = FALSE, allDone = TRUE;
	unsigned i;

	for (i=0; i<length; i++)
	{
		CHAN *pDB = pSP->pChan + varId + i;

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

	DEBUG("pvPutComplete: varId=%ld, length=%ld, anyDone=%ld, "
		"allDone=%ld\n", varId, length, anyDone, allDone);

	return any?anyDone:allDone;
}

/*
 * seq_pvAssign() - Assign/Connect to a channel.
 * Assign to a zero-length string ("") disconnects/de-assigns.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvAssign(SS_ID pSS, VAR_ID varId, const char *pvName)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	pvStat	status;
        unsigned nchar;

	DEBUG("Assign %s to \"%s\"\n", pDB->pVarName, pvName);

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
		pSP->connectCount -= 1;
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
			status = seq_pvMonitor(pSS, varId);
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
epicsShareFunc pvStat epicsShareAPI seq_pvMonitor(SS_ID pSS, VAR_ID varId)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	int	status;

	DEBUG("seq_pvMonitor \"%s\"\n", pDB->dbName);

/*	if (pDB->monitored || !pDB->assigned)	*/
/*	WFL, 96/08/07, don't check monitored because it can get set TRUE */
/*	in the connection handler before this routine is executed; this */
/*	fix pending a proper fix */
	if (!pDB->assigned)
		return pvStatOK;

	status = pvVarMonitorOn(
		pDB->pvid,		/* pvid */
		pDB->type->getType,	/* requested type */
		(int)pDB->count,	/* element count */
		seq_mon_handler,	/* function to call */
		pDB,			/* user arg (db struct) */
		&pDB->monid);		/* where to put event id */

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
epicsShareFunc pvStat epicsShareAPI seq_pvStopMonitor(SS_ID pSS, VAR_ID varId)
{
	CHAN	*pDB = pSS->sprog->pChan + varId;
	int	status;

	if (!pDB->monitored)
		return -1;

	status = pvVarMonitorOff(pDB->pvid,pDB->monid);
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
 * seq_pvSync() - Synchronize pv with an event flag.
 * ev_flag == 0 means unSync.
 */
epicsShareFunc void epicsShareAPI seq_pvSync(SS_ID pSS, VAR_ID varId, EV_ID ev_flag)
{
	assert(ev_flag >= 0 && ev_flag <= pSS->sprog->numEvents);
	pSS->sprog->pChan[varId].efId = ev_flag;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvChannelCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvConnectCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->connectCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvAssignCount(SS_ID pSS)
{
	SPROG	*pSP = pSS->sprog;

	return pSP->assignCount;
}

/*
 * seq_pvConnected() - returns whether database channel is connected.
 */
epicsShareFunc boolean epicsShareAPI seq_pvConnected(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->connected;
}

/*
 * seq_pvAssigned() - returns whether database channel is assigned.
 */
epicsShareFunc boolean epicsShareAPI seq_pvAssigned(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->assigned;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * the array size and the element count returned by the PV layer.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvCount(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->dbCount;
}

/*
 * seq_pvName() - returns channel name.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->dbName;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStatus(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->status;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
epicsShareFunc pvSevr epicsShareAPI seq_pvSeverity(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->severity;
}

/*
 * seq_pvMessage() - returns channel error message.
 */
epicsShareFunc const char *epicsShareAPI seq_pvMessage(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->message;
}

/*
 * seq_pvIndex() - returns index of database variable.
 */
epicsShareFunc VAR_ID epicsShareAPI seq_pvIndex(SS_ID pSS, VAR_ID varId)
{
	return varId; /* index is same as varId */
}

/*
 * seq_pvTimeStamp() - returns channel time stamp.
 */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID pSS, VAR_ID varId)
{
	CHAN *pDB = pSS->sprog->pChan + varId;

	return pDB->timeStamp;
}
/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID pSS, EV_ID ev_flag)
{
	SPROG	*pSP = pSS->sprog;

	epicsMutexMustLock(pSP->programLock);

	DEBUG("seq_efSet: pSP=%p, pSS=%p, ev_flag=%ld\n", pSP, pSS,
		ev_flag);

	/* Set this bit */
	bitSet(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	epicsMutexUnlock(pSP->programLock);
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
epicsShareFunc boolean epicsShareAPI seq_efTest(SS_ID pSS, EV_ID ev_flag)
/* event flag */
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	epicsMutexMustLock(pSP->programLock);

	isSet = bitTest(pSP->pEvents, ev_flag);

	DEBUG("seq_efTest: ev_flag=%ld, event=%#lx, isSet=%d\n",
		ev_flag, pSP->pEvents[0], isSet);

	epicsMutexUnlock(pSP->programLock);

	return isSet;
}

/*
 * seq_efClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc boolean epicsShareAPI seq_efClear(SS_ID pSS, EV_ID ev_flag)
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	isSet = bitTest(pSP->pEvents, ev_flag);

	epicsMutexMustLock(pSP->programLock);

	/* Clear this bit */
	bitClear(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	epicsMutexUnlock(pSP->programLock);

	return isSet;
}

/*
 * seq_efTestAndClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc boolean epicsShareAPI seq_efTestAndClear(SS_ID pSS, EV_ID ev_flag)
{
	SPROG	*pSP = pSS->sprog;
	int	isSet;

	epicsMutexMustLock(pSP->programLock);

	isSet = bitTest(pSP->pEvents, ev_flag);
	bitClear(pSP->pEvents, ev_flag);

	epicsMutexUnlock(pSP->programLock);

	return isSet;
}

/*
 * seq_pvGetQ() - Get queued DB value.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetQ(SS_ID pSS, VAR_ID varId)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	char	*pVar = valPtr(pDB,pSS);
	EV_ID	ev_flag;
	boolean	isSet;

	epicsMutexMustLock(pSP->programLock);

	/* Determine event flag number and whether set */
	ev_flag = pDB->efId;
	isSet = bitTest(pSP->pEvents, ev_flag);

	DEBUG("seq_pvGetQ: pv name=%s, isSet=%d\n", pDB->dbName, isSet);

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
		        pvType type = pDB->type->getType;

			pDB = pEntry->pDB;
			pAccess = &pEntry->value;

			/* Copy value returned into user variable */
			/* For now, can only return _one_ array element */
			pVal = pv_value_ptr(pAccess, type);

			epicsMutexLock(pDB->varLock);
			memcpy(pVar, pVal, pDB->type->size * 1 );
							/* was pDB->dbCount */
			if (pv_is_time_type(type))
			{
				pDB->status = *pv_status_ptr(pAccess, type);
				pDB->severity = *pv_severity_ptr(pAccess, type);
				pDB->timeStamp = *pv_stamp_ptr(pAccess, type);
			}
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

	epicsMutexUnlock(pSP->programLock);

	/* return TRUE iff event flag was set on entry */
	return isSet;
}

/*
 * seq_pvFreeQ() - Same as seq_pvFlushQ.
 *		   Provided for compatibility.
 */
epicsShareFunc void epicsShareAPI seq_pvFreeQ(SS_ID pSS, VAR_ID varId)
{
	seq_pvFreeQ(pSS, varId);
}

/*
 * seq_pvFlushQ() - Flush elements on syncQ queue and clear event flag.
 */
epicsShareFunc void epicsShareAPI seq_pvFlushQ(SS_ID pSS, VAR_ID varId)
{
	SPROG	*pSP = pSS->sprog;
	CHAN	*pDB = pSP->pChan + varId;
	QENTRY	*pEntry;
	EV_ID	ev_flag;

	epicsMutexMustLock(pSP->programLock);

	DEBUG("seq_pvFreeQ: pv name=%s, count=%d\n", pDB->dbName,
		ellCount(&pSP->pQueues[pDB->queueIndex]));

	/* Determine event flag number */
	ev_flag = pDB->efId;

	/* Free queue elements */
	while((pEntry = (QENTRY *)ellGet(&pSP->pQueues[pDB->queueIndex])) != NULL)
		free(pEntry);

	/* Clear event flag */
	bitClear(pSP->pEvents, ev_flag);

	epicsMutexUnlock(pSP->programLock);
}


/* seq_delay() - test for delay() time-out expired */
epicsShareFunc boolean epicsShareAPI seq_delay(SS_ID pSS, DELAY_ID delayId)
{
	double	timeNow, timeElapsed;
	boolean	expired = FALSE;

	/* Calc. elapsed time since state was entered */
	pvTimeGetCurrentDouble( &timeNow );
	timeElapsed = timeNow - pSS->timeEntered;

	/* Check for delay timeout */
	if ( (timeElapsed >= pSS->delay[delayId]) )
	{
		pSS->delayExpired[delayId] = TRUE; /* mark as expired */
		expired = TRUE;
	}
	DEBUG("seq_delay(%s,%ld): %g seconds, %s\n", pSS->pSSName, delayId,
		pSS->delay[delayId], expired ? "expired": "unexpired");
	return expired;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID pSS, DELAY_ID delayId, double delay)
{
	DEBUG("seq_delayInit(%s,%ld,%g): numDelays=%d, maxNumDelays=%d\n",
		pSS->pSSName, delayId, delay, pSS->numDelays, pSS->maxNumDelays);
	assert(delayId <= pSS->numDelays);
	assert(pSS->numDelays < pSS->maxNumDelays);

	pSS->delay[delayId] = delay;
	pSS->numDelays = max(pSS->numDelays, delayId + 1);
}

/*
 * seq_optGet: return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
epicsShareFunc boolean epicsShareAPI seq_optGet(SS_ID pSS, const char *opt)
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

/* 
 * seq_macValueGet - given macro name, return pointer to its value.
 */
epicsShareFunc char *epicsShareAPI seq_macValueGet(SS_ID ssId, const char *pName)
{
	return seqMacValGet(ssId->sprog, pName);
}
