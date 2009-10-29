
TOP = ../..
DOINSTALL = yes
EXTRAINSTALL = dcrt0.o
SRCFILES = dyn_entry.S dyn_fault.c d_xrt0.c rtld.c ash.b.s ash.sym.s etc.c md.c mdprologue.c shlib.c sls_stub.c dyn_ipc.S dyn_pre_process.c


EXTRAINC = -I$(TOP)/lib/libc
EXTRATRG = dcrt0.o
VPATH = ../ash
VPATH += ../../bin/sls/

include $(TOP)/GNUmakefile.global

dcrt0.o: $(OBJFILES)
	$(LD) -r -o $@ $^ -nostdlib 
