TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m -i
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
TESTPROD += sncDelay
TESTPROD += sncEntry
TESTPROD += sncEntryOpte
TESTPROD += sncEntryVar
TESTPROD += sncExitOptx
TESTPROD += sncOptt
TESTPROD += sncOpttVar
TESTPROD += assign
TESTPROD += change
TESTPROD += declarations
TESTPROD += local
TESTPROD += subscript
TESTPROD += syncq
TESTPROD += pvSync
TESTPROD += pvPutAsync
TESTPROD += array
TESTPROD += stop
TESTPROD += userfunc
TESTPROD += race

REGRESSION_TESTS_WITH_DB += eftest
REGRESSION_TESTS_WITH_DB += pvGet
REGRESSION_TESTS_WITH_DB += bittypes

REGRESSION_TESTS_WITHOUT_DB += safeMonitor

REGRESSION_TESTS += $(REGRESSION_TESTS_WITHOUT_DB)
REGRESSION_TESTS += $(REGRESSION_TESTS_WITH_DB)

ifeq '$(EPICS_HAS_UNIT_TEST)' '1'
TESTPROD += $(REGRESSION_TESTS)
TESTSCRIPTS_HOST += $(REGRESSION_TESTS:%=%.t)
TESTSCRIPTS_HOST += $(REGRESSION_TESTS_WITH_DB:%=%Ioc.t)
endif

#TESTPROD += ctest

#  Libraries
PROD_LIBS += seqSoftIoc
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

LIBRARY += seqSoftIoc

DBD += seqSoftIoc.dbd
seqSoftIoc_DBD += base.dbd

seqSoftIoc_SRCS += seqSoftIoc_registerRecordDeviceDriver.cpp

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

%Ioc.t: %$(EXE)
	@$(RM) $@
	@$(PERL) ../makeTestfile.pl $@ $* $<

%.t: %$(EXE)
	@$(RM) $@
	@$(PERL) ../makeTestfile.pl $@ $* $<
