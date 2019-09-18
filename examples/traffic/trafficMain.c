/*************************************************************************\
Copyright (c) 1990-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    17MAR2000 */
/*
 * Main program for traffic sequencer
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iocsh.h"

/* Call trafficRegistrar manually, avoids build problems on Windows */
extern void (*pvar_func_trafficRegistrar)(void);

int main(int argc,char *argv[])
{
    (*pvar_func_trafficRegistrar)();
    if(argc>=2)
        iocsh(argv[1]);
    iocsh(NULL);
    return(0);
}
