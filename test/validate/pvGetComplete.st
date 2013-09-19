/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvGetCompleteTest

%%#include "../testSupport.h"

#define NRUNS 50000

entry {
    seq_test_init(2*NRUNS);
    testDiag("start");
}

ss pvGetComplete {
    int r[2];
    int w[2];
    assign r to {"pvGetComplete1","pvGetComplete2"};
    assign w to {"pvGetComplete1","pvGetComplete2"};
    int n = 0;

    state count {
        when (n < NRUNS) {
            n++;
        } state test
        when () {
        } exit
    }
    state test {
        entry {
            int i;
            w[0] = n;
            w[1] = -n;
            for (i=0; i<2; i++) {
                r[i] = 0;
                pvPut(w[i],SYNC);
            }
            for (i=0; i<2; i++) {
                pvGet(r[i],ASYNC);
            }
        }
        when (delay(2)) {
            testSkip(2,"completion timeout");
        } state count
        when (pvGetComplete(r[0]) && pvGetComplete(r[1])) {
            int i;
            for (i=0; i<2; i++) {
                testOk(r[i]==w[i],"%d==%d",r[i],w[i]);
            }
        } state count
    }
}

exit {
    testDiag("exit");
    seq_test_done();
}
