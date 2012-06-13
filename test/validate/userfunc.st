/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program userfuncTest

%%#include "../testSupport.h"

option +s;

%{
static void incr(SS_ID ssId, int *pv, VAR_ID v);
static void set_i(SEQ_VARS *const pVar, int value);
}%

int i = 0;
assign i;
monitor i;

entry {
    seq_test_init(20);
}

ss myss {
    state doit {
        int i_saved;
        when (delay(0.1)) {
            incr(ssId(), &i, pvIndex(i));
            testOk1(i_saved == i-1);
            i_saved = i;
            set_i(pVar(), 1);
            testOk1(i == 1);
        } state doit
        when (i == 10) {
        } exit
    }
}

exit {
    seq_test_done();
}

%{
static void incr(SS_ID ssId, int *pv, VAR_ID v)
{
    seq_pvGet(ssId, v, SYNC);
    *pv += 1;
    seq_pvPut(ssId, v, SYNC);
}

static void set_i(SEQ_VARS *const pVar, int value)
{
    pVar->i = value;
}
}%
