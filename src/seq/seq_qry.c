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
static void printValue(void *val, unsigned count, int type);
static SPROG *seqQryFind(epicsThreadId tid);
static void seqShowAll(void);

/*
 * seqShow() - Query the sequencer for state information.
 * If a non-zero thread id is specified then print the information about
 * the state program, otherwise print a brief summary of all state programs
 */
epicsShareFunc void epicsShareAPI seqShow(epicsThreadId tid)
{
	SPROG	*sp;
	SSCB	*ss;
	STATE	*st;
	unsigned nss;
	double	timeNow, timeElapsed;

	sp = seqQryFind(tid);
	if (sp == NULL)
		return;

	/* Print info about state program */
	printf("State Program: \"%s\"\n", sp->progName);
	printf("  initial thread id = %p\n", sp->threadId);
	printf("  thread priority = %d\n", sp->threadPriority);
	printf("  number of state sets = %u\n", sp->numSS);
	printf("  number of syncQ queues = %u\n", sp->numQueues);
	if (sp->numQueues > 0)
		printf("  queue array address = %p\n",sp->queues);
	printf("  number of channels = %u\n", sp->numChans);
	printf("  number of channels assigned = %u\n", sp->assignCount);
	printf("  number of channels connected = %u\n", sp->connectCount);
	printf("  options: async=%d, debug=%d, newef=%d, reent=%d, conn=%d, "
		"main=%d\n",
	 ((sp->options & OPT_ASYNC) != 0), ((sp->options & OPT_DEBUG) != 0),
	 ((sp->options & OPT_NEWEF) != 0), ((sp->options & OPT_REENT) != 0),
	 ((sp->options & OPT_CONN)  != 0), ((sp->options & OPT_MAIN)  != 0));
	if ((sp->options & OPT_REENT) != 0)
		printf("  user variables: address = %p, length = %u "
			"= 0x%x bytes\n",
			sp->var, sp->varSize, sp->varSize);
	printf("\n");

	/* Print state set info */
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		unsigned n;

		printf("  State Set: \"%s\"\n", ss->ssName);

		if (ss->threadId != (epicsThreadId)0)
		{
			char threadName[THREAD_NAME_SIZE];
			epicsThreadGetName(ss->threadId, threadName,sizeof(threadName));
			printf("  thread name = %s;", threadName);
		}

		printf("  thread id = %p\n", ss->threadId);

		st = ss->states;
		printf("  First state = \"%s\"\n", st->stateName);

		st = ss->states + ss->currentState;
		printf("  Current state = \"%s\"\n", st->stateName);

		st = ss->states + ss->prevState;
		printf("  Previous state = \"%s\"\n", ss->prevState >= 0 ?
			st->stateName : "");

		pvTimeGetCurrentDouble(&timeNow);
		timeElapsed = timeNow - ss->timeEntered;
		printf("  Elapsed time since state was entered = %.1f "
			"seconds\n", timeElapsed);
		printf("  Queued time delays:\n");
		for (n = 0; n < ss->numDelays; n++)
		{
			printf("\tdelay[%2d]=%f", n, ss->delay[n]);
			if (ss->delayExpired[n])
				printf(" - expired");
			printf("\n");
		}
		printf("\n");
	}
}
/*
 * seqChanShow() - Show channel information for a state program.
 */
