TOP=../..

include $(TOP)/ARCH

ifdef LINUX
CROSS=/usr/local/openbsd-cross/bin/
else
CROSS =
endif 

STRIP_PROG = $(CROSS)strip

all: do-real-make 

install: all install-cpp install-gcc install-cc1 install-libgcc.a

# anyone have any clue why I need these test -f foo's here? Without them
# I get this error message about ~/exobin/foo not being found

install-cpp: cpp
	mkdir -p $(EXODEST)/usr/bin
	install cpp $(EXODEST)/usr/bin/cpp
	$(STRIP_PROG) $(EXODEST)/usr/bin/cpp

install-gcc: xgcc
	mkdir -p $(EXODEST)/usr/bin
	install  xgcc $(EXODEST)/usr/bin/gcc
	$(STRIP_PROG) $(EXODEST)/usr/bin/gcc
	ln -f $(EXODEST)/usr/bin/gcc $(EXODEST)/usr/bin/cc

install-cc1: cc1
	mkdir -p $(EXODEST)/usr/bin
	install cc1 $(EXODEST)/usr/bin/cc1
	$(STRIP_PROG) $(EXODEST)/usr/bin/cc1

install-libgcc.a: libgcc.a
	mkdir -p $(EXODEST)/usr/lib
	cp libgcc.a $(EXODEST)/usr/lib/libgcc.a

do-real-make:
	gmake -f Makefile TOP=$(TOP) cpp xgcc cc1 libgcc.a

clean:
	gmake -f Makefile TOP=$(TOP) clean
	rm -f l-

