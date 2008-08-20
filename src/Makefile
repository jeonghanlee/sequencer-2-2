TOP = ..
include $(TOP)/configure/CONFIG
DIRS += pv
DIRS += seq
DIRS += dev
DIRS += snc

seq_DEPEND_DIRS = pv
dev_DEPEND_DIRS = seq
snc_DEPEND_DIRS = seq
include $(TOP)/configure/RULES_DIRS
