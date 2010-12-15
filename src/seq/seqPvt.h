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

/* global variable for PV system context */
#ifdef DECLARE_PV_SYS
void *pvSys;
#else
extern void *pvSys;
#endif

#define valPtr(ch,ss)	((char*)basePtr(ch,ss)+(ch)->offset)
#define basePtr(ch,ss)	(((ch)->sprog->options&OPT_SAFE)?(ss)->var:bufPtr(ch))
#define bufPtr(ch)	(((ch)->sprog->options&OPT_REENT)?(ch)->sprog->var:0)

#define ssNum(ss)	((ss)->sprog->ss-(ss))

/* Generic iteration on lists */
#define foreach(e,l)		for (e = l; e != 0; e = e->next)

/* Generic min and max */
#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef max
#define max(x, y) (((x) < (y)) ? (y) : (x))
#endif

/* Generic allocation */
#define newArray(type,count)	(type *)calloc(count, sizeof(type))
#define new(type)		newArray(type,1)

typedef struct channel		CHAN;
typedef seqState		STATE;
typedef struct macro		MACRO;
typedef struct state_set	SSCB;
typedef struct program_instance	SPROG;
typedef struct auxiliary_args	AUXARGS;
typedef struct pvreq		PVREQ;
typedef const struct pv_type	PVTYPE;

/* Structure to hold information about database channels */
struct channel
{
	/* static channel data (assigned once on startup) */
	ptrdiff_t	offset;		/* offset to value */
	const char	*varName;	/* variable name */
	unsigned	count;		/* number of elements in array */
	unsigned	eventNum;	/* event number */
	PVTYPE		*type;		/* request type info */
	SPROG		*sprog;		/* state program that owns this struct*/

	/* dynamic channel data (assigned at runtime) */
	char		*dbAsName;	/* channel name from assign statement */
	EV_ID		efId;		/* event flag id if synced */
	boolean		monFlag;	/* whether channel shall be monitored */

	char		*dbName;	/* channel name after macro expansion */
	void		*pvid;		/* PV (process variable) id */
	boolean		assigned;	/* whether channel is assigned */
	boolean		connected;	/* whether channel is connected */
	boolean		*getComplete;	/* array of flags, one for each state set */
	epicsEventId	putSemId;	/* semaphore id for async put */
	short		status;		/* last db access status code */
	epicsTimeStamp	timeStamp;	/* time stamp */
	unsigned	dbCount;	/* actual count for db access */
	short		severity;	/* last db access severity code */
	const char	*message;	/* last db access error message */
	boolean		gotFirstMonitor;
	boolean		gotFirstConnect;
	boolean		monitored;	/* whether channel is monitored */
	void		*monid;		/* event id (supplied by PV lib) */
	QUEUE		queue;		/* queue if queued */

	/* buffer access, only used in safe mode */
	epicsMutexId	varLock;	/* mutex for un-assigned vars */
	boolean		*dirty;		/* array of dirty flags, one for each state set */
	boolean		wr_active;	/* buffer is currently being written */
};

struct pv_type
{
	const char	*typeStr;
	pvType		putType;
	pvType		getType;
	size_t		size;
};

/* Structure to hold information about a State Set */
struct state_set
{
	const char	*ssName;	/* state set name (for debugging) */
	epicsThreadId	threadId;	/* thread id */
	unsigned	threadPriority;	/* thread priority */
	unsigned	stackSize;	/* stack size */
	epicsEventId	allFirstConnectAndMonitorSemId;
	epicsEventId	syncSemId;	/* semaphore for event sync */
	epicsEventId	getSemId;	/* semaphore id for async get */
	epicsEventId	death1SemId;	/* semaphore id for death (#1) */
	epicsEventId	death2SemId;	/* semaphore id for death (#2) */
	epicsEventId	death3SemId;	/* semaphore id for death (#3) */
	epicsEventId	death4SemId;	/* semaphore id for death (#4) */
	unsigned	numStates;	/* number of states */
	STATE		*states;	/* ptr to array of state blocks */
	short		currentState;	/* current state index */
	short		nextState;	/* next state index */
	short		prevState;	/* previous state index */
	short		transNum;	/* highest prio trans. # triggered */
	const bitMask	*mask;		/* current event mask */
	unsigned	maxNumDelays;	/* max. number of delays */
	unsigned	numDelays;	/* number of delays activated */
	double		*delay;		/* queued delay value in secs (array) */
	boolean		*delayExpired;	/* TRUE if delay expired (array) */
	double		timeEntered;	/* time that a state was entered */
	char		*var;		/* variable value block (safe mode) */
	SPROG		*sprog;	/* ptr back to state program block */
};

