#ifndef OPA_H
#define OPA_H

/* opcode attributes */

typedef struct {
    int pe;           /* protected mode? */
    int cpl;          /* cpl */
    int iopl;         /* iopl */
    int seg;          /* data segment selector */
    int repe;         /* repeat? */
    int repne;        /* repeat? */
    int opb;          /* 2 or 4 byte operands? based on pe, prefixes, etc */
    int adb;          /* 2 or 4 byte addresses?  based on pe, prefixes, etc */
    int prefixes;     /* bytes of prefixes */
} op_attr_t;

extern op_attr_t opa;

#endif
