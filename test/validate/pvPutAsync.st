program pvPutAsyncTest

%%#include "../testSupport.h"

int x;
assign x to "pvPutAsync1";

entry {
    testPlan(10);
}

ss test1 {
    state put_async {
        int i;
        entry {
            i = 0;
        }
        when (i == 5) {
        } state put_sync
        when (delay(0.4)) {
            int status;
            x = 1;
            status = pvPut(x,ASYNC);
            /* first ok, next three failure */
            testOk((i==0)==(status==pvStatOK), "pvPut/ASYNC %d: result=%d (%s)",
                i, status, status ? pvMessage(x) : "");
/*
 *             if (status)
 *                 printf("test1: pvPut/ASYNC %d failed: %s\n", i, pvMessage(x));
 *             else
 *                 printf("test1: pvPut/ASYNC %d issued\n", i);
 */
            ++i;
        } state put_async
    }
    state put_sync {
        when (pvPutComplete(x)) {
            int i = 0;
            testPass("pvPut/ASYNC complete\n");
            for (i=0; i<4; i++) {
                int status = pvPut(x,SYNC);
                /* should always succeed */
                testOk(status==pvStatOK, "pvPut/SYNC %d, status=%d (%s)",
                    i, status, status ? pvMessage(x) : "");
#if 0
                if (status) {
                    printf("test1: pvPut/SYNC failed: %s\n", pvMessage(x));
                } else {
                    printf("test1: pvPut/SYNC ok\n");
                }
#endif
            }
        } state wait
    }
    state wait {
        when (delay(1)) {
        } exit
    }
}

#if 0
ss test2 {
    int x;
    assign x to "pvPutAsync2";
    state wait {
        when (delay(0.5)) {
        } state put_async
    }
    state put_async {
        when (pvPutComplete(x)) {
            int status;
            printf("test2: pvPut/ASYNC complete\n");
            x = 1;
            status = pvPut(x,ASYNC);
            /* should always succeed */
            if (status)
                printf("test2: pvPut/ASYNC 1 failed, %s\n", pvMessage(x));
            else
                printf("test2: pvPut/ASYNC 1 issued\n");
            /* should always fail */
            status = pvPut(x,ASYNC);
            if (status)
                printf("test2: pvPut/ASYNC 2 failed, %s\n", pvMessage(x));
            else
                printf("test2: pvPut/ASYNC 2 issued\n");
        } state wait
    }
}
#endif

exit {
    testDone();
    seq_test_done();
}
