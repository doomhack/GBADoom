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
#include "r_main.h"
#include "r_draw.h"
#include "m_bbox.h"
#include "w_wad.h"   /* needed for color translation lump lookup */
#include "v_video.h"
#include "i_video.h"
#include "lprintf.h"

#include "global_data.h"
#include "gba_functions.h"

//
// V_CopyRect
//
// Copies a source rectangle in a screen buffer to a destination
// rectangle in another screen buffer. Source origin in srcx,srcy,
// destination origin in destx,desty, common size in width and height.
// Source buffer specfified by srcscrn, destination buffer by destscrn.
//
// Marks the destination rectangle on the screen dirty.
//
// No return.
//
static void V_CopyRect(int srcx, int srcy, int srcscrn, int width,
                int height, int destx, int desty, int destscrn,
                enum patch_translation_e flags)
{
  unsigned short *src;
  unsigned short *dest;

  if (flags & VPT_STRETCH)
  {
    srcx=srcx*SCREENWIDTH/320;
    srcy=srcy*SCREENHEIGHT/200;
    width=width*SCREENWIDTH/320;
    height=height*SCREENHEIGHT/200;
    destx=destx*SCREENWIDTH/320;
    desty=desty*SCREENHEIGHT/200;
  }

  src = _g->screens[srcscrn].data+SCREENPITCH*srcy+srcx;
  dest = _g->screens[destscrn].data+SCREENPITCH*desty+destx;

  for ( ; height>0 ; height--)
    {
      memcpy (dest, src, width*2);
      src += SCREENPITCH;
      dest += SCREENPITCH;
    }
}

/*
 * V_DrawBackground tiles a 64x64 patch over the entire screen, providing the
 * background for the Help and Setup screens, and plot text betwen levels.
 * cphipps - used to have M_DrawBackground, but that was used the framebuffer
 * directly, so this is my code from the equivalent function in f_finale.c
 */
void V_DrawBackground(const char* flatname, int scrn)
{
    /* erase the entire screen to a tiled background */
    const byte *src;
    int         x,y;
    int         width,height;
    int         lump;

    unsigned short *dest = _g->screens[scrn].data;

    // killough 4/17/98:
    src = W_CacheLumpNum(lump = _g->firstflat + R_FlatNumForName(flatname));

    /* V_DrawBlock(0, 0, scrn, 64, 64, src, 0); */
    width = height = 64;



    while (height--)
    {
        memcpy (dest, src, width * 2);
        src += width;
        dest += SCREENPITCH;
    }

    for (y=0 ; y<SCREENHEIGHT ; y+=64)
        for (x=y ? 0 : 64; x<SCREENWIDTH ; x+=64)
            V_CopyRect(0, 0, scrn, ((SCREENWIDTH-x) < 64) ? (SCREENWIDTH-x) : 64,
                       ((SCREENHEIGHT-y) < 64) ? (SCREENHEIGHT-y) : 64, x, y, scrn, VPT_NONE);
}

void V_DrawPatchNoScale(int x, int y, const patch_t* patch)
{
    y -= patch->topoffset;
    x -= patch->leftoffset;

    byte* desttop = (byte*)_g->screens[0].data;
    desttop += (ScreenYToOffset(y) << 1) + x;

    for (int col = 0; col < patch->width; x++, col++, desttop++)
    {
        const column_t* column = (const column_t*)((const byte*)patch + patch->columnofs[col]);

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            const byte* source = (const byte*)column + 3;
            byte* dest = desttop + (ScreenYToOffset(column->topdelta) << 1);
            unsigned int count = column->length;

            while (count--)
            {
                unsigned short color = *source++;

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

                    *dest16 = ((color & 0xff) | (old << 8));
                }

                dest += 240;
            }

            column = (const column_t*)((const byte*)column + column->length + 4);
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

    const int   DX  = (240<<16) / 320;
    const int   DXI = (320<<16) / 240;
    const int   DY  = ((SCREENHEIGHT<<16)+(FRACUNIT-1)) / 200;
    const int   DYI = (200<<16) / SCREENHEIGHT;

    byte* byte_topleft = (byte*)_g->screens[scrn].data;
    const int byte_pitch = (SCREENPITCH * 2);

    const int left = ( x * DX ) >> FRACBITS;
    const int right =  ((x + patch->width) *  DX) >> FRACBITS;
    const int bottom = ((y + patch->height) * DY) >> FRACBITS;

    int   col = 0;

    for (int dc_x=left; dc_x<right; dc_x++, col+=DXI)
    {
        int colindex = (col>>16);

        //Messy hack to compensate accumulated rounding errors.
        if(dc_x == right-1)
        {
            colindex = patch->width-1;
        }

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
                //Messy hack to compensate accumulated rounding errors.
                if(count == 0)
                    frac = (column->length-1) << FRACBITS;


                unsigned short color = source[(frac>>FRACBITS)&127];

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

                    *dest16 = ((color & 0xff) | (old << 8));
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

void V_DrawNumPatch(int x, int y, int scrn, int lump,
         int cm, enum patch_translation_e flags)
{
    V_DrawPatch(x, y, scrn, W_CacheLumpNum(lump));
}

//
// V_SetPalette
//
// CPhipps - New function to set the palette to palette number pal.
// Handles loading of PLAYPAL and calls I_SetPalette

void V_SetPalette(int pal)
{
	I_SetPalette(pal);
}

//
// V_FillRect
//
// CPhipps - New function to fill a rectangle with a given colour
void V_FillRect(int scrn, int x, int y, int width, int height, byte colour)
{
    unsigned short* dest = _g->screens[scrn].data + x + y*SCREENPITCH;
    while (height--)
    {
        memset(dest, colour, width * 2);
        dest += SCREENPITCH;
    }
}



void V_PlotPixel(int scrn, int x, int y, byte color)
{
    _g->screens[scrn].data[ScreenYToOffset(y)+x] = (color | (color << 8));
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
  register int x;
  register int y;
  register int dx;
  register int dy;
  register int sx;
  register int sy;
  register int ax;
  register int ay;
  register int d;

#define PUTDOT(xx,yy,cc) V_PlotPixel(0,xx,yy,(byte)cc)

  dx = fl->b.x - fl->a.x;
  ax = 2 * (dx<0 ? -dx : dx);
  sx = dx<0 ? -1 : 1;

  dy = fl->b.y - fl->a.y;
  ay = 2 * (dy<0 ? -dy : dy);
  sy = dy<0 ? -1 : 1;

  x = fl->a.x;
  y = fl->a.y;

  if (ax > ay)
  {
    d = ay - ax/2;
    while (1)
    {
      PUTDOT(x,y,color);
      if (x == fl->b.x) return;
      if (d>=0)
      {
        y += sy;
        d -= ax;
      }
      x += sx;
      d += ay;
    }
  }
  else
  {
    d = ax - ay/2;
    while (1)
    {
      PUTDOT(x, y, color);
      if (y == fl->b.y) return;
      if (d >= 0)
      {
        x += sx;
        d -= ay;
      }
      y += sy;
      d += ax;
    }
  }
}
