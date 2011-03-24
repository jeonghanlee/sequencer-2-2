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

/*
 * seq_pvGet() - Get DB value.
 * TODO: add optional timeout argument.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvGet(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG		*sp = ss->sprog;
	CHAN		*ch = sp->chan + varId;
	int		status;
	PVREQ		*req;
	epicsEventId	getSem = ss->getSemId[varId];
	ACHAN		*ach = ch->ach;
	PVMETA		*meta = metaPtr(ch,ss);
	double		tmo = 10.0;

	if ((sp->options & OPT_SAFE) && !ach)
	{
		ss_read_buffer(ss,ch);
		return pvStatOK;
	}
	if (!ach)
	{
		errlogSevPrintf(errlogFatal,
			"pvGet(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}

	if (compType == DEFAULT)
	{
		compType = (sp->options & OPT_ASYNC) ? ASYNC : SYNC;
	}

	/* Check for channel connected */
	if (!ach->connected)
	{
		meta->status = pvStatDISCONN;
		meta->severity = pvSevrINVALID;
		meta->message = "disconnected";
		return meta->status;
	}

	if (compType == SYNC)
	{
		double before, after;
		pvTimeGetCurrentDouble(&before);
		switch (epicsEventWaitWithTimeout(getSem, tmo))
		{
		case epicsEventWaitOK:
			pvTimeGetCurrentDouble(&after);
			tmo -= (after - before);
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvGet(ss %s, var %s, pv %s): failed (timeout "
				"waiting for other get requests to finish)\n",
				ss->ssName, ch->varName, ach->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvGet: epicsEventWaitWithTimeout() failure\n");
			return pvStatERROR;
		}
	}
	else if (compType == ASYNC)
	{
		switch (epicsEventTryWait(getSem))
		{
		case epicsEventWaitOK:
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvGet(ss %s, var %s, pv %s): user error "
				"(there is already a get pending for this variable/"
				"state set combination)\n",
				ss->ssName, ch->varName, ach->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvGet: epicsEventTryWait() failure\n");
			return pvStatERROR;
		}
	}

	/* Allocate and initialize a pv request */
	req = (PVREQ *)freeListMalloc(sp->pvReqPool);
	req->ss = ss;
	req->ch = ch;

	/* Perform the PV get operation with a callback routine specified */
	status = pvVarGetCallback(
			ach->pvid,		/* PV id */
			ch->type->getType,	/* request type */
			(int)ch->count,	/* element count */
			seq_get_handler,	/* callback handler */
			req);			/* user arg */
	if (status != pvStatOK)
	{
		meta->status = pvStatERROR;
		meta->severity = pvSevrMAJOR;
		meta->message = "get failure";
		errlogPrintf("seq_pvGet: pvVarGetCallback() %s failure: %s\n",
			ach->dbName, pvVarGetMess(ach->pvid));
		return status;
	}

	/* Synchronous: wait for completion */
	if (compType == SYNC)
	{
		pvSysFlush(sp->pvSys);
		switch (epicsEventWaitWithTimeout(getSem, tmo))
		{
		case epicsEventWaitOK:
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatTIMEOUT;
			meta->severity = pvSevrMAJOR;
			meta->message = "get completion timeout";
			return meta->status;
		case epicsEventWaitError:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "get completion failure";
			return meta->status;
		}
		epicsEventSignal(getSem);
	}

	return pvStatOK;
}

/*
 * seq_pvGetComplete() - returns whether the last get completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetComplete(SS_ID ss, VAR_ID varId)
{
	epicsEventId	getSem = ss->getSemId[varId];

	switch (epicsEventTryWait(getSem))
	{
	case epicsEventWaitOK:
		epicsEventSignal(getSem);
		return TRUE;
	case epicsEventWaitTimeout:
		return FALSE;
	case epicsEventWaitError:
		errlogPrintf("pvGetComplete: "
		  "epicsEventTryWait(getSemId[%d]) failure\n", varId);
	default:
		return FALSE;
	}
}

static void anonymous_put(SS_ID ss, CHAN *ch)
{
	char	*var = valPtr(ch,ss);	/* ptr to value */

	/* Must lock because multiple writers */
	epicsMutexMustLock(ch->varLock);
	if (ch->queue)
	{
		QUEUE queue = ch->queue;
		pvType type = ch->type->getType;
		size_t size = ch->type->size;
		char value[pv_size_n(type, ch->count)];
		int full;
		DEBUG("seq_pvPut: type=%d, size=%d, count=%d, value=%p, val_ptr=%p, buf_size=%d, q=%p\n",
			type, size, ch->count, value, pv_value_ptr(value, type),
			pv_size_n(type, ch->count), queue);
		print_channel_value(printf, ch, var);
		memcpy(pv_value_ptr(value, type), var, size * ch->count);
		print_channel_value(printf, ch, pv_value_ptr(value, type));
		/* Copy whole message into queue */
		full = seqQueuePut(queue, value);
		if (full)
		{
			errlogSevPrintf(errlogMinor,
			  "pvPut on queued variable %s (anonymous): "
			  "last queue element overwritten (queue is full)\n",
			  ch->varName
			);
		}
	}
	/* check if monitored to mirror behaviour for named PVs */
	else if (ch->monitored)
	{
		ss_write_buffer(ss, ch, var);
	}
	/* Must give varLock before calling seq_efSet, else (possible) deadlock! */
	epicsMutexUnlock(ch->varLock);
	/* Wake up each state set that uses this channel in an event */
	seqWakeup(ss->sprog, ch->eventNum);
	/* If there's an event flag associated with this channel, set it */
	if (ch->efId)
		seq_efSet(ss, ch->efId);
}

