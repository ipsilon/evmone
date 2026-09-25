// g8_avx2 fixup: all tables stored as 16-byte stores (no lane permutes, every scalar load forwards from one xmm store).
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g8_avx2_ofix_LCPI2_0:
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
.Ljda_g8_avx2_ofix_LCPI2_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g8_avx2_ofix_LCPI2_11:
	.byte	91
    .p2align 5
.Ljda_g8_avx2_ofix_LCPI2_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g8_avx2_ofix_LCPI2_13:
	.byte	240
    .p2align 5
.Ljda_g8_avx2_ofix_LCPI2_3:
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
.Ljda_g8_avx2_ofix_LCPI2_4:
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
.Ljda_g8_avx2_ofix_LCPI2_7:
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
.Ljda_g8_avx2_ofix_LCPI2_9:
	.byte	95
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g8_avx2_ofix
    .hidden jda_asm_g8_avx2_ofix
    .type jda_asm_g8_avx2_ofix,@function
jda_asm_g8_avx2_ofix:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$448, %rsp
	vmovdqa	.Ljda_g8_avx2_ofix_LCPI2_0(%rip), %xmm0
	vmovdqa	%xmm0, 16(%rsp)
	vpxor	%xmm0, %xmm0, %xmm0
	vmovdqa	%ymm0, 288(%rsp)
	vmovdqa	%ymm0, 160(%rsp)
	movabsq	$-7523661662112280176, %rax
	movq	%rax, 32(%rsp)
	vpxor	%xmm1, %xmm1, %xmm1
	vmovdqa	%xmm1, 320(%rsp)
	vmovdqa	%xmm1, 192(%rsp)
	movabsq	$-8681104427521506944, %rax
	movq	%rax, 96(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Ljda_g8_avx2_ofix_BB2_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_g8_avx2_ofix_LCPI2_9(%rip), %ymm1
	vpbroadcastq	.Ljda_g8_avx2_ofix_LCPI2_10(%rip), %ymm2
	vmovdqa	.Ljda_g8_avx2_ofix_LCPI2_3(%rip), %ymm3
	vmovdqa	.Ljda_g8_avx2_ofix_LCPI2_4(%rip), %ymm4
	vpbroadcastb	.Ljda_g8_avx2_ofix_LCPI2_11(%rip), %ymm5
	vpbroadcastq	.Ljda_g8_avx2_ofix_LCPI2_12(%rip), %ymm6
	vmovdqa	.Ljda_g8_avx2_ofix_LCPI2_7(%rip), %ymm7
	vpbroadcastb	.Ljda_g8_avx2_ofix_LCPI2_13(%rip), %ymm8
	.p2align	4
.Ljda_g8_avx2_ofix_BB2_2:
	vmovdqa	(%rdi,%rcx,8), %ymm9
	vpmaxsb	%ymm1, %ymm9, %ymm10
	vpaddb	%ymm2, %ymm10, %ymm11
	vpxor	%ymm3, %ymm11, %ymm11
	vpmaxsb	%ymm4, %ymm11, %ymm11
	vpcmpeqb	%ymm5, %ymm9, %ymm9
	vpand	%ymm6, %ymm9, %ymm9
	vpshufb	%ymm11, %ymm9, %ymm12
	vpor	%ymm9, %ymm12, %ymm9
	vpshufb	%ymm11, %ymm11, %ymm11
	vpshufb	%ymm11, %ymm9, %ymm12
	vpor	%ymm9, %ymm12, %ymm9
	vpshufb	%ymm11, %ymm11, %ymm11
	vpshufb	%ymm11, %ymm9, %ymm12
	vpshufb	%ymm11, %ymm11, %ymm11
	vpaddb	%ymm7, %ymm10, %ymm10
	vpshufb	%ymm11, %ymm10, %ymm10
	vpshufb	%ymm10, %ymm10, %ymm11
	vpmaxub	%ymm11, %ymm10, %ymm11
	vpor	%ymm9, %ymm12, %ymm9
	vpshufb	%ymm10, %ymm9, %ymm10
	vpunpcklbw	%ymm10, %ymm9, %ymm10
	vpunpckhbw	%ymm9, %ymm0, %ymm9
	vpaddb	%ymm8, %ymm11, %ymm12
	vpermq	$238, %ymm11, %ymm13
	vpshufb	%ymm12, %ymm13, %ymm13
	vpmaxub	%ymm12, %ymm13, %ymm12
	vmovdqa	%xmm11, (%rsp)
	vmovdqa	%xmm12, 64(%rsp)
	vextracti128	$1, %ymm11, 80(%rsp)
	vmovdqa	%xmm10, 128(%rsp)
	vmovdqa	%xmm9, 144(%rsp)
	vextracti128	$1, %ymm10, 256(%rsp)
	vextracti128	$1, %ymm9, 272(%rsp)
	movzbl	-128(%rsp,%rax), %r8d
	movzwl	-128(%rsp,%rax,2), %r9d
	movw	%r9w, (%rdx,%rcx)
	movzwl	(%rsp,%r8,2), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-64(%rsp,%rax), %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_avx2_ofix_BB2_2
.Ljda_g8_avx2_ofix_BB2_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g8_avx2_ofix, .-jda_asm_g8_avx2_ofix
    .section .note.GNU-stack,"",@progbits
#endif
