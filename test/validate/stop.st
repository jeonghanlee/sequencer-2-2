program stop

ss main {
    state start {
        when (delay(5)) {
            seqStop(epicsThreadGetIdSelf());
        } state start
    }
}

ss progress {
    state running {
        when (delay(1)) {
            printf("still running...\n");
        } state running
    }
}
