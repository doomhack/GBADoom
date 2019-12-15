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
 *      LineOfSight/Visibility checks, uses REJECT Lookup Table.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "r_main.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "m_bbox.h"
#include "lprintf.h"

#include "global_data.h"

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
// killough 4/19/98: made static, cleaned up

inline static int P_DivlineSide(fixed_t x, fixed_t y, const divline_t *node)
{
  fixed_t left, right;
  return
    !node->dx ? x == node->x ? 2 : x <= node->x ? node->dy > 0 : node->dy < 0 :
    !node->dy ? (y) == node->y ? 2 : y <= node->y ? node->dx < 0 : node->dx > 0 :
    (right = ((y - node->y) >> FRACBITS) * (node->dx >> FRACBITS)) <
    (left  = ((x - node->x) >> FRACBITS) * (node->dy >> FRACBITS)) ? 0 :
    right == left ? 2 : 1;
}

//
// P_CrossSubsector
// Returns true
//  if strace crosses the given subsector successfully.
//
// killough 4/19/98: made static and cleaned up

static boolean P_CrossSubsector(int num)
{
  const seg_t *seg = _g->segs + _g->subsectors[num].firstline;
  int count;
  fixed_t opentop = 0, openbottom = 0;
  const sector_t *front = NULL, *back = NULL;

  for (count = _g->subsectors[num].numlines; --count >= 0; seg++)
  { // check lines
      int linenum = seg->linenum;

    const line_t *line = &_g->lines[linenum];
    divline_t divl;

    // allready checked other side?
    if(_g->linedata[linenum].validcount == _g->validcount)
        continue;

    _g->linedata[linenum].validcount = _g->validcount;

    /* OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test
     * cph - this is causing demo desyncs on original Doom demos.
     *  Who knows why. Exclude test for those.
     */

    //ZLB: GBA Doom.
    //I acccidently messed up this test. The monsters would chase
    //you and open doors to come after you. It was awesome!
    //I guess they could just see you even when out of sight.

    if (line->bbox[BOXLEFT] > _g->los.bbox[BOXRIGHT ] ||
            line->bbox[BOXRIGHT] < _g->los.bbox[BOXLEFT  ] ||
            line->bbox[BOXBOTTOM] > _g->los.bbox[BOXTOP   ] ||
            line->bbox[BOXTOP]    < _g->los.bbox[BOXBOTTOM])
        continue;

    // cph - do what we can before forced to check intersection
    if (line->flags & ML_TWOSIDED)
    {

      // no wall to block sight with?
      if ((front = SG_FRONTSECTOR(seg))->floorheight == (back = SG_BACKSECTOR(seg))->floorheight && front->ceilingheight == back->ceilingheight)
  continue;

      // possible occluder
      // because of ceiling height differences
      opentop = front->ceilingheight < back->ceilingheight ?
  front->ceilingheight : back->ceilingheight ;

      // because of floor height differences
      openbottom = front->floorheight > back->floorheight ?
  front->floorheight : back->floorheight ;

      // cph - reject if does not intrude in the z-space of the possible LOS
      if ((opentop >= _g->los.maxz) && (openbottom <= _g->los.minz))
  continue;
    }

    { // Forget this line if it doesn't cross the line of sight
      const vertex_t *v1,*v2;

      v1 = &line->v1;
      v2 = &line->v2;

      if (P_DivlineSide(v1->x, v1->y, &_g->los.strace) ==
          P_DivlineSide(v2->x, v2->y, &_g->los.strace))
        continue;

      divl.dx = v2->x - (divl.x = v1->x);
      divl.dy = v2->y - (divl.y = v1->y);

      // line isn't crossed?
      if (P_DivlineSide(_g->los.strace.x, _g->los.strace.y, &divl) ==
    P_DivlineSide(_g->los.t2x, _g->los.t2y, &divl))
  continue;
    }

    // cph - if bottom >= top or top < minz or bottom > maxz then it must be
    // solid wrt this LOS
    if (!(line->flags & ML_TWOSIDED) || (openbottom >= opentop) ||
  (opentop < _g->los.minz) || (openbottom > _g->los.maxz))
  return false;

    { // crosses a two sided line
      /* cph 2006/07/15 - oops, we missed this in 2.4.0 & .1;
       *  use P_InterceptVector2 for those compat levels only. */ 
      fixed_t frac = P_InterceptVector2(&_g->los.strace, &divl);

      if (front->floorheight != back->floorheight) {
        fixed_t slope = FixedDiv(openbottom - _g->los.sightzstart , frac);
        if (slope > _g->los.bottomslope)
            _g->los.bottomslope = slope;
      }

      if (front->ceilingheight != back->ceilingheight)
        {
          fixed_t slope = FixedDiv(opentop - _g->los.sightzstart , frac);
          if (slope < _g->los.topslope)
            _g->los.topslope = slope;
        }

      if (_g->los.topslope <= _g->los.bottomslope)
        return false;               // stop
    }
  }
  // passed the subsector ok
  return true;
}

