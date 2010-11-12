program pvPutAsync

%%#include <stdlib.h>

int x;
assign x to "pvPutAsync1";

ss test {
    state loop {
        int i = 0;
        when (i == 4) {
        } state stop
        when (delay(0.2)) {
            int status;
            x = 1;
            status = pvPut(x,ASYNC);
            if (status)
                printf("pvPut failed: %d\n",status);
            else
                printf("pvPut ok\n");
            ++i;
        } state loop
    }
    state stop {
        when (delay(10)) {
            exit(0);
        } state stop
    }
}
