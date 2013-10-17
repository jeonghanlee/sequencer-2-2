program structdefTest

option +W;

%%#include <stdlib.h>

struct limits {
    double low;
    double high;
};

struct mystruct {
    char *name;
    double value[5];
    struct limits warning;
    struct limits alarm;
    double deadband;
    %%void (*process)(struct mystruct *ps);
    struct mystruct *children[2];
    int mark;
};

%{
void dump_array(double *v, int n) {
    int i;
    printf("[");
    for (i=0; i<n; i++)
        printf("%s%.1f", i?",":"", v[i]);
    printf("]\n");
}
void dump_mystruct(struct mystruct *p) {
    int i;
    if (p->mark) {
        printf("%s already dumped\n", p->name);
    } else {
        p->mark = 1;
        printf("name=%s, value=", p->name);
        dump_array(p->value, 5);
        printf("  warning={%.1f,%.1f}, alarm={%.1f,%.1f}, deadband=%.1f\n  process=%p\n",
            p->warning.low, p->warning.high,
            p->alarm.low, p->alarm.high,
            p->deadband, p->process);
        for (i=0; i<2; i++) {
            dump_mystruct(p->children[i]);
        }
        p->mark = 0;
    }
}
}%

struct mystruct s = { "xxx", {0.0,1.1,2.2,3.3,4.4}, {-8,+8}, {-10,+10}, 0.01, dump_mystruct, {&s,&s}, 0};

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
