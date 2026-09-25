// g9np_avx2_o42: g9np_avx2_ofix vector core (no block composition, byte outputs); tables in a ring of 4 slots of 512 B.
// The scalar chain e2 = TA[e], e = TB[e2] reads the slot written 4 blocks earlier: the stores have
// usually committed, so both loads are plain L1 hits (Zen 3 forwards a byte load only from the store start), and
// both use (base,index) addressing without displacement (4 cycles on Zen 3 instead of 5).
// e is the plain entry 0..32; e2 is 0x80 + entry into the second 16-byte group.
// Outputs as bytes (no vpunpck): X = v & [ff x8, 0 x8], Y = pshufb(v, min(n, [ff x8, 8..15])).
// Slot (512 B): TA 0 (+16 tail) | X0 48 | TB 128 (+144 tail) | Y0 272 | X1 320 | Y1 368; X/Y zero tails 17 B.
// The first 4 blocks run the vector part only and also write the slot's constant tails (6 stores);
// the last 4 blocks run the chain only. No per-call setup beyond the frame and the constants.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g9np_avx2_o42_C80:
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
.Ljda_g9np_avx2_o42_RAMP:
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
.Ljda_g9np_avx2_o42_IOTA:
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
.Ljda_g9np_avx2_o42_K8:
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
.Ljda_g9np_avx2_o42_N16:
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
.Ljda_g9np_avx2_o42_CM:
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	12
	.byte	13
	.byte	14
	.byte	15
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	12
	.byte	13
	.byte	14
	.byte	15
.Ljda_g9np_avx2_o42_MASK:
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
.Ljda_g9np_avx2_o42_HI80:
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
.Ljda_g9np_avx2_o42_Q26:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
.Ljda_g9np_avx2_o42_BIT:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
.Ljda_g9np_avx2_o42_5F:
	.byte	95
.Ljda_g9np_avx2_o42_5B:
	.byte	91
    .text
    .p2align 4
    .globl jda_asm_g9np_avx2_o42
    .hidden jda_asm_g9np_avx2_o42
    .type jda_asm_g9np_avx2_o42,@function
jda_asm_g9np_avx2_o42:
	addq	$31, %rsi
	shrq	$5, %rsi
	je	.Ljda_g9np_avx2_o42_ret
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$2048, %rsp
	andq	$-4096, %rsp
	vmovdqa	.Ljda_g9np_avx2_o42_C80(%rip), %ymm14
	vmovdqa	.Ljda_g9np_avx2_o42_HI80(%rip), %ymm7
	vmovdqa	.Ljda_g9np_avx2_o42_RAMP(%rip), %ymm15

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
	vpbroadcastb	.Ljda_g9np_avx2_o42_5F(%rip), %ymm0
	vpbroadcastq	.Ljda_g9np_avx2_o42_Q26(%rip), %ymm1
	vmovdqa	.Ljda_g9np_avx2_o42_K8(%rip), %ymm2
	vmovdqa	.Ljda_g9np_avx2_o42_IOTA(%rip), %ymm3
	vpbroadcastb	.Ljda_g9np_avx2_o42_5B(%rip), %ymm4
	vpbroadcastq	.Ljda_g9np_avx2_o42_BIT(%rip), %ymm5
	vmovdqa	.Ljda_g9np_avx2_o42_N16(%rip), %ymm6
	vmovdqa	.Ljda_g9np_avx2_o42_CM(%rip), %ymm13
	vmovdqa	.Ljda_g9np_avx2_o42_MASK(%rip), %ymm12
.Ljda_g9np_avx2_o42_I:
	vpxor	%xmm11, %xmm11, %xmm11
	vmovdqa	%ymm11, 64(%r10)
	vmovdqa	%ymm11, 288(%r10)
	vmovdqu	%ymm11, 336(%r10)
	vmovdqa	%ymm11, 384(%r10)
	vmovdqu	%ymm14, 16(%r10)
	vmovdqu	%ymm15, 144(%r10)
	jmp	.Ljda_g9np_avx2_o42_V
	.p2align	4
.Ljda_g9np_avx2_o42_C:
	movzbl	(%r10,%rax), %r8d
	movzbl	48(%r10,%rax), %r9d
	movb	%r9b, (%rdx,%rcx)
	movzbl	272(%r10,%rax), %r9d
	movb	%r9b, 1(%rdx,%rcx)
	movzbl	(%r10,%r8), %eax
	movzbl	192(%r10,%r8), %r9d
	movb	%r9b, 2(%rdx,%rcx)
	movzbl	240(%r10,%r8), %r9d
	movb	%r9b, 3(%rdx,%rcx)
.Ljda_g9np_avx2_o42_V:
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
	vpminub	%ymm13, %ymm9, %ymm9
	vpshufb	%ymm9, %ymm8, %ymm9
	vpand	%ymm12, %ymm8, %ymm8
	vpxor	%ymm7, %ymm10, %ymm10
	vmovdqa	%xmm10, (%r10)
	vextracti128	$1, %ymm10, 128(%r10)
	vmovdqa	%xmm8, 48(%r10)
	vextracti128	$1, %ymm8, 320(%r10)
	vmovdqa	%xmm9, 272(%r10)
	vextracti128	$1, %ymm9, 368(%r10)
	addq	$512, %r10
	btrq	$11, %r10
	addq	$4, %rcx
	js	.Ljda_g9np_avx2_o42_I
	cmpq	%r11, %rcx
	jb	.Ljda_g9np_avx2_o42_C
	movl	%ecx, %r10d
	andl	$15, %r10d
	shll	$7, %r10d
	addq	%rsp, %r10
	cmpq	%rsi, %rcx
	jae	.Ljda_g9np_avx2_o42_done
.Ljda_g9np_avx2_o42_E:
	movzbl	(%r10,%rax), %r8d
	movzbl	48(%r10,%rax), %r9d
	movb	%r9b, (%rdx,%rcx)
	movzbl	272(%r10,%rax), %r9d
	movb	%r9b, 1(%rdx,%rcx)
	movzbl	(%r10,%r8), %eax
	movzbl	192(%r10,%r8), %r9d
	movb	%r9b, 2(%rdx,%rcx)
	movzbl	240(%r10,%r8), %r9d
	movb	%r9b, 3(%rdx,%rcx)
	addq	$512, %r10
	btrq	$11, %r10
	addq	$4, %rcx
	cmpq	%rsi, %rcx
	jb	.Ljda_g9np_avx2_o42_E
.Ljda_g9np_avx2_o42_done:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
.Ljda_g9np_avx2_o42_ret:
	retq
    .size jda_asm_g9np_avx2_o42, .-jda_asm_g9np_avx2_o42
    .section .note.GNU-stack,"",@progbits
#endif
