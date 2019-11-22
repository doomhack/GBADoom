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
#include "r_draw.h"
#include "m_bbox.h"
#include "r_sky.h"
#include "v_video.h"
#include "lprintf.h"
#include "st_stuff.h"
#include "i_main.h"
#include "i_system.h"
#include "g_game.h"
#include "m_random.h"

#include "global_data.h"

#include "gba_functions.h"


//*****************************************
//Globals.
//*****************************************

int numnodes;
const mapnode_t *nodes;

fixed_t  viewx, viewy, viewz;

angle_t  viewangle;

static byte solidcol[MAX_SCREENWIDTH];

static const seg_t     *curline;
static side_t    *sidedef;
static const line_t    *linedef;
static sector_t  *frontsector;
static sector_t  *backsector;
static drawseg_t *ds_p;

static visplane_t *floorplane, *ceilingplane;
static int             rw_angle1;

static angle_t         rw_normalangle; // angle to line origin
static fixed_t         rw_distance;

static int      rw_x;
static int      rw_stopx;

short floorclip[MAX_SCREENWIDTH], ceilingclip[MAX_SCREENWIDTH]; // dropoff overflow

static fixed_t  rw_scale;
static fixed_t  rw_scalestep;

static int      worldtop;
static int      worldbottom;

// True if any of the segs textures might be visible.
static boolean  segtextured;
static boolean  markfloor;      // False if the back side is the same plane.
static boolean  markceiling;
static boolean  maskedtexture;
static int      toptexture;
static int      bottomtexture;
static int      midtexture;

static fixed_t  rw_midtexturemid;
static fixed_t  rw_toptexturemid;
static fixed_t  rw_bottomtexturemid;

const lighttable_t *fullcolormap;
const lighttable_t *colormaps;

const lighttable_t* fixedcolormap;

int extralight;                           // bumped light from gun blasts
draw_vars_t drawvars;

static short   *mfloorclip;   // dropoff overflow
static short   *mceilingclip; // dropoff overflow
static fixed_t spryscale;
static fixed_t sprtopscreen;

static angle_t  rw_centerangle;
static fixed_t  rw_offset;
static int      rw_lightlevel;

static short      *maskedtexturecol; // dropoff overflow

const texture_t **textures; // proff - 04/05/2000 removed static for OpenGL
fixed_t   *textureheight; //needed for texture pegging (and TFE fix - killough)

short       *flattranslation;             // for global animation
short       *texturetranslation;

fixed_t basexscale, baseyscale;

fixed_t  viewcos, viewsin;

static fixed_t  topfrac;
static fixed_t  topstep;
static fixed_t  bottomfrac;
static fixed_t  bottomstep;

static fixed_t  pixhigh;
static fixed_t  pixlow;

static fixed_t  pixhighstep;
static fixed_t  pixlowstep;

static int      worldhigh;
static int      worldlow;

static lighttable_t current_colormap[256];
static const lighttable_t* current_colormap_ptr;

static fixed_t planeheight;

//*****************************************
// Constants
//*****************************************

const int viewheight = SCREENHEIGHT-ST_SCALED_HEIGHT;
const int centery = (SCREENHEIGHT-ST_SCALED_HEIGHT)/2;
static const int centerxfrac = (SCREENWIDTH/2) << FRACBITS;
static const int centeryfrac = ((SCREENHEIGHT-ST_SCALED_HEIGHT)/2) << FRACBITS;

const fixed_t projection = (SCREENWIDTH/2) << FRACBITS;

static const fixed_t projectiony = ((SCREENHEIGHT * (SCREENWIDTH/2) * 320) / 200) / SCREENWIDTH * FRACUNIT;

static const fixed_t pspritescale = FRACUNIT*SCREENWIDTH/320;
static const fixed_t pspriteiscale = FRACUNIT*320/SCREENWIDTH;

static const fixed_t pspriteyscale = (((SCREENHEIGHT*SCREENWIDTH)/SCREENWIDTH) << FRACBITS) / 200;

static const angle_t clipangle = 537395200; //xtoviewangle[0];

static const int skytexturemid = 100*FRACUNIT;


//********************************************
// This goes here as we want the Thumb code
// to BX to ARM as Thumb long mul is very slow.
//********************************************
inline fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b)
{
    return (fixed_t)((int_64_t) a*b >> FRACBITS);
}

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

static CONSTFUNC int SlopeDiv(unsigned num, unsigned den)
{
  unsigned ans;

  if (den < 512)
    return SLOPERANGE;

  ans = IDiv32(num<<3, den>>8);

  return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

static PUREFUNC int R_PointOnSide(fixed_t x, fixed_t y, const mapnode_t *node)
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

//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
    int nodenum = numnodes-1;

    // special case for trivial maps (single subsector, no nodes)
    if (numnodes == 0)
        return _g->subsectors;

    while (!(nodenum & NF_SUBSECTOR))
        nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];
    return &_g->subsectors[nodenum & ~NF_SUBSECTOR];
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


CONSTFUNC angle_t R_PointToAngle2(fixed_t vx, fixed_t vy, fixed_t x, fixed_t y)
{
    x -= vx;
    y -= vy;

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

static CONSTFUNC angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    return R_PointToAngle2(viewx, viewy, x, y);
}


// killough 5/2/98: move from r_main.c, made static, simplified

