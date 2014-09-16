TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS  = configure

DIRS += test
test_DEPEND_DIRS = src

DIRS += examples
examples_DEPEND_DIRS = src

BRANCH = 2-2
DEFAULT_REPO = rcsadm@repo.acc.bessy.de:/opt/repositories/controls/darcs/epics/support/seq/branch-$(BRANCH)
SEQ_PATH = www/control/SoftDist/sequencer-$(BRANCH)
USER_AT_HOST = wwwcsr@www-csr.bessy.de
DATE = $(shell date -I)
SNAPSHOT = seq-$(BRANCH)-snapshot-$(DATE)
SEQ_TAG = seq-$(subst .,-,$(SEQ_RELEASE))
SEQ_TAG_TIME := $(shell darcs changes --all --xml-output \
	--matches 'exact "TAG $(SEQ_TAG)"' | perl -ne 'print "$$1.$$2" if /date=.(\d{12})(\d{2})/')

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
	darcs push --repo=$(DEFAULT_REPO) -a $(USER_AT_HOST):$(SEQ_PATH)/repo/branch-$(BRANCH)

snapshot: upload_repo
	darcs dist -d $(SNAPSHOT)
	rsync $(SNAPSHOT).tar.gz $(USER_AT_HOST):$(SEQ_PATH)/releases/
	ssh $(USER_AT_HOST) 'cd $(SEQ_PATH)/releases && ln -f -s $(SNAPSHOT).tar.gz seq-$(BRANCH)-snapshot-latest.tar.gz'
	$(RM) $(SNAPSHOT).tar.gz

release: upload_docs upload_repo
	darcs show files | xargs touch -t $(SEQ_TAG_TIME)
	darcs dist -d seq-$(SEQ_RELEASE) -t '^$(SEQ_TAG)$$'
	rsync seq-$(SEQ_RELEASE).tar.gz $(USER_AT_HOST):$(SEQ_PATH)/releases/
	$(RM) seq-$(SEQ_RELEASE).tar.gz

.PHONY: html docs docs.clean upload_docs upload_repo snapshot release
