
TOP = ../..
OBJDIR = obj
STATIC=1
EXTRAINC = -I.. -I../../libc -I.
LIB = libcurses.a
DEFS = -D__OpenBSD__ 
EXTRACLN = nomacros.h codes.c comp_captab.c expanded.c fallback.c lib_gen.c \
	lib_keyname.c names.c unctrl.c keys.tries make_hash make_keys
UNINITOK =
UNUSEDOK =
WARNOK = comp_error.c lib_initscr.c lib_setup.c lib_slkset.c name_match.c \
	parse_entry.c read_bsd_terminfo.c

CPP = c++
AWK = /usr/bin/awk


SRCFILES= \
	access.c alloc_entry.c captoinfo.c codes.c comp_captab.c comp_error.c \
        comp_expand.c comp_hash.c comp_parse.c comp_scan.c define_key.c \
        expanded.c fallback.c hardscroll.c hashmap.c keyok.c lib_acs.c \
        lib_adabind.c lib_addch.c lib_addstr.c lib_baudrate.c \
        lib_beep.c lib_bkgd.c lib_box.c lib_chgat.c lib_clear.c \
        lib_clearok.c lib_clrbot.c lib_clreol.c lib_color.c \
        lib_colorset.c lib_cur_term.c lib_data.c lib_delch.c \
        lib_delwin.c lib_dft_fgbg.c lib_doupdate.c lib_echo.c \
        lib_endwin.c lib_erase.c lib_flash.c lib_freeall.c lib_gen.c \
        lib_getch.c lib_getstr.c lib_hline.c lib_immedok.c lib_inchstr.c \
        lib_initscr.c lib_insch.c lib_insdel.c lib_insstr.c lib_instr.c \
        lib_isendwin.c lib_kernel.c lib_keyname.c lib_leaveok.c \
        lib_longname.c lib_mouse.c lib_move.c lib_mvcur.c lib_mvwin.c \
        lib_napms.c lib_newterm.c lib_newwin.c lib_nl.c lib_options.c \
        lib_overlay.c lib_pad.c lib_print.c lib_printw.c lib_raw.c \
        lib_redrawln.c lib_refresh.c lib_restart.c lib_scanw.c \
        lib_screen.c lib_scroll.c lib_scrollok.c lib_scrreg.c \
        lib_set_term.c lib_setup.c lib_slk.c lib_slkatr_set.c \
        lib_slkatrof.c lib_slkatron.c lib_slkatrset.c lib_slkattr.c \
        lib_slkclear.c lib_slkcolor.c lib_slkinit.c lib_slklab.c \
        lib_slkrefr.c lib_slkset.c lib_slktouch.c lib_termcap.c \
        lib_ti.c lib_touch.c lib_tparm.c lib_tputs.c lib_trace.c \
        lib_traceatr.c lib_tracechr.c lib_tracedmp.c lib_tracemse.c \
        lib_tstp.c lib_twait.c lib_ungetch.c lib_vidattr.c lib_vline.c \
        lib_wattroff.c lib_wattron.c lib_window.c name_match.c names.c \
	nc_panel.c parse_entry.c read_bsd_terminfo.c read_entry.c \
	read_termcap.c resizeterm.c safe_sprintf.c sigaction.c softscroll.c \
	trace_buf.c tries.c unctrl.c wresize.c write_entry.c

export DOINSTALL=yes

include $(TOP)/GNUmakefile.global


fallback.c: $(CURDIR)/term.h $(CURDIR)/curses.priv.h $(CURDIR)/MKfallback.sh
	sh $(CURDIR)/MKfallback.sh $(FALLBACK_LIST) > fallback.c

lib_gen.c: $(CURDIR)/curses.h $(CURDIR)/MKlib_gen.sh
	sh $(CURDIR)/MKlib_gen.sh "$(CPP) -I$(CURDIR)" "$(AWK)" < $(CURDIR)/curses.h > lib_gen.c

nomacros.h: $(CURDIR)/MKlib_gen.sh lib_gen.c
	fgrep undef lib_gen.c > nomacros.h:

keys.tries: make_keys $(CURDIR)/keys.list
	./make_keys $(CURDIR)/keys.list > keys.tries

make_keys: $(CURDIR)/make_keys.c names.c
	$(NCC) -I$(CURDIR) -I. -o make_keys $(CURDIR)/make_keys.c

make_hash: $(CURDIR)/comp_hash.c $(CURDIR)/hashsize.h
	$(NCC) -I$(CURDIR) -DMAIN_PROGRAM -o make_hash $(CURDIR)/comp_hash.c

expanded.c: $(CURDIR)/term.h $(CURDIR)/curses.priv.h $(CURDIR)/MKexpanded.sh
	sh $(CURDIR)/MKexpanded.sh "$(CPP)" $(CPPFLAGS)

comp_captab.c: make_hash $(CURDIR)/ncurses_cfg.h $(CURDIR)/tic.h $(CURDIR)/term.h
	sh $(CURDIR)/MKcaptab.awk $(AWK) $(CURDIR)/Caps > comp_captab.c

lib_keyname.c: $(CURDIR)/ncurses_cfg.h $(CURDIR)/curses.h $(CURDIR)/term.h  $(CURDIR)/keys.list $(CURDIR)/MKkeyname.awk
	$(AWK) -f $(CURDIR)/MKkeyname.awk $(CURDIR)/keys.list > lib_keyname.c

names.c codes.c: $(CURDIR)/MKnames.awk
	$(AWK) -f $(CURDIR)/MKnames.awk $(CURDIR)/Caps
	cat namehdr boolnames boolfnames numnames numfnames strnames strfnames nameftr > names.c
	cat namehdr boolcodes numcodes strcodes codeftr > codes.c
	-rm -f namehdr nameftr codeftr boolnames boolfnames boolcodes numnames numfnames numcodes strnames strfnames strcodes

unctrl.c: $(CURDIR)/MKunctrl.awk
	echo | $(AWK) -f $(CURDIR)/MKunctrl.awk > unctrl.c

lib_options.o: keys.tries
