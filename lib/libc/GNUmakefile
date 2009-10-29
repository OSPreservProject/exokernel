
export TOP = ../..
DOINSTALL=yes
OBJDIR = obj
EXTRAINC = -I$(CURDIR) -I$(TOP)/lib -DPRINTF_LEVEL=-1
LIB = libc.a
DEFS = -DFLOATING_POINT=1
UNINITOK =
UNUSEDOK =
STATIC=1
#DOBBPROF=yes
#DOPROF=yes

#SUBDIRS = $(TOP)/lib/libexos
#EXTRAOBJ = $(TOP)/lib/libexos/obj/*.o


# NFS3 readdir PROBLEM: readdir on NFS3 sometimes return inconsistent data
ifdef NFS3_READDIR_PROB
DEFS += -D__NFS3_READDIR_PROB__
endif



# Portable string functions
VPATH += string
SRCFILES = bm.c memccpy.c strcasecmp.c strcoll.c strdup.c strerror.c \
        strlcat.c strlcpy.c strmode.c strsignal.c strtok.c strxfrm.c \
	__strerror.c __strsignal.c

# i386 asm string functions
SRCFILES += index.S memchr.S rindex.S strcat.S \
	strcmp.S swab.S strchr.S strrchr.S

# 2nd set of portable string functions
SRCFILES += strcspn.c strncat.c strncpy.c strpbrk.c strsep.c strspn.c \
	strstr.c

# subset of string functions used by kernel
VPATH += $(TOP)/sys/xoklibc/string
SRCFILES += bcopy.S bzero.S memcpy.S memset.S strcpy.S strlen.S strncmp.S \
	memmove.S memcmp.S ffs.S bcmp.S

VPATH += compat-43
SRCFILES +=  __setreuid.c __setregid.c creat.c getdtablesize.c gethostid.c \
        getwd.c killpg.c sethostid.c setpgrp.c setregid.c setreuid.c \
        setrgid.c setruid.c sigcompat.c

VPATH += stdio
SRCFILES+= \
	asprintf.c clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
        fgetln.c fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
        fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
        fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
        getc.c getchar.c gets.c getw.c makebuf.c mktemp.c perror.c printf.c \
        putc.c putchar.c puts.c putw.c refill.c remove.c rewind.c rget.c \
        scanf.c setbuf.c setbuffer.c setvbuf.c snprintf.c sprintf.c sscanf.c \
        stdio.c tempnam.c tmpfile.c tmpnam.c ungetc.c vasprintf.c vfprintf.c \
        vfscanf.c vprintf.c vscanf.c vsnprintf.c vsprintf.c vsscanf.c \
        wbuf.c wsetup.c
UNINITOK += vfprintf.c mktemp.c
WARNOK += fgets.c fputc.c fputs.c fread.c fscanf.c fvwrite.c fwalk.c \
	getchar.c putc.c putchar.c puts.c putw.c refill.c rget.c scanf.c \
	setvbuf.c snprintf.c sscanf.c stdio.c ungetc.c vfscanf.c vprintf.c \
	vscanf.c vsnprintf.c vsprintf.c vsscanf.c wbuf.c wsetup.c stdio.c \
	tmpfile.c

VPATH += gen
UNINITOK += fts.c getcap.c
SRCFILES += \
	alarm.c assert.c basename.c clock.c closedir.c confstr.c ctermid.c \
	daemon.c devname.c dirname.c disklabel.c elf_hash.c err.c \
	errx.c errlist.c errno.c exec.c fnmatch.c fstab.c ftok.c fts.c \
	getbsize.c getcap.c getdomainname.c getgrent.c \
	getgrouplist.c gethostname.c getloadavg.c getlogin.c getmntinfo.c \
	getnetgrent.c getpagesize.c getpass.c getpwent.c \
	getttyent.c getusershell.c glob.c initgroups.c isatty.c isctype.c \
	lockf.c nice.c nlist.c opendir.c pause.c popen.c psignal.c pwcache.c \
	raise.c readdir.c rewinddir.c scandir.c seekdir.c setdomainname.c \
	sethostname.c setjmperr.c setmode.c setproctitle.c siginterrupt.c \
	siglist.c signal.c signame.c sigsetops.c sleep.c sysconf.c sysctl.c \
	syslog.c telldir.c time.c times.c timezone.c ttyname.c \
	ttyslot.c ualarm.c uname.c unvis.c usleep.c utime.c \
	valloc.c vis.c wait.c wait3.c waitpid.c warn.c warnx.c vwarn.c \
	vwarnx.c verr.c verrx.c getcwd.c
