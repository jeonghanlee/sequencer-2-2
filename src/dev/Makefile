TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Include files
INC =

#  Include files
USR_CPPFLAGS = -I../../seq

#  Internal debug control
#USR_CPPFLAGS += -DDEBUG=TRUE

#  seqdev library
LIBRARY_DEFAULT = seqdev
LIBRARY_WIN32 = -nil-

LIBSRCS = dev.c

#  Device support declarations
DBD = seq.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# $(INSTALL_DBD)/seq.dbd needed by test/demo depends
depends: $(INSTALL_DBDS)
