/*	Common defs for state programs and run-time sequencer
 *
 *	Author:		Andy Kozubal
 *	Date:		01mar94
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1993 the Regents of the University of California,
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
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 */
#ifndef INCLseqComh
#define INCLseqComh

#include "shareLib.h"
#include "pvAlarm.h"
#include "epicsThread.h"
#include "epicsTime.h"

#include "seqVersion.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bit encoding for run-time options */
#define OPT_DEBUG		(1<<0)	/* turn on debugging */
#define OPT_ASYNC		(1<<1)	/* use async. gets */
#define OPT_CONN		(1<<2)	/* wait for all connections */
#define OPT_REENT		(1<<3)	/* generate reentrant code */
#define OPT_NEWEF		(1<<4)	/* new event flag mode */
#define OPT_MAIN		(1<<5)	/* generate main program */
#define OPT_SAFE		(1<<6)	/* safe mode */

/* Bit encoding for State Specific Options */
#define OPT_NORESETTIMERS	(1<<0)	/* If TRUE, don't reset timers on */
					/* entry to state from same state */
#define OPT_DOENTRYFROMSELF	(1<<1)	/* Do entry{} even if from same state */
#define OPT_DOEXITTOSELF	(1<<2)	/* Do exit{} even if to same state */

/* Macros to handle set & clear event bits */
typedef unsigned long	bitMask;
#define NBITS	(8*sizeof(bitMask))	/* # bits in bitMask word */

#define bitSet(word, bitnum)	(word[(bitnum)/NBITS] |=  (1<<((bitnum)%NBITS)))
#define bitClear(word, bitnum)	(word[(bitnum)/NBITS] &= ~(1<<((bitnum)%NBITS)))
#define bitTest(word, bitnum)	((word[(bitnum)/NBITS] &   (1<<((bitnum)%NBITS))) != 0)

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

typedef	struct state_set_control_block *SS_ID;	/* state set id */

typedef struct UserVar USER_VAR;	/* defined by program */

/* Prototype for action, event, delay, and exit functions */
typedef void ACTION_FUNC(SS_ID ssId, USER_VAR *pVar, short transNum, short *pNextState);
typedef unsigned EVENT_FUNC(SS_ID ssId, USER_VAR *pVar, short *pTransNum, short *pNextState);
typedef void DELAY_FUNC(SS_ID ssId, USER_VAR *pVar);
typedef void ENTRY_FUNC(SS_ID ssId, USER_VAR *pVar);
typedef void EXIT_FUNC(SS_ID ssId, USER_VAR *pVar);

#ifdef OFFSET
#undef OFFSET
#endif
/* The OFFSET macro calculates the byte offset of a structure member
 * from the start of a structure */
#define OFFSET(structure, member) ((long) &(((structure *) 0) -> member))

/* Structure to hold information about database channels */
struct seqChan
{
	char		*dbAsName;	/* assigned channel name */
	ptrdiff_t	offset;		/* offset to value */
	char		*pVarName;	/* variable name, including subscripts*/
	char		*pVarType;	/* variable type, e.g. "int" */
	long		count;		/* element count for arrays */
	long		eventNum;	/* event number for this channel */
	long		efId;		/* event flag id if synced */
	long		monFlag;	/* whether channel is to be monitored */
	int		queued;		/* whether queued via syncQ */
	int		maxQueueSize;	/* max syncQ queue size (0 => def) */
	int		queueIndex;	/* syncQ queue index */
};

/* Structure to hold information about a state */
struct seqState
{
	char		*pStateName;	/* state name */
	ACTION_FUNC	*actionFunc;	/* action routine for this state */
	EVENT_FUNC	*eventFunc;	/* event routine for this state */
	DELAY_FUNC	*delayFunc;	/* delay setup routine for this state */
	ENTRY_FUNC	*entryFunc;	/* statements performed on entry to state */
	EXIT_FUNC	*exitFunc;	/* statements performed on exit from state */
	bitMask		*pEventMask;	/* event mask for this state */
	bitMask		options;	/* State specific option mask */ 
};

/* Structure to hold information about a State Set */
struct seqSS
{
	char		*pSSName;	/* state set name */
	struct seqState	*pStates;	/* array of state blocks */
	long		numStates;	/* number of states in this state set */
	long		numDelays;	/* number of delays in this state set */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
};

/* All information about a state program */
struct seqProgram
{
	long		magic;		/* magic number */
	char		*pProgName;	/* program name (for debugging) */
	struct seqChan	*pChan;		/* table of channels */
	long		numChans;	/* number of db channels */
	struct seqSS	*pSS;		/* array of state set info structs */
	long		numSS;		/* number of state sets */
	long		varSize;	/* # bytes in user variable area */
	char		*pParams;	/* program paramters */
	long		numEvents;	/* number of event flags */
	unsigned	options;	/* options (bit-encoded) */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
	int		numQueues;	/* number of syncQ queues */
};

