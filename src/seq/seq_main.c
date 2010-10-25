/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California
	and the University of Chicago.
	Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)

	Seq() initiates a sequence as a group of cooperating
	tasks.  An optional string parameter specifies the values for
	macros.  The PV context and auxiliary thread are shared by all state
	programs.
***************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "seq.h"

/* function prototypes for local routines */
static SPROG *seqInitTables(struct seqProgram *);
static void init_sprog(struct seqProgram *, SPROG *);
static void init_sscb(struct seqProgram *, SPROG *);
static void init_chan(struct seqProgram *, SPROG *);
static void init_mac(SPROG *);

static void seq_logInit(SPROG *);
static void seqChanNameEval(SPROG *);
static void selectDBtype(char *, short *, short *, short *, short *);

/*	Globals */

/*	Auxiliary sequencer thread id; used to share PV context. */
epicsThreadId seqAuxThreadId = (epicsThreadId) 0;

/*
 * seq: User-callable routine to initiate a state program.
 * Usage:  seq(<pSP>, <macros string>, <stack size>)
 *	pSP is the ptr to the state program structure.
 *	Example:  seq(&myprog, "logfile=mylog", 0)
 * When called from the shell, the 2nd & 3rd parameters are optional.
 *
 * Creates the initial state program thread and returns its thread id.
 * Most initialization is performed here.
 */
epicsThreadId seq (
	struct seqProgram *pSeqProg, char *macroDef, unsigned int stackSize)
{
	epicsThreadId	tid;
	SPROG		*pSP;
	char		*pValue, *pThreadName;
	unsigned int	smallStack;
	AUXARGS		auxArgs;

	/* Print version & date of sequencer */
	printf(SEQ_VERSION "\n");

	/* Exit if no parameters specified */
	if (pSeqProg == 0)
	{
		return 0;
	}

	/* Check for correct state program format */
	if (pSeqProg->magic != MAGIC)
	{	/* Oops */
		errlogPrintf("Illegal magic number in state program.\n");
		errlogPrintf(" - Possible mismatch between SNC & SEQ "
			"versions\n");
		errlogPrintf(" - Re-compile your program?\n");
		epicsThreadSleep( 1.0 );	/* let error messages get printed */
		return 0;
	}

	/* Initialize the sequencer tables */
	pSP = seqInitTables(pSeqProg);

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(pSeqProg->pParams, pSP);

	/* Parse the macro definitions from the command line */
	seqMacParse(macroDef, pSP);

	/* Do macro substitution on channel names */
	seqChanNameEval(pSP);

	/* Initialize sequencer logging */
	seq_logInit(pSP);

	/* Specify stack size */
	if (stackSize == 0)
		stackSize = epicsThreadGetStackSize(THREAD_STACK_SIZE);
	pValue = seqMacValGet(pSP->pMacros, "stack");
	if (pValue != NULL && strlen(pValue) > 0)
	{
		sscanf(pValue, "%ud", &stackSize);
	}
	smallStack = epicsThreadGetStackSize(epicsThreadStackSmall);
	if (stackSize < smallStack)
		stackSize = smallStack;
	pSP->stackSize = stackSize;

	/* Specify thread name */
	pValue = seqMacValGet(pSP->pMacros, "name");
	if (pValue != NULL && strlen(pValue) > 0)
		pThreadName = pValue;
	else
		pThreadName = pSP->pProgName;

	/* Specify PV system name (defaults to CA) */
	pValue = seqMacValGet(pSP->pMacros, "pvsys");
	if (pValue != NULL && strlen(pValue) > 0)
		auxArgs.pPvSysName = pValue;
	else
		auxArgs.pPvSysName = "ca";

	/* Determine debug level (currently only used for PV-level debugging) */
	pValue = seqMacValGet(pSP->pMacros, "debug");
	if (pValue != NULL && strlen(pValue) > 0)
		auxArgs.debug = atol(pValue);
	else
		auxArgs.debug = 0;

