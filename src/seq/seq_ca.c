/*	Process Variable (was CA) interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
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


#include "seq.h"

static void proc_db_events(pvValue *value, pvType type,
	CHAN *ch, SSCB *ss, long complete_type);
static void proc_db_events_queued(pvValue *value, CHAN *ch);

/*
 * seq_connect() - Connect to all database channels.
 */
pvStat seq_connect(SPROG *sp)
{
	CHAN		*ch;
        pvStat		status;
	unsigned	nch;

	for (nch = 0, ch = sp->chan; nch < sp->numChans; nch++, ch++)
	{
		if (ch->dbName == NULL || ch->dbName[0] == 0)
			continue; /* skip records without pv names */
		sp->assignCount += 1; /* keep track of number of *assigned* channels */
		if (ch->monFlag) sp->numMonitoredChans++;/*do it before pvVarCreate*/
	}
	/*
	 * For each channel: connect to db & issue monitor (if monFlag is TRUE).
	 */
	for (nch = 0, ch = sp->chan; nch < sp->numChans; nch++, ch++)
	{
		if (ch->dbName == NULL || ch->dbName[0] == 0)
			continue; /* skip records without pv names */
		DEBUG("seq_connect: connect %s to %s\n", ch->varName,
			ch->dbName);
		/* Connect to it */
		status = pvVarCreate(
			pvSys,			/* PV system context */
			ch->dbName,		/* PV name */
			seq_conn_handler,	/* connection handler routine */
			ch,			/* private data is CHAN struc */
			0,			/* debug level (inherited) */
			&ch->pvid );		/* ptr to PV id */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_connect: pvVarCreate() %s failure: "
				"%s\n", ch->dbName, pvVarGetMess(ch->pvid));
			return status;
		}
		ch->assigned = TRUE;
		/* Clear monitor indicator */
		ch->monitored = FALSE;

		/*
		 * Issue monitor request
		 */
		if (ch->monFlag)
		{
			seq_pvMonitor(sp->ss, nch);
		}
	}
	sp->allDisconnected = FALSE;
	pvSysFlush(pvSys);
	return pvStatOK;
}

/*
 * Event completion type (extra argument passed to proc_db_events().
 */
#define	GET_COMPLETE 0
#define	PUT_COMPLETE 1
#define	MON_COMPLETE 2

/*
 * seq_get_handler() - Sequencer callback handler.
 * Called when a "get" completes.
 */
epicsShareFunc void seq_get_handler(
	void *var, pvType type, int count, pvValue *value, void *arg, pvStat status)
{
	PVREQ *req = (PVREQ *)arg;
	CHAN *ch = req->ch;
	SPROG *sp = ch->sprog;

	freeListFree(sp->pvReqPool, arg);
	/* Process event handling in each state set */
	proc_db_events(value, type, ch, req->ss, GET_COMPLETE);
}

/*
 * seq_put_handler() - Sequencer callback handler.
 * Called when a "put" completes.
 */
epicsShareFunc void seq_put_handler(
	void *var, pvType type, int count, pvValue *value, void *arg, pvStat status)
{
	PVREQ *req = (PVREQ *)arg;
	CHAN *ch = req->ch;
	SPROG *sp = ch->sprog;

	freeListFree(sp->pvReqPool, arg);
	/* Process event handling in each state set */
	proc_db_events(value, type, ch, req->ss, PUT_COMPLETE);
}

/*
 * seq_mon_handler() - PV events (monitors) come here.
 */
epicsShareFunc void seq_mon_handler(
	void *var, pvType type, int count, pvValue *value, void *arg, pvStat status)
{
	CHAN *ch = (CHAN *)arg;
	SPROG *sp = ch->sprog;

	/* Process event handling in each state set */
	proc_db_events(value, type, ch, sp->ss, MON_COMPLETE);
	if(!ch->gotFirstMonitor)
	{
		ch->gotFirstMonitor = 1;
		sp->firstMonitorCount++;
		if((sp->firstMonitorCount==sp->numMonitoredChans)
			&& (sp->firstConnectCount==sp->assignCount))
		{
			SSCB *ss;
			unsigned nss;
			for(nss=0, ss=sp->ss; nss<sp->numSS; nss++,ss++)
			{
				epicsEventSignal(ss->allFirstConnectAndMonitorSemId);
			}
		}
	}
}

