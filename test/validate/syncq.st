program syncq_test

option +s;

string s[2];
assign s;
monitor s;
evflag ef_s;
syncQ s to ef_s 5;

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
        int n = 0;
        when (delay(1)) {
            sprintf(s[0], "%d", n);
            sprintf(s[1], "%d", -n);
            pvPut(s);
            n++;
        } state put
    }
}
