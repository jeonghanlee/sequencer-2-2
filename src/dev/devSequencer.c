/*	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
*/
/* Device support to permit database access to sequencer internals
 *
 * This is experimental only. Note the following:
 *
 * 1. uses INST_IO (an unstructured string)
 *
 * 2. string is of form <seqName>.<xxx>, where <xxx> may exhibit further
 *    structure, e.g. <seqName>.nStatesets, <seqName>.<ssName>.nStates
 *
 *    <seqName>:<ssName>
 *    <seqName>:<ssName>.threadId
 *    <seqName>:<ssName>.threadIdHex
 *    <seqName>:<ssName>.timeElapsed
 *    <seqName>:<ssName>.nStates
 *    <seqName>:<ssName>.prevState
 *    <seqName>:<ssName>.nextState
 *    <seqName>:<ssName>.currentState
 *    <seqName>.nStateSets
 *    <seqName>.nAssgin
 *    <seqName>.nConnect
 *    <seqName>.nChans
 *    <seqName>.nQueues
 *    <seqName>.pQueues
 *    <seqName>.threadPriority
 *    <seqName>.varSize
 *
 * Orginal Auth: William Lupton, W. M. Keck Observatory
 * Modified by   Kukhee Kim, SLAC
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "dbDefs.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbScan.h"
#include "stringinRecord.h"
#include "epicsEvent.h"
#include "epicsExport.h"

#include "pv.h"
#include "seqCom.h"
#include "seqPvt.h"

typedef struct {
    long	number;
    DEVSUPFUN	report;
    DEVSUPFUN	init;
    DEVSUPFUN	init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN	read_or_write;
    DEVSUPFUN	special_linconv;
} DSET;

/* stringin */
LOCAL long siInit( struct stringinRecord *pRec );
LOCAL long siRead( struct stringinRecord *pRec );
LOCAL long siGetIoInitInfo(int cmd, struct stringinRecord *pRec, IOSCANPVT *ppvt);
LOCAL DSET  devSiSeq = { 5, NULL, NULL, siInit, siGetIoInitInfo, siRead };
epicsExportAddress(dset,devSiSeq);

LOCAL void devSeqScanThreadSpawn(void);
LOCAL void devSeqScanThread(void);

LOCAL epicsThreadOnceId devSeqScanThreadOnceFlag = EPICS_THREAD_ONCE_INIT;
LOCAL char*             devSeqScanThreadName     = "devSeqScan";
LOCAL ELLLIST           devSeqScanList;

LOCAL  char *nullStr    = "";

/* Commands for Second field */
LOCAL  char *nStateSets     = "nStateSets";
LOCAL  char *nAssign        = "nAssign";
LOCAL  char *nConnect       = "nConnect";
LOCAL  char *nChans         = "nChans";
LOCAL  char *nQueues        = "nQueues";
LOCAL  char *pQueues        = "pQueues";
LOCAL  char *threadPriority = "threadPriority";
LOCAL  char *varSize        = "varSize";

/* Commands for Third field */
LOCAL  char *threadId       = "threadId";
LOCAL  char *threadIdHex    = "threadIdHex";
LOCAL  char *timeElapsed    = "timeElapsed";
LOCAL  char *nStates        = "nStates";
LOCAL  char *firstState     = "firstState";
LOCAL  char *prevState      = "prevState";
LOCAL  char *nextState      = "nextState";
LOCAL  char *currentState   = "currentState";

LOCAL  char *notFoundMsg    = "Not Found";
LOCAL  char *syntaxErrMsg   = "Syntax Error in INP field";

typedef enum{
    notUpdated =0,
    updated,
    notFound
} updateFlagMenu;

typedef enum {
    seqShownStateSets,
    seqShownAssign,
    seqShownConnect,
    seqShownChans,
    seqShownQueues,
    seqShowpQueues,
    seqShowthreadPriority,
    seqShowvarSize,
    seqShowthreadId,
    seqShowthreadIdHex,
    seqShowtimeElapsed,
    seqShownStates,
    seqShowfirstState,
    seqShowprevState,
    seqShownextState,
    seqShowcurrentState,
    seqShowsyntaxErr
} seqShowVarType;

