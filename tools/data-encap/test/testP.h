
#ifndef __TEST_P_H__
#define __TEST_P_H__

#include "test.h"

struct blah
{
  int value;
};

struct test
{
  int field_1;
  struct blah b;
  struct blah *bb;
  struct blah bbb[2];
  char array[20];
};

ALLOCATOR(test)
SIZEOF(test)
DESTRUCTOR(test)

ALLOCATOR(blah)
DESTRUCTOR(blah)

FIELD_SIMPLE_READER(test,field_1,int)
FIELD_ASSIGN(test,field_1,int)

FIELD_PTR_READER(test,b,struct blah *)
FIELD_COPYIN(test,b,struct blah *)

FIELD_SIMPLE_READER(test,bb,struct blah *)
FIELD_ASSIGN(test,bb,struct blah *)

FIELD_SIMPLE_READER(test,array,char *)
FIELD_SIZEOF(test,array)

FIELD_SIMPLE_READER(blah,value,int)
FIELD_ASSIGN(blah,value,int)

ARRAY_PTR_READER(test,array,char*)
ARRAY_ASSIGN(test,array,char)
ARRAY_COPYIN(test,array,char*)

FIELD_SIMPLE_READER(test,bbb,struct blah *)
ARRAY_PTR_READER(test,bbb,struct blah *)
ARRAY_COPYIN(test,bbb,struct blah *)

GEN_ARRAY_OFFSET(blah);

#endif

