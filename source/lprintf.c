/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  Provides a logical console output routine that allows what is
 *  output to console normally and when output is redirected to
 *  be controlled..
 *
 *-----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stddef.h>

#ifdef GBA
    #include <unistd.h>
#else
    #include <stdio.h>
#endif

#include "lprintf.h"

static void L_Write(const char* s, unsigned int len)
{
#ifdef GBA
    //Straight to libgba's console devoptab, no stdio.
    write(1, s, len);
#else
    fwrite(s, 1, len, stdout);
#endif
}


//Where L_Format writes: the console if buf is NULL, else buf up to end.
typedef struct
{
    char* buf;
    char* end;
} lout_t;

static void L_Out(lout_t* out, const char* s, unsigned int len)
{
    if(!out->buf)
    {
        L_Write(s, len);
        return;
    }

    while(len && out->buf < out->end)
    {
        *out->buf++ = *s++;
        len--;
    }
}

//
// A minimal printf. Supports %s, %.Ns, %d, %.Nd (zero padded to N digits)
// and %%. No malloc and little stack, so I_Error is safe deep in the stack
// or out of memory. Console output goes out piece by piece, with no buffer.
//
static void L_Format(lout_t* out, const char* fmt, va_list v)
{
    while(*fmt)
    {
        const char* run = fmt;

        while(*fmt && *fmt != '%')
            fmt++;

        if(fmt != run)
            L_Out(out, run, fmt - run);

        if(!*fmt)
            break;

        const char* spec = fmt++;

        int prec = -1;

        if(*fmt == '.')
        {
            prec = 0;

            for(fmt++; *fmt >= '0' && *fmt <= '9'; fmt++)
                prec = prec * 10 + (*fmt - '0');
        }

        if(*fmt == 's')
        {
            const char* s = va_arg(v, const char*);
            int len = 0;

            if(!s)
                s = "(null)";

            while((prec < 0 || len < prec) && s[len])
                len++;

            L_Out(out, s, len);
        }
        else if(*fmt == 'd')
        {
            int n = va_arg(v, int);
            unsigned int u = (n < 0) ? 0u - (unsigned int)n : (unsigned int)n;

            //10 digits, a sign, and room to pad to 11.
            char buf[12];
            char* end = buf + sizeof(buf);
            char* p = end;

            do
            {
                *--p = '0' + (u % 10);
                u /= 10;
            } while(u);

            while(end - p < prec && p > buf + 1)
                *--p = '0';

            if(n < 0)
                *--p = '-';

            L_Out(out, p, end - p);
        }
        else if(*fmt == '%')
        {
            L_Out(out, fmt, 1);
        }
        else
        {
            //Unsupported: print it as is.
            if(!*fmt)
                fmt--;

            L_Out(out, spec, fmt + 1 - spec);
        }

        fmt++;
    }
}

void lvprintf(const char* fmt, va_list v)
{
    lout_t out = {NULL, NULL};

    L_Format(&out, fmt, v);
}

int lprintf(const char *s, ...)
{
    va_list v;
    va_start(v,s);

    lvprintf(s, v);

    va_end(v);

    L_Write("\n", 1);

    return 0;
}

int lsnprintf(char* buf, unsigned int size, const char* fmt, ...)
{
    lout_t out = {buf, buf + size - 1};

    va_list v;
    va_start(v, fmt);

    L_Format(&out, fmt, v);

    va_end(v);

    *out.buf = 0;

    return out.buf - buf;
}
