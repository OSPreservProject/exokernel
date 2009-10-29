# Makefile for lmbench results.
#
# Usage: make [ LIST="aix/* sunos/* ..." ] [ what ]
#
# What to make:
#	print			Prints the results 1 per page.
#	8			Prints the result 8 per page
#	8.ps			Saves the postscript of 8 per page in PS/PS8
#	ps			Saves the postscript of 1 per page in PS/PS
#	x			Previews 1 per page using groff -X
#	summary	[default]	Ascii summary of the results
#	percent			Ascii summary as percentages of the best.
#	list			Print out the current list
#	roff			Print the ascii summaries into a roff file
#	html			(Under construction)
#	slides			Makes the pic for inclusion in slides
#
# This Makefile requires groff, gpic, and perl.  You could try it with
# other *roff processors; I have no idea if it works.
#
# There are three sorts of graphical results:
#
# 1. Bargraphs comparing each system in the LIST on the measurements listed
#    in the BG list below (pretty much everything).
# 2. A 2-D graph for each system in LIST, displaying context switch times
#    as a function of (# of processes, size of each process).
# 3. A 2-D graph for each system in LIST, displaying memory read times as
#    a function of (stride size, memory size).
#
# The bargraphs are in a format of my own - the perl script in scripts
# called bargraph takes them as input and produces pic as output.
# It is a pretty straightforward format, you could probably incorparate
# into some Windows spreadsheet if you wanted to.  See tmp/*.bg after
# running make in this directory.
#
# The 2-D graphs are in a format that can (probably) be read by Xgraph.
# I've added a few extensions for titles, etc., that you could just
# take out.  See tmp/mem.* after running a make in this directory.
#
# This Makefile is of marginal usefulness to a site with just one machine.
# I intend to make results available so that people can compare, as well
# as a service where you can compare your results against the "best of
# the breed" for each vendor, as well as against best of the lot.

# List of result files to process.  Defaults to everything.
LIST=	`$(SCRIPTS)getlist $(LST)`	

# Grrrrr
SHELL=/bin/sh

SCRIPTS=../scripts/

MISC=	tmp/misc_mhz.bg \
	tmp/lat_ctx.bg \
	tmp/lat_ctx8.bg \
	tmp/lat_nullsys.bg \
	tmp/lat_signal.bg \
	tmp/lat_pagefault.bg \
	tmp/lat_mappings.bg \
	tmp/lat_fs_create.bg

PROC=	tmp/lat_nullproc.bg \
	tmp/lat_simpleproc.bg \
	tmp/lat_shproc.bg

LATENCY= \
	tmp/lat_pipe.bg \
	tmp/lat_connect.bg \
	tmp/lat_udp_local.bg \
	tmp/lat_rpc_udp_local.bg \
	tmp/lat_tcp_local.bg  \
	tmp/lat_rpc_tcp_local.bg 

BANDWIDTH= \
	tmp/bw_pipe.bg \
	tmp/bw_tcp_local.bg \
	tmp/bw_file.bg \
	tmp/bw_reread.bg \
	tmp/bw_mmap.bg \
	tmp/bw_bcopy_libc.bg \
	tmp/bw_bcopy_unrolled.bg \
	tmp/bw_mem_rdsum.bg \
	tmp/bw_mem_wr.bg

BG=	$(MISC) $(PROC) $(LATENCY) $(BANDWIDTH)

PRINT=groff -p | lpr -h
PS8UP=groff -p | mpage -P -8 -a | $(SCRIPTS)save PS/PS8
PS4UP=groff -p | mpage -P -4 -a | $(SCRIPTS)save PS/PS4
SIZE=-big
GIF=pbm
#GIF=gifmono

summary:
	@$(SCRIPTS)getsummary $(LIST)

percent:
	@$(SCRIPTS)getpercent $(LIST)

roff:
	echo .nf	> summary.roff
	echo .ft CB	>> summary.roff
	echo .ps 12	>> summary.roff
	echo .po .35i	>> summary.roff
	echo .sp .5i	>> summary.roff
	make LIST="$(LIST)" summary	>> summary.roff
	echo .bp	>> summary.roff
	echo .sp .5i	>> summary.roff
	make LIST="$(LIST)" percent	>> summary.roff

