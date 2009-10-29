/* global vars */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "memoryemu.h"
#include "emu.h"
#include "disks.h"
#include "memutil.h"
#include "debug.h"


#define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) fsync(dp->fdesc);
struct vdisk fdisktab[MAX_FDISKS];
struct vdisk hdisktab[MAX_HDISKS];

#define FLOPPY_TYPES 6
u_int floppies[FLOPPY_TYPES][4] = {
    { 360*1024, 2,  9, 40},
    {1200*1024, 2, 15, 80},
    { 720*1024, 2,  9, 80},
    {1440*1024, 2, 18, 80},
    {2880*1024, 2, 38, 80},
    {2880*1024, 2, 36, 80}
};


int checkdp(struct vdisk *disk)
{
    if (disk == NULL) {
	error_disk("DISK: null dp\n");
	return -1;
    } else if (disk->fdesc == -1) {
	error_disk("DISK: closed disk\n");
	return -1;
    } else
	return 0;
}

static void print_disk(int i, struct vdisk *dp)
{
    ASSERT(dp);

    printf("%d: %20s fd=%2d mode=%03u s=%02u,h=%02u,t=%02u",
	   i, dp->dev_name, dp->fdesc, dp->fdesc>0 ? dp->mode : 0, dp->sectors,
	   dp->heads, dp->tracks);
#ifndef NDEBUG
    printf("  reads: %u  writes: %u\n", dp->read_count, dp->write_count);
#endif
    printf("\n");
}

void print_disks(void)
{
int i;
    struct vdisk *dp;

    ASSERT(config.fdisks < MAX_FDISKS);
    printf("%d floppy disks:\n", config.fdisks);
    for (i=0; i<config.fdisks; i++) {
	dp = &fdisktab[i];
	if (! dp->dev_name)
	    break;
	print_disk(i, dp);
    }

    ASSERT(config.hdisks < MAX_HDISKS);
    printf("%d hard disks:\n", config.hdisks);
    for (i=0; i<config.hdisks; i++) {
	dp = &hdisktab[i];
	if (! dp->dev_name)
	    break;
	print_disk(i, dp);
    }
}

/* final disk init, mapping config into bios */
void map_disk_params(void)
{
    int i;

    /* BIOS must be in already, because we modify it. */
    ASSERT(*(Bit8u*)((BIOSSEG<<4)+BIOSSIZE-1) == BIOSSIG);

    /* cmos disk structures are set in cmos.c */

    for(i=0; i<config.fdisks; i++) {
	struct vdisk *dp = &fdisktab[i];
	if (i==1) {
	    *(Bit8u*)(INT1E_ADD + 11) = dp->tracks-1;
	    *(Bit8u*)(INT1E_ADD + 13) = dp->cmos;
	}
    }

    for(i=0; i<config.hdisks && i<4; i++) {
	struct vdisk *dp = &hdisktab[i];
	*(Bit16u*)(INT41_ADD + i*0x10     ) = dp->tracks;
	*(Bit8u*) (INT41_ADD + i*0x10 +  2) = dp->heads;
	*(Bit8u*) (INT41_ADD + i*0x10 + 14) = dp->sectors;
	if (dp->heads > 8)
	    *(Bit8u*) (INT41_ADD + i*0x10 + 8) = 8;
    }
}


void disk_create(const char *name, disk_t type, int floppy, int ro)
{
    struct vdisk *dp;

    ASSERT(name);

    if (floppy) {
	if (config.fdisks >= MAX_FDISKS) {
	    dprintf("too many floppies  (primus?)\n");
	    leaveemu(ERR_CONFIG);
	}
	dp = &fdisktab[config.fdisks];
	config.fdisks ++;
    } else {
	if (config.hdisks >= MAX_HDISKS) {
	    dprintf("too many hard disks\n");
	    leaveemu(ERR_CONFIG);
	}
	dp = &hdisktab[config.hdisks];
	config.hdisks ++;
    }
    dp->dev_name = strdup(name);
    dp->type = type;
    dp->wantrdonly = ro;
    dp->removeable = floppy;
#ifndef NDEBUG
    dp->read_count = 0;
    dp->write_count = 0;
#endif

    disk_open(dp);
}

