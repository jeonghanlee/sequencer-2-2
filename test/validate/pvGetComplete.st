program pvGetComplete

/* option +s; */

ss read {
    double test;
    assign test to "pvGetComplete";

    state get {
        when (delay(2)) {
            printf("pvGet\n");
            pvGet(test, ASYNC);
            if (pvGetComplete(test)) {
                printf("immediate completion: test=%f\n", test);
            }
            epicsThreadSleep(1.0);
            if (pvGetComplete(test)) {
                printf("completion after delay: test=%f\n", test);
            }
        } state wait
    }
    state wait {
        when (pvGetComplete(test)) {
            printf("completion after sync: test=%f\n", test);
        } state get
    }
}

ss write {
    int out=1;
    assign out to "pvGetComplete";
    state run {
        when (delay(0.7)) {
            out = -out;
            printf("pvPut(out=%d)\n",out);
            pvPut(out);
        } state run
    }
}
