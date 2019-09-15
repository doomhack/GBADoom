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
 *      BSP traversal, handling of LineSegs for rendering.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "m_bbox.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_plane.h"
#include "r_things.h"
#include "r_bsp.h" // cph - sanity checking
#include "v_video.h"
#include "lprintf.h"

#include "global_data.h"

//
// R_ClearDrawSegs
//

void R_ClearDrawSegs(void)
{
  _g->ds_p = _g->drawsegs;
}

// CPhipps -
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those
// columns which aren't solid, and updates the solidcol[] array appropriately

static void R_ClipWallSegment(int first, int last, boolean solid)
{
    byte *p;
    while (first < last)
    {
        if (_g->solidcol[first])
        {
            if (!(p = memchr(_g->solidcol+first, 0, last-first))) return; // All solid
            first = p - _g->solidcol;
        }
        else
        {
            int to;
            if (!(p = memchr(_g->solidcol+first, 1, last-first))) to = last;
            else to = p - _g->solidcol;
            R_StoreWallRange(first, to-1);
            if (solid) {
                memset(_g->solidcol+first,1,to-first);
            }
            first = to;
        }
    }
}

//
// R_ClearClipSegs
//

void R_ClearClipSegs (void)
{
  memset(_g->solidcol, 0, SCREENWIDTH);
}

// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// cph - converted to R_RecalcLineFlags. This recalculates all the flags for
// a line, including closure and texture tiling.

static void R_RecalcLineFlags(void)
{
  _g->linedef->r_validcount = _g->gametic;

  /* First decide if the line is closed, normal, or invisible */
  if (!(_g->linedef->flags & ML_TWOSIDED)
      || _g->backsector->ceilingheight <= _g->frontsector->floorheight
      || _g->backsector->floorheight >= _g->frontsector->ceilingheight
      || (
    // if door is closed because back is shut:
    _g->backsector->ceilingheight <= _g->backsector->floorheight

    // preserve a kind of transparent door/lift special effect:
    && (_g->backsector->ceilingheight >= _g->frontsector->ceilingheight ||
        _g->curline->sidedef->toptexture)

    && (_g->backsector->floorheight <= _g->frontsector->floorheight ||
        _g->curline->sidedef->bottomtexture)

    // properly render skies (consider door "open" if both ceilings are sky):
    && (_g->backsector->ceilingpic !=skyflatnum ||
        _g->frontsector->ceilingpic!=skyflatnum)
    )
      )
    _g->linedef->r_flags = RF_CLOSED;
  else {
    // Reject empty lines used for triggers
    //  and special events.
    // Identical floor and ceiling on both sides,
    // identical light levels on both sides,
    // and no middle texture.
    // CPhipps - recode for speed, not certain if this is portable though
    if (_g->backsector->ceilingheight != _g->frontsector->ceilingheight
  || _g->backsector->floorheight != _g->frontsector->floorheight
  || _g->curline->sidedef->midtexture
  || memcmp(&_g->backsector->floor_xoffs, &_g->frontsector->floor_xoffs,
      sizeof(_g->frontsector->floor_xoffs) + sizeof(_g->frontsector->floor_yoffs) +
      sizeof(_g->frontsector->ceiling_xoffs) + sizeof(_g->frontsector->ceiling_yoffs) +
      sizeof(_g->frontsector->ceilingpic) + sizeof(_g->frontsector->floorpic) +
      sizeof(_g->frontsector->lightlevel) + sizeof(_g->frontsector->floorlightsec) +
      sizeof(_g->frontsector->ceilinglightsec))) {
      _g->linedef->r_flags = 0; return;
    } else
      _g->linedef->r_flags = RF_IGNORE;
  }

  /* cph - I'm too lazy to try and work with offsets in this */
  if (_g->curline->sidedef->rowoffset) return;

  /* Now decide on texture tiling */
  if (_g->linedef->flags & ML_TWOSIDED) {
    int c;

    /* Does top texture need tiling */
    if ((c = _g->frontsector->ceilingheight - _g->backsector->ceilingheight) > 0 &&
   (_g->textureheight[_g->texturetranslation[_g->curline->sidedef->toptexture]] > c))
      _g->linedef->r_flags |= RF_TOP_TILE;

    /* Does bottom texture need tiling */
    if ((c = _g->frontsector->floorheight - _g->backsector->floorheight) > 0 &&
   (_g->textureheight[_g->texturetranslation[_g->curline->sidedef->bottomtexture]] > c))
      _g->linedef->r_flags |= RF_BOT_TILE;
  } else {
    int c;
    /* Does middle texture need tiling */
    if ((c = _g->frontsector->ceilingheight - _g->frontsector->floorheight) > 0 &&
   (_g->textureheight[_g->texturetranslation[_g->curline->sidedef->midtexture]] > c))
      _g->linedef->r_flags |= RF_MID_TILE;
  }
}

