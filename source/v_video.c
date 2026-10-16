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
 *  Gamma correction LUT stuff.
 *  Color range translation support
 *  Functions to draw patches (by post) directly to screen.
 *  Functions to blit a block to the screen.
 *
 *-----------------------------------------------------------------------------
 */

#include "doomdef.h"
#include "w_wad.h"   /* needed for color translation lump lookup */
#include "v_video.h"
#include "i_video.h"
#include "lprintf.h"

#include "global_data.h"
#include "gba_functions.h"
#include "doom_iwad.h"

/*
 * V_DrawBackground tiles a 64x64 patch over the entire screen, providing the
 * background for the Help and Setup screens, and plot text betwen levels.
 * cphipps - used to have M_DrawBackground, but that was used the framebuffer
 * directly, so this is my code from the equivalent function in f_finale.c
 */
void V_DrawBackground(const char* flatname)
{
    /* erase the entire screen to a tiled background */
    const byte *src;
    int         lump;

    unsigned short *dest = _g->screens[0].data;

    // killough 4/17/98:
    src = W_CacheLumpNum(lump = _g->firstflat + R_FlatNumForName(flatname));

    for(unsigned int y = 0; y < SCREENHEIGHT; y++)
    {
        for(unsigned int x = 0; x < 240; x+=64)
        {
            unsigned short* d = &dest[ ScreenYToOffset(y) + (x >> 1)];
            const byte* s = &src[((y&63) * 64) + (x&63)];

            unsigned int len = 64;

            if( (240-x) < 64)
                len = 240-x;

            BlockCopy(d, s, len);
        }
    }
}



/*
 * This function draws at GBA resoulution (ie. not pixel doubled)
 * so the st bar and menus don't look like garbage.
 */

void V_DrawPatch(int x, int y, int scrn, const patch_t* patch)
{
    y -= patch->topoffset;
    x -= patch->leftoffset;

    int   col = 0;

    const int   DX  = (240<<FRACBITS) / 320;
    const int   DXI = (320<<FRACBITS) / 240;
    const int   DY  = ((SCREENHEIGHT<<FRACBITS)+(FRACUNIT-1)) / 200;
    const int   DYI = (200<<FRACBITS) / SCREENHEIGHT;

    byte* byte_topleft = (byte*)_g->screens[scrn].data;
    const int byte_pitch = (SCREENPITCH * 2);

    const int left = ( x * DX ) >> FRACBITS;
    const int right =  ((x + patch->width) *  DX) >> FRACBITS;
    const int bottom = ((y + patch->height) * DY) >> FRACBITS;

    for (int dc_x=left; dc_x<right; dc_x++, col+=DXI)
    {
        int colindex = (col>>FRACBITS);

        if(dc_x < 0)
            continue;

        const column_t* column = (const column_t *)((const byte*)patch + patch->columnofs[colindex]);

        if (dc_x >= 240)
            break;

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            const byte* source = (const byte*)column + 3;
            const int topdelta = column->topdelta;

            int dc_yl = (((y + topdelta) * DY) >> FRACBITS);
            int dc_yh = (((y + topdelta + column->length) * DY) >> FRACBITS);

            if ((dc_yl >= SCREENHEIGHT) || (dc_yl > bottom))
                break;

            int count = (dc_yh - dc_yl);

            byte* dest = byte_topleft + (dc_yl*byte_pitch) + dc_x;

            const fixed_t fracstep = DYI;
            fixed_t frac = 0;

            // Inner loop that does the actual texture mapping,
            //  e.g. a DDA-lile scaling.
            // This is as fast as it gets.
            while (count--)
            {
                unsigned short color = source[frac >> FRACBITS];

                //The GBA must write in 16bits.
                if((unsigned int)dest & 1)
                {
                    //Odd addreses, we combine existing pixel with new one.
                    unsigned short* dest16 = (unsigned short*)(dest - 1);


                    unsigned short old = *dest16;

                    *dest16 = (old & 0xff) | (color << 8);
                }
                else
                {
                    unsigned short* dest16 = (unsigned short*)dest;

                    unsigned short old = *dest16;

                    *dest16 = ((color & 0xff) | (old & 0xff00));
                }

                dest += byte_pitch;
                frac += fracstep;
            }

            column = (const column_t *)((const byte *)column + column->length + 4 );
        }
    }
}


// CPhipps - some simple, useful wrappers for that function, for drawing patches from wads

