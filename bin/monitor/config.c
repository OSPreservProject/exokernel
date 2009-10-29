#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "bios.h"
#include "config.h"
#include "debug.h"
#include "disks.h"
#include "emu.h"
#include "memoryemu.h"
#include "video.h"


#define MAX_MEM_SIZE    640



static void usage(void)
{
    fprintf(stderr,
	    "monitor\n\n"
	    "USAGE:\n"
	    "  monitor  [-ABCckbVNtsgxKm23456ez] [-h{0|1|2}] [-H dflags \\\n"
	    "       [-D flags] [-M SIZE] [-P FILE] [ {-F|-L} File ] \\\n"
	    "       [-u confvar] [-f dosrcFile] [-o dbgfile] 2> vital_logs\n"
	    "  monitir --version\n\n"
	    "    -2,3,4,5,6 choose 286, 386, 486 or 586 or 686 CPU\n"
	    "    -A boot from first defined floppy disk (A)\n"
	    "    -C boot from first defined hard disk (C)\n"
	    "    -D set debug-msg mask to flags {+-}{0-9}{#CDEIMPRSWcdghikmnprsvwx}\n"
	    "       #=defint  C=cdrom    D=dos    E=ems       I=ipc     M=dpmi\n"
	    "       P=packet  R=diskread S=sound  W=diskwrite c=config  d=disk\n"
	    "       g=general h=hardware i=i/o    k=keyb      m=mouse   n=ipxnet\n"
	    "       p=printer r=pic      s=serial v=video     w=warning x=xms\n"
	    "    -F use File as global config-file\n"
	    "    -M set memory size to SIZE kilobytes (!)\n"
	);
}

/*
 * DANG_BEGIN_FUNCTION parse_debuglags
 * 
 * arguments: 
 * s - string of options.
 * 
 * description: 
 * This part is fairly flexible...you specify the debugging
 * flags you wish with -D string.  The string consists of the following
 * characters: +   turns the following options on (initial state) -
 * turns the following options off a   turns all the options on/off,
 * depending on whether +/- is set 0-9 sets debug levels (0 is off, 9 is
 * most verbose) #   where # is a letter from the valid option list (see
 * docs), turns that option off/on depending on the +/- state.
 * 
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful. Some
 * options are set by default, some are clear. This is subject to my whim.
 * You can ensure which are set by explicitly specifying.
 * 
 * DANG_END_FUNCTION
 */
int parse_debuglags(const char *s, unsigned char flag)
{
    char c;
    int ret = 0;

    /*
     * if you add new classes of debug messages, make sure to add the
     * letter to the allopts string above so that "1" and "a" can work
     * correctly.
     */


    debug("debug flags: %s\n", s);
    while ((c = *(s++)))
	switch (c) {
	case '+':		/* begin options to turn on */
	    if (!flag)
		flag = 1;
	    break;
	case '-':		/* begin options to turn off */
	    flag = 0;
	    break;

	case 'd':		/* disk */
	    if (flag)
		debug_flags |= DDISK;
	    else
		debug_flags &= ~DDISK;
	    break;
	case 'v':		/* video */
	    if (flag)
		debug_flags |= DVIDEO;
	    else
		debug_flags &= ~DVIDEO;
	    break;
	case 'k':		/* keyboard */
	    if (flag)
		debug_flags |= DKYBD;
	    else
		debug_flags &= ~DKYBD;
	    break;
	case 'i':		/* i/o instructions (in/out) */
	    if (flag)
		debug_flags |= DIO;
	    else
		debug_flags &= ~DIO;
	    break;
	default:
	    printf("Unknown debug-msg mask: %c\n", c);
	    ret = 1;
	}
    return ret;
}

/*
 * DANG_BEGIN_FUNCTION config_defaults
 * 
 * description: 
 * Set all values in the `config` structure to their default
 * value. These will be modified by the config parser.
 * 
 * DANG_END_FUNCTION
 * 
 */
