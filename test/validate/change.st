/* Test state change statement */
program changetest

%%#include <time.h>
%%#include <stdlib.h>
%%#include "epicsUnitTest.h"

#define MAX_TEST 50

entry {
    testPlan(MAX_TEST);
    /* uncomment for repeatability */
    /* srandom(12345); */
}

ss main {
    int x = 0;
    int n = 1;
    state zero {
#if 0
        /* uncomment to test rejection of state change in entry block */
        entry {
            state zero;
        }
#endif
        when (n > MAX_TEST) {
        } exit
        when (delay(0.01)) {
            testOk(x == 0, "state=zero, x=%d", x);
            x = random() % 10 - 5;
            testDiag("new random x=%d", x);
            n++;
            if (x > 0) {
                state pos;
            } else if (x < 0) {
                state neg;
            }
        } state zero
#if 0
        /* uncomment to test rejection of state change in exit block */
        exit {
            state zero;
        }
#endif
    }
    state pos {
        when (n > MAX_TEST) {
        } exit
        when (delay(0.01)) {
            testOk(x > 0, "state=pos, x=%d", x);
            n++;
            x--;
            if (x == 0) state zero;
        } state pos
    }
    state neg {
        when (n > MAX_TEST) {
        } exit
        when (delay(0.01)) {
            testOk(x < 0, "state=neg, x=%d", x);
            n++;
            x++;
            if (x == 0) state zero;
        } state neg
    }
}

exit {
    testDone();
    exit(0);
}
