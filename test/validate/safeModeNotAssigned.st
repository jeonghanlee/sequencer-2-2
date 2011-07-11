program safeModeNotAssignedTest

%%#include "../testSupport.h"

option +s;

int v = 0;

#define MAX_TEST 10

entry {
    testPlan(MAX_TEST*2);
}

ss ss1 {
    int i = 0;
    state idle {
        when (delay(0.1)) {
            testOk(v == i, "ss1: v=%d==%d=i", v, i);
            v++;
            i++;
        } state idle
        when (v==MAX_TEST) {
        } state done
    }
    state done {
        when (delay(0.2)) {
        } exit
    }
}

ss ss2 {
    state idle {
        int i;
        when (delay(0.1)) {
            if (i < MAX_TEST) {
                testOk(v == 0, "ss2: v=%d==0", v);
            }
            i++;
        } state idle
    }
}

exit {
    testDone();
    seq_test_done();
}
