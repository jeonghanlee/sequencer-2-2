TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

# Products
TESTPROD_HOST += race

race_SRCS += race.st

race_LIBS += seq pv
race_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

runtest: install
	splitvt -upper 'softIoc -s -d race.db'\
		-lower './O.$(EPICS_HOST_ARCH)/race -s'
