program syncq_test

option +s;

string s[2];
assign s to "syncq";
monitor s;
evflag ef_s;
syncq s to ef_s 5;

int n = 0;
assign n;

ss get {
    state get {
        when (pvGetQ(s)) {
            printf("got: [%s,%s]\n", s[0], s[1]);
        } state pause
    }
    state pause {
        when (delay(2)) {
        } state get
    }
}

ss put {
    state put {
        when (delay(1)) {
            sprintf(s[0], "%d", n);
            sprintf(s[1], "%d", -n);
            pvPut(s);
            n++;
            pvPut(n);
        } state put
    }
}

ss flush {
    monitor n;
    state flush {
        when (n%20==0) {
            pvFlushQ(s);
        } state flush
    }
}
