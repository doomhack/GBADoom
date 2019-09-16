/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
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
 *      All the clipping: columns, horizontal spans, sky columns.
 *
 *-----------------------------------------------------------------------------*/
//
// 4/25/98, 5/2/98 killough: reformatted, beautified

#include "doomstat.h"
#include "r_main.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "r_plane.h"
#include "r_things.h"
#include "r_draw.h"
#include "w_wad.h"
#include "v_video.h"
#include "lprintf.h"

#include "global_data.h"

// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate


//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
// killough 5/2/98: reformatted, cleaned up
// CPhipps - moved here from r_main.c

static fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
  int     anglea = ANG90 + (visangle-_g->viewangle);
  int     angleb = ANG90 + (visangle-_g->rw_normalangle);

  int     den = FixedMul(_g->rw_distance, finesine[anglea>>ANGLETOFINESHIFT]);

// proff 11/06/98: Changed for high-res
  fixed_t num = FixedMul(_g->projectiony, finesine[angleb>>ANGLETOFINESHIFT]);

  return den > num>>16 ? (num = FixedDiv(num, den)) > 64*FRACUNIT ?
    64*FRACUNIT : num < 256 ? 256 : num : 64*FRACUNIT;
}

//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
  int      texnum;
  sector_t tempsec;      // killough 4/13/98
  const rpatch_t *patch;
  R_DrawColumn_f colfunc;
  draw_column_vars_t dcvars;

  R_SetDefaultDrawColumnVars(&dcvars);

  // Calculate light table.
  // Use different light tables
  //   for horizontal / vertical / diagonal. Diagonal?

  _g->curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally

  // killough 4/11/98: draw translucent 2s normal textures

  colfunc = R_DrawColumn;

  // killough 4/11/98: end translucent 2s normal code

  _g->frontsector = _g->curline->frontsector;
  _g->backsector = _g->curline->backsector;

  // cph 2001/11/25 - middle textures did not animate in v1.2
  texnum = _g->curline->sidedef->midtexture;

    texnum = _g->texturetranslation[texnum];

  // killough 4/13/98: get correct lightlevel for 2s normal textures
  _g->rw_lightlevel = R_FakeFlat(_g->frontsector, &tempsec, NULL, NULL, false) ->lightlevel;

  _g->maskedtexturecol = ds->maskedtexturecol;

  _g->rw_scalestep = ds->scalestep;
  spryscale = ds->scale1 + (x1 - ds->x1)*_g->rw_scalestep;
  mfloorclip = ds->sprbottomclip;
  mceilingclip = ds->sprtopclip;

  // find positioning
  if (_g->curline->linedef->flags & ML_DONTPEGBOTTOM)
    {
      dcvars.texturemid = _g->frontsector->floorheight > _g->backsector->floorheight
        ? _g->frontsector->floorheight : _g->backsector->floorheight;
      dcvars.texturemid = dcvars.texturemid + _g->textureheight[texnum] - _g->viewz;
    }
  else
    {
      dcvars.texturemid =_g->frontsector->ceilingheight<_g->backsector->ceilingheight
        ? _g->frontsector->ceilingheight : _g->backsector->ceilingheight;
      dcvars.texturemid = dcvars.texturemid - _g->viewz;
    }

  dcvars.texturemid += _g->curline->sidedef->rowoffset;

  if (_g->fixedcolormap) {
    dcvars.colormap = _g->fixedcolormap;
  }

  patch = R_CacheTextureCompositePatchNum(texnum);

  // draw the columns
  for (dcvars.x = x1 ; dcvars.x <= x2 ; dcvars.x++, spryscale += _g->rw_scalestep)
    if (_g->maskedtexturecol[dcvars.x] != INT_MAX) // dropoff overflow
      {
        
        if (!_g->fixedcolormap)
          dcvars.z = spryscale; // for filtering -- POPE
        dcvars.colormap = R_ColourMap(_g->rw_lightlevel,spryscale);

        // killough 3/2/98:
        //
        // This calculation used to overflow and cause crashes in Doom:
        //
        // sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);
        //
        // This code fixes it, by using double-precision intermediate
        // arithmetic and by skipping the drawing of 2s normals whose
        // mapping to screen coordinates is totally out of range:

        {
          int_64_t t = ((int_64_t) _g->centeryfrac << FRACBITS) -
            (int_64_t) dcvars.texturemid * spryscale;
          if (t + (int_64_t) _g->textureheight[texnum] * spryscale < 0 ||
              t > (int_64_t) MAX_SCREENHEIGHT << FRACBITS*2)
            continue;        // skip if the texture is out of screen's range
          sprtopscreen = (long)(t >> FRACBITS);
        }

        dcvars.iscale = 0xffffffffu / (unsigned) spryscale;

        // killough 1/25/98: here's where Medusa came in, because
        // it implicitly assumed that the column was all one patch.
        // Originally, Doom did not construct complete columns for
        // multipatched textures, so there were no header or trailer
        // bytes in the column referred to below, which explains
        // the Medusa effect. The fix is to construct true columns
        // when forming multipatched textures (see r_data.c).

        // draw the texture
        R_DrawMaskedColumn(
          patch,
          colfunc,
          &dcvars,
          R_GetPatchColumnWrapped(patch, _g->maskedtexturecol[dcvars.x])
        );

        _g->maskedtexturecol[dcvars.x] = INT_MAX; // dropoff overflow
      }

  R_UnlockTextureCompositePatchNum(texnum);

  _g->curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)

