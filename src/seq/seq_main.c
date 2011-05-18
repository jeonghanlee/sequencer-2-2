/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California
	and the University of Chicago.
	Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************/
#include "seq.h"

/* #define DEBUG printf */
#define DEBUG nothing

static void init_sprog(SPROG *sp, seqProgram *seqProg);
static void init_sscb(SPROG *sp, SSCB *ss, seqSS *seqSS);
static void init_chan(SPROG *sp, CHAN *ch, seqChan *seqChan);
static PVTYPE *find_type(const char *userType);

/*
 * seq: Run a state program.
 * Usage:  seq(<sp>, <macros string>, <stack size>)
 *	sp is the ptr to the state program structure.
 *	Example:  seq(&myprog, "logfile=mylog", 0)
 * When called from the shell, the 2nd & 3rd parameters are optional.
 *
 * Creates the initial state program thread and returns its thread id.
 * Most initialization is performed here.
 */
epicsShareFunc void epicsShareAPI seq(
	seqProgram *seqProg, const char *macroDef, unsigned stackSize)
{
	epicsThreadId	tid;
	SPROG		*sp;
	char		*str;
	const char	*threadName;
	unsigned int	smallStack;

	/* Print version & date of sequencer */
	printf(SEQ_VERSION "\n");

	/* Exit if no parameters specified */
	if (seqProg == 0)
	{
		errlogPrintf("Bad first argument seqProg (is NULL)\n");
		return;
	}

	/* Check for correct state program format */
	if (seqProg->magic != MAGIC)
	{	/* Oops */
		errlogPrintf("Illegal magic number in state program.\n");
		errlogPrintf(" - Possible mismatch between SNC & SEQ "
			"versions\n");
		errlogPrintf(" - Re-compile your program?\n");
		epicsThreadSleep(1.0);	/* let error messages get printed */
		return;
	}

	sp = new(SPROG);

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(sp, seqProg->params);

	/* Parse the macro definitions from the command line */
	seqMacParse(sp, macroDef);

	/* Initialize program struct */
	init_sprog(sp, seqProg);

	/* Specify stack size */
	if (stackSize == 0)
		stackSize = epicsThreadGetStackSize(THREAD_STACK_SIZE);
	str = seqMacValGet(sp, "stack");
	if (str && str[0] != '\0')
	{
		sscanf(str, "%ud", &stackSize);
	}
	smallStack = epicsThreadGetStackSize(epicsThreadStackSmall);
	if (stackSize < smallStack)
		stackSize = smallStack;
	sp->stackSize = stackSize;

	/* Specify thread name */
	str = seqMacValGet(sp, "name");
	if (str && str[0] != '\0')
		threadName = str;
	else
		threadName = sp->progName;

	/* Specify PV system name (defaults to CA) */
	str = seqMacValGet(sp, "pvsys");
	if (str && str[0] != '\0')
		sp->pvSysName = str;
	else
		sp->pvSysName = "ca";

	/* Determine debug level (currently only used for PV-level debugging) */
	str = seqMacValGet(sp, "debug");
	if (str && str[0] != '\0')
		sp->debug = atoi(str);
	else
		sp->debug = 0;

	/* Specify thread priority */
	sp->threadPriority = THREAD_PRIORITY;
	str = seqMacValGet(sp, "priority");
	if (str && str[0] != '\0')
	{
		sscanf(str, "%ud", &(sp->threadPriority));
	}
	if (sp->threadPriority > THREAD_PRIORITY)
		sp->threadPriority = THREAD_PRIORITY;

	tid = epicsThreadCreate(threadName, sp->threadPriority,
		sp->stackSize, sequencer, sp);

	printf("Spawning sequencer program \"%s\", thread %p: \"%s\"\n",
		sp->progName, tid, threadName);
}

/*
 * Copy data from seqCom.h structures into this thread's dynamic structures
 * as defined in seq.h.
 */