//
// killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
// killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//

sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec,
                     int *floorlightlevel, int *ceilinglightlevel,
                     boolean back)
{
  if (floorlightlevel)
    *floorlightlevel = sec->floorlightsec == -1 ?
      sec->lightlevel : _g->sectors[sec->floorlightsec].lightlevel;

  if (ceilinglightlevel)
    *ceilinglightlevel = sec->ceilinglightsec == -1 ? // killough 4/11/98
      sec->lightlevel : _g->sectors[sec->ceilinglightsec].lightlevel;

  if (sec->heightsec != -1)
    {
      const sector_t *s = &_g->sectors[sec->heightsec];
      int heightsec = _g->viewplayer->mo->subsector->sector->heightsec;
      int underwater = heightsec!=-1 && _g->viewz<=_g->sectors[heightsec].floorheight;

      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      // Replace floor and ceiling height with other sector's heights.
      tempsec->floorheight   = s->floorheight;
      tempsec->ceilingheight = s->ceilingheight;

      // killough 11/98: prevent sudden light changes from non-water sectors:
      if (underwater && (tempsec->  floorheight = sec->floorheight,
                          tempsec->ceilingheight = s->floorheight-1, !back))
        {                   // head-below-floor hack
          tempsec->floorpic    = s->floorpic;
          tempsec->floor_xoffs = s->floor_xoffs;
          tempsec->floor_yoffs = s->floor_yoffs;

          if (underwater) {
            if (s->ceilingpic == skyflatnum) {
		tempsec->floorheight   = tempsec->ceilingheight+1;
		tempsec->ceilingpic    = tempsec->floorpic;
                tempsec->ceiling_xoffs = tempsec->floor_xoffs;
                tempsec->ceiling_yoffs = tempsec->floor_yoffs;
	    } else {
		tempsec->ceilingpic    = s->ceilingpic;
		tempsec->ceiling_xoffs = s->ceiling_xoffs;
		tempsec->ceiling_yoffs = s->ceiling_yoffs;
	    }
	  }

          tempsec->lightlevel  = s->lightlevel;

          if (floorlightlevel)
            *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
            _g->sectors[s->floorlightsec].lightlevel; // killough 3/16/98

          if (ceilinglightlevel)
            *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
            _g->sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
        }
      else
        if (heightsec != -1 && _g->viewz >= _g->sectors[heightsec].ceilingheight &&
            sec->ceilingheight > s->ceilingheight)
          {   // Above-ceiling hack
            tempsec->ceilingheight = s->ceilingheight;
            tempsec->floorheight   = s->ceilingheight + 1;

            tempsec->floorpic    = tempsec->ceilingpic    = s->ceilingpic;
            tempsec->floor_xoffs = tempsec->ceiling_xoffs = s->ceiling_xoffs;
            tempsec->floor_yoffs = tempsec->ceiling_yoffs = s->ceiling_yoffs;

            if (s->floorpic != skyflatnum)
              {
                tempsec->ceilingheight = sec->ceilingheight;
                tempsec->floorpic      = s->floorpic;
                tempsec->floor_xoffs   = s->floor_xoffs;
                tempsec->floor_yoffs   = s->floor_yoffs;
              }

            tempsec->lightlevel  = s->lightlevel;

            if (floorlightlevel)
              *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
              _g->sectors[s->floorlightsec].lightlevel; // killough 3/16/98

            if (ceilinglightlevel)
              *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
              _g->sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
          }
      sec = tempsec;               // Use other sector
    }
  return sec;
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

static void R_AddLine (seg_t *line)
{
  int      x1;
  int      x2;
  angle_t  angle1;
  angle_t  angle2;
  angle_t  span;
  angle_t  tspan;
  static sector_t tempsec;     // killough 3/8/98: ceiling/water hack

  _g->curline = line;

  angle1 = R_PointToAngle (line->v1->x, line->v1->y);
  angle2 = R_PointToAngle (line->v2->x, line->v2->y);

  // Clip to view edges.
  span = angle1 - angle2;

  // Back side, i.e. backface culling
  if (span >= ANG180)
    return;

  // Global angle needed by segcalc.
  rw_angle1 = angle1;
  angle1 -= _g->viewangle;
  angle2 -= _g->viewangle;

  tspan = angle1 + _g->clipangle;
  if (tspan > 2*_g->clipangle)
    {
      tspan -= 2*_g->clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return;

      angle1 = _g->clipangle;
    }

  tspan = _g->clipangle - angle2;
  if (tspan > 2*_g->clipangle)
    {
      tspan -= 2*_g->clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return;
      angle2 = 0-_g->clipangle;
    }

  // The seg is in the view range,
  // but not necessarily visible.

  angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
  angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;

  // killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
  x1 = _g->viewangletox[angle1];
  x2 = _g->viewangletox[angle2];

  // Does not cross a pixel?
  if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
    return;

  _g->backsector = line->backsector;

  // Single sided line?
  if (_g->backsector)
    // killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
    _g->backsector = R_FakeFlat(_g->backsector, &tempsec, NULL, NULL, true);

  /* cph - roll up linedef properties in flags */
  if ((_g->linedef = _g->curline->linedef)->r_validcount != _g->gametic)
    R_RecalcLineFlags();

  if (_g->linedef->r_flags & RF_IGNORE)
  {
    return;
  }
  else
    R_ClipWallSegment (x1, x2, _g->linedef->r_flags & RF_CLOSED);
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//

static const int checkcoord[12][4] = // killough -- static const
{
  {3,0,2,1},
  {3,0,2,0},
  {3,1,2,0},
  {0},
  {2,0,2,1},
  {0,0,0,0},
  {3,1,3,0},
  {0},
  {2,0,3,1},
  {2,1,3,1},
  {2,1,3,0}
};

// killough 1/28/98: static // CPhipps - const parameter, reformatted
static boolean R_CheckBBox(const fixed_t *bspcoord)
{
  angle_t angle1, angle2;

  {
    int        boxpos;
    const int* check;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    boxpos = (_g->viewx <= bspcoord[BOXLEFT] ? 0 : _g->viewx < bspcoord[BOXRIGHT ] ? 1 : 2) +
      (_g->viewy >= bspcoord[BOXTOP ] ? 0 : _g->viewy > bspcoord[BOXBOTTOM] ? 4 : 8);

    if (boxpos == 5)
      return true;

    check = checkcoord[boxpos];
    angle1 = R_PointToAngle (bspcoord[check[0]], bspcoord[check[1]]) - _g->viewangle;
    angle2 = R_PointToAngle (bspcoord[check[2]], bspcoord[check[3]]) - _g->viewangle;
  }

  // cph - replaced old code, which was unclear and badly commented
  // Much more efficient code now
  if ((signed)angle1 < (signed)angle2) { /* it's "behind" us */
    /* Either angle1 or angle2 is behind us, so it doesn't matter if we
     * change it to the corect sign
     */
    if ((angle1 >= ANG180) && (angle1 < ANG270))
      angle1 = INT_MAX; /* which is ANG180-1 */
    else
      angle2 = INT_MIN;
  }

  if ((signed)angle2 >= (signed)_g->clipangle) return false; // Both off left edge
  if ((signed)angle1 <= -(signed)_g->clipangle) return false; // Both off right edge
  if ((signed)angle1 >= (signed)_g->clipangle) angle1 = _g->clipangle; // Clip at left edge
  if ((signed)angle2 <= -(signed)_g->clipangle) angle2 = 0-_g->clipangle; // Clip at right edge

  // Find the first clippost
  //  that touches the source post
  //  (adjacent pixels are touching).
  angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
  angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
  {
    int sx1 = _g->viewangletox[angle1];
    int sx2 = _g->viewangletox[angle2];
    //    const cliprange_t *start;

    // Does not cross a pixel.
    if (sx1 == sx2)
      return false;

    if (!memchr(_g->solidcol+sx1, 0, sx2-sx1)) return false;
    // All columns it covers are already solidly covered
  }

  return true;
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished

static void R_Subsector(int num)
{
  int         count;
  seg_t       *line;
  subsector_t *sub;
  sector_t    tempsec;              // killough 3/7/98: deep water hack
  int         floorlightlevel;      // killough 3/16/98: set floor lightlevel
  int         ceilinglightlevel;    // killough 4/11/98

#ifdef RANGECHECK
  if (num>=numsubsectors)
    I_Error ("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

  sub = &_g->subsectors[num];
  _g->frontsector = sub->sector;
  count = sub->numlines;
  line = &_g->segs[sub->firstline];

  // killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
  _g->frontsector = R_FakeFlat(_g->frontsector, &tempsec, &floorlightlevel,
                           &ceilinglightlevel, false);   // killough 4/11/98

  // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
  // killough 3/16/98: add floorlightlevel
  // killough 10/98: add support for skies transferred from sidedefs

  _g->floorplane = _g->frontsector->floorheight < _g->viewz || // killough 3/7/98
    (_g->frontsector->heightsec != -1 &&
     _g->sectors[_g->frontsector->heightsec].ceilingpic == skyflatnum) ?
    R_FindPlane(_g->frontsector->floorheight,
                _g->frontsector->floorpic == skyflatnum &&  // kilough 10/98
                _g->frontsector->sky & PL_SKYFLAT ? _g->frontsector->sky :
                _g->frontsector->floorpic,
                floorlightlevel,                // killough 3/16/98
                _g->frontsector->floor_xoffs,       // killough 3/7/98
                _g->frontsector->floor_yoffs
                ) : NULL;

  _g->ceilingplane = _g->frontsector->ceilingheight > _g->viewz ||
    _g->frontsector->ceilingpic == skyflatnum ||
    (_g->frontsector->heightsec != -1 &&
     _g->sectors[_g->frontsector->heightsec].floorpic == skyflatnum) ?
    R_FindPlane(_g->frontsector->ceilingheight,     // killough 3/8/98
                _g->frontsector->ceilingpic == skyflatnum &&  // kilough 10/98
                _g->frontsector->sky & PL_SKYFLAT ? _g->frontsector->sky :
                _g->frontsector->ceilingpic,
                ceilinglightlevel,              // killough 4/11/98
                _g->frontsector->ceiling_xoffs,     // killough 3/7/98
                _g->frontsector->ceiling_yoffs
                ) : NULL;

  // killough 9/18/98: Fix underwater slowdown, by passing real sector
  // instead of fake one. Improve sprite lighting by basing sprite
  // lightlevels on floor & ceiling lightlevels in the surrounding area.
  //
  // 10/98 killough:
  //
  // NOTE: TeamTNT fixed this bug incorrectly, messing up sprite lighting!!!
  // That is part of the 242 effect!!!  If you simply pass sub->sector to
  // the old code you will not get correct lighting for underwater sprites!!!
  // Either you must pass the fake sector and handle validcount here, on the
  // real sector, or you must account for the lighting in some other way,
  // like passing it as an argument.

  R_AddSprites(sub, (floorlightlevel+ceilinglightlevel)/2);
  while (count--)
  {
    if (line->miniseg == false)
      R_AddLine (line);
    line++;
    _g->curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
  }
}

//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
// killough 5/2/98: reformatted, removed tail recursion

void R_RenderBSPNode(int bspnum)
{
  while (!(bspnum & NF_SUBSECTOR))  // Found a subsector?
    {
      const node_t *bsp = &_g->nodes[bspnum];

      // Decide which side the view point is on.
      int side = R_PointOnSide(_g->viewx, _g->viewy, bsp);
      // Recursively divide front space.
      R_RenderBSPNode(bsp->children[side]);

      // Possibly divide back space.

      if (!R_CheckBBox(bsp->bbox[side^1]))
        return;

      bspnum = bsp->children[side^1];
    }
  R_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}
