/*
 * alignfix.S - data abort entry that emulates unaligned LDRD/STRD/LDM/STM
 *
 * The ARM1176 allows unaligned LDR/STR/LDRH/STRH (U bit), but LDRD, STRD,
 * LDM and STM on addresses that are not word aligned raise an alignment
 * fault. Linux fixes these up in its abort handler, and parts of MAME rely
 * on that. Here AlignFixStub calls AlignFixup () (alignfix.cpp) with the
 * registers of the aborted code (system mode). If the fault was handled,
 * execution continues after the instruction; otherwise the abort goes on to
 * Circle's DataAbortStub as before.
 */

	.text
	.globl	AlignFixStub
AlignFixStub:
	sub	lr, lr, #8			/* lr: address of the aborted instruction */
	stmfd	sp!, {lr}			/* regs[15] = pc */
	sub	sp, sp, #8
	stmia	sp, {r13, r14}^			/* regs[13], regs[14] = sp_sys, lr_sys */
	nop
	stmfd	sp!, {r0-r12}			/* regs[0..12] */
	mov	r0, sp				/* r0: pointer to regs[16] */
	mrs	r1, spsr
	stmfd	sp!, {r1, r2}			/* keep spsr (and 8-byte stack alignment) */
	bl	AlignFixup			/* returns 0 if the access was emulated */
	ldmfd	sp!, {r1, r2}
	msr	spsr_cxsf, r1
	cmp	r0, #0
	bne	1f

	ldmfd	sp!, {r0-r12}
	ldmia	sp, {r13, r14}^			/* sp, lr may have been changed (writeback) */
	nop
	add	sp, sp, #8
	ldmfd	sp!, {lr}			/* new pc */
	movs	pc, lr				/* return, restores cpsr from spsr */

1:	ldmfd	sp!, {r0-r12}
	add	sp, sp, #8
	ldmfd	sp!, {lr}
	add	lr, lr, #8			/* state as on entry */
	b	DataAbortStub

/* End */