	/* Spawn the sequencer auxiliary thread */
	if (seqAuxThreadId == (epicsThreadId) 0)
	{
		unsigned int auxStack = epicsThreadGetStackSize(epicsThreadStackMedium);
		epicsThreadCreate("seqAux", THREAD_PRIORITY+1, auxStack,
				(EPICSTHREADFUNC)seqAuxThread, &auxArgs);
		while (seqAuxThreadId == (epicsThreadId) 0)
			/* wait for thread to init. message system */
			epicsThreadSleep(0.1);

		if (seqAuxThreadId == (epicsThreadId) -1)
		{
			epicsThreadSleep( 1.0 );	/* let error messages get printed */
			return 0;
		}
#ifdef	DEBUG
	printf("thread seqAux spawned, tid=%p\n", (int) seqAuxThreadId);
#endif	/*DEBUG*/
	}

	/* Spawn the initial sequencer thread */
#ifdef	DEBUG
	printf("Spawning thread %s, stackSize=%d\n", pThreadName,
		pSP->stackSize);
#endif	/*DEBUG*/
	/* Specify thread priority */
	pSP->threadPriority = THREAD_PRIORITY;
	pValue = seqMacValGet(pSP->pMacros, "priority");
	if (pValue != NULL && strlen(pValue) > 0)
	{
		sscanf(pValue, "%ud", &(pSP->threadPriority));
	}
	if (pSP->threadPriority > THREAD_PRIORITY)
		pSP->threadPriority = THREAD_PRIORITY;

	tid = epicsThreadCreate(pThreadName, pSP->threadPriority, pSP->stackSize,
		(EPICSTHREADFUNC)sequencer, pSP);

	printf("Spawning state program \"%s\", thread %p: \"%s\"\n",
		pSP->pProgName, tid, pThreadName);

	return tid;
}

/* seqInitTables - initialize sequencer tables */
static SPROG *seqInitTables(struct seqProgram *pSeqProg)
{
	SPROG	*pSP;

	pSP = (SPROG *)calloc(1, sizeof (SPROG));

	/* Initialize state program block */
	init_sprog(pSeqProg, pSP);

	/* Initialize state set control blocks */
	init_sscb(pSeqProg, pSP);

	/* Initialize database channel blocks */
	init_chan(pSeqProg, pSP);

	/* Initialize the macro table */
	init_mac(pSP);

	return pSP;
}

/*
 * Copy data from seqCom.h structures into this thread's dynamic structures
 * as defined in seq.h.
 */
static void init_sprog(struct seqProgram *pSeqProg, SPROG *pSP)
{
	int	i, nWords;

	/* Copy information for state program */
	pSP->numSS = pSeqProg->numSS;
	pSP->numChans = pSeqProg->numChans;
	pSP->numEvents = pSeqProg->numEvents;
	pSP->options = pSeqProg->options;
	pSP->pProgName = pSeqProg->pProgName;
	pSP->entryFunc = pSeqProg->entryFunc;
	pSP->exitFunc = pSeqProg->exitFunc;
	pSP->varSize = pSeqProg->varSize;
	/* Allocate user variable area if reentrant option (+r) is set */
	if ((pSP->options & OPT_REENT) != 0)
		pSP->pVar = (USER_VAR *)calloc(pSP->varSize, 1);

#ifdef	DEBUG
	printf("init_sprog: num SS=%d, num Chans=%d, num Events=%d, "
		"Prog Name=%s, var Size=%d\n", pSP->numSS, pSP->numChans,
		pSP->numEvents, pSP->pProgName, pSP->varSize);
#endif	/*DEBUG*/

	/* Create a semaphore for resource locking on PV events */
	pSP->caSemId = epicsMutexMustCreate();
	pSP->connCount = 0;
	pSP->assignCount = 0;
	pSP->logFd = NULL;

	/* Allocate an array for event flag bits */
	nWords = (pSP->numEvents + NBITS - 1) / NBITS;
	if (nWords == 0)
		nWords = 1;
	pSP->pEvents = (bitMask *)calloc(nWords, sizeof(bitMask));

	/* Allocate and initialize syncQ queues */
	pSP->numQueues = pSeqProg->numQueues;
	pSP->pQueues = NULL;

	if (pSP->numQueues > 0 )
	{
		pSP->pQueues = (ELLLIST *)calloc(pSP->numQueues,
						 sizeof(ELLLIST));
		for (i = 0; i < pSP->numQueues; i++)
			ellInit(&pSP->pQueues[i]);
	}
}

/*
 * Initialize the state set control blocks
 */
