/*************************************************************************\
Copyright (c) 1993      The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2013 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*	Public interface to the sequencer run-time library
 *
 *	Author:		Andy Kozubal
 *	Date:		01mar94
 *
 *	Experimental Physics and Industrial Control System (EPICS)
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

#include "epicsTypes.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "shareLib.h"

#include "pvAlarm.h"
#include "seq_release.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NOEVFLAG		0	/* argument to pvSync to remove sync */

#define DEFAULT_QUEUE_SIZE	2	/* default queue size (elements) */

/* I/O completion type (extra argument passed to seq_pvGet() and seq_pvPut()) */
enum compType {
	DEFAULT,
        ASYNC,
        SYNC
};

typedef	struct state_set *const SS_ID;	/* state set id, opaque */
typedef struct _seq_var SEQ_VARS;	/* defined by program, opaque */
typedef char string[MAX_STRING_SIZE];	/* the string typedef */

typedef SEQ_VARS USER_VAR;              /* for compatibility */

/* these typedefs make the code more self documenting */
typedef unsigned EV_ID;			/* identifier for an event */
typedef unsigned VAR_ID;		/* identifier for a pv */
typedef int seqBool;

typedef struct seqProgram seqProgram;   /* program object, opaque */

/*
 * Function declarations for interface between state program & sequencer.
 * Prefix "seq_" is added by SNC to reduce probability of name clashes.
 * Implementations are in module seq_if.c.
 */

/* event flag operations */
epicsShareFunc void seq_efSet(SS_ID, EV_ID);
epicsShareFunc seqBool seq_efTest(SS_ID, EV_ID);
epicsShareFunc seqBool seq_efClear(SS_ID, EV_ID);
epicsShareFunc seqBool seq_efTestAndClear(SS_ID, EV_ID);
/* pv operations */
epicsShareFunc pvStat seq_pvGet(SS_ID, VAR_ID, enum compType);
epicsShareFunc pvStat seq_pvGetMultiple(SS_ID, VAR_ID,
	unsigned, enum compType);
epicsShareFunc seqBool seq_pvGetQ(SS_ID, VAR_ID);
epicsShareFunc void seq_pvFlushQ(SS_ID, VAR_ID);
/* retain seq_pvFreeQ for compatibility */
#define seq_pvFreeQ seq_pvFlushQ
epicsShareFunc pvStat seq_pvPut(SS_ID, VAR_ID, enum compType);
epicsShareFunc pvStat seq_pvPutMultiple(SS_ID, VAR_ID,
	unsigned, enum compType);
epicsShareFunc seqBool seq_pvGetComplete(SS_ID, VAR_ID);
epicsShareFunc seqBool seq_pvPutComplete(SS_ID, VAR_ID,
	unsigned, seqBool, seqBool*);
epicsShareFunc pvStat seq_pvAssign(SS_ID, VAR_ID, const char *);
epicsShareFunc pvStat seq_pvMonitor(SS_ID, VAR_ID);
epicsShareFunc void seq_pvSync(SS_ID, VAR_ID, unsigned, EV_ID);
epicsShareFunc pvStat seq_pvStopMonitor(SS_ID, VAR_ID);
/* pv info */
epicsShareFunc char *seq_pvName(SS_ID, VAR_ID);
epicsShareFunc unsigned seq_pvCount(SS_ID, VAR_ID);
epicsShareFunc pvStat seq_pvStatus(SS_ID, VAR_ID);
epicsShareFunc pvSevr seq_pvSeverity(SS_ID, VAR_ID);
epicsShareFunc epicsTimeStamp seq_pvTimeStamp(SS_ID, VAR_ID);
epicsShareFunc const char *seq_pvMessage(SS_ID, VAR_ID);
epicsShareFunc seqBool seq_pvAssigned(SS_ID, VAR_ID);
epicsShareFunc seqBool seq_pvConnected(SS_ID, VAR_ID);

#define seq_pvIndex(ssId, varId)	varId
#define seq_ssId(ssId)			ssId
#define seq_pVar(ssId)			_seq_var

/* global operations */
epicsShareFunc void seq_pvFlush(SS_ID);
epicsShareFunc seqBool seq_delay(SS_ID, double);
epicsShareFunc char *seq_macValueGet(SS_ID, const char *);
epicsShareFunc void seq_exit(SS_ID);

/* global info */
epicsShareFunc unsigned seq_pvChannelCount(SS_ID);
epicsShareFunc unsigned seq_pvConnectCount(SS_ID);
epicsShareFunc unsigned seq_pvAssignCount(SS_ID);
epicsShareFunc seqBool seq_optGet(SS_ID, const char *);

/* shell commands */
epicsShareFunc void seqShow(epicsThreadId);
epicsShareFunc void seqChanShow(epicsThreadId, const char *);
epicsShareFunc void seqcar(int level);
epicsShareFunc void seqQueueShow(epicsThreadId);
epicsShareFunc void seqStop(epicsThreadId);
epicsShareFunc epicsThreadId seq(seqProgram *, const char *, unsigned);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif	/*INCLseqComh*/
