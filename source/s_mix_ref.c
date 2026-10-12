//
// Sound mixer inner loops in C: the reference for s_mix_gba.s, which the GBA
// builds instead, and the PC build's implementation. The two must give bit
// identical results. Descriptions are in s_mix.h.
//

#ifndef GBA

#include "s_mix.h"

// Accumulator for one frame.
static int32_t mixbuf[SND_MIX_SAMPLES];

int32_t* S_MixBuffer(void)
{
    return mixbuf;
}

// The nearest sample, not interpolated: through the 8 bit output it sounds
// no different from linear interpolation, at about half the cost.
int32_t S_MixResample(int32_t* buf, const int8_t* src, int32_t ofs, int32_t incr, int32_t count, int32_t amp,
                      int32_t loop_end, int32_t loop_len)
{
    // Bias ofs (and loop_end) by half a sample so that the >> rounds.
    const int32_t half = 1 << (SND_MIX_FRACBITS - 1);

    ofs += half;

    if(loop_end != INT32_MAX)
        loop_end += half;

    amp <<= 8;

    while(count--)
    {
        *buf++ += src[ofs >> SND_MIX_FRACBITS] * amp;

        ofs += incr;

        if(ofs >= loop_end)
            ofs -= loop_len;
    }

    return ofs - half;
}

void S_MixDirect(int32_t* buf, const int8_t* src, int32_t count, int32_t amp)
{
    amp <<= 8;

    while(count--)
        *buf++ += *src++ * amp;
}

void S_MixOutput(int8_t* out, int32_t* buf, int32_t count)
{
    while(count--)
    {
        int32_t v = *buf >> 21;

        *buf++ = 0;

        if(v > 127)
            v = 127;
        else if(v < -128)
            v = -128;

        *out++ = (int8_t)v;
    }
}

#endif // GBA