typedef union {
    long          numSS;
    long          assignCount;
    long          connectCount;
    long          numChans;
    int           numQueues;
    ELLLIST       *pQueues;
    unsigned int  threadPriority;
    long          varSize;

    epicsThreadId threadId;
    double        timeElapsed;
    long          numStates;
    const char    *pFirstStateName;
    const char    *pPrevStateName;
    const char    *pNextStateName;
    const char    *pCurrentStateName;

    char          *pSyntaxErrMsg;
} seqShowVar;

typedef struct {
    ELLNODE               devScanNode;
    IOSCANPVT             ioScanPvt;
    seqShowVarType        type;
    SPROG                 *pSP;
    SSCB                  *pSS;
    STATE                 *pST;
    char                  progName[80];
    char                  stateSetName[80];
    char                  updateFlag;
    epicsMutexId          mutexId;
    seqShowVar            var;
} seqShowScanPvt;

#define UPDATE_SEQSHOW_VAR(UPDATEFLAG, SOURCE, TARGET) \
        if((UPDATEFLAG) || ((TARGET) != (SOURCE))) {\
            (TARGET) = (SOURCE); \
            (UPDATEFLAG) = updated; \
        }
#define NEW(type) (type*)calloc(1,sizeof(type))

LOCAL seqShowScanPvt* seqShowScanPvtInit(struct link* pLink)
{
    seqShowScanPvt   *pvtPt;
    char             inpStr[strlen(pLink->value.instio.string)+1];
    char             *inpArg[3];
    int              argN  = 0;
    int              i     = 0;

    pvtPt = NEW(seqShowScanPvt);
    if(!pvtPt) return NULL;

    pvtPt->updateFlag = 1;
    pvtPt->pSP        = NULL;
    pvtPt->mutexId    = epicsMutexCreate();
    scanIoInit(&pvtPt->ioScanPvt);

    strcpy(inpStr,pLink->value.instio.string);

    /* Find deliminater in INP string
       and replace null characeter to point end of string.
       And assign to sperated strings */
    while(argN<3) {
        if(inpStr[i] == '\0') break;
        inpArg[argN++] = inpStr+i;
        while(i<80) {
            char *tempChar = inpStr + (i++);
            if((*tempChar < '0' || *tempChar > '9') &&
               (*tempChar < 'a' || *tempChar > 'z') &&
               (*tempChar < 'A' || *tempChar > 'Z') ) {
                    if(*tempChar == '\0') i-=1;
                    *tempChar = '\0';
                    break;
            }
        }
    }
    for(i=argN;i<3;i++) inpArg[i] = nullStr;

    strcpy(pvtPt->progName,inpArg[0]);
    strcpy(pvtPt->stateSetName,nullStr);
    if(!strcmp(inpArg[1],nStateSets))          pvtPt->type = seqShownStateSets;
    else if(!strcmp(inpArg[1],nAssign))        pvtPt->type = seqShownAssign;
    else if(!strcmp(inpArg[1],nConnect))       pvtPt->type = seqShownConnect;
    else if(!strcmp(inpArg[1],nChans))         pvtPt->type = seqShownChans;
    else if(!strcmp(inpArg[1],nQueues))        pvtPt->type = seqShownQueues;
    else if(!strcmp(inpArg[1],pQueues))        pvtPt->type = seqShowpQueues;
    else if(!strcmp(inpArg[1],threadPriority)) pvtPt->type = seqShowthreadPriority;
    else if(!strcmp(inpArg[1],varSize))        pvtPt->type = seqShowvarSize;
    else {
        strcpy(pvtPt->stateSetName,inpArg[1]);
        if(!strcmp(inpArg[2],threadId))          pvtPt->type = seqShowthreadId;
        else if(!strcmp(inpArg[2],threadIdHex))  pvtPt->type = seqShowthreadIdHex;
        else if(!strcmp(inpArg[2],timeElapsed))  pvtPt->type = seqShowtimeElapsed;
        else if(!strcmp(inpArg[2],nStates))      pvtPt->type = seqShownStates;
        else if(!strcmp(inpArg[2],firstState))   pvtPt->type = seqShowfirstState;
        else if(!strcmp(inpArg[2],prevState))    pvtPt->type = seqShowprevState;
        else if(!strcmp(inpArg[2],nextState))    pvtPt->type = seqShownextState;
        else if(!strcmp(inpArg[2],nullStr) || !strcmp(inpArg[2],currentState))      
                                                 pvtPt->type = seqShowcurrentState;
        else                                     pvtPt->type = seqShowsyntaxErr;
    } 
    return pvtPt;
}

