#include "types.h"
#include "debug.h"
#include "set.h"

void set_init(Set *s)
{
    ASSERT(s);
    s->items = 0;
}

int set_is_empty(Set *s)
{
    ASSERT(s);
    return s->items == 0;
}

void set_add(Set *s, u_int val)
{
    int i;
    ASSERT(s);

    for(i=0; i < s->items && s->set[i]!=val; i++);
    ASSERT(i < SET_SIZE);
    if (! (i < s->items)) {
	s->set[i] = val;
	s->items ++;
    }
}

int set_get_any(Set *s, u_int *val)
{
    ASSERT(s && val);
    if (s->items == 0)
	return -1;
    *val = s->set[0];
    return 0;	
}

int set_exists(Set *s, u_int val)
{
    int i;
    ASSERT(s);

    for(i=0; i<s->items && s->set[i]!=val; i++);
    return i<s->items;
}

int set_exists_within_delta(Set *s, u_int val, u_int delta, u_int *result)
{
    int i;
    ASSERT(s && result);

    for(i=0; i<s->items; i++) {
	if (val >= s->set[i] && val < s->set[i]+delta) {
	    *result = s->set[i];
	    return 1;
	}
    }
    return 0;
}

#include "pagetable.h"
int set_exists_within_delta_rounded(Set *s, u_int val, u_int delta)
{
    int i;
    ASSERT(s);

    for(i=0; i<s->items; i++) {
	if (val >= PGROUNDDOWN(s->set[i]) && val < PGROUNDUP(s->set[i]+delta)) {
	    /* *result = s->set[i]; */
	    return 1;
	}
    }
    return 0;
}


int set_del(Set *s, u_int val)
{
    int i, j;
    ASSERT(s);

    for(i=0; i<s->items && s->set[i]!=val; i++);
    if (s->set[i]==val) {
	for(j=i; j+1 < s->items; j++)
	    s->set[j] = s->set[j+1];
	s->items --;
	return 1;
    }
    return 0;
}


void set_iter_init(Set *s)
{
    ASSERT(s);
    s->iter = 0;
}

int set_iter_get(Set *s, u_int *val)
{
    ASSERT(s && val);
    if (s->iter < s->items) {
	*val = s->set[s->iter];
	return 1;
    } else
	return 0;
}

void set_iter_next(Set *s)
{
    ASSERT(s);
    s->iter ++;
}
