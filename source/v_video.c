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
  byte *src;
  byte *dest;

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
      memcpy (dest, src, width);
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

  byte *dest = _g->screens[scrn].data;

  // killough 4/17/98:
  src = W_CacheLumpNum(lump = _g->firstflat + R_FlatNumForName(flatname));

  /* V_DrawBlock(0, 0, scrn, 64, 64, src, 0); */
  width = height = 64;
  

  
  while (height--)
  {
	  memcpy (dest, src, width);
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
	
	// CPhipps - auto-no-stretch if not high-res
	if (flags & VPT_STRETCH)
		if ((SCREENWIDTH==320) && (SCREENHEIGHT==200))
			flags &= ~VPT_STRETCH;
		
		// CPhipps - null translation pointer => no translation
		if (!trans)
			flags &= ~VPT_TRANS;

		if (!(flags & VPT_STRETCH))
		{
			int             col;
            byte           *desttop = _g->screens[scrn].data+y*_g->screens[scrn].byte_pitch+x;
			unsigned int    w = patch->width;
			
			if (y<0 || y+patch->height > ((flags & VPT_STRETCH) ? 200 :  SCREENHEIGHT))
			{
				// killough 1/19/98: improved error message:
				lprintf(LO_WARN, "V_DrawMemPatch8: Patch (%d,%d)-(%d,%d) exceeds LFB in vertical direction (horizontal is clipped)\n"
					"Bad V_DrawMemPatch8 (flags=%u)", x, y, x+patch->width, y+patch->height, flags);
				return;
			}

			w--; // CPhipps - note: w = width-1 now, speeds up flipping
			
			for (col=0 ; (unsigned int)col<=w ; desttop++, col++, x++)
			{
				int i;
				const int colindex = (flags & VPT_FLIP) ? (w - col) : (col);
				const rcolumn_t *column = R_GetPatchColumn(patch, colindex);
				
				if (x < 0)
					continue;
				
				if (x >= SCREENWIDTH)
					break;
				
				// step through the posts in a column
				for (i=0; i<column->numPosts; i++)
				{
					const rpost_t *post = &column->posts[i];
					// killough 2/21/98: Unrolled and performance-tuned
					
					const byte *source = column->pixels + post->topdelta;
                    byte *dest = desttop + post->topdelta*_g->screens[scrn].byte_pitch;
					int count = post->length;

					if (!(flags & VPT_TRANS))
					{
						if ((count-=4)>=0)
						{
							do
							{
								register byte s0,s1;
								s0 = source[0];
								s1 = source[1];
								
								dest[0] = s0;
                                dest[_g->screens[scrn].byte_pitch] = s1;
                                dest += _g->screens[scrn].byte_pitch*2;
								s0 = source[2];
								s1 = source[3];
								source += 4;
								dest[0] = s0;
                                dest[_g->screens[scrn].byte_pitch] = s1;
                                dest += _g->screens[scrn].byte_pitch*2;
							} while ((count-=4)>=0);
						}
							
						if (count+=4)
						{
							do
							{
								*dest = *source++;
                                dest += _g->screens[scrn].byte_pitch;
							} while (--count);
						}
					}
					else
					{
						// CPhipps - merged translation code here
						if ((count-=4)>=0)
						{
							do
							{
								register byte s0,s1;
								s0 = source[0];
								s1 = source[1];
								s0 = trans[s0];
								s1 = trans[s1];
								dest[0] = s0;
                                dest[_g->screens[scrn].byte_pitch] = s1;
                                dest += _g->screens[scrn].byte_pitch*2;
								s0 = source[2];
								s1 = source[3];
								s0 = trans[s0];
								s1 = trans[s1];
								source += 4;
								dest[0] = s0;
                                dest[_g->screens[scrn].byte_pitch] = s1;
                                dest += _g->screens[scrn].byte_pitch*2;
							} while ((count-=4)>=0);
						}
						
						if (count+=4)
						{
							do
							{
								*dest = trans[*source++];
                                dest += _g->screens[scrn].byte_pitch;
							} while (--count);
						}
					}
				}
			}
		}
		else
		{
			// CPhipps - move stretched patch drawing code here
			//         - reformat initialisers, move variables into inner blocks
			
			int   col;
			int   w = (patch->width << 16) - 1; // CPhipps - -1 for faster flipping
			int   left, right, top, bottom;
			int   DX  = (SCREENWIDTH<<16)  / 320;
			int   DXI = (320<<16)          / SCREENWIDTH;
			int   DY  = (SCREENHEIGHT<<16) / 200;
			int   DYI = (200<<16)          / SCREENHEIGHT;

			R_DrawColumn_f colfunc;
			draw_column_vars_t dcvars;
            draw_vars_t olddrawvars = _g->drawvars;

			R_SetDefaultDrawColumnVars(&dcvars);
			
            _g->drawvars.byte_topleft = _g->screens[scrn].data;
            _g->drawvars.byte_pitch = _g->screens[scrn].byte_pitch;
			
			if (!(flags & VPT_STRETCH))
			{
				DX = 1 << 16;
				DXI = 1 << 16;
				DY = 1 << 16;
				DYI = 1 << 16;
			}

			if (flags & VPT_TRANS)
			{
				colfunc = R_DrawTranslatedColumn;
				dcvars.translation = trans;
			}
			else
			{
				colfunc = R_DrawColumn;
			}

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
				  
                  dcvars.texturemid = -((dcvars.yl-_g->centery)*dcvars.iscale);
				  
				  colfunc(&dcvars);
			  }
		}
			
        _g->drawvars = olddrawvars;
	}
}

// CPhipps - some simple, useful wrappers for that function, for drawing patches from wads

// CPhipps - GNU C only suppresses generating a copy of a function if it is
// static inline; other compilers have different behaviour.
// This inline is _only_ for the function below

void V_DrawNumPatch(int x, int y, int scrn, int lump,
         int cm, enum patch_translation_e flags)
{
  V_DrawMemPatch(x, y, scrn, R_CachePatchNum(lump), cm, flags);
  R_UnlockPatchNum(lump);
}

static int currentPaletteIndex = 0;

//
// V_SetPalette
//
// CPhipps - New function to set the palette to palette number pal.
// Handles loading of PLAYPAL and calls I_SetPalette

void V_SetPalette(int pal)
{
	currentPaletteIndex = pal;
	I_SetPalette(pal);
}

//
// V_FillRect
//
// CPhipps - New function to fill a rectangle with a given colour
void V_FillRect(int scrn, int x, int y, int width, int height, byte colour)
{
  byte* dest = _g->screens[scrn].data + x + y*_g->screens[scrn].byte_pitch;
  while (height--) {
    memset(dest, colour, width);
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
      scrn->data = malloc(scrn->byte_pitch*scrn->height);
}

//
// V_AllocScreens
//
void V_AllocScreens(void) {
  int i;

  for (i=0; i<NUM_SCREENS; i++)
    V_AllocScreen(&_g->screens[i]);
}

//
// V_FreeScreen
//
void V_FreeScreen(screeninfo_t *scrn)
{
    free(scrn->data);
    scrn->data = NULL;
}

//
// V_FreeScreens
//
void V_FreeScreens(void) {
  int i;

  for (i=0; i<NUM_SCREENS; i++)
    V_FreeScreen(&_g->screens[i]);
}

void V_PlotPixel(int scrn, int x, int y, byte color)
{
  _g->screens[scrn].data[x+_g->screens[scrn].byte_pitch*y] = color;
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
