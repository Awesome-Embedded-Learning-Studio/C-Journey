	.file	"function.c"
	.intel_syntax noprefix
.globl nCompletionStatus
	.bss
	.align 4
	.type	nCompletionStatus, @object
	.size	nCompletionStatus, 4
nCompletionStatus:
	.zero	4
	.text
.globl add
	.type	add, @function
add:
.LFB0:
	.cfi_startproc
	push	rbp
	.cfi_def_cfa_offset 16
	mov	rbp, rsp
	.cfi_offset 6, -16
	.cfi_def_cfa_register 6
	movss	DWORD PTR [rbp-20], xmm0
	movss	DWORD PTR [rbp-24], xmm1
	movss	xmm0, DWORD PTR [rbp-20]
	addss	xmm0, DWORD PTR [rbp-24]
	movss	DWORD PTR [rbp-4], xmm0
	mov	eax, DWORD PTR [rbp-4]
	mov	DWORD PTR [rbp-28], eax
	movss	xmm0, DWORD PTR [rbp-28]
	leave
	ret
	.cfi_endproc
.LFE0:
	.size	add, .-add
.globl add_and_multiply
	.type	add_and_multiply, @function
add_and_multiply:
.LFB1:
	.cfi_startproc
	push	rbp
	.cfi_def_cfa_offset 16
	mov	rbp, rsp
	.cfi_offset 6, -16
	.cfi_def_cfa_register 6
	sub	rsp, 32
	movss	DWORD PTR [rbp-20], xmm0
	movss	DWORD PTR [rbp-24], xmm1
	movss	xmm1, DWORD PTR [rbp-24]
	movss	xmm0, DWORD PTR [rbp-20]
	call	add
	movss	DWORD PTR [rbp-4], xmm0
	movss	xmm1, DWORD PTR [rbp-4]
	movss	xmm0, DWORD PTR .LC0[rip]
	mulss	xmm0, xmm1
	movss	DWORD PTR [rbp-4], xmm0
	mov	eax, DWORD PTR [rbp-4]
	mov	DWORD PTR [rbp-28], eax
	movss	xmm0, DWORD PTR [rbp-28]
	leave
	ret
	.cfi_endproc
.LFE1:
	.size	add_and_multiply, .-add_and_multiply
	.section	.rodata
	.align 4
.LC0:
	.long	1077936128
	.ident	"GCC: (Ubuntu/Linaro 4.4.7-8ubuntu1) 4.4.7"
	.section	.note.GNU-stack,"",@progbits
