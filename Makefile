# Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += src
DIRS += test
include $(TOP)/configure/RULES_TOP
