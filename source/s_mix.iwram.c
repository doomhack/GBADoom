//
// Sound mixer inner loops. In IWRAM and ARM code on the GBA (see the Makefile's
// %.iwram.o rules) since they run for every voice for every output sample.
//

#include "s_mix.h"

#define FRAC_MASK ((1 << SND_MIX_FRACBITS) - 1)

// Accumulator for one frame. In IWRAM (.bss).
static int32_t mixbuf[SND_MIX_SAMPLES];

int32_t* S_MixBuffer(void)
{
    return mixbuf;
}

// Linearly interpolated sample at ofs, as a 16 bit value.
static inline int32_t S_MixInterp(const int8_t* src, int32_t ofs)
{
    const int8_t* s = &src[ofs >> SND_MIX_FRACBITS];

    int32_t v1 = s[0];
    int32_t v2 = s[1];

    return (v1 << 8) + (((v2 - v1) * (ofs & FRAC_MASK)) >> (SND_MIX_FRACBITS - 8));
}

int32_t S_MixResample(int32_t* buf, const int8_t* src, int32_t ofs, int32_t incr, int32_t count, int32_t amp,
                      int32_t loop_end, int32_t loop_len)
{
#ifdef GBA
    // An odd sample here, then pairs in ARM asm below.
    int32_t single = count & 1;

    count >>= 1;
#else
    int32_t single = count;
#endif

    while(single--)
    {
        *buf++ += S_MixInterp(src, ofs) * amp;

        ofs += incr;

        if(ofs >= loop_end)
            ofs -= loop_len;
    }

#ifdef GBA
    // Two samples per pass with the accumulator pair in ldm/stm. Same
    // arithmetic as S_MixInterp(). In asm because GCC spills the pair.
    static_assert(SND_MIX_FRACBITS == 12, "The shifts below assume 12 fraction bits");

    if(count)
    {
        int32_t i, v1, v2;

        asm volatile(
            "1:                                 \n"
            "   ldmia   %[buf], {r4, r5}        \n"
            "   mov     %[i], %[ofs], asr #12   \n"
            "   add     %[v2], %[src], %[i]     \n"
            "   ldrsb   %[v1], [%[v2]]          \n"
            "   ldrsb   %[v2], [%[v2], #1]      \n"
            "   sub     %[i], %[ofs], %[i], lsl #12 \n"
            "   sub     %[v2], %[v2], %[v1]     \n"
            "   mul     %[v2], %[i], %[v2]      \n"
            "   mov     %[v1], %[v1], lsl #8    \n"
            "   add     %[v1], %[v1], %[v2], asr #4 \n"
            "   mla     r4, %[v1], %[amp], r4   \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   mov     %[i], %[ofs], asr #12   \n"
            "   add     %[v2], %[src], %[i]     \n"
            "   ldrsb   %[v1], [%[v2]]          \n"
            "   ldrsb   %[v2], [%[v2], #1]      \n"
            "   sub     %[i], %[ofs], %[i], lsl #12 \n"
            "   sub     %[v2], %[v2], %[v1]     \n"
            "   mul     %[v2], %[i], %[v2]      \n"
            "   mov     %[v1], %[v1], lsl #8    \n"
            "   add     %[v1], %[v1], %[v2], asr #4 \n"
            "   mla     r5, %[v1], %[amp], r5   \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   stmia   %[buf]!, {r4, r5}       \n"
            "   subs    %[count], %[count], #1  \n"
            "   bne     1b                      \n"
            : [buf] "+r" (buf), [ofs] "+r" (ofs), [count] "+r" (count),
              [i] "=&r" (i), [v1] "=&r" (v1), [v2] "=&r" (v2)
            : [src] "r" (src), [incr] "r" (incr), [amp] "r" (amp), [end] "r" (loop_end), [len] "r" (loop_len)
            : "r4", "r5", "cc", "memory");
    }
#endif

    return ofs;
}

void S_MixDirect(int32_t* buf, const int8_t* src, int32_t count, int32_t amp)
{
    amp <<= 8;

    while(count & 3)
    {
        *buf++ += *src++ * amp;
        count--;
    }

    // Four at a time: less loop overhead, and GCC reads the accumulator with ldm.
    while(count)
    {
        int32_t b0 = buf[0], b1 = buf[1], b2 = buf[2], b3 = buf[3];

        b0 += src[0] * amp;
        b1 += src[1] * amp;
        b2 += src[2] * amp;
        b3 += src[3] * amp;

        buf[0] = b0; buf[1] = b1; buf[2] = b2; buf[3] = b3;

        buf += 4;
        src += 4;
        count -= 4;
    }
}

static inline uint32_t S_MixClip(int32_t v)
{
    v >>= 21;

    if(v > 127)
        v = 127;
    else if(v < -128)
        v = -128;

    return v & 0xFF;
}

void S_MixOutput(int8_t* out, int32_t* buf, int32_t count)
{
    uint32_t* o = (uint32_t*)out;

    // Four samples per word store: out is in EWRAM, where byte stores cost as much as word stores.
    do
    {
        *o++ = S_MixClip(buf[0]) | (S_MixClip(buf[1]) << 8) | (S_MixClip(buf[2]) << 16) | (S_MixClip(buf[3]) << 24);

        buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0;
        buf += 4;
    } while(count -= 4);
}