/* Common code for completion and monitor handling */
static void proc_db_events(
	pvValue	*value,
	pvType	type,
	CHAN	*ch,
	SSCB	*ss,		/* originator, for put and get */
	long	complete_type)
{
	SPROG	*sp = ch->sprog;

	DEBUG("proc_db_events: var=%s, pv=%s, type=%s\n", ch->varName,
		ch->dbName, complete_type==0?"get":complete_type==1?"put":"mon");

	/* If monitor on var queued via syncQ, branch to alternative routine */
	if (ch->queued && complete_type == MON_COMPLETE)
	{
		proc_db_events_queued(value, ch);
		return;
	}

	/* Copy value returned into user variable (can get NULL value pointer
	   for put completion only) */
	if (value != NULL)
	{
		void *val = pv_value_ptr(value, type);

		/* Write value to CA buffer (lock-free) */
		ss_write_buffer(ch, val);
		if (pv_is_time_type(type))
		{
			ch->status = *pv_status_ptr(value, type);
			ch->severity = *pv_severity_ptr(value, type);
			ch->timeStamp = *pv_stamp_ptr(value, type);
		}
		/* Copy error message (only when severity indicates error) */
		if (ch->severity != pvSevrNONE)
		{
			const char *pmsg = pvVarGetMess(ch->pvid);
			if (!pmsg) pmsg = "unknown";
			ch->message = pmsg;
		}
	}

	/* Indicate completed pvGet() or pvPut()  */
	switch (complete_type)
	{
	    case GET_COMPLETE:
		ch->getComplete[ssNum(ss)] = TRUE;
		break;
	    case PUT_COMPLETE:
		break;
	    default:
		break;
	}

	/* Wake up each state set that uses this channel in an event */
	seqWakeup(sp, ch->eventNum);

	/* If there's an event flag associated with this channel, set it */
	/* TODO: check if correct/documented to do this for non-monitor completions */
	if (ch->efId > 0)
		seq_efSet(ss, ch->efId);

	/* Give semaphore for completed synchronous pvGet() and pvPut() */
	switch (complete_type)
	{
	    case GET_COMPLETE:
		epicsEventSignal(ss->getSemId);
		break;
	    case PUT_COMPLETE:
		epicsEventSignal(ch->putSemId);
		break;
	    default:
		break;
	}
}

/* Common code for event and callback handling (queuing version) */
static void proc_db_events_queued(pvValue *value, CHAN *ch)
{
	QENTRY	*entry;
	SPROG	*sp;
	int	count;

	/* Get ptr to the state program that owns this db entry */
	sp = ch->sprog;

	/* Determine number of items currently on the queue */
	count = ellCount(&sp->queues[ch->queueIndex]);

	DEBUG("proc_db_events_queued: var=%s, pv=%s, count(max)=%d(%d), "
		"index=%d\n", ch->varName, ch->dbName, count,
		ch->maxQueueSize, ch->queueIndex);

	/* Allocate queue entry (re-use last one if queue has reached its
	   maximum size) */
	assert(count >= 0);
	if ( (unsigned)count < ch->maxQueueSize )
	{
		entry = (QENTRY *) calloc(sizeof(QENTRY), 1);
		if (entry == NULL)
		{
			errlogPrintf("proc_db_events_queued: %s queue memory "
				"allocation failure\n", ch->varName);
			return;
		}
		ellAdd(&sp->queues[ch->queueIndex], (ELLNODE *) entry);
	}
	else
	{
		entry = (QENTRY *) ellLast(&sp->queues[ch->queueIndex]);
		if (entry == NULL)
		{
			errlogPrintf("proc_db_events_queued: %s queue "
				"inconsistent failure\n", ch->varName);
			return;
		}
	}

	/* Copy channel id, value and associated information into queue
	   entry (NB, currently only copy _first_ value for arrays) */
	entry->ch = ch;
	memcpy((char *)&entry->value, (char *)value, sizeof(entry->value));

	/* Set the event flag associated with this channel */
	DEBUG("setting event flag %ld\n", ch->efId);
	seq_efSet(sp->ss, ch->efId);
}