static CONSTFUNC fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    fixed_t dx = D_abs(x - viewx);
    fixed_t dy = D_abs(y - viewy);

    if (dy > dx)
    {
        fixed_t t = dx;
        dx = dy;
        dy = t;
    }

    return FixedDiv(dx, finesine[(tantoangle[FixedDiv(dy,dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT]);
}

static const lighttable_t* R_ColourMap(int lightlevel)
{
    if (fixedcolormap)
        return fixedcolormap;
    else
    {
        if (curline)
        {
            if (curline->v1.y == curline->v2.y)
                lightlevel -= 1 << LIGHTSEGSHIFT;
            else if (curline->v1.x == curline->v2.x)
                lightlevel += 1 << LIGHTSEGSHIFT;
        }

        lightlevel += extralight << LIGHTSEGSHIFT;

        int cm = ((256-lightlevel)>>2) - 24;

        if(cm >= NUMCOLORMAPS)
            cm = NUMCOLORMAPS-1;
        else if(cm < 0)
            cm = 0;

        return fullcolormap + cm*256;
    }
}


//Load a colormap into IWRAM.
static const lighttable_t* R_LoadColorMap(int lightlevel)
{
    const lighttable_t* lm = R_ColourMap(lightlevel);

    if(current_colormap_ptr != lm)
    {
        BlockCopy(&current_colormap[0], lm, 256);
        current_colormap_ptr = lm;
    }

    return &current_colormap[0];
}

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
//
static void R_DrawColumn (draw_column_vars_t *dcvars)
{
    int count = (dcvars->yh - dcvars->yl) + 1;

    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;

    const byte *source = dcvars->source;
    const byte *colormap = dcvars->colormap;

    unsigned short* dest = drawvars.byte_topleft + (dcvars->yl*SCREENPITCH) + dcvars->x;

    const fixed_t		fracstep = dcvars->iscale;
    fixed_t frac = dcvars->texturemid + (dcvars->yl - centery)*fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.

    unsigned int l = (count >> 3);
    unsigned int r = (count & 7);

    while(l--)
    {
        //P1
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P2
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P3
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P4
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P5
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P6
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P7
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;

        //P8
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;
    }


    while(r--)
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.
        *((byte*)dest) = colormap[source[(frac>>FRACBITS)&127]];

        dest += SCREENWIDTH;
        frac += fracstep;
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
        topscreen = sprtopscreen + spryscale*column->topdelta;
        bottomscreen = topscreen + spryscale*column->length;

        dcvars->yl = (topscreen+FRACUNIT-1)>>FRACBITS;
        dcvars->yh = (bottomscreen-1)>>FRACBITS;

        if (dcvars->yh >= mfloorclip[dcvars->x])
            dcvars->yh = mfloorclip[dcvars->x]-1;

        if (dcvars->yl <= mceilingclip[dcvars->x])
            dcvars->yl = mceilingclip[dcvars->x]+1;

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
static void R_DrawVisSprite(const vissprite_t *vis)
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

    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);

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


static const texture_t* R_GetOrLoadTexture(int tex_num)
{
    const texture_t* tex = textures[tex_num];

    if(!tex)
        tex = R_GetTexture(tex_num);

    return tex;
}


//
// R_RenderMaskedSegRange
//

static void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
    int      texnum;
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?

    curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally

    frontsector = SG_FRONTSECTOR(curline);
    backsector = SG_BACKSECTOR(curline);

    texnum = _g->sides[curline->sidenum].midtexture;
    texnum = texturetranslation[texnum];

    // killough 4/13/98: get correct lightlevel for 2s normal textures
    rw_lightlevel = frontsector->lightlevel;

    maskedtexturecol = ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;
    spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
    mfloorclip = ds->sprbottomclip;
    mceilingclip = ds->sprtopclip;

    // find positioning
    if (_g->lines[curline->linenum].flags & ML_DONTPEGBOTTOM)
    {
        dcvars.texturemid = frontsector->floorheight > backsector->floorheight
                ? frontsector->floorheight : backsector->floorheight;
        dcvars.texturemid = dcvars.texturemid + textureheight[texnum] - viewz;
    }
    else
    {
        dcvars.texturemid =frontsector->ceilingheight<backsector->ceilingheight
                ? frontsector->ceilingheight : backsector->ceilingheight;
        dcvars.texturemid = dcvars.texturemid - viewz;
    }

    dcvars.texturemid += _g->sides[curline->sidenum].rowoffset;

    if (fixedcolormap) {
        dcvars.colormap = fixedcolormap;
    }

    const texture_t* texture = R_GetOrLoadTexture(texnum);

    // draw the columns
    for (dcvars.x = x1 ; dcvars.x <= x2 ; dcvars.x++, spryscale += rw_scalestep)
    {
        if (maskedtexturecol[dcvars.x] != SHRT_MAX) // dropoff overflow
        {
            dcvars.colormap = R_LoadColorMap(rw_lightlevel);

            sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);


            dcvars.iscale = IDiv32(INT_MAX, spryscale >> 1);
            //dcvars.iscale = UDiv32(0xffffffffu, (unsigned) spryscale);

            // draw the texture

            int xc = maskedtexturecol[dcvars.x];

            const column_t* column = R_GetColumn(texture, xc);

            R_DrawMaskedColumn(R_DrawColumn, &dcvars, column);

            maskedtexturecol[dcvars.x] = SHRT_MAX; // dropoff overflow
        }
    }

    curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
}


// killough 5/2/98: reformatted

