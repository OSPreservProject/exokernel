TOP = ../..
PROG = tcpdump

SRCFILES = tcpdump.c \
	print-arp.c print-atalk.c print-atm.c print-bootp.c \
	print-decnet.c print-domain.c print-dvmrp.c print-egp.c \
	print-ether.c print-fddi.c print-gre.c print-icmp.c \
	print-igrp.c print-ip.c print-ipx.c print-isoclns.c print-krb.c \
	print-llc.c print-nfs.c print-ntp.c print-null.c print-ospf.c \
	print-pim.c print-ppp.c print-raw.c print-rip.c print-sl.c \
	print-snmp.c print-sunrpc.c print-tcp.c print-tftp.c print-udp.c \
	print-wb.c addrtoname.c bpf_dump.c gmt2local.c machdep.c \
	parsenfsfh.c util.c savestr.c setsignal.c



#DEFS = Dlint
export DOINSTALL=yes
export INSTALLPREFIX=usr/sbin
EXTRAINC =  -I$(TOP)/lib/libpcap/ -Ilinux-include -I. -I$(TOP)/lib/libc
INC = -nostdinc $(EXTRAINC) -I$(TOP)/include -I$(TOP)/sys
EXTRAOBJ = $(TOP)/lib/libpcap/obj/libpcap.a
include $(TOP)/GNUmakefile.global
