TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Select message system (comment as appropriate)
#USR_CPPFLAGS = -DKTL
USR_CPPFLAGS = -DCA

#  Generate Unix main program (not CA only)
ifneq "$(PVCA)" "TRUE"
SNCFLAGS = +m
endif
SNCFLAGS_RTEMS = +i

#  Products
PROD_DEFAULT = demo
PROD_WIN32 = -nil-
OBJS_vxWorks = demo
PROD_RTEMS = demo

#  Sources (CA only)
ifeq "$(PVCA)" "TRUE"
demo_SRCS = demoMain
endif

#  Includes and libraries
PROD_LIBS += seqdev seq
seq_DIR    = $(SEQ_LIB)
seqdev_DIR    = $(SEQ_LIB)

include $(TOP)/test/Makefile.pv

ifeq "$(PVCA)" "TRUE"
PROD_LIBS += dbtoolsIoc miscIoc asIoc dbIoc registryIoc dbStaticIoc rsrvIoc Com
endif

# Unix objects (mostly CA only)
demo_OBJS += demo.o

ifeq "$(PVCA)" "TRUE"
demo_OBJS += registerRecordDeviceDriver.o

demo_OBJS += $(EPICS_BASE_BIN)/iocInit.o

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
LIBRARY_RTEMS = demoIoc
demoIoc_OBJS += registerRecordDeviceDriver.o

demoIoc_OBJS_vxWorks += $(EPICS_BASE_BIN)/iocCoreLibrary
demoIoc_OBJS += $(EPICS_BASE_BIN)/iocInit.o

demoIoc_OBJS += $(EPICS_BASE_BIN)/aoRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/boRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/stringinRecord.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/waveformRecord.o

demoIoc_OBJS += $(EPICS_BASE_BIN)/devAoSoft.o
demoIoc_OBJS += $(EPICS_BASE_BIN)/devBoSoft.o

#  Databases
DBDEXPAND  = records.dbd
DBDNAME    = records.dbd
DBDINSTALL = demo.db

#  Scripts
SCRIPTS = demo.vws

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

demo.c: ../Makefile

