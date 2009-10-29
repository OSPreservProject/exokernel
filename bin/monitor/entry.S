#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/syscall.h>
#include <xok/syscallno.h>	
#include <exos/fpu.h>
#include <exos/callcountdefs.h>
#include <exos/ptrace.h>

#include "vm_layout.h"


.data
tmp_esp: .word 0
monitor_stack:
	.long REGS_TOP
	.long GD_UD|3
trapno: .long 0
eax:	.long 0
ds:	.long 0
eflags: .long 0
cs:     .long 0
eip:	.long 0
erc:	.long 0
esp:    .long 0
ss:	.long 0


.text

/*
 We got here for either of 2 reasons:
 1. the guest did an int at CPL 3 (ss!=GD_UT|3).  We are still CPL 3.
 2. bug in monitor.

    stack on entry here is:   when done:
    eflags                    ss
    cs                        esp
    eip                       eflags
    erc (?)                   cs
                              eip
                              ds
                              es
                              eax
                             ...etc... (pushal)
                              erc
                              trapno
    We need to turn this into a normal trap frame that
    exc_C_handler_header can understand.
*/

uidt_handler_ec:
	popl	(erc)		# save errorcode
uidt_handler:
	popl	(eip)
	popl	(cs)
	popl	(eflags)
	movl    %eax, (eax)
	movw	%ds, (ds)	# save old ds; set new
	movw	$GD_UD|3, %ax
	movw	%ax, %ds
	movl	%esp, (esp)
	movl	$ss, (ss)
	lss	monitor_stack, %esp
	subl	$16, %esp
	pushl	(ss)
	pushl	(esp)
	pushl	(eflags)
	pushl	(cs)
	pushl	(eip)
	pushl	(ds)
	pushl	%es
	movl 	(eax), %eax
	pushal
	pushl	(erc)
	pushl	(trapno)
	call _exc_C_handler_header

#if 0
	pushl	%ss
	pushl	%eax
	movw	%ds, (ds)	# save old ds; set new
	movw	$GD_UD|3, %ax
	movw	%ax, %ds
	movl	%esp, %eax
	addl	$12, %eax	# eax is now esp when we trapped
	lss	monitor_stack, %esp
	subl	$16, %esp	# I assume no VM86
	pushl	-16(%eax)	# push old ss
	pushl	%eax		# push old esp
	pushfl			# push old flags
	pushl	-8(%eax)	# push old cs
	pushl	-12(%eax)	# push old eip
	pushl	(ds)
	pushl	%es
	movl    -20(%eax), %eax	# restore eax
	pushal			# push all; esp is junk
	pushl	(erc)
	pushl	(trapno)
	movl	$1, (_stop)
	call _exc_C_handler_header
#endif

	.globl _monitor_epilogue
_monitor_epilogue:
	cmpw $GD_UT|3, ___u_cs
	je _original_epilogue

	# we interrupted the guest

	# Save guest state into REGS, as if we had trapped out.
	# Never have to set gs, fs, ds, es for VM86 portion of trap frame,
	# because this prologue is not used during VM86.
	movl	%esp, (tmp_esp)
	movl    $REGS_TOP-16, %esp	# no VM86 regs
	pushw   $0
	pushw   ___u_ss
	pushl   (tmp_esp)
	pushl   ___u_eflags
	pushw   $0
	pushw   ___u_cs
	pushl   ___u_ppc
	pushw   $0
	pushw   ___u_ds
	pushw   $0
	pushw   ___u_es
	pushal
	# Error code and trapno are not used when we reschedule guest later.

	movl	$USTACKTOP, %esp	# find a stack
#ifdef TRACE_LOGUE
	push	(___u_ppc)
	push	(___u_cs)
	call	_intr_guest
	add	$8, %esp
#endif
	
_original_epilogue:			# we interrupted the monitor
	pushl	___u_ppc
	pushl	___u_eflags

	movl	$-1, (___u_donate)	
 	cmpl	$0, (___u_in_critical)
	je	_monitor_yield
 	movl	$1, (___u_interrupted)
_monitor_go_back:			# remember, though, cant go back if we came from guest.
	popfl
	ret

	.globl	_monitor_yield
_monitor_yield:	
	pushal

#ifdef TRACE_LOGUE
	cmpw	$GD_UT|3, ___u_cs
	jne	1f
	call	_intr_mon
