/*	Definitions for the run-time sequencer
 *
 *      Author:         Andy Kozubal
 *      Date:           
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991,2,3, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
 *		und Energie GmbH, Germany (HZB)
 *		(see file Copyright.HZB included in this distribution)
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 */
#ifndef	INCLseqPvth
#define	INCLseqPvth

#define OK 0
#define ERROR (-1)
#define LOCAL static

/* global variable for PV system context */
#ifdef DECLARE_PV_SYS
void *pvSys;
#else
extern void *pvSys;
#endif

#define MAX_QUEUE_SIZE 100		/* default max_queue_size */

#define valPtr(ch)	(bufPtr(ch)+(ch)->offset)
#define bufPtr(ch)	(((ch)->sprog->options&OPT_REENT)?(ch)->sprog->pVar:0)

/* Structure to hold information about database channels */
struct db_channel
{
	/* These are supplied by SNC */
	char		*dbAsName;	/* channel name from assign statement */
	ptrdiff_t	offset;		/* offset to value */
	char		*pVarName;	/* variable name string */
	char		*pVarType;	/* variable type string (e.g. ("int") */
	long		count;		/* number of elements in array */
	long		efId;		/* event flag id if synced */
	long		eventNum;	/* event number */
	unsigned	monFlag;	/* TRUE if channel is to be monitored */
	int		queued;		/* TRUE if queued via syncQ */
	int		maxQueueSize;	/* max syncQ queue size (0 => def) */
	int		queueIndex;	/* syncQ queue index */

	/* These are filled in at run time */
	char		*dbName;	/* channel name after macro expansion */
	void		*pvid;		/* PV (process variable) id */
	unsigned	assigned;	/* TRUE only if channel is assigned */
	unsigned	connected;	/* TRUE only if channel is connected */
	unsigned	getComplete;	/* TRUE if previous pvGet completed */
	unsigned	putComplete;	/* TRUE if previous pvPut completed */
	unsigned	putWasComplete;	/* previous value of putComplete */
	short		dbOffset;	/* offset to value in db access struct*/
	short		status;		/* last db access status code */
	epicsTimeStamp	timeStamp;	/* time stamp */
	long		dbCount;	/* actual count for db access */
	short		severity;	/* last db access severity code */
	short		size;		/* size (in bytes) of single var elem */
	short		getType;	/* db get type (e.g. DBR_STS_INT) */
	short		putType;	/* db put type (e.g. DBR_INT) */
	char		*message;	/* last db access error message */
	unsigned	gotFirstMonitor;
	unsigned	gotFirstConnect;
	unsigned	monitored;	/* TRUE if channel IS monitored */
	void		*evid;		/* event id (supplied by PV lib) */
	struct state_program *sprog;	/* state program that owns this struct*/
	struct state_set_control_block *sset; /* current state-set (temp.) */
	epicsMutexId	varLock;	/* mutex to lock out access */
};
typedef struct db_channel CHAN;

/* Structure for syncQ queue entry */
struct queue_entry
{
	ELLNODE		node;		/* linked list node */
	CHAN		*pDB;		/* ptr to db channel info */
	pvValue		value;		/* value, time stamp etc */
};
typedef struct queue_entry QENTRY;

/* Structure to hold information about a state */
struct state_info_block
{
	char		*pStateName;	/* state name */
	ACTION_FUNC	*actionFunc;	/* ptr to action rout. for this state */
	EVENT_FUNC	*eventFunc;	/* ptr to event rout. for this state */
	DELAY_FUNC	*delayFunc;	/* ptr to delay rout. for this state */
	ENTRY_FUNC	*entryFunc;	/* ptr to entry rout. for this state */
	EXIT_FUNC	*exitFunc;	/* ptr to exit rout. for this state */
	bitMask		*pEventMask;	/* event mask for this state */
	bitMask		options;	/* options mask for this state */
};
typedef struct state_info_block STATE;

