/* $Id: demoMain.c,v 1.1.1.1 2000-04-04 03:23:05 wlupton Exp $
 *
 * Main program for demo sequencer
 */

#include <string.h>
#include <unistd.h>

#include "osiThread.h"
#include "dbAccess.h"
#include "errlog.h"
#include "taskwd.h"

#include "seqCom.h"

/* reference sequencer definition */
extern struct seqProgram demo;

/* main program */
int main(int argc,char *argv[]) {
    char macro_def[256];

    /* append ", shell=T" to macro definitions */
    sprintf(macro_def, "%s%s%s", (argc>1)?argv[1]:"", (argc>1)?", ":"",
	    "shell=T");

    /* initialize thread subsystem */
    threadInit();

    /* change directory to the install directory */
    chdir("/home/wlupton/epics/seq");

    /* load database definitions */
    dbLoadDatabase("dbd/records.dbd", NULL, NULL);

    /* register records, device support and drivers */
    registerRecordDeviceDriver(pdbbase);

    /* load record instances */
    dbLoadRecords("dbd/demo.db", NULL);

    /* initialize IOC */
    iocInit();

    /* create sequencer ("shell" macro will cause it to run shell) */
    seq((void *)&demo, macro_def, 0);

threadSleep(1);
threadShowAll(0);

    return 0;
}

