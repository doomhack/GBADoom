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


// killough 5/2/98: tiny engine driven by table above
void V_InitColorTranslation(void)
{

}

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
void V_CopyRect(int srcx, int srcy, int srcscrn, int width,
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

#ifdef RANGECHECK
  if (srcx<0
      ||srcx+width >SCREENWIDTH
      || srcy<0
      || srcy+height>SCREENHEIGHT
      ||destx<0||destx+width >SCREENWIDTH
      || desty<0
      || desty+height>SCREENHEIGHT)
    I_Error ("V_CopyRect: Bad arguments");
#endif

  src = _g->screens[srcscrn].data+_g->screens[srcscrn].byte_pitch*srcy+srcx;
  dest = _g->screens[destscrn].data+_g->screens[destscrn].byte_pitch*desty+destx;

  for ( ; height>0 ; height--)
    {
      memcpy (dest, src, width*2);
      src += _g->screens[srcscrn].byte_pitch;
      dest += _g->screens[destscrn].byte_pitch;
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
      dest += _g->screens[scrn].byte_pitch;
  }

  for (y=0 ; y<SCREENHEIGHT ; y+=64)
    for (x=y ? 0 : 64; x<SCREENWIDTH ; x+=64)
      V_CopyRect(0, 0, scrn, ((SCREENWIDTH-x) < 64) ? (SCREENWIDTH-x) : 64,
     ((SCREENHEIGHT-y) < 64) ? (SCREENHEIGHT-y) : 64, x, y, scrn, VPT_NONE);
  W_UnlockLumpNum(lump);
}

//
// V_Init
//
// Allocates the 4 full screen buffers in low DOS memory
// No return
//

void V_Init (void)
{

}

/*
 * This function draws at GBA resoulution (ie. not pixel doubled)
 * so the st bar and menus don't look like garbage.
 */
void V_DrawPatch(int x, int y, int scrn, const patch_t* patch)
{
    y -= patch->topoffset;
    x -= patch->leftoffset;

    int   col;
    int   left, right, top, bottom;
    int   DX  = (240<<16)  / 320;
    int   DXI = (320<<16)          / 240;
    int   DY  = (SCREENHEIGHT<<16) / 200;
    int   DYI = (200<<16)          / SCREENHEIGHT;

    byte* byte_topleft = (byte*)_g->screens[scrn].data;
    int byte_pitch = (_g->screens[scrn].byte_pitch * 2);
    int dc_iscale = DYI;

    left = ( x * DX ) >> FRACBITS;
    top = ( y * DY ) >> FRACBITS;
    right = ( (x + patch->width) * DX ) >> FRACBITS;
    bottom = ( (y + patch->height) * DY ) >> FRACBITS;

    col = 0;

    for (int dc_x=left; dc_x<right; dc_x++, col+=DXI)
    {
        const int colindex = (col>>16);
        const column_t* column = (const column_t *)((const byte*)patch + patch->columnofs[colindex]);

        // ignore this column if it's to the left of our clampRect
        if (dc_x < 0)
            continue;

        if (dc_x >= 240)
            break;

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            const byte* source = (const byte*)column + 3;
            const int topdelta = column->topdelta;

            int yoffset = 0;

            int dc_yl = (((y + topdelta) * DY)>>FRACBITS);
            int dc_yh = (((y + topdelta + column->length) * DY - (FRACUNIT>>1))>>FRACBITS);

            column = (const column_t *)((const byte *)column + column->length + 4 );

            if ((dc_yh < 0) || (dc_yh < top))
                continue;

            if ((dc_yl >= SCREENHEIGHT) || (dc_yl >= bottom))
                continue;

            if (dc_yh > bottom)
            {
                dc_yh = bottom;
            }

            if (dc_yh >= SCREENHEIGHT)
            {
                dc_yh = SCREENHEIGHT-1;
            }

            if (dc_yl < 0)
            {
                yoffset = 0-dc_yl;
                dc_yl = 0;
            }

            if (dc_yl < top)
            {
                yoffset = top-dc_yl;
                dc_yl = top;
            }

            int dc_texturemid = -((dc_yl-centery)*dc_iscale);

            {
                unsigned int count = dc_yh - dc_yl;

                const byte *colormap = _g->colormaps;

                byte* dest = byte_topleft + (dc_yl*byte_pitch) + dc_x;

                const fixed_t		fracstep = dc_iscale;
                fixed_t frac = dc_texturemid + (dc_yl - centery)*fracstep;

                // Zero length, column does not exceed a pixel.
                if (dc_yl >= dc_yh)
                    continue;

                // Inner loop that does the actual texture mapping,
                //  e.g. a DDA-lile scaling.
                // This is as fast as it gets.
                do
                {
                    // Re-map color indices from wall texture column
                    //  using a lighting/special effects LUT.
                    unsigned short color = colormap[source[(frac>>FRACBITS)&127]];

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
                        //So even addreses we just write the first color twice.
                        unsigned short* dest16 = (unsigned short*)dest;

                        *dest16 = (color | (color << 8));
                    }

                    dest += byte_pitch;
                    frac += fracstep;
                } while (count--);
            }
        }
    }
}

