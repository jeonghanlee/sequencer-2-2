TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS  = configure

DIRS += test
test_DEPEND_DIRS = src

DIRS += examples
examples_DEPEND_DIRS = src

BRANCH = branch-2-2
DEFAULT_REPO = /opt/repositories/controls/darcs/epics/support/seq/$(BRANCH)
SEQ_PATH = www/control/SoftDist/sequencer
USER_AT_HOST = wwwcsr@www-csr.bessy.de
DATE = $(shell date -I)
SNAPSHOT = seq-$(BRANCH)-snapshot-$(DATE)
SEQ_TAG = $(subst .,-,$(SEQ_RELEASE))

include $(TOP)/configure/RULES_TOP

html: src
	$(MAKE) -C documentation

docs: src
	$(MAKE) -C documentation pdf=1

docs.clean:
	$(MAKE) -C documentation clean

realclean clean: docs.clean

upload_docs: docs
	rsync -r -t $(TOP)/html/ $(USER_AT_HOST):$(SEQ_PATH)/

upload_repo:
	darcs push $(DEFAULT_REPO)
	darcs push --repo=$(DEFAULT_REPO) -a $(USER_AT_HOST):$(SEQ_PATH)/repo/$(BRANCH)

snapshot: upload_repo
	darcs dist -d $(SNAPSHOT)
	rsync $(SNAPSHOT).tar.gz $(USER_AT_HOST):$(SEQ_PATH)/releases/
	ssh $(USER_AT_HOST) 'cd $(SEQ_PATH)/releases && ln -f -s $(SNAPSHOT).tar.gz seq-snapshot-latest.tar.gz'
	$(RM) $(SNAPSHOT).tar.gz

release: upload_docs upload_repo
	darcs dist -d seq-$(SEQ_RELEASE) -t '^seq-$(SEQ_TAG)$$'
	rsync seq-$(SEQ_RELEASE).tar.gz $(USER_AT_HOST):$(SEQ_PATH)/releases/
	$(RM) seq-$(SEQ_RELEASE).tar.gz

.PHONY: html docs docs.clean upload_docs upload_repo snapshot release
