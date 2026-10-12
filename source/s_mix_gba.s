/*
    Sound mixer inner loops for the GBA, in ARM code in IWRAM since they run
    for every voice for every output sample. s_mix_ref.c has the same functions
    in C (the reference, and the PC build); the two must give bit identical
    results. Prototypes and descriptions are in s_mix.h.

    Built in: SND_MIX_FRACBITS 12 and SND_MIX_SAMPLES 224 (s_mix.c checks them).

    The ARM7's multiply time depends on Rs, so that is the shorter operand:
    in S_MixDirect the sample (8 bits) rather than amp << 8 (21 bits).
*/

    .bss
    .align 2

    /* Accumulator for one frame. */
mixbuf:
    .space  224 * 4

    .section .iwram, "ax", %progbits
    .arm
    .align 2

/*
    int32_t* S_MixBuffer(void)
*/
    .global S_MixBuffer
    .type   S_MixBuffer, %function
S_MixBuffer:
    ldr     r0, =mixbuf
    bx      lr

    .ltorg

/*
    int32_t S_MixResample(int32_t* buf, const int8_t* src, int32_t ofs, int32_t incr,
                          int32_t count, int32_t amp, int32_t loop_end, int32_t loop_len)

    Linear interpolation. Taking the nearest sample is about half the cost and
    mostly sounds the same, but some instruments get audible artifacts.

    An odd sample on its own, then 2 per pass with the accumulators in ldm/stm.
    r6 = src + 1, so both samples load with a register offset.

    r0 buf, r1 src, r2 ofs, r3 incr, r4-r5 accumulators, r6 src + 1, r7 count,
    r8 amp, r9 loop_end, r10 loop_len, r11 index then fraction, r12 s[0], lr s[1]
*/
    .macro  lerp acc
    mov     r11, r2, asr #12
    ldrsb   r12, [r1, r11]
    ldrsb   lr, [r6, r11]
    sub     r11, r2, r11, lsl #12   @ Fraction.
    sub     lr, lr, r12
    mul     lr, r11, lr             @ Rs is s[1] - s[0], 9 bits: 1 cycle.
    mov     r12, r12, lsl #8
    add     r12, r12, lr, asr #4    @ Interpolated sample, 16 bits.
    mla     \acc, r12, r8, \acc
    add     r2, r2, r3
    cmp     r2, r9
    subge   r2, r2, r10
    .endm

    .global S_MixResample
    .type   S_MixResample, %function
S_MixResample:
    push    {r4-r11, lr}
    add     r12, sp, #36
    ldmia   r12, {r7-r10}           @ count, amp, loop_end, loop_len
    add     r6, r1, #1

    tst     r7, #1
    beq     2f
    ldr     r4, [r0]
    lerp    r4
    str     r4, [r0], #4
2:
    movs    r7, r7, asr #1
    beq     4f
3:
    ldmia   r0, {r4, r5}
    lerp    r4
    lerp    r5
    stmia   r0!, {r4, r5}
    subs    r7, r7, #1
    bne     3b
4:
    mov     r0, r2
    pop     {r4-r11, lr}
    bx      lr

/*
    void S_MixDirect(int32_t* buf, const int8_t* src, int32_t count, int32_t amp)

    One sample at a time until src is word aligned, then 4 per word load (a
    word from ROM costs 7 cycles, 4 bytes 20), then the rest one at a time.

    r0 buf, r1 src, r2 count, r3 amp << 8, r4-r7 accumulators, r8 words,
    r12 4 samples, lr sample
*/
    .macro  mix_one
    ldrsb   lr, [r1], #1
    ldr     r12, [r0]
    mla     r12, r3, lr, r12
    str     r12, [r0], #4
    .endm

    .global S_MixDirect
    .type   S_MixDirect, %function
S_MixDirect:
    push    {r4-r8, lr}
    mov     r3, r3, lsl #8
1:
    tst     r1, #3
    beq     2f
    subs    r2, r2, #1
    bmi     5f
    mix_one
    b       1b
2:
    movs    r8, r2, asr #2
    and     r2, r2, #3
    beq     4f
3:
    ldr     r12, [r1], #4
    ldmia   r0, {r4-r7}
    mov     lr, r12, lsl #24
    mov     lr, lr, asr #24
    mla     r4, r3, lr, r4
    mov     lr, r12, lsl #16
    mov     lr, lr, asr #24
    mla     r5, r3, lr, r5
    mov     lr, r12, lsl #8
    mov     lr, lr, asr #24
    mla     r6, r3, lr, r6
    mov     lr, r12, asr #24
    mla     r7, r3, lr, r7
    stmia   r0!, {r4-r7}
    subs    r8, r8, #1
    bne     3b
4:
    subs    r2, r2, #1
    bmi     5f
    mix_one
    b       4b
5:
    pop     {r4-r8, lr}
    bx      lr

/*
    void S_MixOutput(int8_t* out, int32_t* buf, int32_t count)

    buf >> 21 clipped to signed 8 bit, 4 samples per word store (out is in
    EWRAM, where a byte store costs as much as a word store), and clears buf.
    out must be word aligned and count a multiple of 4.

    r0 out, r1 buf, r2 count, r3 4 samples, r4-r7 samples, r8-r11 zero, r12 byte
*/
    .macro  clip8 reg
    mov     \reg, \reg, asr #21
    cmp     \reg, #127
    movgt   \reg, #127
    cmn     \reg, #128
    mvnlt   \reg, #127              @ -128
    .endm

    .global S_MixOutput
    .type   S_MixOutput, %function
S_MixOutput:
    push    {r4-r11}
    mov     r8, #0
    mov     r9, #0
    mov     r10, #0
    mov     r11, #0
1:
    ldmia   r1, {r4-r7}
    stmia   r1!, {r8-r11}
    clip8   r4
    clip8   r5
    clip8   r6
    clip8   r7
    and     r3, r4, #0xFF
    and     r12, r5, #0xFF
    orr     r3, r3, r12, lsl #8
    and     r12, r6, #0xFF
    orr     r3, r3, r12, lsl #16
    orr     r3, r3, r7, lsl #24
    str     r3, [r0], #4
    subs    r2, r2, #4
    bne     1b
    pop     {r4-r11}
    bx      lr