/* Structure to hold information about a State Set */
struct state_set_control_block
{
	char		*pSSName;	/* state set name (for debugging) */
	epicsThreadId	threadId;	/* thread id */
	unsigned int	threadPriority;	/* thread priority */
	unsigned int	stackSize;	/* stack size */
	epicsEventId	allFirstConnectAndMonitorSemId;
	epicsEventId	syncSemId;	/* semaphore for event sync */
	epicsEventId	getSemId;	/* semaphore id for async get */
	epicsEventId	putSemId;	/* semaphore id for async put */
	epicsEventId	death1SemId;	/* semaphore id for death (#1) */
	epicsEventId	death2SemId;	/* semaphore id for death (#2) */
	epicsEventId	death3SemId;	/* semaphore id for death (#3) */
	epicsEventId	death4SemId;	/* semaphore id for death (#4) */
	long		numStates;	/* number of states */
	STATE		*pStates;	/* ptr to array of state blocks */
	short		currentState;	/* current state index */
	short		nextState;	/* next state index */
	short		prevState;	/* previous state index */
	short		errorState;	/* error state index (-1 if none defd)*/
	short		transNum;	/* highest prio trans. # triggered */
	bitMask		*pMask;		/* current event mask */
	long		maxNumDelays;	/* max. number of delays */
	long		numDelays;	/* number of delays activated */
	double		*delay;		/* queued delay value in secs (array) */
	unsigned	*delayExpired;	/* TRUE if delay expired (array) */
	double		timeEntered;	/* time that a state was entered */
	struct state_program *sprog;	/* ptr back to state program block */
};
typedef struct state_set_control_block SSCB;

/* Macro table */
typedef struct macro
{
	char	*pName;
	char	*pValue;
} MACRO;

/* All information about a state program.
	The address of this structure is passed to the run-time sequencer:
 */
struct state_program
{
	char		*pProgName;	/* program name (for debugging) */
	epicsThreadId	threadId;	/* thread id (main thread) */
	unsigned int	threadPriority;	/* thread priority */
	unsigned int	stackSize;	/* stack size */
	epicsMutexId	caSemId;	/* mutex for locking CA events */
	CHAN		*pChan;		/* table of channels */
	long		numChans;	/* number of db channels, incl. unass */
	long		assignCount;	/* number of db channels assigned */
	long		connCount;	/* number of channels connected */
	long		firstConnectCount;
	long		numMonitoredChans;
	long		firstMonitorCount;
	unsigned	allFirstConnectAndMonitor;
	SSCB		*pSS;		/* array of state set control blocks */
	long		numSS;		/* number of state sets */
	void		*pVar;		/* user variable area (or CA buffer in safe mode) */
	long		varSize;	/* # bytes in user variable area */
	MACRO		*pMacros;	/* ptr to macro table */
	char		*pParams;	/* program paramters */
	bitMask		*pEvents;	/* event bits for event flags & db */
	long		numEvents;	/* number of events */
	unsigned	options;	/* options (bit-encoded) */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
	epicsMutexId	logSemId;	/* logfile locking semaphore */
	FILE		*logFd;		/* logfile file descr. */
	char		*pLogFile;	/* logfile name */
	int		numQueues;	/* number of syncQ queues */
	ELLLIST		*pQueues;	/* ptr to syncQ queues */
};
typedef struct state_program SPROG;

/* Auxiliary thread arguments */
struct auxiliary_args
{
	char		*pPvSysName;	/* PV system ("ca", "ktl", ...) */
	long		debug;		/* debug level */
};
typedef struct auxiliary_args AUXARGS;

/* Macro parameters */
#define	MAX_MACROS	50

/* Thread parameters */
#define THREAD_NAME_SIZE	32
#define THREAD_STACK_SIZE	epicsThreadStackBig
#define THREAD_PRIORITY		epicsThreadPriorityMedium

/* Internal declarations */
extern void seqWakeup(SPROG *pSP, long eventNum);
extern void seqFree(SPROG *pSP);
extern long sequencer (SPROG *pSP);
extern long seqMacParse(char *pMacStr, SPROG *pSP);
extern char *seqMacValGet(MACRO *pMac, char *pName);
extern void seqMacEval(char *pInStr, char *pOutStr, long maxChar, MACRO *pMac);
extern SPROG *seqFindProg(epicsThreadId threadId);
extern epicsStatus seqDelProg(SPROG *pSP);
extern epicsStatus seqAddProg(SPROG *pSP);
extern void *seqAuxThread(void *);
extern epicsThreadId seqAuxThreadId;
extern void seq_get_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_put_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_mon_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_conn_handler(void *var,int connected);
typedef void seqTraversee(SPROG *prog, void *param);
extern epicsStatus seqTraverseProg(seqTraversee *pFunc, void *param);
extern long seq_connect(SPROG *pSP);
extern long seq_disconnect(SPROG *pSP);

#endif	/*INCLseqPvth*/
