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

upload:
	rsync -r -t $(TOP)/html/ wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/
	darcs push wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/repo
	darcs dist -d seq-snapshot-`date -I`
	rsync seq-snapshot-* wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/releases/

.PHONY: install-docs
