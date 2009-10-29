#
# ASHes are disabled by default.  To enable them, you would at least need to
# uncomment the following line
#DEFS += -DASH
# Also make sure the following files are rebuilt: (remove the from obj dir)
# fork.o procaux.o udp_*  net_usr.o ae_net.o xrt0.o
# but there might be other things. (maybe lib/ash?)
#
LIBEXOS = $(TOP)/lib/libexos

EXTRAINC += -I$(LIBEXOS)  -DPRINTF_LEVEL=-1

VPATH += $(LIBEXOS)/os
SRCFILES += xrt0.c ipc.S ipcdemux.c cap.c brk.c shexec.c fork.c tick.c \
	process.c __signal.c oncalls.c __main.c mmap.c mprotect.c cow.c \
        getrusage.c getrlimit.c gettimeofday.c setpriority.c uwk.c \
	vhangup.c revoke.c vm.c ubc.c itimer.c ptrace.c \
	undelete.c stacktrace.c symlookup.c oscallstr.c shm.c \
	regions.c callcount.c kerncallstr.c entry.S fault.c kprintf.c \
	__sysctl.c nanosleep.c rtc.c process_table.c _fpu.c \
	procd_subr.c procd.c procd_proc.c procd_prot.c procd_ps.c \
	synch.c uidt.c page_replacement.c page_io.c \
	__malloc.c __calloc.c pinned_malloc.c mregion.c \
	bufcache.c _ptrace.S pager.c cipc.c profil.c _brk.S magic.c \
	locks.c

# uses <exos/conf.h> to enable which process table abstraction to use

NOPROF   += uidt.c rtc.c fault.c
UNUSEDOK += uidt.c rtc.c
NOOPT    += uidt.c rtc.c


VPATH += $(LIBEXOS)/os/net
SRCFILES += pkt.c ash_pkt.c ae_net.c

VPATH += $(LIBEXOS)/netinet
SRCFILES += net_usr.c dpf_lib.c dpf_lib_udpu.c dpf_lib_udpc.c cksum.c in.c getnetcardno.c

#
# hosttable only here because they are easy to use for debugging.
# real applitions should use the more flexible if/route interface 
#  <exos/netinet/{if.h,route.h}>
#
VPATH += $(LIBEXOS)/netinet/hosttable
SRCFILES += get_ether_from_ip.c get_ip_from_name.c \
	get_name_from_ip.c gethostbyaddr.c gethostbyname.c getipbyether.c \
	hosttable.c get_hostent.c

VPATH += $(LIBEXOS)/net
SRCFILES += arp_init.c arp_table.c dpf_lib_arp.c routenif.c if_ioctl.c

# DEVTTY allows process to see a "/dev/tty", needed to run lmbench
# but is nowhere close to what /dev/tty should be - a HACK
# DEFS += -DDEVTTY
# HBXX don't use DEVTTY if you are using NEWPTY.

# file select.c has a define to set if you want to select for exceptional events
VPATH += $(LIBEXOS)/fd
SRCFILES +=  proc.c fdstat.c fdstat_init.c namei.c namei_sub.c namei_cache.c ftable.c \
	select.c pipe.c open.c flock.c __utime.c \
	null.c userids.c dumb.c console.c fillw.S  \
	mount.c strncpy2.c procaux.c default_modules.c sync.c \
	pathconf.c groups.c

# FASTOPEN does only one namei in open instead of two
# CACHEDLOOKUP, lookups are cached experimental and no cached delitions
# PRINTFDSTAT, prints fd stats at exit time.
#DEFS += -DFASTOPEN -DCACHEDLOOKUP -DPRINTFDSTAT
DEFS += -DPRINTFDSTAT
UNUSEDOK += namei.c

VPATH += $(LIBEXOS)/fd/udp
SRCFILES += udp_init.c udp_sendto.c udp_recvfrom.c udp_socket.c udp_debug.c action.c dpf-ir.c

VPATH += $(LIBEXOS)/fd/pty
SRCFILES += pty.c


# common routines
VPATH += $(LIBEXOS)/fd/cdev 
SRCFILES += sw.c subr_xxx.c subr_uio.c 

# NEWPTY are the new pty's that understand process groups and sessions.  Old
# pty's maintained during transition.

# make sure the following files get recompiled:
# default_modules.c, sw.c, nfs_open.c, nfs_lookup_cache.c (and removed libc.a)
# the following command does the job in libc/obj:
# rm default_modules.? nfs_open.? nfs_lookup_cache.? sw.? subr_xxx.? subr_uio.? libc.a 
# rm cffs_proc.?
# rm cdev.? tty.? tty_pty.? tty_subr.? ctty.? npty.? ntty.? ntty_subr.? tty_npty.?

#ifdef NEWPTY
ifeq (1,1)
SRCFILES += cdev.c tty.c tty_pty.c tty_subr.c ctty.c
DEFS += -DNEWPTY
else

VPATH += $(LIBEXOS)/fd/npty
SRCFILES += npty.c ntty.c ntty_subr.c tty_npty.c
# tty_sched.c has been obsoleted by os/synch.c

endif # NEWPTY


VPATH += $(LIBEXOS)/fd/cffs                                            
SRCFILES += cffs_init.c \
	buffer_tab.c cffs_alloc.c cffs_buffer.c cffs_defaultcache.c \
	cffs_dinode.c cffs_embdir.c cffs_embpath.c cffs_inode.c \
	cffs_rdwr.c cffs_proc.c cffs_fsck.c name_cache.c \
	cffs_xntypes.c spec.c sugar.c ubb-lib.c bit.c cffsd_client.c

VPATH += $(LIBEXOS)/fd/nfs
SRCFILES += nfs_init.c nfs_file_ops.c nfs_pmap.c nfs_xdr.c \
	nfs_proc.c nfs_open.c nfs_cache.c \
	nfs_mnt.c nfs_mount_root.c nfs_rpc.c \
        nfs_rpc_procs.c nfs_getdirentries.c \
        nfs_rpc_rw.c nfs_server.c nfs_rw_new.c \
	nfs_read_nocache.c nfs_write_nocache.c \
	nfs_name_cache.c nfs_lookup_cache.c \
	nfs_debug.c

VPATH += $(LIBEXOS)/debug
SRCFILES += i386-stub.c
NOPROF   += i386-stub.c
UNUSEDOK += i386-stub.c
NOOPT    += i386-stub.c


# The following files are not used
# see the README file in the os/net directory.

#VPATH += $(LIBEXOS)/os/net/tcp
#SRCFILES += tcp.c log.c tcb.c timer.c eth_comm.c bprintf.c mk_tcp.c \
#	tcp_socket.c tcpsocket_init.c tcpsocket_listen.c

#VPATH += $(LIBEXOS)/os/net/udp
#SRCFILES += udp_eth.c mkudp.c udp2_eth.c