void disk_open(struct vdisk *dp)
{
    int i;

    ASSERT(dp);

    if (dp->type == IMAGE) {
	int erc;
    	struct stat buf;
	u_int mode;

	mode = dp->wantrdonly ? O_RDONLY : O_RDWR;
	dp->fdesc = open(dp->dev_name, mode, 0);
	if (dp->fdesc < 0) {
	    debug_disk("open: %s: %s\n", dp->dev_name, strerror(errno));
	    if (errno == EROFS || errno == EACCES) {
		mode = O_RDONLY;
		dp->fdesc = DOS_SYSCALL(open(dp->dev_name, mode, 0));
		if (dp->fdesc < 0) {
		    error_disk("can't open %s for read nor write: %d\n", dp->dev_name, errno);
		    goto fail;
		} else {
		    dp->mode = mode;
		    debug_disk("%s opened as readonly\n", dp->dev_name);
		}
	    } else {
		error_disk("can't open %s, errno=%d\n", dp->dev_name, errno);
	    fail:
		leaveemu(ERR_DISK);
	    }
	} else
	    dp->mode = mode;

	if ((erc = fstat(dp->fdesc, &buf)) < 0) {
	    dprintf("stat: %s: %s\n", dp->dev_name, strerror(errno));
	    leaveemu(ERR_DISK);
	}

	dp->cmos = 0;
	if (dp->removeable)
	    for(i=0; i<FLOPPY_TYPES; i++) {
		if (floppies[i][0] == buf.st_size) {
		    dp->heads = floppies[i][1];
		    dp->sectors = floppies[i][2];
		    dp->tracks = floppies[i][3];
		    dp->cmos = i+1;
		    break;
		}
	    }
	
	if (dp->cmos == 0) {
#define ROUNDUP(i, size) (((i) + ((size)-1)) & ~((size)-1))
	    dp->heads = 16;
	    dp->sectors = 64;
	    dp->tracks = ROUNDUP(buf.st_size, dp->heads*dp->sectors) / (dp->heads*dp->sectors);
	}
    } else {
#if 0
	if (ioctl(dp->fdesc, FD_GTYPE, &fl) == -1) {
	    int err = errno;
	    debug_disk("floppy gettype: %s\n", strerror(err));
	    if (err == ENODEV || err == EIO) {	/* no disk available */
		dp->heads = 0;
		dp->sectors = 0;
		dp->tracks = 0;
		return;
	    }
	    error_disk("can't get floppy parameter of %s (%s)\n", dp->dev_name, strerror(err));
	    leaveemu(ERR_DISK);
	}
	dp->sectors = fl.sectrac;
	dp->heads = fl.heads;
	dp->tracks = fl.tracks;
#else
	dprintf("don't yet support anything but images\n");
	leaveemu(ERR_DISK);
#endif
    }

    debug_disk("%s=%s h=%d, s=%d, t=%d\n", dp->type==IMAGE ? "image" : "device",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks);
}



void disk_close(void)
{
    struct vdisk *dp;

    for (dp = fdisktab; dp < &fdisktab[config.fdisks]; dp++) {
	if (dp->removeable && dp->fdesc >= 0) {
	    debug_disk("DISK: Closing a disk\n");
	    close(dp->fdesc);
	    dp->fdesc = -1;
	}
    }
}


static int read_offset(struct vdisk *dp, char *buffer, u_int offset, u_int count)
{
    ASSERT(dp);
    ASSERT(offset % SECTOR_SIZE == 0);

    if (verify_lina_range((u_int)buffer, PG_W, count*SECTOR_SIZE))
	return -1;

    if (dp->type == IMAGE) {
	int erc, i;
	debug_disk("Reading 0x%04x bytes from %s to %p\n", count*SECTOR_SIZE,
		   dp->dev_name, buffer);

	/* FIXME XXX HACK HACK HACK BUG BUG BUG
	   Screw effiency; I want correctness.  There is a bizarre bug
	   that causes the 9 sector read which linux does to load
	   setup.S to be incorrect under certain circumstances.  If
	   the binary has been modified or merely touched, this 4.5k
	   read will return some parts as garbage.  A second read and
	   all subsequent will be correct.  Go figure.
	*/

	for(i=0; i<2; i++) {
	    if ((erc = lseek(dp->fdesc, offset, SEEK_SET)) != offset) {
		dprintf("read_offset(%x, %x, %x, %x): lseek returned %d, errno=%d\n",
			(u_int)dp, (u_int)buffer, offset, count, erc, errno);
		leaveemu(ERR_DISK);
	    }
	    if ((erc = read(dp->fdesc, buffer, count*SECTOR_SIZE)) != count*SECTOR_SIZE) {
		dprintf("read_offset(%x, %x, %x, %x): read returned %d, errno=%d\n",
			(u_int)dp, (u_int)buffer, offset, count, erc, errno);
		leaveemu(ERR_DISK);
	    }
	}

	return erc;
    } else {
	error_disk("trying to read from a non-image dp\n");
	/* nothing else is supported right now */
	return -1;
    }
}


