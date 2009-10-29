TOP = ..
DOINSTALL = yes
INSTALLPREFIX := usr/include
include $(TOP)/GNUmakefile.global

P = $(EXODEST)/$(INSTALLPREFIX)
define include-rule
	@dir=IDIR ; bdir=`echo $$dir | sed "s/[^/]*\///"` ; \
		echo Installing includes from $$dir ; \
		cd $(TOP)/$$dir \
		&& install -d $P/$$bdir \
		&& install -c -m 0444 *.h $P/$$bdir/
endef

install::
	$(include-rule:IDIR=include/.)
	$(include-rule:IDIR=include/arpa)
	$(include-rule:IDIR=include/vos)
	$(include-rule:IDIR=include/vos/net)
	$(include-rule:IDIR=include/vos/dpf)
	$(include-rule:IDIR=include/exos)
	$(include-rule:IDIR=include/exos/net)
	$(include-rule:IDIR=include/exos/netinet)
	$(include-rule:IDIR=include/machine)
	$(include-rule:IDIR=include/net)
	$(include-rule:IDIR=include/netinet)
	$(include-rule:IDIR=include/netipx)
	$(include-rule:IDIR=include/netiso)
	$(include-rule:IDIR=include/netns)
	$(include-rule:IDIR=include/protocols)
	$(include-rule:IDIR=include/rpc)
	$(include-rule:IDIR=include/rpcsvc)
	$(include-rule:IDIR=include/sys)
	$(include-rule:IDIR=include/ufs/ffs)
	$(include-rule:IDIR=include/vm)
	$(include-rule:IDIR=include/wafer)
	$(include-rule:IDIR=sys/dpf)
	$(include-rule:IDIR=sys/xok)
	$(include-rule:IDIR=sys/xok_include)
	$(include-rule:IDIR=sys/xok_include/net)
	$(include-rule:IDIR=sys/machine)
	$(include-rule:IDIR=sys/ubb)
	$(include-rule:IDIR=lib/xio)
