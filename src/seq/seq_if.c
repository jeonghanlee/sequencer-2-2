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
epicsShareFunc void epicsShareAPI seq_pvFlush(SS_ID ss)
{
	pvSysFlush(pvSys);
}	

/*
 * seq_pvGet() - Get DB value.
 * TODO: add optional timeout argument.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvGet(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	int	sync;	/* whether synchronous get */
	pvStat	status;
	PVREQ	*req;

	/* Determine whether performing asynchronous or synchronous i/o */
	switch (compType)
	{
	case DEFAULT:
		sync = !(sp->options & OPT_ASYNC);
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
	ch->getComplete = FALSE;

	/* Check for channel connected */
	if (!ch->connected)
	{
		ch->status = pvStatDISCONN;
		ch->severity = pvSevrINVALID;
		ch->message = "disconnected";
		return ch->status;
	}

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if (sync)
	{
		epicsEventTryWait(ss->getSemId);
	}

	/* Allocate and initialize a pv request */
	req = (PVREQ *)freeListMalloc(sp->pvReqPool);
	req->ss = ss;
	req->ch = ch;

	/* Perform the PV get operation with a callback routine specified */
	status = pvVarGetCallback(
			ch->pvid,		/* PV id */
			ch->type->getType,	/* request type */
			(int)ch->count,	/* element count */
			seq_get_handler,	/* callback handler */
			req);			/* user arg */
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvGet: pvVarGetCallback() %s failure: %s\n",
			ch->dbName, pvVarGetMess(ch->pvid));
		ch->getComplete[ssNum(ss)] = TRUE;
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (sync)
	{
		epicsEventWaitStatus sem_status;

		pvSysFlush(pvSys);
		sem_status = epicsEventWaitWithTimeout(ss->getSemId, 10.0);
		if (sem_status != epicsEventWaitOK)
		{
			ch->status = pvStatTIMEOUT;
			ch->severity = pvSevrMAJOR;
			ch->message = "get completion timeout";
			return ch->status;
		}
	}
        
	return pvStatOK;
}

/*
 * seq_pvGetComplete() - returns whether the last get completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetComplete(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->getComplete[ssNum(ss)];
}

/*
 * seq_pvPut() - Put DB value.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvPut(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	int	nonb;	/* whether to call pvVarPutNoBlock (=fire&forget) */
	int	sync;	/* whether to wait for completion */
	int	status;
	unsigned count;
	char	*var = valPtr(ch,ss);	/* ptr to value */
	PVREQ	*req;

	DEBUG("seq_pvPut: pv name=%s, var=%p\n", ch->dbName, var);

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
		epicsEventWait(ch->putSemId);
	}

	/* Check for channel connected */
	/* TODO: is this needed? */
	if (!ch->connected)
	{
		ch->status = pvStatDISCONN;
		ch->severity = pvSevrINVALID;
		ch->message = "disconnected";
		return ch->status;
	}

	/* Determine number of elements to put (don't try to put more
	   than db count) */
	count = min(ch->count, ch->dbCount);

	/* Perform the PV put operation (either non-blocking or with a
	   callback routine specified) */
	if (nonb)
	{
		status = pvVarPutNoBlock(
				ch->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				(int)count,		/* element count */
				(pvValue *)var);	/* data value */
	}
	else
	{
		/* Allocate and initialize a pv request */
		req = (PVREQ *)freeListMalloc(sp->pvReqPool);
		req->ss = ss;
		req->ch = ch;

		status = pvVarPutCallback(
				ch->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				(int)count,		/* element count */
				(pvValue *)var,	/* data value */
				seq_put_handler,	/* callback handler */
				req);			/* user arg */
	}
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvPut: pvVarPut%s() %s failure: %s\n",
			nonb ? "NoBlock" : "Callback", ch->dbName,
			pvVarGetMess(ch->pvid));
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (sync) /* => !nonb */
	{
		epicsEventWaitStatus sem_status;

		pvSysFlush(pvSys);
		sem_status = epicsEventWaitWithTimeout(ch->putSemId, 10.0);
		if (sem_status != epicsEventWaitOK)
		{
			ch->status = pvStatTIMEOUT;
			ch->severity = pvSevrMAJOR;
			ch->message = "put completion timeout";
			return ch->status;
		}
	}

	DEBUG("seq_pvPut: status=%d, mess=%s\n", status,
		pvVarGetMess(ch->pvid));
	if (status != pvStatOK)
	{
		DEBUG("pvPut on \"%s\" failed (%d)\n", ch->dbName, status);
		DEBUG("  putType=%d\n", ch->type->putType);
		DEBUG("  size=%d, count=%d\n", ch->type->size, count);
	}

	return pvStatOK;
}