#In Kernel - ctype_.c tolower_.c toupper_.c
SRCFILES += _err.c _errx.c _sys_errlist.c _sys_nerr.c _sys_siglist.c \
	_verr.c _verrx.c _vwarn.c _vwarnx.c _warn.c _warnx.c 
SRCFILES += _setjmp.S alloca.S fabs.S frexp.c infinity.c isinf.c ldexp.c \
	modf.S setjmp.S sigsetjmp.S
SRCFILES += flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S 
SRCFILES += divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S
UNUSEDOK += getgrent.c dirname.c basename.c disklabel.c getpwent.c
UNINITOK += setmode.c
WARNOK += warn.c warnx.c err.c errx.c getbsize.c initgroups.c getnetgrent.c \
	devname.c

VPATH += $(TOP)/sys/xoklibc/gen
SRCFILES += ctype_.c tolower_.c toupper_.c

UNINITOK += getcap.c fts.c syslog.c

VPATH += net
SRCFILES += inet_addr.c inet_lnaof.c inet_makeaddr.c \
        inet_netof.c inet_network.c inet_ntoa.c \
        getservbyname.c getservent.c  \
        getproto.c getprotoent.c getprotoname.c \
        getservbyport.c rcmd.c rcmdsh.c herror.c \
        res_query.c res_send.c res_mkquery.c res_init.c \
        res_debug.c res_comp.c  \
        gethostnamadr.c getnetent.c getnetbyname.c getnetbyaddr.c \
        sethostent.c nsap_addr.c \
        base64.c ethers.c getnetnamadr.c htonl.c htons.c inet_net_ntop.c \
        inet_net_pton.c inet_neta.c inet_ntop.c inet_pton.c ipx_addr.c \
        ipx_ntoa.c iso_addr.c linkaddr.c ns_addr.c ns_ntoa.c ntohl.c ntohs.c \
        res_data.c res_random.c recv.c send.c

UNINITOK += iso_addr.c linkaddr.c
WARNOK += rcmdsh.c

VPATH += time
SRCFILES += asctime.c difftime.c localtime.c strftime.c strptime.c \
# ialloc.c scheck.c zdump.c zic.c

VPATH += locale
SRCFILES += _def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c

VPATH += termios
SRCFILES += cfgetispeed.c cfgetospeed.c cfmakeraw.c cfsetispeed.c \
	cfsetospeed.c cfsetspeed.c tcdrain.c tcflow.c tcflush.c \
	tcgetattr.c tcgetpgrp.c tcsendbreak.c tcsetattr.c tcsetpgrp.c
WARNOK+= tcsetattr.c

VPATH += hash
SRCFILES += sha1.c sha1hl.c rmd160.c rmd160hl.c

VPATH += regex
SRCFILES += regcomp.c regerror.c regexec.c regfree.c
DEFS += -DPOSIX_MISTAKE
UNINITOK += regcomp.c
UNUSEDOK += regcomp.c
WARNOK += regerror.c regexec.c

VPATH += stdlib
SRCFILES += a64l.c abort.c atexit.c atoi.c atof.c atol.c bsearch.c calloc.c \
        cfree.c exit.c getenv.c getopt.c getsubopt.c heapsort.c l64a.c \
        malloc.c merge.c multibyte.c putenv.c radixsort.c rand.c \
        random.c realpath.c setenv.c strtod.c strtol.c strtoq.c strtoul.c \
        strtouq.c system.c tfind.c tsearch.c \
        _rand48.c drand48.c erand48.c jrand48.c lcong48.c lrand48.c \
        mrand48.c nrand48.c seed48.c srand48.c qabs.c qdiv.c \
	abs.S div.S labs.S ldiv.S
UNINITOK+= strtod.c malloc.c
UNUSEDOK+= getsubopt.c

VPATH += $(TOP)/sys/xoklibc/stdlib
SRCFILES += qsort.c

