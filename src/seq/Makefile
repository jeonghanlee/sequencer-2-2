TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Include files
INC = seqCom.h

#  Internal debug control
#USR_CPPFLAGS = -DDEBUG=TRUE

#  seq library
LIBRARY = seq
seq_DLL_LIBS = pv Com
pv_DIR=$(INSTALL_LIB)

LIBSRCS = seq_main.c seq_task.c seq_ca.c seq_if.c seq_mac.c seq_prog.c \
	  seq_qry.c seqVersion.c seqCommands.c

LIBSRCS += $(seq_NAMS:%=%.c)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#  How to generate the version files
seqVersion.c: $(TOP)/configure/CONFIG
	$(RM) $@
	sh ../$(basename $@) $(SEQ_VERSION) > $@

