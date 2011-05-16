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

SEQ_PATH = www/control/SoftDist/sequencer
USER_AT_HOST = wwwcsr@www-csr.bessy.de

include $(TOP)/configure/RULES_TOP

upload:
	rsync -r -t $(TOP)/html/ $(USER_AT_HOST):$(SEQ_PATH)/
	darcs push $(USER_AT_HOST):$(SEQ_PATH)/repo
	darcs dist -d seq-snapshot-`date -I`
	rsync seq-snapshot-* $(USER_AT_HOST):$(SEQ_PATH)/releases/
	ssh $(USER_AT_HOST) 'cd $(SEQ_PATH)/releases && ln -f -s seq-snapshot-`date -I`.tar.gz seq-snapshot-latest.tar.gz'

.PHONY: install-docs
