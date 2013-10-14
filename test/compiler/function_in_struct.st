program function_in_struct

option +W;

%%#include <stdlib.h>

struct mystruct {
    double val;
    int *phi;
    int *plo;
    %%void (*cfun)(struct mystruct *ps);   /* without implicit parameters */
    void (*fun)(struct mystruct *ps);       /* with implicit parameters */
    struct mystruct *children[3];
};

void myfun(struct mystruct *p) {
    printf("myfun says: val=%f, *phi = %d, *plo=%d\n", p->val, *p->phi, *p->plo);
}

%{
void mycfun(struct mystruct *p) {
    printf("mycfun says: val=%f, *phi = %d, *plo=%d\n", p->val, *p->phi, *p->plo);
}
}%

int hilo = 33;
struct mystruct s = { 42.23, &hilo, &hilo, mycfun, myfun, {&s,&s,&s}};

entry {
    struct mystruct *ps = *(&s.children[0]+1);
    myfun(ps);
    ps->fun(ps);
    s.fun(ps);
    if (s.cfun)
        s.cfun(ps);
    ps->children[1]->fun(ps->children[0]);
}

#include "../simple.st"

exit {
    exit(0);
}
