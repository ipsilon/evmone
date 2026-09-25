// g9np_avx2 with the loop-carried entry kept in an xmm byte-broadcast and looked up with vpshufb
// (TA then TB); PT/TA tables and the TB reload are gone, the word tables are stored as before.
#if defined(__x86_64__) && defined(__ELF__)
    .section .rodata
    .p2align 5
.Ljda_g9np_avx2_fnew1_LCPI3_0:
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
.Ljda_g9np_avx2_fnew1_LCPI3_10:
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.byte	32
	.byte	33
    .p2align 5
.Ljda_g9np_avx2_fnew1_LCPI3_11:
	.byte	91
    .p2align 5
.Ljda_g9np_avx2_fnew1_LCPI3_12:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	128
    .p2align 5
.Ljda_g9np_avx2_fnew1_LCPI3_3:
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
.Ljda_g9np_avx2_fnew1_LCPI3_4:
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
.Ljda_g9np_avx2_fnew1_LCPI3_7:
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
.Ljda_g9np_avx2_fnew1_LCPI3_8:
	.zero	16,240
    .p2align 5
.Ljda_g9np_avx2_fnew1_LCPI3_9:
	.byte	95
    .p2align 5
.Ljda_g9np_avx2_fnew1_E0:
	.byte	112
    .p2align 5
    .text
    .p2align 4
    .globl jda_asm_g9np_avx2_fnew1
    .hidden jda_asm_g9np_avx2_fnew1
    .type jda_asm_g9np_avx2_fnew1,@function
jda_asm_g9np_avx2_fnew1:
	pushq	%rbp
	movq	%rsp, %rbp
	andq	$-64, %rsp
	subq	$256, %rsp
	addq	$31, %rsi
	vpxor	%xmm0, %xmm0, %xmm0
	vmovdqa	%ymm0, 128(%rsp)
	vmovdqa	%xmm0, 160(%rsp)
	vmovdqa	%ymm0, 192(%rsp)
	vmovdqa	%ymm0, 224(%rsp)
	cmpq	$32, %rsi
	jb	.Ljda_g9np_avx2_fnew1_BB3_3
	shrq	$5, %rsi
	shlq	$2, %rsi
	movl	$112, %eax
	xorl	%ecx, %ecx
	vpbroadcastb	.Ljda_g9np_avx2_fnew1_LCPI3_9(%rip), %ymm0
	vpbroadcastq	.Ljda_g9np_avx2_fnew1_LCPI3_10(%rip), %ymm1
	vmovdqa	.Ljda_g9np_avx2_fnew1_LCPI3_3(%rip), %ymm2
	vmovdqa	.Ljda_g9np_avx2_fnew1_LCPI3_4(%rip), %ymm3
	vpbroadcastb	.Ljda_g9np_avx2_fnew1_LCPI3_11(%rip), %ymm4
	vpbroadcastq	.Ljda_g9np_avx2_fnew1_LCPI3_12(%rip), %ymm5
	vmovdqa	.Ljda_g9np_avx2_fnew1_LCPI3_7(%rip), %ymm6
	vpbroadcastb	.Ljda_g9np_avx2_fnew1_LCPI3_8(%rip), %ymm7
	vpbroadcastb	.Ljda_g9np_avx2_fnew1_E0(%rip), %xmm15
	.p2align	4
.Ljda_g9np_avx2_fnew1_BB3_2:
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
	vpaddb	%ymm7, %ymm10, %ymm10
	vpor	%ymm8, %ymm11, %ymm8
	vpshufb	%ymm9, %ymm8, %ymm9
	vpunpcklbw	%ymm9, %ymm8, %ymm11
	vpunpckhbw	%ymm8, %ymm9, %ymm8
	vmovdqa	%xmm11, 96(%rsp)
	vmovdqa	%xmm8, 112(%rsp)
	vextracti128	$1, %ymm11, 176(%rsp)
	vextracti128	$1, %ymm8, 192(%rsp)
	vextracti128	$1, %ymm10, %xmm13
	vpshufb	%xmm15, %xmm10, %xmm12
	vpaddb	%xmm7, %xmm15, %xmm14
	vpmaxub	%xmm14, %xmm12, %xmm12
	vpshufb	%xmm12, %xmm13, %xmm13
	vpaddb	%xmm7, %xmm12, %xmm14
	vpmaxub	%xmm14, %xmm13, %xmm15
	vmovd	%xmm12, %r8d
	movzwl	-128(%rsp,%rax,2), %r9d
	movzbl	%r8b, %r8d
	movzwl	-48(%rsp,%r8,2), %r8d
	movw	%r9w, (%rdx,%rcx)
	movw	%r8w, 2(%rdx,%rcx)
	vmovd	%xmm15, %eax
	movzbl	%al, %eax
	addq	$4, %rcx
	cmpq	%rcx, %rsi
	jne	.Ljda_g9np_avx2_fnew1_BB3_2
.Ljda_g9np_avx2_fnew1_BB3_3:
	movq	%rbp, %rsp
	popq	%rbp
	vzeroupper
	retq
    .size jda_asm_g9np_avx2_fnew1, .-jda_asm_g9np_avx2_fnew1
    .section .note.GNU-stack,"",@progbits
#endif
