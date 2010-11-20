/*	Internal common definitions for the run-time sequencer library
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

#include "seq_queue.h"

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

#define valPtr(ch,ss)	(basePtr(ch,ss)+(ch)->offset)
#define basePtr(ch,ss)	(((ch)->sprog->options&OPT_SAFE)?(ss)->pVar:bufPtr(ch))
#define bufPtr(ch)	(((ch)->sprog->options&OPT_REENT)?(ch)->sprog->pVar:0)

#define ssNum(ss)	((ss)->sprog->pSS-(ss))

#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef max
#define max(x, y) (((x) < (y)) ? (y) : (x))
#endif

/* Allocation */
#define newArray(type,count)	(type *)calloc(count, sizeof(type))
#define new(type)		newArray(type,1)

typedef struct db_channel CHAN;
typedef struct queue_entry QENTRY;
typedef struct seqState STATE;
typedef struct macro MACRO;
typedef struct state_set_control_block SSCB;
typedef struct state_program SPROG;
typedef struct auxiliary_args AUXARGS;
typedef struct pvreq PVREQ;

/* Structure to hold information about database channels */
struct db_channel
{
	/* static channel data (assigned once on startup) */
	ptrdiff_t	offset;		/* offset to value */
	char		*pVarName;	/* variable name */
	char		*pVarType;	/* variable type string (e.g. ("int") */
	long		count;		/* number of elements in array */
	long		eventNum;	/* event number */
	int		queued;		/* whether queued via syncQ */
	int		maxQueueSize;	/* max syncQ queue size (0 => def) */
	int		queueIndex;	/* syncQ queue index */
	SPROG		*sprog;		/* state program that owns this struct*/

	/* dynamic channel data (assigned at runtime) */
	char		*dbAsName;	/* channel name from assign statement */
	long		efId;		/* event flag id if synced */
	unsigned	monFlag;	/* whether channel shall be monitored */

	char		*dbName;	/* channel name after macro expansion */
	void		*pvid;		/* PV (process variable) id */
	unsigned	assigned;	/* whether channel is assigned */
	unsigned	connected;	/* whether channel is connected */
	unsigned	*getComplete;	/* array of flags, one for each state set */
	epicsEventId	putSemId;	/* semaphore id for async put */
	short		dbOffset;	/* offset to value in db access struct */
	short		status;		/* last db access status code */
	epicsTimeStamp	timeStamp;	/* time stamp */
	long		dbCount;	/* actual count for db access */
	short		severity;	/* last db access severity code */
	short		size;		/* size (in bytes) of single var elem */
	short		getType;	/* db get type (e.g. DBR_STS_INT) */
	short		putType;	/* db put type (e.g. DBR_INT) */
	const char	*message;	/* last db access error message */
	unsigned	gotFirstMonitor;
	unsigned	gotFirstConnect;
	unsigned	monitored;	/* whether channel is monitored */
	void		*monid;		/* event id (supplied by PV lib) */

	/* buffer access, only used in safe mode */
	epicsMutexId	varLock;	/* mutex for un-assigned vars */
	unsigned	*dirty;		/* array of dirty flags, one for each state set */
	unsigned	wr_active;	/* buffer is currently being written */
};

/* Structure for syncQ queue entry */
struct queue_entry
{
	ELLNODE		node;		/* linked list node */
	CHAN		*pDB;		/* ptr to db channel info */
	pvValue		value;		/* value, time stamp etc */
};

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
	epicsEventId	death1SemId;	/* semaphore id for death (#1) */
	epicsEventId	death2SemId;	/* semaphore id for death (#2) */
	epicsEventId	death3SemId;	/* semaphore id for death (#3) */
	epicsEventId	death4SemId;	/* semaphore id for death (#4) */
	long		numStates;	/* number of states */
	STATE		*pStates;	/* ptr to array of state blocks */
	short		currentState;	/* current state index */
	short		nextState;	/* next state index */
	short		prevState;	/* previous state index */
	short		transNum;	/* highest prio trans. # triggered */
	bitMask		*pMask;		/* current event mask */
	long		maxNumDelays;	/* max. number of delays */
	long		numDelays;	/* number of delays activated */
	double		*delay;		/* queued delay value in secs (array) */
	unsigned	*delayExpired;	/* TRUE if delay expired (array) */
	double		timeEntered;	/* time that a state was entered */
	void		*pVar;		/* variable value block (safe mode) */
	struct state_program *sprog;	/* ptr back to state program block */
};

