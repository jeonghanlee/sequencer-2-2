TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD += demo.dbd

#  Generate snc main programs

#  Products
PROD_DEFAULT = demo
PROD_WIN32 = -nil-
PROD_RTEMS = demo
LIBRARY_vxWorks = demo

demo_SRCS += demo.st

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

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
demo_SRCS_DEFAULT += demo_registerRecordDeviceDriver.cpp
demo_SRCS_DEFAULT += demoMain.c
demo_SRCS_vxWorks += -nil-

demo_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary
demo_OBJS_vxWorks += $(INSTALL_BIN)/seqLibrary
demo_OBJS_vxWorks += $(INSTALL_BIN)/pvLibrary


demo_OBJS += $(EPICS_BASE_BIN)/aoRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/boRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/stringinRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/waveformRecord.o

demo_OBJS += $(EPICS_BASE_BIN)/devAoSoft.o
demo_OBJS += $(EPICS_BASE_BIN)/devBoSoft.o
demo_OBJS += $(EPICS_BASE_BIN)/devWfSoft.o

demo_OBJS += $(INSTALL_BIN)/devSequencer.o

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

