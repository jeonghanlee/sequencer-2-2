/* Test state change statement */
program changetest

%%#include <errlog.h>
%%#include <time.h>

entry {
    /* uncomment to get repeatability */
    /* srandom(12345); */
}

ss main {
    int x = 0;
    state zero {
#if 0
        /* uncomment to test rejection of state change in entry block */
        entry {
            state zero;
        }
#endif
        when (delay(1)) {
        	x = time(0) %10 - 5;
            errlogPrintf("state=zero, new random x=%d\n", x);
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
        when (delay(1)) {
            errlogPrintf("state=pos, x=%d\n", x);
        	if (x == 0) state zero;
            x--;
        } state pos
    }
    state neg {
        when (delay(1)) {
            errlogPrintf("state=neg, x=%d\n", x);
        	if (x == 0) state zero;
            x++;
        } state neg
    }
}
