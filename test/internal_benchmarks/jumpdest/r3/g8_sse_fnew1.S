// g8_sse with the loop-carried entry kept in an xmm byte-broadcast and looked up with pshufb
// (TA then TB); the PT/TA tables are gone, the byte tables are stored as before.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_0:
	.byte	120
	.byte	121
	.byte	122
	.byte	123
	.byte	124
	.byte	125
	.byte	126
	.byte	127
	.byte	128
	.byte	129
	.byte	130
	.byte	131
	.byte	132
	.byte	133
	.byte	134
	.byte	135
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_1:
	.byte	136
	.byte	137
	.byte	138
	.byte	139
	.byte	140
	.byte	141
	.byte	142
	.byte	143
	.byte	144
	.byte	145
	.byte	146
	.byte	147
	.byte	148
	.byte	149
	.byte	150
	.byte	151
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_10:
	.zero	16,240
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_2:
	.byte	128
	.byte	129
	.byte	130
	.byte	131
	.byte	132
	.byte	133
	.byte	134
	.byte	135
	.byte	136
	.byte	137
	.byte	138
	.byte	139
	.byte	140
	.byte	141
	.byte	142
	.byte	143
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_3:
	.zero	16,95
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_4:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_5:
	.byte	8
	.byte	8
	.byte	8
	.byte	8
	.byte	8
	.byte	8
	.byte	8
	.byte	8
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_6:
	.byte	0
	.byte	1
	.byte	2
	.byte	3
	.byte	4
	.byte	5
	.byte	6
	.byte	7
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	12
	.byte	13
	.byte	14
	.byte	15
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_7:
	.zero	16,91
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_8:
	.quad	-9205322385119247871
	.quad	-9205322385119247871
    .p2align 5
.Ljda_g8_sse_fnew1_E0:
	.zero	16,112
    .p2align 5
.Ljda_g8_sse_fnew1_LCPI0_9:
	.byte	18
	.byte	19
	.byte	20
	.byte	21
	.byte	22
	.byte	23
	.byte	24
	.byte	25
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g8_sse_fnew1
    .hidden jda_asm_g8_sse_fnew1
    .type jda_asm_g8_sse_fnew1,@function