static void init_sprog(SPROG *sp, seqProgram *seqProg)
{
	unsigned nss, nch;

	/* Copy information for state program */
	sp->numSS = seqProg->numSS;
	sp->numChans = seqProg->numChans;
	sp->numEvFlags = seqProg->numEvFlags;
	sp->options = seqProg->options;
	sp->progName = seqProg->progName;
	sp->initFunc = seqProg->initFunc;
	sp->entryFunc = seqProg->entryFunc;
	sp->exitFunc = seqProg->exitFunc;
	sp->varSize = seqProg->varSize;
	sp->numQueues = seqProg->numQueues;

	/* Allocate user variable area if reentrant option (+r) is set */
	if (sp->options & OPT_REENT)
		sp->var = (USER_VAR *)calloc(1, sp->varSize);
	else
		sp->var = NULL;

	DEBUG("init_sprog: numSS=%d, numChans=%d, numEvFlags=%u, "
		"progName=%s, varSize=%u\n", sp->numSS, sp->numChans,
		sp->numEvFlags, sp->progName, sp->varSize);

	/* Create semaphores */
	sp->programLock = epicsMutexMustCreate();
	sp->ready = epicsEventMustCreate(epicsEventEmpty);
	sp->dead = epicsEventMustCreate(epicsEventEmpty);

	/* Allocate an array for event flag bits. Note this does
	   *not* reserve space for all event numbers (i.e. including
	   channels), only for event flags. */
	sp->evFlags = newArray(bitMask, NWORDS(sp->numEvFlags));

	/* Allocate and initialize syncQ queues */
	if (sp->numQueues > 0)
	{
		sp->queues = newArray(QUEUE, sp->numQueues);
	}
	/* Initial pool for pv requests is 1kB on 32-bit systems */
	freeListInitPvt(&sp->pvReqPool, 128, sizeof(PVREQ));

	/* Allocate array of state set structs and initialize it */
	sp->ss = newArray(SSCB, sp->numSS);
	for (nss = 0; nss < sp->numSS; nss++)
	{
		init_sscb(sp, sp->ss + nss, seqProg->ss + nss);
	}

	/* Allocate array of channel structs and initialize it */
	sp->chan = newArray(CHAN, sp->numChans);
	for (nch = 0; nch < sp->numChans; nch++)
	{
		init_chan(sp, sp->chan + nch, seqProg->chan + nch);
	}
}

/*
 * Initialize a state set control block
 */
static void init_sscb(SPROG *sp, SSCB *ss, seqSS *seqSS)
{
	unsigned nch;

	/* Fill in SSCB */
	ss->ssName = seqSS->ssName;
	ss->numStates = seqSS->numStates;
	ss->maxNumDelays = seqSS->numDelays;

	ss->delay = newArray(double, ss->maxNumDelays);
	ss->delayExpired = newArray(boolean, ss->maxNumDelays);
	ss->currentState = 0; /* initial state */
	ss->nextState = 0;
	ss->prevState = 0;
	ss->threadId = 0;
	/* Initialize to start time rather than zero time! */
	pvTimeGetCurrentDouble(&ss->timeEntered);
	ss->sprog = sp;

	ss->syncSemId = epicsEventMustCreate(epicsEventEmpty);

	ss->getSemId = newArray(epicsEventId, sp->numChans);
	for (nch = 0; nch < sp->numChans; nch++)
		ss->getSemId[nch] = epicsEventMustCreate(epicsEventFull);

	ss->putSemId = newArray(epicsEventId, sp->numChans);
	for (nch = 0; nch < sp->numChans; nch++)
		ss->putSemId[nch] = epicsEventMustCreate(epicsEventFull);
	ss->dead = epicsEventMustCreate(epicsEventEmpty);

	/* No need to copy the state structs, they can be shared
	   because nothing gets mutated. */
	ss->states = seqSS->states;

	/* Allocate separate user variable area if safe mode option (+s) is set */
	if (sp->options & OPT_SAFE)
	{
		ss->dirty = newArray(boolean, sp->numChans);
		ss->var = (USER_VAR *)calloc(1, sp->varSize);
	}
	else
	{
		ss->dirty = NULL;
		ss->var = sp->var;
	}
}

/*
 * Build the database channel structures.
 */
static void init_chan(SPROG *sp, CHAN *ch, seqChan *seqChan)
{
	DEBUG("init_chan: ch=%p\n", ch);
	ch->sprog = sp;
	ch->varName = seqChan->varName;
	ch->offset = seqChan->offset;
	ch->count = seqChan->count;
	if (ch->count == 0) ch->count = 1;
	ch->efId = seqChan->efId;
	ch->monitored = seqChan->monitored;
	ch->eventNum = seqChan->eventNum;

	/* Fill in request type info */
	ch->type = find_type(seqChan->varType);

	DEBUG("  varname=%s, count=%u\n"
		"  efId=%u, monitored=%u, eventNum=%u\n",
		ch->varName, ch->count,
		ch->efId, ch->monitored, ch->eventNum);
	DEBUG("  type=%p: typeStr=%s, putType=%d, getType=%d, size=%d\n",
		ch->type, ch->type->typeStr,
		ch->type->putType, ch->type->getType, ch->type->size);

	if (seqChan->chName)
	{
		char name_buffer[100];

		seqMacEval(sp, seqChan->chName, name_buffer, sizeof(name_buffer));
		if (name_buffer[0])
		{
			DBCHAN	*dbch = new(DBCHAN);
			dbch->dbName = epicsStrDup(name_buffer);
			if (sp->options & OPT_SAFE)
				dbch->ssMetaData = newArray(PVMETA, sp->numSS);
			ch->dbch = dbch;
			DEBUG("  assigned name=%s, expanded name=%s\n",
				seqChan->chName, ch->dbch->dbName);
		}
	}

	if (!ch->dbch)
	{
		DEBUG("  pv name=<anonymous>\n");
	}

	if (seqChan->queued)
	{
		/* We want to store the whole pv message in the queue,
		   so that we can extract status etc when we remove
		   the message. */
		size_t size = pv_size_n(ch->type->getType, ch->count);

		if (sp->queues[seqChan->queueIndex] == NULL)
			sp->queues[seqChan->queueIndex] =
				seqQueueCreate(seqChan->queueSize, size);
		else
		{
			assert(seqQueueNumElems(sp->queues[seqChan->queueIndex])
				== seqChan->queueSize);
			assert(seqQueueElemSize(sp->queues[seqChan->queueIndex])
				== size);
		}
		ch->queue = sp->queues[seqChan->queueIndex];
		DEBUG("  queued=%d, queueSize=%d, queueIndex=%d, queue=%p\n",
			seqChan->queued, seqChan->queueSize,
			seqChan->queueIndex, ch->queue);
		DEBUG("  queue->numElems=%d, queue->elemSize=%d\n",
			seqQueueNumElems(ch->queue), seqQueueElemSize(ch->queue));
	}
	ch->varLock = epicsMutexMustCreate();
}

