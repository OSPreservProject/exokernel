
#ifndef __TOOL_ENCAP_H__
#define __TOOL_ENCAP_H__


/* ALLOCATOR 
 *
 * module: name of the structure
 */
#define ALLOCATOR_DECL(module) \
static inline struct module * module##_alloc();

#define ALLOCATOR(module) \
static inline struct module * module##_alloc()			\
{								\
  return (struct module *) malloc(sizeof(struct module));	\
}


/* DESTRUCTOR
 *
 * module: name of the structure
 */
#define DESTRUCTOR_DECL(module) \
static inline void module##_free(struct module *);

#define DESTRUCTOR(module) \
static inline void module##_free(struct module *m)		\
{								\
  free(m);							\
}


/* SIZEOF OF STRUCTURE
 *
 * module: name of structure
 */
#define STRUCT_SIZEOF_DECL(module) \
static inline int module##_sizeof();

#define STRUCT_SIZEOF(module) \
static inline int module##_sizeof()				\
{								\
  return sizeof(struct module);					\
}


/* SIZEOF OF FIELD
 *
 * module: name of structure
 * field: name of field
 */
#define FIELD_SIZEOF_DECL(module, field) \
static inline int module##_##field##_sizeof(struct module *);

#define FIELD_SIZEOF(module, field) \
static inline int module##_##field##_sizeof(struct module *m)	\
{								\
  return sizeof(m->##field##);					\
}


/* FIELD SIMPLE READER
 * 
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 */
#define FIELD_SIMPLE_READER_DECL(module, field, type) \
static inline type module##_##field##_get(struct module *);

#define FIELD_SIMPLE_READER(module, field, type) \
static inline type module##_##field##_get(struct module *m)	\
{								\
  return m->##field##;						\
}


/* FIELD PTR READER
 *
 * module: name of structure
 * field: name of the field
 * type: type of the field w/ * (e.g. int *, struct Ppage *)
 */
#define FIELD_PTR_READER_DECL(module, field, type) \
static inline type module##_##field##_ptr(struct module *);

#define FIELD_PTR_READER(module, field, type) \
static inline type module##_##field##_ptr(struct module *m)	\
{								\
  return &(m->##field##);					\
}


/* FIELD ASSIGN
 *
 * assigns value to a field
 * 
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 */
#define FIELD_ASSIGN_DECL(module, field, type) \
static inline void module##_##field##_set(struct module *, type);

#define FIELD_ASSIGN(module, field, type) \
static inline void module##_##field##_set(struct module *m, type v)	\
{									\
  m->##field## = v;							\
}


/* FIELD COPYIN
 *
 * takes in a pointer to a value, dereference the pointer and do an
 * assignment: similar to memmove, may be we should change to that later
 * 
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 */
#define FIELD_COPYIN_DECL(module, field, type) \
static inline void module##_##field##_copyin(struct module *, type);

#define FIELD_COPYIN(module, field, type) \
static inline void module##_##field##_copyin(struct module *m, type v)	\
{									\
  m->##field## = *v;							\
}


/* ARRAY READER
 *
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 *
 * ARRAY_SIMPLE_READER: makes a function that returns the value of a
 * location in an array field of the structure
 * 
 * ARRAY_PTR_READER: makes a function that returns the ptr to a location
 * in an array field of the structure
 */
#define ARRAY_SIMPLE_READER_DECL(module, field, type) \
static inline type module##_##field##_get_at(struct module *, int);

#define ARRAY_SIMPLE_READER(module, field, type) \
static inline type module##_##field##_get_at(struct module *m, int i)	\
{									\
  return m->##field##[i];						\
}

#define ARRAY_PTR_READER_DECL(module, field, type) \
static inline type module##_##field##_ptr_at(struct module *, int);

#define ARRAY_PTR_READER(module, field, type) \
static inline type module##_##field##_ptr_at(struct module *m, int i)	\
{									\
  return &(m->##field##[i]);						\
}


/* ARRAY ASSIGN
 *
 * assigns value to a location of an array field
 *
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 */ 
#define ARRAY_ASSIGN_DECL(module, field, type) \
static inline void module##_##field##_set_at(struct module *, int, type);

#define ARRAY_ASSIGN(module, field, type) \
static inline void module##_##field##_set_at(struct module *m, int i, type v)\
{									\
  m->##field##[i] = v;							\
}


/* ARRAY COPYIN
 *
 * takes in a pointer to a value, dereference the pointer and do an
 * assignment to a location of an array field: similar to memmove, may be we
 * should change to that later
 *
 * module: name of structure
 * field: name of the field
 * type: type of the field (e.g. int, struct Ppage*, etc)
 */ 
#define ARRAY_COPYIN_DECL(module, field, type) \
static inline void module##_##field##_copyin_at(struct module *, int, type);

#define ARRAY_COPYIN(module, field, type) \
static inline void module##_##field##_copyin_at(struct module *m, int i, type v)\
{									\
  m->##field##[i] = *v;							\
}


/* ARRAY REF
 *
 * returns ptr to an object at a location in an array of structure objects
 *
 * array: name of an array of structure objects
 * type: name of the structure
 */
#define ARRAY_REF_DECL(array, type) \
static inline struct type *##array##_get(int);

#define ARRAY_REF(array, type) \
static inline struct type *##array##_get(int i)	\
{							\
  return &(##array##[i]);				\
}


/* ARRAY REF
 *
 * returns ptr to an object at a location in an array of structure objects
 *
 * type: name of the structure
 */
#define GEN_ARRAY_REF_DECL(type) \
static inline struct type *##type##array_get(struct type *, int);

#define GEN_ARRAY_REF(type) \
static inline struct type *##type##array_get(struct type *b, int i)	\
{									\
  return &(b[i]);							\
}


/* STRUCT OFFSETOF
 *
 * returns the offset of a field in a struct
 *
 * type: name of the structure
 * field: name of the field
 */
#define GEN_OFFSETOF_DECL(type, field) \
static inline size_t type##_##field##_offsetof();

#define GEN_OFFSETOF(type, field) \
static inline size_t type##_##field##_offsetof()	\
{						\
  return ((size_t)(&((struct type *)0)->field)); 	\
}

#define OFFSETOF(type,field) type##_##field##_offsetof() 



/* POINTER INC
 *
 * increments a ptr of structure object
 *
 * type: name of structure
 * ptr: the pointer
 * amount: inc amount
 */
#define INC_PTR(ptr,type,amount) \
ptr=##type##array_get(ptr, amount)


/* FIELD INC
 *
 * increments/decrements the field of a structure
 *
 * module: name of structure
 * field: name of field
 * m: ptr to the object
 * amount: inc/dec amount
 * i: index to field
 */
#define INC_FIELD(m,module,field,amount) \
module##_##field##_set(m, module##_##field##_get(m)+amount)

#define DEC_FIELD(m,module,field,amount) \
module##_##field##_set(m, module##_##field##_get(m)-amount)

#define INC_FIELD_AT(m,module,field,i,amount) \
module##_##field##_set_at(m, i, module##_##field##_get_at(m,i)+amount)

#define DEC_FIELD_AT(m,module,field,i,amount) \
module##_##field##_set_at(m, i, module##_##field##_get_at(m,i)-amount)

#endif

