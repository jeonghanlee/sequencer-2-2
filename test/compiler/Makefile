TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m -i
SNCFLAGS_vxWorks += -nil-

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

TESTSCRIPTS_HOST += snc_test.t

ifeq '$(findstring 64,$(ARCH_CLASS))' ''
TESTSCRIPTS_HOST += make_test.t
TESTSCRIPTS_CROSS += make_test.t
else
TESTSCRIPTS_HOST += make64_test.t
TESTSCRIPTS_CROSS += make64_test.t
endif

PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