1:
#endif

	cmpl	$0, (__exos_fpu_used_ctxt)
	je	_monitor_no_fpu
	movl	$0, (__exos_fpu_used_ctxt)
	fnsave	__exos_fpus # save fpu state without causing exception
	fwait		# (conservative) wait for write to memory to complete
_monitor_no_fpu:
	pushl	$0	# needed to differentiate from ipc call
	pushl	$0	# needed to differentiate from ipc call
	int	$T_YIELD


	ALIGN_TEXT
	.globl _monitor_prologue
_monitor_prologue:
#ifdef TRACE_LOGUE
	call	_in_pro
#endif
	cmpw	$GD_UT|3, (___u_cs)	# then we interrupted the monitor; we can use that stack.
	je	_original_prologue	# else esp is of the guest; must fix that first.
#ifdef TRACE_LOGUE
	push	0x2c(%esp)
	push	(___u_cs)
	call	_ret_guest
	add	$8, %esp
#endif
	movl	$USTACKTOP, %esp	# find a tmp stack for prologue  (FIXME: use XSTACK ?)
#ifdef TRACE_LOGUE
	jmp	1f
#endif
	
_original_prologue:
#ifdef TRACE_LOGUE
	push	0x2c(%esp)
	push	(___u_cs)
	call	_ret_mon
	add	$8, %esp
1:
#endif
 	call	___CheckITimers
 	call	__exos_check_paging
 	cmpl	$0, (_pending_nonblocked_signal)
 	je	_monitor_no_sigs
 	call	_DeliverPendingSignal
_monitor_no_sigs:
	cmpl	$0, (___u_revoked_pages)
	je	_no_revocation
	call	__do_revocation
_no_revocation:
	cmpw	$GD_UT|3, (___u_cs)
	jne	_schedule_guest		# we interrupted guest; go right back via guest scheduler

_return_to_host:
	movl	$1, (_irq0_pending)
	popl	%eax
	popl	%eax
	cmpl	$0, %eax
	jne	ipc1_wakeup
	popal

	testl	$EXOS_PT_BEING_PTRACED, ___u_ptrace_flags
	je	_monitor_go_back
	testl	$EXOS_PT_PCONTINUE, ___u_ptrace_flags
	jne	_monitor_go_back
	# set upstack as bp handler expects and jump to it
	movl	%eax, __exos_ptrace_eax_save
	movl	%ebx, __exos_ptrace_ebx_save
	popl	%eax
	popl	%ebx
	pushl	%eax
	pushl	%cs
	pushl	%ebx
	movl	__exos_ptrace_eax_save, %eax
	movl	__exos_ptrace_ebx_save, %ebx
	jmpl	_ptrace_breakpoint_handler_s



#define UIDT_WRAPPER(N) \
	movb $N, (trapno) ; \
	jmpl uidt_handler
#define UIDT_WRAPPER_EC(N) \
	movb $N, (trapno) ; \
	jmpl uidt_handler_ec

	.align 4
	.globl _uidt_entry
