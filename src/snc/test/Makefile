TOP = ../../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

SNCFLAGS = +m

PROD_LIBS += Com
#seq_DIR = $(INSTALL_LIB)

PROD_LIBS += seq
seq_DIR = $(INSTALL_LIB)

PROD += simple
PROD += control
PROD += demo
PROD += johng
PROD += local
#PROD += misplacedExit
PROD += sncDelay
PROD += sncEntry
PROD += sncEntryOpte
PROD += sncEntryVar
PROD += sncExEntry
PROD += sncExOpt
PROD += sncExOpt_DuplOpt
PROD += sncExOpt_UnrecOpt
PROD += sncExample
PROD += sncExitOptx
PROD += sncOptt
PROD += sncOpttVar
#PROD += tcsSeq

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
