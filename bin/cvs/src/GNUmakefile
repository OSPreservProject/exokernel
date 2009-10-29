TOP = ../../..
PROG = cvs
SRCFILES = add.c admin.c buffer.c checkin.c checkout.c classify.c client.c \
commit.c create_adm.c cvsrc.c diff.c edit.c entries.c error.c expand_path.c \
fileattr.c find_names.c hash.c history.c ignore.c import.c \
lock.c log.c login.c logmsg.c main.c mkmodules.c modules.c myndbm.c no_diff.c \
parseinfo.c patch.c rcs.c rcscmds.c recurse.c release.c remove.c repos.c \
root.c rtag.c scramble.c server.c status.c subr.c filesubr.c run.c \
tag.c update.c watch.c wrapper.c vers_ts.c version.c zlib.c

DEFS=-Dlint -DHAVE_CONFIG_H
UNINITOK = $(SRCFILES)
UNUSEDOK = $(SRCFILES)

export INSTALLPREFIX=usr/bin

EXTRAINC = -I$(TOP)/lib/libc -I. -I.. -I../lib -I../src
EXTRAOBJ = $(TOP)/lib/libz/obj/libz.a ../lib/obj/libcvs.a
include $(TOP)/GNUmakefile.global