inline u_int CHS_to_LBA(struct vdisk *dp, u_int c, u_int h, u_int s)
{
    if (!dp)
	return 0;

    return ( (c * dp->heads + h ) * dp->sectors ) + s - 1;
}

int read_sectors(struct vdisk *dp, char *buffer, u_int c, u_int h, u_int s, u_int count)
{
    ASSERT(dp);
    ASSERT(s!=0);

    trace_disk("reading sector into 0x%08x - 0x%08x from %x:%x:%x\n",
	       (u_int)buffer, (u_int)buffer + count*SECTOR_SIZE-1, c, h, s);

#ifndef NDEBUG
    dp->read_count ++;
#endif
    return read_offset(dp, buffer, CHS_to_LBA(dp, c, h, s)*SECTOR_SIZE, count);
}

int write_sectors(struct vdisk *dp, char *buffer, u_int c, u_int h, u_int s, u_int count)
{
    ASSERT(dp);

#ifndef NDEBUG
    dp->write_count ++;
#endif
    if (dp->type == IMAGE) {
	if (lseek(dp->fdesc, CHS_to_LBA(dp, c, h, s)*SECTOR_SIZE, SEEK_SET)==-1)
	    return -1;
	if (verify_lina_range((u_int)buffer, ~PG_W, count*SECTOR_SIZE))
	    return -1;
	return write(dp->fdesc, buffer, count*SECTOR_SIZE);
    } else {
	error_disk("trying to write to a non-image dp\n");
	/* nothing else is supported right now */
	return -1;
    }
}


