TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs
SNCFLAGS = +m

#  Sequence programs and/or object files to create
SEQS = sncExample sncExEntry sncExOpt
PROD = $(SEQS)
OBJS_vxWorks = $(SEQS)

#  Libraries
PROD_LIBS += seq
seq_DIR    = $(INSTALL_LIB)

include $(TOP)/test/Makefile.pv

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

