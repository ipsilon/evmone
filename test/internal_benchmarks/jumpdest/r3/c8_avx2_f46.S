// c8_avx2_ofix core (13 shuffle-port ops) with the loop-carried chain load made forwardable on Zen 3: PTn and PT_hi are
// written as four qword stores (vmovq/vmovhps, store-data path only), the chain loads the qword that starts at
// 8*(e/8) and extracts byte e%8 with shrx, so the load always starts where a store starts (Zen 3 forwards only then).
// VA/VB are plain ymm stores. No ring, no extra per-call setup.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Lc8_avx2_f46_LCPI4_0:
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
.Lc8_avx2_f46_LCPI4_1:
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
.Lc8_avx2_f46_LCPI4_10:
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
.Lc8_avx2_f46_LCPI4_11:
	.zero	16,240
    .p2align 5
.Lc8_avx2_f46_LCPI4_12:
	.byte	95
    .p2align 5
.Lc8_avx2_f46_LCPI4_13:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Lc8_avx2_f46_LCPI4_14:
	.byte	91
    .p2align 5
.Lc8_avx2_f46_LCPI4_15:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Lc8_avx2_f46_LCPI4_2:
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
.Lc8_avx2_f46_LCPI4_3:
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
.Lc8_avx2_f46_LCPI4_6:
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
.Lc8_avx2_f46_LCPI4_7:
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
    .globl jda_asm_c8_avx2_f46
    .hidden jda_asm_c8_avx2_f46
    .type jda_asm_c8_avx2_f46,@function
jda_asm_c8_avx2_f46:
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
	vmovaps	.Lc8_avx2_f46_LCPI4_0(%rip), %xmm0
	vmovaps	%xmm0, 16(%rsp)
	movabsq	$-7523661662112280176, %rax
	movq	%rax, 32(%rsp)
	movabsq	$-8681104427521506944, %rax
	movq	%rax, 96(%rsp)
	vmovaps	.Lc8_avx2_f46_LCPI4_1(%rip), %xmm0
	vmovaps	%xmm0, 336(%rsp)
	vmovaps	.Lc8_avx2_f46_LCPI4_2(%rip), %xmm0
	vmovaps	%xmm0, 352(%rsp)
	vmovdqa	.Lc8_avx2_f46_LCPI4_3(%rip), %xmm0
	vmovdqa	%xmm0, 368(%rsp)
	addq	$31, %rsi
	cmpq	$32, %rsi
	jb	.Lc8_avx2_f46_BB4_3
	shrq	$5, %rsi
	vmovdqa	(%rdi), %ymm8
	shlq	$2, %rsi
	movl	$128, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Lc8_avx2_f46_LCPI4_12(%rip), %ymm0
	vpbroadcastq	.Lc8_avx2_f46_LCPI4_13(%rip), %ymm1
	vmovdqa	.Lc8_avx2_f46_LCPI4_6(%rip), %ymm2
	vmovdqa	.Lc8_avx2_f46_LCPI4_7(%rip), %ymm3
	vpbroadcastb	.Lc8_avx2_f46_LCPI4_14(%rip), %ymm4
	vpbroadcastq	.Lc8_avx2_f46_LCPI4_15(%rip), %ymm5
	vmovdqa	.Lc8_avx2_f46_LCPI4_10(%rip), %ymm6
	vmovdqa	.Lc8_avx2_f46_LCPI4_11(%rip), %xmm7
	.p2align	4
.Lc8_avx2_f46_BB4_2:
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
	vextracti128	$1, %ymm10, %xmm12
	vmovq	%xmm12, 80(%rsp)
	vmovhps	%xmm12, 88(%rsp)
	vpaddb	%xmm7, %xmm10, %xmm10
	vpshufb	%xmm10, %xmm12, %xmm12
	vpmaxub	%xmm10, %xmm12, %xmm10
	vmovq	%xmm10, 64(%rsp)
	vmovhps	%xmm10, 72(%rsp)
	vmovdqa	%ymm11, 128(%rsp)
	vmovdqa	%ymm9, 160(%rsp)
	movzbl	-128(%rsp,%rax), %r8d
	movzbl	208(%rsp,%rax), %r9d
	leal	(,%rax,8), %r10d
	andl	$-8, %eax
	movq	-64(%rsp,%rax), %rax
	shrxq	%r10, %rax, %rax
	movzbl	%al, %eax
	movzwl	-128(%rsp,%r9,2), %r9d
	movw	%r9w, (%rdx,%rcx)
	movzbl	208(%rsp,%r8), %r8d
	movzwl	-112(%rsp,%r8,2), %r8d
	movw	%r8w, 2(%rdx,%rcx)
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Lc8_avx2_f46_BB4_2
.Lc8_avx2_f46_BB4_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_c8_avx2_f46, .-jda_asm_c8_avx2_f46
    .section .note.GNU-stack,"",@progbits
#endif
