#
# $Id: Makefile,v 1.1.1.1 2000-04-04 03:23:15 wlupton Exp $
#

TOP=..

include $(TOP)/configure/CONFIG

TARGETS = CONFIG_APP_INCLUDE

include $(TOP)/configure/RULES

ifeq ($(wildcard $(TOP)/configure/RELEASE.$(HOST_ARCH)),$(TOP)/configure/RELEASE.$(HOST_ARCH))
CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE.$(HOST_ARCH)
endif

CONFIG_APP_INCLUDE: $(TOP)/configure/RELEASE $(TOP)/configure/CONFIG_APP
	$(RM) $@
	@$(PERL) $(TOOLS)/makeConfigAppInclude.pl $(T_A) $@ $(TOP)