static void init_sscb(struct seqProgram *pSeqProg, SPROG *pSP)
{
	SSCB		*pSS;
	STATE		*pState;
	int		nss, nstates;
	struct seqSS	*pSeqSS;
	struct seqState	*pSeqState;


	/* Allocate space for the SSCB structures */
	pSP->pSS = pSS = (SSCB *)calloc(pSeqProg->numSS, sizeof(SSCB));

	/* Copy information for each state set and state */
	pSeqSS = pSeqProg->pSS;
	for (nss = 0; nss < pSeqProg->numSS; nss++, pSS++, pSeqSS++)
	{
		/* Fill in SSCB */
		pSS->pSSName = pSeqSS->pSSName;
		pSS->numStates = pSeqSS->numStates;
		pSS->maxNumDelays = pSeqSS->numDelays;
		pSS->delay = (double *)calloc(pSS->maxNumDelays, sizeof(double));
		pSS->delayExpired = (epicsBoolean *)calloc(pSS->maxNumDelays, sizeof(epicsBoolean));
		pSS->errorState = pSeqSS->errorState;
		pSS->currentState = 0; /* initial state */
		pSS->nextState = 0;
		pSS->prevState = 0;
		pSS->threadId = 0;
		/* Initialize to start time rather than zero time! */
		pvTimeGetCurrentDouble(&pSS->timeEntered);
		pSS->sprog = pSP;
#ifdef	DEBUG
		printf("init_sscb: SS Name=%s, num States=%d, pSS=%p\n",
			pSS->pSSName, pSS->numStates, pSS);
#endif	/*DEBUG*/
		pSS->allFirstConnectAndMonitorSemId = epicsEventMustCreate(epicsEventEmpty);
		/* Create a binary semaphore for synchronizing events in a SS */
		pSS->syncSemId = epicsEventMustCreate(epicsEventEmpty);

		/* Create binary semaphores for synchronous pvGet() and
		   pvPut() */
		pSS->getSemId = epicsEventMustCreate(epicsEventFull);
		pSS->putSemId = epicsEventMustCreate(epicsEventFull);

		/* Create binary semaphores for thread death */
		pSS->death1SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death2SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death3SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death4SemId = epicsEventMustCreate(epicsEventEmpty);

		/* Allocate & fill in state blocks */
		pSS->pStates = pState = (STATE *)calloc(pSS->numStates,
							sizeof(STATE));

		pSeqState = pSeqSS->pStates;
		for (nstates = 0; nstates < pSeqSS->numStates;
					       nstates++, pState++, pSeqState++)
		{
			pState->pStateName = pSeqState->pStateName;
			pState->actionFunc = pSeqState->actionFunc;
			pState->eventFunc = pSeqState->eventFunc;
			pState->delayFunc = pSeqState->delayFunc;
			pState->entryFunc = pSeqState->entryFunc;
			pState->exitFunc = pSeqState->exitFunc;
			pState->pEventMask = pSeqState->pEventMask;
			pState->options = pSeqState->options;
#ifdef	DEBUG
		printf("init_sscb: State Name=%s, Event Mask=%p\n",
			pState->pStateName, *pState->pEventMask);
#endif	/*DEBUG*/
		}
	}

#ifdef	DEBUG
	printf("init_sscb: numSS=%d\n", pSP->numSS);
#endif	/*DEBUG*/
}

/*
 * init_chan--Build the database channel structures.
 * Note:  Actual PV name is not filled in here. */
