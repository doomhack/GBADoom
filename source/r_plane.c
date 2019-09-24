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
 *      Here is a core component: drawing the floors and ceilings,
 *       while maintaining a per column clipping list only.
 *      Moreover, the sky areas have to be determined.
 *
 * MAXVISPLANES is no longer a limit on the number of visplanes,
 * but a limit on the number of hash slots; larger numbers mean
 * better performance usually but after a point they are wasted,
 * and memory and time overheads creep in.
 *
 * For more information on visplanes, see:
 *
 * http://classicgaming.com/doom/editing/
 *
 * Lee Killough
 *
 *-----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "z_zone.h"  /* memory allocation wrappers -- killough */

#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_sky.h"
#include "r_plane.h"
#include "v_video.h"
#include "lprintf.h"

#include "global_data.h"

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  ((unsigned)((picnum)*3+(lightlevel)+(height)*7) & (MAXVISPLANES-1))

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
}

//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  dsvars.source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//  xoffs
//  yoffs
//
// BASIC PRIMITIVE
//

static void R_MapPlane(int y, int x1, int x2, draw_span_vars_t *dsvars)
{
  angle_t angle;
  fixed_t distance, length;
  unsigned index;

#ifdef RANGECHECK
  if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>(unsigned)viewheight)
    I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
#endif

  distance = FixedMul(_g->planeheight, yslope[y]);
  dsvars->xstep = FixedMul(distance,_g->basexscale);
  dsvars->ystep = FixedMul(distance,_g->baseyscale);

  length = FixedMul (distance, distscale[x1]);
  angle = (_g->viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;

  // killough 2/28/98: Add offsets
  dsvars->xfrac =  _g->viewx + FixedMul(finecosine[angle], length) + _g->xoffs;
  dsvars->yfrac = -_g->viewy - FixedMul(finesine[angle],   length) + _g->yoffs;

  if (!(dsvars->colormap == _g->fixedcolormap))
    {
      dsvars->z = distance;
      index = distance >> LIGHTZSHIFT;
      if (index >= MAXLIGHTZ )
        index = MAXLIGHTZ-1;
      dsvars->colormap = _g->planezlight[index];
    }
  else
   {
      dsvars->z = 0;
   }

  dsvars->y = y;
  dsvars->x1 = x1;
  dsvars->x2 = x2;
  
  R_DrawSpan(dsvars);
}

//
// R_ClearPlanes
// At begining of frame.
//

void R_ClearPlanes(void)
{
  int i;

  // opening / clipping determination
  for (i=0 ; i<SCREENWIDTH ; i++)
    _g->floorclip[i] = viewheight, _g->ceilingclip[i] = -1;

  for (i=0;i<MAXVISPLANES;i++)    // new code -- killough
    for (*_g->freehead = _g->visplanes[i], _g->visplanes[i] = NULL; *_g->freehead; )
      _g->freehead = &(*_g->freehead)->next;

  _g->lastopening = _g->openings;

  // scale will be unit scale at SCREENWIDTH/2 distance
  _g->basexscale = FixedDiv (_g->viewsin,projection);
  _g->baseyscale = FixedDiv (_g->viewcos,projection);
}

// New function, by Lee Killough

static visplane_t *new_visplane(unsigned hash)
{
  visplane_t *check = _g->freetail;
  if (!check)
    check = calloc(1, sizeof *check);
  else
    if (!(_g->freetail = _g->freetail->next))
      _g->freehead = &_g->freetail;
  check->next = _g->visplanes[hash];
  _g->visplanes[hash] = check;
  return check;
}

/*
 * R_DupPlane
 *
 * cph 2003/04/18 - create duplicate of existing visplane and set initial range
 */
