// g8_avx2 redesign: tables go to a ring of 8 stack slots (16-byte stores, no lane permutes) and the scalar chain reads
// them 8 blocks later, so its loads hit L1 instead of forwarding from in-flight stores.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g8_avx2_onew1_LCPI2_0:
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
.Ljda_g8_avx2_onew1_LCPI2_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g8_avx2_onew1_LCPI2_11:
	.byte	91
    .p2align 5
.Ljda_g8_avx2_onew1_LCPI2_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g8_avx2_onew1_LCPI2_13:
	.byte	240
    .p2align 5
.Ljda_g8_avx2_onew1_LCPI2_3:
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
.Ljda_g8_avx2_onew1_LCPI2_4:
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
.Ljda_g8_avx2_onew1_LCPI2_7:
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
.Ljda_g8_avx2_onew1_LCPI2_9:
	.byte	95
    .p2align 5
    .p2align 5
.Ljda_g8_avx2_onew1_TAINIT:
	.fill	16, 1, 128
	.byte	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
    .text
    .p2align 4
    .globl jda_asm_g8_avx2_onew1
    .hidden jda_asm_g8_avx2_onew1
    .type jda_asm_g8_avx2_onew1,@function
jda_asm_g8_avx2_onew1:
	pushq	%rbp
	movq	%rsp, %rbp
	pushq	%rbx
	andq	$-8192, %rsp
	subq	$8192, %rsp
	addq	$31, %rsi
	shrq	$5, %rsi
	je	.Ljda_g8_avx2_onew1_BB_3
	vmovdqa	.Ljda_g8_avx2_onew1_TAINIT(%rip), %ymm1
	vpbroadcastb	.Ljda_g8_avx2_onew1_LCPI2_0(%rip), %ymm2
	vpxor	%xmm0, %xmm0, %xmm0
	movabsq	$-7523661662112280176, %r8
	movabsq	$-8681104427521506944, %r9
	movq	%rsp, %r10
	leaq	4096(%rsp), %r11
.Ljda_g8_avx2_onew1_BB_1:
	vmovdqa	%ymm1, (%r10)
	movq	%r8, 32(%r10)
	vmovdqa	%ymm2, 64(%r10)
	movq	%r9, 96(%r10)
	vmovdqa	%ymm0, 128(%r10)
	vmovdqa	%ymm0, 160(%r10)
	vmovdqa	%ymm0, 192(%r10)
	vmovdqa	%ymm0, 256(%r10)
	vmovdqa	%ymm0, 288(%r10)
	vmovdqa	%ymm0, 320(%r10)
	addq	$512, %r10
	cmpq	%r11, %r10
	jne	.Ljda_g8_avx2_onew1_BB_1
	movq	%rsi, %rbx
	shlq	$5, %rbx
	leaq	-32(%rdi,%rbx), %rbx
	shlq	$2, %rsi
	movq	$-32, %rcx
	movq	%rsp, %r10
	movl	$128, %eax
	vpbroadcastb	.Ljda_g8_avx2_onew1_LCPI2_9(%rip), %ymm1
	vpbroadcastq	.Ljda_g8_avx2_onew1_LCPI2_10(%rip), %ymm2
	vmovdqa	.Ljda_g8_avx2_onew1_LCPI2_3(%rip), %ymm3
	vmovdqa	.Ljda_g8_avx2_onew1_LCPI2_4(%rip), %ymm4
	vpbroadcastb	.Ljda_g8_avx2_onew1_LCPI2_11(%rip), %ymm5
	vpbroadcastq	.Ljda_g8_avx2_onew1_LCPI2_12(%rip), %ymm6
	vmovdqa	.Ljda_g8_avx2_onew1_LCPI2_7(%rip), %ymm7
	vpbroadcastb	.Ljda_g8_avx2_onew1_LCPI2_13(%rip), %ymm8
	jmp	.Ljda_g8_avx2_onew1_BB_4
	.p2align	4
.Ljda_g8_avx2_onew1_BB_2:
	movzbl	-128(%r10,%rax), %r8d
	movzwl	-128(%r10,%rax,2), %r9d
	movw	%r9w, (%rdx,%rcx)
	movzwl	(%r10,%r8,2), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	movzbl	-64(%r10,%rax), %eax
.Ljda_g8_avx2_onew1_BB_4:
	vmovdqa	(%rdi), %ymm9
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
	vmovdqa	%xmm11, (%r10)
	vmovdqa	%xmm12, 64(%r10)
	vextracti128	$1, %ymm11, 80(%r10)
	vmovdqa	%xmm10, 128(%r10)
	vmovdqa	%xmm9, 144(%r10)
	vextracti128	$1, %ymm10, 256(%r10)
	vextracti128	$1, %ymm9, 272(%r10)
	addq	$512, %r10
	btrq	$12, %r10
	addq	$32, %rdi
	cmpq	%rbx, %rdi
	cmovaq	%rbx, %rdi
	addq	$4, %rcx
	js	.Ljda_g8_avx2_onew1_BB_4
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_avx2_onew1_BB_2
.Ljda_g8_avx2_onew1_BB_3:
	leaq	-8(%rbp), %rsp
	popq	%rbx
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g8_avx2_onew1, .-jda_asm_g8_avx2_onew1
    .section .note.GNU-stack,"",@progbits
#endif
