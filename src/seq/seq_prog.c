/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1991, The Regents of the University of California.
		         Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)

	State program list management functions.
	All active state programs are inserted into the list when created
	and removed from the list when deleted.
***************************************************************************/
#define DEBUG  errlogPrintf             /* nothing, printf, errlogPrintf etc. */

#include <string.h>
#include "seq.h"

/*
 * seqFindProg() - find a program in the state program list from thread id.
 */
SPROG *seqFindProg(epicsThreadId threadId)
{
    SSCB *pSS = seqFindStateSet(threadId);
    return pSS ? pSS->sprog : NULL;
}

struct findStateSetArgs {
    SSCB *pSS;
    epicsThreadId threadId;
};

static int findStateSet(SPROG *pSP, void *param)
{
    struct findStateSetArgs *pargs = (struct findStateSetArgs *)param;
    int n;

    for (n = 0; n < pSP->numSS; n++) {
        SSCB *pSS = pSP->pSS + n;

        DEBUG("findStateSet trying %s[%d] pSS[%d].threadId=%p\n",
            pSP->pProgName, pSP->instance, n, pSS->threadId);
        if (pSS->threadId == pargs->threadId) {
            pargs->pSS = pSS;
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * seqFindStateSet() - find a state set in the state program list from thread id.
 */
SSCB *seqFindStateSet(epicsThreadId threadId)
{
    struct findStateSetArgs args;

    args.pSS = 0;
    args.threadId = threadId;
    seqTraverseProg(findStateSet, &args);
    return args.pSS;
}

struct findByNameArgs {
    SPROG *pSP;
    char *pProgName;
    int instance;
};

static int findByName(SPROG *pSP, void *param)
{
    struct findByNameArgs *pargs = (struct findByNameArgs *)param;
    int found = strcmp(pSP->pProgName, pargs->pProgName) == 0 && pSP->instance == pargs->instance;
    if (found)
        pargs->pSP = pSP;
    return found;
}

/*
 * seqFindProgByName() - find a program in the program instance list by name
 * and instance number.
 */
epicsShareFunc SPROG *epicsShareAPI seqFindProgByName(char *pProgName, int instance)
{
    struct findByNameArgs args;

    args.pSP = 0;
    args.pProgName = pProgName;
    args.instance = instance;
    seqTraverseProg(findByName, &args);
    return args.pSP;
}

struct traverseInstancesArgs {
    seqTraversee *pFunc;
    void *param;
};

static int traverseInstances(SPROG **ppInstances, struct seqProgram *pseq, void *param)
{
    struct traverseInstancesArgs *pargs = (struct traverseInstancesArgs *)param;
    SPROG *pSP;

    foreach(pSP, *ppInstances) {
        if (pargs->pFunc(pSP, pargs->param))
            return TRUE;    /* terminate traversal */
    }
    return FALSE;           /* continue traversal */
}

/*
 * seqTraverseProg() - visit all existing program instances and
 * call the specified routine or function.  Passes one parameter of
 * pointer size.
 */
void seqTraverseProg(seqTraversee * pFunc, void *param)
{
    struct traverseInstancesArgs args;
    args.pFunc = pFunc;
    args.param = param;
    traverseSequencerPrograms(traverseInstances, &args);
}

static int addProg(SPROG **ppInstances, struct seqProgram *pseq, void *param)
{
    SPROG *pSP = (SPROG *)param;

    assert(ppInstances);
    if (strcmp(pSP->pProgName, pseq->pProgName) == 0) {
        SPROG *pCurSP, *pLastSP = NULL;
        int instance = -1;

        foreach(pCurSP, *ppInstances) {
            pLastSP = pCurSP;
            /* check precondition */
            assert(pCurSP != pSP);
            instance = max(pCurSP->instance, instance);
        }
        pSP->instance = instance + 1;
        if (pLastSP != NULL) {
            pLastSP->next = pSP;
        } else {
            *ppInstances = pSP;
        }
        DEBUG("Added program %p, instance %d to instance list.\n", pSP, pSP->instance);
        return TRUE;
    }
    return FALSE;
}

/*
 * seqAddProg() - add a program to the program instance list.
 * Precondition: must not be already in the list.
 */
void seqAddProg(SPROG *pSP)
{
    traverseSequencerPrograms(addProg, pSP);
}

static int delProg(SPROG **ppInstances, struct seqProgram *pseq, void *param)
{
    SPROG *pSP = (SPROG *)param;

    assert(ppInstances);
    if (strcmp(pSP->pProgName, pseq->pProgName) == 0) {
        SPROG *pCurSP;

        if (*ppInstances == pSP) {
            *ppInstances = pSP->next;
            DEBUG("Deleted program %p, instance %d from instance list.\n", pSP, pSP->instance);
            return TRUE;
        }
        foreach(pCurSP, *ppInstances) {
            if (pCurSP->next == pSP) {
                pCurSP->next = pSP->next;
                DEBUG("Deleted program %p, instance %d from instance list.\n", pSP, pSP->instance);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 * seqDelProg() - delete a program from the program instance list.
 */
void seqDelProg(SPROG *pSP)
{
    traverseSequencerPrograms(delProg, pSP);
}
