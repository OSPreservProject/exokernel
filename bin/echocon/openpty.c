#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <unistd.h>
#include <memory.h>
#include <grp.h>
#include <err.h>

#include <sys/tty.h>
/*#include "util.h"*/

#ifndef EXOPC
#define kprintf printf
#endif
#define TTY_LETTERS "pqrstuvwxyzPQRST"

int
openpty(amaster, aslave, name, termp, winp)
        int *amaster, *aslave;
        char *name;
        struct termios *termp;
        struct winsize *winp;
{
#ifdef EXOPC
        static char line[] = "/dev/ptyXX";
#else
        static char line[] = "/dev/ptyXX";
#endif
        register const char *cp1, *cp2;
        register int master, slave;

        for (cp1 = TTY_LETTERS; *cp1; cp1++) {
                line[8] = *cp1;
                for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
                        line[9] = *cp2;
                        line[5] = 'p';
                        if ((master = open(line, O_RDWR, 0)) == -1) {
                                if (errno == ENOENT)
                                        return (-1);    /* out of ptys */
                        } else {
			  line[5] = 't';
                                if ((slave = open(line, O_RDWR, 0)) != -1) {
                                        *amaster = master;
                                        *aslave = slave;
#if 0
					do {
			    int i;
			    ioctl(slave,TIOCEXCL,0); 
			    ioctl(master,TIOCEXCL,0); 
			    i = open(line, O_RDWR, 0);
			    printf("open slave twice: %d errno: %d\n",i
				 ,errno);
			  } while(0);
#endif

                                        if (name)
                                                strcpy(name, line);
                                        if (termp)
                                                (void) tcsetattr(slave, 
                                                        TCSAFLUSH, termp);
                                        if (winp)
                                                (void) ioctl(slave, TIOCSWINSZ, 
                                                        (char *)winp);
                                        return (0);
                                }
				kprintf("CLOSE MASTER\n");
                                (void) close(master);
                        }
                }
        }
        errno = ENOENT; /* out of ptys */
        return (-1);
}


