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

/* function prototypes for local routines */
static void seqInitTables(SPROG *, seqProgram *);
static void init_sprog(seqProgram *, SPROG *);
static void init_sscb(seqProgram *, SPROG *);
static void init_chan(seqProgram *, SPROG *);
static PVTYPE *find_type(const char *userType);

/*	Globals */

/*	Auxiliary sequencer thread id; used to share PV context. */
epicsThreadId seqAuxThreadId = (epicsThreadId) 0;

/*
 * seq: User-callable routine to run a state program.
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
	char		*value;
	const char	*threadName;
	unsigned	smallStack;
	AUXARGS		auxArgs;

	/* Print version & date of sequencer */
	printf(SEQ_VERSION "\n");

	/* Exit if no parameters specified */
	if (seqProg == 0)
	{
		return;
	}

	/* Check for correct state program format */
	if (seqProg->magic != MAGIC)
	{	/* Oops */
		errlogPrintf("Illegal magic number in state program.\n");
		errlogPrintf(" - Possible mismatch between SNC & SEQ "
			"versions\n");
		errlogPrintf(" - Re-compile your program?\n");
		epicsThreadSleep( 1.0 );	/* let error messages get printed */
		return;
	}

	sp = (SPROG *)calloc(1, sizeof (SPROG));

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(sp, seqProg->params);

	/* Parse the macro definitions from the command line */
	seqMacParse(sp, macroDef);

	/* Initialize the sequencer tables */
	seqInitTables(sp, seqProg);


	/* Specify stack size */
	if (stackSize == 0)
		stackSize = epicsThreadGetStackSize(THREAD_STACK_SIZE);
	value = seqMacValGet(sp, "stack");
	if (value != NULL && strlen(value) > 0)
	{
		sscanf(value, "%ud", &stackSize);
	}
	smallStack = epicsThreadGetStackSize(epicsThreadStackSmall);
	if (stackSize < smallStack)
		stackSize = smallStack;
	sp->stackSize = stackSize;

	/* Specify thread name */
	value = seqMacValGet(sp, "name");
	if (value != NULL && strlen(value) > 0)
		threadName = value;
	else
		threadName = sp->progName;

	/* Specify PV system name (defaults to CA) */
	value = seqMacValGet(sp, "pvsys");
	if (value != NULL && strlen(value) > 0)
		auxArgs.pvSysName = value;
	else
		auxArgs.pvSysName = "ca";

	/* Determine debug level (currently only used for PV-level debugging) */
	value = seqMacValGet(sp, "debug");
	if (value != NULL && strlen(value) > 0)
		auxArgs.debug = atol(value);
	else
		auxArgs.debug = 0;

	/* Spawn the sequencer auxiliary thread */
	if (seqAuxThreadId == (epicsThreadId) 0)
	{
		unsigned auxStack = epicsThreadGetStackSize(epicsThreadStackMedium);
		epicsThreadCreate("seqAux", THREAD_PRIORITY+1, auxStack,
				(EPICSTHREADFUNC)seqAuxThread, &auxArgs);
		while (seqAuxThreadId == (epicsThreadId) 0)
			/* wait for thread to init. message system */
			epicsThreadSleep(0.1);

		if (seqAuxThreadId == (epicsThreadId) -1)
		{
			epicsThreadSleep( 1.0 );	/* let error messages get printed */
			return;
		}
		DEBUG("thread seqAux spawned, tid=%p\n", seqAuxThreadId);
	}

	/* Spawn the initial sequencer thread */
	DEBUG("Spawning thread %s, stackSize=%d\n", threadName,
		sp->stackSize);
	/* Specify thread priority */
	sp->threadPriority = THREAD_PRIORITY;
	value = seqMacValGet(sp, "priority");
	if (value != NULL && strlen(value) > 0)
	{
		sscanf(value, "%ud", &(sp->threadPriority));
	}
	if (sp->threadPriority > THREAD_PRIORITY)
		sp->threadPriority = THREAD_PRIORITY;

	tid = epicsThreadCreate(threadName, sp->threadPriority, sp->stackSize,
		(EPICSTHREADFUNC)sequencer, sp);

	printf("Spawning state program \"%s\", thread %p: \"%s\"\n",
		sp->progName, tid, threadName);
}

