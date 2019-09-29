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
 *  Refresh of things, i.e. objects represented by sprites.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "r_draw.h"
#include "r_things.h"
#include "v_video.h"
#include "lprintf.h"

#include "global_data.h"

#define MINZ        (FRACUNIT*4)
#define BASEYCENTER 100

typedef struct {
  int x1;
  int x2;
  int column;
  int topclip;
  int bottomclip;
} maskdraw_t;

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//


//
// INITIALIZATION FUNCTIONS
//


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//

static void R_InstallSpriteLump(int lump, unsigned frame,
                                unsigned rotation, boolean flipped)
{
  if (frame >= MAX_SPRITE_FRAMES || rotation > 8)
    I_Error("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

  if ((int) frame > _g->maxframe)
    _g->maxframe = frame;

  if (rotation == 0)
    {    // the lump should be used for all rotations
      int r;
      for (r=0 ; r<8 ; r++)
        if (_g->sprtemp[frame].lump[r]==-1)
          {
            _g->sprtemp[frame].lump[r] = lump - _g->firstspritelump;
            _g->sprtemp[frame].flip[r] = (byte) flipped;
            _g->sprtemp[frame].rotate = false; //jff 4/24/98 if any subbed, rotless
          }
      return;
    }

  // the lump is only used for one rotation

  if (_g->sprtemp[frame].lump[--rotation] == -1)
    {
      _g->sprtemp[frame].lump[rotation] = lump - _g->firstspritelump;
      _g->sprtemp[frame].flip[rotation] = (byte) flipped;
      _g->sprtemp[frame].rotate = true; //jff 4/24/98 only change if rot used
    }
}

//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
// (4 chars exactly) to be used.
//
// Builds the sprite rotation matrixes to account
// for horizontally flipped sprites.
//
// Will report an error if the lumps are inconsistent.
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
//
// A sprite that is flippable will have an additional
//  letter/number appended.
//
// The rotation character can be 0 to signify no rotations.
//
// 1/25/98, 1/31/98 killough : Rewritten for performance
//
// Empirically verified to have excellent hash
// properties across standard Doom sprites:

#define R_SpriteNameHash(s) ((unsigned)((s)[0]-((s)[1]*3-(s)[3]*2-(s)[2])*2))

static void R_InitSpriteDefs(const char * const * namelist)
{
  size_t numentries = _g->lastspritelump-_g->firstspritelump+1;
  struct { int index, next; } *hash;
  int i;

  if (!numentries || !*namelist)
    return;

  // count the number of sprite names
  for (i=0; namelist[i]; i++)
    ;

  _g->numsprites = i;

  _g->sprites = Z_Malloc(_g->numsprites *sizeof(*_g->sprites), PU_STATIC, NULL);

  memset(_g->sprites, 0, _g->numsprites *sizeof(*_g->sprites));

  // Create hash table based on just the first four letters of each sprite
  // killough 1/31/98

  hash = malloc(sizeof(*hash)*numentries); // allocate hash table

  for (i=0; (size_t)i<numentries; i++)             // initialize hash table as empty
    hash[i].index = -1;

  for (i=0; (size_t)i<numentries; i++)             // Prepend each sprite to hash chain
    {                                      // prepend so that later ones win
      const char* sn = W_GetNameForNum(i+_g->firstspritelump);

      int j = R_SpriteNameHash(sn) % numentries;
      hash[i].next = hash[j].index;
      hash[j].index = i;
    }

  // scan all the lump names for each of the names,
  //  noting the highest frame letter.

  for (i=0 ; i<_g->numsprites ; i++)
    {
      const char *spritename = namelist[i];
      int j = hash[R_SpriteNameHash(spritename) % numentries].index;

      if (j >= 0)
        {
          memset(_g->sprtemp, -1, sizeof(_g->sprtemp));
          _g->maxframe = -1;
          do
            {
              const char* sn = W_GetNameForNum(j + _g->firstspritelump);

              //register lumpinfo_t *lump = _g->lumpinfo + j + _g->firstspritelump;

              // Fast portable comparison -- killough
              // (using int pointer cast is nonportable):

              if (!((sn[0] ^ spritename[0]) |
                    (sn[1] ^ spritename[1]) |
                    (sn[2] ^ spritename[2]) |
                    (sn[3] ^ spritename[3])))
                {
                  R_InstallSpriteLump(j+_g->firstspritelump,
                                      sn[4] - 'A',
                                      sn[5] - '0',
                                      false);
                  if (sn[6])
                    R_InstallSpriteLump(j+_g->firstspritelump,
                                        sn[6] - 'A',
                                        sn[7] - '0',
                                        true);
                }
            }
          while ((j = hash[j].next) >= 0);

          // check the frames that were found for completeness
          if ((_g->sprites[i].numframes = ++_g->maxframe))  // killough 1/31/98
            {
              int frame;
              for (frame = 0; frame < _g->maxframe; frame++)
                switch ((int) _g->sprtemp[frame].rotate)
                  {
                  case -1:
                    // no rotations were found for that frame at all
                    I_Error ("R_InitSprites: No patches found "
                             "for %.8s frame %c", namelist[i], frame+'A');
                    break;

                  case 0:
                    // only the first rotation is needed
                    break;

                  case 1:
                    // must have all 8 frames
                    {
                      int rotation;
                      for (rotation=0 ; rotation<8 ; rotation++)
                        if (_g->sprtemp[frame].lump[rotation] == -1)
                          I_Error ("R_InitSprites: Sprite %.8s frame %c "
                                   "is missing rotations",
                                   namelist[i], frame+'A');
                      break;
                    }
                  }
              // allocate space for the frames present and copy sprtemp to it
              _g->sprites[i].spriteframes =
                Z_Malloc (_g->maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
              memcpy (_g->sprites[i].spriteframes, _g->sprtemp,
                      _g->maxframe*sizeof(spriteframe_t));
            }
        }
    }
  free(hash);             // free hash table
}

//
// GAME FUNCTIONS
//


//
// R_InitSprites
// Called at program start.
//

void R_InitSprites(const char * const *namelist)
{
  R_InitSpriteDefs(namelist);
}

//
// R_ClearSprites
// Called at frame start.
//

void R_ClearSprites (void)
{
  _g->num_vissprite = 0;            // killough
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
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//



void R_DrawMaskedColumn(const patch_t *patch,
  R_DrawColumn_f colfunc,
  draw_column_vars_t *dcvars,
  const column_t *column
)
{
    int     i;
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
    if (vis->mobjflags & MF_TRANSLATION)
      {
        colfunc = R_DrawTranslatedColumn;
        dcvars.translation = translationtables +
          ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
      }
    else
        colfunc = R_DrawColumn; // killough 3/14/98, 4/11/98

// proff 11/06/98: Changed for high-res
  dcvars.iscale = FixedDiv (FRACUNIT, vis->scale);
  dcvars.texturemid = vis->texturemid;
  frac = vis->startfrac;

  _g->spryscale = vis->scale;
  _g->sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid,_g->spryscale);

  for (dcvars.x=vis->x1 ; dcvars.x<=vis->x2 ; dcvars.x++, frac += vis->xiscale)
    {
      texturecolumn = frac>>FRACBITS;

      const column_t* column = (column_t *) ((byte *)patch + patch->columnofs[texturecolumn]);

      R_DrawMaskedColumn(
        patch,
        colfunc,
        &dcvars,
        column
      );
    }
  //R_UnlockPatchNum(vis->patch+_g->firstspritelump); // cph - release lump
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
  int heightsec;      // killough 3/27/98

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
    //R_UnlockPatchNum(lump+_g->firstspritelump);
  }

  // off the side?
  if (x1 > SCREENWIDTH || x2 < 0)
    return;

  // killough 4/9/98: clip things which are out of view due to height
  // e6y: fix of hanging decoration disappearing in Batman Doom MAP02
  // centeryfrac -> viewheightfrac
  if (fz  > _g->viewz + FixedDiv(viewheightfrac, xscale) ||
      gzt < _g->viewz - FixedDiv(viewheightfrac-viewheight, xscale))
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
void R_AddSprites(subsector_t* subsec, int lightlevel)
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

void R_DrawPlayerSprites(void)
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

  //    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

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

  //    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

  for (ds=_g->ds_p ; ds-- > _g->drawsegs ; )  // new -- killough
    if (ds->maskedtexturecol)
      R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

    R_DrawPlayerSprites ();
}
