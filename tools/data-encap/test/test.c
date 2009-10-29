
#ifndef __ENCAP__
#include "testP.h"
#else
#include "test.h"
#endif

int main()
{
  struct test* a = test_alloc();
  struct blah* b = blah_alloc();
  struct blah* c;

  char *d;
  char *f;
  char g;
  char *h;

  printf("%d\n",test_sizeof());        // should be 40
  printf("%d\n",test_array_sizeof(a)); // should be 20
  
  test_field_1_set(a,99);
  printf("%d\n",test_field_1_get(a));  // should be 99

  INC_FIELD(test,field_1,a,1);
  printf("%d\n",test_field_1_get(a));  // should be 100

  DEC_FIELD(test,field_1,a,50);
  printf("%d\n",test_field_1_get(a));  // should be 50

  blah_value_set(b, 66);
  test_b_copyin(a,b);
  test_bb_set(a,b);

  c = test_b_get(a);
  printf("%d\n",blah_value_get(c));    // should be 66
  blah_value_set(c,77);
  printf("%d\n",blah_value_get(c));    // should be 77
  c = test_b_get(a);
  printf("%d\n",blah_value_get(c));    // should be 77
  c = test_bb_get(a);
  printf("%d\n",blah_value_get(c));    // should be 66

  c = test_b_get(a);
  test_bbb_copyin_at(a,c,0);
  c = test_bb_get(a);
  test_bbb_copyin_at(a,c,1);

  c = test_bbb_get_at(a,0);
  printf("%d\n",blah_value_get(c));    // should be 77
  c = test_bbb_get_at(a,1);
  printf("%d\n",blah_value_get(c));    // should be 66

  d = test_array_get(a);
  strcpy(d,"hello world\0");
  printf("%s\n",d);	               // should be hello world
  f = test_array_get(a);
  printf("%s\n",f);	               // should be hello world

  h = test_array_get_at(a,0);
  printf("%c\n",*h);	               // should be h

  test_array_set_at(a,'_',5);
  g = '!';
  test_array_copyin_at(a,&g,11);
  test_array_set_at(a,'\0',12);
  f = test_array_get(a);
  printf("%s\n",f);	               // should be hello_world!

  c = test_bbb_get(a);
  printf("%d\n",blah_value_get(c));    // should be 77
  INC_PTR(blah,c,1);
  printf("%d\n",blah_value_get(c));    // should be 66

  blah_free(b);
  test_free(a);
  return 0;
}

#ifdef __ENCAP__
#include "testP.h"
#endif