void boot(struct vdisk *dp)
{
    char *buffer;

    ASSERT(dp);
    ASSERT(checkdp(dp)==0);

    debug_disk("trying to boot from %s\n", dp->dev_name);

    if (verify_lina_range(BOOT_ADDR, PG_W, SECTOR_SIZE)) {
	dprintf("low mem not mapped\n");
	leaveemu(ERR_ASSERT);
    }

    if (checkdp(dp)) {
	error_disk("failed to open boot file %s\n", dp->dev_name);
	leaveemu(ERR_CONFIG);
    }

    buffer = (char *) BOOT_ADDR;
    debug_disk ("Booting from bootfile=%s...\n",dp->dev_name);
    if (read_offset(dp, buffer, 0, 1) < 0) {
	error_disk("%s: %s\n", dp->dev_name, strerror(errno));
	leaveemu(ERR_DISK);
    }
	
#if 0
    if (dp->type == PARTITION) {/* we boot partition boot record, not MBR! */
	debug_disk("Booting partition boot record from part=%s....\n", dp->dev_name);
	if (read(dp->fdesc, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
	    error_disk("reading partition boot sector using partition %s.\n", dp->dev_name);
	    leaveemu(16);
	}
    } else if (read_sectors(dp, buffer, 0, 0, 1, 1) != SECTOR_SIZE) {
	error_disk("can't boot from %s\n", dp->dev_name);
	dp = hdisktab;
	debug_disk("using harddisk (%s) instead\n", dp->dev_name);
	if (read_sectors(dp, buffer, 0, 0, 1, 1) != SECTOR_SIZE) {
	    error_disk("can't boot from hard disk\n");
	    leaveemu(16);
	}
    }
#endif
}


inline void disk_error(struct vdisk *dp, Bit8u error)
{
    HI(ax) = error;
    set_CF();
    if (dp)
	dp->error=error;
}

inline void disk_ok(struct vdisk *dp)
{
    HI(ax) = DERR_NOERR;
    clear_CF();
    if (dp)
	dp->error=DERR_NOERR;
}

/*
 * Notes on how the BIOS treats the disk:
 *
 *  floppies                          0
 *  hard drives                       0x80
 *  heads         are numbered from   0
 *  sectors                           1
 *  tracks                            0
 *
 * most to least significant:
 * track  head  sector
 *
 */

void int13(u_char i)
{
    unsigned int disk, head, sect, track, number;
    int res;
    char *buffer;
    struct vdisk *dp;
    int ah;

    disk = LO(dx);
    if (disk <= config.fdisks) {
	dp = &fdisktab[disk];
	switch (HI(ax)) {
#define DISKETTE_MOTOR_TIMEOUT (*((unsigned char *)0x440))
	    /* NOTE: we don't need this counter, but older games seem to rely
	     * on it. We count it down in INT08 (bios.S) --SW, --Hans
	     */
	case 0:
	case 2:
	case 3:
	case 5:
	case 10:
	case 11:
	    DISKETTE_MOTOR_TIMEOUT = 3 * 18;	/* set timout to 3 seconds */
	    break;
	}
    } else if (disk >= 0x80 && disk < 0x80 + config.hdisks) {
	dp = &hdisktab[disk - 0x80];
	debug_disk("trying to open hd, %d, %p\n", disk, dp);
    } else {
	dp = NULL;
    }

    trace_disk("int 0x13, ah=0x%02x\n", HI(ax));
    ah = HI(ax);
    switch (ah) {
    case 0:			/* init */
	debug_disk("DISK %d init\n", disk);
	if (checkdp(dp))
	    disk_error(dp, DERR_CONTROLLER);
	else {
	    /* dp->sectors =  */
	    disk_ok(dp);
	}
	break;

    case 1:			/* read error code into AL */
	debug_disk("DISK %d error code\n", disk);
	LO(ax) = dp ? dp->error : DERR_CONTROLLER;
	if (LO(ax) == DERR_NOERR)
	    clear_CF();
	else
	    set_CF();
	break;

    case 2:			/* read */
    case 4:                      /* verify */
	FLUSHDISK(dp);
	head = HI(dx);
	sect = (REG(ecx) & 0x3f);
	track = HI(cx) | ((REG(ecx) & 0xc0) << 2);
	/* buffer = SEG_ADR((char *), es, bx); */
	buffer = (char *)make_lina(REG(es), REG(ebx));
	number = LO(ax);
	debug_disk("DISK %d read/verify %d sectors at t:%d,h:%d,s:%d = %d -> %p\n",
		   disk, number, track, head, sect,
		   CHS_to_LBA(dp, track, head, sect), (void *) buffer);

	if (checkdp(dp)) {
	    disk_error(dp, DERR_CONTROLLER);
	    break;
	} else if (head >= dp->heads ||
		   sect > dp->sectors || track >= dp->tracks) {
	    debug_disk("DISK %d sector not found\n", disk);
	    disk_error(dp, DERR_NOTFOUND);
	    break;
	}
	if (ah==2) {
	    res = read_sectors(dp, buffer, track, head, sect, number);
	} else {
	    res = number*SECTOR_SIZE;
	}

	if (res < 0) {
	    disk_error(dp, -res);
	    break;
	} else if (res & 511) {	/* must read multiple of 512 bytes */
	    disk_error(dp, DERR_BADSEC);
	    break;
	}
	LWORD(eax) = res >> 9;
	disk_ok(dp);
	debug_disk("DISK %d read OK.\n", disk);
	break;

    case 3:			/* write */
	debug_disk("int 0x13, write\n");
	FLUSHDISK(dp);
	head = HI(dx);
	sect = (REG(ecx) & 0x3f);
	track = (HI(cx)) | ((REG(ecx) & 0xc0) << 2);
	buffer = (char *)make_lina(REG(es), REG(ebx));
	number = LO(ax);

	debug_disk("DISK %d write %d sectors at t:%d,h:%d,s:%d = %d -> %p\n",
		   disk, number, track, head, sect,
		   CHS_to_LBA(dp, track, head, sect), (void *) buffer);

	if (checkdp(dp)) {
	    disk_error(dp, DERR_CONTROLLER);
	    break;
	} else if (head >= dp->heads ||
		   sect > dp->sectors || track >= dp->tracks) {
	    debug_disk("DISK %d sector not found\n", disk);
	    disk_error(dp, DERR_NOTFOUND);
	    break;
	}
	if (dp->mode & O_RDONLY) {
	    if (dp->removeable)
		disk_error(dp, DERR_WP);
	    else
		disk_error(dp, DERR_WRITEFLT);
	    break;
	}
	res = write_sectors(dp, buffer, track, head, sect, number);

	if (res < 0) {
	    disk_error(dp, -res);
	    break;
	} else if (res & 511) {	/* must write multiple of 512 bytes */
	    disk_error(dp, DERR_BADSEC);
	    break;
	}
	LWORD(eax) = res >> 9;
	disk_ok(dp);
	debug_disk("DISK %d write OK.\n", disk);
	break;

   case 5:                      /* format */
   case 6:                      /* format track, set bad sector flags */
   case 7:                      /* format drive starting at track */
       /* FIXME: could do this as a write */
       disk_error(dp, DERR_CONTROLLER);
       break;

    case 8:			/* get disk drive parameters */
	debug_disk("disk get parameters %d\n", disk);

	if (checkdp(dp)) {
	    disk_error(dp, DERR_CONTROLLER);
	    break;
	}

	LO(ax) = 0;
	disk_ok(dp);
	LO(bx) = dp->cmos;
	/* bh untouched */
	LO(cx) = dp->sectors | (((dp->tracks - 1) & 0x300) >> 2);
	HI(cx) = (dp->tracks - 1) & 0xff;
	LO(dx) = (disk < 0x80) ? config.fdisks : config.hdisks;
	HI(dx) = dp->heads - 1;
	break;

    case 0x9:			/* initialise drive from bpb */
	if (checkdp(dp))
	    disk_error(dp, DERR_CONTROLLER);
	else
	    disk_ok(dp);
	break;

    case 0x0A:			/* read long sectors */
    case 0x0B:                  /* write long sectors */
	disk_error(dp, DERR_BADCMD); /* uses ECC; is optional */
	break;

    case 0x0C:			/* explicit seek heads */
    case 0x0D:			/* Drive reset (hd only) */
	disk_ok(dp);
	break;

    case 0x0E:			/* read/write sector buffer */
    case 0x0F:
	disk_error(dp, DERR_CONTROLLER);
	break;

    case 0x10:			/* Test drive is ok */
    case 0x11:			/* Recalibrate */
    case 0x12:			/* controller RAM diagnostics */
    case 0x13:                  /* controller diagnostics */
    case 0x14:			/* AT diagnostics */
	if (checkdp(dp))
	    disk_error(dp, DERR_CONTROLLER);
	else
	    disk_ok(dp);
	break;

    case 0x15:			/* Get type */
	debug_disk("disk gettype for disk 0x%x\n", disk);
	if (checkdp(dp)) {
	    disk_error(dp, DERR_CONTROLLER);
	    HI(ax) = 0;	/* disk not there */
	    break;
	}
	    
	if (disk >= 0x80) {
	    if (dp->cmos!=0 || dp->removeable) {
		HI(ax) = 1;	/* floppy disk, change detect (1=no, 2=yes) */
		LWORD(edx) = 0;
		LWORD(ecx) = 0;
	    } else {
		HI(ax) = 3;	/* fixed disk */
		number = dp->tracks * dp->sectors * dp->heads;
		LWORD(ecx) = number >> 16;
		LWORD(edx) = number & 0xffff;
	    }
	} else {
	    HI(ax) = 1;	/* floppy, no change detect=1 */
	}
	disk_ok(dp);
	break;

    case 0x16:  /* get disk change status */
	if (checkdp(dp)) {
	    disk_error(dp, DERR_CONTROLLER);
	    break;
	}
	if (disk >= 0x80) {
	    clear_CF();  /* change line inactive */
	    HI(ax) = 0;
	} else {
	    set_CF();
	    HI(ax) = 6;  /* active or not supported */
	}
	dp->error = DERR_NOERR;
	break;

    case 0x17:  /* set disk type */
	/* FIXME: change CMOS type? */
	disk_error(dp, DERR_BADCMD);
	break;

    case 0x18:			/* Set media type for format */
	track = HI(cx) + ((LO(cx) & 0xc0) << 2);
	sect = LO(cx) & 0x3f;
	disk_error(dp, DERR_BADCMD);
	break;

	/* 0x19 - 0x1C  -- ESDI drive commands */
	/* 0x22 - 0x25  -- PS/1 PS/2 hard drive commands */
	/* 0x41         -- installation check,  MS crap */
	/* 0x42         -- extended read */
	/* 0x43         -- extended write */
	/* 0x4?         -- cdrom  */
	/* 0xdc         -- windows disk interrupt? */

    case 0xf9:			/* SWBIOS installation check */
	set_CF();
	break;
    case 0xfe:			/* SWBIOS get extended cyl count */
	if (dp)
	    LWORD(edx) = dp->tracks % 1024;
	clear_CF();
	break;
    default:
	error_disk("disk error, unknown command: int13, ax=0x%x\n", LWORD(eax));
	disk_error(dp, DERR_BADCMD);
	break;
    }

#if 1  /* errors are okay, like if probing for disks */
    if (dp->error != DERR_NOERR && dp->error != DERR_NOTFOUND) {
	error_disk("disk error for ah=0x%02x\n", ah);
    }
#endif
    trace_disk("leaving int 0x13; %x:%x %x:%x\n", REG(cs), REG(eip), REG(ss), REG(esp));
}
