/*************************************************************************\
Copyright (c) 2010-2011 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include <stdlib.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "seqCom.h"

#include "../testSupport.h"

static epicsEventId this_test_done;

void run_seq_test(seqProgram *seqProg)
{
    if (!this_test_done) {
        this_test_done = epicsEventMustCreate(epicsEventEmpty);
    } else {
        epicsEventWait(this_test_done);
    }
    epicsThreadSleep(2.0);
    seq(seqProg,0,0);
}

void seq_test_done(void)
{
#if defined(vxWorks)
    epicsEventSignal(this_test_done);
#else
    exit(0);
#endif
}
