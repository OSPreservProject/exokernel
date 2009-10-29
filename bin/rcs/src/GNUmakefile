WFLAGS=
export WFLAGS
DOINSTALL=yes
export DOINSTALL
INSTALLPREFIX=/usr/local/bin
export INSTALLPREFIX

all:
	$(MAKE) -k -f GNUmakefile.ci
	$(MAKE) -k -f GNUmakefile.co
	$(MAKE) -k -f GNUmakefile.ident
	$(MAKE) -k -f GNUmakefile.merge
	$(MAKE) -k -f GNUmakefile.rcs
	$(MAKE) -k -f GNUmakefile.rcsclean
	$(MAKE) -k -f GNUmakefile.rcsdiff
	$(MAKE) -k -f GNUmakefile.rcsmerge
	$(MAKE) -k -f GNUmakefile.rlog

install:
	$(MAKE) -k -f GNUmakefile.ci install
	$(MAKE) -k -f GNUmakefile.co install
	$(MAKE) -k -f GNUmakefile.ident install
	$(MAKE) -k -f GNUmakefile.merge install
	$(MAKE) -k -f GNUmakefile.rcs install
	$(MAKE) -k -f GNUmakefile.rcsclean install
	$(MAKE) -k -f GNUmakefile.rcsdiff install
	$(MAKE) -k -f GNUmakefile.rcsmerge install
	$(MAKE) -k -f GNUmakefile.rlog install

clean:
	$(MAKE) -f GNUmakefile.ci	clean
	$(MAKE) -f GNUmakefile.co clean
	$(MAKE) -f GNUmakefile.ident clean
	$(MAKE) -f GNUmakefile.merge clean
	$(MAKE) -f GNUmakefile.rcs clean
	$(MAKE) -f GNUmakefile.rcsclean clean
	$(MAKE) -f GNUmakefile.rcsdiff clean
	$(MAKE) -f GNUmakefile.rcsmerge clean
	$(MAKE) -f GNUmakefile.rlog clean
