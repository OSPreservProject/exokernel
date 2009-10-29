
TOP = ../..
DOINSTALL=yes
OBJDIR = obj_pic
EXTRAINC = -I$(CURDIR)/../libc/ -DPRINTF_LEVEL=-1
SLIB = libc.so.1.1
DEFS = -DPIC -DFLOATING_POINT=1 #-DUSE_SYNCER
UNINITOK =
UNUSEDOK =
CCPICFLAGS = -fpic
ASPICFLAGS = -k
INSTALLPREFIX = bin
EXTRAOBJ = _udivdi3.o _umoddi3.o _divdi3.o _moddi3.o
# MY_GCC = /disk/hs0/dwyatt/bin/bin/gcc

VPATH = ../libc/string
SRCFILES = bzero.S memchr.S memcmp.S memcpy.S memset.S strcasecmp.c \
       strcat.S strchr.S strcmp.S strcpy.S strcspn.c strdup.c \
       strlen.S strncat.c strncmp.S strncpy.c strpbrk.c strrchr.S \
       strsep.c strspn.c strstr.c strtok.c swab.S bcopy.S strftime.c\
       memccpy.c strerror.c __strerror.c strcoll.c bcmp.S index.S rindex.S \

VPATH += ../libc/os
SRCFILES += ipc.S brk.c exec.c fork.c tick.c shared.c dyn_post_process.c \
	signal.c oncalls.c __main.c mmap.c mprotect.c cow.c uname.c \
        getrusage.c getrlimit.c gettimeofday.c time.c setpriority.c uwk.c \
	getlogin.c setsid.c vhangup.c revoke.c vm.c ubc.c alarm.c undelete.c \
	stacktrace.c symlookup.c entry.S fault.c silly.c silly1.c

VPATH += ../libc/net
SRCFILES += pkt.c ash_pkt.c ae_net.c \
	 inet_addr.c inet_lnaof.c inet_makeaddr.c \
	inet_netof.c inet_network.c inet_ntoa.c \
	getservbyname.c getservent.c  \
	getproto.c getprotoent.c getprotoname.c \
	getservbyport.c rcmd.c herror.c \
	res_query.c res_send.c res_mkquery.c res_init.c \
	res_debug.c res_comp.c	\
	gethostnamadr.c getnetent.c getnetbyname.c getnetbyaddr.c \
	sethostent.c nsap_addr.c


VPATH += ../libc/net/tcp
SRCFILES += cksum.c tcp.c log.c tcb.c timer.c eth_comm.c bprintf.c mk_tcp.c \
	tcp_socket.c tcpsocket_init.c tcpsocket_listen.c


VPATH += ../libc/net/udp
SRCFILES += udp_eth.c mkudp.c udp2_eth.c

VPATH += ../libc/stdio
SRCFILES+=  clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
        fgetln.c fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
        fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
        fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
        getc.c getchar.c gets.c getw.c makebuf.c mktemp.c perror.c printf.c \
        putc.c putchar.c puts.c putw.c refill.c remove.c rewind.c rget.c \
        scanf.c setbuf.c setbuffer.c setvbuf.c snprintf.c sprintf.c sscanf.c \
        stdio.c tempnam.c tmpnam.c ungetc.c \
        vfscanf.c vprintf.c vscanf.c vsnprintf.c vsprintf.c vsscanf.c \
        wbuf.c wsetup.c \
	kprintf.c \
	vfprintf.c \
#	tmpfile.c
UNINITOK += vfprintf.c

VPATH += ../libc/gen
SRCFILES += ctype_.c isctype.c qsort.c setjmp.S errno.c sleep.c getenv.c \
	isinf.c infinity.c errlist.c flt_rounds.S vis.c \
	glob.c fnmatch.c getpwent.c getgrent.c sysconf.c isatty.c \
	vwarn.c vwarnx.c warn.c warnx.c _vwarn.c _vwarnx.c _warn.c _warnx.c \
	err.c _err.c verr.c _verr.c errx.c _errx.c verrx.c _verrx.c \
	tolower_.c toupper_.c popen.c getloadavg.c raise.c getcap.c \
	getbsize.c setmode.c \
	closedir.c opendir.c readdir.c rewinddir.c scandir.c \
	seekdir.c telldir.c fts.c pwcache.c ttyname.c \
	syslog.c getpass.c ttyslot.c getttyent.c initgroups.c \
	getgrouplist.c getusershell.c daemon.c \
	valloc.c getnetgrent.c getdomainname.c setproctitle.c ldexp.c

UNINITOK += qsort.c getcap.c fts.c syslog.c

VPATH += ../libc/time
SRCFILES += asctime.c difftime.c ialloc.c localtime.c scheck.c #zdump.c zic.c

VPATH += ../libc/locale
SRCFILES += _def_messages.c _def_monetary.c _def_numeric.c  _def_time.c

VPATH += ../libc/termios
SRCFILES += cfgetispeed.c cfgetospeed.c cfmakeraw.c cfsetispeed.c \
	cfsetospeed.c cfsetspeed.c tcdrain.c tcflow.c tcflush.c \
	tcgetattr.c tcgetpgrp.c tcsendbreak.c tcsetattr.c tcsetpgrp.c

VPATH += ../libc/regex
SRCFILES += regcomp.c regerror.c regexec.c regfree.c
DEFS += -DPOSIX_MISTAKE
UNINITOK += regcomp.c
UNUSEDOK += regcomp.c

VPATH += ../libc/stdlib
SRCFILES += malloc.c calloc.c strtol.c strtoul.c atoi.c atol.c atof.c \
	multibyte.c strtod.c strtoq.c strtouq.c getopt.c system.c \
	random.c rand.c ldiv.c abort.c abs.c bsearch.c calloc.c \
	div.S heapsort.c labs.S ldiv.S merge.c multibyte.c qabs.c \
	qsort.c radixsort.c bsearch.c \
        _rand48.c erand48.c lcong48.c mrand48.c seed48.c \
        drand48.c jrand48.c lrand48.c nrand48.c srand48.c 
