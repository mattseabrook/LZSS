/* Public domain. Okumura LZSS codec for AArch64 using the platform C ABI. */
#ifdef __APPLE__
#define C(name) _##name
#else
#define C(name) name
#endif

#define N 4096
#define F 18
#define START (N - F)
#define RING_MASK (N - 1)

/* Shared layout checked in C/lzss.c. */
#define LEFT 4114
#define RIGHT 12308
#define PARENT 21014
#define MATCH 29208
#define BUF_SIZE 8
#define FLAG_MASK 96
#define RUN 100

.text

.macro enter_codec
    .cfi_startproc
    stp x29, x30, [sp, #-112]!
    .cfi_def_cfa_offset 112
    .cfi_offset x29, -112
    .cfi_offset x30, -104
    mov x29, sp
    stp x19, x20, [sp, #16]
    .cfi_offset x19, -96
    .cfi_offset x20, -88
    stp x21, x22, [sp, #32]
    .cfi_offset x21, -80
    .cfi_offset x22, -72
    stp x23, x24, [sp, #48]
    .cfi_offset x23, -64
    .cfi_offset x24, -56
    stp x25, x26, [sp, #64]
    .cfi_offset x25, -48
    .cfi_offset x26, -40
    stp x27, x28, [sp, #80]
    .cfi_offset x27, -32
    .cfi_offset x28, -24
.endm

.macro leave_codec
    ldp x27, x28, [sp, #80]
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #112
    ret
    .cfi_endproc
.endm

/* x0 = tree. Calculate bases once per tree operation. */
.macro tree_arrays
    mov w9, #LEFT
    add x9, x0, x9
    mov w10, #RIGHT
    add x10, x0, x10
    mov w11, #PARENT
    add x11, x0, x11
    mov w14, #N
.endm

/* w3 = node. Return w5 = parent and x4 = pointer to parent's link. */
.macro find_parent_link
    ldrh w5, [x11, w3, uxtw #1]
    add x4, x10, w5, uxtw #1
    ldrh w13, [x4]
    cmp w13, w3
    b.eq .Llink_found\@
    add x4, x9, w5, uxtw #1
.Llink_found\@:
.endm

/* x0 = tree, w1 = node. Traverse the first-byte bucket. */
.p2align 2
.Linsert:
    tree_arrays
    mov w12, #MATCH
    add x12, x0, x12
    add x2, x0, w1, uxtw
    ldrb w3, [x2]
    add w3, w3, #1
    add w3, w3, #N
    strh w14, [x9, w1, uxtw #1]
    strh w14, [x10, w1, uxtw #1]
    str wzr, [x12, #4]
.Linsert_right:
    add x4, x10, w3, uxtw #1
    b .Linsert_visit
.Linsert_left:
    add x4, x9, w3, uxtw #1
.Linsert_visit:
    ldrh w13, [x4]
    cmp w13, w14
    b.eq .Linsert_attach
    mov w3, w13
    add x5, x0, w3, uxtw
    mov w6, #1
.Lcompare:
    ldrb w7, [x2, w6, uxtw]
    ldrb w8, [x5, w6, uxtw]
    subs w7, w7, w8
    b.ne .Lconsider
    add w6, w6, #1
    cmp w6, #F
    b.lo .Lcompare
.Lconsider:
    ldr w13, [x12, #4]
    cmp w6, w13
    b.ls .Linsert_branch
    stp w3, w6, [x12]
    cmp w6, #F
    b.eq .Linsert_replace
.Linsert_branch:
    tbnz w7, #31, .Linsert_left
    b .Linsert_right
.Linsert_attach:
    strh w1, [x4]
    strh w3, [x11, w1, uxtw #1]
    ret
.Linsert_replace:
    find_parent_link
    strh w1, [x4]
    strh w5, [x11, w1, uxtw #1]
    ldrh w13, [x9, w3, uxtw #1]
    strh w13, [x9, w1, uxtw #1]
    strh w1, [x11, w13, uxtw #1]
    ldrh w13, [x10, w3, uxtw #1]
    strh w13, [x10, w1, uxtw #1]
    strh w1, [x11, w13, uxtw #1]
    strh w14, [x11, w3, uxtw #1]
    ret

/* x0 = tree, w1 = node. Replace a two-child node by its predecessor. */
.p2align 2
.Lerase:
    tree_arrays
    mov w3, w1
    ldrh w5, [x11, w3, uxtw #1]
    cmp w5, w14
    b.eq .Lerase_done
    ldrh w6, [x9, w3, uxtw #1]
    ldrh w8, [x10, w3, uxtw #1]
    cmp w6, w14
    b.ne .Lerase_has_left
    mov w6, w8
    b .Lerase_link
.Lerase_has_left:
    cmp w8, w14
    b.eq .Lerase_link
    mov w7, w6
.Lerase_walk:
    ldrh w13, [x10, w6, uxtw #1]
    cmp w13, w14
    b.eq .Lerase_predecessor
    mov w6, w13
    b .Lerase_walk
.Lerase_predecessor:
    cmp w6, w7
    b.eq .Lerase_right_child
    ldrh w5, [x11, w6, uxtw #1]
    ldrh w13, [x9, w6, uxtw #1]
    strh w13, [x10, w5, uxtw #1]
    strh w5, [x11, w13, uxtw #1]
    strh w7, [x9, w6, uxtw #1]
    strh w6, [x11, w7, uxtw #1]
.Lerase_right_child:
    strh w8, [x10, w6, uxtw #1]
    strh w6, [x11, w8, uxtw #1]
.Lerase_link:
    find_parent_link
    strh w6, [x4]
    strh w5, [x11, w6, uxtw #1]
    strh w14, [x11, w3, uxtw #1]
.Lerase_done:
    ret

/* x19 input, x20 length, x21 output, x22 tree, x23 input cursor,
   x24 last progress position, x25 flag offset, w26 head, w27 tail,
   w28 bytes of lookahead. */
.p2align 2
.globl C(lzss_asm_encode)
C(lzss_asm_encode):
    enter_codec
    mov x19, x0
    mov x20, x1
    mov x21, x2
    mov x22, x3
    cbz x20, .Lencode_success

    mov x0, x22
    mov w1, #32
    mov w2, #START
    bl C(memset)
    add x0, x22, #START
    mov w1, #0
    mov w2, #(2*F - 1)
    bl C(memset)

    mov w9, #PARENT
    add x9, x22, x9
    mov w10, #N
    mov w11, #N
.Linit_parents:
    strh w10, [x9], #2
    subs w11, w11, #1
    b.ne .Linit_parents
    mov w9, #(RIGHT + 2*(N + 1))
    add x9, x22, x9
    mov w11, #256
.Linit_roots:
    strh w10, [x9], #2
    subs w11, w11, #1
    b.ne .Linit_roots

    mov w23, #F
    cmp x20, x23
    csel x23, x20, x23, lo
    mov w28, w23
    add x9, x22, #START
    mov x10, x19
    mov w11, w28
.Lprime:
    ldrb w12, [x10], #1
    strb w12, [x9], #1
    subs w11, w11, #1
    b.ne .Lprime
    mov w25, #1
.Lseed:
    mov x0, x22
    mov w1, #START
    sub w1, w1, w25
    bl .Linsert
    add w25, w25, #1
    cmp w25, #F
    b.ls .Lseed
    mov x0, x22
    mov w1, #START
    bl .Linsert

    mov x24, #0
    mov w26, #0
    mov w27, #START
    mov w9, #1
    str w9, [sp, #FLAG_MASK]
.Lencode_token:
    cbz x24, .Lreport
    sub x9, x23, x24
    cmp x9, #(1 << 20)
    b.lo .Lencode_group
.Lreport:
    mov x0, x23
    mov x1, x20
    bl C(lzss_cli_progress)
    mov x24, x23
.Lencode_group:
    ldr w9, [sp, #FLAG_MASK]
    cmp w9, #1
    b.ne .Lmatch
    mov x0, x21
    mov w1, #17
    bl C(lzss_cli_reserve)
    tbz w0, #0, .Lencode_return
    ldp x9, x25, [x21]
    strb wzr, [x9, x25]
    add x10, x25, #1
    str x10, [x21, #BUF_SIZE]
.Lmatch:
    mov w9, #MATCH
    add x9, x22, x9
    ldp w10, w13, [x9]
    cmp w13, w28
    csel w13, w13, w28, lo
    ldp x9, x11, [x21]
    ldr w12, [sp, #FLAG_MASK]
    cmp w13, #2
    b.hi .Lencode_reference
    mov w13, #1
    ldrb w14, [x9, x25]
    orr w14, w14, w12
    strb w14, [x9, x25]
    ldrb w14, [x22, x27]
    strb w14, [x9, x11]
    add x11, x11, #1
    b .Ladvance
.Lencode_reference:
    add x9, x9, x11
    strb w10, [x9]
    lsr w10, w10, #4
    and w10, w10, #0xf0
    sub w14, w13, #3
    orr w10, w10, w14
    strb w10, [x9, #1]
    add x11, x11, #2
.Ladvance:
    str x11, [x21, #BUF_SIZE]
    str w13, [sp, #RUN]
    lsl w12, w12, #1
    ands w12, w12, #0xff
    mov w9, #1
    csel w12, w12, w9, ne
    str w12, [sp, #FLAG_MASK]
.Lslide:
    mov x0, x22
    mov w1, w26
    bl .Lerase
    cmp x23, x20
    b.hs .Lexhausted
    ldrb w10, [x19, x23]
    add x23, x23, #1
    strb w10, [x22, x26]
    cmp w26, #(F - 1)
    b.hs .Lstep
    add x9, x22, #N
    strb w10, [x9, x26]
    b .Lstep
.Lexhausted:
    sub w28, w28, #1
.Lstep:
    add w26, w26, #1
    and w26, w26, #RING_MASK
    add w27, w27, #1
    and w27, w27, #RING_MASK
    cbz w28, .Lencode_success
    mov x0, x22
    mov w1, w27
    bl .Linsert
    ldr w9, [sp, #RUN]
    subs w9, w9, #1
    str w9, [sp, #RUN]
    b.ne .Lslide
    b .Lencode_token
.Lencode_success:
    mov w0, #1
.Lencode_return:
    leave_codec

/* x19 input, x20 length, x21 output, w22 distance,
   x23 input cursor, w24 flags and group counter, w25 match length. */
.p2align 2
.globl C(lzss_asm_decode)
C(lzss_asm_decode):
    enter_codec
    mov x19, x0
    mov x20, x1
    mov x21, x2
    mov x23, #0
.Ldecode_group:
    cmp x23, x20
    b.hs .Ldecode_success
    ldrb w24, [x19, x23]
    add x23, x23, #1
    orr w24, w24, #0xff00
.Ldecode_token:
    cmp x23, x20
    b.hs .Ldecode_success
    tbz w24, #0, .Ldecode_reference
    mov x0, x21
    mov w1, #1
    bl C(lzss_cli_reserve)
    tbz w0, #0, .Ldecode_return
    ldp x9, x10, [x21]
    ldrb w11, [x19, x23]
    add x23, x23, #1
    strb w11, [x9, x10]
    add x10, x10, #1
    str x10, [x21, #BUF_SIZE]
    b .Lnext_bit
.Ldecode_reference:
    sub x9, x20, x23
    cmp x9, #2
    b.lo .Ldecode_success
    add x9, x19, x23
    ldrb w10, [x9]
    ldrb w11, [x9, #1]
    add x23, x23, #2
    and w25, w11, #0xf
    add w25, w25, #3
    and w11, w11, #0xf0
    orr w10, w10, w11, lsl #4
    ldr w22, [x21, #BUF_SIZE]
    add w22, w22, #(START - 1)
    sub w22, w22, w10
    and w22, w22, #RING_MASK
    add w22, w22, #1
    mov x0, x21
    mov w1, w25
    bl C(lzss_cli_reserve)
    tbz w0, #0, .Ldecode_return

    ldp x9, x10, [x21]
    add x11, x10, x25
    str x11, [x21, #BUF_SIZE]
    cmp x10, x22
    b.hs .Lcopy_start
    mov w14, #32
.Lprefill:
    sub x12, x22, x10
    cmp x12, #START
    csel w13, w14, wzr, ls
    strb w13, [x9, x10]
    add x10, x10, #1
    subs w25, w25, #1
    b.eq .Lnext_bit
    cmp x10, x22
    b.lo .Lprefill
.Lcopy_start:
    add x10, x9, x10
    sub x9, x10, x22
.Lcopy:
    ldrb w13, [x9], #1
    strb w13, [x10], #1
    subs w25, w25, #1
    b.ne .Lcopy
.Lnext_bit:
    lsr w24, w24, #1
    tbnz w24, #8, .Ldecode_token
    b .Ldecode_group
.Ldecode_success:
    mov w0, #1
.Ldecode_return:
    leave_codec

#ifndef __APPLE__
.section .note.GNU-stack,"",%progbits
#endif