static PUREFUNC int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
  fixed_t lx = line->v1.x;
  fixed_t ly = line->v1.y;
  fixed_t ldx = line->v2.x - lx;
  fixed_t ldy = line->v2.y - ly;

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

static void R_DrawSprite (const vissprite_t* spr)
{
    drawseg_t *ds;
    short* clipbot = floorclip;
    short* cliptop = ceilingclip;

    drawseg_t* drawsegs  =_g->drawsegs;

    //int     clipbot[MAX_SCREENWIDTH]; // killough 2/8/98: // dropoff overflow
    //int     cliptop[MAX_SCREENWIDTH]; // change to MAX_*  // dropoff overflow
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

    for (ds=ds_p ; ds-- > drawsegs ; )  // new -- killough
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

        if (scale < spr->scale || (lowscale < spr->scale && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline)))
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

    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr);
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
    sprdef = &_g->sprites[psp->state->sprite];

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
    else if (fixedcolormap)
        vis->colormap = fixedcolormap;           // fixed color
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis->colormap = fullcolormap;            // full bright // killough 3/20/98
    else
        vis->colormap = R_LoadColorMap(lightlevel);  // local light

    R_DrawVisSprite(vis);
}



//
// R_DrawPlayerSprites
//

static void R_DrawPlayerSprites(void)
{
  int i, lightlevel = _g->viewplayer->mo->subsector->sector->lightlevel;
  pspdef_t *psp;

  // clip to screen bounds
  mfloorclip = screenheightarray;
  mceilingclip = negonearray;

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

//#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))
#define bcopyp(d, s, n) BlockCopy(d, s, (n) * sizeof(void *))

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

static void R_SortVisSprites (void)
{

    int i = _g->num_vissprite;

    if (i)
    {
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

static void R_DrawMasked(void)
{
    int i;
    drawseg_t *ds;
    drawseg_t* drawsegs = _g->drawsegs;


    R_SortVisSprites();

    // draw all vissprites back to front
    for (i = _g->num_vissprite ;--i>=0; )
        R_DrawSprite(_g->vissprite_ptrs[i]);         // killough

    // render any remaining masked mid textures

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):
    for (ds=ds_p ; ds-- > drawsegs ; )  // new -- killough
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
static void R_DrawSpan(draw_span_vars_t *dsvars)
{
    int count = (dsvars->x2 - dsvars->x1) + 1;

    if(count <= 0)
        return;

    const byte *source = dsvars->source;
    const byte *colormap = dsvars->colormap;

    unsigned short* dest = drawvars.byte_topleft + dsvars->y*SCREENPITCH + dsvars->x1;

    const unsigned int step = ((dsvars->xstep << 10) & 0xffff0000) | ((dsvars->ystep >> 6)  & 0x0000ffff);

    unsigned int position = ((dsvars->xfrac << 10) & 0xffff0000) | ((dsvars->yfrac >> 6)  & 0x0000ffff);

    unsigned int l = count >> 3;
    unsigned int r = count & 7;

    while(l--)
    {
        //P1
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P2
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P3
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P4
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P5
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P6
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P7
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;

        //P8
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;
    }

    while(r--)
    {
        *((byte*)dest) = colormap[source[((position >> 4) & 0x0fc0) | (position >> 26)]];
        dest++;
        position += step;
    }
}


static void R_MapPlane(int y, int x1, int x2, draw_span_vars_t *dsvars)
{
    angle_t angle;
    fixed_t distance, length;

    distance = FixedMul(planeheight, yslope[y]);
    dsvars->xstep = FixedMul(distance,basexscale);
    dsvars->ystep = FixedMul(distance,baseyscale);

    length = FixedMul (distance, distscale[x1]);
    angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;

    // killough 2/28/98: Add offsets
    dsvars->xfrac =  viewx + FixedMul(finecosine[angle], length);
    dsvars->yfrac = -viewy - FixedMul(finesine[angle],   length);

    if(fixedcolormap)
    {
        dsvars->colormap = fixedcolormap;
    }

    dsvars->y = y;
    dsvars->x1 = x1;
    dsvars->x2 = x2;

    R_DrawSpan(dsvars);
}

//
// R_MakeSpans
//

static void R_MakeSpans(int x, unsigned int t1, unsigned int b1, unsigned int t2, unsigned int b2, draw_span_vars_t *dsvars)
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

            an = viewangle;

            // Normal Doom sky, only one allowed per level
            dcvars.texturemid = skytexturemid;    // Default y-offset
            texture = _g->skytexture;             // Default texture
            flip = 0;                         // Doom flips it


          /* Sky is always drawn full bright, i.e. colormaps[0] is used.
           * Because of this hack, sky is not affected by INVUL inverse mapping.
           * Until Boom fixed this. Compat option added in MBF. */

            if (!(dcvars.colormap = fixedcolormap))
                dcvars.colormap = fullcolormap;          // killough 3/20/98

            // proff 09/21/98: Changed for high-res
            dcvars.iscale = FRACUNIT*200/viewheight;

            const texture_t* tex = R_GetOrLoadTexture(texture);

            const unsigned int widthmask = tex->widthmask;
            const patch_t* patch = tex->patches[0].patch;

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

            int stop;
            draw_span_vars_t dsvars;

            dsvars.source = W_CacheLumpNum(_g->firstflat + flattranslation[pl->picnum]);

            planeheight = D_abs(pl->height-viewz);

            dsvars.colormap = R_LoadColorMap(pl->lightlevel);

            stop = pl->maxx + 1;


            pl->top[pl->minx-1] = pl->top[stop] = 0xff; // dropoff overflow

            for (x = pl->minx ; x <= stop ; x++)
            {
                R_MakeSpans(x,pl->top[x-1],pl->bottom[x-1], pl->top[x],pl->bottom[x], &dsvars);
            }
        }
    }
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
  int     anglea = ANG90 + (visangle-viewangle);
  int     angleb = ANG90 + (visangle-rw_normalangle);

  int     den = FixedMul(rw_distance, finesine[anglea>>ANGLETOFINESHIFT]);

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
    if (_g->num_vissprite >= MAXVISSPRITES)
    {
#ifdef RANGECHECK
        I_Error("Vissprite overflow.");
#endif
        return NULL;
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

    fx = thing->x;
    fy = thing->y;
    fz = thing->z;

    tr_x = fx - viewx;
    tr_y = fy - viewy;

    gxt = FixedMul(tr_x,viewcos);
    gyt = -FixedMul(tr_y,viewsin);

    tz = gxt-gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    xscale = FixedDiv(projection, tz);

    gxt = -FixedMul(tr_x,viewsin);
    gyt = FixedMul(tr_y,viewcos);
    tx = -(gyt+gxt);

    // too far off the side?
    if (D_abs(tx)>(tz<<2))
        return;

    // decide which patch to use for sprite relative to player
    sprdef = &_g->sprites[thing->sprite];
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

    const patch_t* patch = W_CacheLumpNum(lump + _g->firstspritelump);

    /* calculate edges of the shape
     * cph 2003/08/1 - fraggle points out that this offset must be flipped
     * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
    if (flip)
    {
        tx -= (patch->width - patch->leftoffset) << FRACBITS;
    } else
    {
        tx -= patch->leftoffset << FRACBITS;
    }
    x1 = (centerxfrac + FixedMul(tx,xscale)) >> FRACBITS;

    // off the side?
    if(x1 > SCREENWIDTH)
        return;

    tx += patch->width<<FRACBITS;

    x2 = ((centerxfrac + FixedMul (tx,xscale) ) >> FRACBITS) - 1;

    // off the side?
    if(x2 < 0)
        return;


    gzt = fz + (patch->topoffset << FRACBITS);
    width = patch->width;

    // store information in a vissprite
    vis = R_NewVisSprite ();

    //No more vissprites.
    if(!vis)
        return;

    vis->mobjflags = thing->flags;
    // proff 11/06/98: Changed for high-res
    vis->scale = FixedDiv(projectiony, tz);
    vis->gx = fx;
    vis->gy = fy;
    vis->gz = fz;
    vis->gzt = gzt;                          // killough 3/27/98
    vis->texturemid = vis->gzt - viewz;
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
    else if (fixedcolormap)
        vis->colormap = fixedcolormap;      // fixed map
    else if (thing->frame & FF_FULLBRIGHT)
        vis->colormap = fullcolormap;     // full bright  // killough 3/20/98
    else
    {      // diminished light
        vis->colormap = R_ColourMap(lightlevel);
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
        check = Z_Calloc(1, sizeof(visplane_t), PU_LEVEL, NULL);
    else
    {
        if (!(_g->freetail = _g->freetail->next))
            _g->freehead = &_g->freetail;
    }

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

    BlockSet(check->top, UINT_MAX, sizeof(check->top));

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
    new_pl->minx = start;
    new_pl->maxx = stop;

    BlockSet(new_pl->top, UINT_MAX, sizeof(new_pl->top));

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

static void R_DrawColumnInCache(const column_t* patch, byte* cache, int originy, int cacheheight)
{
    int     count;
    int		position;
    const byte*	source;

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
            ByteCopy(cache + position, source, count);

        patch = (const column_t *)(  (const byte *)patch + patch->length + 4);
    }
}

/*
 * Draw a column of pixels of the specified texture.
 * If the texture is simple (1 patch, full height) then just draw
 * straight from const patch_t*.
*/

static unsigned int FindColumnCacheItem(unsigned int texture, unsigned int column)
{
    //static unsigned int looks, peeks;
    //looks++;

    unsigned int cx = (column << 16 | texture);

    unsigned int key = ((column >> 4) ^ texture) & 0xf;

    unsigned int* cc = (unsigned int*)&columnCacheEntries[key];

    unsigned int i = key;

    do
    {
        //peeks++;
        unsigned int cy = *cc;

        if( (cy == 0) || (cy == cx) )
            return i;

        cc+=16;
        i+=16;

    } while(i < 128);

    //No space. Random eviction.
    return ((P_Random() & 7) * 16) + key;
}

static void R_DrawSegTextureColumn(unsigned int texture, int texcolumn, draw_column_vars_t* dcvars)
{
    //static int total, misses;

    const texture_t* tex = R_GetOrLoadTexture(texture);

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
        int colmask = 0xfffe;

        if(dcvars->iscale > (4 << FRACBITS))
            colmask = 0xfff0;
        else if (dcvars->iscale > (2 << FRACBITS))
            colmask = 0xfff8;

        const int xc = (texcolumn & colmask) & tex->widthmask;

        unsigned int cachekey = FindColumnCacheItem(texture, xc);

        byte* colcache = &columnCache[cachekey*128];
        column_cache_entry_t* cacheEntry = &columnCacheEntries[cachekey];

        //total++;

        if((cacheEntry->texture != texture) || cacheEntry->column != xc)
        {
            //misses++;

            byte tmpCache[128];

            cacheEntry->texture = texture & 0xffff;
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
                                         tmpCache,
                                         patch->originy,
                                         tex->height);

                }
            }

            BlockCopy(colcache, tmpCache, 128);
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

    for ( ; rw_x < rw_stopx ; rw_x++)
    {

        // mark floor / ceiling areas

        int yh = bottomfrac>>HEIGHTBITS;
        int yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

        // no space above wall?
        int bottom,top = ceilingclip[rw_x]+1;

        if (yl < top)
            yl = top;

        if (markceiling)
        {
            bottom = yl-1;

            if (bottom >= floorclip[rw_x])
                bottom = floorclip[rw_x]-1;

            if (top <= bottom)
            {
                ceilingplane->top[rw_x] = top;
                ceilingplane->bottom[rw_x] = bottom;
            }
            // SoM: this should be set here
            ceilingclip[rw_x] = bottom;
        }

        bottom = floorclip[rw_x]-1;
        if (yh > bottom)
            yh = bottom;

        if (markfloor)
        {

            top  = yh < ceilingclip[rw_x] ? ceilingclip[rw_x] : yh;

            if (++top <= bottom)
            {
                floorplane->top[rw_x] = top;
                floorplane->bottom[rw_x] = bottom;
            }
            // SoM: This should be set here to prevent overdraw
            floorclip[rw_x] = top;
        }

        // texturecolumn and lighting are independent of wall tiers
        if (segtextured)
        {
            // calculate texture offset
            angle_t angle =(rw_centerangle+xtoviewangle[rw_x])>>ANGLETOFINESHIFT;

            texturecolumn = rw_offset-FixedMul(finetangent[angle],rw_distance);

            texturecolumn >>= FRACBITS;

            dcvars.colormap = R_LoadColorMap(rw_lightlevel);

            dcvars.x = rw_x;

            dcvars.iscale = IDiv32(INT_MAX, ((unsigned)rw_scale) >> 1);

            //dcvars.iscale = IDiv32(0xffffffffu >> 1, ((unsigned)rw_scale) >> 1);
        }

        // draw the wall tiers
        if (midtexture)
        {

            dcvars.yl = yl;     // single sided line
            dcvars.yh = yh;
            dcvars.texturemid = rw_midtexturemid;
            //

            R_DrawSegTextureColumn(midtexture, texturecolumn, &dcvars);

            ceilingclip[rw_x] = viewheight;
            floorclip[rw_x] = -1;
        }
        else
        {

            // two sided line
            if (toptexture)
            {
                // top wall
                int mid = pixhigh>>HEIGHTBITS;
                pixhigh += pixhighstep;

                if (mid >= floorclip[rw_x])
                    mid = floorclip[rw_x]-1;

                if (mid >= yl)
                {
                    dcvars.yl = yl;
                    dcvars.yh = mid;
                    dcvars.texturemid = rw_toptexturemid;

                    R_DrawSegTextureColumn(toptexture, texturecolumn, &dcvars);

                    ceilingclip[rw_x] = mid;
                }
                else
                    ceilingclip[rw_x] = yl-1;
            }
            else  // no top wall
            {

                if (markceiling)
                    ceilingclip[rw_x] = yl-1;
            }

            if (bottomtexture)          // bottom wall
            {
                int mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
                pixlow += pixlowstep;

                // no space above wall?
                if (mid <= ceilingclip[rw_x])
                    mid = ceilingclip[rw_x]+1;

                if (mid <= yh)
                {
                    dcvars.yl = mid;
                    dcvars.yh = yh;
                    dcvars.texturemid = rw_bottomtexturemid;

                    R_DrawSegTextureColumn(bottomtexture, texturecolumn, &dcvars);

                    floorclip[rw_x] = mid;
                }
                else
                    floorclip[rw_x] = yh+1;
            }
            else        // no bottom wall
            {
                if (markfloor)
                    floorclip[rw_x] = yh+1;
            }

            // cph - if we completely blocked further sight through this column,
            // add this info to the solid columns array for r_bsp.c
            if ((markceiling || markfloor) && (floorclip[rw_x] <= ceilingclip[rw_x] + 1))
            {
                solidcol[rw_x] = 1; _g->didsolidcol = 1;
            }

            // save texturecol for backdrawing of masked mid texture
            if (maskedtexture)
                maskedtexturecol[rw_x] = texturecolumn;
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;
    }
}

static boolean R_CheckOpenings(const int start)
{
    int pos = _g->lastopening - _g->openings;
    int need = (rw_stopx - start)*4 + pos;

#ifdef RANGECHECK
    if(need > MAXOPENINGS)
        I_Error("Openings overflow. Need = %d", need);
#endif

    return need <= MAXOPENINGS;
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

    // don't overflow and crash
    if (ds_p == &_g->drawsegs[MAXDRAWSEGS])
    {
#ifdef RANGECHECK
        I_Error("Drawsegs overflow.");
#endif
        return;
    }


    linedata_t* linedata = &_g->linedata[curline->linenum];

    linedata->r_flags |= ML_MAPPED;

    sidedef = &_g->sides[curline->sidenum];
    linedef = &_g->lines[curline->linenum];


    // mark the segment as visible for auto map
    linedata->r_flags |= ML_MAPPED;
    //linedef->flags |= ML_MAPPED;

    // calculate rw_distance for scale calculation
    rw_normalangle = curline->angle + ANG90;

    offsetangle = rw_normalangle-rw_angle1;

    if (D_abs(offsetangle) > ANG90)
        offsetangle = ANG90;

    hyp = (viewx==curline->v1.x && viewy==curline->v1.y)?
                0 : R_PointToDist (curline->v1.x, curline->v1.y);

    rw_distance = FixedMul(hyp, finecosine[offsetangle>>ANGLETOFINESHIFT]);

    ds_p->x1 = rw_x = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop+1;

    //Openings overflow. Nevermind.
    if(!R_CheckOpenings(start))
        return;

    // calculate scale at both ends and step
    ds_p->scale1 = rw_scale = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start]);

    if (stop > start)
    {
        ds_p->scale2 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop]);
        ds_p->scalestep = rw_scalestep = IDiv32(ds_p->scale2-rw_scale, stop-start);
    }
    else
        ds_p->scale2 = ds_p->scale1;

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed

    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    ds_p->maskedtexturecol = NULL;

    if (!backsector)
    {
        // single sided line
        midtexture = texturetranslation[sidedef->midtexture];

        // a single sided line is terminal, so it must mark ends
        markfloor = markceiling = true;

        if (linedef->flags & ML_DONTPEGBOTTOM)
        {         // bottom of texture at bottom
            fixed_t vtop = frontsector->floorheight + textureheight[sidedef->midtexture];
            rw_midtexturemid = vtop - viewz;
        }
        else        // top of texture at top
            rw_midtexturemid = worldtop;

        rw_midtexturemid += FixedMod(sidedef->rowoffset, textureheight[midtexture]);

        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = screenheightarray;
        ds_p->sprbottomclip = negonearray;
        ds_p->bsilheight = INT_MAX;
        ds_p->tsilheight = INT_MIN;
    }
    else      // two sided line
    {
        ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
        ds_p->silhouette = 0;

        if(linedata->r_flags & RF_CLOSED)
        { /* cph - closed 2S line e.g. door */
            // cph - killough's (outdated) comment follows - this deals with both
            // "automap fixes", his and mine
            // killough 1/17/98: this test is required if the fix
            // for the automap bug (r_bsp.c) is used, or else some
            // sprites will be displayed behind closed doors. That
            // fix prevents lines behind closed doors with dropoffs
            // from being displayed on the automap.

            ds_p->silhouette = SIL_BOTH;
            ds_p->sprbottomclip = negonearray;
            ds_p->bsilheight = INT_MAX;
            ds_p->sprtopclip = screenheightarray;
            ds_p->tsilheight = INT_MIN;

        }
        else
        { /* not solid - old code */

            if (frontsector->floorheight > backsector->floorheight)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = frontsector->floorheight;
            }
            else
                if (backsector->floorheight > viewz)
                {
                    ds_p->silhouette = SIL_BOTTOM;
                    ds_p->bsilheight = INT_MAX;
                }

            if (frontsector->ceilingheight < backsector->ceilingheight)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = frontsector->ceilingheight;
            }
            else
                if (backsector->ceilingheight < viewz)
                {
                    ds_p->silhouette |= SIL_TOP;
                    ds_p->tsilheight = INT_MIN;
                }
        }

        worldhigh = backsector->ceilingheight - viewz;
        worldlow = backsector->floorheight - viewz;

        // hack to allow height changes in outdoor areas
        if (frontsector->ceilingpic == _g->skyflatnum && backsector->ceilingpic == _g->skyflatnum)
            worldtop = worldhigh;

        markfloor = worldlow != worldbottom
                || backsector->floorpic != frontsector->floorpic
                || backsector->lightlevel != frontsector->lightlevel
                ;

        markceiling = worldhigh != worldtop
                || backsector->ceilingpic != frontsector->ceilingpic
                || backsector->lightlevel != frontsector->lightlevel
                ;

        if (backsector->ceilingheight <= frontsector->floorheight || backsector->floorheight >= frontsector->ceilingheight)
            markceiling = markfloor = true;   // closed door

        if (worldhigh < worldtop)   // top texture
        {
            toptexture = texturetranslation[sidedef->toptexture];
            rw_toptexturemid = linedef->flags & ML_DONTPEGTOP ? worldtop :
                                                                        backsector->ceilingheight+textureheight[sidedef->toptexture]-viewz;
            rw_toptexturemid += FixedMod(sidedef->rowoffset, textureheight[toptexture]);
        }

        if (worldlow > worldbottom) // bottom texture
        {
            bottomtexture = texturetranslation[sidedef->bottomtexture];
            rw_bottomtexturemid = linedef->flags & ML_DONTPEGBOTTOM ? worldtop : worldlow;

            rw_bottomtexturemid += FixedMod(sidedef->rowoffset, textureheight[bottomtexture]);
        }

        // allocate space for masked texture tables
        if (sidedef->midtexture)    // masked midtexture
        {
            maskedtexture = true;
            ds_p->maskedtexturecol = maskedtexturecol = _g->lastopening - rw_x;
            _g->lastopening += rw_stopx - rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    segtextured = ((midtexture | toptexture | bottomtexture | maskedtexture) > 0);

    if (segtextured)
    {
        rw_offset = FixedMul (hyp, -finesine[offsetangle >>ANGLETOFINESHIFT]);

        rw_offset += sidedef->textureoffset + curline->offset;

        rw_centerangle = ANG90 + viewangle - rw_normalangle;

        rw_lightlevel = frontsector->lightlevel;
    }

    // if a floor / ceiling plane is on the wrong side of the view
    // plane, it is definitely invisible and doesn't need to be marked.
    if (frontsector->floorheight >= viewz)       // above view plane
        markfloor = false;
    if (frontsector->ceilingheight <= viewz &&
            frontsector->ceilingpic != _g->skyflatnum)   // below view plane
        markceiling = false;

    // calculate incremental stepping values for texture edges
    worldtop >>= 4;
    worldbottom >>= 4;

    topstep = -FixedMul (rw_scalestep, worldtop);
    topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

    bottomstep = -FixedMul (rw_scalestep,worldbottom);
    bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);

    if (backsector)
    {
        worldhigh >>= 4;
        worldlow >>= 4;

        if (worldhigh < worldtop)
        {
            pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
            pixhighstep = -FixedMul (rw_scalestep,worldhigh);
        }
        if (worldlow > worldbottom)
        {
            pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
            pixlowstep = -FixedMul (rw_scalestep,worldlow);
        }
    }

    // render it
    if (markceiling)
    {
        if (ceilingplane)   // killough 4/11/98: add NULL ptr checks
            ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
        else
            markceiling = 0;
    }

    if (markfloor)
    {
        if (floorplane)     // killough 4/11/98: add NULL ptr checks
            /* cph 2003/04/18  - ceilingplane and floorplane might be the same
       * visplane (e.g. if both skies); R_CheckPlane doesn't know about
       * modifications to the plane that might happen in parallel with the check
       * being made, so we have to override it and split them anyway if that is
       * a possibility, otherwise the floor marking would overwrite the ceiling
       * marking, resulting in HOM. */
            if (markceiling && ceilingplane == floorplane)
                floorplane = R_DupPlane (floorplane, rw_x, rw_stopx-1);
            else
                floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
        else
            markfloor = 0;
    }

    _g->didsolidcol = 0;
    R_RenderSegLoop();

    /* cph - if a column was made solid by this wall, we _must_ save full clipping info */
    if (backsector && _g->didsolidcol)
    {
        if (!(ds_p->silhouette & SIL_BOTTOM))
        {
            ds_p->silhouette |= SIL_BOTTOM;
            ds_p->bsilheight = backsector->floorheight;
        }
        if (!(ds_p->silhouette & SIL_TOP))
        {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = backsector->ceilingheight;
        }
    }

    // save sprite clipping info
    if ((ds_p->silhouette & SIL_TOP || maskedtexture) && !ds_p->sprtopclip)
    {
        ByteCopy(_g->lastopening, ceilingclip+start, sizeof(short)*(rw_stopx-start));
        ds_p->sprtopclip = _g->lastopening - start;
        _g->lastopening += rw_stopx - start;
    }

    if ((ds_p->silhouette & SIL_BOTTOM || maskedtexture) && !ds_p->sprbottomclip)
    {
        ByteCopy(_g->lastopening, floorclip+start, sizeof(short)*(rw_stopx-start));
        ds_p->sprbottomclip = _g->lastopening - start;
        _g->lastopening += rw_stopx - start;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
    {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
    {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }

    ds_p++;
}


// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// cph - converted to R_RecalcLineFlags. This recalculates all the flags for
// a line, including closure and texture tiling.

static void R_RecalcLineFlags(void)
{
    linedata_t* linedata = &_g->linedata[linedef->lineno];

    const side_t* side = &_g->sides[curline->sidenum];

    linedata->r_validcount = (_g->gametic & 0xffff);

    /* First decide if the line is closed, normal, or invisible */
    if (!(linedef->flags & ML_TWOSIDED)
            || backsector->ceilingheight <= frontsector->floorheight
            || backsector->floorheight >= frontsector->ceilingheight
            || (
                // if door is closed because back is shut:
                backsector->ceilingheight <= backsector->floorheight

                // preserve a kind of transparent door/lift special effect:
                && (backsector->ceilingheight >= frontsector->ceilingheight ||
                    side->toptexture)

                && (backsector->floorheight <= frontsector->floorheight ||
                    side->bottomtexture)

                // properly render skies (consider door "open" if both ceilings are sky):
                && (backsector->ceilingpic !=_g->skyflatnum ||
                    frontsector->ceilingpic!=_g->skyflatnum)
                )
            )
        linedata->r_flags = (RF_CLOSED | (linedata->r_flags & ML_MAPPED));
    else
    {
        // Reject empty lines used for triggers
        //  and special events.
        // Identical floor and ceiling on both sides,
        // identical light levels on both sides,
        // and no middle texture.
        // CPhipps - recode for speed, not certain if this is portable though
        if (backsector->ceilingheight != frontsector->ceilingheight
                || backsector->floorheight != frontsector->floorheight
                || side->midtexture
                || backsector->ceilingpic != frontsector->ceilingpic
                || backsector->floorpic != frontsector->floorpic
                || backsector->lightlevel != frontsector->lightlevel)
        {
            linedata->r_flags = (linedata->r_flags & ML_MAPPED); return;
        } else
            linedata->r_flags = (RF_IGNORE | (linedata->r_flags & ML_MAPPED));
    }

    /* cph - I'm too lazy to try and work with offsets in this */
    if (side->rowoffset) return;

    /* Now decide on texture tiling */
    if (linedef->flags & ML_TWOSIDED)
    {
        int c;

        /* Does top texture need tiling */
        if ((c = frontsector->ceilingheight - backsector->ceilingheight) > 0 &&
                (textureheight[texturetranslation[side->toptexture]] > c))
            linedata->r_flags |= RF_TOP_TILE;

        /* Does bottom texture need tiling */
        if ((c = frontsector->floorheight - backsector->floorheight) > 0 &&
                (textureheight[texturetranslation[side->bottomtexture]] > c))
            linedata->r_flags |= RF_BOT_TILE;
    } else {
        int c;
        /* Does middle texture need tiling */
        if ((c = frontsector->ceilingheight - frontsector->floorheight) > 0 &&
                (textureheight[texturetranslation[side->midtexture]] > c))
            linedata->r_flags |= RF_MID_TILE;
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
        if (solidcol[first])
        {
            if (!(p = ByteFind(solidcol+first, 0, last-first)))
                return; // All solid

            first = p - solidcol;
        }
        else
        {
            int to;
            if (!(p = ByteFind(solidcol+first, 1, last-first)))
                to = last;
            else
                to = p - solidcol;

            R_StoreWallRange(first, to-1);

            if (solid)
            {
                //memset(solidcol+first,1,to-first);
                ByteSet(solidcol+first, 1, to-first);
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

static void R_AddLine (const seg_t *line)
{
    int      x1;
    int      x2;
    angle_t  angle1;
    angle_t  angle2;
    angle_t  span;
    angle_t  tspan;

    curline = line;

    angle1 = R_PointToAngle (line->v1.x, line->v1.y);
    angle2 = R_PointToAngle (line->v2.x, line->v2.y);

    // Clip to view edges.
    span = angle1 - angle2;

    // Back side, i.e. backface culling
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

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

    backsector = SG_BACKSECTOR(line);

    /* cph - roll up linedef properties in flags */
    linedef = &_g->lines[curline->linenum];
    linedata_t* linedata = &_g->linedata[linedef->lineno];

    if (linedata->r_validcount != (_g->gametic & 0xffff))
        R_RecalcLineFlags();

    if (linedata->r_flags & RF_IGNORE)
    {
        return;
    }
    else
        R_ClipWallSegment (x1, x2, linedata->r_flags & RF_CLOSED);
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
    const seg_t       *line;
    subsector_t *sub;

    sub = &_g->subsectors[num];
    frontsector = sub->sector;
    count = sub->numlines;
    line = &_g->segs[sub->firstline];

    if(frontsector->floorheight < viewz)
    {
        floorplane = R_FindPlane(frontsector->floorheight,
                                     frontsector->floorpic,
                                     frontsector->lightlevel                // killough 3/16/98
                                     );
    }
    else
    {
        floorplane = NULL;
    }


    if(frontsector->ceilingheight > viewz || (frontsector->ceilingpic == _g->skyflatnum))
    {
        ceilingplane = R_FindPlane(frontsector->ceilingheight,     // killough 3/8/98
                                       frontsector->ceilingpic,
                                       frontsector->lightlevel
                                       );
    }
    else
    {
        ceilingplane = NULL;
    }

    R_AddSprites(sub, frontsector->lightlevel);
    while (count--)
    {
        R_AddLine (line);
        line++;
        curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
    }
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//

static const byte checkcoord[12][4] = // killough -- static const
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
        const byte* check;

        // Find the corners of the box
        // that define the edges from current viewpoint.
        boxpos = (viewx <= ((fixed_t)bspcoord[BOXLEFT]<<FRACBITS) ? 0 : viewx < ((fixed_t)bspcoord[BOXRIGHT]<<FRACBITS) ? 1 : 2) +
                (viewy >= ((fixed_t)bspcoord[BOXTOP]<<FRACBITS) ? 0 : viewy > ((fixed_t)bspcoord[BOXBOTTOM]<<FRACBITS) ? 4 : 8);

        if (boxpos == 5)
            return true;

        check = checkcoord[boxpos];
        angle1 = R_PointToAngle (((fixed_t)bspcoord[check[0]]<<FRACBITS), ((fixed_t)bspcoord[check[1]]<<FRACBITS)) - viewangle;
        angle2 = R_PointToAngle (((fixed_t)bspcoord[check[2]]<<FRACBITS), ((fixed_t)bspcoord[check[3]]<<FRACBITS)) - viewangle;
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

        if (!ByteFind(solidcol+sx1, 0, sx2-sx1)) return false;
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

// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

//Non recursive version.
//constant stack space used and easier to
//performance profile.
#define MAX_BSP_DEPTH 128

static void R_RenderBSPNode(int bspnum)
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

            bsp = &nodes[bspnum];
            side = R_PointOnSide (viewx, viewy, bsp);

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
        bsp = &nodes[bspnum];

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

            bsp = &nodes[bspnum];
        }

        bspnum = bsp->children[side^1];
    }
}


static void R_ClearDrawSegs(void)
{
    ds_p = _g->drawsegs;
}

static void R_ClearClipSegs (void)
{
    BlockSet(solidcol, 0, SCREENWIDTH);
}

//
// R_ClearSprites
// Called at frame start.
//

static void R_ClearSprites(void)
{
  _g->num_vissprite = 0;            // killough
}

//
// RDrawPlanes
// At the end of each frame.
//

static void R_DrawPlanes (void)
{
    visplane_t *pl;
    int i;
    for (i=0;i<MAXVISPLANES;i++)
        for (pl=_g->visplanes[i]; pl; pl=pl->next)
            R_DoDrawPlane(pl);
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
    R_RenderBSPNode (numnodes-1);

    R_DrawPlanes ();

    R_DrawMasked ();
}