//
// V_DrawMemPatch
//
// CPhipps - unifying patch drawing routine, handles all cases and combinations
//  of stretching, flipping and translating
//
// This function is big, hopefully not too big that gcc can't optimise it well.
// In fact it packs pretty well, there is no big performance lose for all this merging;
// the inner loops themselves are just the same as they always were
// (indeed, laziness of the people who wrote the 'clones' of the original V_DrawPatch
//  means that their inner loops weren't so well optimised, so merging code may even speed them).
//
static void V_DrawMemPatch(int x, int y, int scrn, const rpatch_t *patch, int cm, enum patch_translation_e flags)
{
    const byte *trans = NULL;

    y -= patch->topoffset;
    x -= patch->leftoffset;

    // CPhipps - move stretched patch drawing code here
    //         - reformat initialisers, move variables into inner blocks

    int   col;
    int   w = (patch->width << 16) - 1; // CPhipps - -1 for faster flipping
    int   left, right, top, bottom;
    int   DX  = (SCREENWIDTH<<16)  / 320;
    int   DXI = (320<<16)          / SCREENWIDTH;
    int   DY  = (SCREENHEIGHT<<16) / 200;
    int   DYI = (200<<16)          / SCREENHEIGHT;

    draw_column_vars_t dcvars;
    draw_vars_t olddrawvars = _g->drawvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    _g->drawvars.byte_topleft = _g->screens[scrn].data;
    _g->drawvars.byte_pitch = _g->screens[scrn].byte_pitch;

    left = ( x * DX ) >> FRACBITS;
    top = ( y * DY ) >> FRACBITS;
    right = ( (x + patch->width) * DX ) >> FRACBITS;
    bottom = ( (y + patch->height) * DY ) >> FRACBITS;

    dcvars.iscale = DYI;

    col = 0;

    for (dcvars.x=left; dcvars.x<right; dcvars.x++, col+=DXI)
    {
        int i;
        const int colindex = (flags & VPT_FLIP) ? ((w - col)>>16): (col>>16);
        const rcolumn_t *column = R_GetPatchColumn(patch, colindex);

        // ignore this column if it's to the left of our clampRect
        if (dcvars.x < 0)
            continue;
        if (dcvars.x >= SCREENWIDTH)
            break;

        // step through the posts in a column
        for (i=0; i<column->numPosts; i++)
        {
            const rpost_t *post = &column->posts[i];
            int yoffset = 0;

            dcvars.yl = (((y + post->topdelta) * DY)>>FRACBITS);
            dcvars.yh = (((y + post->topdelta + post->length) * DY - (FRACUNIT>>1))>>FRACBITS);

            if ((dcvars.yh < 0) || (dcvars.yh < top))
                continue;
            if ((dcvars.yl >= SCREENHEIGHT) || (dcvars.yl >= bottom))
                continue;

            if (dcvars.yh >= bottom)
            {
                dcvars.yh = bottom-1;
            }

            if (dcvars.yh >= SCREENHEIGHT)
            {
                dcvars.yh = SCREENHEIGHT-1;
            }

            if (dcvars.yl < 0)
            {
                yoffset = 0-dcvars.yl;
                dcvars.yl = 0;
            }

            if (dcvars.yl < top)
            {
                yoffset = top-dcvars.yl;
                dcvars.yl = top;
            }

            dcvars.source = column->pixels + post->topdelta + yoffset;

            dcvars.texturemid = -((dcvars.yl-centery)*dcvars.iscale);

            R_DrawColumn(&dcvars);
        }
    }

    _g->drawvars = olddrawvars;
}

// CPhipps - some simple, useful wrappers for that function, for drawing patches from wads

// CPhipps - GNU C only suppresses generating a copy of a function if it is
// static inline; other compilers have different behaviour.
// This inline is _only_ for the function below

void V_DrawNumPatch(int x, int y, int scrn, int lump,
         int cm, enum patch_translation_e flags)
{
    V_DrawPatch(x, y, scrn, W_CacheLumpNum(lump));

  //V_DrawMemPatch(x, y, scrn, R_CachePatchNum(lump), cm, flags);
  //R_UnlockPatchNum(lump);
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
    unsigned short* dest = _g->screens[scrn].data + x + y*_g->screens[scrn].byte_pitch;
    while (height--)
    {
        memset(dest, colour, width * 2);
        dest += _g->screens[scrn].byte_pitch;
    }
}

//
// V_InitMode
//
void V_InitMode(video_mode_t mode)
{
    lprintf(LO_INFO, "V_InitMode: using 8 bit video mode\n");
}

//
// V_GetNumPixelBits
//
int V_GetNumPixelBits(void)
{
    return 8;
}

//
// V_AllocScreen
//
void V_AllocScreen(screeninfo_t *scrn)
{
    if ((scrn->byte_pitch * scrn->height) > 0)
      scrn->data = malloc( (scrn->byte_pitch*scrn->height) * 2);
}

//
// V_AllocScreens
//
void V_AllocScreens(void)
{
  V_AllocScreen(&_g->screens[1]);
}

//
// V_FreeScreen
//
void V_FreeScreen(screeninfo_t *scrn)
{

}

//
// V_FreeScreens
//
void V_FreeScreens(void)
{

}

void V_PlotPixel(int scrn, int x, int y, byte color)
{
    _g->screens[scrn].data[x+_g->screens[scrn].byte_pitch*y] = (color | (color << 8));
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

#ifdef RANGECHECK         // killough 2/22/98
  static int fuck = 0;

  // For debugging only
  if
  (
       fl->a.x < 0 || fl->a.x >= SCREENWIDTH
    || fl->a.y < 0 || fl->a.y >= SCREENHEIGHT
    || fl->b.x < 0 || fl->b.x >= SCREENWIDTH
    || fl->b.y < 0 || fl->b.y >= SCREENHEIGHT
  )
  {
    //jff 8/3/98 use logical output routine
    lprintf(LO_DEBUG, "fuck %d \r", fuck++);
    return;
  }
#endif

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