/* All information about a state program.
	The address of this structure is passed to the run-time sequencer:
 */
struct state_program
{
	char		*pProgName;	/* program name (for debugging) */
	epicsThreadId	threadId;	/* thread id (main thread) */
	unsigned int	threadPriority;	/* thread priority */
	unsigned int	stackSize;	/* stack size */
	epicsMutexId	programLock;	/* mutex for locking CA events */
	CHAN		*pChan;		/* table of channels */
	long		numChans;	/* number of db channels, incl. unass */
	long		assignCount;	/* number of db channels assigned */
	long		connectCount;	/* number of channels connected */
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
	INIT_FUNC	*initFunc;	/* init function */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
	int		numQueues;	/* number of syncQ queues */
	ELLLIST		*pQueues;	/* ptr to syncQ queues */
	void		*pvReqPool;	/* freeList for pv requests */

	int		instance;	/* program instance number */
	SPROG		*next;		/* next element in program list */
};

/* Auxiliary thread arguments */
struct auxiliary_args
{
	char		*pPvSysName;	/* PV system ("ca", "ktl", ...) */
	long		debug;		/* debug level */
};

struct pvreq
{
	CHAN		*pDB;		/* requested variable */
	SSCB		*pSS;		/* state set that made the request */
};

/* Thread parameters */
#define THREAD_NAME_SIZE	32
#define THREAD_STACK_SIZE	epicsThreadStackBig
#define THREAD_PRIORITY		epicsThreadPriorityMedium

/* Internal procedures */

/* Generic iteration on lists */
#define foreach(e,l)		for (e = l; e != 0; e = e->next)

extern void seqWakeup(SPROG *pSP, long eventNum);
extern void seqFree(SPROG *pSP);
extern long sequencer (SPROG *pSP);
extern void *seqAuxThread(void *);
extern epicsThreadId seqAuxThreadId;
/* seq_mac.c */
extern void seqMacParse(SPROG *pSP, char *pMacStr);
extern char *seqMacValGet(SPROG *pSP, char *pName);
extern void seqMacEval(SPROG *pSP, char *pInStr, char *pOutStr, size_t maxChar);
extern void seqMacFree(SPROG *pSP);
extern void seq_get_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_put_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_mon_handler(
	void *var, pvType type, int count, pvValue *pValue, void *arg, pvStat status);
extern void seq_conn_handler(void *var,int connected);
/* seq_prog.c */
typedef int seqTraversee(SPROG *prog, void *param);
void seqTraverseProg(seqTraversee *pFunc, void *param);
SSCB *seqFindStateSet(epicsThreadId threadId);
SPROG *seqFindProg(epicsThreadId threadId);
void seqDelProg(SPROG *pSP);
void seqAddProg(SPROG *pSP);
/* seqCommands.c */
typedef int sequencerProgramTraversee(SPROG **sprog, struct seqProgram *pseq, void *param);
void traverseSequencerPrograms(sequencerProgramTraversee *traversee, void *param);

extern long seq_connect(SPROG *pSP);
extern long seq_disconnect(SPROG *pSP);
extern void ss_write_buffer(CHAN *pDB, void *pVal);

/* debug/query support */
typedef int pr_fun(const char *format,...);
static int nothing (const char *format,...) {return 0;}

#endif	/*INCLseqPvth*/
