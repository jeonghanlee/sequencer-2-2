TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------
#  Message system-independent support

INC     += pv.h pvAlarm.h

LIBRARY += pv
pv_SRCS += pvNew.cc pv.cc

#----------------------------------------
#  KTL (Keck Task Library) supported? (never under VxWorks)
ifeq "$(PVKTL)" "TRUE"

USR_CPPFLAGS_vxWorks += -nil-
USR_CPPFLAGS_DEFAULT += -DPVKTL
USR_INCLUDES_vxWorks += -nil-
USR_INCLUDES_DEFAULT += -I$(KROOT)/rel/default/include

INC                  += pvKtl.h pvKtlCnv.h

LIBRARY              += pvKtl
pvKtl_SRCS_vxWorks   += -nil-
pvKtl_SRCS_DEFAULT   += pvKtl.cc pvKtlCnv.cc

endif

#----------------------------------------
#  CA (Channel Access) supported? also link into pvLibrary under VxWorks
ifeq "$(PVCA)" "TRUE"

USR_CPPFLAGS      += -DPVCA

INC               += pvCa.h

LIBRARY           += pvCa
pv_SRCS_vxWorks   += pvCa.cc
pvCa_SRCS_DEFAULT += pvCa.cc

endif

#----------------------------------------
#  FILE (Demo File... from the manual) supported?
ifeq "$(PVFILE)" "TRUE"

USR_CPPFLAGS  += -DPVFILE

INC           += pvFile.h

LIBRARY       += pvFile
pvFile_SRCS   += pvFile.cc

endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

