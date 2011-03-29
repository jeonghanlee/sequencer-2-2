#Makefile one level down from top of application tree
TOP = ..
include $(TOP)/configure/CONFIG

#directories in which to build
DIRS += simple
DIRS += johng
DIRS += validate
DIRS += demo

ifeq '$(EPICS_HAS_UNIT_TEST)' '1'
DIRS += unit
endif

include $(TOP)/configure/RULES_DIRS
