#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += src
DIRS += test
include $(TOP)/configure/RULES_TOP

# Override "tar" rules with rule that runs tar from one level up, generates
# sub-directory with version number in it, and gzips the result (this rule
# is not a replacement for the "tar" rule; it does not support .current_rel_-
# hist and EPICS_BASE files; it does not handle files in the top-level
# directory well)
tar:
	@MODULE=$(notdir $(shell pwd)); \
	TARNAME=$$MODULE-$(SEQ_VERSION); \
	TARFILE=$$MODULE/$$TARNAME.tar; \
	echo "TOP: Creating $$TARNAME.tar file..."; \
	cd ..; $(RM) $$TARNAME; ln -s $$MODULE $$TARNAME; \
	ls $$TARNAME/READ* $$TARNAME/Makefile* | xargs tar vcf $$TARFILE; \
	for DIR in ${DIRS} docs; do    \
		find $$TARNAME/$$DIR -name CVS -prune -o ! -type d -print \
		| grep -v "/O\..*$$" | grep -v /fm | grep -v /anl | \
		xargs tar vrf $$TARFILE; \
	done; \
	gzip -f $$TARFILE; \
	$(RM) $$TARNAME

