/*************************************************************************\
Copyright (c) 2010-2011 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program reassignTest

%%#include "../testSupport.h"

int x, y;
assign x to "pv1";
assign y to "";

#define NTESTS 1

entry {
    testPlan(NTESTS);
}

ss reassign {
    state test {
        when () {
            pvAssign(x,"");
            pvAssign(x,"pv1");
            pvAssign(x,"pv2");
            pvAssign(y,"");
            pvAssign(y,"pv1");
            seqShow(epicsThreadGetIdSelf());
        } state done
    }
    state done {
        when(delay(1)) {
            testPass("ok");
        } exit
    }
}

exit {
    testDone();
    seq_test_done();
}
