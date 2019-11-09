#ifndef GBA_FUNCTIONS_H
#define GBA_FUNCTIONS_H

#include <string.h>
#include "doomtype.h"
#include "m_fixed.h"

//#define __arm__

#ifdef __arm__
    #include <gba_systemcalls.h>
    #include <gba_dma.h>
#endif

//***********************************************************************
//The following math functions were taken from the Jaguar Port of Doom
//here: https://github.com/Arc0re/jaguardoom/blob/master/jagonly.c
//
//There may be a licence incompatibility with the iD release
//and the GPL that prBoom (and this as derived work) is under.
//***********************************************************************

#ifndef __arm__

static CONSTFUNC unsigned UDiv32 (unsigned aa, unsigned bb)
{
    unsigned        bit;
    unsigned        c;

    if ( (aa>>30) >= bb)
        return 0x7fffffff;

    bit = 1;
    while (aa > bb && bb < 0x80000000)
    {
        bb <<= 1;
        bit <<= 1;
    }

    c = 0;

    do
    {
        if (aa >= bb)
        {
            aa -=bb;
            c |= bit;
        }
        bb >>=1;
        bit >>= 1;
    } while (bit && aa);

    return c;
}

#endif

inline static CONSTFUNC int IDiv32 (int a, int b)
{

    //use bios divide on gba.
#ifdef __arm__
    return Div(a, b);
#else

    unsigned        aa,bb,c;
    int             sign;

    sign = a^b;
    if (a<0)
        aa = -a;
    else
        aa = a;
    if (b<0)
        bb = -b;
    else
        bb = b;

    c = UDiv32(aa,bb);

    if (sign < 0)
        c = -(int)c;
    return c;
#endif
}

inline static void BlockCopy(void* dest, const void* src, const unsigned int len)
{
#ifdef __arm__
    const int words = len >> 2;

    DMA3COPY(src, dest, DMA_DST_INC | DMA_SRC_INC | DMA32 | DMA_IMMEDIATE | words)
#else
    memcpy(dest, src, len);
#endif
}

inline static void BlockSet(void* dest, volatile unsigned int val, const unsigned int len)
{
#ifdef __arm__
    const int words = len >> 2;

    DMA3COPY(&val, dest, DMA_SRC_FIXED | DMA_DST_INC | DMA32 | DMA_IMMEDIATE | words)
#else
    memset(dest, val, len);
#endif
}

inline static void ByteCopy(byte* dest, const byte* src, unsigned int count)
{
    do
    {
        *dest++ = *src++;
    } while(--count);
}

inline static void ByteSet(byte* dest, byte val, unsigned int count)
{
    do
    {
        *dest++ = val;
    } while(--count);
}

inline static void* ByteFind(byte* mem, byte val, unsigned int count)
{
    do
    {
        if(*mem == val)
            return mem;

        mem++;
    } while(--count);

    return NULL;
}

#endif // GBA_FUNCTIONS_H
