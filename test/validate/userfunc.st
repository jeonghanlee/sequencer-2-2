program userfuncTest

%%#include "../testSupport.h"

option +s;

%{
static void incr(SS_ID ssId, int *pv, VAR_ID v)
{
    seq_pvGet(ssId, v, SYNC);
    *pv += 1;
    seq_pvPut(ssId, v, SYNC);
}
}%

int i = 0;
assign i;
monitor i;

foreign pVar, ssId;

entry {
    testPlan(10);
}

ss myss {
    state doit {
        int i_saved;
        when (delay(0.1)) {
            incr(ssId, &i, pvIndex(i));
            testOk1(i_saved == i-1);
            i_saved = i;
        } state doit
        when (i == 10) {
        } exit
    }
}

exit {
    testDone();
    seq_test_done();
}
