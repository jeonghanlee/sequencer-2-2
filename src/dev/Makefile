TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CPPFLAGS = -I../../seq

OBJS_IOC += devSequencer

#  Device support declarations
DBD = devSequencer.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# $(INSTALL_DBD)/seq.dbd needed by test/demo depends
depends: $(INSTALL_DBDS)
