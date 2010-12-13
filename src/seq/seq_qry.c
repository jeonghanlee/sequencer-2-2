/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California.
		         Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)

	Task query & debug routines for run-time sequencer
***************************************************************************/
#include "seq.h"

static int wait_rtn(void);
static void printValue(void *pVal, unsigned count, int type);
static SPROG *seqQryFind(epicsThreadId tid);
static void seqShowAll(void);

/*
 * seqShow() - Query the sequencer for state information.
 * If a non-zero thread id is specified then print the information about
 * the state program, otherwise print a brief summary of all state programs
 */
epicsShareFunc void epicsShareAPI seqShow(epicsThreadId tid)
{
	SPROG	*pSP;
	SSCB	*pSS;
	STATE	*pST;
	unsigned nss;
	double	timeNow, timeElapsed;

	pSP = seqQryFind(tid);
	if (pSP == NULL)
		return;

	/* Print info about state program */
	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("  initial thread id = %p\n", pSP->threadId);
	printf("  thread priority = %d\n", pSP->threadPriority);
	printf("  number of state sets = %u\n", pSP->numSS);
	printf("  number of syncQ queues = %u\n", pSP->numQueues);
	if (pSP->numQueues > 0)
		printf("  queue array address = %p\n",pSP->pQueues);
	printf("  number of channels = %u\n", pSP->numChans);
	printf("  number of channels assigned = %u\n", pSP->assignCount);
	printf("  number of channels connected = %u\n", pSP->connectCount);
	printf("  options: async=%d, debug=%d, newef=%d, reent=%d, conn=%d, "
		"main=%d\n",
	 ((pSP->options & OPT_ASYNC) != 0), ((pSP->options & OPT_DEBUG) != 0),
	 ((pSP->options & OPT_NEWEF) != 0), ((pSP->options & OPT_REENT) != 0),
	 ((pSP->options & OPT_CONN)  != 0), ((pSP->options & OPT_MAIN)  != 0));
	if ((pSP->options & OPT_REENT) != 0)
		printf("  user variables: address = %p, length = %u "
			"= 0x%x bytes\n",
			pSP->pVar, pSP->varSize, pSP->varSize);
	printf("\n");

	/* Print state set info */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		unsigned n;

		printf("  State Set: \"%s\"\n", pSS->pSSName);

		if (pSS->threadId != (epicsThreadId)0)
		{
			char threadName[THREAD_NAME_SIZE];
			epicsThreadGetName(pSS->threadId, threadName,sizeof(threadName));
			printf("  thread name = %s;", threadName);
		}

		printf("  thread id = %p\n", pSS->threadId);

		pST = pSS->pStates;
		printf("  First state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->currentState;
		printf("  Current state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->prevState;
		printf("  Previous state = \"%s\"\n", pSS->prevState >= 0 ?
			pST->pStateName : "");

		pvTimeGetCurrentDouble(&timeNow);
		timeElapsed = timeNow - pSS->timeEntered;
		printf("  Elapsed time since state was entered = %.1f "
			"seconds\n", timeElapsed);
		printf("  Queued time delays:\n");
		for (n = 0; n < pSS->numDelays; n++)
		{
			printf("\tdelay[%2d]=%f", n, pSS->delay[n]);
			if (pSS->delayExpired[n])
				printf(" - expired");
			printf("\n");
		}
		printf("\n");
	}
}
/*
 * seqChanShow() - Show channel information for a state program.
 */
epicsShareFunc void epicsShareAPI seqChanShow(epicsThreadId tid, const char *pStr)
{
	SPROG	*pSP;
	CHAN	*pDB;
	int	nch;
        int	n;
	char	tsBfr[50], connQual;
	int	match, showAll;

	pSP = seqQryFind(tid);
	if(!pSP) return;

	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("Number of channels=%u\n", pSP->numChans);

	if (pStr != NULL)
	{
		connQual = pStr[0];
		/* Check for channel connect qualifier */
		if ((connQual == '+') || (connQual == '-'))
		{
			pStr += 1;
		}
	}
	else
		connQual = 0;

	pDB = pSP->pChan;
	for (nch = 0; (unsigned)nch < pSP->numChans; )
	{
		if (pStr != NULL)
		{
			/* Check for channel connect qualifier */
			if (connQual == '+')
				showAll = pDB->connected;
			else if (connQual == '-')
				showAll = pDB->assigned && (!pDB->connected);
			else
				showAll = TRUE;

			/* Check for pattern match if specified */
			match = (pStr[0] == 0) ||
					(strstr(pDB->dbName, pStr) != NULL);
			if (!(match && showAll))
			{
				pDB += 1;
				nch += 1;
				continue; /* skip this channel */
			}
		}
		printf("\n#%d of %u:\n", nch+1, pSP->numChans);
		printf("Channel name: \"%s\"\n", pDB->dbName);
		printf("  Unexpanded (assigned) name: \"%s\"\n", pDB->dbAsName);
		printf("  Variable name: \"%s\"\n", pDB->pVarName);
		printf("    offset = %d\n", pDB->offset);
		printf("    type = %s\n", pDB->pVarType);
		printf("    count = %u\n", pDB->count);
		printValue(bufPtr(pDB)+pDB->offset, pDB->count, pDB->putType);

		printf("  Monitor flag = %d\n", pDB->monFlag);
		if (pDB->monitored)
			printf("    Monitored\n");
		else
			printf("    Not monitored\n");

		if (pDB->assigned)
			printf("  Assigned\n");
		else
			printf("  Not assigned\n");

		if(pDB->connected)
			printf("  Connected\n");
		else
			printf("  Not connected\n");

		if(pDB->getComplete)
			printf("  Last get completed\n");
		else
			printf("  Get not completed or no get issued\n");

		if(epicsEventTryWait(pDB->putSemId))
			printf("  Last put completed\n");
		else
			printf("  Put not completed or no put issued\n");

		printf("  Status = %d\n", pDB->status);
		printf("  Severity = %d\n", pDB->severity);
		printf("  Message = %s\n", pDB->message != NULL ?
			pDB->message : "");

		/* Print time stamp in text format: "yyyy/mm/dd hh:mm:ss.sss" */
		epicsTimeToStrftime(tsBfr, sizeof(tsBfr),
			"%Y/%m/%d %H:%M:%S.%03f", &pDB->timeStamp);
		printf("  Time stamp = %s\n", tsBfr);

		n = wait_rtn();
		if (n == 0)
			return;
		nch += n;
		if (nch < 0)
			nch = 0;
		pDB = pSP->pChan + nch;
	}
}
/*
 * seqcar() - Sequencer Channel Access Report
 */

struct seqStats
{
	int	level;
	int	nProgs;
	int	nChans;
	int	nConn;
};

static int seqcarCollect(SPROG *pSP, void *param)
{
	struct seqStats *pstats = (struct seqStats *) param;
	CHAN	*pDB = pSP->pChan;
	unsigned nch;
	int	level = pstats->level;
	int 	printedProgName = 0;
	pstats->nProgs++;
	for (nch = 0; nch < pSP->numChans; nch++)
	{
		if (pDB->assigned) pstats->nChans++;
		if (pDB->connected) pstats->nConn++;
		if (level > 1 ||
		    (level == 1 && !pDB->connected))
		    {
			if (!printedProgName)
			{
				printf("  Program \"%s\"\n", pSP->pProgName);
				printedProgName = 1;
			}
			printf("    Variable \"%s\" %sconnected to PV \"%s\"\n",
				pDB->pVarName,
				pDB->connected ? "" : "not ",
				pDB->dbName);
		}
		pDB++;
	}
	return FALSE;	/* continue traversal */
}

epicsShareFunc void epicsShareAPI seqcar(int level)
{
	struct seqStats stats = {0, 0, 0, 0};
	int diss;
	stats.level = level;
	seqTraverseProg(seqcarCollect, (void *) &stats);
	diss = stats.nChans - stats.nConn;
	printf("Total programs=%d, channels=%d, connected=%d, disconnected=%d\n",
		stats.nProgs, stats.nChans, stats.nConn, diss);
	return;
}

#if 0
epicsShareFunc void epicsShareAPI seqcaStats(int *pchans, int *pdiscon)
{
	struct seqStats stats = {0, 0, 0, 0};
	seqTraverseProg(seqcarCollect, (void *) &stats);
	if (pchans)  *pchans  = stats.nChans;
	if (pdiscon) *pdiscon = stats.nChans - stats.nConn;
}
#endif

/*
 * seqQueueShow() - Show syncQ queue information for a state program.
 */
epicsShareFunc void epicsShareAPI seqQueueShow(epicsThreadId tid)
{
	SPROG	*pSP;
	ELLLIST	*pQueue;
	int	nque, n;
	char	tsBfr[50];

	pSP = seqQryFind(tid);
	if(!pSP) return;

	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("Number of queues = %d\n", pSP->numQueues);

	pQueue = pSP->pQueues;
	for (nque = 0; (unsigned)nque < pSP->numQueues; )
	{
		QENTRY	*pEntry;
		int i;

		printf("\nQueue #%d of %d:\n", nque+1, pSP->numQueues);
		printf("Number of entries = %d\n", ellCount(pQueue));
		for (pEntry = (QENTRY *) ellFirst(pQueue), i = 1;
		     pEntry != NULL;
		     pEntry = (QENTRY *) ellNext(&pEntry->node), i++)
		{
			CHAN	*pDB = pEntry->pDB;
			pvValue	*pAccess = &pEntry->value;
			void	*pVal = (char *)pAccess + pDB->dbOffset;

			printf("\nEntry #%d: channel name: \"%s\"\n",
							    i, pDB->dbName);
			printf("  Variable name: \"%s\"\n", pDB->pVarName);
			printValue(pVal, 1, pDB->putType);
							/* was pDB->count */
			printf("  Status = %d\n",
					pAccess->timeStringVal.status);
			printf("  Severity = %d\n",
					pAccess->timeStringVal.severity);

			/* Print time stamp in text format:
			   "yyyy/mm/dd hh:mm:ss.sss" */
			epicsTimeToStrftime(tsBfr, sizeof(tsBfr), "%Y/%m/%d "
				"%H:%M:%S.%03f", &pAccess->timeStringVal.stamp);
			printf("  Time stamp = %s\n", tsBfr);
		}

		n = wait_rtn();
		if (n == 0)
			return;
		nque += n;
		if (nque < 0)
			nque = 0;
		pQueue = pSP->pQueues + nque;
	}
}

/* Read from console until a RETURN is detected */
static int wait_rtn(void)
{
	char	bfr[10];
	int	i, n;

	printf("Next? (+/- skip count)\n");
	for (i = 0;  i < 10; i++)
	{
		int c = getchar();
		if (c == EOF)
			break;
		if ((bfr[i] = (char)c) == '\n')
			break;
	}
	bfr[i] = 0;
	if (bfr[0] == 'q')
		return 0; /* quit */

	n = atoi(bfr);
	if (n == 0)
		n = 1;
	return n;
}

/* Print the current internal value of a database channel */
static void printValue(void *pVal, unsigned count, int type)
{
	char	*c = (char *)pVal;
	short	*s = (short *)pVal;
	long	*l = (long *)pVal;
	float	*f = (float *)pVal;
	double	*d = (double *)pVal;
	typedef char string[MAX_STRING_SIZE];
	string	*t = (string *)pVal;

	while (count--)
	{
		switch (type)
		{
		case pvTypeSTRING:
			printf(" \"%.*s\"", MAX_STRING_SIZE, *t++);
			break;
		case pvTypeCHAR:
			printf(" %d", *c++);
			break;
		case pvTypeSHORT:
			printf(" %d", *s++);
			break;
		case pvTypeLONG:
			printf(" %ld", *l++);
			break;
		case pvTypeFLOAT:
			printf(" %g", *f++);
			break;
		case pvTypeDOUBLE:
			printf(" %g", *d++);
			break;
		}
	}
	printf("\n");
}

/* Find a state program associated with a given thread id */
static SPROG *seqQryFind(epicsThreadId tid)
{
	SPROG	*pSP;

	if (tid == 0)
	{
		seqShowAll();
		return NULL;
	}

	/* Find a state program that has this thread id */
	pSP = seqFindProg(tid);
	if (pSP == NULL)
	{
		printf("No state program exists for thread id %ld\n", (long)tid);
		return NULL;
	}

	return pSP;
}

static int	seqProgCount;

/* This routine is called by seqTraverseProg() for seqShowAll() */
static int seqShowSP(SPROG *pSP, void *parg)
{
	SSCB	*pSS;
	unsigned nss;
	const char *progName;
	char	threadName[THREAD_NAME_SIZE];

	if (seqProgCount++ == 0)
		printf("Program Name     Thread ID  Thread Name      SS Name\n\n");

	progName = pSP->pProgName;
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			strcpy(threadName,"(no thread)");
		else
			epicsThreadGetName(pSS->threadId, threadName,
				      sizeof(threadName));
		printf("%-16s %-10p %-16s %-16s\n", progName,
			pSS->threadId, threadName, pSS->pSSName );
		progName = "";
	}
	printf("\n");
	return FALSE;	/* continue traversal */
}

/* Print a brief summary of all state programs */
static void seqShowAll(void)
{

	seqProgCount = 0;
	seqTraverseProg(seqShowSP, 0);
	if (seqProgCount == 0)
		printf("No active state programs\n");
}

/* avoid nothing define but not used warnings */
pr_fun *qry_nothing_dummy = nothing;
