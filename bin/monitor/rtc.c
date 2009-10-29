#include <time.h>
#include <sys/time.h>

#include "emu.h"
#include "cmos.h"
#include "debug.h"
#include "disks.h"
#include "timers.h"
#include "int.h"

static u_short BCD(int binval)
{
    unsigned short tmp1, tmp2;
    
    /* bit 2 of register 0xb set=binary mode, clear=BCD mode */
    if (cmos.subst[CMOS_STATUSB] & 4)
	return binval;
    
    if (binval > 99)
	binval = 99;
    
    tmp1 = binval / 10;
    tmp2 = binval % 10;
    return ((tmp1 << 4) | tmp2);
}

int cmos_date(int reg)
{
#if 0
    unsigned long ticks;
    struct timeval tp;
    struct timezone tzp;
#else
    time_t this_time;
#endif
    struct tm tms;
    struct tm *tm = &tms;
    int tmp;
    
    /* get the time */
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
#else
    this_time = time(0);
    /* tm = localtime(&this_time); */
    /* tm = gmtime(&this_time); */
    tm->tm_sec = 0;
    tm->tm_min = 1;
    tm->tm_hour = 12;
    tm->tm_mday = 1;
    tm->tm_mon = 2;
    tm->tm_year = 1999;
    tm->tm_wday = 1;
    tm->tm_yday = 90;
    tm->tm_isdst = 0;
#endif

    switch (reg) {
    case CMOS_SEC:
	return BCD(tm->tm_sec);
	
    case CMOS_MIN:
	return BCD(tm->tm_min);
	
    case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
	tmp = BCD(tm->tm_hour);
	if (!(cmos.subst[CMOS_STATUSB] & 2)) {
	    if (tmp == 0)
		return 12;
	    else if (tmp > 12)
		return tmp - 12;
	}
	return tmp;
	
    case CMOS_DOW:
	return BCD(tm->tm_wday);
	
    case CMOS_DOM:
	return BCD(tm->tm_mday);
	
    case CMOS_MONTH:
	if (cmos.flag[8])
	    return cmos.subst[8];
	else
	    return BCD(1 + tm->tm_mon);
	
    case CMOS_YEAR:
	if (cmos.flag[9])
	    return cmos.subst[9];
	else
	    return BCD(tm->tm_year);
	
    default:
	debug_cmos("CMOS: cmos_time() register 0x%02x defaulted to 0\n", reg);
	return 0;
    }
    
    /* the reason for month and year I return the substituted values is this:
     * Norton Sysinfo checks the CMOS operation by reading the year, writing
     * a new year, reading THAT year, and then rewriting the old year,
     * apparently assuming that the CMOS year can be written to, and only
     * changes if the year changes, which is not likely between the 2 writes.
     * Since I personally know that dosemu won't stay uncrashed for 2 hours,
     * much less a year, I let it work that way.
     */
    
}


void set_ticks(unsigned long new)
{
    volatile unsigned long *ticks = BIOS_TICK_ADDR;
    volatile unsigned char *overflow = TICK_OVERFLOW_ADDR;
    
    /* ignore_segv++; */
    *ticks = new;
    *overflow = 0;
    /* ignore_segv--; */
    /* warn("TIMER: update value of %d\n", (40 / config.freq)); */
}



