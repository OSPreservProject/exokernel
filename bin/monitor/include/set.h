#ifndef _SET_H_
#define _SET_H_

#include "types.h"

#define SET_SIZE 5

typedef struct {
    u_int items;
    u_int iter;
    u_int set[SET_SIZE];
} Set;


void set_init(Set *s);
int set_is_empty(Set *s);
int set_get_any(Set *s, u_int *val);
void set_add(Set *s, u_int val);
int set_exists(Set *s, u_int val);
int set_exists_within_delta(Set *s, u_int val, u_int delta, u_int *result);
int set_exists_within_delta_rounded(Set *s, u_int val, u_int delta);
int set_del(Set *s, u_int val);
void set_iter_init(Set *s);
int set_iter_get(Set *s, u_int *val);
void set_iter_next(Set *s);


#endif
