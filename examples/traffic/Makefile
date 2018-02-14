TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

DBD += traffic.dbd
# do not install dbd file, just build it:
INSTALL_DBDS=

TESTPROD_IOC = traffic
traffic_LIBS += $(EPICS_BASE_IOC_LIBS) seq pv
traffic_SRCS += traffic.st
traffic_SRCS += traffic_registerRecordDeviceDriver.cpp
traffic_SRCS += trafficMain.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