/* seqInitTables - initialize sequencer tables */
static void seqInitTables(SPROG *sp, seqProgram *seqProg)
{
	/* Initialize state program block */
	init_sprog(seqProg, sp);

	/* Initialize state set control blocks */
	init_sscb(seqProg, sp);

	/* Initialize database channel blocks */
	init_chan(seqProg, sp);
}

/*
 * Copy data from seqCom.h structures into this thread's dynamic structures
 * as defined in seq.h.
 */
static void init_sprog(seqProgram *seqProg, SPROG *sp)
{
	unsigned nWords;

	/* Copy information for state program */
	sp->numSS = seqProg->numSS;
	sp->numChans = seqProg->numChans;
	sp->numEvents = seqProg->numEvents;
	sp->options = seqProg->options;
	sp->progName = seqProg->progName;
	sp->initFunc = seqProg->initFunc;
	sp->entryFunc = seqProg->entryFunc;
	sp->exitFunc = seqProg->exitFunc;
	sp->varSize = seqProg->varSize;
	sp->numQueues = seqProg->numQueues;

	/* Allocate user variable area if reentrant option (+r) is set */
	if (sp->options & OPT_REENT)
	{
		sp->var = (char *)calloc(sp->varSize, 1);
	}

	DEBUG("init_sprog: num SS=%ld, num Chans=%ld, num Events=%ld, "
		"Prog Name=%s, var Size=%ld\n", sp->numSS, sp->numChans,
		sp->numEvents, sp->progName, sp->varSize);

	/* Create a semaphore for resource locking on PV events */
	sp->programLock = epicsMutexMustCreate();
	sp->connectCount = 0;
	sp->assignCount = 0;
	sp->allDisconnected = TRUE;

	/* Allocate an array for event flag bits */
	nWords = (sp->numEvents + NBITS - 1) / NBITS;
	if (nWords == 0)
		nWords = 1;
	sp->events = (bitMask *)calloc(nWords, sizeof(bitMask));

	/* Allocate and initialize syncQ queues */
	sp->numQueues = seqProg->numQueues;
	sp->queues = NULL;

	/* Allocate and initialize syncQ queues */
	if (sp->numQueues > 0)
	{
		sp->queues = newArray(QUEUE, sp->numQueues);
	}
	/* initial pool for pv requests is 1kB on 32-bit systems */
	freeListInitPvt(&sp->pvReqPool, 128, sizeof(PVREQ));
}

/*
 * Initialize the state set control blocks
 */
static void init_sscb(seqProgram *seqProg, SPROG *sp)
{
	SSCB		*ss;
	unsigned	nss;
	seqSS		*seqSS;


	/* Allocate space for the SSCB structures */
	sp->ss = ss = (SSCB *)calloc(seqProg->numSS, sizeof(SSCB));

	/* Copy information for each state set and state */
	seqSS = seqProg->ss;
	for (nss = 0; nss < seqProg->numSS; nss++, ss++, seqSS++)
	{
		/* Fill in SSCB */
		ss->ssName = seqSS->ssName;
		ss->numStates = seqSS->numStates;
		ss->maxNumDelays = seqSS->numDelays;

		ss->delay = (double *)calloc(ss->maxNumDelays, sizeof(double));
		ss->delayExpired = (boolean *)calloc(ss->maxNumDelays, sizeof(boolean));
		ss->currentState = 0; /* initial state */
		ss->nextState = 0;
		ss->prevState = 0;
		ss->threadId = 0;
		/* Initialize to start time rather than zero time! */
		pvTimeGetCurrentDouble(&ss->timeEntered);
		ss->sprog = sp;

		DEBUG("init_sscb: SS Name=%s, num States=%ld, ss=%p\n",
			ss->ssName, ss->numStates, ss);
		ss->allFirstConnectAndMonitorSemId = epicsEventMustCreate(epicsEventEmpty);
		/* Create a binary semaphore for synchronizing events in a SS */
		ss->syncSemId = epicsEventMustCreate(epicsEventEmpty);

		/* Create binary semaphores for synchronous pvGet() and
		   pvPut() */
		ss->getSemId = epicsEventMustCreate(epicsEventFull);

		/* Create binary semaphores for thread death */
		ss->death1SemId = epicsEventMustCreate(epicsEventEmpty);
		ss->death2SemId = epicsEventMustCreate(epicsEventEmpty);
		ss->death3SemId = epicsEventMustCreate(epicsEventEmpty);
		ss->death4SemId = epicsEventMustCreate(epicsEventEmpty);

		/* No need to copy the state structs, they can be shared
		   because nothing gets mutated. */
		ss->states = seqSS->states;

		/* Allocate user variable area if safe mode option (+s) is set */
		if (sp->options & OPT_SAFE)
		{
			sp->var = (char *)calloc(sp->varSize, 1);
		}
	}

	DEBUG("init_sscb: numSS=%ld\n", sp->numSS);
}