_uidt_entry:
	/* Yes, I too would prefer a macro.  But gasp and as suck, and the
	   rest is already written to compile with gcc.  So we use what we got. */
	UIDT_WRAPPER(0);
	UIDT_WRAPPER(1);
	UIDT_WRAPPER(2);
	UIDT_WRAPPER(3);
	UIDT_WRAPPER(4);
	UIDT_WRAPPER(5);
	UIDT_WRAPPER(6);
	UIDT_WRAPPER(7);
	UIDT_WRAPPER_EC(8);
	UIDT_WRAPPER(9);
	UIDT_WRAPPER_EC(10);
	UIDT_WRAPPER_EC(11);
	UIDT_WRAPPER_EC(12);
	UIDT_WRAPPER_EC(13);
	UIDT_WRAPPER_EC(14);
	UIDT_WRAPPER(15);
	UIDT_WRAPPER(16);
	UIDT_WRAPPER_EC(17);
	UIDT_WRAPPER(18);
	UIDT_WRAPPER(19);
	UIDT_WRAPPER(20);
	UIDT_WRAPPER(21);
	UIDT_WRAPPER(22);
	UIDT_WRAPPER(23);
	UIDT_WRAPPER(24);
	UIDT_WRAPPER(25);
	UIDT_WRAPPER(26);
	UIDT_WRAPPER(27);
	UIDT_WRAPPER(28);
	UIDT_WRAPPER(29);
	UIDT_WRAPPER(30);
	UIDT_WRAPPER(31);
	UIDT_WRAPPER(32);
	UIDT_WRAPPER(33);
	UIDT_WRAPPER(34);
	UIDT_WRAPPER(35);
	UIDT_WRAPPER(36);
	UIDT_WRAPPER(37);
	UIDT_WRAPPER(38);
	UIDT_WRAPPER(39);
	UIDT_WRAPPER(40);
	UIDT_WRAPPER(41);
	UIDT_WRAPPER(42);
	UIDT_WRAPPER(43);
	UIDT_WRAPPER(44);
	UIDT_WRAPPER(45);
	UIDT_WRAPPER(46);
	UIDT_WRAPPER(47);
	UIDT_WRAPPER(48);
	UIDT_WRAPPER(49);
	UIDT_WRAPPER(50);
	UIDT_WRAPPER(51);
	UIDT_WRAPPER(52);
	UIDT_WRAPPER(53);
	UIDT_WRAPPER(54);
	UIDT_WRAPPER(55);
	UIDT_WRAPPER(56);
	UIDT_WRAPPER(57);
	UIDT_WRAPPER(58);
	UIDT_WRAPPER(59);
	UIDT_WRAPPER(60);
	UIDT_WRAPPER(61);
	UIDT_WRAPPER(62);
	UIDT_WRAPPER(63);
	UIDT_WRAPPER(64);
	UIDT_WRAPPER(65);
	UIDT_WRAPPER(66);
	UIDT_WRAPPER(67);
	UIDT_WRAPPER(68);
	UIDT_WRAPPER(69);
	UIDT_WRAPPER(70);
	UIDT_WRAPPER(71);
	UIDT_WRAPPER(72);
	UIDT_WRAPPER(73);
	UIDT_WRAPPER(74);
	UIDT_WRAPPER(75);
	UIDT_WRAPPER(76);
	UIDT_WRAPPER(77);
	UIDT_WRAPPER(78);
	UIDT_WRAPPER(79);
	UIDT_WRAPPER(80);
	UIDT_WRAPPER(81);
	UIDT_WRAPPER(82);
	UIDT_WRAPPER(83);
	UIDT_WRAPPER(84);
	UIDT_WRAPPER(85);
	UIDT_WRAPPER(86);
	UIDT_WRAPPER(87);
	UIDT_WRAPPER(88);
	UIDT_WRAPPER(89);
	UIDT_WRAPPER(90);
	UIDT_WRAPPER(91);
	UIDT_WRAPPER(92);
	UIDT_WRAPPER(93);
	UIDT_WRAPPER(94);
	UIDT_WRAPPER(95);
	UIDT_WRAPPER(96);
	UIDT_WRAPPER(97);
	UIDT_WRAPPER(98);
	UIDT_WRAPPER(99);
	UIDT_WRAPPER(100);
	UIDT_WRAPPER(101);
	UIDT_WRAPPER(102);
	UIDT_WRAPPER(103);
	UIDT_WRAPPER(104);
	UIDT_WRAPPER(105);
	UIDT_WRAPPER(106);
	UIDT_WRAPPER(107);
	UIDT_WRAPPER(108);
	UIDT_WRAPPER(109);
	UIDT_WRAPPER(110);
	UIDT_WRAPPER(111);
	UIDT_WRAPPER(112);
	UIDT_WRAPPER(113);
	UIDT_WRAPPER(114);
	UIDT_WRAPPER(115);
	UIDT_WRAPPER(116);
	UIDT_WRAPPER(117);
	UIDT_WRAPPER(118);
	UIDT_WRAPPER(119);
	UIDT_WRAPPER(120);
	UIDT_WRAPPER(121);
	UIDT_WRAPPER(122);
	UIDT_WRAPPER(123);
	UIDT_WRAPPER(124);
	UIDT_WRAPPER(125);
	UIDT_WRAPPER(126);
	UIDT_WRAPPER(127);
	UIDT_WRAPPER(128);
	UIDT_WRAPPER(129);
	UIDT_WRAPPER(130);
	UIDT_WRAPPER(131);
	UIDT_WRAPPER(132);
	UIDT_WRAPPER(133);
	UIDT_WRAPPER(134);
	UIDT_WRAPPER(135);
	UIDT_WRAPPER(136);
	UIDT_WRAPPER(137);
	UIDT_WRAPPER(138);
	UIDT_WRAPPER(139);
	UIDT_WRAPPER(140);
	UIDT_WRAPPER(141);
	UIDT_WRAPPER(142);
	UIDT_WRAPPER(143);
	UIDT_WRAPPER(144);
	UIDT_WRAPPER(145);
	UIDT_WRAPPER(146);
	UIDT_WRAPPER(147);
	UIDT_WRAPPER(148);
	UIDT_WRAPPER(149);
	UIDT_WRAPPER(150);
	UIDT_WRAPPER(151);
	UIDT_WRAPPER(152);
	UIDT_WRAPPER(153);
	UIDT_WRAPPER(154);
	UIDT_WRAPPER(155);
	UIDT_WRAPPER(156);
	UIDT_WRAPPER(157);
	UIDT_WRAPPER(158);
	UIDT_WRAPPER(159);
	UIDT_WRAPPER(160);
	UIDT_WRAPPER(161);
	UIDT_WRAPPER(162);
	UIDT_WRAPPER(163);
	UIDT_WRAPPER(164);
	UIDT_WRAPPER(165);
	UIDT_WRAPPER(166);
	UIDT_WRAPPER(167);
	UIDT_WRAPPER(168);
	UIDT_WRAPPER(169);
	UIDT_WRAPPER(170);
	UIDT_WRAPPER(171);
	UIDT_WRAPPER(172);
	UIDT_WRAPPER(173);
	UIDT_WRAPPER(174);
	UIDT_WRAPPER(175);
	UIDT_WRAPPER(176);
	UIDT_WRAPPER(177);
	UIDT_WRAPPER(178);
	UIDT_WRAPPER(179);
	UIDT_WRAPPER(180);
	UIDT_WRAPPER(181);
	UIDT_WRAPPER(182);
	UIDT_WRAPPER(183);
	UIDT_WRAPPER(184);
	UIDT_WRAPPER(185);
	UIDT_WRAPPER(186);
	UIDT_WRAPPER(187);
	UIDT_WRAPPER(188);
	UIDT_WRAPPER(189);
	UIDT_WRAPPER(190);
	UIDT_WRAPPER(191);
	UIDT_WRAPPER(192);
	UIDT_WRAPPER(193);
	UIDT_WRAPPER(194);
	UIDT_WRAPPER(195);
	UIDT_WRAPPER(196);
	UIDT_WRAPPER(197);
	UIDT_WRAPPER(198);
	UIDT_WRAPPER(199);
	UIDT_WRAPPER(200);
	UIDT_WRAPPER(201);
	UIDT_WRAPPER(202);
	UIDT_WRAPPER(203);
	UIDT_WRAPPER(204);
	UIDT_WRAPPER(205);
	UIDT_WRAPPER(206);
	UIDT_WRAPPER(207);
	UIDT_WRAPPER(208);
	UIDT_WRAPPER(209);
	UIDT_WRAPPER(210);
	UIDT_WRAPPER(211);
	UIDT_WRAPPER(212);
	UIDT_WRAPPER(213);
	UIDT_WRAPPER(214);
	UIDT_WRAPPER(215);
	UIDT_WRAPPER(216);
	UIDT_WRAPPER(217);
	UIDT_WRAPPER(218);
	UIDT_WRAPPER(219);
	UIDT_WRAPPER(220);
	UIDT_WRAPPER(221);
	UIDT_WRAPPER(222);
	UIDT_WRAPPER(223);
	UIDT_WRAPPER(224);
	UIDT_WRAPPER(225);
	UIDT_WRAPPER(226);
	UIDT_WRAPPER(227);
	UIDT_WRAPPER(228);
	UIDT_WRAPPER(229);
	UIDT_WRAPPER(230);
	UIDT_WRAPPER(231);
	UIDT_WRAPPER(232);
	UIDT_WRAPPER(233);
	UIDT_WRAPPER(234);
	UIDT_WRAPPER(235);
	UIDT_WRAPPER(236);
	UIDT_WRAPPER(237);
	UIDT_WRAPPER(238);
	UIDT_WRAPPER(239);
	UIDT_WRAPPER(240);
	UIDT_WRAPPER(241);
	UIDT_WRAPPER(242);
	UIDT_WRAPPER(243);
	UIDT_WRAPPER(244);
	UIDT_WRAPPER(245);
	UIDT_WRAPPER(246);
	UIDT_WRAPPER(247);
	UIDT_WRAPPER(248);
	UIDT_WRAPPER(249);
	UIDT_WRAPPER(250);
	UIDT_WRAPPER(251);
	UIDT_WRAPPER(252);
	UIDT_WRAPPER(253);
	UIDT_WRAPPER(254);
	UIDT_WRAPPER(255);
