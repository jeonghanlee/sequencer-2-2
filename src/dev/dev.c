/* $Id: dev.c,v 1.1.1.1 2000-04-04 03:22:41 wlupton Exp $
 *
 * Device support to permit database access to sequencer internals
 *
 * This is experimental only. Note the following:
 *
 * 1. uses INST_IO (an unstructured string)
 *
 * 2. string is of form <seqName>.<xxx>, where <xxx> may exhibit further
 *    structure, e.g. <seqName>.nStatesets, <seqName>.<ssName>.nStates,
 *    <seqName>.ss[0].name etc.
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <string.h>

#include "seq.h"

#include "alarm.h"
#include "dbDefs.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbScan.h"

#include "stringinRecord.h"

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
DSET  devSiSeq = { 5, NULL, NULL, siInit, NULL, siRead };

LOCAL long siInit( struct stringinRecord *pRec )
{
    struct link *pLink = &pRec->inp;

    /* check that link is of type INST_IO */
    if ( pLink->type != INST_IO ) {
	return ERROR;
    }

    return OK;
}

LOCAL long siRead( struct stringinRecord *pRec )
{
    struct link *pLink = &pRec->inp;
    struct instio *pInstio = ( struct instio * ) &pLink->value.instio;
    SPROG *prog;
    char name[80];
    int i;

    /* parse the string as a sequencer name and an integer (n) */
    if ( sscanf( pInstio->string, "%s %d", name, &i ) != 2 ) {
	return ERROR;
    }

    /* return the name of this sequencer's nth state set */
    prog = seqFindProgByName( name );
    if ( prog == NULL ) {
	return ERROR;
    }

    if ( i < 0 || i >= prog->numSS ) {
	return ERROR;
    }

    strcpy( pRec->val, prog->pSS[i].pSSName );

    return OK;
}

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1  2000/03/29 01:57:50  wlupton
 * initial insertion
 *
 */

