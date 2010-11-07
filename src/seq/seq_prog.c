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

static epicsMutexId progListLock;
static SPROG *progList;

static void seqProgListInit(void)
{
    if (!progListLock)
        progListLock = epicsMutexMustCreate();
}

/*
 * seqFindProg() - find a program in the state program list from thread id.
 */
SPROG *seqFindProg(epicsThreadId threadId)
{
    SPROG *pSP = NULL;

    seqProgListInit();
    epicsMutexMustLock(progListLock);
    foreach(pSP, progList) {
        int n;

        for (n = 0; n < pSP->numSS; n++) {
            if (pSP->pSS[n].threadId == threadId) {
                break;
            }
        }
    }
    epicsMutexUnlock(progListLock);
    return pSP;
}

/*
 * seqFindProgByName() - find a program in the state program list by name
 * and instance number.
 */
epicsShareFunc SPROG *epicsShareAPI seqFindProgByName(char *pProgName, int instance)
{
    SPROG *pSP = NULL;

    seqProgListInit();
    epicsMutexMustLock(progListLock);
    foreach(pSP, progList) {
        if (strcmp(pSP->pProgName, pProgName) == 0 && pSP->instance == instance) {
            break;
        }
    }
    epicsMutexUnlock(progListLock);
    return pSP;
}

/*
 * seqTraverseProg() - travers programs in the state program list and
 * call the specified routine or function.  Passes one parameter of
 * pointer size.
 */
void seqTraverseProg(seqTraversee * pFunc, void *param)
{
    SPROG *pSP;

    seqProgListInit();
    epicsMutexMustLock(progListLock);
    foreach(pSP, progList) {
        pFunc(pSP, param);
    }
    epicsMutexUnlock(progListLock);
}

/*
 * seqAddProg() - add a program to the state program list.
 * Precondition: must not be already in the list.
 */
void seqAddProg(SPROG *pSP)
{
    SPROG *pCurSP, *pLastSP = NULL;
    int instance = -1;

    seqProgListInit();
    epicsMutexMustLock(progListLock);
    foreach(pCurSP, progList) {
        pLastSP = pCurSP;
        /* check precondition */
        assert(pCurSP != pSP);
        if (strcmp(pCurSP->pProgName, pSP->pProgName) == 0) {
            instance = max(pCurSP->instance, instance);
        }
    }
    pSP->instance = instance + 1;
    if (pLastSP != NULL) {
        pLastSP->next = pSP;
    } else {
        progList = pSP;
    }
    epicsMutexUnlock(progListLock);
    DEBUG("Added program %p, instance %d to list.\n", pSP, pSP->instance);
}

/* 
 * seqDelProg() - delete a program from the program list.
 * Returns TRUE if deleted, else FALSE.
 */
void seqDelProg(SPROG *pSP)
{
    SPROG *pCurSP;

    seqProgListInit();
    epicsMutexMustLock(progListLock);
    foreach(pCurSP, progList) {
        if (pCurSP->next == pSP) {
            pCurSP->next = pSP->next;
        }
    }
    epicsMutexUnlock(progListLock);
}
