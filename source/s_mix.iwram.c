//
// Sound mixer inner loops. In IWRAM and ARM code on the GBA (see the Makefile's
// %.iwram.o rules) since they run for every voice for every output sample.
//

#include "code_opt.h"
ARM_CODE_DEFAULT

#include "s_mix.h"

// Accumulator for one frame. In IWRAM (.bss).
static int32_t mixbuf[SND_MIX_SAMPLES];

int32_t* S_MixBuffer(void)
{
    return mixbuf;
}

// The nearest sample, not interpolated: through the 8 bit output it sounds
// no different from linear interpolation, at about half the cost.
HOT_CODE int32_t S_MixResample(int32_t* buf, const int8_t* src, int32_t ofs, int32_t incr, int32_t count, int32_t amp,
                      int32_t loop_end, int32_t loop_len)
{
    // Bias ofs (and loop_end) by half a sample so that the >> rounds.
    const int32_t half = 1 << (SND_MIX_FRACBITS - 1);

    ofs += half;

    if(loop_end != INT32_MAX)
        loop_end += half;

    amp <<= 8;

#ifdef GBA
    // Up to three samples here, then four at a time in ARM asm below.
    int32_t single = count & 3;

    count >>= 2;
#else
    int32_t single = count;
#endif

    while(single--)
    {
        *buf++ += src[ofs >> SND_MIX_FRACBITS] * amp;

        ofs += incr;

        if(ofs >= loop_end)
            ofs -= loop_len;
    }

#ifdef GBA
    // Four samples per pass with the accumulators in ldm/stm. Same arithmetic
    // as above. In asm because GCC doesn't use stm here. amp is the mla's
    // first operand: the multiplier (the sample) sets how long it takes.
    static_assert(SND_MIX_FRACBITS == 12, "The shifts below assume 12 fraction bits");

    if(count)
    {
        int32_t s;

        asm volatile(
            "1:                                 \n"
            "   ldmia   %[buf], {r4-r7}         \n"
            "   mov     %[s], %[ofs], asr #12   \n"
            "   ldrsb   %[s], [%[src], %[s]]    \n"
            "   mla     r4, %[amp], %[s], r4    \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   mov     %[s], %[ofs], asr #12   \n"
            "   ldrsb   %[s], [%[src], %[s]]    \n"
            "   mla     r5, %[amp], %[s], r5    \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   mov     %[s], %[ofs], asr #12   \n"
            "   ldrsb   %[s], [%[src], %[s]]    \n"
            "   mla     r6, %[amp], %[s], r6    \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   mov     %[s], %[ofs], asr #12   \n"
            "   ldrsb   %[s], [%[src], %[s]]    \n"
            "   mla     r7, %[amp], %[s], r7    \n"
            "   add     %[ofs], %[ofs], %[incr] \n"
            "   cmp     %[ofs], %[end]          \n"
            "   subge   %[ofs], %[ofs], %[len]  \n"
            "   stmia   %[buf]!, {r4-r7}        \n"
            "   subs    %[count], %[count], #1  \n"
            "   bne     1b                      \n"
            : [buf] "+r" (buf), [ofs] "+r" (ofs), [count] "+r" (count), [s] "=&r" (s)
            : [src] "r" (src), [incr] "r" (incr), [amp] "r" (amp), [end] "r" (loop_end), [len] "r" (loop_len)
            : "r4", "r5", "r6", "r7", "cc", "memory");
    }
#endif

    return ofs - half;
}

HOT_CODE void S_MixDirect(int32_t* buf, const int8_t* src, int32_t count, int32_t amp)
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

static inline __attribute__((always_inline)) uint32_t S_MixClip(int32_t v)
{
    v >>= 21;

    if(v > 127)
        v = 127;
    else if(v < -128)
        v = -128;

    return v & 0xFF;
}

HOT_CODE void S_MixOutput(int8_t* out, int32_t* buf, int32_t count)
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
