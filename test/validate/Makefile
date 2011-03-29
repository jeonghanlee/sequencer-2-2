TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m -i
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
PROD += sncDelay
PROD += sncEntry
PROD += sncEntryOpte
PROD += sncEntryVar
PROD += sncExitOptx
PROD += sncOptt
PROD += sncOpttVar
PROD += assign
PROD += change
PROD += declarations
PROD += local
PROD += subscript
PROD += syncq
PROD += pvSync
PROD += pvPutAsync
PROD += pvGetComplete
PROD += array
PROD += stop
PROD += userfunc
PROD += eftest
PROD += race

#PROD += ctest

#  Libraries
PROD_LIBS += seqSoftIoc
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

LIBRARY += seqSoftIoc

DBD += seqSoftIoc.dbd
seqSoftIoc_DBD += base.dbd

seqSoftIoc_SRCS += seqSoftIoc_registerRecordDeviceDriver.cpp

DB_INSTALLS += syncq.db
DB_INSTALLS += pvGetComplete.db
DB_INSTALLS += pvPutAsync.db
DB_INSTALLS += userfunc.db
DB_INSTALLS += eftest.db
DB_INSTALLS += race.db

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
