TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD += demo.dbd

#  Generate snc main programs

#  Products
PROD_DEFAULT = demo
PROD_WIN32 = -nil-

demo_SRCS += demo.st

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += softDevIoc
PROD_LIBS += recIoc
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

demo_OBJS += $(INSTALL_BIN)/devSequencer.o

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

