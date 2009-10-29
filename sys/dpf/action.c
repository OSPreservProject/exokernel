#include <dpf/action.h>
#include <dpf/demand.h>
#include <xok/malloc.h>
#include <xok/printf.h>

void action_init(struct action_block *prog) {
	int statesz = prog->statesize;

	memset(prog, 0, sizeof(struct action_block));
	prog->state = NULL;
	/* add in demand here */
	prog->statesize = statesz;
}

#define realloc(p, n) memcpy(calloc(1, n), p, n-1);

void action_insert(struct action_block *prog, a_ops ops, int op1, int op2, int op3) {
	prog->code = realloc(prog->code, (prog->codesize+1)*sizeof(struct inst));
	prog->code[prog->codesize].ops = ops;
	prog->code[prog->codesize].op1 = op1;
	prog->code[prog->codesize].op2 = op2;
	prog->code[prog->codesize].op3 = op3;
	prog->codesize++;
}

#if 0
void action_run(struct action_block *prog, uint8 *msg) {
	int regs[MAXREGS], ip = 0;
	struct inst cur;
	uint32 *state;
	struct state *scur;

	for(scur = prog->state; scur->next2 != NULL; scur = scur->next2) {
	  ; /* just get to the end.... */
	}
	scur->next2 = calloc(1, sizeof(struct state));
	scur->state = calloc(prog->statesize, sizeof(int));
	state = scur->state;

	memset(regs, 0, 4*MAXREGS);

	while (1) {
		cur = prog->code[ip++];

		switch(cur.ops) {
		case LDm:
			regs[cur.op3] = msg[regs[cur.op1]+cur.op2];
			break;
		case LDs:
			regs[cur.op3] = msg[regs[cur.op1]+cur.op2];
			break;
		case STs:
			if(regs[cur.op1]+cur.op2 < 0)
				fatal(bad ST pointer);
			state[regs[cur.op1]+cur.op2] = regs[cur.op3];
			break;
		OPER(ADD, +)
		OPERi(ADDi, +)
		OPER(SUB, -)
		OPERi(SUBi, -)
		OPER(MUL, *)
		OPERi(MULi, *)
		OPER(DIV, /)
		OPERi(DIVi, /)
		OPER(AND, &)
		OPERi(ANDi, &)
		OPER(OR, |)
		OPERi(ORi, |)
		OPER(XOR, ^)
		OPERi(XORi, ^)
		COMP(CMPEQ, ==)
		COMPi(CMPEQi, ==)
		COMP(CMPLT, <)
		COMPi(CMPLTi, <)
		COMP(CMPLE, <=)
		COMPi(CMPLEi, <=)
		case BEQ:
			if(regs[cur.op1] == 0)
				ip += regs[cur.op2] - 1;
			if(ip < 0)
				fatal(bad ip);
			break;
		case BNE:
			if(regs[cur.op1] != 0)
				ip += regs[cur.op2] - 1;
			if(ip < 0)
				fatal(bad ip);
			break;
		case END:
//printf("added state: 0x%08x\n", state[0]);
			return;
		default:
			fatal(bad opcode);
		}
	}
}
#endif
