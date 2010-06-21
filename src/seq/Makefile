TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Include files
INC += seqCom.h
INC += seqVersion.h

#  Internal debug control
#USR_CPPFLAGS = -DDEBUG=TRUE

#  seq library
LIBRARY = seq
seq_LIBS = pv Com

LIBSRCS = seq_main.c seq_task.c seq_ca.c seq_if.c seq_mac.c seq_prog.c \
	  seq_qry.c seqCommands.c

LIBSRCS += $(seq_NAMS:%=%.c)

# For R3.13 compatibility only
OBJLIB_vxWorks=seq
OBJLIB_SRCS = $(LIBSRCS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#  How to generate the version file
$(COMMON_DIR)/seqVersion.h: $(TOP)/configure/CONFIG ../seqVersion.pl
	$(RM) $@
	$(PERL) ../seqVersion.pl $(SEQ_VERSION) > $@
