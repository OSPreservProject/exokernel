
/*
 * Copyright MIT 1999
 */

#include <xok/asm.h>
#include <xok/syscall.h>


ENTRY(__asm_ipitest_plain)
	int	$T_IPITEST
	ret


ENTRY(__asm_fasttrap)
	int	$T_FASTTRAP
	ret

