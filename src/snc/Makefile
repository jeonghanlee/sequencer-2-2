TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Internal debug control
#USR_CPPFLAGS = -DDEBUG

#  snc product
PROD_HOST = snc
PROD_HOST += lemon

snc_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CFLAGS += -DUSE_LEMON
snc_SRCS += snl.c
snc_SRCS += snl_lex.re
snc_SRCS += snc_main.c parse.c phase2.c
snc_SRCS += gen_ss_code.c gen_tables.c sncVersion.c

lemon_SRCS = lemon.c

LEMON = $(INSTALL_BIN)/lemon

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#  How to generate the version file
sncVersion.c: $(TOP)/configure/CONFIG
	$(RM) $@
	$(PERL) ../sncVersion.pl $(SEQ_VERSION) > $@

clean::
	@$(RM) snc.c snc_lex.c

snl.c snl.h: snl.lem snl.lt $(LEMON)
	$(LEMON) snl.lem

%.lem: ../%.lem
	$(CP) $< .

%.lt: ../%.lt
	$(CP) $< .

vpath %.re ..

snl_lex.c: snl.re snl.h
	re2c -o $@ $<