/*
 * find_type() -- returns types for DB put/get, element size based on user variable type.
 * Mapping is determined by the following pv_type_map[] array.
 * pvTypeTIME_* types for gets/monitors return status, severity, and time stamp
 * in addition to the value.
 */
static PVTYPE pv_type_map[] =
{
	{ "char",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(char)		},
	{ "short",		pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(short)		},
	{ "int",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(int)		},
	{ "long",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)		},
	{ "unsigned char",	pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(unsigned char)	},
	{ "unsigned short",	pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(unsigned short)	},
	{ "unsigned int",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(unsigned int)	},
	{ "unsigned long",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(unsigned long)	},
	{ "float",		pvTypeFLOAT,	pvTypeTIME_FLOAT,	sizeof(float)		},
	{ "double",		pvTypeDOUBLE,	pvTypeTIME_DOUBLE,	sizeof(double)		},
	{ "string",		pvTypeSTRING,	pvTypeTIME_STRING,	sizeof(string)		},

	{ "epicsInt8",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(epicsInt8)	},
	{ "epicsUInt8",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(epicsUInt8)	},
	{ "epicsInt16",		pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(epicsInt16)	},
	{ "epicsUInt16",	pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(epicsUInt16)	},
	{ "epicsInt32",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(epicsInt32)	},
	{ "epicsUInt32",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(epicsUInt32)	},

	{ NULL,			pvTypeERROR,	pvTypeERROR,		0			}
};

static PVTYPE *find_type(const char *userType)
{
	PVTYPE	*pt;

	/* TODO: select pvType according to sizeof int/long/etc */
	assert(sizeof(char)==1);
	assert(sizeof(unsigned char)==1);
	assert(sizeof(short)==2);
	assert(sizeof(unsigned short)==2);
	assert(sizeof(int)==4);
	assert(sizeof(unsigned int)==4);
	for (pt = pv_type_map; pt->typeStr; pt++)
		if (strcmp(userType, pt->typeStr) == 0)
			break;
	return pt;
}

/* Free all allocated memory in a program structure */
void seq_free(SPROG *sp)
{
	unsigned nss, nch;

	/* Delete state sets */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;

		epicsEventDestroy(ss->syncSemId);
		for (nch = 0; nch < sp->numChans; nch++)
		{
			epicsEventDestroy(ss->getSemId[nch]);
			epicsEventDestroy(ss->putSemId[nch]);
		}
		free(ss->getSemId);
		free(ss->putSemId);

		epicsEventDestroy(ss->dead);

                if (ss->delay) free(ss->delay);
                if (ss->delayExpired) free(ss->delayExpired);
                if (ss->dirty) free(ss->dirty);
		if (ss->var) free(ss->var);
	}

	/* Delete program-wide semaphores */
	epicsMutexDestroy(sp->programLock);
	if (sp->ready) epicsEventDestroy(sp->ready);

	seqMacFree(sp);

	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN *ch = sp->chan + nch;

		if (ch->dbch)
		{
			if (ch->dbch->ssMetaData)
				free(ch->dbch->ssMetaData);
			if (ch->dbch->dbName)
				free(ch->dbch->dbName);
			free(ch->dbch);
		}
	}
	free(sp->chan);
	free(sp->ss);
	if (sp->queues) {
		unsigned nq;
		for (nq = 0; nq < sp->numQueues; nq++)
			seqQueueDestroy(sp->queues[nq]);
		free(sp->queues);
	}
	if (sp->evFlags) free(sp->evFlags);
	if (sp->var) free(sp->var);
	free(sp);
}
