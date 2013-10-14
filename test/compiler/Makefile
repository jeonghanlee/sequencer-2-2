TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

TESTSCRIPTS_HOST += snc_test.t

# uncomment these tests if building for 32 bit systems
#TESTSCRIPTS_HOST += build_test.t
#TESTSCRIPTS_CROSS += build_test.t

# uncomment these tests if building for 64 bit systems
#TESTSCRIPTS_HOST += build64_test.t
#TESTSCRIPTS_CROSS += build64_test.t

TESTPROD_HOST += type_expr
TESTPROD_HOST += funcdef
TESTPROD_HOST += cppComment
TESTPROD_HOST += structdef

PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
