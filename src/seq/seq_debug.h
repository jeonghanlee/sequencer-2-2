#ifndef INCLseq_debugh
#define INCLseq_debugh

#include <stdio.h>
#include "errlog.h"

static int nothing(const char *format,...) {return 0;}

/* To enable debug messages in a region of code, say e.g. */

#undef DEBUG
#define DEBUG printf

/* ... code with debug messages enabled... */

#undef DEBUG
#define DEBUG nothing

#endif
