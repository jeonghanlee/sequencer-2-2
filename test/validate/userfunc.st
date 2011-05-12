program userfuncTest

option +r;

%{
static void incr(SS_ID ssId, int *pv, VAR_ID v)
{
    seq_pvGet(ssId, v, SYNC);
    *pv += 1;
    seq_pvPut(ssId, v, SYNC);
}
}%

int i;
assign i to "counter";

foreign pVar, ssId;

ss myss {
    state doit {
        when (delay(1)) {
            incr(ssId, &i, pvIndex(i));
        } state doit
    }
}
