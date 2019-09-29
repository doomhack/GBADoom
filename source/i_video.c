/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by
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
 *  DOOM graphics stuff for SDL
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>

#include "doomstat.h"
#include "doomdef.h"
#include "doomtype.h"
#include "v_video.h"
#include "r_draw.h"
#include "d_main.h"
#include "d_event.h"
#include "i_video.h"
#include "i_sound.h"
#include "z_zone.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "lprintf.h"

#include "i_system_e32.h"

#include "global_data.h"

//
// I_StartTic
//

void I_StartTic (void)
{
	I_ProcessKeyEvents();
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
	I_UpdateMusic();
}


boolean I_StartDisplay(void)
{
    _g->screens[0].data = I_GetBackBuffer();

    // Same with base row offset.
    _g->drawvars.byte_topleft = _g->screens[0].data;
    _g->drawvars.byte_pitch = _g->screens[0].byte_pitch;

    return true;
}

void I_EndDisplay(void)
{

}


//
// I_InitInputs
//

static void I_InitInputs(void)
{

}
/////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////
// Palette stuff.
//
static void I_UploadNewPalette(int pal)
{
  // This is used to replace the current 256 colour cmap with a new one
  // Used by 256 colour PseudoColor modes
  
	int pplump;

	const byte* palette;

	unsigned int num_pals;

	pplump = W_GetNumForName("PLAYPAL");
	
	palette = W_CacheLumpNum(pplump);
		
    num_pals = W_LumpLength(pplump) / (3*256);
	
    if(pal >= num_pals)
        pal = 0;

	palette += (pal*256*3);

    _g->current_pallete = palette;

    I_SetPallete_e32(palette);

	W_UnlockLumpNum(pplump);
	
#ifdef RANGECHECK
	if ((size_t)pal >= num_pals)
		I_Error("I_UploadNewPalette: Palette number out of range (%d>=%d)", pal, num_pals);
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Graphics API

void I_ShutdownGraphics(void)
{
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
}

//
// I_FinishUpdate
//
#define NO_PALETTE_CHANGE 1000

void I_FinishUpdate (void)
{
    if (_g->newpal != NO_PALETTE_CHANGE)
	{
        I_UploadNewPalette(_g->newpal);
        _g->newpal = NO_PALETTE_CHANGE;
	}

    I_FinishUpdate_e32(_g->screens[0].data, _g->current_pallete, SCREENWIDTH, SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette (int pal)
{
    _g->newpal = pal;
}


void I_PreInitGraphics(void)
{
	I_InitScreen_e32();
}

// CPhipps -
// I_SetRes
// Sets the screen resolution
void I_SetRes(void)
{
    //backbuffer
    _g->screens[0].width = SCREENWIDTH;
    _g->screens[0].height = SCREENHEIGHT;
    _g->screens[0].byte_pitch = SCREENPITCH;

    // statusbar
    _g->screens[1].width = SCREENWIDTH;
    _g->screens[1].height = (ST_SCALED_HEIGHT+1);
    _g->screens[1].byte_pitch = SCREENPITCH;

    lprintf(LO_INFO,"I_SetRes: Using resolution %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
}

void I_InitGraphics(void)
{
  char titlebuffer[1024];
  static int    firsttime=1;

  if (firsttime)
  {
    firsttime = 0;

    atexit(I_ShutdownGraphics);
    lprintf(LO_INFO, "I_InitGraphics: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);

    /* Set the video mode */
    I_UpdateVideoMode();

    /* Setup the window title */
    strcpy(titlebuffer,PACKAGE);
    strcat(titlebuffer," ");
    strcat(titlebuffer,VERSION);

    /* Initialize the input system */
    I_InitInputs();

	I_CreateBackBuffer_e32();
  }
}

void I_UpdateVideoMode(void)
{
  video_mode_t mode;

  lprintf(LO_INFO, "I_UpdateVideoMode: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);

  mode = VID_MODE8;

  V_InitMode(mode);
  V_FreeScreens();

  I_SetRes();

  lprintf(LO_INFO, "I_UpdateVideoMode:\n");

  V_AllocScreens();

  R_InitBuffer(SCREENWIDTH, SCREENHEIGHT);
}
