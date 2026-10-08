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
 *  The status bar widget definitions and prototypes
 *
 *-----------------------------------------------------------------------------*/

#ifndef __STLIB__
#define __STLIB__

// We are referring to patches.
#include "r_defs.h"
#include "v_video.h"  // color ranges

//
// Typedefs of widgets
//

// The status bar area a widget draws in, in screen pixels.
// x and w are even so the area can be copied in halfwords.
typedef struct
{
  short x;
  short y;
  short w;
  short h;

  // the widget changed last frame, so the other
  //  screen page still needs this area copied over
  bool pending;
} st_area_t;

// Number widget

typedef struct
{
  // upper right-hand corner
  //  of the number (right-justified)
  short   x;
  short   y;

  // max # of digits in number
  short width;

  // last number value
  int   oldnum;

  // pointer to current value
  const int*  num;

  // list of patches for 0-9
  const patch_t** p;

  st_area_t a;

} st_number_t;

// Multiple Icon widget
typedef struct
{
  // center-justified location of icons
  short   x;
  short   y;

  // last icon number
  int     oldinum;

  // pointer to current icon
  const int*    inum;

  // list of icons
  const patch_t**   p;

  st_area_t a;

} st_multicon_t;

//
// Widget creation, access, and update routines
//
// Each frame a widget whose value changed is erased to the status bar
// background and drawn into the back page only. The next frame, when that
// page is the front page, the area is copied to the new back page.
// refresh redraws everything into the back page (after ST_refreshBackground).
//

// Number widget routines
void STlib_initNum
(st_number_t* n,
  int x,
  int y,
  const patch_t **pl,
  const int* num,
  int width );

void STlib_updateNum (st_number_t* n, bool refresh);


// Multiple Icon widget routines
void STlib_initMultIcon
( st_multicon_t* mi,
  int x,
  int y,
  const patch_t**   il,
  int count,
  const int* inum );


void STlib_updateMultIcon (st_multicon_t* mi, bool refresh);

// Draws the whole status bar background into the back page.
void ST_refreshBackground(void);

// Copies the whole status bar from the front page to the back page.
void ST_copyFromFront(void);


#endif
