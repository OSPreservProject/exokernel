#ifndef _EXOS_SYNCH_H_
#define	_EXOS_SYNCH_H_

/* HBXX - Emulates OpenBSD's sys/select.h and some of proc.h 
 * the selinfo struct contains a counter that is incremented on every selwakeup.
 * On every selrecord, the current counter of selinfo is recorded and a predicate
 * is built containinig the current count.
 * when selselect_pred is called the predicate is returned, and flushed 
 *
 * CAVEAT - you may call multiple selrecord's, the first selselect_pred will return
 * a predicate that will fire when any selwakeup is used.  Any subsequent selselect_pred
 * not preceded by a selrecord, will return an empty predicate.
 * This is not a problem with select, since it first does all the plain select 
 * (which do the selrecords) and then does the select_pred, the first will return a 
 * complete predicate, and all other will return empty.  When a predicate fires we
 * scan all file descriptors again.  This implementation would be problematic if we
 * were to tag each predicate with the file descriptor belonging to it, and not rescan
 * all file descriptors.  
 */


struct selinfo {
	int counter; 
};

void	selrecord __P((int rw, struct selinfo *));
void	selwakeup __P((struct selinfo *));

/* used for file table select_pred operation */
struct wk_term;
int     selselect_pred __P((int rw, struct wk_term *pred));

int     tsleep __P((void *chan, int pri, char *wmesg, int timo));
void    wakeup __P((void *chan));

struct wk_term;
int tsleep_pred_directed (struct wk_term *u, int sz, 
			  int pri, char *wmesg, int ticks, int envid);
#define tsleep_pred(u,sz,pri,wmesg,ticks) tsleep_pred_directed(u,sz,pri,wmesg,ticks,-1)

extern int lbolt;
extern int hz;
#endif /* !_EXOS_SYNCH_H_ */
