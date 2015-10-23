program timerTest

struct epicsTimeStamp start_time;

struct timer {
    double delay;
    double delta_delay;
};

struct timer timer1, timer2, timer3;

void timer_init(struct timer *timer, double delay_time) {
    timer->delay = timer->delta_delay = delay_time;
}

void timer_update(struct timer *timer) {
    timer->delay += timer->delta_delay;
}

void dump_time_diff(char *msg, struct epicsTimeStamp *relative_to) {
    struct epicsTimeStamp current_time;
    epicsTimeGetCurrent(&current_time);
    printf("%s: %.3f\n", msg,
        epicsTimeDiffInSeconds(&current_time, relative_to));
}

entry {
    epicsTimeGetCurrent(&start_time);
}

ss timer {
    struct epicsTimeStamp cycle_start_time;
    state reset {
        when () {
            timer_init(&timer1, 0.333);
            timer_init(&timer2, 0.877);
            timer_init(&timer3, 2.797);
            epicsTimeGetCurrent(&cycle_start_time);
            dump_time_diff("timers initialized", &start_time);
        } state cycle
    }
    state cycle {
        option -t;
        when (delay(60.166)) {
        } state reset
        when (delay(timer1.delay)) {
            timer_update(&timer1);
            dump_time_diff("timer1 fired", &cycle_start_time);
        } state cycle
        when (delay(timer2.delay)) {
            timer_update(&timer2);
            dump_time_diff("timer2 fired", &cycle_start_time);
        } state cycle
        when (delay(timer3.delay)) {
            timer_update(&timer3);
            dump_time_diff("timer3 fired", &cycle_start_time);
        } state cycle
    }
}

#if 0
expected output:
timers initialized: 0.000
timer1 fired: 0.333
timer1 fired: 0.666
timer2 fired: 0.877
timer1 fired: 0.999
timer1 fired: 1.332
timer1 fired: 1.665
timer2 fired: 1.754
timer1 fired: 1.998
timer1 fired: 2.331
timer2 fired: 2.631
timer1 fired: 2.664
timer3 fired: 2.797
timer1 fired: 2.997
timer1 fired: 3.330
timer2 fired: 3.508
timer1 fired: 3.663
timer1 fired: 3.996
timer1 fired: 4.329
timer2 fired: 4.385
timer1 fired: 4.662
timer1 fired: 4.995
timer2 fired: 5.262
timer1 fired: 5.328
timer3 fired: 5.594
timer1 fired: 5.661
...
#endif
