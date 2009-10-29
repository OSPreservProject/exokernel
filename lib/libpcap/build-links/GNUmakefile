
TOP = ../../..
EXTRATRG=foobar

include $(TOP)/GNUmakefile.global

foobar:
	ln -sf bpf/net ../net
	ln -sf bpf/net/bpf_filter.c ../bpf_filter.c
	ln -sf lbl/gnuc.h ../gnuc.h

clean::
	rm -f ../net
	rm -f ../bpf_filter.c
	rm -f ../gnuc.h