static boolean P_CrossBSPNode(int bspnum)
{
    while (!(bspnum & NF_SUBSECTOR))
    {
        register const mapnode_t *bsp = nodes + bspnum;

        divline_t dl;
        dl.x = ((fixed_t)bsp->x << FRACBITS);
        dl.y = ((fixed_t)bsp->y << FRACBITS);
        dl.dx = ((fixed_t)bsp->dx << FRACBITS);
        dl.dy = ((fixed_t)bsp->dy << FRACBITS);

        int side,side2;
        side = P_DivlineSide(_g->los.strace.x,_g->los.strace.y,&dl)&1;
        side2= P_DivlineSide(_g->los.t2x, _g->los.t2y, &dl);

        if (side == side2)
            bspnum = bsp->children[side]; // doesn't touch the other side
        else         // the partition plane is crossed here
            if (!P_CrossBSPNode(bsp->children[side]))
                return 0;  // cross the starting side
            else
                bspnum = bsp->children[side^1];  // cross the ending side
    }
    return P_CrossSubsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//
// P_CheckSight
// Returns true
//  if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
// killough 4/20/98: cleaned up, made to use new LOS struct

boolean P_CheckSight(mobj_t *t1, mobj_t *t2)
{
  const sector_t *s1 = t1->subsector->sector;
  const sector_t *s2 = t2->subsector->sector;
  int pnum = (s1-_g->sectors)*_g->numsectors + (s2-_g->sectors);

  // First check for trivial rejection.
  // Determine subsector entries in REJECT table.
  //
  // Check in REJECT table.

  if (_g->rejectmatrix[pnum>>3] & (1 << (pnum&7)))   // can't possibly be connected
    return false;

  /* killough 11/98: shortcut for melee situations
   * same subsector? obviously visible
   * cph - compatibility optioned for demo sync, cf HR06-UV.LMP */
  if (t1->subsector == t2->subsector)
    return true;

  // An unobstructed LOS is possible.
  // Now look from eyes of t1 to any part of t2.

  _g->validcount++;

  _g->los.topslope = (_g->los.bottomslope = t2->z - (_g->los.sightzstart =
                                             t1->z + t1->height -
                                             (t1->height>>2))) + t2->height;
  _g->los.strace.dx = (_g->los.t2x = t2->x) - (_g->los.strace.x = t1->x);
  _g->los.strace.dy = (_g->los.t2y = t2->y) - (_g->los.strace.y = t1->y);

  if (t1->x > t2->x)
    _g->los.bbox[BOXRIGHT] = t1->x, _g->los.bbox[BOXLEFT] = t2->x;
  else
    _g->los.bbox[BOXRIGHT] = t2->x, _g->los.bbox[BOXLEFT] = t1->x;

  if (t1->y > t2->y)
    _g->los.bbox[BOXTOP] = t1->y, _g->los.bbox[BOXBOTTOM] = t2->y;
  else
    _g->los.bbox[BOXTOP] = t2->y, _g->los.bbox[BOXBOTTOM] = t1->y;


    _g->los.maxz = INT_MAX; _g->los.minz = INT_MIN;

  // the head node is the last node output
  return P_CrossBSPNode(numnodes-1);
}