LOCAL void devSeqScanThreadSpawn(void) 
{
    epicsUInt32 devSeqScanStack;

    /* Init Linked List */
    ellInit(&devSeqScanList);

    /* Spawn the Scan Task */
    devSeqScanStack = epicsThreadGetStackSize(epicsThreadStackMedium);
    epicsThreadCreate(devSeqScanThreadName, THREAD_PRIORITY, devSeqScanStack,
                      (EPICSTHREADFUNC)devSeqScanThread,NULL);
}

LOCAL void devSeqScanThread(void)
{
    ELLLIST          *pdevSeqScanList = &devSeqScanList;
    seqShowScanPvt   *pvtPt;
    seqShowVar       *varPt;
    int              i;
    double           timeNow;


    while(!pdevSeqScanList->count) {
        epicsThreadSleep(0.5);
    }

    while(TRUE) {
        pvtPt = (seqShowScanPvt*) ellFirst(pdevSeqScanList);
        do {
            pvtPt->pSP = seqFindProgByName(pvtPt->progName, 0);

            if(!pvtPt->pSP) continue;
            varPt = &(pvtPt->var);

            epicsMutexLock(pvtPt->mutexId);
            switch(pvtPt->type){
                case seqShownStateSets:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->numSS, varPt->numSS)
                    break;
                case seqShownAssign:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->assignCount, varPt->assignCount)
                    break;
                case seqShownConnect:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->connectCount, varPt->connectCount)
                    break;
                case seqShownChans:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->numChans, varPt->numChans)
                    break;
                case seqShownQueues:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->numQueues, varPt->numQueues)
                    break;
                case seqShowpQueues:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->pQueues, varPt->pQueues)
                    break;
                case seqShowthreadPriority:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->threadPriority, varPt->threadPriority)
                    break;
                case seqShowvarSize:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSP->varSize, varPt->varSize)
                    break;
                 default:
                    for(i=0, pvtPt->pSS = pvtPt->pSP->pSS; i < pvtPt->pSP->numSS; i++, (pvtPt->pSS)++) {
                        if(!strcmp(pvtPt->pSS->pSSName, pvtPt->stateSetName)) break;
                    }
                    if(i >= pvtPt->pSP->numSS) { 
                        pvtPt->updateFlag = notFound;
                        epicsMutexUnlock(pvtPt->mutexId);
                        scanIoRequest(pvtPt->ioScanPvt);
                        continue;
                    }
                    break;
        }
        switch(pvtPt->type){
                case seqShowthreadId:
                case seqShowthreadIdHex:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSS->threadId, varPt->threadId)
                    break;
                case seqShowtimeElapsed:
                    pvTimeGetCurrentDouble(&timeNow);
                    varPt->timeElapsed = timeNow - pvtPt->pSS->timeEntered;
                    pvtPt->updateFlag = 1; 
                    break;
                case seqShownStates:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pSS->numStates, varPt->numStates)
                    break;
                case seqShowfirstState:
                    pvtPt->pST = pvtPt->pSS->pStates;
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pST->pStateName, varPt->pFirstStateName)
                    break;
                case seqShowprevState:
                    pvtPt->pST = pvtPt->pSS->pStates + pvtPt->pSS->prevState;
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, 
                                       pvtPt->pSS->prevState >= 0 ? pvtPt->pST->pStateName: nullStr, 
                                       varPt->pPrevStateName)
                    break;
                case seqShownextState:
                    pvtPt->pST = pvtPt->pSS->pStates + pvtPt->pSS->nextState;
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, 
                                       pvtPt->pSS->nextState >= 0 ? pvtPt->pST->pStateName: nullStr, 
                                       varPt->pNextStateName)
                    break;
                case seqShowcurrentState:
                    pvtPt->pST = pvtPt->pSS->pStates + pvtPt->pSS->currentState;
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, pvtPt->pST->pStateName, varPt->pCurrentStateName)
                    break;
                case seqShowsyntaxErr:
                    UPDATE_SEQSHOW_VAR(pvtPt->updateFlag, syntaxErrMsg, varPt->pSyntaxErrMsg);
                    break;
                default:
                    break;
            }
            epicsMutexUnlock(pvtPt->mutexId);

            if(pvtPt->updateFlag) { 
                pvtPt->updateFlag = notUpdated;
                scanIoRequest(pvtPt->ioScanPvt);
            }

        } while( (pvtPt = (seqShowScanPvt*) ellNext(&pvtPt->devScanNode)) );
        epicsThreadSleep(0.1);
    }
}


