//Fast 64 by 32 division.
//Found here: https://stackoverflow.com/questions/15215196/64bit-32bit-division-faster-algorithm-for-arm-neon

//Original source: http://www.peter-teichmann.de/adiv2e.html

.section .iwram
.arm
.align

.global udiv64_arm

udiv64_arm:
    adds      r0,r0,r0
    adc       r1,r1,r1

    .rept 31
        cmp     r1,r2
        subcs   r1,r1,r2
        adcs    r0,r0,r0
        adc     r1,r1,r1
    .endr

    cmp     r1,r2
    subcs   r1,r1,r2
    adcs    r0,r0,r0

    bx      lr



//Faster u32 division.
//Sourced from here: https://thinkingeek.com/2013/08/11/arm-assembler-raspberry-pi-chapter-15/

.global udiv32_arm

udiv32_arm :
    /* r0 contains N and Ni */
    /* r1 contains D */
    /* r2 contains Q */
    /* r3 will contain Di */

    mov r3, r1                   /* r3 ← r1 */
    cmp r3, r0, LSR #1           /* update cpsr with r3 - r0/2 */
    .Lloop2:
      movls r3, r3, LSL #1       /* if r3 <= 2*r0 (C=0 or Z=1) then r3 ← r3*2 */
      cmp r3, r0, LSR #1         /* update cpsr with r3 - (r0/2) */
      bls .Lloop2                /* branch to .Lloop2 if r3 <= 2*r0 (C=0 or Z=1) */

    mov r2, #0                   /* r2 ← 0 */

    .Lloop3:
      cmp r0, r3                 /* update cpsr with r0 - r3 */
      subhs r0, r0, r3           /* if r0 >= r3 (C=1) then r0 ← r0 - r3 */
      adc r2, r2, r2             /* r2 ← r2 + r2 + C.
                                    Note that if r0 >= r3 then C=1, C=0 otherwise */

      mov r3, r3, LSR #1         /* r3 ← r3/2 */
      cmp r3, r1                 /* update cpsr with r3 - r1 */
      bhs .Lloop3                /* if r3 >= r1 branch to .Lloop3 */

    mov r0, r2                   /* move quotient to r0 return val */
    bx lr
