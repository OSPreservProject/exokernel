
#ifndef __TEST_H__
#define __TEST_H__

#include "../encap.h"

struct blah;
struct test;

ALLOCATOR_DECL(test);
SIZEOF_DECL(test);
DESTRUCTOR_DECL(test);

ALLOCATOR_DECL(blah);
DESTRUCTOR_DECL(blah);

FIELD_READER_DECL(test,field_1,int)
FIELD_READER_DECL(test,b,struct blah *)
FIELD_READER_DECL(test,bb,struct blah *)
FIELD_READER_DECL(test,bbb,struct blah *)
ARRAY_READER_DECL(test,bbb,struct blah *)
FIELD_READER_DECL(test,array,char *)

FIELD_ASSIGN_DECL(test,field_1,int)
FIELD_COPYIN_DECL(test,b,struct blah *)
FIELD_ASSIGN_DECL(test,bb,struct blah *)
ARRAY_COPYIN_DECL(test,bbb,struct blah *)
FIELD_ASSIGN_DECL(test,array,char *)

FIELD_SIZEOF_DECL(test,array)

FIELD_READER_DECL(blah,value,int)
FIELD_ASSIGN_DECL(blah,value,int)

ARRAY_READER_DECL(test,array,char*)
ARRAY_ASSIGN_DECL(test,array,char)
ARRAY_COPYIN_DECL(test,array,char*)

GEN_ARRAY_OFFSET_DECL(blah)

#endif

