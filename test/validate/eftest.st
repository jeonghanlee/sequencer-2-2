program pvGetComplete

option +s;

evflag f;

ss read {
    double test;
    assign test to "eftest";
    monitor test;
    sync test to f;

    state wait {
        when (efTestAndClear(f)) {
            printf("test=%f\n", test);
        } state wait
    }
}

ss write {
    double out = 1.0;
    assign out to "eftest";
    state run {
        when (delay(1)) {
            out = -out;
            pvPut(out);
        } state run
    }
}
