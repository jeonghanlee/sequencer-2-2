/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvGetCancelTest

/* option +s; */

%%#include "../testSupport.h"

entry {
    seq_test_init(8);
    testDiag("start");
}

ss pvGetCancel {
    int x[2];
    int y[2] = {1,2};
    assign x to { "pvGetCancel1", "pvGetCancel2"};
    assign y to { "pvGetCancel1", "pvGetCancel2"};
    typename pvStat status;

    state test {
        entry {
            testOk1(pvGet(x[0],ASYNC) == pvStatOK);
            testOk1(pvGet(x[1],ASYNC) == pvStatOK);
            pvGetCancel(x);
            testOk1(pvPut(y[0],SYNC) == pvStatOK);
            testOk1(pvPut(y[1],SYNC) == pvStatOK);
            testOk1(pvGet(x[0],ASYNC) == pvStatOK);
            testOk1(pvGet(x[1],ASYNC) == pvStatOK);
        }
        when (delay(5.0)) {
            testFail("pvGet completion timeout");
            testSkip(1,"further tests aborted");
        } exit
        when (pvGetComplete(x)) {
            testOk1(x[0] == y[0]);
            testOk1(x[1] == y[1]);
        } state done
    }
    state done {
        when (delay(0.2)) {
        } exit
    }
}

exit {
    testDiag("exit");
    seq_test_done();
}
