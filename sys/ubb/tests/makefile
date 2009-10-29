CC=gcc
CFLAGS= -I../../ubb -I../../../sys -DEXOPC

LIBS=
TST=tst
HDRS=
LIBS= ../libxn.a 

OBJS= test.o ffs.o spec.o 
SRC= $(OBJS:.o=.c)

ALL= subdirs xn # client
all: $(ALL)  test

../libxn.a:  
	$(MAKE) -C ../ libxn.a

subdirs:
	$(MAKE) -C ../

xn: $(OBJS) $(LIBS) 
	$(CC) $(CFLAGS) -o xn $(OBJS) $(LIBS)

test:
	./xn

depend:
	makedepend $(CFLAGS) $(SRC) 

clean:
	rm -f *.o  $(ALL)  vd
# DO NOT DELETE

test.o: /usr/include/stdlib.h /usr/include/machine/ansi.h
test.o: /usr/include/sys/types.h /usr/include/machine/types.h
test.o: /usr/include/sys/cdefs.h /usr/include/machine/cdefs.h
test.o: /usr/include/machine/endian.h /usr/include/memory.h
test.o: /usr/include/string.h /usr/include/stddef.h ../xn.h
test.o: ../kernel/virtual-disk.h ../kernel/disk.h ffs.h ../lib/demand.h
test.o: /usr/include/stdio.h ../root-catalogue.h
ffs.o: ../xn-struct.h ../xn.h /usr/include/stdlib.h
ffs.o: /usr/include/machine/ansi.h /usr/include/sys/types.h
ffs.o: /usr/include/machine/types.h /usr/include/sys/cdefs.h
ffs.o: /usr/include/machine/cdefs.h /usr/include/machine/endian.h ../ubb.h
ffs.o: ../mv.h ../ubb-inst.h ../uops.h ../uop-classes.h /usr/include/stddef.h
ffs.o: ../kernel/virtual-disk.h /usr/include/memory.h /usr/include/string.h
ffs.o: ../kernel/disk.h ../lib/demand.h /usr/include/stdio.h ../lib/ubb-lib.h
ffs.o: ffs.h ../root-catalogue.h spec.h ../kernel/kernel.h ../lib/sync-disk.h
spec.o: /usr/include/string.h /usr/include/machine/ansi.h
spec.o: /usr/include/sys/cdefs.h /usr/include/machine/cdefs.h
spec.o: /usr/include/memory.h ../lib/ubb-lib.h ../lib/demand.h
spec.o: /usr/include/stdio.h /usr/include/sys/types.h
spec.o: /usr/include/machine/types.h /usr/include/machine/endian.h
spec.o: /usr/include/stdlib.h spec.h ../ubb.h ../xn.h ../mv.h ../ubb-inst.h
spec.o: ../uops.h ../uop-classes.h
