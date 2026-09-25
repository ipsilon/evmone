// g9np_avx2_o41: g9np_avx2_ofix vector core (no block composition); tables in a ring of 4 slots of 256 B.
// The scalar chain e2 = TA[e], e = TB[e2] reads the slot written 4 blocks earlier: the stores have
// usually committed, so both loads are plain L1 hits (Zen 3 forwards a byte load only from the store start), and
// both use (base,index) addressing without displacement (4 cycles on Zen 3 instead of 5).
// e is the plain entry 0..32; e2 is 0x80 + entry into the second 16-byte group.
// Slot: TA 0 (+16 tail) | W0 48 (+80 zero) | TB 128 (+144 tail) | W1 176 (+208 zero).
// The first 4 blocks run the vector part only and also write the slot's constant tails (6 stores);
// the last 4 blocks run the chain only. No per-call setup beyond the frame and the constants.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g9np_avx2_o41_C80:
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
.Ljda_g9np_avx2_o41_RAMP:
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
	.byte	16
	.byte	17
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
.Ljda_g9np_avx2_o41_IOTA:
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
.Ljda_g9np_avx2_o41_K8:
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
.Ljda_g9np_avx2_o41_N16:
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
.Ljda_g9np_avx2_o41_HI80:
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
	.byte	128
.Ljda_g9np_avx2_o41_Q26:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
.Ljda_g9np_avx2_o41_BIT:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
.Ljda_g9np_avx2_o41_5F:
	.byte	95
.Ljda_g9np_avx2_o41_5B:
	.byte	91
    .text
    .p2align 4
    .globl jda_asm_g9np_avx2_o41
    .hidden jda_asm_g9np_avx2_o41
    .type jda_asm_g9np_avx2_o41,@function
jda_asm_g9np_avx2_o41:
	addq	$31, %rsi
	shrq	$5, %rsi
	je	.Ljda_g9np_avx2_o41_ret
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$1024, %rsp
	andq	$-2048, %rsp
	vmovdqa	.Ljda_g9np_avx2_o41_C80(%rip), %ymm12
	vmovdqa	.Ljda_g9np_avx2_o41_HI80(%rip), %ymm7
	vmovdqa	.Ljda_g9np_avx2_o41_RAMP(%rip), %ymm13
	vpxor	%xmm14, %xmm14, %xmm14
	movl	$4, %ecx
	cmpq	%rcx, %rsi
	cmovbq	%rsi, %rcx
	shlq	$5, %rcx
	addq	%rcx, %rdi
	shrq	$3, %rcx
	shlq	$2, %rsi
	movq	%rsi, %r11
	subq	%rcx, %r11
	negq	%rcx
	movq	%rsp, %r10
	xorl	%eax, %eax
	vpbroadcastb	.Ljda_g9np_avx2_o41_5F(%rip), %ymm0
	vpbroadcastq	.Ljda_g9np_avx2_o41_Q26(%rip), %ymm1
	vmovdqa	.Ljda_g9np_avx2_o41_K8(%rip), %ymm2
	vmovdqa	.Ljda_g9np_avx2_o41_IOTA(%rip), %ymm3
	vpbroadcastb	.Ljda_g9np_avx2_o41_5B(%rip), %ymm4
	vpbroadcastq	.Ljda_g9np_avx2_o41_BIT(%rip), %ymm5
	vmovdqa	.Ljda_g9np_avx2_o41_N16(%rip), %ymm6
.Ljda_g9np_avx2_o41_I:
	vmovdqu	%ymm12, 16(%r10)
	vmovdqa	%xmm14, 80(%r10)
	vmovdqa	%ymm14, 96(%r10)
	vmovdqu	%ymm13, 144(%r10)
	vmovdqa	%xmm14, 208(%r10)
	vmovdqa	%ymm14, 224(%r10)
	jmp	.Ljda_g9np_avx2_o41_V
	.p2align	4
.Ljda_g9np_avx2_o41_C:
	movzbl	(%r10,%rax), %r8d
	movzwl	48(%r10,%rax,2), %r9d
	movzbl	(%r10,%r8), %eax
	movzwl	-80(%r10,%r8,2), %r8d
	movw	%r9w, (%rdx,%rcx)
	movw	%r8w, 2(%rdx,%rcx)
.Ljda_g9np_avx2_o41_V:
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
	vpxor	%ymm7, %ymm10, %ymm10
	vmovdqa	%xmm10, (%r10)
	vextracti128	$1, %ymm10, 128(%r10)
	vmovdqa	%xmm11, 48(%r10)
	vmovdqa	%xmm8, 64(%r10)
	vextracti128	$1, %ymm11, 176(%r10)
	vextracti128	$1, %ymm8, 192(%r10)
	addq	$256, %r10
	btrq	$10, %r10
	addq	$4, %rcx
	js	.Ljda_g9np_avx2_o41_I
	cmpq	%r11, %rcx
	jb	.Ljda_g9np_avx2_o41_C
	movl	%ecx, %r10d
	andl	$15, %r10d
	shll	$6, %r10d
	addq	%rsp, %r10
	cmpq	%rsi, %rcx
	jae	.Ljda_g9np_avx2_o41_done
.Ljda_g9np_avx2_o41_E:
	movzbl	(%r10,%rax), %r8d
	movzwl	48(%r10,%rax,2), %r9d
	movzbl	(%r10,%r8), %eax
	movzwl	-80(%r10,%r8,2), %r8d
	movw	%r9w, (%rdx,%rcx)
	movw	%r8w, 2(%rdx,%rcx)
	addq	$256, %r10
	btrq	$10, %r10
	addq	$4, %rcx
	cmpq	%rsi, %rcx
	jb	.Ljda_g9np_avx2_o41_E
.Ljda_g9np_avx2_o41_done:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
.Ljda_g9np_avx2_o41_ret:
	retq
    .size jda_asm_g9np_avx2_o41, .-jda_asm_g9np_avx2_o41
    .section .note.GNU-stack,"",@progbits
#endif
