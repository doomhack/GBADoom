.section .iwram
.arm
.align

.global udiv64_arm

udiv64_arm:

/*
    Tweaked version of 64/32 division found in
    section 7.3.1.3 of
    ARM System Developer’s Guide
    Designing and Optimizing System Software

    ISBN: 1-55860-874-5

    r0 = numerator high, return quotient
    r1 = numerator low
    r2 = denominator
    r3 = scratch
*/

    cmp r0, r2
    bcs .overflow_32
    rsb r2, r2, #0
    adds r3, r1, r1
    adcs r1, r2, r0, LSL#1
    subcc r1, r1, r2

    .rept 31
        adcs r3, r3, r3
        adcs r1, r2, r1, LSL#1
        subcc r1,r1, r2
    .endr

    adcs r0, r3, r3
    bx lr

    .overflow_32:
    mov r0, #-1
    bx lr
