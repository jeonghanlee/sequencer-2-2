TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs (not needed under VxWorks)

SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
SEQS = sncDelay sncEntry sncEntryOpte sncEntryVar sncExitOptx sncOptt sncOpttVar
PROD = $(SEQS)
OBJS_vxWorks = $(SEQS)

sncDelay_SRCS = sncDelay.st
sncEntry_SRCS = sncEntry.st
sncEntryOpte_SRCS = sncEntryOpte.st
sncEntryVar_SRCS = sncEntryVar.st
sncExitOptx_SRCS = sncExitOptx.st
sncOptt_SRCS = sncOptt.st
sncOpttVar_SRCS = sncOpttVar.st

#  Libraries
PROD_LIBS += seq
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

include $(TOP)/test/Makefile.pv

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

