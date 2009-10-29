TOP = ../..

ifdef LINUX
CC_VER =
endif
ifdef OPENBSD
CC_VER = -V 2.8.1
endif

OBJDIR = obj
AUXPROGS = genvec gensys
AUXDEP = sys_syms.h
AUXDEFS = -DEXOPC
AUXINC = -I$(TOP)/sys
AUXCSRCS += ../src/gensyms.c ../src/gensyms-exopc.c
SYSCALLCONF = syscall.conf

VPATH = src
AUTOINC = sys_proto.h syscallno.h sys_ucall.h sys_ubatch.h batch.c \
	kerncallstr.c sys_syms.h
AUTOFILES = vector.s idt.c sys_sctab.c $(AUTOINC)
EXTRACLN = $(AUTOFILES) $(addprefix $(TOP)/sys/xok/, $(AUTOINC)) \
	$(AUXPROGS) gensyms-exopc
EXTRATRG = $(AUTOFILES)

include $(TOP)/GNUmakefile.global

define gensyms-rule
	@rm -f $@ $@~
	./gensyms-exopc > $@~ || (rm -f $@~; exit 1)
	mv $@~ $@
endef
define gensys-rule
	rm -f $@ $@~
	./gensys FLAG $< > $@~ || (rm -f $@~; exit 1)
	mv $@~ $@
endef
define install-include
	ln -fs $(CURDIR)/$(OBJDIR)/$@ $(TOP)/sys/xok/
endef

vector.s idt.c: trap.conf genvec
	./genvec $<

gensyms.o: ../src/gensyms.c
	$(NCC) $(DEFS) $(EXTRADEFS) -MD -c -g -O $(WFLAGS) $(WARNOK_F) -DEXOPC \
		../src/gensyms.c

gensyms-exopc.o: ../src/gensyms-exopc.c
	$(NCC) -I$(TOP)/include -I$(TOP)/sys -I$(TOP)/lib/libexos \
		$(DEFS) $(EXTRADEFS) -MD -c -g -O $(WFLAGS) $(WARNOK_F) -DEXOPC \
		../src/gensyms-exopc.c

gensyms-exopc: gensyms-exopc.o gensyms.o
	$(NCC) $(DEFS) $(EXTRADEFS) -MD -o gensyms-exopc gensyms.o gensyms-exopc.o

sys_syms.h: gensyms-exopc
	$(gensyms-rule)
	$(install-include)

sys_sctab.c: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-t)
sys_proto.h: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-p)
	$(install-include)
sys_ucall.h: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-u)
	$(install-include)
kerncallstr.c: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-s)
	ln -fs $(CURDIR)/$(OBJDIR)/$@ $(TOP)/sys/kern/
	ln -fs $(CURDIR)/$(OBJDIR)/$@ $(TOP)/lib/libexos/os/
syscallno.h: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-h)
	$(install-include)
sys_ubatch.h: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-b)
	$(install-include)
batch.c: $(SYSCALLCONF) gensys
	$(gensys-rule:FLAG=-d)
	ln -fs $(CURDIR)/$(OBJDIR)/$@ $(TOP)/sys/kern/

