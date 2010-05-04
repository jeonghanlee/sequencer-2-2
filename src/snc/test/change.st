/* Test state change statement */
program changetest

%%#include <time.h>

entry {
    /* uncomment to get repeatability */
    /* srandom(12345); */
}

ss main {
    int x = 0;
    state zero {
        when (delay(1)) {
        	x = time(0) %10 - 5;
            seqLog("state=zero, new random x=%d\n", x);
            if (x > 0) {
                state pos;
            } else if (x < 0) {
                state neg;
            }
        } state zero
    }
    state pos {
        when (delay(1)) {
            seqLog("state=pos, x=%d\n", x);
        	if (x == 0) state zero;
            x--;
        } state pos
    }
    state neg {
        when (delay(1)) {
            seqLog("state=neg, x=%d\n", x);
        	if (x == 0) state zero;
            x++;
        } state neg
    }
}
