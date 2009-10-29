/*
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * Revision 1.1.1.2  1998/06/24 00:55:46  ganger
 * Update to 6/23/98
 *
 * Revision 1.2  1998/06/23 16:14:18  pinckney
 * Merge in changes from Greg and Exotec
 *
 * -- updated alfs/webserver/disk progs
 * -- remove many rcs $Id and friends
 * -- added some .cvsignore files
 * -- updated emulator with more ioctls
 * -- fixed bug in backtrace
 * -- updated netcsum
 * -- gettime now only uses tcp
 *
 * Revision 1.2  1998/06/21 14:36:49  pinckney
 * removed more $Id and $Version stuff
 *
 * Revision 1.1.1.1  1998/06/15 21:09:09  pinckney
 * Initial checkin of the exokernel sources from the MIT repository. This 
 * coresponds to sometime after the v1.0.6 snapshot.
 *
 * Revision 1.1  1996/09/29 07:43:12  elliotw
 * Initial checkin of Perl 4.036.
 * In order to use perl, you must modify your locore.S to enable the floating point device.  Do a "grep -n floating ~elliotw/exopc/sys/kern/locore.S" to find out how.
 * This is a work in progress, and many parts are still not fully functional.
 *
 * Revision 4.0.1.2  91/11/05  17:24:31  lwall
 * patch11: random cleanup
 * 
 * Revision 4.0.1.1  91/06/07  11:10:33  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0  91/03/20  01:22:38  lwall
 * 4.0 baseline.
 * 
 */

#define FILLPCT 80		/* don't make greater than 99 */
#define DBM_CACHE_MAX 63	/* cache 64 entries for dbm file */
				/* (resident array acts as a write-thru cache)*/

#define COEFFSIZE (16 * 8)	/* size of coeff array */

typedef struct hentry HENT;

struct hentry {
    HENT	*hent_next;
    char	*hent_key;
    STR		*hent_val;
    int		hent_hash;
    int		hent_klen;
};

struct htbl {
    HENT	**tbl_array;
    int		tbl_max;	/* subscript of last element of tbl_array */
    int		tbl_dosplit;	/* how full to get before splitting */
    int		tbl_fill;	/* how full tbl_array currently is */
    int		tbl_riter;	/* current root of iterator */
    HENT	*tbl_eiter;	/* current entry of iterator */
    SPAT 	*tbl_spatroot;	/* list of spats for this package */
    char	*tbl_name;	/* name, if a symbol table */
#ifdef SOME_DBM
#ifdef HAS_GDBM
    GDBM_FILE	tbl_dbm;
#else
#ifdef HAS_NDBM
    DBM		*tbl_dbm;
#else
    int		tbl_dbm;
#endif
#endif
#endif
    unsigned char tbl_coeffsize;	/* is 0 for symbol tables */
};

STR *hfetch();
bool hstore();
STR *hdelete();
HASH *hnew();
void hclear();
void hentfree();
void hfree();
int hiterinit();
HENT *hiternext();
char *hiterkey();
STR *hiterval();
bool hdbmopen();
void hdbmclose();
bool hdbmstore();
