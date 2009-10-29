
#ifndef __VOS_WK_H__
#define __VOS_WK_H__

#include <sys/types.h>
#include <xok/wk.h>
#include <vos/errno.h>


#define SECONDS2TICKS(secs) ((secs * 1000000) / __sysinfo.si_rate)
#define TICKS2SECONDS(ticks) ((ticks * __sysinfo.si_rate) / 1000000)

#define USECONDS2TICKS(usecs) (usecs / __sysinfo.si_rate)
#define TICKS2USECONDS(ticks) (ticks * __sysinfo.si_rate)


/* 
 * wk_waitfor_pred_directed: constructs and downloads a large wakeup predicate
 * composed of the given predicate and all the registered predicates. After
 * downloading the wakeup predicate, put process to sleep by yielding to the
 * given envid. If one of the registered predicates causes the process to
 * wakeup, the associated callback will be invoked and downloading and
 * sleeping occurs again. If the process wakes up because a given predicate is
 * evaluated to be true, the wakeup predicate is removed and the procedure
 * reutrns 0. Procedure returns -1 if failure occurs. Errno is set to be:
 *
 *   V_NOMEM:       no more memory, cannot constrct a large wakeup predicate.
 *   V_WK_BADSZ:    bad size given for the wk_term.
 *   V_WK_BADREG:   bad registered wakeup predicate encountered.
 *   V_WK_DLFAILED: wakeup predicate downloading failed.
 */

extern int 
wk_waitfor_pred_directed (struct wk_term *t, u_int sz, int envid);
#define wk_waitfor_pred(t, sz) wk_waitfor_pred_directed((t), (sz), -1)


/* wk_register_extra, wk_deregister_extra: registering and deregistering extra
 * predicates that should be attached to all downloaded predicates. The
 * "construct" routine will be called at predicate construction time, and the
 * "callback" routine will be called if the provided predicate component wakes
 * the sleeper. Both will be given "param" as a parameter. "maxlen" must be
 * the upper bound on the predicate returned by "construct". 
 *
 * wk_register_extra returns an index to a table of registered predicates if
 * successful, -1 otherwise. Errno is set to:
 * 
 *   V_WK_MTERM: too many predicates already.
 *
 * wk_deregister_extra returns 0 if successful, -1 otherwise. Errno is set to
 *
 *   V_WK_TNOTFOUND: not found in table of registered predicates.
 */

extern int 
wk_register_extra (int (*construct)(struct wk_term *,u_quad_t), 
                   void (*callback)(u_quad_t), u_quad_t param, u_int maxlen);

extern int 
wk_deregister_extra (int (*construct)(struct wk_term *,u_quad_t), u_quad_t param);


/* wk_waitfor_timeout: constructs and downloads a predicate that is composed
 * of the given predicate and a sleeping predicate sleeping for the number of
 * ticks given. Calls wk_wait_for_pred_directed with the new predicate. If the
 * process wakes up because the given predicate is triggered, returns 0.
 * Otherwise, return -1. Errno is set to:
 *
 *   V_WOULDBLOCK: the sleeping predicate is triggered
 *   V_WAKENUP:    neither predicate was triggered, process wakenup for other
 *                 reasons (e.g. ipc)
 *   other errno values defined for wk_waitfor_pred_directed
 */
extern int 
wk_waitfor_timeout (struct wk_term *u, int sz0, int ticks, int envid);


/* wk_sleep, wk_sleep_usecs: put process to sleep for the given number of
 * seconds/u-seconds.  Always return -1. Errno is set to errno values defined
 * for wk_waitfor_timeout.
 */
extern int
wk_sleep(u_int secs, int envid);
extern int
wk_sleep_usecs(u_int usecs, int envid);


/* wk_waitfor_freepages: put process to sleep waiting for free pages to
 * surface. Wait for (1) the number of free pages to increase, or (2) the
 * number of dirty pages to decrease, or (3) the number of pinned pages to
 * decrease. Any of these can have the effect of making more allocatable
 * pages. Returns 0 if any of these conditions caused the process to wakeup.
 * Returns -1 if errno. Errno is set to:
 *
 *   V_NOMEM: no page available to start with
 *   other errno values defined for wk_waitfor_pred_directed
 */
extern int 
wk_waitfor_freepages();


/* wk_waitfor_value_xxx: constructs and downloads a predicate that waits for
 * the relation between value at addr and the given value to occur. These
 * procedures wait while loops until the actual condition occurs.
 */
void 
wk_waitfor_value (int *addr, int value, u_int cap);
void 
wk_waitfor_value_neq (int *addr, int value, u_int cap);
void 
wk_waitfor_value_lt (int *addr, int value, u_int cap);
void 
wk_waitfor_value_gt (int *addr, int value, u_int cap);


/* wk_mkxxxx_pred: generic constructors for defining wk predicate terms.
 * The upper size limit of each term is given by the #defined value. The
 * actual size of the term is returned.
 */

#define UWK_MKCMP_EQ_PRED_SIZE 3
extern int 
wk_mkcmp_eq_pred (struct wk_term *t, int *addr, int value, u_int cap);

#define UWK_MKCMP_NEQ_PRED_SIZE 3
extern int 
wk_mkcmp_neq_pred (struct wk_term *t, int *addr, int value, u_int cap);

#define UWK_MKENVDEATH_PRED_SIZE 10
extern int 
wk_mkenvdeath_pred (struct wk_term *t);

#define UWK_MKSLEEP_PRED_SIZE 10
extern int 
wk_mksleep_pred (struct wk_term *t, u_quad_t ticks);
extern int 
wk_mkusleep_pred (struct wk_term *t, u_int usecs);

#define UWK_MKTRUE_PRED_SIZE 3
extern int 
wk_mktrue_pred(struct wk_term *t);

#define UWK_MKFALSE_PRED_SIZE 3
extern int 
wk_mkfalse_pred(struct wk_term *t);

#endif  /* __VOS_WK_H__ */