list:
	@echo $(LIST)

print: bargraphs ctx mem

8:
	make LIST="$(LIST)" PRINT="groff -p | mpage -P -8 -a | lpr -h" print

8.ps 8ps 8up:
	make LIST="$(LIST)" PRINT="$(PS8UP)" print

4.ps 4ps 4up:
	make LIST="$(LIST)" PRINT="$(PS4UP)" print

ps:
	make LIST="$(LIST)" PRINT="groff -p | $(SCRIPTS)save PS/PS" print

smallps:
	make LIST="$(LIST)" SIZE= PRINT="groff -p | $(SCRIPTS)save PS/smallPS" print

x: 
	make LIST="$(LIST)" PRINT="$(SCRIPTS)xroff -p" print

smallx:
	make LIST="$(LIST)" PRINT="$(SCRIPTS)xroff -p" SIZE= print

slides:
	make LIST="$(LIST)" SIZE=-slide bargraphs.slides ctx.slides mem.slides

paper:
	make LIST="$(LIST)" tbl.paper ctx.paper mem.paper

html:
	-make clean
	$(SCRIPTS)bghtml $(BG)
	$(SCRIPTS)html-list $(LIST)
	make LIST="$(LIST)" summary > HTML/summary
	make LIST="$(LIST)" percent > HTML/percent
	make LIST="$(LIST)" SIZE= PRINT="groff -p | $(SCRIPTS)save PS/PS" print
	make LIST="$(LIST)" SIZE= SHARP="#" PRINT="groff -p | $(SCRIPTS)save PS/PS" mem ctx
	gs -sOutputFile=HTML/bar%02d.$(GIF) -sDEVICE=$(GIF) -q -dNOPAUSE PS/PS < /dev/null
	gs -sOutputFile=HTML/ctx%02d.$(GIF) -sDEVICE=$(GIF) -q -dNOPAUSE PS/PS.1 < /dev/null
	gs -sOutputFile=HTML/mem%02d.$(GIF) -sDEVICE=$(GIF) -q -dNOPAUSE PS/PS.2 < /dev/null
	gs -sOutputFile=HTML/mem-unscaled%02d.$(GIF) -sDEVICE=$(GIF) -q -dNOPAUSE PS/PS.3 < /dev/null
	gs -sOutputFile=HTML/ctx-unscaled%02d.$(GIF) -sDEVICE=$(GIF) -q -dNOPAUSE PS/PS.4 < /dev/null
	$(SCRIPTS)/gifs $(GIF)
	# rm HTML/*.bpm HTML/*.gifmono

bghtml:
	$(SCRIPTS)bghtml $(BG)

html-list:
	$(SCRIPTS)html-list $(LIST)


mem:
	-@mkdir tmp PS 2>/dev/null
	$(SCRIPTS)getmem $(LIST)
	for i in $(LIST); \
	do	f=`basename $$i`; \
		MEM="$${MEM}tmp/mem.$$f "; \
	done; \
	$(SHARP)$(SCRIPTS)getmax -graph $$MEM; \
	for i in $(LIST); \
	do	f=`basename $$i`; \
		$(SCRIPTS)graph -close $(SIZE) -nomarks -logx -xm tmp/mem.$$f; \
		echo .bp; \
        done | $(PRINT)

mem.paper:
	-@mkdir tmp PS 2>/dev/null
	$(SCRIPTS)allmem $(LIST) | \
	$(SCRIPTS)graph -close -small -nolabels -nodatal \
	-thk1_5 -nomarks -grapheach -logx -xm > tmp/mem.pic

mem.slides:
	-@mkdir tmp PS 2>/dev/null
	$(SCRIPTS)getmem $(LIST)
	for i in $(LIST); \
	do	f=`basename $$i`; \
		MEM="$${MEM}tmp/mem.$$f "; \
	done; \
	$(SHARP)$(SCRIPTS)getmax -graph $$MEM; \
	for i in $(LIST); \
	do	f=`basename $$i`; \
		$(SCRIPTS)graph -close $(SIZE) -nomarks -logx -xm tmp/mem.$$f > tmp/mem.$$f.pic; \
        done 