/*
 * init_chan--Build the database channel structures.
 * Note:  Actual PV name is not filled in here. */
static void init_chan(seqProgram *seqProg, SPROG *sp)
{
	unsigned	nchan;
	CHAN		*ch;
	seqChan		*seqChan;

	/* Allocate space for the CHAN structures */
	sp->chan = (CHAN *)calloc(sp->numChans, sizeof(CHAN));
	ch = sp->chan;

	seqChan = seqProg->chan;
	for (nchan = 0; nchan < sp->numChans; nchan++, ch++, seqChan++)
	{
		DEBUG("init_chan: ch=%p\n", ch);
		ch->sprog = sp;
		ch->varName = seqChan->varName;
		ch->offset = seqChan->offset;
		ch->count = seqChan->count;
		ch->efId = seqChan->efId;
		ch->monFlag = seqChan->monitored;
		ch->eventNum = seqChan->eventNum;
		ch->assigned = 0;

		if (seqChan->dbAsName != NULL)
		{
			char name_buffer[100];

			seqMacEval(sp, seqChan->dbAsName, name_buffer, sizeof(name_buffer));
			if (name_buffer[0])
			{
				ch->dbAsName = epicsStrDup(name_buffer);
			}
			DEBUG("  assigned name=%s, expanded name=%s\n",
				seqChan->dbAsName, ch->dbAsName);
		}
		else
			DEBUG("  pv name=<anonymous>\n");

		/* Latest error message (dynamically allocated) */
		ch->message = NULL;

		/* Fill in get/put db types, element size */
		ch->type = find_type(seqChan->varType);

		DEBUG(" Assigned Name=%s, VarName=%s, VarType=%s, count=%ld\n"
			"   size=%u, efId=%ld, monFlag=%u, eventNum=%ld\n",
			ch->dbAsName, ch->varName,
			ch->type->typeStr, ch->count,
			ch->type->size,
			ch->efId, ch->monFlag, ch->eventNum);

		ch->varLock = epicsMutexMustCreate();
		ch->dirty = (boolean *)calloc(sp->numSS,sizeof(boolean));
		ch->getComplete = (boolean *)calloc(sp->numSS,sizeof(boolean));
		ch->putSemId = epicsEventMustCreate(epicsEventFull);
	}
}

/*
 * find_type -- returns types for DB put/get and element size
 * based on user variable type.
 * Mapping is determined by the following pv_type_map[] array.
 * pvTypeTIME_* types for gets/monitors return status and time stamp.
 */
static PVTYPE pv_type_map[] =
{
	{ "char",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(char)	},
	{ "short",		pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(short)	},
	{ "int",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)	},
	{ "long",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)	},
	{ "unsigned char",	pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(char)	},
	{ "unsigned short",	pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(short)	},
	{ "unsigned int",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)	},
	{ "unsigned long",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)	},
	{ "float",		pvTypeFLOAT,	pvTypeTIME_FLOAT,	sizeof(float)	},
	{ "double",		pvTypeDOUBLE,	pvTypeTIME_DOUBLE,	sizeof(double)	},
	{ "string",		pvTypeSTRING,	pvTypeTIME_STRING,	sizeof(string)	},
	{ NULL,			pvTypeERROR,	pvTypeERROR,		0		}
};

static PVTYPE *find_type(const char *userType)
{
	PVTYPE	*pt = pv_type_map;

	while (pt->typeStr)
	{
		if (strcmp(userType, pt->typeStr) == 0)
		{
			break;
		}
		pt++;
	}
	return pt;
}
