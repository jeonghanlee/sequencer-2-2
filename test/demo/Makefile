TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD += demo.dbd

#  Generate snc main programs

#  Products
PROD_HOST = demo

demo_SRCS += demo.st

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
demo_SRCS_DEFAULT += demo_registerRecordDeviceDriver.cpp
demo_SRCS_DEFAULT += demoMain.c
demo_SRCS_vxWorks += -nil-

demo_OBJS += $(INSTALL_BIN)/devSequencer.o

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