void config_defaults(void)
{
    int i;

    for (i=0; i<MAX_FDISKS; i++) {
	memset(&fdisktab[i], 0, sizeof(struct vdisk));
	fdisktab[i].fdesc = -1;
    }
    for (i=0; i<MAX_HDISKS; i++) {
	memset(&hdisktab[i], 0, sizeof(struct vdisk));
	hdisktab[i].fdesc = -1;
    }
    config.boot_protected = 0;
    config.exit_on_hlt = 0;
    config.fdisks = 0;
    config.hdisks = 0;
    config.hdiskboot = 1;

    vm86s.cpu_type = CPU_386;
    /* defaults - used when /proc is missing, cpu!=x86 etc. */
    config.rdtsc = 0;
    config.smp = 0;
#ifdef X86_EMULATOR
    config.emuspeed = 50;	/* instruction cycles per us */
#endif
    config.realcpu = CPU_686;
    config.pci = 1;		/* fair guess */
    config.mathco = 1;
    debug("Running on CPU=%d86, FPU=%d, rdtsc=%d\n",
	  config.realcpu, config.mathco, config.rdtsc);

#ifdef X86_EMULATOR
    config.cpuemu = 0;
#endif
    config.mem_size = 640;
    config.phys_mem_size = MEGS_RAM * 1024;
    config.ems_size = 0;
    config.ems_frame = 0xd000;
    config.xms_size = 0;
    config.max_umb = 0;
    config.dpmi = 0;
    config.secure = 1;		/* need to have it 'on', else user may trick it out
				   via -F option */
    config.mouse_flag = 0;
    config.mapped_bios = 0;
    config.vbios_file = NULL;
    config.bios_file = NULL;
    config.vbios_copy = 0;
    config.vbios_seg = 0xc000;
    config.vbios_size = 0x10000;
    config.console = 1;
    config.console_keyb = 1;
    config.console_video = 1;
    config.kbd_tty = 0;
    config.bootdisk = fdisktab;
    config.exitearly = 0;
    config.term_esc_char = 30;	/* Ctrl-^ */
    /* config.term_method = METHOD_FAST; */
    config.term_color = 1;
    /* config.term_updatelines = 25; */
    config.term_updatefreq = 4;
    config.term_charset = CHARSET_LATIN;
    /* config.term_corner = 1; */
    config.X_updatelines = 25;
    config.X_updatefreq = 8;
    config.X_display = NULL;	/* NULL means use DISPLAY variable */
    config.X_title = "emu";
    config.X_icon_name = "emu";
    config.X_blinkrate = 8;
    config.X_sharecmap = 0;	/* Don't share colourmap in graphics modes */
    config.X_mitshm = 0;
    config.X_fixed_aspect = 1;
    config.X_aspect_43 = 0;
    config.X_lin_filt = 0;
    config.X_bilin_filt = 0;
    config.X_mode13fact = 2;
    config.X_winsize_x = 0;
    config.X_winsize_y = 0;
    config.X_gamma = 100;
    config.vgaemu_memsize = 0;
    config.vesamode_list = NULL;
    config.X_lfb = 1;
    config.X_pm_interface = 1;
    config.X_keycode = 0;
    config.X_font = "vga";
    config.usesX = 0;
    config.X = 0;
    config.X_mgrab_key = "";	/* off , NULL = "Home" */
    config.hogthreshold = 10;	/* bad estimate of a good garrot value */
    config.chipset = PLAINVGA;
    config.cardtype = CARD_VGA;
    config.pci_video = 0;
    config.fullrestore = 0;
    config.graphics = 0;
    config.gfxmemsize = 256;
    config.vga = 0;		/* this flags BIOS graphics */
    config.dualmon = 0;
    config.force_vt_switch = 0;
    config.speaker = SPKR_EMULATED;

    /* The frequency in usec of the SIGALRM call (in signal.c) is
     * equal to this value / 6, and thus is currently 9158us = 100 Hz
     * The 6 (TIMER DIVISOR) is a constant of unknown origin
     * NOTE: if you set 'timer 18' in config.dist you can't get anything
     * better that 55555 (108Hz) because of integer math.
     * see timer_interrupt_init() in init.c
     */
    config.update = 54925;	/* should be = 1E6/config.freq */
    config.freq = 18;		/* rough frequency (real PC = 18.2065) */
    config.wantdelta = 9154;	/* requested value for setitimer */
    config.realdelta = 9154;

    config.timers = 1;		/* deliver timer ints */
    config.keybint = 1;		/* keyboard interrupts */

    /* Lock file stuff */
#if 0
    config.tty_lockdir = PATH_LOCKD;	/* The Lock directory  */
    config.tty_lockfile = NAME_LOCKF;	/* Lock file pretext ie LCK.. */
    config.tty_lockbinary = 0;		/* Binary lock files ? */
#endif
    config.tty_lockdir = 0;
    config.tty_lockfile = 0;
    config.tty_lockbinary = 0;

    config.num_ser = 0;
    config.num_lpt = 0;
    config.fastfloppy = 1;

    config.emusys = (char *) NULL;
    config.emubat = (char *) NULL;
    config.emuini = (char *) NULL;
    config.dosbanner = 1;
    config.allowvideoportaccess = 0;

/*     config.keytable = &keytable_list[KEYB_USER]; */
    config.keytable = 0;

    config.detach = 0;		/* Don't detach from current tty and open
				 * new VT. */
    config.debugout = NULL;	/* default to no debug output file */

    config.pre_stroke = NULL;	/* defualt no keyboard pre-strokes */
    config.debugger = 0;

    config.sillyint = 0;
    config.must_spare_hardware_ram = 0;
    memset(config.hardware_pages, 0, sizeof(config.hardware_pages));

#if 0
    mice->fd = -1;
    mice->add_to_io_select = 0;
    mice->type = 0;
    mice->flags = 0;
    mice->intdrv = 0;
    mice->cleardtr = 0;
    mice->baudRate = 0;
    mice->sampleRate = 0;
    mice->lastButtons = 0;
    mice->chordMiddle = 0;
#endif

    config.sb_base = 0x220;
    config.sb_dma = 1;
    config.sb_irq = 5;
#ifdef __NetBSD__
    config.sb_dsp = "/dev/sound";
    config.sb_mixer = "/dev/mixer";
#else
    config.sb_dsp = "/dev/dsp";
    config.sb_mixer = "/dev/mixer";
#endif				/* !__NetBSD__ */
    config.mpu401_base = 0x330;

    config.vnet = 1;

    config.idle_int = 0;
}

