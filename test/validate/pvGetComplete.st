program pvGetComplete

%%#include "stdlib.h"
%%#include "epicsThread.h"
%%#include "epicsUnitTest.h"

option +s;

/* use literal C code to circumvent safe mode */
%%int shared = 1;

foreign shared;

entry {
    testPlan(60);
}

ss read {
    int test;
    connect test to "pvGetComplete";
    int lastPut;
    int n = 0;

    state get {
        when (n >= 20) {
        } exit
        when (delay(0.2)) {
            /* printf("pvGet\n"); */
            lastPut = shared;
            pvGet(test, ASYNC);
            if (pvGetComplete(test)) {
                testOk(lastPut==test, "immediate completion:   lastPut=%d==%d=test", lastPut, test);
            } else {
                testSkip(1, "no immediate completion");
            }
            epicsThreadSleep(0.1);
            if (pvGetComplete(test)) {
                testOk(lastPut==test, "completion after delay: lastPut=%d==%d=test", lastPut, test);
            } else {
                testSkip(1, "no immediate completion");
            }
            n++;
        } state wait
    }
    state wait {
        when (pvGetComplete(test)) {
            testOk(lastPut==test, "completion after sync:  lastPut=%d==%d=test", lastPut, test);
        } state get
    }
}

ss write {
    int out = 0;
    connect out to "pvGetComplete";

    state run {
        when (delay(0.07)) {
            shared = out;
            pvPut(out);
            out++;
        } state run
    }
}

exit {
    testDone();
    exit(1);
}