// CPhipps - GNU C only suppresses generating a copy of a function if it is
// static inline; other compilers have different behaviour.
// This inline is _only_ for the function below

void V_DrawNumPatch(int x, int y, int scrn, int lump)
{
    V_DrawPatch(x, y, scrn, W_CacheLumpNum(lump));
}

//
// V_DrawRawStrip
//
// One DMA per row: a whole screen in one would hold off the VBlank
// interrupt that restarts the sound DMA for longer than mix_out's padding.
//
void V_DrawRawStrip(const byte* pic, int width, int x)
{
    unsigned short* dest = _g->screens[0].data;
    const byte* src = pic + x;

    for(int y = 0; y < RAWPAGE_HEIGHT; y++, src += width, dest += SCREENPITCH)
    {
        if(!(x & 1))
        {
            BlockCopy16(dest, src, RAWPAGE_WIDTH);
        }
        else
        {
            //The screen takes 16 bit writes and the row starts mid halfword,
            //so put each pair of pixels together from the halfwords either side.
            const unsigned short* s = (const unsigned short*)(src - 1);
            unsigned int prev = *s++;

            for(int i = 0; i < RAWPAGE_WIDTH / 2; i++)
            {
                const unsigned int next = *s++;

                dest[i] = (prev >> 8) | (next << 8);
                prev = next;
            }
        }
    }
}

void V_DrawRawPage(int lump)
{
    if(W_LumpLength(lump) != RAWPAGE_WIDTH * RAWPAGE_HEIGHT)
        I_Error("V_DrawRawPage: %.8s is not a GbaWadUtil page", W_GetNameForNum(lump));

    V_DrawRawStrip(W_CacheLumpNum(lump), RAWPAGE_WIDTH, 0);
}

//
// V_SetPalette
//
// CPhipps - New function to set the palette to palette number pal.
// Calls I_SetPalette with a palette of the current gamma level's GBAPAL

void V_SetPalette(int pal)
{
	I_SetPalette(pal);
}

//Select the GBAPAL palettes for a gamma level (Doom's gammatable,
//applied by GbaWadUtil).
void V_SetPalLump(int index)
{
    if(index < 0)
        index = 0;
    else if(index > GBAPAL_GAMMA_LEVELS-1)
        index = GBAPAL_GAMMA_LEVELS-1;

    const unsigned short* gbapal = (const unsigned short*)W_CacheLumpName("GBAPAL");

    _g->pallete_lump = gbapal + index * GBAPAL_PALETTES * GBAPAL_COLOURS;
}

//
// V_FillRect
//
// CPhipps - New function to fill a rectangle with a given colour
void V_FillRect(int x, int y, int width, int height, byte colour)
{
    byte* fb = (byte*)_g->screens[0].data;

    byte* dest = &fb[(ScreenYToOffset(y) << 1) + x];

    while (height--)
    {
        BlockSet(dest, colour, width);
        dest += (SCREENPITCH << 1);
    }
}

static inline void V_PlotPixel(int x, int y, int color)
{
    byte* fb = (byte*)_g->screens[0].data;

    // Compute the byte pointer for the pixel
    byte* dest = &fb[(ScreenYToOffset(y) << 1) + x];

    // Use 16-bit aligned pointer for read-modify-write
    unsigned short* dest16 = (unsigned short*)((uintptr_t)dest & ~1);
    unsigned short old = *dest16;

    if ((uintptr_t)dest & 1)
        *dest16 = (old & 0x00FF) | ((color & 0xFF) << 8);
    else
        *dest16 = (old & 0xFF00) | (color & 0xFF);
}

//
// WRAP_V_DrawLine()
//
// Draw a line in the frame buffer.
// Classic Bresenham w/ whatever optimizations needed for speed
//
// Passed the frame coordinates of line, and the color to be drawn
// Returns nothing
//
void V_DrawLine(fline_t* fl, int color)
{
    int x0 = fl->a.x;
    int x1 = fl->b.x;

    int y0 = fl->a.y;
    int y1 = fl->b.y;

    int dx =  D_abs(x1-x0);
    int sx = x0<x1 ? 1 : -1;

    int dy = -D_abs(y1-y0);
    int sy = y0<y1 ? 1 : -1;

    int err = dx + dy;

    while(true)
    {
        V_PlotPixel(x0, y0, color);

        if (x0==x1 && y0==y1)
            break;

        int e2 = 2*err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}
