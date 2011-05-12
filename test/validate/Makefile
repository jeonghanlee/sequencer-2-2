TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m -i
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
TESTPROD_HOST += decl
TESTPROD_HOST += local
TESTPROD_HOST += pvPutAsync
TESTPROD_HOST += pvSync
TESTPROD_HOST += sncDelay
TESTPROD_HOST += sncEntry
TESTPROD_HOST += sncEntryOpte
TESTPROD_HOST += sncEntryVar
TESTPROD_HOST += sncExitOptx
TESTPROD_HOST += sncOptt
TESTPROD_HOST += sncOpttVar
TESTPROD_HOST += stop
TESTPROD_HOST += subscript
TESTPROD_HOST += syncq
TESTPROD_HOST += userfunc

REGRESSION_TESTS_WITH_DB += bittypes
REGRESSION_TESTS_WITH_DB += evflag
REGRESSION_TESTS_WITH_DB += norace
REGRESSION_TESTS_WITH_DB += pvGet

# uncomment this test to see race
# fail (safe mode off)
#REGRESSION_TESTS_WITH_DB += race

REGRESSION_TESTS_WITHOUT_DB += array
REGRESSION_TESTS_WITHOUT_DB += assign
REGRESSION_TESTS_WITHOUT_DB += change
REGRESSION_TESTS_WITHOUT_DB += safeMonitor

REGRESSION_TESTS += $(REGRESSION_TESTS_WITHOUT_DB)
REGRESSION_TESTS += $(REGRESSION_TESTS_WITH_DB)

ifeq '$(EPICS_HAS_UNIT_TEST)' '1'
TESTPROD_HOST += $(REGRESSION_TESTS)
TESTSCRIPTS_HOST += $(REGRESSION_TESTS:%=%.t)
TESTSCRIPTS_HOST += $(REGRESSION_TESTS_WITH_DB:%=%Ioc.t)
endif

#TESTPROD_HOST += ctest

#  Libraries
PROD_LIBS += seqSoftIoc
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

LIBRARY += seqSoftIoc

DBD += seqSoftIoc.dbd
seqSoftIoc_DBD += base.dbd

seqSoftIoc_SRCS += seqSoftIoc_registerRecordDeviceDriver.cpp
seqSoftIoc_SRCS += testSupport.c

PROD_vxWorks = vxTestHarness
vxTestHarness_SRCS += $(REGRESSION_TESTS:%=%.st)
vxTestHarness_SRCS += testSupport.c
vxTestHarness_SRCS += vxTestHarness_registerRecordDeviceDriver.cpp
vxTestHarness_OBJS += $(EPICS_BASE_BIN)/vxComLibrary

DBD += vxTestHarness.dbd
vxTestHarness_DBD += base.dbd
vxTestHarness_DBD += $(COMMON_DIR)/vxTestHarnessRegistrars.dbd

USR_DBDFLAGS += -I $(COMMON_DIR)

DB += vxTestHarness.db

# PROD_RTEMS = rtemsTestHarness
# rtemsTestHarness_SRCS += $(TESTPROD_HOST::%=%.st)

#PROD_RTEMS += $(TESTPROD_HOST)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

%Ioc.t: %$(EXE)
	@$(RM) $@
	@$(PERL) ../makeTestfile.pl $@ $* $<

%.t: %$(EXE)
	@$(RM) $@
	@$(PERL) ../makeTestfile.pl $@ $* $<

norace.o race.o: ../raceCommon.st

$(COMMON_DIR)/vxTestHarnessRegistrars.dbd:
	$(RM) $@
	$(foreach t,$(REGRESSION_TESTS),echo 'registrar($tTestRegistrar)' >> $@;)

$(COMMON_DIR)/vxTestHarness.dbd: $(COMMON_DIR)/vxTestHarnessRegistrars.dbd

$(COMMON_DIR)/vxTestHarness.db: $(REGRESSION_TESTS_WITH_DB:%=../%.db)
	$(RM) $@
	$(PERL) -ne print $(REGRESSION_TESTS_WITH_DB:%=../%.db) > $@

st.cmd: ../st.cmd.vxWorks
	$(CP) $< $@

ifeq "$(OS_CLASS)" "vxWorks"
build: st.cmd
endif