#define NEXTWORD(p) for(;*p && *p!=' ' && *p!='\t'; p++);\
for(;*p==' ' || *p=='\t'; p++);
#define LINE_LEN 80

int isAffirmative(const char *p)
{
    if (!strcasecmp(p, "yes") ||
	!strcasecmp(p, "true") ||
	!strcasecmp(p, "on") ||
	!strcasecmp(p, "1"))
	return 1;
    else
	return 0;
}


int isNegative(const char *p)
{
    if (!strcasecmp(p, "no") ||
	!strcasecmp(p, "false") ||
	!strcasecmp(p, "off") ||
	!strcasecmp(p, "0"))
	return 1;
    else
	return 0;
}


/* returns 0 if loaded ok, else -1 */

int parse_config(const char *filename)
{
    char line[LINE_LEN];
    int lineno = 1;
    FILE *f;

    ASSERT(filename);

    if (!filename || !*filename || !(f = fopen(filename, "r"))) {
	dprintf("Could not open config file %s", filename);
	return -1;
    }
    debug("opened %s\n", filename);

    for(;;) {
	char buffer[64], *p;
	int i;
	int no_match = 0;

	lineno++;
	fgets(line, LINE_LEN, f);
	if (feof(f))
	    break;
	
	for (p=line; *p==' ' || *p=='\t'; p++);
	if (!*p || *p=='\n' || *p == '#') {
	    /* nothing */
	} else if (sscanf(line, "ram %d", &i) == 1) {
	    config.phys_mem_size = i*1024;
	} else if (sscanf(line, "fdisk %64s", buffer) == 1) {
	    disk_create(buffer, IMAGE, 1, 0);
	} else if (sscanf(line, "hdisk %64s", buffer) == 1) {
	    disk_create(buffer, IMAGE, 0, 0);
	} else if (sscanf(line, "boot-protected %d", &i) == 1) {
	    config.boot_protected = i ? 1 : 0;
	} else if (sscanf(line, "exit-on-hlt %d", &i) == 1) {
	    config.exit_on_hlt = i ? 1 : 0;
	} else if (sscanf(line, "debug-on-boot %d", &i) == 1) {
	    config.exitearly = i ? 1 : 0;
	} else if (sscanf(line, "msg-%64s %d", buffer, &i) == 2) {
	    i = i ? 1 : 0;
	    if (!strcmp(buffer, "trace")) {
		debug_flags = (debug_flags & ~DTRACE) | (DTRACE*i);
	    } else if (!strcmp(buffer, "debug")) {
		debug_flags = (debug_flags & ~DDEBUG) | (DDEBUG*i);
	    } else if (!strcmp(buffer, "info")) {
		debug_flags = (debug_flags & ~DINFO) | (DINFO*i);
	    } else if (!strcmp(buffer, "warn")) {
		debug_flags = (debug_flags & ~DWARN) | (DWARN*i);
	    } else if (!strcmp(buffer, "error")) {
		debug_flags = (debug_flags & ~DERROR) | (DERROR*i);
	    } else if (!strcmp(buffer, "disk")) {
		debug_flags = (debug_flags & ~DDISK) | (DDISK*i);
	    } else if (!strcmp(buffer, "video")) {
		debug_flags = (debug_flags & ~DVIDEO) | (DVIDEO*i);
	    } else if (!strcmp(buffer, "kybd")) {
		debug_flags = (debug_flags & ~DKYBD) | (DKYBD*i);
	    } else if (!strcmp(buffer, "io")) {
		debug_flags = (debug_flags & ~DIO) | (DIO*i);
	    } else if (!strcmp(buffer, "mem")) {
		debug_flags = (debug_flags & ~DMEM) | (DMEM*i);
	    } else {
		no_match = 1;
	    }
	} else if (sscanf(line, "debug-flags %d", &i) == 1) {
	    debug_flags = i;
	} else if (sscanf(line, "log %64s", buffer) == 1) {
	    config.debugout = strdup(buffer);
	} else if (sscanf(line, "bios-file %64s", buffer) == 1) {
	    config.bios_file = strdup(buffer);
	} else if (sscanf(line, "debugger %d", &i) == 1) {
	    config.debugger = i;
	} else if (sscanf(line, "megs-ram %d", &i) == 1 && i>=1) {
	    config.phys_mem_size = i * 1024;
	} else if (sscanf(line, "video %64s", buffer) == 1) {
	    if (!strcasecmp(buffer, "mda"))
		config.cardtype = CARD_MDA;
	    else if (!strcasecmp(buffer, "cga"))
		config.cardtype = CARD_CGA;
	    else if (!strcasecmp(buffer, "ega"))
		config.cardtype = CARD_EGA;
	    else if (!strcasecmp(buffer, "vga"))
		config.cardtype = CARD_VGA;
	    else
		no_match = 1;
	} else if (sscanf(line, "idle-int %d", &i) == 1) {
	    if (i<0 || i>0xff)
		i = 0;
	    config.idle_int = i;
	} else {
	    no_match = 1;
	}
	if (no_match) {
	    dprintf("invalid config line #%d:  %s\n", lineno, line);
	    leaveemu(ERR_CONFIG);
	}
    }

    return 0;
}