epicsShareFunc void epicsShareAPI seqChanShow(epicsThreadId tid, const char *str)
{
	SPROG	*sp;
	CHAN	*ch;
	int	nch;
        int	n;
	char	tsBfr[50], connQual;
	int	match, showAll;

	sp = seqQryFind(tid);
	if(!sp) return;

	printf("State Program: \"%s\"\n", sp->progName);
	printf("Number of channels=%u\n", sp->numChans);

	if (str != NULL)
	{
		connQual = str[0];
		/* Check for channel connect qualifier */
		if ((connQual == '+') || (connQual == '-'))
		{
			str += 1;
		}
	}
	else
		connQual = 0;

	ch = sp->chan;
	for (nch = 0; (unsigned)nch < sp->numChans; )
	{
		if (str != NULL)
		{
			/* Check for channel connect qualifier */
			if (connQual == '+')
				showAll = ch->connected;
			else if (connQual == '-')
				showAll = ch->assigned && (!ch->connected);
			else
				showAll = TRUE;

			/* Check for pattern match if specified */
			match = (str[0] == 0) ||
					(strstr(ch->dbName, str) != NULL);
			if (!(match && showAll))
			{
				ch += 1;
				nch += 1;
				continue; /* skip this channel */
			}
		}
		printf("\n#%d of %u:\n", nch+1, sp->numChans);
		printf("Channel name: \"%s\"\n", ch->dbName);
		printf("  Unexpanded (assigned) name: \"%s\"\n", ch->dbAsName);
		printf("  Variable name: \"%s\"\n", ch->varName);
		printf("    offset = %d\n", ch->offset);
		printf("    type = %s\n", ch->type->typeStr);
		printf("    count = %u\n", ch->count);
		printValue(bufPtr(ch)+ch->offset, ch->count, ch->type->putType);

		printf("  Monitor flag = %d\n", ch->monFlag);
		if (ch->monitored)
			printf("    Monitored\n");
		else
			printf("    Not monitored\n");

		if (ch->assigned)
			printf("  Assigned\n");
		else
			printf("  Not assigned\n");

		if(ch->connected)
			printf("  Connected\n");
		else
			printf("  Not connected\n");

		if(ch->getComplete)
			printf("  Last get completed\n");
		else
			printf("  Get not completed or no get issued\n");

		if(epicsEventTryWait(ch->putSemId))
			printf("  Last put completed\n");
		else
			printf("  Put not completed or no put issued\n");

		printf("  Status = %d\n", ch->status);
		printf("  Severity = %d\n", ch->severity);
		printf("  Message = %s\n", ch->message != NULL ?
			ch->message : "");

		/* Print time stamp in text format: "yyyy/mm/dd hh:mm:ss.sss" */
		epicsTimeToStrftime(tsBfr, sizeof(tsBfr),
			"%Y/%m/%d %H:%M:%S.%03f", &ch->timeStamp);
		printf("  Time stamp = %s\n", tsBfr);

		n = wait_rtn();
		if (n == 0)
			return;
		nch += n;
		if (nch < 0)
			nch = 0;
		ch = sp->chan + nch;
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

static int seqcarCollect(SPROG *sp, void *param)
{
	struct seqStats *pstats = (struct seqStats *) param;
	CHAN	*ch = sp->chan;
	unsigned nch;
	int	level = pstats->level;
	int 	printedProgName = 0;
	pstats->nProgs++;
	for (nch = 0; nch < sp->numChans; nch++)
	{
		if (ch->assigned) pstats->nChans++;
		if (ch->connected) pstats->nConn++;
		if (level > 1 ||
		    (level == 1 && !ch->connected))
		    {
			if (!printedProgName)
			{
				printf("  Program \"%s\"\n", sp->progName);
				printedProgName = 1;
			}
			printf("    Variable \"%s\" %sconnected to PV \"%s\"\n",
				ch->varName,
				ch->connected ? "" : "not ",
				ch->dbName);
		}
		ch++;
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
	SPROG	*sp;
	ELLLIST	*queue;
	int	nque, n;
	char	tsBfr[50];

	sp = seqQryFind(tid);
	if(!sp) return;

	printf("State Program: \"%s\"\n", sp->progName);
	printf("Number of queues = %d\n", sp->numQueues);

	queue = sp->queues;
	for (nque = 0; (unsigned)nque < sp->numQueues; )
	{
		QENTRY	*entry;
		int i;

		printf("\nQueue #%d of %d:\n", nque+1, sp->numQueues);
		printf("Number of entries = %d\n", ellCount(queue));
		for (entry = (QENTRY *) ellFirst(queue), i = 1;
		     entry != NULL;
		     entry = (QENTRY *) ellNext(&entry->node), i++)
		{
			CHAN	*ch = entry->ch;
			pvValue	*access = &entry->value;
			void	*val = pv_value_ptr(access, ch->type->getType);

			printf("\nEntry #%d: channel name: \"%s\"\n",
							    i, ch->dbName);
			printf("  Variable name: \"%s\"\n", ch->varName);
			printValue(val, 1, ch->type->putType);
							/* was ch->count */
			printf("  Status = %d\n",
					access->timeStringVal.status);
			printf("  Severity = %d\n",
					access->timeStringVal.severity);

			/* Print time stamp in text format:
			   "yyyy/mm/dd hh:mm:ss.sss" */
			epicsTimeToStrftime(tsBfr, sizeof(tsBfr), "%Y/%m/%d "
				"%H:%M:%S.%03f", &access->timeStringVal.stamp);
			printf("  Time stamp = %s\n", tsBfr);
		}

		n = wait_rtn();
		if (n == 0)
			return;
		nque += n;
		if (nque < 0)
			nque = 0;
		queue = sp->queues + nque;
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
static void printValue(void *val, unsigned count, int type)
{
	char	*c = (char *)val;
	short	*s = (short *)val;
	long	*l = (long *)val;
	float	*f = (float *)val;
	double	*d = (double *)val;
	typedef char string[MAX_STRING_SIZE];
	string	*t = (string *)val;

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
	SPROG	*sp;

	if (tid == 0)
	{
		seqShowAll();
		return NULL;
	}

	/* Find a state program that has this thread id */
	sp = seqFindProg(tid);
	if (sp == NULL)
	{
		printf("No state program exists for thread id %ld\n", (long)tid);
		return NULL;
	}

	return sp;
}

static int	seqProgCount;

/* This routine is called by seqTraverseProg() for seqShowAll() */
static int seqShowSP(SPROG *sp, void *parg)
{
	SSCB	*ss;
	unsigned nss;
	const char *progName;
	char	threadName[THREAD_NAME_SIZE];

	if (seqProgCount++ == 0)
		printf("Program Name     Thread ID  Thread Name      SS Name\n\n");

	progName = sp->progName;
	for (nss = 0, ss = sp->ss; nss < sp->numSS; nss++, ss++)
	{
		if (ss->threadId == 0)
			strcpy(threadName,"(no thread)");
		else
			epicsThreadGetName(ss->threadId, threadName,
				      sizeof(threadName));
		printf("%-16s %-10p %-16s %-16s\n", progName,
			ss->threadId, threadName, ss->ssName );
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