visplane_t *R_DupPlane(const visplane_t *pl, int start, int stop)
{
      unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height);
      visplane_t *new_pl = new_visplane(hash);

      new_pl->height = pl->height;
      new_pl->picnum = pl->picnum;
      new_pl->lightlevel = pl->lightlevel;
      new_pl->xoffs = pl->xoffs;           // killough 2/28/98
      new_pl->yoffs = pl->yoffs;
      new_pl->minx = start;
      new_pl->maxx = stop;
      memset(new_pl->top, 0xff, sizeof new_pl->top);
      return new_pl;
}
//
// R_FindPlane
//
// killough 2/28/98: Add offsets

visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                        fixed_t xoffs, fixed_t yoffs)
{
  visplane_t *check;
  unsigned hash;                      // killough

  if (picnum == _g->skyflatnum || picnum & PL_SKYFLAT)
    height = lightlevel = 0;         // killough 7/19/98: most skies map together

  // New visplane algorithm uses hash table -- killough
  hash = visplane_hash(picnum,lightlevel,height);

  for (check=_g->visplanes[hash]; check; check=check->next)  // killough
    if (height == check->height &&
        picnum == check->picnum &&
        lightlevel == check->lightlevel &&
        xoffs == check->xoffs &&      // killough 2/28/98: Add offset checks
        yoffs == check->yoffs)
      return check;

  check = new_visplane(hash);         // killough

  check->height = height;
  check->picnum = picnum;
  check->lightlevel = lightlevel;
  check->minx = SCREENWIDTH; // Was SCREENWIDTH -- killough 11/98
  check->maxx = -1;
  check->xoffs = xoffs;               // killough 2/28/98: Save offsets
  check->yoffs = yoffs;

  memset (check->top, 0xff, sizeof check->top);

  return check;
}

//
// R_CheckPlane
//
visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
{
  int intrl, intrh, unionl, unionh, x;

  if (start < pl->minx)
    intrl   = pl->minx, unionl = start;
  else
    unionl  = pl->minx,  intrl = start;

  if (stop  > pl->maxx)
    intrh   = pl->maxx, unionh = stop;
  else
    unionh  = pl->maxx, intrh  = stop;

  for (x=intrl ; x <= intrh && pl->top[x] == 0xffffffffu; x++) // dropoff overflow
    ;

  if (x > intrh) { /* Can use existing plane; extend range */
    pl->minx = unionl; pl->maxx = unionh;
    return pl;
  } else /* Cannot use existing plane; create a new one */
    return R_DupPlane(pl,start,stop);
}

//
// R_MakeSpans
//

static void R_MakeSpans(int x, unsigned int t1, unsigned int b1,
                        unsigned int t2, unsigned int b2,
                        draw_span_vars_t *dsvars)
{
	for (; t1 < t2 && t1 <= b1; t1++)
        R_MapPlane(t1, _g->spanstart[t1], x-1, dsvars);
	
	for (; b1 > b2 && b1 >= t1; b1--)
        R_MapPlane(b1, _g->spanstart[b1] ,x-1, dsvars);
	
	while (t2 < t1 && t2 <= b2)
        _g->spanstart[t2++] = x;
	
	while (b2 > b1 && b2 >= t2)
        _g->spanstart[b2--] = x;
}

// New function, by Lee Killough

