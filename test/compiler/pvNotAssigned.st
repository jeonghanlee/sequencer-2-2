program pvNotAssigned

int a;

entry {
    pvGet(a);
    pvPut(a);
    pvAssign(a, "");
    pvMonitor(a);
}
ss x {
    state y {
        entry {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        }
        when () {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        } exit
        exit {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        }
    }
}
exit {
    pvAssign(a, "");
    pvMonitor(a);
    pvGet(a);
    pvPut(a);
}
