#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_

#include "types.h"

/* to identify protected regions of memory */
#define PROT_G_IDT 1
#define PROT_H_IDT 2
#define PROT_G_GDT 3
#define PROT_H_GDT 4
#define PROT_G_LDT 5
#define PROT_H_LDT 6


#define DESC_TYPE(desc)         	(((desc)->s<<4) + (desc)->type)
#define DESC_IS_WRITABLE_DATA(desc)	((DESC_TYPE(desc) & ~5) == 0x12)
#define DESC_IS_DATA(desc)		((DESC_TYPE(desc) & ~7) == 0x10)
#define DESC_IS_NCONF_CODE(desc)	((DESC_TYPE(desc) & ~3) == 0x18)
#define DESC_IS_CONF_CODE(desc)		((DESC_TYPE(desc) & ~3) == 0x1c)
#define DESC_IS_CODE(desc)		((DESC_TYPE(desc) & ~7) == 0x18)
#define DESC_IS_TSS(desc)		( DESC_TYPE(desc)       == 0x09)
#define DESC_IS_BUSY_TSS(desc)		( DESC_TYPE(desc)       == 0x0a)
#define DESC_IS_TASKGATE(desc)		( DESC_TYPE(desc)       == 0x05)
#define DESC_IS_INTGATE(desc)		((DESC_TYPE(desc) & ~8) == 0x06)
#define DESC_IS_TRAPGATE(desc)		((DESC_TYPE(desc) & ~8) == 0x07)
#define DESC_IS_CALLGATE(desc)		( DESC_TYPE(desc)       == 0x0c)
#define DESC_IS_LDT(desc)     		( DESC_TYPE(desc)       == 0x02)

#define SEL_IS_LOCAL(sel)	(sel & 4)
#define SEL_IS_GLOBAL(sel)	(!SEL_IS_LOCAL(sel))

enum desc_types {
    DESC_GATE, DESC_SEG
};

struct gate_descr {
    unsigned off_15_0 : 16;   /* low 16 bits of offset in segment */
    unsigned segsel : 16;     /* segment selector */
    unsigned args : 5;        /* # args, 0 for interrupt/trap gates */
    unsigned rsv1 : 3;        /* reserved (should be zero I guess) */
    unsigned type : 4;        /* type (STS_{TG,IG32,TG32}) */
    unsigned s : 1;           /* must be 0 (system) */
    unsigned dpl : 2;         /* descriptor (meaning new) privilege level */
    unsigned p : 1;           /* Present */
    unsigned off_31_16 : 16;  /* high bits of offset in segment */
};
#define GATE_OFFSET(desc)	((desc)->off_15_0 | ((desc)->off_31_16<<16))


struct seg_descr {
    unsigned lim_15_0 : 16;   /* low bits of segment limit */
    unsigned base_15_0 : 16;  /* low bits of segment base address */
    unsigned base_23_16 : 8;  /* middle bits of segment base */
    unsigned type : 4;        /* segment type */
    unsigned s : 1;           /* 0 = system, 1 = application */
    unsigned dpl : 2;         /* Descriptor Privilege Level */
    unsigned p : 1;           /* Present */
    unsigned lim_19_16 : 4;   /* High bits of segment limit */
    unsigned avl : 1;         /* Unused (available for software use) */
    unsigned rsv1 : 1;        /* reserved */
    unsigned db : 1;          /* 0 = 16-bit segment, 1 = 32-bit segment */
    unsigned g : 1;           /* Granularity: limit scaled by 4K when set */
    unsigned base_31_24 : 8;  /* Hight bits of base */
};
#define SEG_BASE(desc)		(((desc)->base_31_24 << 24) | ((desc)->base_23_16 << 16) | (desc)->base_15_0)
#define SEG_LIMIT(desc)		((((desc)->lim_19_16 << 16) | (desc)->lim_15_0) * ((desc)->g ? 4096 : 1))


/* most generic descriptor, when P is clear. */
struct descr {
    unsigned avail1 : 32;
    unsigned avail2 : 8;
    unsigned type : 4;        /* type */
    unsigned s : 1;           /* must be 0 (system) */
    unsigned dpl : 2;         /* descriptor (meaning new) privilege level */
    unsigned p : 1;           /* Present */
    unsigned avail3 : 16;
};


int id_va_readonly(u_int va);
int id_va_readonly_page(u_int va);
u_int id_pa_readonly(u_int pa, u_int len, int *match);
void add_readonly(u_int va, u_int pa);
void remove_readonly(u_int va, u_int pa);
int load_ldtr(Bit16u selector);
void set_ldt(Bit16u selector, Bit32u base, Bit16u limit);
void set_gdt(Bit32u base, Bit16u limit);
void set_idt(Bit32u base, Bit16u limit);
int merge_gdt(void);
int segment_writable(u_int selector, int *writable);
int get_descriptor(u_int selector, struct descr *descr);
int get_tss_descriptor(u_int selector, struct seg_descr *descr, int busy);
void print_dt_entry(u_int i, const struct descr *sd);
void init_protected_ranges(void);
void print_dt_mappings(void);

u_int get_limit(u_int seg);
u_int _get_base(u_int seg);
#ifndef NDEBUG
  u_int get_base_debug(u_int seg, const char *file, u_int line);
  #define get_base(seg) get_base_debug(seg, __FILE__,  __LINE__)
#else
  #define get_base(seg) _get_base(seg)
#endif


#endif
