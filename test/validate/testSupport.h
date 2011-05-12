#ifndef INCtestSupport_h
#define INCtestSupport_h

#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsUnitTest.h"

void run_seq_test(seqProgram *seqProg);
void seq_test_done(void);

#endif /* INCtestSupport_h */
