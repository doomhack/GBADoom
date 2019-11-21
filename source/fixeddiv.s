//Fast 64 by 32 division.
//Found here: https://stackoverflow.com/questions/15215196/64bit-32bit-division-faster-algorithm-for-arm-neon

//Original source: http://www.peter-teichmann.de/adiv2e.html

.section .iwram
.arm
.align

.global udiv64_arm
.global udiv32_arm


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