static void init_chan(struct seqProgram *pSeqProg, SPROG *pSP)
{
	int		nchan;
	CHAN		*pDB;
	struct seqChan	*pSeqChan;

	/* Allocate space for the CHAN structures */
	pSP->pChan = (CHAN *)calloc(pSP->numChans, sizeof(CHAN));
	pDB = pSP->pChan;

	pSeqChan = pSeqProg->pChan;
	for (nchan = 0; nchan < pSP->numChans; nchan++, pDB++, pSeqChan++)
	{
#ifdef	DEBUG
		printf("init_chan: pDB=%p\n", pDB);
#endif	/*DEBUG*/
		pDB->sprog = pSP;
		pDB->sset = NULL;	/* set temporarily during get/put */
		pDB->dbAsName = pSeqChan->dbAsName;
		pDB->pVarName = pSeqChan->pVarName;
		pDB->pVarType = pSeqChan->pVarType;
		pDB->pVar = pSeqChan->pVar;	/* offset for +r option */
		pDB->count = pSeqChan->count;
		pDB->efId = pSeqChan->efId;
		pDB->monFlag = pSeqChan->monFlag;
		pDB->eventNum = pSeqChan->eventNum;
		pDB->queued = pSeqChan->queued;
		pDB->maxQueueSize = pSeqChan->maxQueueSize ?
				    pSeqChan->maxQueueSize : MAX_QUEUE_SIZE;
		pDB->queueIndex = pSeqChan->queueIndex;
		pDB->assigned = 0;

		/* Latest error message (dynamically allocated) */
		pDB->message = NULL;

		/* Fill in get/put db types, element size, & access offset */
		selectDBtype(pSeqChan->pVarType, &pDB->getType,
			&pDB->putType, &pDB->size, &pDB->dbOffset);

		/* Reentrant option: Convert offset to addr of the user var. */
		if ((pSP->options & OPT_REENT) != 0)
			pDB->pVar += (ptrdiff_t)pSP->pVar;
#ifdef	DEBUG
		printf(" Assigned Name=%s, VarName=%s, VarType=%s, "
			"count=%d\n", pDB->dbAsName, pDB->pVarName,
			pDB->pVarType, pDB->count);
		printf("   size=%d, dbOffset=%d\n", pDB->size,
			pDB->dbOffset);
		printf("   efId=%d, monFlag=%d, eventNum=%d\n",
			pDB->efId, pDB->monFlag, pDB->eventNum);
#endif	/*DEBUG*/
	}
}

/* 
 * init_mac - initialize the macro table.
 */
static void init_mac(SPROG *pSP)
{
	int		i;
	MACRO		*pMac;

	pSP->pMacros = pMac = (MACRO *)calloc(MAX_MACROS, sizeof (MACRO));
#ifdef	DEBUG
	printf("init_mac: pMac=%p\n", pMac);
#endif	/*DEBUG*/

	for (i = 0 ; i < MAX_MACROS; i++, pMac++)
	{
		pMac->pName = NULL;
		pMac->pValue = NULL;
	}
}

/*
 * Evaluate channel names by macro substitution.
 */
#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)
static void seqChanNameEval(SPROG *pSP)
{
	CHAN		*pDB;
	int		i;

	pDB = pSP->pChan;
	for (i = 0; i < pSP->numChans; i++, pDB++)
	{
		pDB->dbName = (char *)calloc(1, MACRO_STR_LEN);
		seqMacEval(pDB->dbAsName, pDB->dbName, MACRO_STR_LEN, pSP->pMacros);
#ifdef	DEBUG
		printf("seqChanNameEval: \"%s\" evaluated to \"%s\"\n",
			pDB->dbAsName, pDB->dbName);
#endif	/*DEBUG*/
	}
}

/*
 * selectDBtype -- returns types for DB put/get, element size, and db access
 * offset based on user variable type.
 * Mapping is determined by the following typeMap[] array.
 * pvTypeTIME_* types for gets/monitors return status and time stamp.
 */
static struct typeMap {
	char	*pTypeStr;
	short	putType;
	short	getType;
	short	size;
	short	offset;
} typeMap[] = {
	{
	"char",		 pvTypeCHAR,	pvTypeTIME_CHAR,
	sizeof (char),   OFFSET(pvTimeChar, value[0])
	},

	{
	"short",	 pvTypeSHORT,	pvTypeTIME_SHORT,
	sizeof (short),  OFFSET(pvTimeShort, value[0])
	},

	{
	"int",		 pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"long",		 pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"unsigned char", pvTypeCHAR,	pvTypeTIME_CHAR,
	sizeof (char),   OFFSET(pvTimeChar, value[0])
	},

	{
	"unsigned short",pvTypeSHORT,	pvTypeTIME_SHORT,
	sizeof (short),  OFFSET(pvTimeShort, value[0])
	},

	{
	"unsigned int",  pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"unsigned long", pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"float",	 pvTypeFLOAT,	pvTypeTIME_FLOAT,
	sizeof (float),  OFFSET(pvTimeFloat, value[0])
	},

	{
	"double",	 pvTypeDOUBLE,	pvTypeTIME_DOUBLE,
	sizeof (double), OFFSET(pvTimeDouble, value[0])
	},

	{
	"string",	 pvTypeSTRING,	pvTypeTIME_STRING,
	MAX_STRING_SIZE, OFFSET(pvTimeString, value[0])
	},

	{
	0, 0, 0, 0, 0
	}
};

