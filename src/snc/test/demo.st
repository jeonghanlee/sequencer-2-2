/* demo.st,v 1.1.1.2 2000/04/25 19:15:14 wlupton Exp
 *
 * Demo EPICS sequence
 */

#define PRINTF seqLog

/* message system choice */
#ifdef KTL

#define PVSYS    "pvsys=ktl"

#define LIGHT    "demo.light"
#define LIGHTON  "demo.lighton"
#define LIGHTOFF "demo.lightoff"
#define VOLTAGE  "demo.voltage"
#define LOLIMIT  "demo.lolimit"
#define HILIMIT  "demo.hilimit"

#else

#define PVSYS    "pvsys=ca"

#define LIGHT    "demo:light"
#define LIGHTON  "demo:lightOn"
#define LIGHTOFF "demo:lightOff"
#define VOLTAGE  "demo:voltage"
#define LOLIMIT  "demo:loLimit"
#define HILIMIT  "demo:hiLimit"

#endif

program demo( PVSYS ",debug=1" )

/* options */
option  +r;

/* control variables and limits for voltage control */
double  voltage;
assign  voltage  to VOLTAGE;
monitor voltage;

double  loLimit;
assign  loLimit  to LOLIMIT;
monitor loLimit;
evflag  loFlag;
sync    loLimit  to loFlag;

double  hiLimit;
assign  hiLimit  to HILIMIT;
monitor hiLimit;
evflag  hiFlag;
sync    hiLimit  to hiFlag;

double  x[2][100];
assign  x to {"apple", "orange"};
monitor x;
evflag  xFlag;
sync    x[0] xFlag;

%%TS_STAMP t;
/* declare t; */
long    i = 0;

/* temporary timer stuff */
double frac;
long done;
%%void doStuff( double frac );
%%double getFractionalSecond();

/* entry handler (called in context of first state-set before other threads
   are created) */
entry {
#if defined( UNIX ) && TRUE
%%#include "logClient.h"
    iocLogInit();
#endif
}

/* light control state-set */
ss light {

    /* control variables and transition points for light control */
    long    light = -1;
    assign  light    to LIGHT;

    string  sstate;
    assign  sstate to "demo:STATE";

    state START {
        entry {
            sstate = "START";
            pvPut(sstate);
        }
        when () {
            light = 0;
            pvPut( light );
        } state LIGHT_OFF
    }

    state LIGHT_OFF {

        double  lightOn;
        assign  lightOn  to LIGHTON;
        monitor lightOn;

        when ( voltage > lightOn ) {
            light = 1;
            pvPut( light );
            PRINTF( "light_off -> light_on\n" );
        } state LIGHT_ON
    }

    state LIGHT_ON {

        double  lightOff = 1.1;
        assign  lightOff to LIGHTOFF;
        monitor lightOff;

        when ( voltage < lightOff ) {
            light = 0;
            pvPut( light );
            PRINTF( "light_on -> light_off\n" );
        } state LIGHT_OFF
    }
}

/* voltage ramp state set */
ss ramp {
    state START {
        when () {
            voltage = loLimit;
            pvPut( voltage );
            PRINTF("start -> ramp_up\n");
        } state RAMP_UP
    }

    state RAMP_UP {
        when ( voltage > hiLimit ) {
            PRINTF("ramp_up -> ramp_down\n");
        } state RAMP_DOWN

        when ( delay( 0.1 ) ) {
            voltage += 0.1;
            if ( pvPut( voltage ) < 0 )
                PRINTF( "pvPut( voltage ) failed\n" );
            /* PRINTF( "voltage increment\n" ); */
        } state RAMP_UP
    }

    state RAMP_DOWN {
        when ( voltage < loLimit ) {
            PRINTF("ramp_down -> ramp_up\n");
        } state RAMP_UP

        when ( delay( 0.1 ) ) {
            voltage -= 0.1;
            if ( pvPut( voltage ) < 0 )
                PRINTF( "pvPut( voltage ) failed\n" );
            /* PRINTF( "voltage decrement\n" ); */
        } state RAMP_DOWN
    }
}

/* hi / lo limit constraint state set */
ss limit {
    state START {
        when ( efTestAndClear( loFlag ) && loLimit > hiLimit ) {
            hiLimit = loLimit;
            pvPut( hiLimit );
        } state START

        when ( efTestAndClear( hiFlag ) && hiLimit < loLimit ) {
            loLimit = hiLimit;
            pvPut( loLimit );
        } state START
    }
}

/* apples and oranges */
ss apples {
    state init {
        when ( efTestAndClear( xFlag ) ) {
            PRINTF( "xFlag\n" );
            for ( i = 0; i < 100; i++ )
                x[1][i] = x[0][i] + 2;
            pvPut( x[1] );
            if ( pvStatus( x[0] ) == pvStatOK ) {
                t = pvTimeStamp( x[0] );
                printf( "status=%d, severity=%d, timestamp=%d %d\n",
                        pvStatus( x[0] ), pvSeverity( x[0] ),
                        t.secPastEpoch, t.nsec );
                }
        } state init
    }
}

/* test timers */
ss timer {

    state START {
        when () {
            frac = 0.0;
            done = FALSE;
        } state POLL
    }

    state POLL {
        when ( delay( 0.1 ) ) {
            frac = getFractionalSecond();
        } state POLL

        when ( !done && frac >= 0.5 ) {
            doStuff( frac );
            done = TRUE;
        } state POLL

        when ( done && frac < 0.5 ) {
            done = FALSE;
        } state DONE
    }

    state DONE {
        when (done) {
        } state DONE
    }
}

%%#include <time.h>

%{

double getFractionalSecond() {
    struct timeval tv;
    ( void ) gettimeofday( &tv, NULL );
    return ( ( double ) tv.tv_usec ) / 1e6;
}

void doStuff( double frac ) {
    printf( "doStuff at frac = %g\n", frac );
}

}%
