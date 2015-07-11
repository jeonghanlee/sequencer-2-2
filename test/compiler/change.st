/*************************************************************************\
Copyright (c) 2010-2015 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program changeTest

ss main {
    int x = 0;
    state zero {
        entry {
            state zero;
        }
        when (n > MAX_TEST) {
        } exit
        when (delay(0.01)) {
            if (x > 0) {
                state pos;
            } else if (x < 0) {
                state neg;
            }
        } state zero
        exit {
            state zero;
        }
    }
    state pos {
        when (delay(0.01)) {
            x--;
            if (x == 0) state zero;
        } state pos
    }
    state neg {
        when (delay(0.01)) {
            x++;
            if (x == 0) state zero;
        } state neg
    }
}
