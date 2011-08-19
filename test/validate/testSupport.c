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

static seqProgram *prog;

static int doit(void)
{
    seq(prog,0,0);
    return 0;
}

void run_seq_test(seqProgram *seqProg)
{
    if (!this_test_done) {
        this_test_done = epicsEventMustCreate(epicsEventEmpty);
    }
    prog = seqProg;
    runTestFunc(seqProg->progName, doit);
    epicsEventWait(this_test_done);
}

void seq_test_done(void)
{
#if defined(vxWorks)
    epicsEventSignal(this_test_done);
#else
    exit(0);
#endif
}
