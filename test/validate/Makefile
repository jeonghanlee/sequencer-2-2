TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS = +m

#  Sequence programs and/or object files to create
SEQS = sncDelay sncEntry sncEntryOpte sncEntryVar sncExitOptx sncOptt sncOpttVar
PROD = $(SEQS)
OBJS_vxWorks = $(SEQS)

#  Sequencer and EPICS libraries
PROD_LIBS += seq
seq_DIR    = $(SEQ_LIB)

include $(TOP)/test/Makefile.pv

PROD_LIBS += Com

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

