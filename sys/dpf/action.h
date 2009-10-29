/* action code interface */
#ifndef __ACTION_H__
#define __ACTION_H__

#include <xok/types.h>
#include <xok/xoktypes.h>

#define MAXREGS 32

/* void action_run(struct inst *prog, char *msg, char *state); */

typedef enum {
	LDm,
	LDs,
	STs,
	ADD,
	ADDi,
	SUB,
	SUBi,
	MUL,
	MULi,
	DIV,
	DIVi,
	AND,
	ANDi,
	OR,
	ORi,
	XOR,
	XORi,
	CMPEQ,
	CMPEQi,
	CMPLT,
	CMPLTi,
	CMPLE,
	CMPLEi,
	BEQ,
	BNE,
	END
} a_ops;

struct inst {
	a_ops ops;
	int op1;
	int op2;
	int op3;
};

/* assume all frags of message should timeout at once
   also first frag determines address of other frags */

struct msgfrags {
  int timeout;
  uint32 *state;
  char *frag;
  int fraglen;
  struct msgfrags *next2;
};

struct state {
  uint32 state;
  unsigned long long time;
  struct state *next2;
};

struct frag_return {
  int headtail;
  int msgid;
};

struct action_block {
	struct inst *code;
	uint16 codesize;
  uint16 statesize;
  struct state *state, *last;
  uint32 stateoffset;
  uint32 statemask;
  uint32 coffset;
/*	struct msgfrags *frags;
	struct msgfrags *last;
	struct msgfrags *fragstail;
	struct msgfrags *lasttail; */
};

void action_init(struct action_block *prog);
void action_insert(struct action_block *prog, a_ops ops, int op1, int op2, int op3);

#define OPER(NAME, OP) case NAME: regs[cur.op3] = regs[cur.op1] OP regs[cur.op2]; break;
#define OPERi(NAME, OP) case NAME: regs[cur.op3] = regs[cur.op1] OP cur.op2; break;
#define COMP(NAME, OP) case NAME: if(regs[cur.op1] OP regs[cur.op2]) regs[cur.op3] = 1; else regs[cur.op3] = 0; break;
#define COMPi(NAME, OP) case NAME: if(regs[cur.op1] OP cur.op2) regs[cur.op3] = 1; else regs[cur.op3] = 0; break;
#endif
