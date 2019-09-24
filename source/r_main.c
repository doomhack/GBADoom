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
 *      Rendering main loop and setup functions,
 *       utility functions (BSP, geometry, trigonometry).
 *      See tables.c, too.
 *
 *-----------------------------------------------------------------------------*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_draw.h"
#include "m_bbox.h"
#include "r_sky.h"
#include "v_video.h"
#include "lprintf.h"
#include "st_stuff.h"
#include "i_main.h"
#include "i_system.h"
#include "g_game.h"

#include "global_data.h"

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

const int viewheight = SCREENHEIGHT-ST_SCALED_HEIGHT;
const int viewheightfrac = (SCREENHEIGHT-ST_SCALED_HEIGHT)<<FRACBITS;
const int centery = (SCREENHEIGHT-ST_SCALED_HEIGHT)/2;
const int centerx = SCREENWIDTH/2;
const int centerxfrac = (SCREENWIDTH/2) << FRACBITS;
const int centeryfrac = ((SCREENHEIGHT-ST_SCALED_HEIGHT)/2) << FRACBITS;

const fixed_t projection = (SCREENWIDTH/2) << FRACBITS;
const fixed_t projectiony = ((SCREENHEIGHT * (SCREENWIDTH/2) * 320) / 200) / SCREENWIDTH * FRACUNIT;

const fixed_t pspritescale = FRACUNIT*SCREENWIDTH/320;
const fixed_t pspriteiscale = FRACUNIT*320/SCREENWIDTH;

const fixed_t pspriteyscale = (((SCREENHEIGHT*SCREENWIDTH)/SCREENWIDTH) << FRACBITS) / 200;

const angle_t clipangle = 537395200; //xtoviewangle[0];


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

PUREFUNC int R_PointOnSide(fixed_t x, fixed_t y, const node_t *node)
{
  if (!node->dx)
    return x <= node->x ? node->dy > 0 : node->dy < 0;

  if (!node->dy)
    return y <= node->y ? node->dx < 0 : node->dx > 0;

  x -= node->x;
  y -= node->y;

  // Try to quickly decide by looking at sign bits.
  if ((node->dy ^ node->dx ^ x ^ y) < 0)
    return (node->dy ^ x) < 0;  // (left is negative)
  return FixedMul(y, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, x);
}

// killough 5/2/98: reformatted

PUREFUNC int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
  fixed_t lx = line->v1->x;
  fixed_t ly = line->v1->y;
  fixed_t ldx = line->v2->x - lx;
  fixed_t ldy = line->v2->y - ly;

  if (!ldx)
    return x <= lx ? ldy > 0 : ldy < 0;

  if (!ldy)
    return y <= ly ? ldx < 0 : ldx > 0;

  x -= lx;
  y -= ly;

  // Try to quickly decide by looking at sign bits.
  if ((ldy ^ ldx ^ x ^ y) < 0)
    return (ldy ^ x) < 0;          // (left is negative)
  return FixedMul(y, ldx>>FRACBITS) >= FixedMul(ldy>>FRACBITS, x);
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up

#include <math.h>

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
  static fixed_t oldx, oldy;
  static angle_t oldresult;

  x -= _g->viewx; y -= _g->viewy;

  if ( /* !render_precise && */
      // e6y: here is where "slime trails" can SOMETIMES occur
      (x < INT_MAX/4 && x > -INT_MAX/4 && y < INT_MAX/4 && y > -INT_MAX/4)
     )
  {
    // old R_PointToAngle
    return (x || y) ?
    x >= 0 ?
      y >= 0 ?
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :                // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
  }

  // R_PointToAngleEx merged into R_PointToAngle
  // e6y: The precision of the code above is abysmal so use the CRT atan2 function instead!
  if (oldx != x || oldy != y)
  {
    oldx = x;
    oldy = y;
    oldresult = (int)(atan2(y, x) * ANG180/M_PI);
  }
  return oldresult;
}

angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y)
{
  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ?
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :                // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
}

//
// R_InitTextureMapping
//
// killough 5/2/98: reformatted

static void R_InitTextureMapping (void)
{

}

//
// R_InitLightTables
//

#define DISTMAP 2

static void R_InitLightTables (void)
{
    int i;


    // Calculate the light levels to use
    //  for each level / distance combination.
    // Calculate the light levels to use
    //  for each level / distance combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
        int startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
        for (int j=0 ; j<MAXLIGHTZ ; j++)
        {
            int scale = FixedDiv ((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
            scale >>= LIGHTSCALESHIFT;
            int level = startmap - scale/DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS-1;

            _g->zlight[i][j] = _g->colormaps + level*256;
        }
    }
}

//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//


void R_SetViewSize(int blocks)
{
  _g->setsizeneeded = true;
}

//
// R_ExecuteSetViewSize
//

void R_ExecuteSetViewSize (void)
{
  _g->setsizeneeded = false;

  R_InitBuffer (SCREENWIDTH, viewheight);

  R_InitTextureMapping();
}

//
// R_Init
//

void R_Init (void)
{
  // CPhipps - R_DrawColumn isn't constant anymore, so must
  //  initialise in code
  // current column draw function
  lprintf(LO_INFO, "R_LoadTrigTables\n");
  R_LoadTrigTables();
  lprintf(LO_INFO, "R_InitData\n");
  R_InitData();
  R_SetViewSize(10);
  lprintf(LO_INFO, "R_Init: R_InitPlanes\n");
  R_InitPlanes();
  lprintf(LO_INFO, "R_InitLightTables\n");
  R_InitLightTables();
  lprintf(LO_INFO, "R_InitSkyMap\n");
  R_InitSkyMap();
  lprintf(LO_INFO, "R_InitTranslationsTables\n");
  R_InitTranslationTables();
}

//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
  int nodenum = _g->numnodes-1;

  // special case for trivial maps (single subsector, no nodes)
  if (_g->numnodes == 0)
    return _g->subsectors;

  while (!(nodenum & NF_SUBSECTOR))
    nodenum = _g->nodes[nodenum].children[R_PointOnSide(x, y, _g->nodes+nodenum)];
  return &_g->subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_SetupFrame
//

static void R_SetupFrame (player_t *player)
{
  _g->viewplayer = player;

  _g->viewx = player->mo->x;
  _g->viewy = player->mo->y;
  _g->viewz = player->viewz;
  _g->viewangle = player->mo->angle;

  _g->extralight = player->extralight;

  _g->viewsin = finesine[_g->viewangle>>ANGLETOFINESHIFT];
  _g->viewcos = finecosine[_g->viewangle>>ANGLETOFINESHIFT];

  _g->fullcolormap = &_g->colormaps[0];

  if (player->fixedcolormap)
    {
      _g->fixedcolormap = _g->fullcolormap   // killough 3/20/98: use fullcolormap
        + player->fixedcolormap*256*sizeof(lighttable_t);
    }
  else
    _g->fixedcolormap = 0;

  _g->validcount++;
}

//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{
  R_SetupFrame (player);

  // Clear buffers.
  R_ClearClipSegs ();
  R_ClearDrawSegs ();
  R_ClearPlanes ();
  R_ClearSprites ();

  // The head node is the last node output.
  R_RenderBSPNode (_g->numnodes-1);

  R_DrawPlanes ();

    R_DrawMasked ();

}
