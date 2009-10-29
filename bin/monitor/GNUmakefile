TOP = ../..
PROG = monitor
OBJDIR = obj
SRCFILES = monitor.c cpu.c handler.c init.c port.c ports.c emu.c int10.c dma.c portss.c int.c vc.c dump.c disks.c pic.c i386.c pagetable.c uidt.c memutil.c vars.c handler_utils.c pagefault.c console.c config.c test_trap.c debugger.c memcheck.c dev_list.c cmos.c rtc.c timers.c n_serv_8042.c n_serv_backend.c descriptors.c set.c logue.c entry.S repeat.c tasks.c decode.c
VPATH += video
SRCFILES += s3.c trident.c et4000.c avance.c ati.c cirrus.c video.c vga.c console.c
DEFS += -Wuninitialized -Wunused -DNIOPORTS=0x400 -DNEW_KBD_CODE -DEXO_HOST -DMULTIPLE_GUEST
# -DTRACE_LOGUE -DNDEBUG
EXTRAINC = -I$(TOP)/bin/monitor/include -I$(TOP)/include
EXTRAOBJ =

INSTALLPREFIX=usr/bin
export DOINSTALL=yes

include $(TOP)/GNUmakefile.global
