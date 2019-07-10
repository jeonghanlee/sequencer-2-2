TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

#  Generate snc main programs
SNCFLAGS += +m

#  Sequence programs and/or object files to create
TESTPROD_HOST += decl
TESTPROD_HOST += delay
TESTPROD_HOST += entry
TESTPROD_HOST += entryVar
TESTPROD_HOST += optt
TESTPROD_HOST += parameter
TESTPROD_HOST += pvTimeStamp
TESTPROD_HOST += syncq
TESTPROD_HOST += timer
TESTPROD_HOST += version

#  Libraries
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
