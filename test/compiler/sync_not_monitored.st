/*************************************************************************\
Copyright (c) 2010-2015 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program p

float x = 0.0;
assign x;
evflag f;
sync x to f; /* ok: not monitored */

ss simple {
    state simple {
        /* nonsense code to avoid unused variable warnings */
        when (x==x) {
            efSet(f);
        } exit
    }
}
