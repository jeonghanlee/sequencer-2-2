#Makefile at top of application tree
TOP = .
include $(TOP)/config/CONFIG_APP

#directories in which to build
DIRS += src test

include $(TOP)/config/RULES_TOP