/*
 * Function declarations for interface between state program & sequencer.
 * Prefix "seq_" is added by SNC to reduce probability of name clashes.
 * Implementations are in module seq_if.c.
 */

/* I/O completion type (extra argument passed to seq_pvGet() and seq_pvPut()) */
#define HONOR_OPTION 0
#define ASYNCHRONOUS 1
#define SYNCHRONOUS  2

epicsShareFunc void epicsShareAPI seq_efSet(SS_ID, long);		/* set an event flag */
epicsShareFunc long epicsShareAPI seq_efTest(SS_ID, long);		/* test an event flag */
epicsShareFunc long epicsShareAPI seq_efClear(SS_ID, long);		/* clear an event flag */
epicsShareFunc long epicsShareAPI seq_efTestAndClear(SS_ID, long);	/* test & clear an event flag */
epicsShareFunc long epicsShareAPI seq_pvGet(SS_ID, long, long);		/* get pv value */
epicsShareFunc int epicsShareAPI seq_pvGetQ(SS_ID, int);		/* get queued pv value */
epicsShareFunc int epicsShareAPI seq_pvFreeQ(SS_ID, int);		/* free elements on pv queue */
epicsShareFunc long epicsShareAPI seq_pvPut(SS_ID, long, long);		/* put pv value */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID, long);/* get time stamp value */
epicsShareFunc long epicsShareAPI seq_pvAssign(SS_ID, long, char *);	/* assign/connect to a pv */
epicsShareFunc long epicsShareAPI seq_pvMonitor(SS_ID, long);		/* enable monitoring on pv */
epicsShareFunc long epicsShareAPI seq_pvStopMonitor(SS_ID, long);	/* disable monitoring on pv */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID, long);		/* pv name */
epicsShareFunc long epicsShareAPI seq_pvStatus(SS_ID, long);		/* pv alarm status code */
epicsShareFunc long epicsShareAPI seq_pvSeverity(SS_ID, long);		/* pv alarm severity */
epicsShareFunc char *epicsShareAPI seq_pvMessage(SS_ID, long);		/* pv error message */
epicsShareFunc long epicsShareAPI seq_pvAssigned(SS_ID, long);		/* whether assigned */
epicsShareFunc long epicsShareAPI seq_pvConnected(SS_ID, long);		/* whether connected */
epicsShareFunc long epicsShareAPI seq_pvGetComplete(SS_ID, long);	/* whether last get completed */
epicsShareFunc long epicsShareAPI seq_pvPutComplete(SS_ID, long, long, long, long *);
									/* whether last put completed */
epicsShareFunc long epicsShareAPI seq_pvChannelCount(SS_ID);		/* number of channels */
epicsShareFunc long epicsShareAPI seq_pvConnectCount(SS_ID);		/* number of channels conn'ed */
epicsShareFunc long epicsShareAPI seq_pvAssignCount(SS_ID);		/* number of channels ass'ned */
epicsShareFunc long epicsShareAPI seq_pvCount(SS_ID, long);		/* number of elements in arr */
epicsShareFunc void epicsShareAPI seq_pvFlush(void);			/* flush put/get requests */
epicsShareFunc long epicsShareAPI seq_pvIndex(SS_ID, long);		/* index of pv */
epicsShareFunc long               seq_seqLog(SS_ID, const char *, ...);	/* logging */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID, long, double);	/* initialize a delay entry */
epicsShareFunc long epicsShareAPI seq_delay(SS_ID, long);		/* test a delay entry */
epicsShareFunc char *epicsShareAPI seq_macValueGet(SS_ID, char *);	/* given macro name, return ptr to val*/
epicsShareFunc unsigned epicsShareAPI seq_optGet (SS_ID ssId, char *opt);
									/* check an option for TRUE/FALSE */

epicsShareFunc long epicsShareAPI seqShow (epicsThreadId);
epicsShareFunc long epicsShareAPI seqChanShow (epicsThreadId, char *);
epicsShareFunc long epicsShareAPI seqcar(int level);
epicsShareFunc void epicsShareAPI seqcaStats(int *pchans, int *pdiscon);
epicsShareFunc long epicsShareAPI seqQueueShow (epicsThreadId tid);
epicsShareFunc long epicsShareAPI seqStop (epicsThreadId);
epicsShareFunc void epicsShareAPI seqRegisterSequencerProgram (struct seqProgram *p);
epicsShareFunc void epicsShareAPI seqRegisterSequencerCommands (void);
epicsShareFunc epicsThreadId epicsShareAPI seq(struct seqProgram *, char *, unsigned int);
epicsShareFunc struct state_program* epicsShareAPI seqFindProgByName (char *, int);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif	/*INCLseqComh*/