static void selectDBtype(
	char	*pUserType,
	short	*pGetType,
	short	*pPutType,
	short	*pSize,
	short	*pOffset)
{
	struct typeMap	*pMap;

	for (pMap = &typeMap[0]; *pMap->pTypeStr != 0; pMap++)
	{
		if (strcmp(pUserType, pMap->pTypeStr) == 0)
		{
			*pGetType = pMap->getType;
			*pPutType = pMap->putType;
			*pSize = pMap->size;
			*pOffset = pMap->offset;
			return;
		}
	}
	*pGetType = *pPutType = *pSize = *pOffset = 0; /* this shouldn't happen */
}

/*
 * seq_logInit() - Initialize logging.
 * If "logfile" is not specified, then we log to standard output.
 */
static void seq_logInit(SPROG *pSP)
{
	char	*pValue;
	FILE	*fd;

	/* Create a logging resource locking semaphore */
	pSP->logSemId = epicsMutexMustCreate();
	pSP->pLogFile = "";

	/* Check for logfile spec. */
	pValue = seqMacValGet(pSP->pMacros, "logfile");
	if (pValue != NULL && strlen(pValue) > 0)
	{	/* Create & open a new log file for write only */
		fd = fopen(pValue, "w");
		if (fd == NULL)
		{
			errlogPrintf("Log file open error, file=%s, error=%s\n",
			pSP->pLogFile, strerror(errno));
		}
		else
		{
			errlogPrintf("Log file opened, fd=%d, file=%s\n",
				     fileno(fd), pValue);
			pSP->logFd = fd;
			pSP->pLogFile = pValue;
		}
	}
}

/*
 * seq_logv
 * Log a message to the console or a file with thread name, date, & time of day.
 * The format looks like "mythread 12/13/93 10:07:43: Hello world!".
 */
#define	LOG_BFR_SIZE	200

static long seq_logv(SPROG *pSP, const char *fmt, va_list args)
{
	int		count, status;
	epicsTimeStamp	timeStamp;
	char		logBfr[LOG_BFR_SIZE], *eBfr=logBfr+LOG_BFR_SIZE, *pBfr;
	FILE		*fd = pSP->logFd ? pSP->logFd : stdout;
	pBfr = logBfr;

	/* Enter thread name */
	sprintf(pBfr, "%s ", epicsThreadGetNameSelf() );
	pBfr += strlen(pBfr);

	/* Get time of day */
	epicsTimeGetCurrent(&timeStamp);	/* time stamp format */

	/* Convert to text format: "yyyy/mm/dd hh:mm:ss" */
	epicsTimeToStrftime(pBfr, eBfr-pBfr, "%Y/%m/%d %H:%M:%S", &timeStamp);
	pBfr += strlen(pBfr);

	/* Insert ": " */
	*pBfr++ = ':';
	*pBfr++ = ' ';

	/* Append the user's msg to the buffer */
	vsprintf(pBfr, fmt, args);
	pBfr += strlen(pBfr);

	/* Write the msg */
	epicsMutexMustLock(pSP->logSemId);
	count = pBfr - logBfr + 1;
	status = fwrite(logBfr, 1, count, fd);
	epicsMutexUnlock(pSP->logSemId);

	if (status != count)
	{
		errlogPrintf("Log file write error, fd=%d, file=%s, error=%s\n",
			fileno(pSP->logFd), pSP->pLogFile, strerror(errno));
		return ERROR;
	}

	/* If this is not stdout, flush the buffer */
	if (fd != stdout)
	{
		epicsMutexMustLock(pSP->logSemId);
		fflush(pSP->logFd);
		epicsMutexUnlock(pSP->logSemId);
	}
	return OK;
}

/*
 * seq_seqLog() - State program interface to seq_log().
 * Does not require ptr to state program block.
 */
long seq_seqLog(SS_ID ssId, const char *fmt, ...)
{
	SPROG		*pSP;
	va_list		args;
	long		rtn;

	va_start (args, fmt);
	pSP = ((SSCB *)ssId)->sprog;
	rtn = seq_logv(pSP, fmt, args);
	va_end (args);
	return(rtn);
}
