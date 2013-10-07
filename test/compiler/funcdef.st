program funcdefTest

option +r;

int x;
assign x;

ss simple {
    state simple {
        when () {
            void *p = f1();
            p = f2(p,0);
            pvSetX(1.0);
        } exit
    }
}

function void *f1(void)
{
    return 0;
}

function void *f2(void *x, double **d)
{
    void *(*f)(void) = (void *(*)(void))x;
    return f();
}

function void pvSetX(int val)
{
    pvPut(x,val);
    printf("x=%d, val=%d\n", x, val);
}
