TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS  = configure

DIRS += test
test_DEPEND_DIRS = src

DIRS += examples
examples_DEPEND_DIRS = src

ifdef docs
DIRS += documentation
documentation_DEPEND_DIRS = src
endif

include $(TOP)/configure/RULES_TOP

install-docs:
	rsync -r -t $(TOP)/html/* wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer
	rsync -r -t /opt/repositories/controls/darcs/epics/support/seq/ wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/repo/

.PHONY: install-docs
