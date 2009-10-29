
TOP = ../..
PROG = ash.b
NOLIB = nolib
EXTRAINC = -I$(TOP)/lib/libc
LDADDR = 2000
SRCFILES = ash_asm.S ash_ae_net.c ash_disk.c ash_printf.c
DEFS = -DGANGER_ASH_HACK
EXTRATRG = ash.b.s ash.sym.s
EXTRACLN = ash.b.s ash.sym.s
EXPORT = _ash_test ___envid/___ash_envid _end/_ash_end _pkt_recv \
	_asn \
	_edata/_ash_edata _disk_done \
	_ganger_info/_ash_ganger_info \

#	_lb_fid _lb_isserver _lb_cnt _lb_req _lb_rep \
#	_lb_time_distribution


include $(TOP)/GNUmakefile.global

%.sym.s: %.b GNUmakefile
	@rm -f $@~
	$(TOP)/tools/symset/symset $< $(EXPORT) > $@~ && mv -f $@~ $@
