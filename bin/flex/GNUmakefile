TOP=../..

all: do-real-make 

install: all install-flex

install-flex: flex
	install -d $(EXODEST)/usr/bin
	install -c flex $(EXODEST)/usr/bin
	install -d $(EXODEST)/usr/lib
	install -c -m 644 libfl.a $(EXODEST)/usr/lib/libfl.a
	install -d $(EXODEST)/usr/include
	install -c -m 644 FlexLexer.h $(EXODEST)/usr/include/FlexLexer.h

do-real-make:
	gmake -f Makefile TOP=$(TOP) all

clean:
	gmake -f Makefile TOP=$(TOP) clean
	rm -f *~
