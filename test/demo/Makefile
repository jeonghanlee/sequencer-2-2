TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Select message system (comment as appropriate)
#USR_CPPFLAGS = -DKTL
USR_CPPFLAGS = -DCA

# FIXME -- I don't know why this is necessary -- should the seq_DIR below
# not generate the proper -L argument??       Eric Norum  2001-02-16
#USR_LDFLAGS += -L$(INSTALL_LIB)

#  Generate Unix main program (not CA only)
ifneq "$(PVCA)" "TRUE"
SNCFLAGS = +m
endif

#  Products
PROD_DEFAULT = demo
PROD_WIN32 = -nil-
PROD_RTEMS = demo
LIBRARY_vxWorks = demo

#  Sources (CA only)
ifeq "$(PVCA)" "TRUE"
demo_SRCS = demoMain.c
endif

#  Includes and libraries
PROD_LIBS += seqdev seq
PROD_LIBS += iocsh miscIoc rsrvIoc dbtoolsIoc asIoc dbIoc registryIoc dbStaticIoc ca Com readline curses

seq_DIR    = $(INSTALL_LIB)
seqdev_DIR = $(INSTALL_LIB)

include $(TOP)/test/Makefile.pv

ifeq "$(PVCA)" "TRUE"
PROD_LIBS += iocsh miscIoc rsrvIoc dbtoolsIoc asIoc dbIoc registryIoc dbStaticIoc ca Com
endif

# Unix objects (mostly CA only)
demo_SRCS += demo.st

ifeq "$(PVCA)" "TRUE"
demo_SRCS_DEFAULT += demo_registerRecordDeviceDriver.cpp
demo_SRCS_vxWorks +=  -nil-

demo_OBJS += $(EPICS_BASE_BIN)/aoRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/boRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/stringinRecord.o
demo_OBJS += $(EPICS_BASE_BIN)/waveformRecord.o

demo_OBJS += $(EPICS_BASE_BIN)/devAoSoft.o
demo_OBJS += $(EPICS_BASE_BIN)/devBoSoft.o
demo_OBJS += $(EPICS_BASE_BIN)/devWfSoft.o
endif

#  VxWorks library
LIBRARY_vxWorks = demoIoc

demoIoc_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary

demoIoc_OBJS += $(EPICS_BASE_BIN)/aoRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/boRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/stringinRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/waveformRecord.o

demoIoc_OBJS += $(EPICS_BASE_BIN)/devAoSoft.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/devBoSoft.o

#  Databases
DBD += demo.dbd
DBD += demo.db

#  Scripts
SCRIPTS = demo.vws

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

