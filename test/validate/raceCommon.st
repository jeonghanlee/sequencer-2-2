%%#include <stdlib.h>
%%#include "epicsUnitTest.h"

%%#include <stdio.h>
%%#include <assert.h>
%%#include <sched.h>

string x;
assign x to "x";
monitor x;

string y;
assign y to "";

evflag efx;
sync x to efx;

foreign MAX_STRING_SIZE;

entry {
    testPlan(0);
    testDiag("This test will run for 30 seconds...");
}

ss race1 {
    state init {
        when () {
            x[0] = 0;
        } state wait
    }
    state wait {
        string z;
        assign z to "";

        when (delay(0.001)) {
            int j;
            for (j=0; j <1000; j++) {
                int i;
                for (i=0; i<MAX_STRING_SIZE; i++) {
                    /* sched_yield(); */
                    if (!x[i]) break;
                    if (x[i] != x[0]) {
                        /* this should never be executed */
                        testFail("x[0]=%c!=%c=x[%d]",x[0],x[i],i);
                        break;
                    }
                }
            }
        } state wait
        when ( (x[0] && x[1] && x[0] != x[1])
            || (y[0] && y[1] && y[0] != y[1])
            || (z[0] && z[1] && z[0] != z[1]) ) {
            /* this should never be executed */
            testFail("%c/%c",x[0],x[1]);
        } state wait
    }
}

ss race2 {
    state init {
        when () {
            x[0] = 0;
        } state wait
    }
    state wait {
        when (delay(0.001)) {
            int j;
            for (j=0; j <1000; j++) {
                int i;
                for (i=0; i<MAX_STRING_SIZE; i++) {
                    /* sched_yield(); */
                    if (!x[i]) break;
                    if (x[i] != x[0]) {
                        /* this should never be executed */
                        testFail("x[0]=%c!=%c=x[%d]",x[0],x[i],i);
                        break;
                    }
                }
            }
        } state wait
        when (delay(0.002)) {
            int j;
            for (j=0; j <1000; j++) {
                int i;
                for (i=0; i<MAX_STRING_SIZE; i++) {
                    /* sched_yield(); */
                    if (!x[i]) break;
                    if (x[i] != x[0]) {
                        /* this should never be executed */
                        testFail("x[0]=%c!=%c=x[%d]",x[0],x[i],i);
                        break;
                    }
                }
            }
        } state wait
        when (x[0] && x[1] && x[0] != x[1]) {
            /* this should never be executed */
            testFail("x[0]=%c!=%c=x[1]",x[0],x[1]);
        } state wait
    }
}

ss race3 {
    state init {
        when () {
            x[0] = 0;
        } state wait
    }
    state wait {
        string z;
        assign z to "";

        when (delay(0.001)) {
            int j;
            for (j=0; j <1000; j++) {
                int i;
                for (i=0; i<MAX_STRING_SIZE; i++) {
                    /* sched_yield(); */
                    if (!x[i]) break;
                    if (x[i] != x[0]) {
                        /* this should never be executed */
                        testFail("x[0]=%c!=%c=x[%d]",x[0],x[i],i);
                        break;
                    }
                }
            }
        } state wait
        when (x[0] && x[1] && x[0] != x[1]) {
            /* this should never be executed */
            testFail("x[0]=%c!=%c=x[1]",x[0],x[1]);
        } state wait
    }
}

ss stop {
    state idle {
        when (delay(30)) {
        } exit
    }
}

exit {
    testDone();
    exit(0);
}
