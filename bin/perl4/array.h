/*
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct atbl {
    STR	**ary_array;
    STR **ary_alloc;
    STR *ary_magic;
    int ary_max;
    int ary_fill;
    char ary_flags;
};

#define ARF_REAL 1	/* free old entries */

STR *afetch();
bool astore();
STR *apop();
STR *ashift();
void afree();
void aclear();
bool apush();
int alen();
ARRAY *anew();
ARRAY *afake();
void aunshift();
void afill();