mem.smallgif:
	-@mkdir tmp PS 2>/dev/null
	$(SCRIPTS)getmem $(LIST)
	for i in $(LIST); \
	do	f=`basename $$i`; \
		MEM="$${MEM}tmp/mem.$$f "; \
	done; \
	# $(SCRIPTS)getmax -graph $$MEM; \
	for i in $(LIST); \
	do	f=`basename $$i`; \
		$(SCRIPTS)graph -close -nodatal -nomarks -logx -xm tmp/mem.$$f | \
		groff -p | \
		gs -sDEVICE=$(GIF) -IGS -sPAPERSIZE=lmgraph -q \
		-sOutputFile=HTML/mem.$$f.smallgif -; \
        done 

ctx:
	-@mkdir PS tmp 2>/dev/null
	$(SCRIPTS)getctx $(LIST)
	for i in $(LIST); \
	do	f=`basename $$i`; \
		CTX="$${CTX}tmp/ctx.$$f "; \
	done; \
	for i in $(LIST); \
	do	f=`basename $$i`; \
		$(SCRIPTS)graph $(SIZE) -below tmp/ctx.$$f; \
		echo .bp; \
	done | $(PRINT)

#	$(SHARP)$(SCRIPTS)getmax -graph $$CTX; 

ctx.paper:
	-@mkdir PS tmp 2>/dev/null
	$(SCRIPTS)allctx $(LIST) | \
	$(SCRIPTS)graph -nolabels -nomarks -thk1_5 \
	-nodatal -small -close -br_title > tmp/ctx.pic

ctx.slides:
	-@mkdir PS tmp 2>/dev/null
	$(SCRIPTS)getctx $(SIZE) $(LIST)
	for i in $(LIST); \
	do	f=`basename $$i`; \
		CTX="$${CTX}tmp/ctx.$$f "; \
	done; \
	$(SHARP)$(SCRIPTS)getmax -graph $$CTX; \
	for i in $(LIST); \
	do	f=`basename $$i`; \
		$(SCRIPTS)graph $(SIZE) tmp/ctx.$$f > tmp/ctx.$$f.pic; \
	done 

tbl.paper:
	$(SCRIPTS)getbg -paper $(LIST) 

bargraphs.1st:
	-@mkdir PS tmp 2>/dev/null
	$(SCRIPTS)getbg $(SIZE) -nosort $(LIST)
	#$(SCRIPTS)getmax -v $(PROC)
	#$(SCRIPTS)getmax -v $(LATENCY)
	#$(SCRIPTS)getmax -v -half $(BANDWIDTH)

bargraphs: bargraphs.1st
	for i in $(BG); \
	do	$(SCRIPTS)bargraph $(SIZE) -nobox -sideways $$i; \
		echo .bp; \
        done | $(PRINT)

bargraphs.slides: bargraphs.1st
	for i in $(BG); \
	do	$(SCRIPTS)bargraph $(SIZE) -nobox -sideways $$i > $${i}.pic; \
        done 

bargraphs.8up: bargraphs.1st
	for i in $(BG); \
	do	$(SCRIPTS)bargraph -sideways $(SIZE) -nobox $$i; \
		echo .bp; \
	done | $(PS8UP)

latency.8up: bargraphs.1st
	for i in $(LATENCY); \
	do	$(SCRIPTS)bargraph -sideways $(SIZE) -nobox $$i; \
		echo .bp; \
	done | $(PS8UP)

bw.8up: bargraphs.1st
	for i in $(BANDWIDTH); \
	do	$(SCRIPTS)bargraph -sideways $(SIZE) -nobox $$i; \
		echo .bp; \
	done | $(PS8UP)

get:	# nothing to do

clean:
	/bin/rm -f PS/* GIF/* HTML/* tmp/* summary.roff