static void R_RenderSegLoop (void)
{
  const rpatch_t *tex_patch;

  draw_column_vars_t dcvars;
  fixed_t  texturecolumn = 0;   // shut up compiler warning

  R_SetDefaultDrawColumnVars(&dcvars);

  for ( ; _g->rw_x < _g->rw_stopx ; _g->rw_x++)
    {

       // mark floor / ceiling areas

      int yh = _g->bottomfrac>>HEIGHTBITS;
      int yl = (_g->topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

      // no space above wall?
      int bottom,top = _g->ceilingclip[_g->rw_x]+1;

      if (yl < top)
        yl = top;

      if (_g->markceiling)
        {
          bottom = yl-1;

          if (bottom >= _g->floorclip[_g->rw_x])
            bottom = _g->floorclip[_g->rw_x]-1;

          if (top <= bottom)
            {
              _g->ceilingplane->top[_g->rw_x] = top;
              _g->ceilingplane->bottom[_g->rw_x] = bottom;
            }
          // SoM: this should be set here
          _g->ceilingclip[_g->rw_x] = bottom;
        }

//      yh = bottomfrac>>HEIGHTBITS;

      bottom = _g->floorclip[_g->rw_x]-1;
      if (yh > bottom)
        yh = bottom;

      if (_g->markfloor)
        {

          top  = yh < _g->ceilingclip[_g->rw_x] ? _g->ceilingclip[_g->rw_x] : yh;

          if (++top <= bottom)
            {
              _g->floorplane->top[_g->rw_x] = top;
              _g->floorplane->bottom[_g->rw_x] = bottom;
            }
          // SoM: This should be set here to prevent overdraw
          _g->floorclip[_g->rw_x] = top;
        }

      // texturecolumn and lighting are independent of wall tiers
      if (_g->segtextured)
        {
          // calculate texture offset
          angle_t angle =(_g->rw_centerangle+_g->xtoviewangle[_g->rw_x])>>ANGLETOFINESHIFT;

          texturecolumn = _g->rw_offset-FixedMul(finetangent[angle],_g->rw_distance);

		  texturecolumn >>= FRACBITS;

          dcvars.colormap = R_ColourMap(_g->rw_lightlevel,_g->rw_scale);
          dcvars.z = _g->rw_scale; // for filtering -- POPE

          dcvars.x = _g->rw_x;
          dcvars.iscale = 0xffffffffu / (unsigned)_g->rw_scale;
        }

      // draw the wall tiers
      if (_g->midtexture)
        {

          dcvars.yl = yl;     // single sided line
          dcvars.yh = yh;
          dcvars.texturemid = _g->rw_midtexturemid;
          tex_patch = R_CacheTextureCompositePatchNum(_g->midtexture);
          dcvars.source = R_GetTextureColumn(tex_patch, texturecolumn);
          R_DrawColumn (&dcvars);
          R_UnlockTextureCompositePatchNum(_g->midtexture);
          tex_patch = NULL;
          _g->ceilingclip[_g->rw_x] = _g->viewheight;
          _g->floorclip[_g->rw_x] = -1;
        }
      else
        {

          // two sided line
          if (_g->toptexture)
            {
              // top wall
              int mid = _g->pixhigh>>HEIGHTBITS;
              _g->pixhigh += _g->pixhighstep;

              if (mid >= _g->floorclip[_g->rw_x])
                mid = _g->floorclip[_g->rw_x]-1;

              if (mid >= yl)
                {
                  dcvars.yl = yl;
                  dcvars.yh = mid;
                  dcvars.texturemid = _g->rw_toptexturemid;
                  tex_patch = R_CacheTextureCompositePatchNum(_g->toptexture);
                  dcvars.source = R_GetTextureColumn(tex_patch,texturecolumn);
                  R_DrawColumn (&dcvars);
                  R_UnlockTextureCompositePatchNum(_g->toptexture);
                  tex_patch = NULL;
                  _g->ceilingclip[_g->rw_x] = mid;
                }
              else
                _g->ceilingclip[_g->rw_x] = yl-1;
            }
          else  // no top wall
            {

            if (_g->markceiling)
              _g->ceilingclip[_g->rw_x] = yl-1;
             }

          if (_g->bottomtexture)          // bottom wall
            {
              int mid = (_g->pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
              _g->pixlow += _g->pixlowstep;

              // no space above wall?
              if (mid <= _g->ceilingclip[_g->rw_x])
                mid = _g->ceilingclip[_g->rw_x]+1;

              if (mid <= yh)
                {
                  dcvars.yl = mid;
                  dcvars.yh = yh;
                  dcvars.texturemid = _g->rw_bottomtexturemid;
                  tex_patch = R_CacheTextureCompositePatchNum(_g->bottomtexture);
                  dcvars.source = R_GetTextureColumn(tex_patch, texturecolumn);
                  R_DrawColumn  (&dcvars);
                  R_UnlockTextureCompositePatchNum(_g->bottomtexture);
                  tex_patch = NULL;
                  _g->floorclip[_g->rw_x] = mid;
                }
              else
                _g->floorclip[_g->rw_x] = yh+1;
            }
          else        // no bottom wall
            {
            if (_g->markfloor)
              _g->floorclip[_g->rw_x] = yh+1;
            }

    // cph - if we completely blocked further sight through this column,
    // add this info to the solid columns array for r_bsp.c
    if ((_g->markceiling || _g->markfloor) &&
        (_g->floorclip[_g->rw_x] <= _g->ceilingclip[_g->rw_x] + 1)) {
      _g->solidcol[_g->rw_x] = 1; _g->didsolidcol = 1;
    }

          // save texturecol for backdrawing of masked mid texture
          if (_g->maskedtexture)
            _g->maskedtexturecol[_g->rw_x] = texturecolumn;
        }

      _g->rw_scale += _g->rw_scalestep;
      _g->topfrac += _g->topstep;
      _g->bottomfrac += _g->bottomstep;
    }
}

// killough 5/2/98: move from r_main.c, made static, simplified

static CONSTFUNC fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
  fixed_t dx = D_abs(x - _g->viewx);
  fixed_t dy = D_abs(y - _g->viewy);

  if (dy > dx)
    {
      fixed_t t = dx;
      dx = dy;
      dy = t;
    }

  return FixedDiv(dx, finesine[(tantoangle[FixedDiv(dy,dx) >> DBITS]
                                + ANG90) >> ANGLETOFINESHIFT]);
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange(const int start, const int stop)
{
  fixed_t hyp;
  angle_t offsetangle;

  if (_g->ds_p == _g->drawsegs+_g->maxdrawsegs)   // killough 1/98 -- fix 2s line HOM
    {
      unsigned pos = _g->ds_p - _g->drawsegs; // jff 8/9/98 fix from ZDOOM1.14a
      unsigned newmax = _g->maxdrawsegs ? _g->maxdrawsegs*2 : 128; // killough
      _g->drawsegs = realloc(_g->drawsegs,newmax*sizeof(*_g->drawsegs));
      _g->ds_p = _g->drawsegs + pos;          // jff 8/9/98 fix from ZDOOM1.14a
      _g->maxdrawsegs = newmax;
    }

  if(_g->curline->miniseg == false) // figgi -- skip minisegs
    _g->curline->linedef->flags |= ML_MAPPED;

#ifdef RANGECHECK
  if (start >=viewwidth || start > stop)
    I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

  _g->sidedef = _g->curline->sidedef;
  _g->linedef = _g->curline->linedef;

  // mark the segment as visible for auto map
  _g->linedef->flags |= ML_MAPPED;

  // calculate rw_distance for scale calculation
  _g->rw_normalangle = _g->curline->angle + ANG90;

  offsetangle = _g->rw_normalangle-_g->rw_angle1;

  if (D_abs(offsetangle) > ANG90)
    offsetangle = ANG90;

  hyp = (_g->viewx==_g->curline->v1->x && _g->viewy==_g->curline->v1->y)?
    0 : R_PointToDist (_g->curline->v1->x, _g->curline->v1->y);
  _g->rw_distance = FixedMul(hyp, finecosine[offsetangle>>ANGLETOFINESHIFT]);

  _g->ds_p->x1 = _g->rw_x = start;
  _g->ds_p->x2 = stop;
  _g->ds_p->curline = _g->curline;
  _g->rw_stopx = stop+1;

  {     // killough 1/6/98, 2/1/98: remove limit on openings
    size_t pos = _g->lastopening - _g->openings;
    size_t need = (_g->rw_stopx - start)*4 + pos;
    if (need > _g->maxopenings)
      {
        drawseg_t *ds;                //jff 8/9/98 needed for fix from ZDoom
        int *oldopenings = _g->openings; // dropoff overflow
        int *oldlast = _g->lastopening; // dropoff overflow

        do
          _g->maxopenings = _g->maxopenings ?_g-> maxopenings*2 : 16384;
        while (need > _g->maxopenings);
        _g->openings = realloc(_g->openings, _g->maxopenings * sizeof(*_g->openings));
        _g->lastopening = _g->openings + pos;

      // jff 8/9/98 borrowed fix for openings from ZDOOM1.14
      // [RH] We also need to adjust the openings pointers that
      //    were already stored in drawsegs.
      for (ds = _g->drawsegs; ds < _g->ds_p; ds++)
        {
#define ADJUST(p) if (ds->p + ds->x1 >= oldopenings && ds->p + ds->x1 <= oldlast)\
            ds->p = ds->p - oldopenings + _g->openings;
          ADJUST (maskedtexturecol);
          ADJUST (sprtopclip);
          ADJUST (sprbottomclip);
        }
#undef ADJUST
      }
  }  // killough: end of code to remove limits on openings

  // calculate scale at both ends and step

  _g->ds_p->scale1 = _g->rw_scale =
    R_ScaleFromGlobalAngle (_g->viewangle + _g->xtoviewangle[start]);

  if (stop > start)
    {
      _g->ds_p->scale2 = R_ScaleFromGlobalAngle (_g->viewangle + _g->xtoviewangle[stop]);
      _g->ds_p->scalestep = _g->rw_scalestep = (_g->ds_p->scale2-_g->rw_scale) / (stop-start);
    }
  else
    _g->ds_p->scale2 = _g->ds_p->scale1;

  // calculate texture boundaries
  //  and decide if floor / ceiling marks are needed

  _g->worldtop = _g->frontsector->ceilingheight - _g->viewz;
  _g->worldbottom = _g->frontsector->floorheight - _g->viewz;

  _g->midtexture = _g->toptexture = _g->bottomtexture = _g->maskedtexture = 0;
  _g->ds_p->maskedtexturecol = NULL;

  if (!_g->backsector)
    {
      // single sided line
      _g->midtexture = _g->texturetranslation[_g->sidedef->midtexture];

      // a single sided line is terminal, so it must mark ends
      _g->markfloor = _g->markceiling = true;

      if (_g->linedef->flags & ML_DONTPEGBOTTOM)
        {         // bottom of texture at bottom
          fixed_t vtop = _g->frontsector->floorheight +
            _g->textureheight[_g->sidedef->midtexture];
          _g->rw_midtexturemid = vtop - _g->viewz;
        }
      else        // top of texture at top
        _g->rw_midtexturemid = _g->worldtop;

      _g->rw_midtexturemid += FixedMod(_g->sidedef->rowoffset, _g->textureheight[_g->midtexture]);

      _g->ds_p->silhouette = SIL_BOTH;
      _g->ds_p->sprtopclip = screenheightarray;
      _g->ds_p->sprbottomclip = negonearray;
      _g->ds_p->bsilheight = INT_MAX;
      _g->ds_p->tsilheight = INT_MIN;
    }
  else      // two sided line
    {
      _g->ds_p->sprtopclip = _g->ds_p->sprbottomclip = NULL;
      _g->ds_p->silhouette = 0;

      if (_g->linedef->r_flags & RF_CLOSED) { /* cph - closed 2S line e.g. door */
  // cph - killough's (outdated) comment follows - this deals with both
  // "automap fixes", his and mine
  // killough 1/17/98: this test is required if the fix
  // for the automap bug (r_bsp.c) is used, or else some
  // sprites will be displayed behind closed doors. That
  // fix prevents lines behind closed doors with dropoffs
  // from being displayed on the automap.

  _g->ds_p->silhouette = SIL_BOTH;
  _g->ds_p->sprbottomclip = negonearray;
  _g->ds_p->bsilheight = INT_MAX;
  _g->ds_p->sprtopclip = screenheightarray;
  _g->ds_p->tsilheight = INT_MIN;

      } else { /* not solid - old code */

  if (_g->frontsector->floorheight > _g->backsector->floorheight)
    {
      _g->ds_p->silhouette = SIL_BOTTOM;
      _g->ds_p->bsilheight = _g->frontsector->floorheight;
    }
  else
    if (_g->backsector->floorheight > _g->viewz)
      {
        _g->ds_p->silhouette = SIL_BOTTOM;
        _g->ds_p->bsilheight = INT_MAX;
      }

  if (_g->frontsector->ceilingheight < _g->backsector->ceilingheight)
    {
      _g->ds_p->silhouette |= SIL_TOP;
      _g->ds_p->tsilheight = _g->frontsector->ceilingheight;
    }
  else
    if (_g->backsector->ceilingheight < _g->viewz)
      {
        _g->ds_p->silhouette |= SIL_TOP;
        _g->ds_p->tsilheight = INT_MIN;
      }
      }

      _g->worldhigh = _g->backsector->ceilingheight - _g->viewz;
      _g->worldlow = _g->backsector->floorheight - _g->viewz;

      // hack to allow height changes in outdoor areas
      if (_g->frontsector->ceilingpic == _g->skyflatnum
          && _g->backsector->ceilingpic == _g->skyflatnum)
        _g->worldtop = _g->worldhigh;

      _g->markfloor = _g->worldlow != _g->worldbottom
        || _g->backsector->floorpic != _g->frontsector->floorpic
        || _g->backsector->lightlevel != _g->frontsector->lightlevel

        // killough 3/7/98: Add checks for (x,y) offsets
        || _g->backsector->floor_xoffs != _g->frontsector->floor_xoffs
        || _g->backsector->floor_yoffs != _g->frontsector->floor_yoffs

        // killough 4/15/98: prevent 2s normals
        // from bleeding through deep water
        || _g->frontsector->heightsec != -1

        // killough 4/17/98: draw floors if different light levels
        || _g->backsector->floorlightsec != _g->frontsector->floorlightsec
        ;

      _g->markceiling = _g->worldhigh != _g->worldtop
        || _g->backsector->ceilingpic != _g->frontsector->ceilingpic
        || _g->backsector->lightlevel != _g->frontsector->lightlevel

        // killough 3/7/98: Add checks for (x,y) offsets
        || _g->backsector->ceiling_xoffs != _g->frontsector->ceiling_xoffs
        || _g->backsector->ceiling_yoffs != _g->frontsector->ceiling_yoffs

        // killough 4/15/98: prevent 2s normals
        // from bleeding through fake ceilings
        || (_g->frontsector->heightsec != -1 &&
            _g->frontsector->ceilingpic!=_g->skyflatnum)

        // killough 4/17/98: draw ceilings if different light levels
        || _g->backsector->ceilinglightsec != _g->frontsector->ceilinglightsec
        ;

      if (_g->backsector->ceilingheight <= _g->frontsector->floorheight
          || _g->backsector->floorheight >= _g->frontsector->ceilingheight)
        _g->markceiling = _g->markfloor = true;   // closed door

      if (_g->worldhigh < _g->worldtop)   // top texture
      {
          _g->toptexture = _g->texturetranslation[_g->sidedef->toptexture];
          _g->rw_toptexturemid = _g->linedef->flags & ML_DONTPEGTOP ? _g->worldtop :
                                                                      _g->backsector->ceilingheight+_g->textureheight[_g->sidedef->toptexture]-_g->viewz;
          _g->rw_toptexturemid += FixedMod(_g->sidedef->rowoffset, _g->textureheight[_g->toptexture]);
      }

      if (_g->worldlow > _g->worldbottom) // bottom texture
      {
          _g->bottomtexture = _g->texturetranslation[_g->sidedef->bottomtexture];
          _g->rw_bottomtexturemid = _g->linedef->flags & ML_DONTPEGBOTTOM ? _g->worldtop :
                                                                            _g->worldlow;
          _g->rw_bottomtexturemid += FixedMod(_g->sidedef->rowoffset, _g->textureheight[_g->bottomtexture]);
      }

      // allocate space for masked texture tables
      if (_g->sidedef->midtexture)    // masked midtexture
        {
          _g->maskedtexture = true;
          _g->ds_p->maskedtexturecol = _g->maskedtexturecol = _g->lastopening - _g->rw_x;
          _g->lastopening += _g->rw_stopx - _g->rw_x;
        }
    }

  // calculate rw_offset (only needed for textured lines)
  _g->segtextured = _g->midtexture | _g->toptexture | _g->bottomtexture | _g->maskedtexture;

  if (_g->segtextured)
    {
      _g->rw_offset = FixedMul (hyp, -finesine[offsetangle >>ANGLETOFINESHIFT]);

      _g->rw_offset += _g->sidedef->textureoffset + _g->curline->offset;

      _g->rw_centerangle = ANG90 + _g->viewangle - _g->rw_normalangle;

      _g->rw_lightlevel = _g->frontsector->lightlevel;
    }

  // Remember the vars used to determine fractional U texture
  // coords for later - POPE
  _g->ds_p->rw_offset = _g->rw_offset;
  _g->ds_p->rw_distance = _g->rw_distance;
  _g->ds_p->rw_centerangle = _g->rw_centerangle;
      
  // if a floor / ceiling plane is on the wrong side of the view
  // plane, it is definitely invisible and doesn't need to be marked.

  // killough 3/7/98: add deep water check
  if (_g->frontsector->heightsec == -1)
    {
      if (_g->frontsector->floorheight >= _g->viewz)       // above view plane
        _g->markfloor = false;
      if (_g->frontsector->ceilingheight <= _g->viewz &&
          _g->frontsector->ceilingpic != _g->skyflatnum)   // below view plane
        _g->markceiling = false;
    }

  // calculate incremental stepping values for texture edges
  _g->worldtop >>= 4;
  _g->worldbottom >>= 4;

  _g->topstep = -FixedMul (_g->rw_scalestep, _g->worldtop);
  _g->topfrac = (_g->centeryfrac>>4) - FixedMul (_g->worldtop, _g->rw_scale);

  _g->bottomstep = -FixedMul (_g->rw_scalestep,_g->worldbottom);
  _g->bottomfrac = (_g->centeryfrac>>4) - FixedMul (_g->worldbottom, _g->rw_scale);

  if (_g->backsector)
    {
      _g->worldhigh >>= 4;
      _g->worldlow >>= 4;

      if (_g->worldhigh < _g->worldtop)
        {
          _g->pixhigh = (_g->centeryfrac>>4) - FixedMul (_g->worldhigh, _g->rw_scale);
          _g->pixhighstep = -FixedMul (_g->rw_scalestep,_g->worldhigh);
        }
      if (_g->worldlow > _g->worldbottom)
        {
          _g->pixlow = (_g->centeryfrac>>4) - FixedMul (_g->worldlow, _g->rw_scale);
          _g->pixlowstep = -FixedMul (_g->rw_scalestep,_g->worldlow);
        }
    }

  // render it
  if (_g->markceiling) {
    if (_g->ceilingplane)   // killough 4/11/98: add NULL ptr checks
      _g->ceilingplane = R_CheckPlane (_g->ceilingplane, _g->rw_x, _g->rw_stopx-1);
    else
      _g->markceiling = 0;
  }

  if (_g->markfloor) {
    if (_g->floorplane)     // killough 4/11/98: add NULL ptr checks
      /* cph 2003/04/18  - ceilingplane and floorplane might be the same
       * visplane (e.g. if both skies); R_CheckPlane doesn't know about
       * modifications to the plane that might happen in parallel with the check
       * being made, so we have to override it and split them anyway if that is
       * a possibility, otherwise the floor marking would overwrite the ceiling
       * marking, resulting in HOM. */
      if (_g->markceiling && _g->ceilingplane == _g->floorplane)
    _g->floorplane = R_DupPlane (_g->floorplane, _g->rw_x, _g->rw_stopx-1);
      else
    _g->floorplane = R_CheckPlane (_g->floorplane, _g->rw_x, _g->rw_stopx-1);
    else
      _g->markfloor = 0;
  }

  _g->didsolidcol = 0;
  R_RenderSegLoop();

  /* cph - if a column was made solid by this wall, we _must_ save full clipping info */
  if (_g->backsector && _g->didsolidcol) {
    if (!(_g->ds_p->silhouette & SIL_BOTTOM)) {
      _g->ds_p->silhouette |= SIL_BOTTOM;
      _g->ds_p->bsilheight = _g->backsector->floorheight;
    }
    if (!(_g->ds_p->silhouette & SIL_TOP)) {
      _g->ds_p->silhouette |= SIL_TOP;
      _g->ds_p->tsilheight = _g->backsector->ceilingheight;
    }
  }

  // save sprite clipping info
  if ((_g->ds_p->silhouette & SIL_TOP || _g->maskedtexture) && !_g->ds_p->sprtopclip)
    {
      memcpy (_g->lastopening, _g->ceilingclip+start, sizeof(int)*(_g->rw_stopx-start)); // dropoff overflow
      _g->ds_p->sprtopclip = _g->lastopening - start;
      _g->lastopening += _g->rw_stopx - start;
    }
  if ((_g->ds_p->silhouette & SIL_BOTTOM || _g->maskedtexture) && !_g->ds_p->sprbottomclip)
    {
      memcpy (_g->lastopening, _g->floorclip+start, sizeof(int)*(_g->rw_stopx-start)); // dropoff overflow
      _g->ds_p->sprbottomclip = _g->lastopening - start;
      _g->lastopening += _g->rw_stopx - start;
    }
  if (_g->maskedtexture && !(_g->ds_p->silhouette & SIL_TOP))
    {
      _g->ds_p->silhouette |= SIL_TOP;
      _g->ds_p->tsilheight = INT_MIN;
    }
  if (_g->maskedtexture && !(_g->ds_p->silhouette & SIL_BOTTOM))
    {
      _g->ds_p->silhouette |= SIL_BOTTOM;
      _g->ds_p->bsilheight = INT_MAX;
    }
  _g->ds_p++;

}
