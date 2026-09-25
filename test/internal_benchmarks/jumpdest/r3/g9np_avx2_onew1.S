// g9np_avx2_ofix redesign: tables go to a ring of 8 stack slots and the scalar chain reads them 8 blocks later, so its loads hit L1 instead of forwarding from in-flight stores.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_0:
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
	.byte	144
	.byte	145
	.byte	146
	.byte	147
	.byte	148
	.byte	149
	.byte	150
	.byte	151
	.byte	152
	.byte	153
	.byte	154
	.byte	155
	.byte	156
	.byte	157
	.byte	158
	.byte	159
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_11:
	.byte	91
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_3:
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
.Ljda_g9np_avx2_onew1_LCPI3_4:
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
.Ljda_g9np_avx2_onew1_LCPI3_7:
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
.Ljda_g9np_avx2_onew1_LCPI3_8:
	.zero	16,240
    .p2align 5
.Ljda_g9np_avx2_onew1_LCPI3_9:
	.byte	95
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g9np_avx2_onew1
    .hidden jda_asm_g9np_avx2_onew1
    .type jda_asm_g9np_avx2_onew1,@function
jda_asm_g9np_avx2_onew1:
	pushq	%rbp
	movq	%rsp, %rbp
	pushq	%rbx
	andq	$-8192, %rsp
	subq	$8192, %rsp
	addq	$31, %rsi
	vmovdqa	.Ljda_g9np_avx2_onew1_LCPI3_0(%rip), %ymm0
	vmovdqa	%ymm0, 64(%rsp)
	vmovdqa	%xmm0, 32(%rsp)
	vpxor	%xmm0, %xmm0, %xmm0
	vmovdqa	%ymm0, 128(%rsp)
	vpxor	%xmm0, %xmm0, %xmm0
	vmovdqa	%xmm0, 160(%rsp)
	vmovdqa	%xmm0, 144(%rsp)
	vmovdqa	%xmm0, 224(%rsp)
	vmovdqa	%xmm0, 208(%rsp)
	vmovdqa	%xmm0, 240(%rsp)
	cmpq	$32, %rsi
	jb	.Ljda_g9np_avx2_onew1_BB3_3
	shrq	$5, %rsi
	xorl	%r8d, %r8d
.Ljda_g9np_avx2_onew1_BB3_C:
	movl	%r8d, %r9d
	andl	$480, %r9d
	movdqa	(%rsp,%r9), %xmm8
	movdqa	%xmm8, 512(%rsp,%r8)
	movdqa	16(%rsp,%r9), %xmm8
	movdqa	%xmm8, 528(%rsp,%r8)
	addq	$32, %r8
	cmpq	$3584, %r8
	jne	.Ljda_g9np_avx2_onew1_BB3_C
	movq	%rsi, %rbx
	shlq	$5, %rbx
	leaq	-32(%rdi,%rbx), %rbx
	shlq	$2, %rsi
	movl	$128, %eax
	movq	$-32, %rcx
	movq	%rsp, %r10
	vpbroadcastb	.Ljda_g9np_avx2_onew1_LCPI3_9(%rip), %ymm0
	vpbroadcastq	.Ljda_g9np_avx2_onew1_LCPI3_10(%rip), %ymm1
	vmovdqa	.Ljda_g9np_avx2_onew1_LCPI3_3(%rip), %ymm2
	vmovdqa	.Ljda_g9np_avx2_onew1_LCPI3_4(%rip), %ymm3
	vpbroadcastb	.Ljda_g9np_avx2_onew1_LCPI3_11(%rip), %ymm4
	vpbroadcastq	.Ljda_g9np_avx2_onew1_LCPI3_12(%rip), %ymm5
	vmovdqa	.Ljda_g9np_avx2_onew1_LCPI3_7(%rip), %ymm6
	vmovdqa	.Ljda_g9np_avx2_onew1_LCPI3_8(%rip), %xmm7
	jmp	.Ljda_g9np_avx2_onew1_BB3_V
	.p2align	4
.Ljda_g9np_avx2_onew1_BB3_2:
	movzbl	-80(%r10,%rax), %r8d
	movzwl	-160(%r10,%rax,2), %r9d
	movzwl	-80(%r10,%r8,2), %r8d
	movw	%r9w, (%rdx,%rcx)
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-128(%r10,%rax), %eax
.Ljda_g9np_avx2_onew1_BB3_V:
	vmovdqa	(%rdi), %ymm8
	vpmaxsb	%ymm0, %ymm8, %ymm9
	vpaddb	%ymm1, %ymm9, %ymm10
	vpxor	%ymm2, %ymm10, %ymm10
	vpmaxsb	%ymm3, %ymm10, %ymm10
	vpcmpeqb	%ymm4, %ymm8, %ymm8
	vpand	%ymm5, %ymm8, %ymm8
	vpshufb	%ymm10, %ymm8, %ymm11
	vpor	%ymm8, %ymm11, %ymm8
	vpshufb	%ymm10, %ymm10, %ymm10
	vpshufb	%ymm10, %ymm8, %ymm11
	vpor	%ymm8, %ymm11, %ymm8
	vpshufb	%ymm10, %ymm10, %ymm10
	vpshufb	%ymm10, %ymm8, %ymm11
	vpshufb	%ymm10, %ymm10, %ymm10
	vpaddb	%ymm6, %ymm9, %ymm9
	vpshufb	%ymm10, %ymm9, %ymm9
	vpshufb	%ymm9, %ymm9, %ymm10
	vpmaxub	%ymm10, %ymm9, %ymm10
	vpor	%ymm8, %ymm11, %ymm8
	vpshufb	%ymm9, %ymm8, %ymm9
	vpunpcklbw	%ymm9, %ymm8, %ymm11
	vpunpckhbw	%ymm8, %ymm9, %ymm8
	vmovdqa	%xmm10, 48(%r10)
	vextracti128	$1, %ymm10, %xmm9
	vmovdqa	%xmm9, 16(%r10)
	vpaddb	%xmm7, %xmm10, %xmm10
	vpshufb	%xmm10, %xmm9, %xmm9
	vpmaxub	%xmm10, %xmm9, %xmm9
	vmovdqa	%xmm9, (%r10)
	vmovdqa	%xmm11, 96(%r10)
	vmovdqa	%xmm8, 112(%r10)
	vextracti128	$1, %ymm11, 176(%r10)
	vextracti128	$1, %ymm8, 192(%r10)
	addq	$512, %r10
	btrq	$12, %r10
	addq	$32, %rdi
	cmpq	%rbx, %rdi
	cmovaq	%rbx, %rdi
	addq	$4, %rcx
	js	.Ljda_g9np_avx2_onew1_BB3_V
	cmpq	%rcx, %rsi
	jne	.Ljda_g9np_avx2_onew1_BB3_2
.Ljda_g9np_avx2_onew1_BB3_3:
	leaq	-8(%rbp), %rsp
	popq	%rbx
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g9np_avx2_onew1, .-jda_asm_g9np_avx2_onew1
    .section .note.GNU-stack,"",@progbits
#endif
