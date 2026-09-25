// Frozen clang-21 -O3 builds of the g8_sse, g8_avx2, g9np_avx2 and c8_avx2 kernels (x86-64 ELF).
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g8_sse_LCPI0_0:
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
.Ljda_g8_sse_LCPI0_1:
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
.Ljda_g8_sse_LCPI0_10:
	.zero	16,240
    .p2align 5
.Ljda_g8_sse_LCPI0_2:
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
.Ljda_g8_sse_LCPI0_3:
	.zero	16,95
    .p2align 5
.Ljda_g8_sse_LCPI0_4:
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
.Ljda_g8_sse_LCPI0_5:
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
.Ljda_g8_sse_LCPI0_6:
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
.Ljda_g8_sse_LCPI0_7:
	.zero	16,91
    .p2align 5
.Ljda_g8_sse_LCPI0_8:
	.quad	-9205322385119247871
	.quad	-9205322385119247871
    .p2align 5
.Ljda_g8_sse_LCPI0_9:
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
    .globl jda_asm_g8_sse
    .hidden jda_asm_g8_sse
    .type jda_asm_g8_sse,@function
jda_asm_g8_sse:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$448, %rsp
	movaps	.Ljda_g8_sse_LCPI0_0(%rip), %xmm0
	movups	%xmm0, 152(%rsp)
	movups	%xmm0, 104(%rsp)
	xorps	%xmm0, %xmm0
	movaps	%xmm0, 352(%rsp)
	movups	%xmm0, 248(%rsp)
	movaps	%xmm0, 304(%rsp)
	movups	%xmm0, 200(%rsp)
	movdqa	.Ljda_g8_sse_LCPI0_1(%rip), %xmm1
	movdqu	%xmm1, 168(%rsp)
	movdqu	%xmm1, 120(%rsp)
	movaps	%xmm0, 368(%rsp)
	movups	%xmm0, 264(%rsp)
	movaps	%xmm0, 320(%rsp)
	movups	%xmm0, 216(%rsp)
	movdqa	.Ljda_g8_sse_LCPI0_2(%rip), %xmm0
	movdqa	%xmm0, 16(%rsp)
	movabsq	$-7523661662112280176, %rax
	movq	%rax, 32(%rsp)
	movabsq	$-8681104427521506944, %rax
	movq	%rax, 80(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Ljda_g8_sse_BB0_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	movdqa	.Ljda_g8_sse_LCPI0_3(%rip), %xmm0
	movdqa	.Ljda_g8_sse_LCPI0_4(%rip), %xmm1
	movq	.Ljda_g8_sse_LCPI0_5(%rip), %xmm2
	movdqa	.Ljda_g8_sse_LCPI0_6(%rip), %xmm3
	movdqa	.Ljda_g8_sse_LCPI0_7(%rip), %xmm4
	movdqa	.Ljda_g8_sse_LCPI0_8(%rip), %xmm5
	movdqa	.Ljda_g8_sse_LCPI0_9(%rip), %xmm6
	movdqa	.Ljda_g8_sse_LCPI0_10(%rip), %xmm7
	.p2align	4
.Ljda_g8_sse_BB0_2:
	movdqa	(%rdi,%rcx,8), %xmm13
	movdqa	16(%rdi,%rcx,8), %xmm12
	movdqa	%xmm13, %xmm8
	pmaxsb	%xmm0, %xmm8
	movdqa	%xmm8, %xmm10
	paddb	%xmm1, %xmm10
	pxor	%xmm2, %xmm10
	pmaxsb	%xmm3, %xmm10
	pcmpeqb	%xmm4, %xmm13
	pand	%xmm5, %xmm13
	movdqa	%xmm12, %xmm9
	pmaxsb	%xmm0, %xmm9
	movdqa	%xmm9, %xmm11
	paddb	%xmm1, %xmm11
	pxor	%xmm2, %xmm11
	pmaxsb	%xmm3, %xmm11
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
	movdqa	%xmm10, (%rsp)
	movdqa	%xmm11, 64(%rsp)
	paddb	%xmm7, %xmm10
	pshufb	%xmm10, %xmm11
	pmaxub	%xmm10, %xmm11
	movdqa	%xmm11, 48(%rsp)
	movq	%xmm8, 96(%rsp)
	movq	%xmm9, 144(%rsp)
	movq	%xmm13, 192(%rsp)
	movq	%xmm12, 240(%rsp)
	movdqa	%xmm13, 288(%rsp)
	movdqa	%xmm12, 336(%rsp)
	movzbl	-32(%rsp,%rax), %r8d
	movzbl	-128(%rsp,%rax), %r9d
	movzbl	16(%rsp,%r9), %r10d
	movzbl	64(%rsp,%rax), %r11d
	movb	%r11b, (%rdx,%rcx)
	movzbl	176(%rsp,%r8), %r8d
	movb	%r8b, 1(%rdx,%rcx)
	movzbl	112(%rsp,%r9), %r8d
	movb	%r8b, 2(%rdx,%rcx)
	movzbl	224(%rsp,%r10), %r8d
	movb	%r8b, 3(%rdx,%rcx)
	movzbl	-80(%rsp,%rax), %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_sse_BB0_2
.Ljda_g8_sse_BB0_3:
	movq	%rbp, %rsp
	popq	%rbp
	retq
    .size jda_asm_g8_sse, .-jda_asm_g8_sse
    .section .rodata
    .p2align 5
.Ljda_g8_avx2_LCPI2_0:
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
.Ljda_g8_avx2_LCPI2_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g8_avx2_LCPI2_11:
	.byte	91
    .p2align 5
.Ljda_g8_avx2_LCPI2_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g8_avx2_LCPI2_13:
	.byte	240
    .p2align 5
.Ljda_g8_avx2_LCPI2_3:
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
.Ljda_g8_avx2_LCPI2_4:
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
.Ljda_g8_avx2_LCPI2_7:
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
.Ljda_g8_avx2_LCPI2_9:
	.byte	95
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g8_avx2
    .hidden jda_asm_g8_avx2
    .type jda_asm_g8_avx2,@function
jda_asm_g8_avx2:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$448, %rsp
	vmovdqa	.Ljda_g8_avx2_LCPI2_0(%rip), %xmm0
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
	jb	.Ljda_g8_avx2_BB2_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_g8_avx2_LCPI2_9(%rip), %ymm1
	vpbroadcastq	.Ljda_g8_avx2_LCPI2_10(%rip), %ymm2
	vmovdqa	.Ljda_g8_avx2_LCPI2_3(%rip), %ymm3
	vmovdqa	.Ljda_g8_avx2_LCPI2_4(%rip), %ymm4
	vpbroadcastb	.Ljda_g8_avx2_LCPI2_11(%rip), %ymm5
	vpbroadcastq	.Ljda_g8_avx2_LCPI2_12(%rip), %ymm6
	vmovdqa	.Ljda_g8_avx2_LCPI2_7(%rip), %ymm7
	vpbroadcastb	.Ljda_g8_avx2_LCPI2_13(%rip), %ymm8
	.p2align	4
.Ljda_g8_avx2_BB2_2:
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
	vpblendd	$240, %ymm11, %ymm12, %ymm11
	vmovdqa	%ymm11, 64(%rsp)
	vinserti128	$1, %xmm9, %ymm10, %ymm11
	vmovdqa	%ymm11, 128(%rsp)
	vperm2i128	$49, %ymm9, %ymm10, %ymm9
	vmovdqa	%ymm9, 256(%rsp)
	movzbl	-128(%rsp,%rax), %r8d
	movzwl	-128(%rsp,%rax,2), %r9d
	movw	%r9w, (%rdx,%rcx)
	movzwl	(%rsp,%r8,2), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-64(%rsp,%rax), %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_avx2_BB2_2
.Ljda_g8_avx2_BB2_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g8_avx2, .-jda_asm_g8_avx2
    .section .rodata
    .p2align 5
.Ljda_g9np_avx2_LCPI3_0:
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
.Ljda_g9np_avx2_LCPI3_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g9np_avx2_LCPI3_11:
	.byte	91
    .p2align 5
.Ljda_g9np_avx2_LCPI3_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g9np_avx2_LCPI3_3:
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
.Ljda_g9np_avx2_LCPI3_4:
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
.Ljda_g9np_avx2_LCPI3_7:
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
.Ljda_g9np_avx2_LCPI3_8:
	.zero	16,240
    .p2align 5
.Ljda_g9np_avx2_LCPI3_9:
	.byte	95
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g9np_avx2
    .hidden jda_asm_g9np_avx2
    .type jda_asm_g9np_avx2,@function
jda_asm_g9np_avx2:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$320, %rsp
	addq	$31, %rsi
	vmovdqa	.Ljda_g9np_avx2_LCPI3_0(%rip), %ymm0
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
	jb	.Ljda_g9np_avx2_BB3_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_g9np_avx2_LCPI3_9(%rip), %ymm0
	vpbroadcastq	.Ljda_g9np_avx2_LCPI3_10(%rip), %ymm1
	vmovdqa	.Ljda_g9np_avx2_LCPI3_3(%rip), %ymm2
	vmovdqa	.Ljda_g9np_avx2_LCPI3_4(%rip), %ymm3
	vpbroadcastb	.Ljda_g9np_avx2_LCPI3_11(%rip), %ymm4
	vpbroadcastq	.Ljda_g9np_avx2_LCPI3_12(%rip), %ymm5
	vmovdqa	.Ljda_g9np_avx2_LCPI3_7(%rip), %ymm6
	vmovdqa	.Ljda_g9np_avx2_LCPI3_8(%rip), %xmm7
	.p2align	4
.Ljda_g9np_avx2_BB3_2:
	vmovdqa	(%rdi,%rcx,8), %ymm8
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
	vmovdqa	%xmm10, 48(%rsp)
	vmovdqa	%ymm10, (%rsp)
	vmovdqa	16(%rsp), %xmm9
	vpaddb	%xmm7, %xmm10, %xmm10
	vpshufb	%xmm10, %xmm9, %xmm9
	vpmaxub	%xmm10, %xmm9, %xmm9
	vmovdqa	%xmm9, (%rsp)
	vmovdqa	%xmm11, 96(%rsp)
	vmovdqa	%xmm8, 112(%rsp)
	vextracti128	$1, %ymm11, 176(%rsp)
	vextracti128	$1, %ymm8, 192(%rsp)
	movzbl	-80(%rsp,%rax), %r8d
	movzwl	-160(%rsp,%rax,2), %r9d
	movzwl	-80(%rsp,%r8,2), %r8d
	movw	%r9w, (%rdx,%rcx)
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-128(%rsp,%rax), %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g9np_avx2_BB3_2
.Ljda_g9np_avx2_BB3_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g9np_avx2, .-jda_asm_g9np_avx2
    .section .rodata
    .p2align 5
.Ljda_c8_avx2_LCPI4_0:
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
.Ljda_c8_avx2_LCPI4_1:
	.byte	128
	.byte	129
	.byte	130
	.byte	131
	.byte	132
	.byte	133
	.byte	134
	.byte	135
	.byte	144
	.byte	145
	.byte	146
	.byte	147
	.byte	148
	.byte	149
	.byte	150
	.byte	151
    .p2align 5
.Ljda_c8_avx2_LCPI4_10:
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
.Ljda_c8_avx2_LCPI4_11:
	.zero	16,240
    .p2align 5
.Ljda_c8_avx2_LCPI4_12:
	.byte	95
    .p2align 5
.Ljda_c8_avx2_LCPI4_13:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_c8_avx2_LCPI4_14:
	.byte	91
    .p2align 5
.Ljda_c8_avx2_LCPI4_15:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_c8_avx2_LCPI4_2:
	.byte	160
	.byte	161
	.byte	162
	.byte	163
	.byte	164
	.byte	165
	.byte	166
	.byte	167
	.byte	176
	.byte	177
	.byte	178
	.byte	179
	.byte	180
	.byte	181
	.byte	182
	.byte	183
    .p2align 5
.Ljda_c8_avx2_LCPI4_3:
	.byte	192
	.byte	193
	.byte	194
	.byte	195
	.byte	196
	.byte	197
	.byte	198
	.byte	199
	.byte	208
	.byte	209
	.byte	210
	.byte	211
	.byte	212
	.byte	213
	.byte	214
	.byte	215
    .p2align 5
.Ljda_c8_avx2_LCPI4_6:
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
.Ljda_c8_avx2_LCPI4_7:
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
    .text
    .p2align 4
    .globl jda_asm_c8_avx2
    .hidden jda_asm_c8_avx2
    .type jda_asm_c8_avx2,@function
jda_asm_c8_avx2:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$448, %rsp
	vxorps	%xmm0, %xmm0, %xmm0
	vmovaps	%ymm0, (%rsp)
	vmovaps	%ymm0, 32(%rsp)
	vmovaps	%ymm0, 96(%rsp)
	vmovaps	%ymm0, 320(%rsp)
	vmovaps	%ymm0, 352(%rsp)
	vmovaps	%ymm0, 288(%rsp)
	vmovaps	%ymm0, 256(%rsp)
	vmovaps	%ymm0, 224(%rsp)
	vmovaps	%ymm0, 192(%rsp)
	vmovaps	%ymm0, 160(%rsp)
	vmovaps	%ymm0, 128(%rsp)
	vmovaps	%ymm0, 64(%rsp)
	vmovaps	.Ljda_c8_avx2_LCPI4_0(%rip), %xmm0
	vmovaps	%xmm0, 16(%rsp)
	movabsq	$-7523661662112280176, %rax
	movq	%rax, 32(%rsp)
	movabsq	$-8681104427521506944, %rax
	movq	%rax, 96(%rsp)
	vmovaps	.Ljda_c8_avx2_LCPI4_1(%rip), %xmm0
	vmovaps	%xmm0, 336(%rsp)
	vmovaps	.Ljda_c8_avx2_LCPI4_2(%rip), %xmm0
	vmovaps	%xmm0, 352(%rsp)
	vmovdqa	.Ljda_c8_avx2_LCPI4_3(%rip), %xmm0
	vmovdqa	%xmm0, 368(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Ljda_c8_avx2_BB4_3
	shrq	$5, %rsi
	vmovdqa	(%rdi), %ymm8
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_c8_avx2_LCPI4_12(%rip), %ymm0
	vpbroadcastq	.Ljda_c8_avx2_LCPI4_13(%rip), %ymm1
	vmovdqa	.Ljda_c8_avx2_LCPI4_6(%rip), %ymm2
	vmovdqa	.Ljda_c8_avx2_LCPI4_7(%rip), %ymm3
	vpbroadcastb	.Ljda_c8_avx2_LCPI4_14(%rip), %ymm4
	vpbroadcastq	.Ljda_c8_avx2_LCPI4_15(%rip), %ymm5
	vmovdqa	.Ljda_c8_avx2_LCPI4_10(%rip), %ymm6
	vmovdqa	.Ljda_c8_avx2_LCPI4_11(%rip), %xmm7
	.p2align	4
.Ljda_c8_avx2_BB4_2:
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
	vpunpckhbw	%ymm8, %ymm9, %ymm9
	vmovdqa	32(%rdi,%rcx,8), %ymm8
	vmovdqa	%xmm10, (%rsp)
	vmovdqa	%ymm10, 64(%rsp)
	vmovdqa	80(%rsp), %xmm12
	vpaddb	%xmm7, %xmm10, %xmm10
	vpshufb	%xmm10, %xmm12, %xmm12
	vpmaxub	%xmm10, %xmm12, %xmm10
	vmovdqa	%xmm10, 64(%rsp)
	vmovdqa	%ymm11, 128(%rsp)
	vmovdqa	%ymm9, 160(%rsp)
	movzbl	-128(%rsp,%rax), %r8d
	movzbl	208(%rsp,%rax), %r9d
	movzbl	208(%rsp,%r8), %r8d
	movzwl	-128(%rsp,%r9,2), %r9d
	movw	%r9w, (%rdx,%rcx)
	movzwl	-112(%rsp,%r8,2), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-64(%rsp,%rax), %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_c8_avx2_BB4_2
.Ljda_c8_avx2_BB4_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_c8_avx2, .-jda_asm_c8_avx2
    .section .note.GNU-stack,"",@progbits
#endif
