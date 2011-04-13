program declarations

option +s;

foreign MAX_STRING_SIZE;

int *(*(*complicated)[2][3])[1] = 0;

evflag ef;

#if 0
evflag efa[3];  /* error: array of event flags */
evflag *efp;    /* error: pointer to event flag */
#endif

/* struct member access */
%%struct r { int x; } rv;

/* declare the variable and the tag to avoid warnings */
foreign rv, x;

%%TS_STAMP ts;
foreign ts;

int var;
assign var;

entry {
    printf("%d\n", rv.x);
    ts = pvTimeStamp(var);
}

ss one {
    state one {
        char *a[4] = {
            "1", "2", "3"
        };
        /* assign a; */
        string b[4] = {
            "1234567890123456789012345678901234567890",
            "abcdefghijabcdefghijabcdefghijabcdefghij",
            "ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ",
            "123"
        };
        assign b;
        int c[2] = {123,456};
        assign c;
        monitor b;
        monitor c;
        sync b to ef;
        sync c to ef;
        string s = "s";
        assign s;
        /* evflag ef; */    /* error: event flag must be global */
        /* foreign z; */    /* error: foreign must be global */
        entry {
            /* pvPut(a); */ /* error: not assigned */
            pvPut(b);
            pvPut(c);
            pvPut(s);
        }
        when (delay(10)) {
        } state one
        when (efTestAndClear(ef)) {
            printf("Event on b or c\n");
            {
                /* shadow state local variable 's' */
                string s = "blub";
                printf("s=%s\n",s);
            }
        } state one
        when (delay(0.1) && !complicated) {
            exit(0);
        } exit
    }
}