/* All information about a state program.
   The address of this structure is passed to the run-time sequencer.
 */
struct program_instance
{
	const char	*progName;	/* program name (for debugging) */
	epicsThreadId	threadId;	/* thread id (main thread) */
	unsigned	threadPriority;	/* thread priority */
	unsigned	stackSize;	/* stack size */
	epicsMutexId	programLock;	/* mutex for locking CA events */
	CHAN		*chan;		/* table of channels */
	unsigned	numChans;	/* number of db channels, incl. unass */
	unsigned	assignCount;	/* number of db channels assigned */
	unsigned	connectCount;	/* number of channels connected */
	unsigned	firstConnectCount;
	unsigned	numMonitoredChans;
	unsigned	firstMonitorCount;
	unsigned	allFirstConnectAndMonitor;
	boolean		allDisconnected;
	QUEUE		*queues;	/* array of syncQ queues */
	unsigned	numQueues;	/* number of syncQ queues */
	SSCB		*ss;		/* array of state set control blocks */
	unsigned	numSS;		/* number of state sets */
	char		*var;		/* user variable area (or CA buffer in safe mode) */
	size_t		varSize;	/* # bytes in user variable area */
	MACRO		*macros;	/* ptr to macro table */
	char		*params;	/* program paramters */
	bitMask		*events;	/* event bits for event flags & db */
	unsigned	numEvents;	/* number of events */
	unsigned	options;	/* options (bit-encoded) */
	INIT_FUNC	*initFunc;	/* init function */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
	void		*pvReqPool;	/* freeList for pv requests */

	int		instance;	/* program instance number */
	SPROG		*next;		/* next element in program list */
};

/* Auxiliary thread arguments */
struct auxiliary_args
{
	char		*pvSysName;	/* PV system ("ca", "ktl", ...) */
	int		debug;		/* debug level */
};

struct pvreq
{
	CHAN		*ch;		/* requested variable */
	SSCB		*ss;		/* state set that made the request */
};

/* Thread parameters */
#define THREAD_NAME_SIZE	32
#define THREAD_STACK_SIZE	epicsThreadStackBig
#define THREAD_PRIORITY		epicsThreadPriorityMedium

/* Internal procedures */

/* seq_task.c */
void sequencer (SPROG *sp);
void ss_write_buffer(CHAN *ch, void *val);
void seqWakeup(SPROG *sp, unsigned eventNum);
void seqFree(SPROG *sp);
void *seqAuxThread(void *);
epicsThreadId seqAuxThreadId;
/* seq_mac.c */
void seqMacParse(SPROG *sp, const char *macStr);
char *seqMacValGet(SPROG *sp, const char *name);
void seqMacEval(SPROG *sp, const char *inStr, char *outStr, size_t maxChar);
void seqMacFree(SPROG *sp);
/* seq_ca.c */
void seq_get_handler(void *var, pvType type, int count,
	pvValue *value, void *arg, pvStat status);
void seq_put_handler(void *var, pvType type, int count,
	pvValue *value, void *arg, pvStat status);
void seq_mon_handler(void *var, pvType type, int count,
	pvValue *value, void *arg, pvStat status);
void seq_conn_handler(void *var,int connected);
pvStat seq_connect(SPROG *sp);
pvStat seq_disconnect(SPROG *sp);
/* seq_prog.c */
typedef int seqTraversee(SPROG *prog, void *param);
void seqTraverseProg(seqTraversee *func, void *param);
SSCB *seqFindStateSet(epicsThreadId threadId);
SPROG *seqFindProg(epicsThreadId threadId);
void seqDelProg(SPROG *sp);
void seqAddProg(SPROG *sp);
/* seqCommands.c */
typedef int sequencerProgramTraversee(SPROG **sprog, seqProgram *pseq, void *param);
void traverseSequencerPrograms(sequencerProgramTraversee *traversee, void *param);

/* debug/query support */
typedef int pr_fun(const char *format,...);
static int nothing (const char *format,...) {return 0;}

#endif	/*INCLseqPvth*/
