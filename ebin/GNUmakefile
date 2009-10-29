
TOP = ..
SUBDIRS = env0 # rconsoled init client test_ffs hsh 

# Build INITPROG even if it is not listed in SUBDIRS
ifndef INITPROG
INITPROG = rconsoled
endif # !INITPROG
ifneq ($(filter-out $(SUBDIRS), $(INITPROG)),)
SUBDIRS += $(INITPROG)
endif # INITPROG not in SUBDIRS

include $(TOP)/GNUmakefile.global

include:: all