# udivmoddi.c



VPATH += ../libc/netinet
SRCFILES += hosttable.c gethostbyname.c gethostname.c \
        ip_utils.c get_ether_from_ip.c net_usr.c \
        get_ip_from_name.c ethif_usr.c dpf_lib.c dpf_lib_udpu.c in.c \
	get_name_from_ip.c gethostbyaddr.c getnetcardno.c \
	get_hostent.c

VPATH += ../libc/fd
SRCFILES +=  proc.c fdstat.c namei.c namei_sub.c namei_cache.c ftable.c \
	select.c pipe.c open.c flock.c utime.c \
	getcwd.c null.c userids.c  \
	dumb.c console.c fillw.S  \
	mount.c strncpy2.c procaux.c default_modules.c sync.c
# FASTOPEN does only one namei in open instead of two
# CACHEDLOOKUP, lookups are cached experimental and no cached delitions
# PRINTFDSTAT, prints fd stats at exit time.
#DEFS += -DFASTOPEN -DCACHEDLOOKUP -DPRINTFDSTAT
DEFS += -DPRINTFDSTAT
UNUSEDOK = namei.c

VPATH += ../libc/fd/udp
SRCFILES += udp_init.c udp_sendto.c udp_recvfrom.c udp_socket.c

VPATH += ../libc/fd/pty
SRCFILES += pty.c

VPATH += ../libc/fd/npty
SRCFILES += npty.c ntty.c tty_subr.c sw.c tty_npty.c \
      subr_xxx.c subr_uio.c tty_sched.c

VPATH += ../libc/fd/cffs                                            
SRCFILES += cffs_init.c \
	buffer_tab.c cffs_alloc.c cffs_buffer.c cffs_defaultcache.c \
	cffs_dinode.c cffs_embdir.c cffs_embpath.c cffs_inode.c \
	cffs_rdwr.c cffs_proc.c cffs_fsck.c name_cache.c

ifndef NFS_ROOT_HOST
NFS_ROOT_HOST = \"zwolle.lcs.mit.edu\"
endif
ifndef NFS_ROOT_PATH
NFS_ROOT_PATH = \"/disk/zw0/exopc\"
endif
ifndef NFS_ROOT_HOST2
NFS_ROOT_HOST2 = \"redlab.lcs.mit.edu\"
endif
ifndef NFS_ROOT_PATH2
NFS_ROOT_PATH2 = \"/disk/ex0/exopc\"
endif

DEFS += -DNFS_ROOT_HOST=$(NFS_ROOT_HOST) -DNFS_ROOT_PATH=$(NFS_ROOT_PATH)
DEFS += -DNFS_ROOT_HOST2=$(NFS_ROOT_HOST2) -DNFS_ROOT_PATH2=$(NFS_ROOT_PATH2)

VPATH += ../libc/fd/nfs
SRCFILES += nfs_init.c nfs_root.c \
        nfs_pmap.c nfs_xdr.c nfs_proc.c nfs_mnt.c \
        nfs_rpc.c nfs_udp.c nfs_struct_support.c \
        nfs_mount_root.c nfs_rpc_procs.c \
        nfs_getdirentries.c print_error.c\
        nfs_rpc_rw.c nfs_p_server.c nfs_p_rpc.c 

# nfs_readdir.c

DEFS+=-D__DBINTERFACE_PRIVATE
VPATH+= ../libc/db/btree

SRCFILES+=    bt_close.c bt_conv.c bt_debug.c bt_delete.c \
      bt_get.c bt_open.c \
      bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
      bt_utils.c
UNINITOK += bt_delete.c bt_put.c bt_seq.c bt_split.c

VPATH+= ../libc/db/db
SRCFILES+=    db.c

VPATH+= ../libc/db/hash
SRCFILES+=    hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
      hash_page.c hsearch.c ndbm.c

VPATH+= ../libc/db/mpool
SRCFILES+=    mpool.c

VPATH+= ../libc/db/recno
SRCFILES+= rec_close.c rec_delete.c rec_get.c rec_open.c \
      rec_put.c rec_search.c \
      rec_seq.c rec_utils.c

VPATH += ../libc/crypt
SRCFILES += crypt.c md5crypt.c morecrypt.c

VPATH += ../libc/md
SRCFILES += md4c.c md5c.c md4hl.c md5hl.c
UNINITOK += md4c.c

VPATH += ../libc/rpc
SRCFILES += bindresvport.c

include $(TOP)/GNUmakefile.global.dyn

md4hl.c: mdXhl.c
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $< > $@

md5hl.c: mdXhl.c
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $< > $@

_udivdi3.o: $(TOP)/lib/libc_so/_udivdi3.o
	cp $(TOP)/lib/libc_so/_udivdi3.o $(TOP)/lib/libc_so/obj_pic/_udivdi3.o

_umoddi3.o: $(TOP)/lib/libc_so/_umoddi3.o
	cp $(TOP)/lib/libc_so/_umoddi3.o $(TOP)/lib/libc_so/obj_pic/_umoddi3.o

_divdi3.o: $(TOP)/lib/libc_so/_divdi3.o
	cp $(TOP)/lib/libc_so/_divdi3.o $(TOP)/lib/libc_so/obj_pic/_uivdi3.o

_moddi3.o: $(TOP)/lib/libc_so/_moddi3.o
	cp $(TOP)/lib/libc_so/_moddi3.o $(TOP)/lib/libc_so/obj_pic/_moddi3.o