jda_asm_g8_sse_fnew1:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$448, %rsp
	movaps	.Ljda_g8_sse_fnew1_LCPI0_0(%rip), %xmm0
	movups	%xmm0, 152(%rsp)
	movups	%xmm0, 104(%rsp)
	xorps	%xmm0, %xmm0
	movaps	%xmm0, 352(%rsp)
	movups	%xmm0, 248(%rsp)
	movaps	%xmm0, 304(%rsp)
	movups	%xmm0, 200(%rsp)
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_1(%rip), %xmm1
	movdqu	%xmm1, 168(%rsp)
	movdqu	%xmm1, 120(%rsp)
	movaps	%xmm0, 368(%rsp)
	movups	%xmm0, 264(%rsp)
	movaps	%xmm0, 320(%rsp)
	movups	%xmm0, 216(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Ljda_g8_sse_fnew1_BB0_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$112, %eax
	xorl	%ecx, %ecx
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_3(%rip), %xmm0
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_4(%rip), %xmm1
	movq	.Ljda_g8_sse_fnew1_LCPI0_5(%rip), %xmm2
	movdqa	.Ljda_g8_sse_fnew1_E0(%rip), %xmm3
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_7(%rip), %xmm4
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_8(%rip), %xmm5
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_9(%rip), %xmm6
	movdqa	.Ljda_g8_sse_fnew1_LCPI0_10(%rip), %xmm7
	.p2align	4
.Ljda_g8_sse_fnew1_BB0_2:
	movdqa	(%rdi,%rcx,8), %xmm13
	movdqa	16(%rdi,%rcx,8), %xmm12
	movdqa	%xmm13, %xmm8
	pmaxsb	%xmm0, %xmm8
	movdqa	%xmm8, %xmm10
	paddb	%xmm1, %xmm10
	pxor	%xmm2, %xmm10
	pmaxsb	.Ljda_g8_sse_fnew1_LCPI0_6(%rip), %xmm10
	pcmpeqb	%xmm4, %xmm13
	pand	%xmm5, %xmm13
	movdqa	%xmm12, %xmm9
	pmaxsb	%xmm0, %xmm9
	movdqa	%xmm9, %xmm11
	paddb	%xmm1, %xmm11
	pxor	%xmm2, %xmm11
	pmaxsb	.Ljda_g8_sse_fnew1_LCPI0_6(%rip), %xmm11
	pcmpeqb	%xmm4, %xmm12
	pand	%xmm5, %xmm12
	movdqa	%xmm12, %xmm14
	pshufb	%xmm11, %xmm14
	por	%xmm12, %xmm14
	pshufb	%xmm11, %xmm11
	movdqa	%xmm14, %xmm15
	pshufb	%xmm11, %xmm15
	por	%xmm14, %xmm15
	pshufb	%xmm11, %xmm11
	movdqa	%xmm15, %xmm12
	pshufb	%xmm11, %xmm12
	por	%xmm15, %xmm12
	pshufb	%xmm11, %xmm11
	movdqa	%xmm13, %xmm14
	pshufb	%xmm10, %xmm14
	por	%xmm13, %xmm14
	pshufb	%xmm10, %xmm10
	movdqa	%xmm14, %xmm15
	pshufb	%xmm10, %xmm15
	por	%xmm14, %xmm15
	pshufb	%xmm10, %xmm10
	movdqa	%xmm15, %xmm13
	pshufb	%xmm10, %xmm13
	por	%xmm15, %xmm13
	pshufb	%xmm10, %xmm10
	paddb	%xmm6, %xmm8
	pshufb	%xmm10, %xmm8
	movdqa	%xmm8, %xmm10
	pshufb	%xmm8, %xmm10
	pmaxub	%xmm8, %xmm10
	paddb	%xmm6, %xmm9
	pshufb	%xmm11, %xmm9
	movdqa	%xmm9, %xmm11
	pshufb	%xmm9, %xmm11
	pmaxub	%xmm9, %xmm11
	movq	%xmm8, 96(%rsp)
	movq	%xmm9, 144(%rsp)
	movq	%xmm13, 192(%rsp)
	movq	%xmm12, 240(%rsp)
	movdqa	%xmm13, 288(%rsp)
	movdqa	%xmm12, 336(%rsp)
	paddb	%xmm7, %xmm10
	paddb	%xmm7, %xmm11
	pshufb	%xmm3, %xmm10
	paddb	%xmm7, %xmm3
	pmaxub	%xmm3, %xmm10
	movd	%xmm10, %r9d
	pshufb	%xmm10, %xmm11
	paddb	%xmm7, %xmm10
	pmaxub	%xmm10, %xmm11
	movzbl	%r9b, %r9d
	movdqa	%xmm11, %xmm3
	movzbl	-16(%rsp,%rax), %r8d
	movzbl	32(%rsp,%r9), %r10d
	movzbl	80(%rsp,%rax), %r11d
	movb	%r11b, (%rdx,%rcx)
	movzbl	176(%rsp,%r8), %r8d
	movb	%r8b, 1(%rdx,%rcx)
	movzbl	128(%rsp,%r9), %r8d
	movb	%r8b, 2(%rdx,%rcx)
	movzbl	224(%rsp,%r10), %r8d
	movb	%r8b, 3(%rdx,%rcx)
	movd	%xmm3, %eax
	movzbl	%al, %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_sse_fnew1_BB0_2
.Ljda_g8_sse_fnew1_BB0_3:
	movq	%rbp, %rsp
	popq	%rbp
	retq
    .size jda_asm_g8_sse_fnew1, .-jda_asm_g8_sse_fnew1
    .section .note.GNU-stack,"",@progbits
#endif
