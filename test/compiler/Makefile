TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m -i
SNCFLAGS_vxWorks += -nil-

SNC = $(INSTALL_HOST_BIN)/snc

TESTSCRIPTS_HOST += test.t

#TESTPROD += misplacedExit
#TESTPROD += scope
#TESTPROD += sncExOpt_DuplOpt
#TESTPROD += sncExOpt_UnrecOpt
#TESTPROD += tooLong

PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