LOCAL long siInit( struct stringinRecord *pRec )
{
    struct link      *pLink   = &pRec->inp;

    /* check that link is of type INST_IO */
    if ( pLink->type != INST_IO ) {
	return -1;
    }

    pRec->dpvt = (void *) seqShowScanPvtInit(pLink);
    if(!pRec->dpvt) return -1;

    epicsThreadOnce(&devSeqScanThreadOnceFlag, (void(*)(void *)) devSeqScanThreadSpawn, (void *) NULL);
   
    ellAdd(&devSeqScanList, &(((seqShowScanPvt *) pRec->dpvt)->devScanNode));
   
    return 0;
}

LOCAL long siRead( struct stringinRecord *pRec )
{
    seqShowScanPvt    *pvtPt = (seqShowScanPvt *)pRec->dpvt;
    seqShowVar        *varPt;

    if(!pvtPt || pvtPt->updateFlag == notFound || !pvtPt->pSP ) { 
        strcpy(pRec->val, notFoundMsg); 
        return 0; 
    }
    varPt = &(pvtPt->var);
   
    epicsMutexLock(pvtPt->mutexId);
    switch(pvtPt->type){
        case seqShownStateSets:     sprintf(pRec->val, "%ld", varPt->numSS);                      break;
        case seqShownAssign:        sprintf(pRec->val, "%ld", varPt->assignCount);                break;
        case seqShownConnect:       sprintf(pRec->val, "%ld", varPt->connectCount);               break;
        case seqShownChans:         sprintf(pRec->val, "%ld", varPt->numChans);                   break;
        case seqShownQueues:        sprintf(pRec->val, "%d", varPt->numQueues);                   break;
        case seqShowpQueues:        sprintf(pRec->val, "%p", varPt->pQueues);                     break;
        case seqShowthreadPriority: sprintf(pRec->val, "%d", varPt->threadPriority);              break;
        case seqShowvarSize:        sprintf(pRec->val, "%ld", varPt->varSize);                    break;
        case seqShowthreadId:       sprintf(pRec->val, "%lu", (unsigned long) varPt->threadId);   break;
        case seqShowthreadIdHex:    sprintf(pRec->val, "0x%lx", (unsigned long) varPt->threadId); break;
        case seqShowtimeElapsed:    sprintf(pRec->val, "%.3f", varPt->timeElapsed);               break;
        case seqShownStates:        sprintf(pRec->val, "%ld", varPt->numStates);                  break;
        case seqShowfirstState:     strcpy(pRec->val, varPt->pFirstStateName);                    break;
        case seqShowprevState:      strcpy(pRec->val, varPt->pPrevStateName);                     break;
        case seqShownextState:      strcpy(pRec->val, varPt->pNextStateName);                     break;
        case seqShowcurrentState:   strcpy(pRec->val, varPt->pCurrentStateName);                  break;
        case seqShowsyntaxErr:      strcpy(pRec->val, varPt->pSyntaxErrMsg);                      break;
    }
    epicsMutexUnlock(pvtPt->mutexId);

    return 0;
}

LOCAL long siGetIoInitInfo(int cmd, struct stringinRecord *pRec, IOSCANPVT *ppvt)
{
    seqShowScanPvt  *pvtPt = (seqShowScanPvt *)pRec->dpvt;

    *ppvt = pvtPt->ioScanPvt;

    return 0;
}

pr_fun *devSequencer_nothing_dummy = nothing;
