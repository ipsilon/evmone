// g8_avx2 with the loop-carried entry kept in an xmm byte-broadcast and looked up with vpshufb
// (TA then TB, TB reloaded from a stored ymm instead of vextracti128); the JUMPDEST words are read from plainly stored unpack results via a static remap table.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g8_avx2_fnew2_LCPI2_0:
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
.Ljda_g8_avx2_fnew2_LCPI2_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g8_avx2_fnew2_LCPI2_11:
	.byte	91
    .p2align 5
.Ljda_g8_avx2_fnew2_LCPI2_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g8_avx2_fnew2_LCPI2_13:
	.byte	240
    .p2align 5
.Ljda_g8_avx2_fnew2_LCPI2_3:
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
.Ljda_g8_avx2_fnew2_LCPI2_4:
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
.Ljda_g8_avx2_fnew2_LCPI2_7:
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
.Ljda_g8_avx2_fnew2_LCPI2_9:
	.byte	95
    .p2align 5
.Ljda_g8_avx2_fnew2_R:
	.byte	0,2,4,6,8,10,12,14,32,34,36,38,40,42,44,46
	.byte	64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
	.byte	64,64,64,64,64,64,64,64
    .p2align 5
.Ljda_g8_avx2_fnew2_E0:
	.byte	112
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g8_avx2_fnew2
    .hidden jda_asm_g8_avx2_fnew2
    .type jda_asm_g8_avx2_fnew2,@function
jda_asm_g8_avx2_fnew2:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$256, %rsp
	vpxor	%xmm0, %xmm0, %xmm0
	vmovdqa	%ymm0, 128(%rsp)
	vmovdqu	.Ljda_g8_avx2_fnew2_R(%rip), %ymm1
	vmovdqu	%ymm1, 160(%rsp)
	movq	.Ljda_g8_avx2_fnew2_R+32(%rip), %rax
	movq	%rax, 192(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Ljda_g8_avx2_fnew2_BB2_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$112, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_g8_avx2_fnew2_LCPI2_9(%rip), %ymm1
	vpbroadcastq	.Ljda_g8_avx2_fnew2_LCPI2_10(%rip), %ymm2
	vmovdqa	.Ljda_g8_avx2_fnew2_LCPI2_3(%rip), %ymm3
	vmovdqa	.Ljda_g8_avx2_fnew2_LCPI2_4(%rip), %ymm4
	vpbroadcastb	.Ljda_g8_avx2_fnew2_LCPI2_11(%rip), %ymm5
	vpbroadcastq	.Ljda_g8_avx2_fnew2_LCPI2_12(%rip), %ymm6
	vmovdqa	.Ljda_g8_avx2_fnew2_LCPI2_7(%rip), %ymm7
	vpbroadcastb	.Ljda_g8_avx2_fnew2_LCPI2_13(%rip), %ymm8
	vpbroadcastb	.Ljda_g8_avx2_fnew2_E0(%rip), %xmm15
	.p2align	4
.Ljda_g8_avx2_fnew2_BB2_2:
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
	vpaddb	%ymm8, %ymm11, %ymm11
	vpor	%ymm9, %ymm12, %ymm9
	vpshufb	%ymm10, %ymm9, %ymm10
	vpunpcklbw	%ymm10, %ymm9, %ymm10
	vpunpckhbw	%ymm9, %ymm0, %ymm9
	vmovdqa	%ymm11, (%rsp)
	vmovdqa	16(%rsp), %xmm13
	vmovdqa	%ymm10, 64(%rsp)
	vmovdqa	%ymm9, 96(%rsp)
	vpshufb	%xmm15, %xmm11, %xmm12
	vpaddb	%xmm8, %xmm15, %xmm14
	vpmaxub	%xmm14, %xmm12, %xmm12
	vpshufb	%xmm12, %xmm13, %xmm13
	vpaddb	%xmm8, %xmm12, %xmm14
	vpmaxub	%xmm14, %xmm13, %xmm15
	vmovd	%xmm12, %r8d
	movzbl	48(%rsp,%rax), %r9d
	movzbl	%r8b, %r8d
	movzwl	64(%rsp,%r9), %r9d
	movzbl	48(%rsp,%r8), %r8d
	movw	%r9w, (%rdx,%rcx)
	movzwl	80(%rsp,%r8), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	vmovd	%xmm15, %eax
	movzbl	%al, %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g8_avx2_fnew2_BB2_2
.Ljda_g8_avx2_fnew2_BB2_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g8_avx2_fnew2, .-jda_asm_g8_avx2_fnew2
    .section .note.GNU-stack,"",@progbits
#endif
