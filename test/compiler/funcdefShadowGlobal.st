program funcdefShadowGlobalTest

option +r;

int x;
assign x;

void pvSetX(int x)
{
    pvPut(x);
}

ss simple {
    state simple {
        when () {
            pvSetX(1.0);
        } exit
    }
}
