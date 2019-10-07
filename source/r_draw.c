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
 *      The actual span/column drawing functions.
 *      Here find the main potential for optimization,
 *       e.g. inline assembly, different algorithms.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "v_video.h"
#include "st_stuff.h"
#include "g_game.h"
#include "am_map.h"
#include "lprintf.h"

#include "global_data.h"

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//


//
// Spectre/Invisibility.
//


#define FUZZOFF (SCREENWIDTH)

static const int fuzzoffset[FUZZTABLE] =
{
  FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
  FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
  FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
  FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
  FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
  FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
  FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF
};

void R_SetDefaultDrawColumnVars(draw_column_vars_t *dcvars)
{
    dcvars->x = dcvars->yl = dcvars->yh = 0;
	dcvars->iscale = dcvars->texturemid = 0;
	dcvars->source = NULL;
    dcvars->colormap = colormaps;
	dcvars->translation = NULL;
}


//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
void R_DrawFuzzColumn (draw_column_vars_t *dcvars)
{ 
    int count;

    unsigned short* dest;
    fixed_t		frac;
    fixed_t		fracstep;

	int dc_yl = dcvars->yl;
	int dc_yh = dcvars->yh;

    // Adjust borders. Low... 
    if (!dc_yl) 
		dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1)
        dc_yh = viewheight - 2;
	
	 count = dc_yh - dc_yl;

    // Zero length, column does not exceed a pixel.
    if (dc_yl >= dc_yh)
		return;
    
    dest = drawvars.byte_topleft + (dcvars->yl*drawvars.byte_pitch) + dcvars->x;


    // Looks familiar.
    fracstep = dcvars->iscale;
	
    frac = dcvars->texturemid + (dc_yl-centery)*fracstep;

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
		// Lookup framebuffer, and retrieve
		//  a pixel that is either one column
		//  left or right of the current one.
		// Add index from colormap to index.
        unsigned char srcpxl = dest[fuzzoffset[_g->fuzzpos]] & 0xff;


        unsigned short color = fullcolormap[6*256+srcpxl];

        *dest = (color | (color << 8));

		// Clamp table lookup index.
        if (++_g->fuzzpos == FUZZTABLE)
            _g->fuzzpos = 0;
	
		dest += SCREENWIDTH;

		frac += fracstep; 
    } while (count--); 
} 


//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//


void R_DrawTranslatedColumn (draw_column_vars_t *dcvars)
{
    int count = dcvars->yh - dcvars->yl;

	const byte *source = dcvars->source;
	const byte *colormap = dcvars->colormap;
	const byte *translation = dcvars->translation;

    unsigned short* dest = drawvars.byte_topleft + (dcvars->yl*drawvars.byte_pitch) + dcvars->x;

    const fixed_t		fracstep = dcvars->iscale;
    fixed_t frac = dcvars->texturemid + (dcvars->yl - centery)*fracstep;
 
	const unsigned int sw = SCREENWIDTH;

    // Zero length, column does not exceed a pixel.
    if (dcvars->yl >= dcvars->yh)
		return;

    // Here we do an additional index re-mapping.
    do 
    {
		// Translation tables are used
		//  to map certain colorramps to other ones,
		//  used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//  is mapped to gray, red, black/indigo. 
        unsigned short color = colormap[translation[source[frac>>FRACBITS]]];

        *dest = (color | (color << 8));
		dest += sw;
	
		frac += fracstep;
    } while (count--); 
} 





//
// R_InitBuffer
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//

void R_InitBuffer()
{
	// Same with base row offset.
    drawvars.byte_topleft = _g->screens[0].data;
    drawvars.byte_pitch = _g->screens[0].byte_pitch;
}
