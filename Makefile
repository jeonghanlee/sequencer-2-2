# Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += src
DIRS += test

src_DEPEND_DIRS  = configure
test_DEPEND_DIRS = src
include $(TOP)/configure/RULES_TOP
