TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs

SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
SEQS = sncExample sncExEntry sncExOpt
PROD = $(SEQS)
OBJS_vxWorks = $(SEQS)
sncExample_SRCS = sncExample.st
sncExEntry_SRCS = sncExEntry.st
sncExOpt_SRCS = sncExOpt.st

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += iocsh
PROD_LIBS += miscIoc
PROD_LIBS += rsrvIoc
PROD_LIBS += dbtoolsIoc
PROD_LIBS += asIoc
PROD_LIBS += dbIoc
PROD_LIBS += registryIoc
PROD_LIBS += dbStaticIoc
PROD_LIBS += ca
PROD_LIBS += Com

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

