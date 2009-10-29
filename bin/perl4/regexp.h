/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */

/*
 * Revision 1.1.1.1  1998/06/15 21:09:09  pinckney
 * Initial checkin of the exokernel sources from the MIT repository. This 
 * coresponds to sometime after the v1.0.6 snapshot.
 *
 * Revision 1.1  1996/09/29 07:43:16  elliotw
 * Initial checkin of Perl 4.036.
 * In order to use perl, you must modify your locore.S to enable the floating point device.  Do a "grep -n floating ~elliotw/exopc/sys/kern/locore.S" to find out how.
 * This is a work in progress, and many parts are still not fully functional.
 *
 * Revision 4.0.1.2  91/11/05  18:24:31  lwall
 * patch11: minimum match length calculation in regexp is now cumulative
 * patch11: initial .* in pattern had dependency on value of $*
 * 
 * Revision 4.0.1.1  91/06/07  11:51:18  lwall
 * patch4: new copyright notice
 * patch4: // wouldn't use previous pattern if it started with a null character
 * patch4: $` was busted inside s///
 * 
 * Revision 4.0  91/03/20  01:39:23  lwall
 * 4.0 baseline.
 * 
 */

typedef struct regexp {
	char **startp;
	char **endp;
	STR *regstart;		/* Internal use only. */
	char *regstclass;
	STR *regmust;		/* Internal use only. */
	int regback;		/* Can regmust locate first try? */
	int minlen;		/* mininum possible length of $& */
	int prelen;		/* length of precomp */
	char *precomp;		/* pre-compilation regular expression */
	char *subbase;		/* saved string so \digit works forever */
	char *subbeg;		/* same, but not responsible for allocation */
	char *subend;		/* end of subbase */
	char reganch;		/* Internal use only. */
	char do_folding;	/* do case-insensitive match? */
	char lastparen;		/* last paren matched */
	char nparens;		/* number of parentheses */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#define ROPT_ANCH 1
#define ROPT_SKIP 2
#define ROPT_IMPLICIT 4

regexp *regcomp();
int regexec();
