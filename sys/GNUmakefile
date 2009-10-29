TOP = ..
include $(TOP)/ARCH

#ENABLE_IDE=1

#OSKIT=/home/ny2/ericp/oskit-0.97
ifdef OSKIT
OSKIT_INC=-I${OSKIT}/oskit/dev -I${OSKIT}/oskit -I${OSKIT}
OSKIT_DRIVER_LIB=${OSKIT}/linux/dev/liboskit_linux_dev.a
OSKIT_DEFS=-DOSKIT_PRESENT
else
OSKIT_INC=
OSKIT_DRIVER_LIB=
OSKIT_DEFS=
endif #OSKIT


CC_VER = -V 2.8.1
OBJDIR = obj
PROG = xok.gdb
SUBDIRS = conf 
#SUBDIRS += ubb
ifndef INITPROG
INITPROG = rconsoled
endif
DEFS = -DKERNEL ${OSKIT_DEFS}
DEFS += # -DDUMPFILT -DKDEBUG -DXN
NOLIB = yes

ifdef ENABLE_IDE
DEFS += -DENABLE_IDE
endif

system = `uname`

# for some reason when you say make the entry point FOO, OpenBSD really makes it
# FOO+0x20, probably to account for the a.out header. Linux and GNU ld on
# OpenBSD does not do this.

ifdef LINUX
LOAD_DIRECTIVE = -Ttext F0100020
endif
ifdef FREEBSD
LOAD_DIRECTIVE = -Ttext F0100000
endif
ifdef OPENBSD
ifdef GNULD
LOAD_DIRECTIVE = -Ttext F0100020
else
LOAD_DIRECTIVE = -Ttext F0100000
endif
endif

ifdef FREEBSD
Z = -Z
endif
ifdef LINUX
Z = -z
endif
ifdef OPENBSD
Z = -z
endif

# might have to punt -dc -dp for freebsd too...
LDFLAGS = $(DBG) -dc -dp -static $(Z) $(LOAD_DIRECTIVE) -nostdlib -e start

EXTRAINC = ${OSKIT_INC}
INC = -nostdinc -I$(TOP)/sys -I. $(EXTRAINC)
EXTRATRG = xok
EXTRACLN = $(OBJDIR)/xok $(OBJDIR)/initprog.h $(OBJDIR)/vers.s \
	$(OBJDIR)/xok.osid
UNUSEDOK = 
UNINITOK = 

VPATH += kern
SRCFILES += locore.S init.c capability.c env.c idt.c kclock.c \
            malloc.c pmap.c printf.c sched.c sys_sctab.c \
	    smp.c smp_sys.c apic.c mplock.c ash.c ipi.c msgring.c \
            syscall.c vector.s pkt.c disk.c wk.c pxn.c bc.c \
            loopback.c pktring.c reboot.S pctr.c vcopy.c batch.c \
	    kdebug.c i386-stub.c debug.S smptramp.S perf.c \
	    partition.c micropart.c ipc.c kstrerror.c picirq.c driver_table.c

# SRCFILES += fsprot.c

ifdef OSKIT
VPATH += oskit
SRCFILES += osenv_imp.c oskit_init.c oskit_dump.c oskit_iunknown_iid.c \
            oskit_netio_iid.c oskit_bufio_iid.c oskit_isa_driver_iid.c \
            oskit_netdev_iid.c oskit_device_iid.c oskit_blkdev_iid.c \
            oskit_etherdev_iid.c oskit_driver_iid.c oskit_bus_iid.c \
            oskit_blkio_iid.c oskit_idebus_iid.c device_register.c\
            osenv_mem_imp.c osenv_intr_imp.c register.c driver_register.c \
            dma.c probe.c root.c isa_bus.c timer_pit.c osenv_timer_imp.c \
            oskit_isabus_iid.c io.c pcibus.c oskit_pci.c net_io.c \
                      sleep.c xok_net_interface.c buf_io_extern.c
endif #OSKIT

ifdef EXO_PAM
SRCFILES += pam.c
endif


NOOPT    += i386-stub.c kdebug.c
UNUSEDOK += i386-stub.c kdebug.c wk.c
UNINITOK += wk.c

VPATH += dev
SRCFILES += console.c bios32.c pci.c scsi_base.c scsiconf.c ncr.c \
	sd.c if_de.c if_de_init.c if_ed.c ata-all.c ata-disk.c ata-dma.c# smc.c lb.c

VPATH += i386/isa
SRCFILES += clock.c
BADFORMATOK += clock.c

VPATH += conf/obj
SRCFILES += vector.s idt.c sys_sctab.c

VPATH += dpf
SRCFILES += dpf-ir.c dpf.c xlate.c output.c consistency.c pid.c \
	 dpf-ir-print.c optimize.c sysdpf.c fast-interp.c action.c
# interp.c gen.c
UNUSEDOK += gen.c

VPATH += xoklibc/string
SRCFILES += bcopy.S bzero.S memcpy.S memset.S strcpy.S strlen.S strncmp.S \
	memmove.S strcat.S memcmp.S ffs.S bcmp.S strncpy.c

VPATH += xoklibc/stdlib
SRCFILES += qsort.c rand.c
UNINITOK += qsort.c

VPATH += xoklibc/gen
SRCFILES += ctype_.c toupper_.c tolower_.c

VPATH += vcode
SRCFILES += vcode.c dis.c
UNUSEDOK += vcode.c


#VPATH += ubb
#SRCFILES += bootstrap.c check.c dependency.c init.c interp.c iov.c mv.c \
#	registry.c root-catalogue.c template.c type.c ubb-dis.c udf.c xn.c \
#	xn-sys.c rules.c

#VPATH += ubb/lib
#SRCFILES += set.c bit.c arena.c table.c ubb-lib.c

#VPATH += ubb/kernel
#SRCFILES += real-kernel.c bc.c

VPATH += xok	# For cleaning

EXTRAOBJ = env0.b.o $(INITPROG).b.o vers.o ${OSKIT_DRIVER_LIB} # ubb/obj/libudf.a

ID := $(patsubst %/sys/obj, %, $(CURDIR))
ID := $(shell echo "$${USER-root}@`hostname`:$(ID)")
ID := $(ID) ($(shell date +%m/%d/%y-%H:%M:%S)).
OSID := $(ID)
ID := .globl _osid; _osid: .asciz "$(ID)"

PRE_LINK = echo '$(ID)' | $(AS) -o vers.o ; rm -f xok.osid ; \
	echo -n '$(OSID)' > xok.osid
#POST_LINK = cp xok xok.gdb && strip -x xok

DOINSTALL=yes
INSTALLPREFIX=boot
EXTRAINSTALLPREFIX=boot
EXTRAINSTALL=xok xok.osid

include $(TOP)/GNUmakefile.global

##  Get embedded binaries from ebin directory
vpath %.b.s $(patsubst %.b.o, $(TOP)/ebin/%, $(EXTRAOBJ))

vers.o:
	@touch $@

vers.s: FORCE
	 @test 'X$(ID)' = 'X$(OLDID)' \
		|| (rm -f $@~; set -x; \
			echo '$(ID)' > $@~; mv -f $@~ $@)

PROGDEF := \#define INITPROG $(INITPROG)
OLDPROGDEF := $(shell cat initprog.h 2> /dev/null)
init.o: initprog.h
initprog.h: FORCE
	 @test 'X$(PROGDEF)' = 'X$(OLDPROGDEF)' \
		|| (rm -f $@~; set -x; \
			echo '$(PROGDEF)' > $@~; mv -f $@~ $@)