/*
 * seq_pvPutComplete() - returns whether the last put completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvPutComplete(
	SS_ID		ss,
	VAR_ID		varId,
	unsigned	length,
	boolean		any,
	boolean		*complete)
{
	SPROG	*sp = ss->sprog;
	long	anyDone = FALSE, allDone = TRUE;
	unsigned i;

	for (i=0; i<length; i++)
	{
		CHAN *ch = sp->chan + varId + i;

		long done = epicsEventTryWait(ch->putSemId);

		anyDone = anyDone || done;
		allDone = allDone && done;

		if (complete != NULL)
		{
			complete[i] = done;
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
epicsShareFunc pvStat epicsShareAPI seq_pvAssign(SS_ID ss, VAR_ID varId, const char *pvName)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	pvStat	status;
        unsigned nchar;

	DEBUG("Assign %s to \"%s\"\n", ch->varName, pvName);

	if (ch->assigned)
	{	/* Disconnect this PV */
		status = pvVarDestroy(ch->pvid);
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarDestroy() %s failure: "
				"%s\n", ch->dbName, pvVarGetMess(ch->pvid));
		}
		free(ch->dbName);
		ch->assigned = FALSE;
		sp->assignCount -= 1;
	}

	if (ch->connected)
	{
		ch->connected = FALSE;
		sp->connectCount -= 1;
	}
	ch->monitored = FALSE;
	nchar = strlen(pvName);
	ch->dbName = (char *)calloc(1, nchar + 1);
	strcpy(ch->dbName, pvName);

	/* Connect */
	if (nchar > 0)
	{
		ch->assigned = TRUE;
		sp->assignCount += 1;
		status = pvVarCreate(
			pvSys,			/* PV system context */
			ch->dbName,		/* DB channel name */
			seq_conn_handler,	/* connection handler routine */
			ch,			/* user ptr is CHAN structure */
			0,			/* debug level (inherited) */
			&ch->pvid);		/* ptr to pvid */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarCreate() %s failure: "
				"%s\n", ch->dbName, pvVarGetMess(ch->pvid));
			return status;
		}

		if (ch->monFlag)
		{
			status = seq_pvMonitor(ss, varId);
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
epicsShareFunc pvStat epicsShareAPI seq_pvMonitor(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	int	status;

	DEBUG("seq_pvMonitor \"%s\"\n", ch->dbName);

/*	if (ch->monitored || !ch->assigned)	*/
/*	WFL, 96/08/07, don't check monitored because it can get set TRUE */
/*	in the connection handler before this routine is executed; this */
/*	fix pending a proper fix */
	if (!ch->assigned)
		return pvStatOK;

	status = pvVarMonitorOn(
		ch->pvid,		/* pvid */
		ch->type->getType,	/* requested type */
		(int)ch->count,	/* element count */
		seq_mon_handler,	/* function to call */
		ch,			/* user arg (db struct) */
		&ch->monid);		/* where to put event id */

	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvMonitor: pvVarMonitorOn() %s failure: %s\n",
			ch->dbName, pvVarGetMess(ch->pvid));
		return status;
	}
	pvSysFlush(pvSys);

	ch->monitored = TRUE;
	return pvStatOK;
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStopMonitor(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	int	status;

	if (!ch->monitored)
		return -1;

	status = pvVarMonitorOff(ch->pvid,ch->monid);
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvStopMonitor: pvVarMonitorOff() %s failure: "
			"%s\n", ch->dbName, pvVarGetMess(ch->pvid));
		return status;
	}

	ch->monitored = FALSE;

	return status;
}

/*
 * seq_pvSync() - Synchronize pv with an event flag.
 * ev_flag == 0 means unSync.
 */
