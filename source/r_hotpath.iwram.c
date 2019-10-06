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



//*****************************************
//Column cache stuff.
//GBA has 16kb of Video Memory for columns
//*****************************************

#ifndef __arm__
static byte columnCache[128*128];
#else
extern byte* columnCache;
#endif

typedef struct column_cache_entry_t
{
    unsigned short texture;
    unsigned short column;
}column_cache_entry_t;

static column_cache_entry_t columnCacheEntries[128];


// killough 5/3/98: reformatted

CONSTFUNC int SlopeDiv(unsigned num, unsigned den)
{
  unsigned ans;

  if (den < 512)
    return SLOPERANGE;

  ans = IDiv32(num<<3, den>>8);

  return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//
angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    x -= _g->viewx;
    y -= _g->viewy;

    if ( (!x) && (!y) )
        return 0;

    if (x>= 0)
    {
        // x >=0
        if (y>= 0)
        {
            // y>= 0

            if (x>y)
            {
                // octant 0
                return tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 1
                return ANG90-1-tantoangle[ SlopeDiv(x,y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 8
                return -tantoangle[SlopeDiv(y,x)];
            }
            else
            {
                // octant 7
                return ANG270+tantoangle[ SlopeDiv(x,y)];
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (y>= 0)
        {
            // y>= 0
            if (x>y)
            {
                // octant 3
                return ANG180-1-tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 2
                return ANG90+ tantoangle[ SlopeDiv(x,y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 4
                return ANG180+tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 5
                return ANG270-1-tantoangle[ SlopeDiv(x,y)];
            }
        }
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

static inline int between(int l,int u,int x)
{ return (l > x ? l : x > u ? u : x); }

static const lighttable_t* R_ColourMap(int lightlevel, fixed_t spryscale)
{
  if (_g->fixedcolormap)
      return _g->fixedcolormap;
  else
  {
    if (_g->curline)
    {
        if (_g->curline->v1->y == _g->curline->v2->y)
          lightlevel -= 1 << LIGHTSEGSHIFT;
        else
          if (_g->curline->v1->x == _g->curline->v2->x)
            lightlevel += 1 << LIGHTSEGSHIFT;
    }

    lightlevel += _g->extralight << LIGHTSEGSHIFT;

    /* cph 2001/11/17 -
     * Work out what colour map to use, remembering to clamp it to the number of
     * colour maps we actually have. This formula is basically the one from the
     * original source, just brought into one place. The main difference is it
     * throws away less precision in the lightlevel half, so it supports 32
     * light levels in WADs compared to Doom's 16.
     *
     * Note we can make it more accurate if we want - we should keep all the
     * precision until the final step, so slight scale differences can count
     * against slight light level variations.
     */
    return _g->fullcolormap + between(0,NUMCOLORMAPS-1, ((256-lightlevel)*2*NUMCOLORMAPS/256) - 4 - (FixedMul(spryscale,pspriteiscale)/2 >> LIGHTSCALESHIFT))*256;
  }
}




//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
static void R_DrawMaskedColumn(R_DrawColumn_f colfunc, draw_column_vars_t *dcvars, const column_t *column)
{
    int     topscreen;
    int     bottomscreen;
    fixed_t basetexturemid = dcvars->texturemid;

    for ( ; column->topdelta != 0xff ; )
    {
        // calculate unclipped screen coordinates for post
        topscreen = _g->sprtopscreen + _g->spryscale*column->topdelta;
        bottomscreen = topscreen + _g->spryscale*column->length;

        dcvars->yl = (topscreen+FRACUNIT-1)>>FRACBITS;
        dcvars->yh = (bottomscreen-1)>>FRACBITS;

        if (dcvars->yh >= _g->mfloorclip[dcvars->x])
            dcvars->yh = _g->mfloorclip[dcvars->x]-1;

        if (dcvars->yl <= _g->mceilingclip[dcvars->x])
            dcvars->yl = _g->mceilingclip[dcvars->x]+1;

        // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
        if (dcvars->yl <= dcvars->yh && dcvars->yh < viewheight)
        {
            dcvars->source =  (const byte*)column + 3;

            dcvars->texturemid = basetexturemid - (column->topdelta<<FRACBITS);

            // Drawn by either R_DrawColumn
            //  or (SHADOW) R_DrawFuzzColumn.
            colfunc (dcvars);

        }

        column = (const column_t *)((const byte *)column + column->length + 4);
    }
    dcvars->texturemid = basetexturemid;
}

//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
// CPhipps - new wad lump handling, *'s to const*'s
static void R_DrawVisSprite(vissprite_t *vis, int x1, int x2)
{
    int      texturecolumn;
    fixed_t  frac;
    const patch_t *patch = W_CacheLumpNum(vis->patch+_g->firstspritelump);

    R_DrawColumn_f colfunc;
    draw_column_vars_t dcvars;


    R_SetDefaultDrawColumnVars(&dcvars);

    dcvars.colormap = vis->colormap;

    // killough 4/11/98: rearrange and handle translucent sprites
    // mixed with translucent/non-translucenct 2s normals

    if (!dcvars.colormap)   // NULL colormap = shadow draw
        colfunc = R_DrawFuzzColumn;    // killough 3/14/98
    else
    {
        if (vis->mobjflags & MF_TRANSLATION)
        {
            colfunc = R_DrawTranslatedColumn;
            dcvars.translation = translationtables +
                    ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
        }
        else
            colfunc = R_DrawColumn; // killough 3/14/98, 4/11/98
    }

    // proff 11/06/98: Changed for high-res
    dcvars.iscale = FixedDiv (FRACUNIT, vis->scale);
    dcvars.texturemid = vis->texturemid;
    frac = vis->startfrac;

    _g->spryscale = vis->scale;
    _g->sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid,_g->spryscale);

    for (dcvars.x=vis->x1 ; dcvars.x<=vis->x2 ; dcvars.x++, frac += vis->xiscale)
    {
        texturecolumn = frac>>FRACBITS;

        const column_t* column = (const column_t *) ((const byte *)patch + patch->columnofs[texturecolumn]);

        R_DrawMaskedColumn(colfunc, &dcvars, column);
    }
}


static const column_t* R_GetColumn(const texture_t* texture, int texcolumn)
{
    if(texture->patchcount == 1)
    {
        //simple texture.

        const unsigned int widthmask = texture->widthmask;
        const patch_t* patch = texture->patches[0].patch;

        const unsigned int xc = texcolumn & widthmask;

        return (const column_t *) ((const byte *)patch + patch->columnofs[xc]);
    }
    else
    {
        const patch_t* realpatch;

        const int xc = (texcolumn & 0xffff) & texture->widthmask;

        for(int i=0; i<texture->patchcount; i++)
        {
            const texpatch_t* patch = &texture->patches[i];

            realpatch = patch->patch;

            int x1 = patch->originx;
            int x2 = x1 + realpatch->width;

            if (x2 > texture->width)
                x2 = texture->width;

            if(xc >= x1 && xc < x2)
            {
                return (const column_t *)((const byte *)realpatch + realpatch->columnofs[xc-x1]);
            }
        }
    }

    return NULL;
}

//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
    int      texnum;
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

    texnum = _g->curline->sidedef->midtexture;
    texnum = _g->texturetranslation[texnum];

    // killough 4/13/98: get correct lightlevel for 2s normal textures
    _g->rw_lightlevel = _g->frontsector->lightlevel;

    _g->maskedtexturecol = ds->maskedtexturecol;

    _g->rw_scalestep = ds->scalestep;
    _g->spryscale = ds->scale1 + (x1 - ds->x1)*_g->rw_scalestep;
    _g->mfloorclip = ds->sprbottomclip;
    _g->mceilingclip = ds->sprtopclip;

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

    const texture_t* texture = _g->textures[texnum];

    // draw the columns
    for (dcvars.x = x1 ; dcvars.x <= x2 ; dcvars.x++, _g->spryscale += _g->rw_scalestep)
    {
        if (_g->maskedtexturecol[dcvars.x] != INT_MAX) // dropoff overflow
        {

            if (!_g->fixedcolormap)
                dcvars.z = _g->spryscale; // for filtering -- POPE
            dcvars.colormap = R_ColourMap(_g->rw_lightlevel,_g->spryscale);

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
                int_64_t t = ((int_64_t) centeryfrac << FRACBITS) -
                        (int_64_t) dcvars.texturemid * _g->spryscale;
                if (t + (int_64_t) _g->textureheight[texnum] * _g->spryscale < 0 ||
                        t > (int_64_t) MAX_SCREENHEIGHT << FRACBITS*2)
                    continue;        // skip if the texture is out of screen's range
                _g->sprtopscreen = (long)(t >> FRACBITS);
            }

            dcvars.iscale = 0xffffffffu / (unsigned) _g->spryscale;

            // killough 1/25/98: here's where Medusa came in, because
            // it implicitly assumed that the column was all one patch.
            // Originally, Doom did not construct complete columns for
            // multipatched textures, so there were no header or trailer
            // bytes in the column referred to below, which explains
            // the Medusa effect. The fix is to construct true columns
            // when forming multipatched textures (see r_data.c).

            // draw the texture

            int xc = _g->maskedtexturecol[dcvars.x];

            const column_t* column = R_GetColumn(texture, xc);

            R_DrawMaskedColumn(colfunc, &dcvars, column);

            _g->maskedtexturecol[dcvars.x] = INT_MAX; // dropoff overflow
        }
    }

    _g->curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
}


// killough 5/2/98: reformatted

static PUREFUNC int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
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
// R_DrawSprite
//

static void R_DrawSprite (vissprite_t* spr)
{
    drawseg_t *ds;
    int     clipbot[MAX_SCREENWIDTH]; // killough 2/8/98: // dropoff overflow
    int     cliptop[MAX_SCREENWIDTH]; // change to MAX_*  // dropoff overflow
    int     x;
    int     r1;
    int     r2;
    fixed_t scale;
    fixed_t lowscale;

    for (x = spr->x1 ; x<=spr->x2 ; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale is the clip seg.

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):

    for (ds=_g->ds_p ; ds-- > _g->drawsegs ; )  // new -- killough
    {      // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
                (!ds->silhouette && !ds->maskedtexturecol))
            continue;      // does not cover sprite

        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr->scale || (lowscale < spr->scale &&
                                   !R_PointOnSegSide (spr->gx, spr->gy, ds->curline)))
        {
            if (ds->maskedtexturecol)       // masked mid texture?
                R_RenderMaskedSegRange(ds, r1, r2);
            continue;               // seg is behind sprite
        }

        // clip this piece of the sprite
        // killough 3/27/98: optimized and made much shorter

        if (ds->silhouette&SIL_BOTTOM && spr->gz < ds->bsilheight) //bottom sil
            for (x=r1 ; x<=r2 ; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];

        if (ds->silhouette&SIL_TOP && spr->gzt > ds->tsilheight)   // top sil
            for (x=r1 ; x<=r2 ; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
    }

    // all clipping has been performed, so draw the sprite
    // check for unclipped columns

    for (x = spr->x1 ; x<=spr->x2 ; x++) {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;

        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }

    _g->mfloorclip = clipbot;
    _g->mceilingclip = cliptop;
    R_DrawVisSprite (spr, spr->x1, spr->x2);
}


//
// R_DrawPSprite
//

static void R_DrawPSprite (pspdef_t *psp, int lightlevel)
{
    int           x1, x2;
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    int           lump;
    boolean       flip;
    vissprite_t   *vis;
    vissprite_t   avis;
    int           width;
    fixed_t       topoffset;

    // decide which patch to use

#ifdef RANGECHECK
    if ( (unsigned)psp->state->sprite >= (unsigned)numsprites)
        I_Error ("R_ProjectSprite: Invalid sprite number %i", psp->state->sprite);
#endif

    sprdef = &_g->sprites[psp->state->sprite];

#ifdef RANGECHECK
    if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
        I_Error ("R_ProjectSprite: Invalid sprite frame %i : %li",
                 psp->state->sprite, psp->state->frame);
#endif

    sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

    lump = sprframe->lump[0];
    flip = (boolean) sprframe->flip[0];

    {
        const patch_t* patch = W_CacheLumpNum(lump+_g->firstspritelump);
        // calculate edges of the shape
        fixed_t       tx;
        tx = psp->sx-160*FRACUNIT;

        tx -= patch->leftoffset<<FRACBITS;
        x1 = (centerxfrac + FixedMul (tx, pspritescale))>>FRACBITS;

        tx += patch->width<<FRACBITS;
        x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;

        width = patch->width;
        topoffset = patch->topoffset<<FRACBITS;
        //R_UnlockPatchNum(lump+_g->firstspritelump);
    }

    // off the side
    if (x2 < 0 || x1 > SCREENWIDTH)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    // killough 12/98: fix psprite positioning problem
    vis->texturemid = (BASEYCENTER<<FRACBITS) /* +  FRACUNIT/2 */ -
            (psp->sy-topoffset);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= SCREENWIDTH ? SCREENWIDTH-1 : x2;
    // proff 11/06/98: Added for high-res
    vis->scale = pspriteyscale;

    if (flip)
    {
        vis->xiscale = - pspriteiscale;
        vis->startfrac = (width<<FRACBITS)-1;
    }
    else
    {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale*(vis->x1-x1);

    vis->patch = lump;

    if (_g->viewplayer->powers[pw_invisibility] > 4*32
            || _g->viewplayer->powers[pw_invisibility] & 8)
        vis->colormap = NULL;                    // shadow draw
    else if (_g->fixedcolormap)
        vis->colormap = _g->fixedcolormap;           // fixed color
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis->colormap = _g->fullcolormap;            // full bright // killough 3/20/98
    else
        // add a fudge factor to better match the original game
        vis->colormap = R_ColourMap(lightlevel,
                                    FixedMul(pspritescale, 0x2b000));  // local light

    R_DrawVisSprite(vis, vis->x1, vis->x2);
}



//
// R_DrawPlayerSprites
//

static void R_DrawPlayerSprites(void)
{
  int i, lightlevel = _g->viewplayer->mo->subsector->sector->lightlevel;
  pspdef_t *psp;

  // clip to screen bounds
  _g->mfloorclip = screenheightarray;
  _g->mceilingclip = negonearray;

  // add all active psprites
  for (i=0, psp=_g->viewplayer->psprites; i<NUMPSPRITES; i++,psp++)
    if (psp->state)
      R_DrawPSprite (psp, lightlevel);
}


//
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
//

#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))

// killough 9/2/98: merge sort

static void msort(vissprite_t **s, vissprite_t **t, int n)
{
  if (n >= 16)
    {
      int n1 = n/2, n2 = n - n1;
      vissprite_t **s1 = s, **s2 = s + n1, **d = t;

      msort(s1, t, n1);
      msort(s2, t, n2);

      while ((*s1)->scale > (*s2)->scale ?
             (*d++ = *s1++, --n1) : (*d++ = *s2++, --n2));

      if (n2)
        bcopyp(d, s2, n2);
      else
        bcopyp(d, s1, n1);

      bcopyp(s, t, n);
    }
  else
    {
      int i;
      for (i = 1; i < n; i++)
        {
          vissprite_t *temp = s[i];
          if (s[i-1]->scale < temp->scale)
            {
              int j = i;
              while ((s[j] = s[j-1])->scale < temp->scale && --j);
              s[j] = temp;
            }
        }
    }
}

void R_SortVisSprites (void)
{
    if (_g->num_vissprite)
    {
        int i = _g->num_vissprite;

        // If we need to allocate more pointers for the vissprites,
        // allocate as many as were allocated for sprites -- killough
        // killough 9/22/98: allocate twice as many

        if (_g->num_vissprite_ptrs < _g->num_vissprite*2)
        {
            free(_g->vissprite_ptrs);  // better than realloc -- no preserving needed
            _g->vissprite_ptrs = malloc((_g->num_vissprite_ptrs = _g->num_vissprite_alloc*2) * sizeof *_g->vissprite_ptrs);
        }

        while (--i>=0)
            _g->vissprite_ptrs[i] = _g->vissprites+i;

        // killough 9/22/98: replace qsort with merge sort, since the keys
        // are roughly in order to begin with, due to BSP rendering.

        msort(_g->vissprite_ptrs, _g->vissprite_ptrs + _g->num_vissprite, _g->num_vissprite);
    }
}

//
// R_DrawMasked
//

void R_DrawMasked(void)
{
    int i;
    drawseg_t *ds;

    R_SortVisSprites();

    // draw all vissprites back to front
    for (i = _g->num_vissprite ;--i>=0; )
        R_DrawSprite(_g->vissprite_ptrs[i]);         // killough

    // render any remaining masked mid textures

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):
    for (ds=_g->ds_p ; ds-- > _g->drawsegs ; )  // new -- killough
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

    R_DrawPlayerSprites ();
}


//
// R_DrawSpan
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
void R_DrawSpan(draw_span_vars_t *dsvars)
{
    int count = (dsvars->x2 - dsvars->x1);

    const byte *source = dsvars->source;
    const byte *colormap = dsvars->colormap;

    unsigned short* dest = _g->drawvars.byte_topleft + dsvars->y*_g->drawvars.byte_pitch + dsvars->x1;

    const unsigned int step = ((dsvars->xstep << 10) & 0xffff0000) | ((dsvars->ystep >> 6)  & 0x0000ffff);

    unsigned int position = ((dsvars->xfrac << 10) & 0xffff0000) | ((dsvars->yfrac >> 6)  & 0x0000ffff);

    unsigned int xtemp, ytemp, spot;

    do
    {
        // Calculate current texture index in u,v.
        ytemp = (position >> 4) & 0x0fc0;
        xtemp = (position >> 26);
        spot = xtemp | ytemp;

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        unsigned short color = colormap[source[spot]];


        *dest++ = (color | (color << 8));
        position += step;

    } while (count--);
}


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

    if (!(dsvars->colormap = _g->fixedcolormap))
    {
        dsvars->z = distance;
        index = distance >> LIGHTZSHIFT;
        if (index >= MAXLIGHTZ )
            index = MAXLIGHTZ-1;

        int cm_index = _g->planezlight[index];

        dsvars->colormap = _g->colormaps + cm_index;
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
        if (pl->picnum == _g->skyflatnum)
        { // sky flat
            int texture;
            angle_t an, flip;

            an = _g->viewangle;

            // Normal Doom sky, only one allowed per level
            dcvars.texturemid = skytexturemid;    // Default y-offset
            texture = _g->skytexture;             // Default texture
            flip = 0;                         // Doom flips it


          /* Sky is always drawn full bright, i.e. colormaps[0] is used.
           * Because of this hack, sky is not affected by INVUL inverse mapping.
           * Until Boom fixed this. Compat option added in MBF. */

            if (!(dcvars.colormap = _g->fixedcolormap))
                dcvars.colormap = _g->fullcolormap;          // killough 3/20/98

            // proff 09/21/98: Changed for high-res
            dcvars.iscale = FRACUNIT*200/viewheight;

            const unsigned int widthmask = _g->textures[texture]->widthmask;
            const patch_t* patch = _g->textures[texture]->patches[0].patch;

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
            _g->planezlight = zlight[light];

            pl->top[pl->minx-1] = pl->top[stop] = 0xff; // dropoff overflow

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


//*******************************************

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
  fixed_t num = FixedMul(projectiony, finesine[angleb>>ANGLETOFINESHIFT]);

  return den > num>>16 ? (num = FixedDiv(num, den)) > 64*FRACUNIT ?
    64*FRACUNIT : num < 256 ? 256 : num : 64*FRACUNIT;
}


//
// R_NewVisSprite
//

static vissprite_t *R_NewVisSprite(void)
{
    if (_g->num_vissprite >= _g->num_vissprite_alloc)             // killough
    {
        size_t num_vissprite_alloc_prev = _g->num_vissprite_alloc;

        _g->num_vissprite_alloc = _g->num_vissprite_alloc ? _g->num_vissprite_alloc+32 : 32;
        _g->vissprites = realloc(_g->vissprites,_g->num_vissprite_alloc*sizeof(*_g->vissprites));

        //e6y: set all fields to zero
        memset(_g->vissprites + num_vissprite_alloc_prev, 0,
               (_g->num_vissprite_alloc - num_vissprite_alloc_prev)*sizeof(*_g->vissprites));
    }
    return _g->vissprites + _g->num_vissprite++;
}


//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//

static void R_ProjectSprite (mobj_t* thing, int lightlevel)
{
    fixed_t   gzt;               // killough 3/27/98
    fixed_t   tx;
    fixed_t   xscale;
    int       x1;
    int       x2;
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    int       lump;
    boolean   flip;
    vissprite_t *vis;
    fixed_t   iscale;

    // transform the origin point
    fixed_t tr_x, tr_y;
    fixed_t fx, fy, fz;
    fixed_t gxt, gyt;
    fixed_t tz;
    int width;

    {
        fx = thing->x;
        fy = thing->y;
        fz = thing->z;
    }
    tr_x = fx - _g->viewx;
    tr_y = fy - _g->viewy;

    gxt = FixedMul(tr_x,_g->viewcos);
    gyt = -FixedMul(tr_y,_g->viewsin);

    tz = gxt-gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    xscale = FixedDiv(projection, tz);

    gxt = -FixedMul(tr_x,_g->viewsin);
    gyt = FixedMul(tr_y,_g->viewcos);
    tx = -(gyt+gxt);

    // too far off the side?
    if (D_abs(tx)>(tz<<2))
        return;

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if ((unsigned) thing->sprite >= (unsigned)numsprites)
        I_Error ("R_ProjectSprite: Invalid sprite number %i", thing->sprite);
#endif

    sprdef = &_g->sprites[thing->sprite];

#ifdef RANGECHECK
    if ((thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
        I_Error ("R_ProjectSprite: Invalid sprite frame %i : %i", thing->sprite,
                 thing->frame);
#endif

    if (!sprdef->spriteframes)
    {
        I_Error ("R_ProjectSprite: Missing spriteframes %i : %i", thing->sprite,
                 thing->frame);
    }


    sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        angle_t ang = R_PointToAngle(fx, fy);
        unsigned rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
        lump = sprframe->lump[rot];
        flip = (boolean) sprframe->flip[rot];
    }
    else
    {
        // use single rotation for all views
        lump = sprframe->lump[0];
        flip = (boolean) sprframe->flip[0];
    }

    {

        const patch_t* patch = W_CacheLumpNum(lump + _g->firstspritelump);

        /* calculate edges of the shape
     * cph 2003/08/1 - fraggle points out that this offset must be flipped
     * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
        if (flip) {
            tx -= (patch->width - patch->leftoffset) << FRACBITS;
        } else {
            tx -= patch->leftoffset << FRACBITS;
        }
        x1 = (centerxfrac + FixedMul(tx,xscale)) >> FRACBITS;

        tx += patch->width<<FRACBITS;
        x2 = ((centerxfrac + FixedMul (tx,xscale) ) >> FRACBITS) - 1;

        gzt = fz + (patch->topoffset << FRACBITS);
        width = patch->width;
    }

    // off the side?
    if (x1 > SCREENWIDTH || x2 < 0)
        return;

    // store information in a vissprite
    vis = R_NewVisSprite ();

    vis->mobjflags = thing->flags;
    // proff 11/06/98: Changed for high-res
    vis->scale = FixedDiv(projectiony, tz);
    vis->gx = fx;
    vis->gy = fy;
    vis->gz = fz;
    vis->gzt = gzt;                          // killough 3/27/98
    vis->texturemid = vis->gzt - _g->viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= SCREENWIDTH ? SCREENWIDTH-1 : x2;
    iscale = FixedDiv (FRACUNIT, xscale);

    if (flip)
    {
        vis->startfrac = (width<<FRACBITS)-1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale*(vis->x1-x1);
    vis->patch = lump;

    // get light level
    if (thing->flags & MF_SHADOW)
        vis->colormap = NULL;             // shadow draw
    else if (_g->fixedcolormap)
        vis->colormap = _g->fixedcolormap;      // fixed map
    else if (thing->frame & FF_FULLBRIGHT)
        vis->colormap = _g->fullcolormap;     // full bright  // killough 3/20/98
    else
    {      // diminished light
        vis->colormap = R_ColourMap(lightlevel,xscale);
    }
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
static void R_AddSprites(subsector_t* subsec, int lightlevel)
{
  sector_t* sec=subsec->sector;
  mobj_t *thing;

  // BSP is traversed by subsector.
  // A sector might have been split into several
  //  subsectors during BSP building.
  // Thus we check whether its already added.

  if (sec->validcount == _g->validcount)
    return;

  // Well, now it will be done.
  sec->validcount = _g->validcount;

  // Handle all things in sector.

  for (thing = sec->thinglist; thing; thing = thing->snext)
    R_ProjectSprite(thing, lightlevel);
}

//
// R_FindPlane
//
// killough 2/28/98: Add offsets


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

static visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel)
{
    visplane_t *check;
    unsigned hash;                      // killough

    if (picnum == _g->skyflatnum)
        height = lightlevel = 0;         // killough 7/19/98: most skies map together

    // New visplane algorithm uses hash table -- killough
    hash = visplane_hash(picnum,lightlevel,height);

    for (check=_g->visplanes[hash]; check; check=check->next)  // killough
        if (height == check->height &&
                picnum == check->picnum &&
                lightlevel == check->lightlevel)
            return check;

    check = new_visplane(hash);         // killough

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH; // Was SCREENWIDTH -- killough 11/98
    check->maxx = -1;

    memset (check->top, 0xff, sizeof check->top);

    return check;
}

/*
 * R_DupPlane
 *
 * cph 2003/04/18 - create duplicate of existing visplane and set initial range
 */
static visplane_t *R_DupPlane(const visplane_t *pl, int start, int stop)
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
// R_CheckPlane
//
static visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
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

    for (x=intrl ; x <= intrh && pl->top[x] == 0xff; x++) // dropoff overflow
        ;

    if (x > intrh) { /* Can use existing plane; extend range */
        pl->minx = unionl; pl->maxx = unionh;
        return pl;
    } else /* Cannot use existing plane; create a new one */
        return R_DupPlane(pl,start,stop);
}


//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
//
void R_DrawColumn (draw_column_vars_t *dcvars)
{
    int count = dcvars->yh - dcvars->yl;

    const byte *source = dcvars->source;
    const byte *colormap = dcvars->colormap;

    unsigned short* dest = _g->drawvars.byte_topleft + (dcvars->yl*_g->drawvars.byte_pitch) + dcvars->x;

    const fixed_t		fracstep = dcvars->iscale;
    fixed_t frac = dcvars->texturemid + (dcvars->yl - centery)*fracstep;

    // Zero length, column does not exceed a pixel.
    if (dcvars->yl >= dcvars->yh)
        return;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.
        unsigned short color = colormap[source[(frac>>FRACBITS)&127]];


        *dest = (color | (color << 8));

        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}



static void R_DrawColumnInCache(const column_t* patch, byte* cache, int originy, int cacheheight)
{
    int		count;
    int		position;
    const byte*	source;
    byte*	dest;

    dest = (byte *)cache + 3;

    while (patch->topdelta != 0xff)
    {
        source = (const byte *)patch + 3;
        count = patch->length;
        position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            memcpy (cache + position, source, count);

        patch = (const column_t *)(  (const byte *)patch + patch->length + 4);
    }
}

/*
 * Draw a column of pixels of the specified texture.
 * If the texture is simple (1 patch, full height) then just draw
 * straight from const patch_t*.
*/
static void DrawSegTextureColumn(int texture, int texcolumn, draw_column_vars_t* dcvars)
{
    //static int total = 0;
    //static int misses = 0;

    texture_t* tex = _g->textures[texture];

    if(tex->patchcount == 1)
    {
        //simple texture.

        const unsigned int widthmask = tex->widthmask;
        const patch_t* patch = tex->patches[0].patch;

        const unsigned int xc = texcolumn & widthmask;

        const column_t* column = (const column_t *) ((const byte *)patch + patch->columnofs[xc]);

        dcvars->source = (const byte*)column + 3;

        R_DrawColumn (dcvars);
    }
    else
    {
        const int xc = (texcolumn & 0xfffe) & tex->widthmask;

        unsigned int cachekey = (xc & 0x7e) | (texture & 1);

        byte* colcache = &columnCache[cachekey*128];
        column_cache_entry_t* cacheEntry = &columnCacheEntries[cachekey];

        //total++;

        if((cacheEntry->texture != texture) || cacheEntry->column != xc)
        {
            byte tmpCol[128];

            //misses++;

            cacheEntry->texture = texture;
            cacheEntry->column = xc;

            for(int i=0; i<tex->patchcount; i++)
            {
                const texpatch_t* patch = &tex->patches[i];

                const patch_t* realpatch = patch->patch;

                int x1 = patch->originx;
                int x2 = x1 + realpatch->width;

                if (x2 > tex->width)
                    x2 = tex->width;

                if(xc >= x1 && xc < x2)
                {
                    const column_t* patchcol = (const column_t *)((const byte *)realpatch + realpatch->columnofs[xc-x1]);

                    R_DrawColumnInCache (patchcol,
                                         tmpCol,
                                         patch->originy,
                                         tex->height);

                }
            }

            memcpy(colcache, tmpCol, 128);
        }

        dcvars->source = colcache;

        R_DrawColumn (dcvars);
    }
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
          angle_t angle =(_g->rw_centerangle+xtoviewangle[_g->rw_x])>>ANGLETOFINESHIFT;

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
          //

          DrawSegTextureColumn(_g->midtexture, texturecolumn, &dcvars);

          _g->ceilingclip[_g->rw_x] = viewheight;
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

                  DrawSegTextureColumn(_g->toptexture, texturecolumn, &dcvars);

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

                  DrawSegTextureColumn(_g->bottomtexture, texturecolumn, &dcvars);

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

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
static void R_StoreWallRange(const int start, const int stop)
{
    fixed_t hyp;
    angle_t offsetangle;

    if (_g->ds_p == _g->drawsegs+_g->maxdrawsegs)   // killough 1/98 -- fix 2s line HOM
    {
        unsigned pos = _g->ds_p - _g->drawsegs; // jff 8/9/98 fix from ZDOOM1.14a

        unsigned newmax = _g->maxdrawsegs + 32;

        _g->drawsegs = realloc(_g->drawsegs,newmax*sizeof(*_g->drawsegs));

        _g->ds_p = _g->drawsegs + pos;          // jff 8/9/98 fix from ZDOOM1.14a
        _g->maxdrawsegs = newmax;
    }

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
                _g->maxopenings = _g->maxopenings ?_g-> maxopenings + 32 : 32;
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
                ADJUST (maskedtexturecol)
                ADJUST (sprtopclip)
                ADJUST (sprbottomclip)
            }
#undef ADJUST
        }
    }  // killough: end of code to remove limits on openings

    // calculate scale at both ends and step
    _g->ds_p->scale1 = _g->rw_scale = R_ScaleFromGlobalAngle (_g->viewangle + xtoviewangle[start]);

    if (stop > start)
    {
        _g->ds_p->scale2 = R_ScaleFromGlobalAngle (_g->viewangle + xtoviewangle[stop]);
        //_g->ds_p->scalestep = _g->rw_scalestep = (_g->ds_p->scale2-_g->rw_scale) / (stop-start);
        _g->ds_p->scalestep = _g->rw_scalestep = IDiv32(_g->ds_p->scale2-_g->rw_scale, stop-start);
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

        if (_g->linedef->r_flags & RF_CLOSED)
        { /* cph - closed 2S line e.g. door */
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

        }
        else
        { /* not solid - old code */

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
        if (_g->frontsector->ceilingpic == _g->skyflatnum && _g->backsector->ceilingpic == _g->skyflatnum)
            _g->worldtop = _g->worldhigh;

        _g->markfloor = _g->worldlow != _g->worldbottom
                || _g->backsector->floorpic != _g->frontsector->floorpic
                || _g->backsector->lightlevel != _g->frontsector->lightlevel
                ;

        _g->markceiling = _g->worldhigh != _g->worldtop
                || _g->backsector->ceilingpic != _g->frontsector->ceilingpic
                || _g->backsector->lightlevel != _g->frontsector->lightlevel
                ;

        if (_g->backsector->ceilingheight <= _g->frontsector->floorheight || _g->backsector->floorheight >= _g->frontsector->ceilingheight)
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

    // if a floor / ceiling plane is on the wrong side of the view
    // plane, it is definitely invisible and doesn't need to be marked.
    if (_g->frontsector->floorheight >= _g->viewz)       // above view plane
        _g->markfloor = false;
    if (_g->frontsector->ceilingheight <= _g->viewz &&
            _g->frontsector->ceilingpic != _g->skyflatnum)   // below view plane
        _g->markceiling = false;

    // calculate incremental stepping values for texture edges
    _g->worldtop >>= 4;
    _g->worldbottom >>= 4;

    _g->topstep = -FixedMul (_g->rw_scalestep, _g->worldtop);
    _g->topfrac = (centeryfrac>>4) - FixedMul (_g->worldtop, _g->rw_scale);

    _g->bottomstep = -FixedMul (_g->rw_scalestep,_g->worldbottom);
    _g->bottomfrac = (centeryfrac>>4) - FixedMul (_g->worldbottom, _g->rw_scale);

    if (_g->backsector)
    {
        _g->worldhigh >>= 4;
        _g->worldlow >>= 4;

        if (_g->worldhigh < _g->worldtop)
        {
            _g->pixhigh = (centeryfrac>>4) - FixedMul (_g->worldhigh, _g->rw_scale);
            _g->pixhighstep = -FixedMul (_g->rw_scalestep,_g->worldhigh);
        }
        if (_g->worldlow > _g->worldbottom)
        {
            _g->pixlow = (centeryfrac>>4) - FixedMul (_g->worldlow, _g->rw_scale);
            _g->pixlowstep = -FixedMul (_g->rw_scalestep,_g->worldlow);
        }
    }

    // render it
    if (_g->markceiling)
    {
        if (_g->ceilingplane)   // killough 4/11/98: add NULL ptr checks
            _g->ceilingplane = R_CheckPlane (_g->ceilingplane, _g->rw_x, _g->rw_stopx-1);
        else
            _g->markceiling = 0;
    }

    if (_g->markfloor)
    {
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
    if (_g->backsector && _g->didsolidcol)
    {
        if (!(_g->ds_p->silhouette & SIL_BOTTOM))
        {
            _g->ds_p->silhouette |= SIL_BOTTOM;
            _g->ds_p->bsilheight = _g->backsector->floorheight;
        }
        if (!(_g->ds_p->silhouette & SIL_TOP))
        {
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
                && (_g->backsector->ceilingpic !=_g->skyflatnum ||
                    _g->frontsector->ceilingpic!=_g->skyflatnum)
                )
            )
        _g->linedef->r_flags = RF_CLOSED;
    else
    {
        // Reject empty lines used for triggers
        //  and special events.
        // Identical floor and ceiling on both sides,
        // identical light levels on both sides,
        // and no middle texture.
        // CPhipps - recode for speed, not certain if this is portable though
        if (_g->backsector->ceilingheight != _g->frontsector->ceilingheight
                || _g->backsector->floorheight != _g->frontsector->floorheight
                || _g->curline->sidedef->midtexture
                || _g->backsector->ceilingpic != _g->frontsector->ceilingpic
                || _g->backsector->floorpic != _g->frontsector->floorpic
                || _g->backsector->lightlevel != _g->frontsector->lightlevel)
        {
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
            if (!(p = memchr(_g->solidcol+first, 0, last-first)))
                return; // All solid

            first = p - _g->solidcol;
        }
        else
        {
            int to;
            if (!(p = memchr(_g->solidcol+first, 1, last-first)))
                to = last;
            else
                to = p - _g->solidcol;

            R_StoreWallRange(first, to-1);

            if (solid)
            {
                memset(_g->solidcol+first,1,to-first);
            }

            first = to;
        }
    }
}

//
// R_ClearClipSegs
//

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

    _g->curline = line;

    angle1 = R_PointToAngle (line->v1->x, line->v1->y);
    angle2 = R_PointToAngle (line->v2->x, line->v2->y);

    // Clip to view edges.
    span = angle1 - angle2;

    // Back side, i.e. backface culling
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    _g->rw_angle1 = angle1;
    angle1 -= _g->viewangle;
    angle2 -= _g->viewangle;

    tspan = angle1 + clipangle;
    if (tspan > 2*clipangle)
    {
        tspan -= 2*clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = clipangle;
    }

    tspan = clipangle - angle2;
    if (tspan > 2*clipangle)
    {
        tspan -= 2*clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;
        angle2 = 0-clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.

    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;

    // killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
        return;

    _g->backsector = line->backsector;

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

#ifdef RANGECHECK
    if (num>=numsubsectors)
        I_Error ("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

    sub = &_g->subsectors[num];
    _g->frontsector = sub->sector;
    count = sub->numlines;
    line = &_g->segs[sub->firstline];

    if(_g->frontsector->floorheight < _g->viewz)
    {
        _g->floorplane = R_FindPlane(_g->frontsector->floorheight,
                                     _g->frontsector->floorpic,
                                     _g->frontsector->lightlevel                // killough 3/16/98
                                     );
    }
    else
    {
        _g->floorplane = NULL;
    }


    if(_g->frontsector->ceilingheight > _g->viewz || (_g->frontsector->ceilingpic == _g->skyflatnum))
    {
        _g->ceilingplane = R_FindPlane(_g->frontsector->ceilingheight,     // killough 3/8/98
                                       _g->frontsector->ceilingpic,
                                       _g->frontsector->lightlevel
                                       );
    }
    else
    {
        _g->ceilingplane = NULL;
    }

    R_AddSprites(sub, _g->frontsector->lightlevel);
    while (count--)
    {
        R_AddLine (line);
        line++;
        _g->curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
    }
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
static boolean R_CheckBBox(const short *bspcoord)
{
    angle_t angle1, angle2;

    {
        int        boxpos;
        const int* check;

        // Find the corners of the box
        // that define the edges from current viewpoint.
        boxpos = (_g->viewx <= ((fixed_t)bspcoord[BOXLEFT]<<FRACBITS) ? 0 : _g->viewx < ((fixed_t)bspcoord[BOXRIGHT]<<FRACBITS) ? 1 : 2) +
                (_g->viewy >= ((fixed_t)bspcoord[BOXTOP]<<FRACBITS) ? 0 : _g->viewy > ((fixed_t)bspcoord[BOXBOTTOM]<<FRACBITS) ? 4 : 8);

        if (boxpos == 5)
            return true;

        check = checkcoord[boxpos];
        angle1 = R_PointToAngle (((fixed_t)bspcoord[check[0]]<<FRACBITS), ((fixed_t)bspcoord[check[1]]<<FRACBITS)) - _g->viewangle;
        angle2 = R_PointToAngle (((fixed_t)bspcoord[check[2]]<<FRACBITS), ((fixed_t)bspcoord[check[3]]<<FRACBITS)) - _g->viewangle;
    }

    // cph - replaced old code, which was unclear and badly commented
    // Much more efficient code now
    if ((signed)angle1 < (signed)angle2)
    { /* it's "behind" us */
        /* Either angle1 or angle2 is behind us, so it doesn't matter if we
     * change it to the corect sign
     */
        if ((angle1 >= ANG180) && (angle1 < ANG270))
            angle1 = INT_MAX; /* which is ANG180-1 */
        else
            angle2 = INT_MIN;
    }

    if ((signed)angle2 >= (signed)clipangle) return false; // Both off left edge
    if ((signed)angle1 <= -(signed)clipangle) return false; // Both off right edge
    if ((signed)angle1 >= (signed)clipangle) angle1 = clipangle; // Clip at left edge
    if ((signed)angle2 <= -(signed)clipangle) angle2 = 0-clipangle; // Clip at right edge

    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    {
        int sx1 = viewangletox[angle1];
        int sx2 = viewangletox[angle2];
        //    const cliprange_t *start;

        // Does not cross a pixel.
        if (sx1 == sx2)
            return false;

        if (!memchr(_g->solidcol+sx1, 0, sx2-sx1)) return false;
        // All columns it covers are already solidly covered
    }

    return true;
}

//Render a BSP subsector if bspnum is a leaf node.
//Return false if bspnum is frame node.

static boolean R_RenderBspSubsector(int bspnum)
{
    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            R_Subsector (0);
        else
            R_Subsector (bspnum & (~NF_SUBSECTOR));

        return true;
    }

    return false;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

PUREFUNC int R_PointOnSide(fixed_t x, fixed_t y, const mapnode_t *node)
{
    fixed_t dx = (fixed_t)node->dx << FRACBITS;
    fixed_t dy = (fixed_t)node->dy << FRACBITS;

    fixed_t nx = (fixed_t)node->x << FRACBITS;
    fixed_t ny = (fixed_t)node->y << FRACBITS;

    if (!dx)
        return x <= nx ? node->dy > 0 : node->dy < 0;

    if (!dy)
        return y <= ny ? node->dx < 0 : node->dx > 0;

    x -= nx;
    y -= ny;

    // Try to quickly decide by looking at sign bits.
    if ((dy ^ dx ^ x ^ y) < 0)
        return (dy ^ x) < 0;  // (left is negative)

    return FixedMul(y, node->dx) >= FixedMul(node->dy, x);
}


// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

//Non recursive version.
//constant stack space used and easier to
//performance profile.
#define MAX_BSP_DEPTH 128

void R_RenderBSPNode(int bspnum)
{
    int stack[MAX_BSP_DEPTH];
    int sp = 0;

    const mapnode_t* bsp;
    int side = 0;

    while(true)
    {
        //Front sides.
        while (!R_RenderBspSubsector(bspnum))
        {
            if(sp == MAX_BSP_DEPTH)
                break;

            bsp = &_g->nodes[bspnum];
            side = R_PointOnSide (_g->viewx, _g->viewy, bsp);

            stack[sp++] = bspnum;
            stack[sp++] = side;

            bspnum = bsp->children[side];
        }

        if(sp == 0)
        {
            //back at root node and not visible. All done!
            return;
        }

        //Back sides.
        side = stack[--sp];
        bspnum = stack[--sp];
        bsp = &_g->nodes[bspnum];

        // Possibly divide back space.
        //Walk back up the tree until we find
        //a node that has a visible backspace.
        while(!R_CheckBBox (bsp->bbox[side^1]))
        {
            if(sp == 0)
            {
                //back at root node and not visible. All done!
                return;
            }

            //Back side next.
            side = stack[--sp];
            bspnum = stack[--sp];

            bsp = &_g->nodes[bspnum];
        }

        bspnum = bsp->children[side^1];
    }
}
