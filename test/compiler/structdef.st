program structdefTest

option +W;

%%#include <stdlib.h>

struct limits {
    double low;
    double high;
};

struct mystruct {
    double value[5];
    struct limits warning;
    struct limits alarm;
    double deadband;
    %%void (*process)(struct mystruct *ps);
    struct mystruct *children[2];
};

%{
void dump_array(double *v, int n) {
    int i;
    printf("[");
    for (i=0; i<n; i++)
        printf("%s%f", i?",":"", v[i]);
    printf("]");
}
void dump_mystruct(struct mystruct *p) {
    printf("value=");
    dump_array(p->value, 5);
    printf("warning={%f,%f}, alarm={%f,%f}, deadband=%f, process=%p, children=[%p,%p]\n",
        p->warning.low, p->warning.high,
        p->alarm.low, p->alarm.high,
        p->deadband, p->process, p->children[0], p->children[1]);
}
}%

struct mystruct s = { {0.0,1.1,2.2,3.3,4.4}, {-8,+8}, {-10,+10}, 0.01, dump_mystruct, {&s,&s}};

entry {
    struct mystruct *ps = *(&s.children[0]+1);
    if (s.process)
        s.process(ps);
    ps->children[1]->process(ps->children[0]);
}

#include "../simple.st"

exit {
    exit(0);
}
