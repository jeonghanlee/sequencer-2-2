/*************************************************************************\
Copyright (c) 2014      Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program opt_p

option +p;

int a[1];
assign a to {};

ss test {
    state test {
        when () {
            pvAssign(a,"");     /* error: not allowed (even with +p) */
            pvAssigned(a);      /* error: not allowed (even with +p) */
            pvConnected(a);     /* error: not allowed (even with +p) */
            pvCount(a);         /* error: not allowed (even with +p) */
            pvFlushQ(a);        /* error: not allowed (even with +p) */
            pvFreeQ(a);         /* error: not allowed (even with +p) */
            pvGet(a);           /* error: not allowed (even with +p) */
            pvGetCancel(a);     /* ok: always allowed */
            pvGetComplete(a);   /* ok: allowed due to +p */
            pvGetQ(a);          /* error: not allowed (even with +p) */
            pvIndex(a);         /* error: not allowed (even with +p) */
            pvMessage(a);       /* error: not allowed (even with +p) */
            pvMonitor(a);       /* ok: allowed due to +p */
            pvName(a);          /* error: not allowed (even with +p) */
            pvPut(a);           /* error: not allowed (even with +p) */
            pvPutCancel(a);     /* ok: always allowed */
            pvPutComplete(a);   /* ok: always allowed */
            pvSeverity(a);      /* error: not allowed (even with +p) */
            pvStatus(a);        /* error: not allowed (even with +p) */
            pvStopMonitor(a);   /* ok: allowed due to +p */
            pvSync(a);          /* ok: allowed due to +p */
            pvTimeStamp(a);     /* error: not allowed (even with +p) */
        } exit
    }
}