/*
 * seq_pvPut() - Put DB value.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvPut(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	int	status;
	int	count;
	char	*var = valPtr(ch,ss);	/* ptr to value */
	PVREQ	*req;
	ACHAN	*ach = ch->ach;
	PVMETA	*meta = metaPtr(ch,ss);
	epicsEventId	putSem = ss->putSemId[varId];
	double	tmo = 10.0;

	DEBUG("seq_pvPut: pv name=%s, var=%p\n", ach ? ach->dbName : "<anonymous>", var);

	/* First handle anonymous PV (safe mode only) */
	if ((sp->options & OPT_SAFE) && !ach)
	{
		anonymous_put(ss, ch);
		return pvStatOK;
	}
	if (!ach)
	{
		errlogSevPrintf(errlogFatal,
			"pvPut(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}

	/* Check for channel connected */
	if (!ach->connected)
	{
		meta->status = pvStatDISCONN;
		meta->severity = pvSevrINVALID;
		meta->message = "disconnected";
		return meta->status;
	}

	/* Determine whether to perform synchronous, asynchronous, or
           plain put
	   ((+a) option was never honored for put, so DEFAULT
	   means non-blocking and therefore implicitly asynchronous) */
	if (compType == SYNC)
	{
		double before, after;
		pvTimeGetCurrentDouble(&before);
		switch (epicsEventWaitWithTimeout(putSem, tmo))
		{
		case epicsEventWaitOK:
			pvTimeGetCurrentDouble(&after);
			tmo -= (after - before);
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvPut(ss %s, var %s, pv %s): failed (timeout "
				"waiting for other put requests to finish)\n",
				ss->ssName, ch->varName, ach->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvPut: epicsEventWaitWithTimeout() failure\n");
			return pvStatERROR;
		}
	}
	else if (compType == ASYNC)
	{
		switch (epicsEventTryWait(putSem))
		{
		case epicsEventWaitOK:
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "already one put pending";
			status = meta->status;
			errlogSevPrintf(errlogFatal,
				"pvPut(ss %s, var %s, pv %s): user error "
				"(there is already a put pending for this variable/"
				"state set combination)\n",
				ss->ssName, ch->varName, ach->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvPut: epicsEventTryWait() failure\n");
			return pvStatERROR;
		}
	}

	/* Determine number of elements to put (don't try to put more
	   than db count) */
	assert(ach->dbCount <= INT_MAX);
	count = (int)ach->dbCount;

	/* Perform the PV put operation (either non-blocking or with a
	   callback routine specified) */
	if (compType == DEFAULT)
	{
		status = pvVarPutNoBlock(
				ach->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				count,			/* element count */
				(pvValue *)var);	/* data value */
	}
	else
	{
		/* Allocate and initialize a pv request */
		req = (PVREQ *)freeListMalloc(sp->pvReqPool);
		req->ss = ss;
		req->ch = ch;

		status = pvVarPutCallback(
				ach->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				count,			/* element count */
				(pvValue *)var,	/* data value */
				seq_put_handler,	/* callback handler */
				req);			/* user arg */
	}
	if (status != pvStatOK)
	{
		errlogPrintf("pvPut: pvVarPut%s() %s failure: %s\n",
			(compType == DEFAULT) ? "NoBlock" : "Callback",
			ach->dbName, pvVarGetMess(ach->pvid));
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (compType == SYNC)
	{
		pvSysFlush(sp->pvSys);
		switch (epicsEventWaitWithTimeout(putSem, tmo))
		{
		case epicsEventWaitOK:
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatTIMEOUT;
			meta->severity = pvSevrMAJOR;
			meta->message = "put completion timeout";
			status = meta->status;
			break;
		case epicsEventWaitError:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "put completion failure";
			status = meta->status;
			break;
		}
		epicsEventSignal(putSem);
	}

	DEBUG("seq_pvPut: status=%d, mess=%s\n", status,
		pvVarGetMess(ach->pvid));
	if (status != pvStatOK)
	{
		DEBUG("pvPut on \"%s\" failed (%d)\n", ach->dbName, status);
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
	boolean		anyDone = FALSE, allDone = TRUE;
	unsigned	n;

	for (n = 0; n < length; n++)
	{
		epicsEventId	putSem = ss->putSemId[varId+n];
		boolean		done = FALSE;

		switch (epicsEventTryWait(putSem))
		{
		case epicsEventWaitOK:
			epicsEventSignal(putSem);
			done = TRUE;
			break;
		case epicsEventWaitError:
			errlogPrintf("pvPutComplete: "
			  "epicsEventTryWait(putSemId[%d]) failure\n", varId);
		case epicsEventWaitTimeout:
			break;
		}

		anyDone = anyDone || done;
		allDone = allDone && done;

		if (complete)
		{
			complete[n] = done;
		}
		else if (any && done)
		{
			break;
		}
	}

	DEBUG("pvPutComplete: varId=%u, length=%u, anyDone=%u, allDone=%u\n",
		varId, length, anyDone, allDone);

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
	pvStat	status = pvStatOK;
	ACHAN	*ach = ch->ach;

	if (!pvName) pvName = "";

	DEBUG("Assign %s to \"%s\"\n", ch->varName, pvName);

	epicsMutexMustLock(sp->programLock);

	if (ach)	/* Disconnect this PV */
	{
		status = pvVarDestroy(ach->pvid);
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarDestroy() %s failure: "
				"%s\n", ach->dbName, pvVarGetMess(ach->pvid));
		}
		free(ach->dbName);
	}

	if (pvName[0] == 0)
	{
		free(ach);
		ch->ach = NULL;
		sp->assignCount -= 1;
		if (ach->connected)
		{
			ach->connected = FALSE;
			sp->connectCount -= 1;
		}
	}
	else		/* Connect */
	{
		if (!ach)
		{
			ch->ach = ach = new(ACHAN);
		}
		ach->dbName = epicsStrDup(pvName);

		sp->assignCount += 1;
		status = pvVarCreate(
			sp->pvSys,		/* PV system context */
			ach->dbName,		/* DB channel name */
			seq_conn_handler,	/* connection handler routine */
			ch,			/* user ptr is CHAN structure */
			0,			/* debug level (inherited) */
			&ach->pvid);		/* ptr to pvid */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarCreate() %s failure: "
				"%s\n", ach->dbName, pvVarGetMess(ach->pvid));
		}
	}

	epicsMutexUnlock(sp->programLock);

	return status;
}

/*
 * seq_pvMonitor() - Initiate monitoring on a channel
 */
epicsShareFunc pvStat epicsShareAPI seq_pvMonitor(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	ACHAN	*ach = ch->ach;

	if (!ach && (sp->options & OPT_SAFE))
	{
		ch->monitored = TRUE;
		return pvStatOK;
	}
	if (!ach)
	{
		errlogSevPrintf(errlogFatal,
			"pvMonitor(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}
	ch->monitored = TRUE;
	return seq_monitor(ch, TRUE);
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStopMonitor(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	ACHAN	*ach = ch->ach;

	if (!ach && (sp->options & OPT_SAFE))
	{
		ch->monitored = FALSE;
		return pvStatOK;
	}
	if (!ach)
	{
		errlogSevPrintf(errlogFatal,
			"pvStopMonitor(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}
	ch->monitored = FALSE;
	return seq_monitor(ch, FALSE);
}

/*
 * seq_pvSync() - Synchronize pv with an event flag.
 * ev_flag == 0 means unSync.
 */
epicsShareFunc void epicsShareAPI seq_pvSync(SS_ID ss, VAR_ID varId, EV_ID ev_flag)
{
	assert(ev_flag >= 0 && ev_flag <= ss->sprog->numEvFlags);
	ss->sprog->chan[varId].efId = ev_flag;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvChannelCount(SS_ID ss)
{
	return ss->sprog->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvConnectCount(SS_ID ss)
{
	return ss->sprog->connectCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvAssignCount(SS_ID ss)
{
	return ss->sprog->assignCount;
}

/* Flush outstanding PV requests */
epicsShareFunc void epicsShareAPI seq_pvFlush(SS_ID ss)
{
	pvSysFlush(ss->sprog->pvSys);
}

/*
 * seq_pvConnected() - returns whether database channel is connected.
 */
epicsShareFunc boolean epicsShareAPI seq_pvConnected(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->ach && ch->ach->connected;
}

/*
 * seq_pvAssigned() - returns whether database channel is assigned.
 */
epicsShareFunc boolean epicsShareAPI seq_pvAssigned(SS_ID ss, VAR_ID varId)
{
	return ss->sprog->chan[varId].ach != NULL;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * the array size and the element count returned by the PV layer.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvCount(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->ach ? ch->ach->dbCount : ch->count;
}

/*
 * seq_pvName() - returns channel name.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->ach ? ch->ach->dbName : NULL;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStatus(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->ach ? meta->status : pvStatOK;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
epicsShareFunc pvSevr epicsShareAPI seq_pvSeverity(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->ach ? meta->severity : pvSevrOK;
}

/*
 * seq_pvMessage() - returns channel error message.
 */
epicsShareFunc const char *epicsShareAPI seq_pvMessage(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->ach ? meta->message : "";
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
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	if (ch->ach)
	{
		return meta->timeStamp;
	}
	else
	{
		epicsTimeStamp ts;
		epicsTimeGetCurrent(&ts);
		return ts;
	}
}

/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;

	DEBUG("seq_efSet: sp=%p, ss=%p, ev_flag=%d\n", sp, ss,
		ev_flag);
	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);

	epicsMutexMustLock(sp->programLock);

	/* Set this bit */
	bitSet(sp->evFlags, ev_flag);

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

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);

	DEBUG("seq_efTest: ev_flag=%d, isSet=%d\n", ev_flag, isSet);

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

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);
	bitClear(sp->evFlags, ev_flag);

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

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);
	bitClear(sp->evFlags, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

struct getq_cp_arg {
	CHAN	*ch;
	void	*var;
	PVMETA	*meta;
};

static void *getq_cp(void *dest, const void *value, size_t elemSize)
{
	struct getq_cp_arg *arg = (struct getq_cp_arg *)dest;
	CHAN	*ch = arg->ch;
	ACHAN	*ach = ch->ach;
	PVMETA	*meta = arg->meta;
	void	*var = arg->var;
	pvType	type = ch->type->getType;
	if (ach)
	{
		assert(pv_is_time_type(type));
		/* Copy status, severity and time stamp */
		meta->status = *pv_status_ptr(value,type);
		meta->severity = *pv_severity_ptr(value,type);
		meta->timeStamp = *pv_stamp_ptr(value,type);
		memcpy(var, pv_value_ptr(value,type), ch->type->size * ch->count);
	}
	return NULL;
}

/*
 * seq_pvGetQ() - Get queued DB value.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetQ(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	void	*var = valPtr(ch,ss);
	EV_ID	ev_flag = ch->efId;
	boolean	isSet;
	ACHAN	*ach = ch->ach;
	PVMETA	*meta = metaPtr(ch,ss);

	epicsMutexMustLock(sp->programLock);

	/* Determine event flag number and whether set */
	isSet = bitTest(sp->evFlags, ev_flag);

	DEBUG("seq_pvGetQ: pv name=%s, isSet=%d\n",
		ach ? ach->dbName : "<anomymous>", isSet);

	/* If set, queue should be non-empty */
	if (isSet)
	{
#if 0
		pvType	type = ch->type->getType;
		char	buffer[pv_size_n(type, ch->count)];
		pvValue	*value = (pvValue *)buffer;
#endif
		struct getq_cp_arg arg = {ch, var, meta};
		QUEUE	queue = ch->queue;
		boolean	empty;

		empty = seqQueueGetF(queue, getq_cp, &arg);
		if (empty)
		{
			errlogSevPrintf(errlogMajor,
				"pvGetQ: event flag set but queue is empty\n");
		}
		else
		{
			if (ach)
			{
#if 0
				assert(pv_is_time_type(type));
				/* Copy status, severity and time stamp */
				meta->status = *pv_status_ptr(value,type);
				meta->severity = *pv_severity_ptr(value,type);
				meta->timeStamp = *pv_stamp_ptr(value,type);
				memcpy(var, pv_value_ptr(value,type), ch->type->size * ch->count);
#endif
				/* If queue is now empty, clear the event flag */
				if (seqQueueIsEmpty(queue))
					bitClear(sp->evFlags, ev_flag);
			}
		}
	}
	epicsMutexUnlock(sp->programLock);

	/* return whether event flag was set on entry */
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
	EV_ID	ev_flag = ch->efId;
	QUEUE	queue = ch->queue;

	DEBUG("seq_pvFlushQ: pv name=%s, count=%d\n", ch->ach ? ch->ach->dbName : "<anomymous>",
		seqQueueUsed(queue));
	seqQueueFlush(queue);

	epicsMutexMustLock(sp->programLock);
	/* Clear event flag */
	bitClear(sp->evFlags, ev_flag);
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
	DEBUG("seq_delay(%s,%u): %g seconds, %s\n", ss->ssName, delayId,
		ss->delay[delayId], expired ? "expired": "unexpired");
	return expired;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID ss, DELAY_ID delayId, double delay)
{
	DEBUG("seq_delayInit(%s,%u,%g): numDelays=%d, maxNumDelays=%d\n",
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

	assert(opt);
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