/* Disconnect all database channels */
pvStat seq_disconnect(SPROG *sp)
{
	CHAN	*ch;
	unsigned nch;
	SPROG	*mySP; /* will be NULL if this is not a sequencer thread */

	/* Did we already disconnect? */
	if (sp->allDisconnected)
		return pvStatOK;
	DEBUG("seq_disconnect: sp = %p\n", sp);

	/* Attach to PV context of pvSys creator (auxiliary thread) */
	mySP = seqFindProg(epicsThreadGetIdSelf());
	if (mySP == NULL)
	{
#ifdef	DEBUG_DISCONNECT
		errlogPrintf("seq_disconnect: pvSysAttach(pvSys)\n");
#endif	/*DEBUG_DISCONNECT*/
		/* not a sequencer thread */
		pvSysAttach(pvSys);
	}

	ch = sp->chan;
#ifdef	DEBUG_DISCONNECT
	errlogPrintf("seq_disconnect: sp = %p, ch = %p\n", sp, ch);
#endif	/*DEBUG_DISCONNECT*/

	for (nch = 0; nch < sp->numChans; nch++, ch++)
	{
		pvStat	status;

		if (!ch->assigned)
			continue;
		DEBUG("seq_disconnect: disconnect %s from %s\n",
 			ch->varName, ch->dbName);
		/* Disconnect this PV */
		status = pvVarDestroy(ch->pvid);
		if (status != pvStatOK)
		    errlogPrintf("seq_disconnect: pvVarDestroy() %s failure: "
				"%s\n", ch->dbName, pvVarGetMess(ch->pvid));

		/* Clear monitor & connect indicators */
		ch->monitored = FALSE;
		ch->connected = FALSE;
	}

	sp->allDisconnected = TRUE;

	pvSysFlush(pvSys);

	return pvStatOK;
}

/*
 * seq_conn_handler() - Sequencer connection handler.
 * Called each time a connection is established or broken.
 */
void seq_conn_handler(void *var,int connected)
{
	CHAN	*ch;
	SPROG	*sp;

	/* Private data is db ptr (specified at pvVarCreate()) */
	ch = (CHAN *)pvVarGetPrivate(var);

	/* State program that owns this db entry */
	sp = ch->sprog;

	/* If PV not connected */
	if (!connected)
	{
		DEBUG("%s disconnected from %s\n", ch->varName, ch->dbName);
		if (ch->connected)
		{
			ch->connected = FALSE;
			sp->connectCount--;
			ch->monitored = FALSE;
		}
		else
		{
			errlogPrintf("%s disconnected but already disconnected %s\n",
				ch->varName, ch->dbName);
		}
	}
	else	/* PV connected */
	{
		DEBUG("%s connected to %s\n", ch->varName,ch->dbName);
		if (!ch->connected)
		{
			ch->connected = TRUE;
			sp->connectCount++;
			if (ch->monFlag)
				ch->monitored = TRUE;
			ch->dbCount = (unsigned)pvVarGetCount(var);
			if (ch->dbCount > ch->count)
				ch->dbCount = ch->count;
		}
		else
		{
			printf("%s connected but already connected %s\n",
				ch->varName,ch->dbName);
		}
		if(!ch->gotFirstConnect)
		{
			ch->gotFirstConnect = 1;
			sp->firstConnectCount++;
			if((sp->firstMonitorCount==sp->numMonitoredChans)
				&& (sp->firstConnectCount==sp->assignCount))
			{
				SSCB *ss;
				unsigned nss;
				for(nss=0, ss=sp->ss; nss<sp->numSS; nss++,ss++)
				{
					epicsEventSignal(ss->allFirstConnectAndMonitorSemId);
				}
			}
		}
	}

	/* Wake up each state set that is waiting for event processing */
	seqWakeup(sp, 0);
}