static void R_DoDrawPlane(visplane_t *pl)
{
	register int x;
	draw_column_vars_t dcvars;

	R_SetDefaultDrawColumnVars(&dcvars);

	if (pl->minx <= pl->maxx)
	{
        if (pl->picnum == _g->skyflatnum || pl->picnum & PL_SKYFLAT)
		{ // sky flat
			int texture;
			angle_t an, flip;

			// killough 10/98: allow skies to come from sidedefs.
			// Allows scrolling and/or animated skies, as well as
			// arbitrary multiple skies per level without having
			// to use info lumps.

            an = _g->viewangle;

			if (pl->picnum & PL_SKYFLAT)
			{
				// Sky Linedef
                const line_t *l = &_g->lines[pl->picnum & ~PL_SKYFLAT];

				// Sky transferred from first sidedef
                const side_t *s = *l->sidenum + _g->sides;
				
				// Texture comes from upper texture of reference sidedef
                texture = _g->texturetranslation[s->toptexture];

				// Horizontal offset is turned into an angle offset,
				// to allow sky rotation as well as careful positioning.
				// However, the offset is scaled very small, so that it
				// allows a long-period of sky rotation.

		        an += s->textureoffset;

				// Vertical offset allows careful sky positioning.

				dcvars.texturemid = s->rowoffset - 28*FRACUNIT;

				// We sometimes flip the picture horizontally.
				//
				// Doom always flipped the picture, so we make it optional,
				// to make it easier to use the new feature, while to still
				// allow old sky textures to be used.

				flip = l->special==272 ? 0u : ~0u;
			}
			else
			{    // Normal Doom sky, only one allowed per level
				dcvars.texturemid = skytexturemid;    // Default y-offset
                texture = _g->skytexture;             // Default texture
				flip = 0;                         // Doom flips it
			}

		  /* Sky is always drawn full bright, i.e. colormaps[0] is used.
		   * Because of this hack, sky is not affected by INVUL inverse mapping.
		   * Until Boom fixed this. Compat option added in MBF. */
			
            if (!(dcvars.colormap = _g->fixedcolormap))
                dcvars.colormap = _g->fullcolormap;          // killough 3/20/98
			
			// proff 09/21/98: Changed for high-res
            dcvars.iscale = FRACUNIT*200/viewheight;

            const int patchnum = _g->textures[texture]->patches[0].patch;
            const unsigned int widthmask = _g->textures[texture]->widthmask;
            const patch_t* patch = W_CacheLumpNum(patchnum);

			// killough 10/98: Use sky scrolling offset, and possibly flip picture
			for (x = pl->minx; (dcvars.x = x) <= pl->maxx; x++)
			{
				if ((dcvars.yl = pl->top[x]) != -1 && dcvars.yl <= (dcvars.yh = pl->bottom[x])) // dropoff overflow
				{
                    const int xc = (((an + xtoviewangle[x])^flip) >> ANGLETOSKYSHIFT) & widthmask;

                    const column_t* column = (const column_t *) ((const byte *)patch + patch->columnofs[xc]);

                    dcvars.source = (const byte*)column + 3;
					R_DrawColumn(&dcvars);
				}
			}
		}
		else
		{     // regular flat
			
			int stop, light;
			draw_span_vars_t dsvars;
			
            dsvars.source = W_CacheLumpNum(_g->firstflat + _g->flattranslation[pl->picnum]);
			
            _g->xoffs = pl->xoffs;  // killough 2/28/98: Add offsets
            _g->yoffs = pl->yoffs;
			
            _g->planeheight = D_abs(pl->height-_g->viewz);
            light = (pl->lightlevel >> LIGHTSEGSHIFT) + _g->extralight;
			
			if (light >= LIGHTLEVELS)
				light = LIGHTLEVELS-1;
			
			if (light < 0)
				light = 0;
			
			stop = pl->maxx + 1;
            _g->planezlight = _g->zlight[light];
			pl->top[pl->minx-1] = pl->top[stop] = 0xffffffffu; // dropoff overflow
			
			for (x = pl->minx ; x <= stop ; x++)
			{
				R_MakeSpans(x,pl->top[x-1],pl->bottom[x-1], pl->top[x],pl->bottom[x], &dsvars);
			}
				
            W_UnlockLumpNum(_g->firstflat + _g->flattranslation[pl->picnum]);
		}
	}
}

//
// RDrawPlanes
// At the end of each frame.
//

void R_DrawPlanes (void)
{
  visplane_t *pl;
  int i;
  for (i=0;i<MAXVISPLANES;i++)
    for (pl=_g->visplanes[i]; pl; pl=pl->next)
      R_DoDrawPlane(pl);
}
