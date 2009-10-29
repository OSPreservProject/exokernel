
TOP = ../..
OBJDIR = obj
SUBDIRS=build-links
EXTRAINC = -I$(CURDIR) -Ilinux-include -I$(TOP)/lib/libc -I$(TOP)/lib -I$(TOP)
LIB = libpcap.a
DEFS = -DHAVE_MALLOC_H=1 -DHAVE_ETHER_HOSTTON=1 -DHAVE_STRERROR=1 -DHAVE_NET_IF_ARP_H=1
STATIC = 1
SRCFILES = pcap-exo.c pcap.c inet.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c scanner.c grammar.c \
	version.c

include $(TOP)/GNUmakefile.global

