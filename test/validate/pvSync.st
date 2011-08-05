/*************************************************************************\
Copyright (c) 2010-2011 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvSyncTest

%%#include "../testSupport.h"

option +s;

int x = 0;
assign x;
monitor x;

evflag f;

entry {
    testPlan(5);
    pvSync(x,f);
}

ss pvSync {
    state idle {
        int y = 0;
        when (efTestAndClear(f)) {
            y++;
            printf("test: x=%d\n",x);
            testOk(x==y, "test: x=%d==%d=y", x, y);
            if (y==5)
                state done;
        } state idle
    }
    state done {
        when () {} exit
    }
}

ss help {
    state one {
        when (delay(0.1)) {
            testDiag("set: x=%d",x);
            x++;
            pvPut(x);
        } state one
    }
}

exit {
    testDone();
    seq_test_done();
}
