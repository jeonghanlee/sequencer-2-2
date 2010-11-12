program pvSync

int x;
assign x;
monitor x;

evflag f;

entry {
    pvSync(x,f);
}

ss pvSync {
    state idle {
        when (delay(10)) {
        } state idle
    }
}
