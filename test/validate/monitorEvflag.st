program monitorEvflagTest

option +s;

%%#include "../testSupport.h"
%%#include <stdio.h>

#define NTESTS 100
#define NCYCLESPERTEST 1000
#define NCYCLES (NTESTS*NCYCLESPERTEST)

int requested;
assign requested to "requested";

int actual;
assign actual to "actual";
monitor actual;

evflag actualChanged;
sync actual actualChanged;

entry {
    seq_test_init(NTESTS);
}

ss monitorEvflagTest {
    int cycleCount = 1;
    int error = 0;
    state init {
        when (delay(1.0)) {
            efClear(actualChanged);
        } state makeRequest
    }
    state makeRequest {
        when (error || cycleCount > NCYCLES) {
        } exit
        when (cycleCount <= NCYCLES) {
            requested = cycleCount;
            pvPut(requested);
        } state waitForActualToEqualRequested
    }
    state waitForActualToEqualRequested {
        when (efTestAndClear(actualChanged) /* && actual == requested */) {
            if (actual != requested) {
                testFail("requested(%d)!=actual(%d)", requested, actual);
                testSkip((NCYCLES-cycleCount)/NCYCLESPERTEST, "failure");
                error = TRUE;
            } else if (cycleCount % NCYCLESPERTEST == 0) {
                testPass("requested(%d)==actual(%d)", requested, actual);
            }
            cycleCount++;
        } state makeRequest
        when (delay(1.0)) {
            testFail("timeout in cycle %d/%d (requested=%d, actual=%d)",
                cycleCount + 1, NCYCLES, requested, actual);
            testSkip((NCYCLES-cycleCount)/NCYCLESPERTEST, "failure");
        } exit
    }
}

exit {
    seq_test_done();
}