epicsShareFunc void epicsShareAPI seq_pvSync(SS_ID ss, VAR_ID varId, EV_ID ev_flag)
{
	assert(ev_flag >= 0 && ev_flag <= ss->sprog->numEvents);
	ss->sprog->chan[varId].efId = ev_flag;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvChannelCount(SS_ID ss)
{
	SPROG	*sp = ss->sprog;

	return sp->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvConnectCount(SS_ID ss)
{
	SPROG	*sp = ss->sprog;

	return sp->connectCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvAssignCount(SS_ID ss)
{
	SPROG	*sp = ss->sprog;

	return sp->assignCount;
}

/*
 * seq_pvConnected() - returns whether database channel is connected.
 */
epicsShareFunc boolean epicsShareAPI seq_pvConnected(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->connected;
}

/*
 * seq_pvAssigned() - returns whether database channel is assigned.
 */
epicsShareFunc boolean epicsShareAPI seq_pvAssigned(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->assigned;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * the array size and the element count returned by the PV layer.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvCount(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->dbCount;
}

/*
 * seq_pvName() - returns channel name.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->dbName;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStatus(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->status;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
epicsShareFunc pvSevr epicsShareAPI seq_pvSeverity(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->severity;
}

/*
 * seq_pvMessage() - returns channel error message.
 */
epicsShareFunc const char *epicsShareAPI seq_pvMessage(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->message;
}

/*
 * seq_pvIndex() - returns index of database variable.
 */
epicsShareFunc VAR_ID epicsShareAPI seq_pvIndex(SS_ID ss, VAR_ID varId)
{
	return varId; /* index is same as varId */
}

/*
 * seq_pvTimeStamp() - returns channel time stamp.
 */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;

	return ch->timeStamp;
}
/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;

	epicsMutexMustLock(sp->programLock);

	DEBUG("seq_efSet: sp=%p, ss=%p, ev_flag=%ld\n", sp, ss,
		ev_flag);

	/* Set this bit */
	bitSet(sp->events, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(sp, ev_flag);

	epicsMutexUnlock(sp->programLock);
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
epicsShareFunc boolean epicsShareAPI seq_efTest(SS_ID ss, EV_ID ev_flag)
/* event flag */
{
	SPROG	*sp = ss->sprog;
	int	isSet;

	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->events, ev_flag);

	DEBUG("seq_efTest: ev_flag=%ld, event=%#lx, isSet=%d\n",
		ev_flag, sp->events[0], isSet);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

/*
 * seq_efClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc boolean epicsShareAPI seq_efClear(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;
	int	isSet;

	isSet = bitTest(sp->events, ev_flag);

	epicsMutexMustLock(sp->programLock);

	/* Clear this bit */
	bitClear(sp->events, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(sp, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

/*
 * seq_efTestAndClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc boolean epicsShareAPI seq_efTestAndClear(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;
	int	isSet;

	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->events, ev_flag);
	bitClear(sp->events, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

/*
 * seq_pvGetQ() - Get queued DB value.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetQ(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	char	*var = valPtr(ch,ss);
	EV_ID	ev_flag;
	boolean	isSet;

	epicsMutexMustLock(sp->programLock);

	/* Determine event flag number and whether set */
	ev_flag = ch->efId;
	isSet = bitTest(sp->events, ev_flag);

	DEBUG("seq_pvGetQ: pv name=%s, isSet=%d\n", ch->dbName, isSet);

	/* If set, queue should be non empty */
	if (isSet)
	{
		QENTRY	*entry;
		pvValue	*access;
		void	*val;

		/* Dequeue first entry */
		entry = (QENTRY *) ellGet(&sp->queues[ch->queueIndex]);

		/* If none, "impossible" */
		if (entry == NULL)
		{
			errlogPrintf("seq_pvGetQ: evflag set but queue empty "
				"(impossible)\n");
			isSet = FALSE;
		}

		/* Extract information from entry (code from seq_ca.c)
		   (ch changed to refer to channel for which monitor
		   was posted) */
		else
		{
		        pvType type = ch->type->getType;

			ch = entry->ch;
			access = &entry->value;

			/* Copy value returned into user variable */
			/* For now, can only return _one_ array element */
			val = pv_value_ptr(access, type);

			epicsMutexLock(ch->varLock);
			memcpy(var, val, ch->type->size * 1 );
							/* was ch->dbCount */
			if (pv_is_time_type(type))
			{
				ch->status = *pv_status_ptr(access, type);
				ch->severity = *pv_severity_ptr(access, type);
				ch->timeStamp = *pv_stamp_ptr(access, type);
			}
			epicsMutexUnlock(ch->varLock);

			/* Free queue entry */
			free(entry);
		}
	}

	/* If queue is now empty, clear the event flag */
	if (ellCount(&sp->queues[ch->queueIndex]) == 0)
	{
		bitClear(sp->events, ev_flag);
	}

	epicsMutexUnlock(sp->programLock);

	/* return TRUE iff event flag was set on entry */
	return isSet;
}

/*
 * seq_pvFreeQ() - Same as seq_pvFlushQ.
 *		   Provided for compatibility.
 */
epicsShareFunc void epicsShareAPI seq_pvFreeQ(SS_ID ss, VAR_ID varId)
{
	seq_pvFreeQ(ss, varId);
}

/*
 * seq_pvFlushQ() - Flush elements on syncQ queue and clear event flag.
 */
epicsShareFunc void epicsShareAPI seq_pvFlushQ(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	QENTRY	*entry;
	EV_ID	ev_flag;

	epicsMutexMustLock(sp->programLock);

	DEBUG("seq_pvFreeQ: pv name=%s, count=%d\n", ch->dbName,
		ellCount(&sp->queues[ch->queueIndex]));

	/* Determine event flag number */
	ev_flag = ch->efId;

	/* Free queue elements */
	while((entry = (QENTRY *)ellGet(&sp->queues[ch->queueIndex])) != NULL)
		free(entry);

	/* Clear event flag */
	bitClear(sp->events, ev_flag);

	epicsMutexUnlock(sp->programLock);
}


/* seq_delay() - test for delay() time-out expired */
epicsShareFunc boolean epicsShareAPI seq_delay(SS_ID ss, DELAY_ID delayId)
{
	double	timeNow, timeElapsed;
	boolean	expired = FALSE;

	/* Calc. elapsed time since state was entered */
	pvTimeGetCurrentDouble( &timeNow );
	timeElapsed = timeNow - ss->timeEntered;

	/* Check for delay timeout */
	if ( (timeElapsed >= ss->delay[delayId]) )
	{
		ss->delayExpired[delayId] = TRUE; /* mark as expired */
		expired = TRUE;
	}
	DEBUG("seq_delay(%s,%ld): %g seconds, %s\n", ss->ssName, delayId,
		ss->delay[delayId], expired ? "expired": "unexpired");
	return expired;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID ss, DELAY_ID delayId, double delay)
{
	DEBUG("seq_delayInit(%s,%ld,%g): numDelays=%d, maxNumDelays=%d\n",
		ss->ssName, delayId, delay, ss->numDelays, ss->maxNumDelays);
	assert(delayId <= ss->numDelays);
	assert(ss->numDelays < ss->maxNumDelays);

	ss->delay[delayId] = delay;
	ss->numDelays = max(ss->numDelays, delayId + 1);
}

/*
 * seq_optGet: return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
epicsShareFunc boolean epicsShareAPI seq_optGet(SS_ID ss, const char *opt)
{
	SPROG	*sp = ss->sprog;

	switch (opt[0])
	{
	case 'a': return ( (sp->options & OPT_ASYNC) != 0);
	case 'c': return ( (sp->options & OPT_CONN)  != 0);
	case 'd': return ( (sp->options & OPT_DEBUG) != 0);
	case 'e': return ( (sp->options & OPT_NEWEF) != 0);
	case 'm': return ( (sp->options & OPT_MAIN)  != 0);
	case 'r': return ( (sp->options & OPT_REENT) != 0);
	case 's': return ( (sp->options & OPT_SAFE)  != 0);
	default:  return FALSE;
	}
}

/* 
 * seq_macValueGet - given macro name, return pointer to its value.
 */
epicsShareFunc char *epicsShareAPI seq_macValueGet(SS_ID ssId, const char *name)
{
	return seqMacValGet(ssId->sprog, name);
}
