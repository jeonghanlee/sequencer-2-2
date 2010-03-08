TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#USE_LEMON = 1

#  Internal debug control
#USR_CPPFLAGS = -DDEBUG

#  YACC options (generate y.tab.h; generate y.output)
YACCOPT	= -d -v

#  snc product
PROD_HOST = snc
ifdef USE_LEMON
PROD_HOST += lemon
endif

snc_LIBS += $(EPICS_BASE_IOC_LIBS)
ifdef USE_LEMON
USR_CFLAGS += -DUSE_LEMON
snc_SRCS += snc_lex.l snl.c
else
snc_SRCS += snc.c
endif
snc_SRCS += snc_main.c parse.c phase2.c
snc_SRCS += gen_ss_code.c gen_tables.c sncVersion.c

#  Lex library
snc_SYS_LIBS_WIN32    += -nil-
snc_SYS_LIBS_cygwin32 += -nil-
snc_SYS_LIBS_Linux    += fl
snc_SYS_LIBS_DEFAULT  += l

ifdef USE_LEMON
lemon_SRCS = lemon.c

LEMON = $(INSTALL_BIN)/lemon
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#  The generated lex file includes snc.h
#snc_lex.c: snc.h ../snc_lex.l
snc$(OBJ): snc_lex.c

#  How to generate the version file
sncVersion.c: $(TOP)/configure/CONFIG
	$(RM) $@
	$(PERL) ../sncVersion.pl $(SEQ_VERSION) > $@

clean::
	@$(RM) snc.c snc_lex.c

ifdef USE_LEMON
snl.c snl.h: snl.lem snl.lt $(LEMON)
	$(LEMON) snl.lem

%.lem: ../%.lem
	$(CP) $< .

%.lt: ../%.lt
	$(CP) $< .

snc_lex$(OBJ) snc_lex.c: snl.h
else
snc.c: snc_lex.c
endif
