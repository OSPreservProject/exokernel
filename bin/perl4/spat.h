/*
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * Revision 1.1.1.1  1998/06/15 21:09:09  pinckney
 * Initial checkin of the exokernel sources from the MIT repository. This 
 * coresponds to sometime after the v1.0.6 snapshot.
 *
 * Revision 1.1  1996/09/29 07:43:16  elliotw
 * Initial checkin of Perl 4.036.
 * In order to use perl, you must modify your locore.S to enable the floating point device.  Do a "grep -n floating ~elliotw/exopc/sys/kern/locore.S" to find out how.
 * This is a work in progress, and many parts are still not fully functional.
 *
 * Revision 4.0.1.1  91/06/07  11:51:59  lwall
 * patch4: new copyright notice
 * patch4: added global modifier for pattern matches
 * 
 * Revision 4.0  91/03/20  01:39:36  lwall
 * 4.0 baseline.
 * 
 */

struct scanpat {
    SPAT	*spat_next;		/* list of all scanpats */
    REGEXP	*spat_regexp;		/* compiled expression */
    ARG		*spat_repl;		/* replacement string for subst */
    ARG		*spat_runtime;		/* compile pattern at runtime */
    STR		*spat_short;		/* for a fast bypass of execute() */
    short	spat_flags;
    char	spat_slen;
};

#define SPAT_USED 1			/* spat has been used once already */
#define SPAT_ONCE 2			/* use pattern only once per reset */
#define SPAT_SCANFIRST 4		/* initial constant not anchored */
#define SPAT_ALL 8			/* initial constant is whole pat */
#define SPAT_SKIPWHITE 16		/* skip leading whitespace for split */
#define SPAT_FOLD 32			/* case insensitivity */
#define SPAT_CONST 64			/* subst replacement is constant */
#define SPAT_KEEP 128			/* keep 1st runtime pattern forever */
#define SPAT_GLOBAL 256			/* pattern had a g modifier */

EXT SPAT *curspat;		/* what to do \ interps from */
EXT SPAT *lastspat;		/* what to use in place of null pattern */

EXT char *hint INIT(Nullch);	/* hint from cmd_exec to do_match et al */

#define Nullspat Null(SPAT*)