DEFS+=-D__DBINTERFACE_PRIVATE
VPATH+= db/btree
SRCFILES+= bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
      bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
      bt_utils.c
UNINITOK += bt_delete.c bt_put.c bt_seq.c bt_split.c

VPATH+= db/db
SRCFILES+= db.c

VPATH+= db/hash
SRCFILES+= hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
      hash_page.c hsearch.c ndbm.c
WARNOK+= hash_buf.c
UNINITOK+= hash_page.c

VPATH+= db/mpool
SRCFILES+= mpool.c

VPATH+= db/recno
SRCFILES+= rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c \
	rec_search.c rec_seq.c rec_utils.c
UNINITOK+= rec_open.c
UNUSEDOK+= rec_seq.c

VPATH += crypt
SRCFILES += cast.c crypt.c morecrypt.c md5crypt.c arc4random.c blowfish.c \
	bcrypt.c

VPATH += nls
SRCFILES += catclose.c catgets.c catopen.c _catclose.c _catgets.c _catopen.c

VPATH += quad
SRCFILES += adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c fixdfdi.c \
        fixsfdi.c fixunsdfdi.c fixunssfdi.c floatdidf.c floatdisf.c \
        floatunsdidf.c iordi3.c lshldi3.c lshrdi3.c moddi3.c muldi3.c \
        negdi2.c notdi2.c qdivrem.c subdi3.c ucmpdi2.c udivdi3.c umoddi3.c \
        xordi3.c

VPATH += yp
SRCFILES += ypmatch_cache.c yp_bind.c yp_get_default_domain.c \
        yp_first.c yp_all.c yp_order.c \
        yp_master.c yp_maplist.c yperr_string.c ypprot_err.c _yp_check.c \
        xdr_domainname.c xdr_keydat.c xdr_mapname.c xdr_peername.c \
        xdr_valdat.c xdr_ypbind_binding.c xdr_ypbind_resp.c \
        xdr_ypbind_resptype.c xdr_ypbind_setdom.c xdr_ypmaplist.c \
        xdr_ypreq_key.c xdr_ypreq_nokey.c xdr_ypresp_all.c \
        xdr_ypresp_key_val.c xdr_ypresp_maplist.c xdr_ypresp_master.c \
        xdr_ypresp_order.c xdr_ypresp_val.c xdr_ypstat.c

VPATH += md
SRCFILES += md4c.c md5c.c md4hl.c md5hl.c
UNINITOK += md4c.c
EXTRACLN += md4hl.c md5hl.c

VPATH += rpc
SRCFILES += auth_none.c auth_unix.c  authunix_prot.c bindresvport.c \
        clnt_generic.c clnt_perror.c clnt_raw.c clnt_simple.c clnt_tcp.c \
        clnt_udp.c rpc_dtablesize.c get_myaddress.c getrpcent.c getrpcport.c \
        pmap_clnt.c pmap_getmaps.c pmap_getport.c pmap_prot.c \
        pmap_prot2.c pmap_rmt.c rpc_prot.c rpc_commondata.c rpc_callmsg.c \
        svc.c svc_auth.c svc_auth_unix.c svc_raw.c svc_run.c svc_simple.c \
        svc_tcp.c svc_udp.c xdr.c xdr_array.c xdr_float.c xdr_mem.c \
        xdr_rec.c xdr_reference.c xdr_stdio.c
UNINITOK += pmap_prot2.c pmap_rmt.c
WARNOK += auth_none.c auth_unix.c clnt_raw.c clnt_simple.c clnt_tcp.c \
	clnt_udp.c rpc_prot.c rpc_callmsg.c svc_auth.c svc_run.c svc_simple.c \
	svc_tcp.c svc_udp.c xdr.c xdr_array.c xdr_rec.c xdr_reference.c

VPATH += gmon
SRCFILES += gmon.c mcount.c
NOPROF += gmon.c mcount.c profil.c
NOBBPROF += atexit.c uidt.c vm.c fault.c mcount.c

include $(TOP)/lib/libexos/GNUmakefile.inc

include $(TOP)/lib/xio/GNUmakefile.inc


include $(TOP)/GNUmakefile.global

md4hl.c: mdXhl.c
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $< > $@

md5hl.c: mdXhl.c
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $< > $@
