/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program sizeofTest

%%#include "../testSupport.h"

entry {
    seq_test_init(2);
}

ss test {
    state sizes {
        when () {
            char c;
            char *p;
            testOk1(sizeof(char)==1);
            testOk1(sizeof(*p)==1);
            c = *p;
        } exit
    }
}

exit {
    seq_test_done();
}
