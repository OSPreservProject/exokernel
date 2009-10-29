
VPATH += $(VOS)/voslibc/stdlib
SRCFILES += a64l.c abort.c atexit.c atoi.c atof.c atol.c bsearch.c calloc.c \
	cfree.c exit.c getenv.c getopt.c getsubopt.c heapsort.c l64a.c \
	merge.c multibyte.c putenv.c radixsort.c rand.c \
	random.c realpath.c setenv.c strtod.c strtol.c strtoq.c strtoul.c \
	strtouq.c system.c tfind.c tsearch.c \
	_rand48.c drand48.c erand48.c jrand48.c lcong48.c lrand48.c \
	mrand48.c nrand48.c seed48.c srand48.c qabs.c qdiv.c \
	abs.S div.S labs.S ldiv.S

# took out: malloc.c
