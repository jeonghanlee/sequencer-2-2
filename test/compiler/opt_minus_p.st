/*************************************************************************\
Copyright (c) 2014      Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program opt_p

option -p;

int a[1];
assign a to {};

ss test {
    state test {
        when () {
            pvAssign(a,"");     /* error: not allowed */
            pvAssigned(a);      /* error: not allowed */
            pvConnected(a);     /* error: not allowed */
            pvCount(a);         /* error: not allowed */
            pvFlushQ(a);        /* error: not allowed */
            pvFreeQ(a);         /* error: not allowed */
            pvGet(a);           /* error: not allowed */
            pvGetCancel(a);     /* ok: did not exist in 2.1 */
            pvGetComplete(a);   /* error: not allowed in compatibility mode (option -p) */
            pvGetQ(a);          /* error: not allowed */
            pvIndex(a);         /* error: not allowed */
            pvMessage(a);       /* error: not allowed */
            pvMonitor(a);       /* error: not allowed in compatibility mode (option -p) */
            pvName(a);          /* error: not allowed */
            pvPut(a);           /* error: not allowed */
            pvPutCancel(a);     /* ok: did not exist in 2.1 */
            pvPutComplete(a);   /* ok: was already allowed in 2.1 */
            pvSeverity(a);      /* error: not allowed */
            pvStatus(a);        /* error: not allowed */
            pvStopMonitor(a);   /* error: not allowed in compatibility mode (option -p) */
            pvSync(a);          /* error: not allowed in compatibility mode (option -p) */
            pvTimeStamp(a);     /* error: not allowed */
        } exit
    }
}
