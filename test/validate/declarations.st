program declarations

option +s;

foreign MAX_STRING_SIZE;

int *(*(*complicated)[2][3])[1] = 0;

evflag ef;

#if 0
evflag efa[3];
evflag *efp;
#endif

/* struct member access */
%%struct r { int x; } rv;

/* declare the variable and the tag to avoid warnings */
foreign rv, x;

entry {
    printf("%d\n", rv.x);
}

ss one {
    state one {
        char *a[4];
        /* assign a; */
        string b[4];
        assign b;
        int c[2];
        assign c;
        monitor b;
        monitor c;
        sync b to ef;
        sync c to ef;
        string s = "s";
        assign s;
        /* evflag ef; */
        /* declare z; */
        entry {
#if 0
            a[0] = "1234567890123456789012345678901234567890";
            a[1] = "abcdefghijabcdefghijabcdefghijabcdefghij";
            a[2] = "ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ";
            a[3] = "123";
            b[0] = "1234567890123456789012345678901234567890";
            b[1] = "abcdefghijabcdefghijabcdefghijabcdefghij";
            b[2] = "ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ";
            b[3] = "123";
            c[0] = 123;
            c[1] = 456;
            s = "bla";
#endif
            /* pvPut(a); */
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
    }
}