#undef NEXTWORD
#undef LINE_LEN


/*
  This is called to parse the command-line arguments and config
  files. 
 */
void config_init(int argc, char **argv)
{
    int c;
    char *confname = NULL;
    
    config_defaults();

    opterr = 0;
    confname = CONFIG_FILE;

    while ((c = getopt(argc, argv, "F:")) != EOF) {
	switch (c) {
	case 'F':
	    confname = optarg;
	    break;
	}
    }

    if (parse_config(confname))
	exit(1);
    

    optreset = 1;
    optind = 1;
    opterr = 0;
    while ((c = getopt(argc, argv, "ACcF:I:kM:D:P:v:VNtsgx:KLm2345e:dXY:Z:E:o:Ou:")) != EOF) {
	switch (c) {
	case 'F':		/* previously parsed config file argument */
	case 'I':
	case 'd':
	case 'o':
	case 'O':
	case 'L':
	case 'u':
	    break;
	case 'A':
	    config.hdiskboot = 0;
	    break;
	case 'C':
	    config.hdiskboot = 1;
	    break;
	case 'c':
	    config.console_video = 1;
	    break;
	case 'k':
	    config.console_keyb = 1;
	    break;
	case 'M':{
	    int max_mem = config.vga ? 640 : MAX_MEM_SIZE;
	    config.mem_size = atoi(optarg);
	    if (config.mem_size > max_mem)
		config.mem_size = max_mem;
	    break;
	}
	case 'D':
	    parse_debuglags(optarg, 1);
	    break;
	case 'V':
	    v_printf("Configuring as VGA video card & mapped ROM\n");
	    config.vga = 1;
	    config.mapped_bios = 1;
	    if (config.mem_size > 640)
		config.mem_size = 640;
	    break;
	case 'v':
	    config.cardtype = atoi(optarg);
	    if (config.cardtype > 7)	/* keep it updated when adding a new card! */
		config.cardtype = 1;
	    v_printf("Configuring cardtype as %d\n", config.cardtype);
	    break;
	case 'N':
	    warn("will exit early\n");
	    config.exitearly = 1;
	    break;

	case '2':
	    debug("CPU set to 286\n");
	    vm86s.cpu_type = CPU_286;
	    break;

	case '3':
	    debug("CPU set to 386\n");
	    vm86s.cpu_type = CPU_386;
	    break;

	case '4':
	    debug("CPU set to 486\n");
	    vm86s.cpu_type = CPU_486;
	    break;

	case '5':
	    debug("CPU set to 586\n");
	    vm86s.cpu_type = CPU_586;
	    break;

	case '?':
	default:
	    error("unrecognized option: -%c\n\r", c);
	    usage();
	    fflush(stdout);
	    fflush(stderr);
	    _exit(1);
	}
    }
}
