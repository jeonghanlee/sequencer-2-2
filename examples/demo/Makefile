TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

DBD += demo.dbd
# do not install dbd file, just build it:
INSTALL_DBDS=

TESTPROD_IOC = demo
demo_SRCS += demo.st

#generic IOC stuff
demo_SRCS += demo_registerRecordDeviceDriver.cpp
demo_SRCS_DEFAULT += demoMain.c
demo_SRCS_vxWorks += -nil-
demo_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

demo_LIBS += $(EPICS_BASE_IOC_LIBS)
demo_LIBS += seq pv

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
