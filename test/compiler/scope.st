program scope

int i, j;
evflag f, g, h;

ss start {
    assign i;
    monitor i;
    sync i to f;
    syncq i to f;
    int k;
    state first {
        assign j;
        monitor j;
        sync j to g;
        syncq k to h;
        when () {
        } exit
    }
}
