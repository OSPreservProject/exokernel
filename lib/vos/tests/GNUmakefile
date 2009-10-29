# libvos does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../..

DBG = -g -O2
CWD = $(TOP)/lib/vos/tests
CRT0 = $(TOP)/lib/vos/obj/crt0.o
LIBVOS = $(TOP)/lib/vos/obj/libvos.a 
LIBC = $(TOP)/lib/vos/voslibc/obj/libc.a
LIBS = $(LIBVOS) $(LIBC) $(LIBVOS)
CL = $(CC) -static -N -Ttext 800020 -nostdlib 

OBJDIR = obj
SRCFILES += fork.c fopen.c msgshm.c msgring.c smp_pi.c pi.c \
	ipcping.c ipc.c ipcport.c udp.c tcp.c tcpsrv.c \
	locks.c basic.c simple.c rsync.c cksd.c cksd_2.c \
	klock.c

EXTRATRG = ipc msgshm msgring ipcping fork ipcport fopen cksd cksd_2 \
	udp tcp tcpsrv locks basic simple pi smp_pi rsync klock

EXTRACLN = $(EXTRATRG)
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = /usr/local/vos/test

DEFS=-DNOTEXOS

include $(TOP)/GNUmakefile.global


rsync: rsync.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) rsync.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

cksd_2: cksd_2.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) cksd_2.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

cksd: cksd.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) cksd.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

klock: klock.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) klock.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

locks: locks.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) locks.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

ipcping: ipcping.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) ipcping.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

msgshm: msgshm.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) msgshm.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

msgring: msgring.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) msgring.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

smp_pi: smp_pi.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) smp_pi.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

pi: pi.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) pi.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

ipc: ipc.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) ipc.o $(LIBS) 
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

fork: fork.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) fork.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

ipcport: ipcport.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) ipcport.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

udp: udp.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) udp.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

tcpsrv: tcpsrv.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) tcpsrv.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

tcp: tcp.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) tcp.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

basic: basic.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) basic.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

simple: simple.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) simple.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

fopen: fopen.o $(CRT0) $(LIBS)
	$(CL) -o $@ $(CRT0) fopen.o $(LIBS)
	cp $@ $@.aout
	$(TOP)/tools/setmagic/setmagic $@ 700

