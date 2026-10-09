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

//This is to keep the codesize under control.
//This whole file needs to fit within IWRAM.
//Levels are set in code_opt.h.
#include "code_opt.h"
ARM_CODE_DEFAULT

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#ifndef GBA
    #include <time.h>
#endif

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
#include "i_system.h"
#include "m_random.h"

#include "global_data.h"

#include "gba_functions.h"

#include "vram_spare.h"


//#define static

//Tables in spare VRAM/OAM: layout in vram_spare.h.
#ifndef GBA
vram1_spare_t vram1_spare_storage;
vram_tail_t vram_tail_storage;
oam_spare_t oam_spare_storage;
objpal_spare_t objpal_spare_storage;
#endif

static_assert(sizeof(yslope) / sizeof(yslope[0]) >= SCREENHEIGHT - ST_SCALED_HEIGHT, "yslope must cover every view row");

//Read the VRAM copies of these tables, not the ROM ones.
#define yslope (vram1_spare->yslope)
#define distscale (vram1_spare->distscale)
#define xtoviewangle (vram1_spare->xtoviewangle)

//*****************************************
//Flat cache stuff.
//The flats are cached in the last 8kb of
//OBJ VRAM (vram_tail->flatCache), as VRAM
//is faster than ROM. 4 flat slots measured
//no faster than 2.
//*****************************************

//Lump held in each flat cache slot.
static int flatCacheLump[FLAT_CACHE_SLOTS] = {-1, -1};

//Screen area (in pixel pairs) drawn with each flat this frame.
//Used to pick which flats to cache for the next frame.
#define MAX_FLAT_STATS 16

typedef struct flat_stat_t
{
    int lump;
    unsigned int area;
} flat_stat_t;

static flat_stat_t flatStats[MAX_FLAT_STATS];
static unsigned int numFlatStats;



//*****************************************
//Globals.
//*****************************************

int numnodes;
const mapnode_t *nodes;

fixed_t  viewx, viewy, viewz;

angle_t  viewangle;

static byte solidcol[MAX_SCREENWIDTH];

static byte spanstart[MAX_SCREENHEIGHT];                // killough 2/8/98


static const seg_t     *curline;
static const side_t *sidedef;
static const line_t    *linedef;
static sector_t  *frontsector;
static sector_t  *backsector;
static drawseg_t *ds_p;

static visplane_t *floorplane, *ceilingplane;
static int             rw_angle1;

static angle_t         rw_normalangle; // angle to line origin
static fixed_t         rw_distance;

static int      rw_stopx;

static fixed_t  rw_scale;
static fixed_t  rw_scalestep;

static int      worldtop;
static int      worldbottom;

static int didsolidcol; /* True if at least one column was marked solid */

// True if any of the segs textures might be visible.
static bool  segtextured;
static bool  markfloor;      // False if the back side is the same plane.
static bool  markceiling;
static bool  maskedtexture;
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

const byte* texcolpool;
const texrun_t* texcolruns;

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

//Colormaps copied into IWRAM (ROM is slower), with the ROM copy each
//slot holds. Walls, flats and masked textures switch between a few
//light levels, so several slots avoid most of the copies.
#define COLORMAP_SLOTS 4

static lighttable_t colormapSlots[COLORMAP_SLOTS][256];
static const lighttable_t* colormapSlotSrc[COLORMAP_SLOTS];
static unsigned int colormapNextSlot;

static fixed_t planeheight;

size_t num_vissprite;

bool highDetail = false;



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

static const fixed_t pspriteyscale = (SCREENHEIGHT << FRACBITS) / 200;
static const fixed_t pspriteyiscale = ((UINT_MAX) / ((SCREENHEIGHT << FRACBITS) / 200));


static const angle_t clipangle = 537395200; //xtoviewangle[0];

static const int skytexturemid = 100*FRACUNIT;
static const fixed_t skyiscale = (FRACUNIT*200)/((SCREENHEIGHT-ST_HEIGHT)+16);


//********************************************
// On the GBA we exploit that an 8 bit write
// will mirror to the upper 8 bits too.
// it saves an OR and Shift per pixel.
//********************************************
#ifdef GBA
    typedef byte pixel;
#else
    typedef unsigned short pixel;
#endif

//********************************************
// This goes here as we want the Thumb code
// to BX to ARM as Thumb long mul is very slow.
//********************************************
inline fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b)
{
    return (fixed_t)((int_64_t) a*b >> FRACBITS);
}

//BSP traversal and wall setup are WARM_CODE (code_opt.h). Functions that
//call each other must share a level, or GCC won't inline them.
#define R_BSP_OPT WARM_CODE

//This is a hack. I want FixedMul inlined only in this file. Sorry, not sorry.

static inline __attribute__((always_inline)) fixed_t FixedMulInline(fixed_t a, fixed_t b)
{
    return (fixed_t)((int_64_t) a*b >> FRACBITS);
}

#define FixedMul FixedMulInline

//Same hack for FixedReciprocal. GCC won't inline the m_fixed.h version
//into functions with a different optimize attribute.
static inline __attribute__((always_inline)) fixed_t FixedReciprocalInline(const fixed_t v)
{
    unsigned int val = v < 0 ? -v : v;

    const unsigned int shift = shiftTable[val >> FRACBITS];

    const fixed_t result = (reciprocalTable[val >> shift] >> shift);

    return v < 0 ? -result : result;
}


//Inline copy of R_SetDefaultDrawColumnVars (r_draw.c, in ROM).
static inline __attribute__((always_inline)) void R_SetDefaultDrawColumnVarsInline(draw_column_vars_t *dcvars)
{
    dcvars->x = dcvars->yl = dcvars->yh = 0;
    dcvars->iscale = dcvars->texturemid = 0;
    dcvars->source = NULL;
    dcvars->colormap = colormaps;
}

#define R_SetDefaultDrawColumnVars R_SetDefaultDrawColumnVarsInline

static inline __attribute__((always_inline)) int min(int x, int y)
{
    return x < y ? x : y;
}

static inline __attribute__((always_inline)) int max(int x, int y)
{
    return x > y ? x : y;
}

static inline __attribute__((always_inline)) int clamp(int lo, int v, int hi)
{
    return min(max(v, lo), hi);
}

// killough 5/3/98: reformatted

//FixedApproxDiv with the reciprocal inlined (it's a call otherwise).
static inline __attribute__((always_inline)) fixed_t FixedApproxDivInline(const fixed_t a, const fixed_t b)
{
    return FixedMul(a, FixedReciprocalInline(b));
}

static CONSTFUNC R_BSP_OPT int SlopeDiv(unsigned num, unsigned den)
{
    const unsigned int ans = FixedApproxDivInline(num << 3, den >> 8) >> FRACBITS;

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

static PUREFUNC R_BSP_OPT int R_PointOnSide(fixed_t x, fixed_t y, const mapnode_t *node)
{
    const fixed_t nx = x - ((fixed_t)node->x << FRACBITS);
    const fixed_t ny = y - ((fixed_t)node->y << FRACBITS);

    return FixedMul(ny, node->dx) >= FixedMul(node->dy, nx);
}

//
// R_PointInSector
//
// killough 5/2/98: reformatted, cleaned up

sector_t *R_PointInSector(fixed_t x, fixed_t y)
{
    int nodenum = numnodes-1;

    // special case for trivial maps (single subsector, no nodes)
    if (numnodes == 0)
        return SS_SECTOR(_g->subsectors);

    while (!(nodenum & NF_SUBSECTOR))
        nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];
    return SS_SECTOR(&_g->subsectors[nodenum & ~NF_SUBSECTOR]);
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

CONSTFUNC R_BSP_OPT angle_t R_PointToAngle2(const fixed_t vx, const fixed_t vy, fixed_t x, fixed_t y)
{
    x -= vx;
    y -= vy;

    if (!x && !y)
        return 0;

    const int absx = (x < 0) ? -x : x;
    const int absy = (y < 0) ? -y : y;

    angle_t base;

    if (absx > absy)
    {
        const int slope = SlopeDiv(absy, absx);
        base = tantoangle[slope];
    }
    else
    {
        const int slope = SlopeDiv(absx, absy);
        base = ANG90 - 1 - tantoangle[slope];
    }

    if (x >= 0)
    {
        if (y < 0) base = -base;   // quadrant IV
    }
    else
    {
        if (y >= 0)
          base = ANG90 + (ANG90 - base); // quadrant II
        else
          base = ANG180 + base; // quadrant III
    }

    return base;
}

// killough 5/2/98: move from r_main.c, made static, simplified

static CONSTFUNC R_BSP_OPT fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    fixed_t dx = D_abs(x - viewx);
    fixed_t dy = D_abs(y - viewy);

    if (dy > dx)
    {
        fixed_t t = dx;
        dx = dy;
        dy = t;
    }

    return FixedApproxDivInline(dx, finesine[(tantoangle[FixedApproxDivInline(dy,dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT]);
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

        lightlevel += (extralight +_g->gamma) << LIGHTSEGSHIFT;

        int cm = ((256-lightlevel)>>2) - 24;

        if(cm >= NUMCOLORMAPS)
            cm = NUMCOLORMAPS-1;
        else if(cm < 0)
            cm = 0;

        return fullcolormap + cm*256;
    }
}


//Which colormap objpal_spare->fixedColormap holds.
static const lighttable_t* objpalFixedSrc;

//Colormaps used directly per pixel (not via R_LoadColorMap) are read
//from the OBJ palette copies if there is one, as ROM is slower.
static const lighttable_t* R_FastColormap(const lighttable_t* cm)
{
    if (cm == colormaps)
        return objpal_spare->fullColormap;

    if (cm && cm == fixedcolormap)
        return objpal_spare->fixedColormap;

    return cm;
}

//Load a colormap into IWRAM. The slot replaced is the oldest one. That
//is never one still in use, as every caller finishes drawing with its
//colormap before the next load.
static const lighttable_t* R_LoadColorMap(int lightlevel)
{
    const lighttable_t* lm = R_ColourMap(lightlevel);

    for(unsigned int i = 0; i < COLORMAP_SLOTS; i++)
    {
        if(colormapSlotSrc[i] == lm)
            return colormapSlots[i];
    }

    const unsigned int slot = colormapNextSlot;
    colormapNextSlot = (slot + 1) & (COLORMAP_SLOTS - 1);

    BlockCopy(colormapSlots[slot], lm, 256);
    colormapSlotSrc[slot] = lm;

    return colormapSlots[slot];
}

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
//




#define COLEXTRABITS 9
#define COLBITS (FRACBITS + COLEXTRABITS)

inline static void R_DrawColumnPixel(unsigned short* dest, const byte* source, const byte* colormap, unsigned int frac)
{
    pixel* d = (pixel*)dest;

#ifdef GBA
    *d = colormap[source[frac]];
#else
    unsigned int color = colormap[source[frac]];

    *d = (color | (color << 8));
#endif
}

static void __attribute__((flatten)) HOT_CODE R_DrawColumn (const draw_column_vars_t *dcvars)
{
    int count = (dcvars->yh - dcvars->yl) + 1;

    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;


#ifndef GBA
    if(dcvars->yh > viewheight)
    {
        lprintf("R_DrawColumn: yh too large (%d)\n", dcvars->yh);
        return;
    }
#endif

    const byte *source = dcvars->source;
    const byte *colormap = dcvars->colormap;

    unsigned short* dest = drawvars.byte_topleft + ScreenYToOffset(dcvars->yl) + dcvars->x;

    const unsigned int		fracstep = (dcvars->iscale << COLEXTRABITS);
    unsigned int frac = (dcvars->texturemid + (dcvars->yl - centery)*dcvars->iscale) << COLEXTRABITS;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.

    unsigned int l = (count >> 4);

    while(l--)
    {
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;

        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;

        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;

        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
        R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep;
    }

    unsigned int r = (count & 15);

    switch(r)
    {
        case 15:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 14:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 13:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 12:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 11:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 10:    R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 9:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 8:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 7:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 6:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 5:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 4:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 3:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 2:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS); dest+=SCREENWIDTH; frac+=fracstep; [[fallthrough]];
        case 1:     R_DrawColumnPixel(dest, source, colormap, frac >> COLBITS);
    }
}

static void R_DrawColumnHiRes(const draw_column_vars_t *dcvars)
{
    int count = (dcvars->yh - dcvars->yl) + 1;

    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;

    const byte *source = dcvars->source;
    const byte *colormap = dcvars->colormap;

    volatile unsigned short* dest = drawvars.byte_topleft + ScreenYToOffset(dcvars->yl) + dcvars->x;

    const unsigned int fracstep = (dcvars->iscale << COLEXTRABITS);
    unsigned int frac = (dcvars->texturemid + (dcvars->yl - centery)*dcvars->iscale) << COLEXTRABITS;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.

    unsigned int mask;
    unsigned int shift;

    if(!dcvars->odd_pixel)
    {
        mask = 0xff00;
        shift = 0;
    }
    else
    {
        mask = 0xff;
        shift = 8;
    }

    do
    {
        unsigned int old = *dest;
        const unsigned int color = colormap[source[frac>>COLBITS]];

        *dest = ((old & mask) | (color << shift));

        dest+=SCREENWIDTH;
        frac+=fracstep;

    } while(--count);

}

#define FUZZOFF (SCREENWIDTH)
#define FUZZTABLE 64

static const signed char fuzzoffset[FUZZTABLE] =
{
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF
};

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
static void R_DrawFuzzColumn (const draw_column_vars_t *dcvars)
{
    int dc_yl = dcvars->yl;
    int dc_yh = dcvars->yh;

    // Adjust borders. Low...
    if (dc_yl <= 0)
        dc_yl = 1;

    // .. and high.
    if (dc_yh >= viewheight-1)
        dc_yh = viewheight - 2;

    int count = (dc_yh - dc_yl) + 1;

    // Zero length, column does not exceed a pixel.
    if (count <= 0)
        return;

    const byte* colormap = &fullcolormap[6*256];

    unsigned short* dest = drawvars.byte_topleft + ScreenYToOffset(dc_yl) + dcvars->x;

    unsigned int fuzzpos = _g->fuzzpos;

    do
    {
        R_DrawColumnPixel(dest, (const byte*)(&dest[fuzzoffset[fuzzpos]]), colormap, 0);
        dest += SCREENWIDTH;
        fuzzpos++;

        fuzzpos &= (FUZZTABLE-1);
    } while(--count);

    _g->fuzzpos = fuzzpos;
}

//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
static void R_DrawMaskedColumn(R_DrawColumn_f colfunc, draw_column_vars_t *dcvars, const column_t *column)
{
    const fixed_t basetexturemid = dcvars->texturemid;

    const int fclip_x = mfloorclip[dcvars->x];
    const int cclip_x = mceilingclip[dcvars->x];

    while (column->topdelta != 0xff)
    {
        // calculate unclipped screen coordinates for post
        const int topscreen = sprtopscreen + spryscale*column->topdelta;
        const int bottomscreen = topscreen + spryscale*column->length;

        int yh = (bottomscreen-1)>>FRACBITS;
        int yl = (topscreen+FRACUNIT-1)>>FRACBITS;

        if(yh >= fclip_x)
            yh = fclip_x - 1;

        if(yl <= cclip_x)
            yl = cclip_x + 1;

        // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
        if (yh < viewheight && yl <= yh)
        {
            dcvars->source =  (const byte*)column + 3;

            dcvars->texturemid = basetexturemid - (column->topdelta<<FRACBITS);

            dcvars->yh = yh;
            dcvars->yl = yl;

            // Drawn by either R_DrawColumn
            //  or (SHADOW) R_DrawFuzzColumn.
            colfunc (dcvars);
        }

        column = (const column_t *)((const byte *)column + column->length + 4);
    }

    dcvars->texturemid = basetexturemid;
}

//*******************************************
// High detail sprites.
// Each pixel has two samples, at the centres of its left (low byte)
// and right halves. Where both halves come from the same texture
// column the pixel is drawn whole, otherwise each half is drawn.
// (Merging the two columns' posts to write both halves at once was
// tried: sprites draw few pixels, and the merge cost as much as it
// saved.)
//*******************************************

//vis->startfrac is the texel under the left half of x1. Halves whose
//sample is outside the patch (only at the sprite's edges) are skipped.
static void R_DrawVisSpriteHiRes(const vissprite_t* vis, draw_column_vars_t* dcvars)
{
    const patch_t* patch = vis->patch;
    const unsigned int width = patch->width;

    const fixed_t step = vis->xiscale >> 1; //Per half pixel.
    fixed_t frac = vis->startfrac;

    for (int x = vis->x1; x <= vis->x2; x++)
    {
        const fixed_t fracr = frac + step;

        const column_t* columnl = NULL;
        const column_t* columnr = NULL;

        if ((unsigned int)(frac >> FRACBITS) < width)
            columnl = (const column_t *)((const byte *)patch + patch->columnofs[frac >> FRACBITS]);

        if ((unsigned int)(fracr >> FRACBITS) < width)
            columnr = (const column_t *)((const byte *)patch + patch->columnofs[fracr >> FRACBITS]);

        dcvars->x = x;

        if (columnl && columnl == columnr)
        {
            //Both halves the same: draw the whole pixel.
            R_DrawMaskedColumn(R_DrawColumn, dcvars, columnl);
        }
        else
        {
            if (columnl)
            {
                dcvars->odd_pixel = false;
                R_DrawMaskedColumn(R_DrawColumnHiRes, dcvars, columnl);
            }

            if (columnr)
            {
                dcvars->odd_pixel = true;
                R_DrawMaskedColumn(R_DrawColumnHiRes, dcvars, columnr);
            }
        }

        frac = fracr + step;
    }
}

//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
// CPhipps - new wad lump handling, *'s to const*'s
static void R_DrawVisSprite(const vissprite_t *vis)
{
    fixed_t  frac;

    R_DrawColumn_f colfunc = R_DrawColumn;
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    dcvars.colormap = vis->colormap;

    // killough 4/11/98: rearrange and handle translucent sprites
    // mixed with translucent/non-translucenct 2s normals

    if (!dcvars.colormap)   // NULL colormap = shadow draw
        colfunc = R_DrawFuzzColumn;    // killough 3/14/98

    // proff 11/06/98: Changed for high-res
    dcvars.iscale = vis->iscale;
    dcvars.texturemid = vis->texturemid;
    frac = vis->startfrac;

    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);

    //Shadows are always low detail (see R_SpriteSamples).
    if (highDetail && dcvars.colormap)
    {
        R_DrawVisSpriteHiRes(vis, &dcvars);
        return;
    }

    const patch_t *patch = vis->patch;

    const fixed_t xiscale = vis->xiscale;

    for (dcvars.x = vis->x1; dcvars.x <= vis->x2; dcvars.x++)
    {
        const column_t* column = (const column_t *) ((const byte *)patch + patch->columnofs[frac >> FRACBITS]);
        R_DrawMaskedColumn(colfunc, &dcvars, column);

        frac += xiscale;

        if(((frac >> FRACBITS) >= patch->width) || frac < 0)
            break;
    }
}

//Composited 128 byte texture column, built by GbaWadUtil.
inline static const byte* R_GetTextureColumn(const texture_t* texture, int texcolumn)
{
    return texcolpool + (texture->colids[texcolumn & texture->widthmask] << 7);
}

//
// R_DrawMaskedTextureColumn
// Masked mid textures. Like R_DrawMaskedColumn, but the
// runs of opaque rows are separate from the pixels, which
// are the texture's composited column.
//
static void R_DrawMaskedTextureColumn(draw_column_vars_t *dcvars, const texture_t* texture, int texcolumn)
{
    const int xc = texcolumn & texture->widthmask;

    const texrun_t* run = &texcolruns[texture->colids[texture->width + xc]];

    dcvars->source = texcolpool + (texture->colids[xc] << 7);

    const int fclip_x = mfloorclip[dcvars->x];
    const int cclip_x = mceilingclip[dcvars->x];

    while (run->topdelta != 0xff)
    {
        // calculate unclipped screen coordinates for run
        const int topscreen = sprtopscreen + spryscale*run->topdelta;
        const int bottomscreen = topscreen + spryscale*run->length;

        int yh = (bottomscreen-1)>>FRACBITS;
        int yl = (topscreen+FRACUNIT-1)>>FRACBITS;

        if(yh >= fclip_x)
            yh = fclip_x - 1;

        if(yl <= cclip_x)
            yl = cclip_x + 1;

        if (yh < viewheight && yl <= yh)
        {
            dcvars->yh = yh;
            dcvars->yl = yl;

            R_DrawColumn (dcvars);
        }

        run++;
    }
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

static void R_RenderMaskedSegRange(const drawseg_t *ds, int x1, int x2)
{
    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?

    curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally

    frontsector = SG_FRONTSECTOR(curline);
    backsector = SG_BACKSECTOR(curline);

    const side_t* side = SIDE(curline->sidenum);

    const int texnum = texturetranslation[side->midtexture];

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

    dcvars.texturemid += (side->rowoffset << FRACBITS);

    const texture_t* texture = R_GetOrLoadTexture(texnum);

    dcvars.colormap = R_LoadColorMap(rw_lightlevel);

    // draw the columns
    for (dcvars.x = x1 ; dcvars.x <= x2 ; dcvars.x++, spryscale += rw_scalestep)
    {
        const int xc = maskedtexturecol[dcvars.x];

        if (xc != SHRT_MAX) // dropoff overflow
        {
            sprtopscreen = centeryfrac - FixedMul(dcvars.texturemid, spryscale);

            dcvars.iscale = FixedReciprocal((unsigned)spryscale);

            // draw the texture
            R_DrawMaskedTextureColumn(&dcvars, texture, xc);

            maskedtexturecol[dcvars.x] = SHRT_MAX; // dropoff overflow
        }
    }

    curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_ColourMap doesn't try using it for other things */
}


// killough 5/2/98: reformatted

static inline int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
    const fixed_t lx = (fixed_t)line->v1.x << FRACBITS;
    const fixed_t ly = (fixed_t)line->v1.y << FRACBITS;
    const int ldx = line->v2.x - line->v1.x;    // map units
    const int ldy = line->v2.y - line->v1.y;

    if (!ldx)
        return x <= lx ? ldy > 0 : ldy < 0;

    if (!ldy)
        return y <= ly ? ldx < 0 : ldx > 0;

    x -= lx;
    y -= ly;

    return FixedMul(y, ldx) >= FixedMul(ldy, x);
}

//
// R_DrawSprite
//

//Number of entries in vram_tail->dsclip this frame.
static unsigned int num_dsclip;

//Summarise the drawsegs that can clip sprites (those with a
//silhouette or a masked mid texture), in drawseg order.
static void R_BuildDrawsegClip(void)
{
    const drawseg_t* drawsegs = _g->drawsegs;
    drawseg_clip_t* dc = vram_tail->dsclip;

    for (const drawseg_t* ds = drawsegs; ds < ds_p; ds++)
    {
        if (!ds->silhouette && !ds->maskedtexturecol)
            continue;

        dc->xkey = ((unsigned int)ds->x2 << 16) | (255 - ds->x1);

        if (ds->scale1 > ds->scale2)
        {
            dc->lowscale = ds->scale2;
            dc->scale = ds->scale1;
        }
        else
        {
            dc->lowscale = ds->scale1;
            dc->scale = ds->scale2;
        }

        dc->index = ds - drawsegs;
        dc->masked = (ds->maskedtexturecol != NULL);

        dc++;
    }

    num_dsclip = dc - vram_tail->dsclip;
}

// Return the next summary entry below dc (scanning down to first)
// that overlaps the sprite, or NULL.
// Both halves of (xkey - skey) are >= 0 only if the drawseg
// overlaps the sprite: the high half is dx2 - sx1 and the
// low half is sx2 - dx1. Values are < 256, so a negative half
// sets bit 31 or bit 15. (A borrow from the low half only
// happens when the low test has already failed.)
// Kept out of line so the scan loop has registers to itself.
static __attribute__((noinline)) const drawseg_clip_t* R_NextClipSeg(const drawseg_clip_t* dc, const drawseg_clip_t* first, unsigned int skey)
{
    while (dc-- > first)
    {
        if (!((dc->xkey - skey) & 0x80008000))
            return dc;
    }

    return NULL;
}

static void R_DrawSprite (const vissprite_t* spr)
{
    short* clipbot = oam_spare->floorclip;
    short* cliptop = oam_spare->ceilingclip;

    const int sx1 = spr->x1;
    const int sx2 = spr->x2;
    const fixed_t sprscale = spr->scale;

    for (int x = sx1 ; x<=sx2 ; x++)
    {
        clipbot[x] = viewheight;
        cliptop[x] = -1;
    }


    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale is the clip seg.

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):

    // Scan the summary (R_BuildDrawsegClip) rather than the
    // drawsegs, and only read a drawseg if it covers the sprite.

    const unsigned int skey = ((unsigned int)sx1 << 16) | (255 - sx2);

    const drawseg_t* drawsegs  =_g->drawsegs;
    const drawseg_clip_t* dsclip = vram_tail->dsclip;

    // determine which drawsegs obscure the sprite
    for (const drawseg_clip_t* dc = dsclip + num_dsclip; (dc = R_NextClipSeg(dc, dsclip, skey)); )  // new -- killough
    {
        const unsigned int xkey = dc->xkey;

        const int dx1 = 255 - (int)(xkey & 0xffff);
        const int dx2 = xkey >> 16;

        const drawseg_t* ds = drawsegs + dc->index;

        const int r1 = dx1 < sx1 ? sx1 : dx1;
        const int r2 = dx2 > sx2 ? sx2 : dx2;

        if (dc->scale < sprscale || (dc->lowscale < sprscale && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline)))
        {
            if (dc->masked)       // masked mid texture?
                R_RenderMaskedSegRange(ds, r1, r2);

            continue;               // seg is behind sprite
        }

        // clip this piece of the sprite
        // killough 3/27/98: optimized and made much shorter

        if (ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight) //bottom sil
        {
            for (int x = r1; x <= r2; x++)
            {
                if (clipbot[x] == viewheight)
                    clipbot[x] = ds->sprbottomclip[x];
            }

        }

        fixed_t gzt = spr->gz + (spr->patch->topoffset << FRACBITS);

        if (ds->silhouette & SIL_TOP && gzt > ds->tsilheight)   // top sil
        {
            for (int x=r1; x <= r2; x++)
            {
                if (cliptop[x] == -1)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }
    }

    // all clipping has been performed, so draw the sprite
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr);
}


//Where a sprite pixel's samples are, as fractions of a pixel: its centre
//in low detail, or the centres of its left and right halves in high
//detail (shadows are always low detail). A pixel is drawn if any of its
//samples is inside the sprite, so the first and last pixels may have
//only one half inside it in high detail.
static void R_SpriteSamples(bool hires, fixed_t* first, fixed_t* last)
{
    *first = hires ? FRACUNIT/4 : FRACUNIT/2;
    *last = hires ? (3*FRACUNIT)/4 : FRACUNIT/2;
}

//
// R_DrawPSprite
//

static void R_DrawPSprite (pspdef_t *psp, int lightlevel)
{
    int           width;
    fixed_t       topoffset;

    // decide which patch to use
    const spritedef_t* sprdef = &_g->sprites[psp->state->sprite];
    const spriteframe_t* sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

    const patch_t* patch = W_CacheLumpNum(sprframe->lump[0]+_g->firstspritelump);

    // calculate edges of the shape
    fixed_t tx = psp->sx-160*FRACUNIT;

    tx -= patch->leftoffset<<FRACBITS;
    const fixed_t xl = centerxfrac + FixedMul (tx, pspritescale);

    tx += patch->width<<FRACBITS;
    const fixed_t xr = centerxfrac + FixedMul (tx, pspritescale);

    const bool shadow = _g->player.powers[pw_invisibility] > 4*32 || _g->player.powers[pw_invisibility] & 8;

    fixed_t sample1, sample2;
    R_SpriteSamples(highDetail && !shadow, &sample1, &sample2);

    // Columns with a sample within [xl, xr).
    const int x1 = (xl - sample2 + FRACUNIT - 1) >> FRACBITS;
    const int x2 = ((xr - sample1 + FRACUNIT - 1) >> FRACBITS) - 1;

    width = patch->width;
    topoffset = patch->topoffset<<FRACBITS;



    // off the side
    if (x2 < 0 || x1 >= SCREENWIDTH || x2 < x1)
        return;

    // store information in a vissprite
    vissprite_t vis;
    vis.mobjflags = 0;
    vis.texturemid = (BASEYCENTER<<FRACBITS) - (psp->sy-topoffset);
    vis.x1 = max(x1, 0);
    vis.x2 = min(x2, SCREENWIDTH-1);
    // proff 11/06/98: Added for high-res
    vis.scale = pspriteyscale;
    vis.iscale = pspriteyiscale;

    const bool flip = (bool) SPR_FLIPPED(sprframe, 0);

    // Texel under the first sample of the first drawn pixel.
    const fixed_t frac = FixedMul((vis.x1 << FRACBITS) + sample1 - xl, pspriteiscale);

    if (flip)
    {
        vis.xiscale = - pspriteiscale;
        vis.startfrac = ((width<<FRACBITS)-1) - frac;
    }
    else
    {
        vis.xiscale = pspriteiscale;
        vis.startfrac = frac;
    }

    vis.patch = patch;

    if (shadow)
        vis.colormap = NULL;                    // shadow draw
    else if (fixedcolormap)
        vis.colormap = R_FastColormap(fixedcolormap);           // fixed color
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis.colormap = R_FastColormap(fullcolormap);            // full bright // killough 3/20/98
    else
        vis.colormap = R_LoadColorMap(lightlevel);  // local light

    R_DrawVisSprite(&vis);
}



//
// R_DrawPlayerSprites
//

static void R_DrawPlayerSprites(void)
{

  int i, lightlevel = _g->player.mo->sector->lightlevel;
  pspdef_t *psp;

  // clip to screen bounds
  mfloorclip = oam_spare->screenheightarray;
  mceilingclip = oam_spare->negonearray;

  // add all active psprites
  for (i=0, psp=_g->player.psprites; i<NUMPSPRITES; i++,psp++)
    if (psp->state)
      R_DrawPSprite (psp, lightlevel);
}


//
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
//
static int compare (const void* l, const void* r)
{
    const vissprite_t* vl = *(const vissprite_t**)l;
    const vissprite_t* vr = *(const vissprite_t**)r;

    return vr->scale - vl->scale;
}

static void R_SortVisSprites (void)
{
    int i = num_vissprite;

    if (i)
    {
        while (--i>=0)
            vram1_spare->vissprite_ptrs[i] = _g->vissprites+i;

        qsort(vram1_spare->vissprite_ptrs, num_vissprite, sizeof (vissprite_t*), compare);
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

    if (num_vissprite)
        R_BuildDrawsegClip();

    // draw all vissprites back to front
    for (i = num_vissprite ;--i>=0; )
        R_DrawSprite(vram1_spare->vissprite_ptrs[i]);         // killough

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

inline static void R_DrawSpanPixel(unsigned short* dest, const byte* source, const byte* colormap, unsigned int position, unsigned int position2, unsigned int mask)
{
    const unsigned int p1 = colormap[source[((position >> 4) & mask) | (position >> 26)]];
    const unsigned int p2 = colormap[source[((position2 >> 4) & mask) | (position2 >> 26)]];

    *dest = (p1 | (p2 << 8));
}

static void R_DrawSpan(unsigned int y, unsigned int x1, const unsigned int count, const draw_span_vars_t *dsvars)
{
    const byte *source = dsvars->source;
    const byte *colormap = dsvars->colormap;

    unsigned short* dest = drawvars.byte_topleft + ScreenYToOffset(y) + x1;

    const unsigned int step = dsvars->step;
    unsigned int position = dsvars->position;

    //Keep the mask in a register so the shift folds into the AND.
    //(and rd, rmask, rpos, lsr #4) instead of (lsr) + (and #imm).
    unsigned int mask = 0x0fc0;

#ifdef GBA
    __asm__("" : "+r"(mask));
#endif

    unsigned int l = (count >> 3);

    if(l)
    {
        do
        {
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;

            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
            R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2;
        } while(--l);
    }

    const unsigned int r = (count & 7);

    switch(r)
    {
        case 7:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 6:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 5:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 4:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 3:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 2:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask); dest++; position+=step*2; [[fallthrough]];
        case 1:     R_DrawSpanPixel(dest, source, colormap, position, position + step, mask);
    }
}

static void __attribute__((flatten)) HOT_CODE R_MapPlane(unsigned int y, unsigned int x1, unsigned int x2, draw_span_vars_t *dsvars)
{
    const fixed_t distance = FixedMul(planeheight, yslope[y]);
    const fixed_t length = FixedMul (distance, distscale[x1]);
    const angle_t angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
    const unsigned int count = (x2 - x1);

    // killough 2/28/98: Add offsets
    const unsigned int xfrac =  viewx + FixedMul(finecosine[angle], length);
    const unsigned int yfrac = -viewy - FixedMul(finesine[angle],   length);

    dsvars->position = ((xfrac << 10) & 0xffff0000) | ((yfrac >> 6)  & 0x0000ffff);

    //UV steps are half-steps as we draw at full resolution. Shifts have been reduced/increased by 1
    dsvars->step = ((FixedMul(distance,basexscale) << 9) & 0xffff0000) | ((FixedMul(distance,baseyscale) >> 7) & 0x0000ffff);

    R_DrawSpan(y, x1, count, dsvars);
}

//
// R_MakeSpans
//

static void R_MakeSpans(int x, unsigned int t1, unsigned int b1, unsigned int t2, unsigned int b2, draw_span_vars_t *dsvars)
{
    for (; t1 < t2 && t1 <= b1; t1++)
        R_MapPlane(t1, spanstart[t1], x, dsvars);

    for (; b1 > b2 && b1 >= t1; b1--)
        R_MapPlane(b1, spanstart[b1], x, dsvars);

    while (t2 < t1 && t2 <= b2)
        spanstart[t2++] = x;

    while (b2 > b1 && b2 >= t2)
        spanstart[b2--] = x;
}



//*******************************************
// Flat cache.
// Flats are read from VRAM if cached as this
// avoids the ROM wait states on each texel.
//*******************************************

static const byte* R_GetFlat(int lump)
{
    for(unsigned int i = 0; i < FLAT_CACHE_SLOTS; i++)
    {
        if(flatCacheLump[i] == lump)
            return vram_tail->flatCache[i];
    }

    return W_CacheLumpNum(lump);
}

static void R_AddFlatStat(int lump, unsigned int area)
{
    for(unsigned int i = 0; i < numFlatStats; i++)
    {
        if(flatStats[i].lump == lump)
        {
            flatStats[i].area += area;
            return;
        }
    }

    if(numFlatStats < MAX_FLAT_STATS)
    {
        flatStats[numFlatStats].lump = lump;
        flatStats[numFlatStats].area = area;
        numFlatStats++;
    }
}

static void R_LoadFlat(unsigned int slot, int lump)
{
    if(W_LumpLength(lump) < FLAT_SIZE)
        return;

    //GbaWadUtil 4 byte aligns every lump.
    BlockCopy(vram_tail->flatCache[slot], W_CacheLumpNum(lump), FLAT_SIZE);

    flatCacheLump[slot] = lump;
}

//Copying a flat into VRAM costs roughly the same as the
//ROM wait states saved drawing this many pixel pairs.
#define FLAT_COPY_AREA 1536

//Cache the (up to) two flats that covered the most screen area
//this frame, ready for the next frame. Flats already cached get a
//bonus so similar sized flats don't thrash the cache.
static void R_UpdateFlatCache(void)
{
    int want[FLAT_CACHE_SLOTS];
    unsigned int wantArea[FLAT_CACHE_SLOTS];

    for(unsigned int s = 0; s < FLAT_CACHE_SLOTS; s++)
    {
        want[s] = -1;
        wantArea[s] = FLAT_COPY_AREA; //Not worth caching below this.
    }

    for(unsigned int i = 0; i < numFlatStats; i++)
    {
        const int lump = flatStats[i].lump;
        unsigned int area = flatStats[i].area;

        for(unsigned int s = 0; s < FLAT_CACHE_SLOTS; s++)
        {
            if(flatCacheLump[s] == lump)
                area += FLAT_COPY_AREA;
        }

        //Insert into the sorted wanted list.
        for(unsigned int w = 0; w < FLAT_CACHE_SLOTS; w++)
        {
            if(area > wantArea[w])
            {
                for(unsigned int j = FLAT_CACHE_SLOTS-1; j > w; j--)
                {
                    want[j] = want[j-1];
                    wantArea[j] = wantArea[j-1];
                }

                want[w] = lump;
                wantArea[w] = area;
                break;
            }
        }
    }

    numFlatStats = 0;

    //Keep slots that already hold a wanted flat.
    bool keep[FLAT_CACHE_SLOTS];

    for(unsigned int s = 0; s < FLAT_CACHE_SLOTS; s++)
    {
        keep[s] = false;

        for(unsigned int w = 0; w < FLAT_CACHE_SLOTS; w++)
        {
            if(want[w] != -1 && flatCacheLump[s] == want[w])
            {
                keep[s] = true;
                want[w] = -1;
            }
        }
    }

    //Load the rest into the remaining slots.
    for(unsigned int w = 0; w < FLAT_CACHE_SLOTS; w++)
    {
        if(want[w] == -1)
            continue;

        for(unsigned int s = 0; s < FLAT_CACHE_SLOTS; s++)
        {
            if(!keep[s])
            {
                R_LoadFlat(s, want[w]);
                keep[s] = true;
                break;
            }
        }
    }
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

            // Normal Doom sky, only one allowed per level
            dcvars.texturemid = skytexturemid;    // Default y-offset

          /* Sky is always drawn full bright, i.e. colormaps[0] is used.
           * Because of this hack, sky is not affected by INVUL inverse mapping.
           * Until Boom fixed this. Compat option added in MBF. */

            dcvars.colormap = R_FastColormap(fixedcolormap ? fixedcolormap : fullcolormap);          // killough 3/20/98

            // proff 09/21/98: Changed for high-res
            dcvars.iscale = skyiscale;

            const texture_t* tex = R_GetOrLoadTexture(_g->skytexture);

            // killough 10/98: Use sky scrolling offset
            for (x = pl->minx; (dcvars.x = x) <= pl->maxx; x++)
            {
                if ((dcvars.yl = pl->limits[x].top) != 0xff && dcvars.yl <= (dcvars.yh = pl->limits[x].bottom)) // dropoff overflow
                {
                    int xc = ((viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT);

                    dcvars.source = R_GetTextureColumn(tex, xc);
                    R_DrawColumn(&dcvars);
                }
            }
        }
        else
        {     // regular flat

            draw_span_vars_t dsvars;

            const int lump = _g->firstflat + flattranslation[pl->picnum];

            dsvars.source = R_GetFlat(lump);
            dsvars.colormap = R_LoadColorMap(pl->lightlevel);

            planeheight = D_abs(pl->height-viewz);

            const int stop = pl->maxx + 1;

            pl->limits[pl->minx-1].top = pl->limits[stop].top = 0xff; // dropoff overflow

            unsigned int area = 0;

            for (x = pl->minx ; x <= stop ; x++)
            {
                const unsigned int top = pl->limits[x].top;
                const unsigned int bottom = pl->limits[x].bottom;

                if(top != 0xff)
                    area += (bottom - top) + 1;

                R_MakeSpans(x,pl->limits[x-1].top,pl->limits[x-1].bottom, top, bottom, &dsvars);
            }

            R_AddFlatStat(lump, area);
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

static inline R_BSP_OPT fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    const int anglea = ANG90 + (visangle - viewangle);
    const int angleb = ANG90 + (visangle - rw_normalangle);

    const fixed_t den = FixedMul(rw_distance, finesine[anglea >> ANGLETOFINESHIFT]);
    const fixed_t num = FixedMul(projectiony, finesine[angleb >> ANGLETOFINESHIFT]);

    if (den <= (num >> 16))
        return 64 * FRACUNIT;

    const fixed_t scale = FixedDiv(num, den);

    return clamp(256, scale, 64 * FRACUNIT);
}

//
// R_NewVisSprite
//
static vissprite_t *R_NewVisSprite(void)
{
    if (num_vissprite >= MAXVISSPRITES)
    {
#ifdef RANGECHECK
        I_Error("Vissprite overflow.");
#endif
        return NULL;
    }

    return _g->vissprites + num_vissprite++;
}


//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//

static void R_ProjectSprite (mobj_t* thing, int lightlevel)
{
    const fixed_t fx = thing->x;
    const fixed_t fy = thing->y;
    const fixed_t fz = thing->z;

    const fixed_t tr_x = fx - viewx;
    const fixed_t tr_y = fy - viewy;

    const fixed_t tz = FixedMul(tr_x,viewcos)-(-FixedMul(tr_y,viewsin));

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    //Too far away. Always draw Cyberdemon and Spiderdemon. They are big sprites!
    if( (tz > MAXZ) && (thing->type != MT_CYBORG) && (thing->type != MT_SPIDER) )
        return;

    fixed_t tx = -(FixedMul(tr_y,viewcos)+(-FixedMul(tr_x,viewsin)));

    // too far off the side?
    if (D_abs(tx)>(tz<<2))
        return;

    // decide which patch to use for sprite relative to player
    const spritedef_t* sprdef = &_g->sprites[thing->state->sprite];
    const spriteframe_t* sprframe = &sprdef->spriteframes[thing->state->frame & FF_FRAMEMASK];

    unsigned int rot = 0;

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        angle_t ang = R_PointToAngle2(viewx, viewy, fx, fy);
        rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
    }

    const bool flip = (bool)SPR_FLIPPED(sprframe, rot);
    const patch_t* patch = W_CacheLumpNum(sprframe->lump[rot] + _g->firstspritelump);

    /* calculate edges of the shape
     * cph 2003/08/1 - fraggle points out that this offset must be flipped
     * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
    if (flip)
        tx -= (patch->width - patch->leftoffset) << FRACBITS;
    else
        tx -= patch->leftoffset << FRACBITS;

    const fixed_t xscale = FixedDiv(projection, tz);

    const fixed_t xl = (centerxfrac + FixedMul(tx,xscale));

    fixed_t sample1, sample2;
    R_SpriteSamples(highDetail && !(thing->flags & MF_SHADOW), &sample1, &sample2);

    // off the side?
    if(xl > ((SCREENWIDTH << FRACBITS) - (FRACUNIT - sample2)))
        return;


    const fixed_t xr = (centerxfrac + FixedMul(tx + (patch->width << FRACBITS),xscale));

    // off the side?
    if(xr <= sample1)
        return;

    //Too small.
    if(xr <= (xl + FRACUNIT + (FRACUNIT >> 2)))
        return;

    // Columns with a sample within [xl, xr).
    const int x1 = (xl - sample2 + FRACUNIT - 1) >> FRACBITS;
    const int x2 = ((xr - sample1 + FRACUNIT - 1) >> FRACBITS) - 1;

    // store information in a vissprite
    vissprite_t* vis = R_NewVisSprite();

    //No more vissprites.
    if(!vis)
        return;

    vis->mobjflags = thing->flags;
    // proff 11/06/98: Changed for high-res
    vis->scale = FixedDiv(projectiony, tz);
    vis->iscale = tz >> 7;
    vis->patch = patch;
    vis->gx = fx;
    vis->gy = fy;
    vis->gz = fz;
    vis->texturemid = (fz + (patch->topoffset << FRACBITS)) - viewz;
    vis->x1 = max(x1, 0);
    vis->x2 = min(x2, SCREENWIDTH-1);

    const fixed_t iscale = FixedReciprocal(xscale);

    // Texel under the first sample of the first drawn pixel.
    const fixed_t frac = FixedMul((vis->x1 << FRACBITS) + sample1 - xl, iscale);

    if (flip)
    {
        vis->startfrac = ((patch->width<<FRACBITS)-1) - frac;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = frac;
        vis->xiscale = iscale;
    }

    // get light level
    if (thing->flags & MF_SHADOW)
        vis->colormap = NULL;             // shadow draw
    else if (fixedcolormap)
        vis->colormap = R_FastColormap(fixedcolormap);      // fixed map
    else if (thing->state->frame & FF_FULLBRIGHT)
        vis->colormap = R_FastColormap(fullcolormap);     // full bright  // killough 3/20/98
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
static void R_AddSprites(sector_t* sec, int lightlevel)
{
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

static R_BSP_OPT visplane_t *new_visplane(unsigned hash)
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

static R_BSP_OPT visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel)
{
    if (picnum == _g->skyflatnum)
        height = lightlevel = 0;         // killough 7/19/98: most skies map together

    // New visplane algorithm uses hash table -- killough
    const unsigned int hash = visplane_hash(picnum,lightlevel,height);

    for (visplane_t* check=_g->visplanes[hash]; check; check=check->next)  // killough
        if (height == check->height && picnum == check->picnum && lightlevel == check->lightlevel)
            return check;

    visplane_t *check = new_visplane(hash);         // killough

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH; // Was SCREENWIDTH -- killough 11/98
    check->maxx = -1;
    check->modified = false;

    //Set top = 0xff, bottom = 0 for each column.
    BlockSet(check->limits, 0x00ff00ff, sizeof(check->limits));

    return check;
}

/*
 * R_DupPlane
 *
 * cph 2003/04/18 - create duplicate of existing visplane and set initial range
 */
static R_BSP_OPT visplane_t *R_DupPlane(const visplane_t *pl, int start, int stop)
{
    const unsigned int hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height);
    visplane_t *new_pl = new_visplane(hash);

    new_pl->height = pl->height;
    new_pl->picnum = pl->picnum;
    new_pl->lightlevel = pl->lightlevel;
    new_pl->minx = start;
    new_pl->maxx = stop;
    new_pl->modified = false;

    //Set top = 0xff, bottom = 0 for each column.
    BlockSet(new_pl->limits, 0x00ff00ff, sizeof(new_pl->limits));

    return new_pl;
}


//
// R_CheckPlane
//
static R_BSP_OPT visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
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

    for (x=intrl ; x <= intrh && pl->limits[x].top == 0xff; x++) // dropoff overflow
        ;

    if (x > intrh)
    {
        pl->minx = unionl; pl->maxx = unionh;
        return pl;
    }
    else
        return R_DupPlane(pl,start,stop);
}

static void R_DrawSegTextureColumn(const texture_t* tex, int texcolumn, draw_column_vars_t* dcvars)
{
    dcvars->source = R_GetTextureColumn(tex, texcolumn);

    R_DrawColumn (dcvars);
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)

//Optimise me
//Inlined into R_StoreWallRange, so it shares the BSP chain's level.
static void R_BSP_OPT R_RenderSegLoop (int rw_x)
{
    fixed_t  texturecolumn = 0;   // shut up compiler warning

    draw_column_vars_t dcvars;

    R_SetDefaultDrawColumnVars(&dcvars);

    dcvars.colormap = R_LoadColorMap(rw_lightlevel);

    //Copy the per-seg state into locals so it isn't reloaded
    //(and stepped values stored back) on every column.
    const int stopx = rw_stopx;

    short* const fclip = oam_spare->floorclip;
    short* const cclip = oam_spare->ceilingclip;
    const angle_t* const xtoangle = xtoviewangle;

    const bool textured = segtextured;
    const bool markc = markceiling;
    const bool markf = markfloor;

    visplane_t* const cplane = ceilingplane;
    visplane_t* const fplane = floorplane;

    const angle_t centerangle = rw_centerangle;
    const fixed_t offset = rw_offset;
    const fixed_t distance = rw_distance;

    fixed_t scale = rw_scale;
    const fixed_t scalestep = rw_scalestep;

    fixed_t tfrac = topfrac;
    const fixed_t tstep = topstep;
    fixed_t bfrac = bottomfrac;
    const fixed_t bstep = bottomstep;

    fixed_t phigh = pixhigh;
    const fixed_t phighstep = pixhighstep;
    fixed_t plow = pixlow;
    const fixed_t plowstep = pixlowstep;

    const unsigned int midtex = midtexture;
    const unsigned int toptex = toptexture;
    const unsigned int bottomtex = bottomtexture;

    const fixed_t midtexturemid = rw_midtexturemid;
    const fixed_t toptexturemid = rw_toptexturemid;
    const fixed_t bottomtexturemid = rw_bottomtexturemid;

    //Look textures up once per seg rather than per column.
    const texture_t* const midtex_t = midtex ? R_GetOrLoadTexture(midtex) : NULL;
    const texture_t* const toptex_t = toptex ? R_GetOrLoadTexture(toptex) : NULL;
    const texture_t* const bottomtex_t = bottomtex ? R_GetOrLoadTexture(bottomtex) : NULL;

    short* const maskedcol = maskedtexture ? maskedtexturecol : NULL;

    bool solid = false;

    //The loop always runs at least once (rw_stopx = stop + 1, stop >= start).
    if(markc)
        cplane->modified = true;

    if(markf)
        fplane->modified = true;

    for ( ; rw_x < stopx ; rw_x++)
    {
        // mark floor / ceiling areas

        int yh = bfrac>>HEIGHTBITS;
        int yl = (tfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

        int cc_rwx = cclip[rw_x];
        int fc_rwx = fclip[rw_x];

        if (yl <= cc_rwx)
          yl = cc_rwx + 1;

        // texturecolumn and lighting are independent of wall tiers
        if (textured)
        {
            // calculate texture offset
            angle_t angle =(centerangle+xtoangle[rw_x])>>ANGLETOFINESHIFT;

            texturecolumn = (offset-FixedMul(finetangent[angle],distance)) >> FRACBITS;

            dcvars.x = rw_x;

            dcvars.iscale = FixedReciprocalInline((unsigned)scale);
        }

        if (markc)
        {
            int bottom = min(yl, fc_rwx) - 1;

            int top = cc_rwx+1;

            if (top <= bottom)
                cplane->limits[rw_x].limits = ((top) | (bottom << 8));

            cc_rwx = bottom;
        }

        if (yh >= fc_rwx)
            yh = fc_rwx - 1;

        if (markf)
        {
            int top = max(yh, cc_rwx) + 1;

            if (top <= fc_rwx-1)
                fplane->limits[rw_x].limits = ((top) | ((fc_rwx-1) << 8));

            fc_rwx = top;
        }

        // draw the wall tiers
        if (midtex)
        {
            dcvars.texturemid = midtexturemid;

            dcvars.yl = yl;
            dcvars.yh = yh;
            R_DrawSegTextureColumn(midtex_t, texturecolumn, &dcvars);

            cc_rwx = viewheight;
            fc_rwx = -1;
        }
        else
        {
            if (toptex)
            {
                // top wall
                int mid = min((phigh >> HEIGHTBITS), fc_rwx - 1);
                phigh += phighstep;

                if (mid >= yl)
                {
                    dcvars.yl = yl;
                    dcvars.yh = mid;
                    dcvars.texturemid = toptexturemid;
                    R_DrawSegTextureColumn(toptex_t, texturecolumn, &dcvars);
                    cc_rwx = mid;
                }
                else
                    cc_rwx = yl - 1;
            }
            else
            {
                if (markc)
                    cc_rwx = yl-1;
            }

            if (bottomtex)          // bottom wall
            {
                int mid = max(((plow + HEIGHTUNIT - 1) >> HEIGHTBITS), cc_rwx + 1);
                plow += plowstep;

                if (mid <= yh)
                {
                    dcvars.yl = mid;
                    dcvars.yh = yh;
                    dcvars.texturemid = bottomtexturemid;
                    R_DrawSegTextureColumn(bottomtex_t, texturecolumn, &dcvars);
                    fc_rwx = mid;
                }
                else
                    fc_rwx = yh + 1;
            }
            else        // no bottom wall
            {
                if (markf)
                    fc_rwx = yh + 1;
            }

            // cph - if we completely blocked further sight through this column,
            // add this info to the solid columns array for r_bsp.c
            if ((markc || markf) && (fc_rwx <= cc_rwx + 1))
            {
                solidcol[rw_x] = 1;
                solid = true;
            }

            // save texturecol for backdrawing of masked mid texture
            if (maskedcol)
                maskedcol[rw_x] = texturecolumn;
        }

        scale += scalestep;
        tfrac += tstep;
        bfrac += bstep;

        fclip[rw_x] = fc_rwx;
        cclip[rw_x] = cc_rwx;
    }

    if(solid)
        didsolidcol = 1;
}

static R_BSP_OPT bool R_CheckOpenings(const int start)
{
    int pos = _g->lastopening - _g->openings;
    int need = (rw_stopx - start)*4 + pos;

#ifdef RANGECHECK
    if(need > MAXOPENINGS)
        I_Error("Openings overflow. Need = %d", need);
#endif

    return need <= MAXOPENINGS;
}

//ceil(2^32 / n) for 2 <= n < 128 (entries 0 and 1 unused).
#define RECIP32(n) ((n) < 2 ? 0u : 0xFFFFFFFFu / ((n) < 2 ? 2u : (unsigned int)(n)) + 1u)
#define RECIP32_4(n) RECIP32(n), RECIP32(n+1), RECIP32(n+2), RECIP32(n+3)
#define RECIP32_16(n) RECIP32_4(n), RECIP32_4(n+4), RECIP32_4(n+8), RECIP32_4(n+12)
#define RECIP32_64(n) RECIP32_16(n), RECIP32_16(n+16), RECIP32_16(n+32), RECIP32_16(n+48)

static const unsigned int smallRecip[128] = { RECIP32_64(0), RECIP32_64(64) };

//Exactly a / n (truncated, like FixedDiv(a, n << FRACBITS)) for
//|a| < 2^22 and 1 <= n < 128, without a 64 bit divide.
//With m = ceil(2^32 / n), m * n = 2^32 + e where 0 <= e < n, so
//(|a| * m) >> 32 = floor(|a| / n + |a| * e / (n * 2^32)), and the
//extra term is < 1/n because |a| * e < 2^22 * 2^7 < 2^32.
static inline int R_DivSmall(const int a, const unsigned int n)
{
    if (n == 1)
        return a;

    const unsigned int ua = a < 0 ? -a : a;
    const int q = (int)(((unsigned long long)ua * smallRecip[n]) >> 32);

    return a < 0 ? -q : q;
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//

static R_BSP_OPT void R_StoreWallRange(const int start, const int stop)
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

    // mark the segment as visible for auto map
    linedata->r_mapped = 1;

    sidedef = SIDE(curline->sidenum);
    linedef = &_g->lines[curline->linenum];

    // calculate rw_distance for scale calculation
    rw_normalangle = ((angle_t)curline->angle << 16) + ANG90;

    offsetangle = rw_normalangle-rw_angle1;

    if (D_abs(offsetangle) > ANG90)
        offsetangle = ANG90;

    const fixed_t v1x = (fixed_t)curline->v1.x << FRACBITS;
    const fixed_t v1y = (fixed_t)curline->v1.y << FRACBITS;

    hyp = (viewx==v1x && viewy==v1y)? 0 : R_PointToDist (v1x, v1y);

    rw_distance = FixedMul(hyp, finecosine[offsetangle>>ANGLETOFINESHIFT]);

    int rw_x = ds_p->x1 = start;
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

        //Same as FixedDiv(scale2 - scale1, (stop - start) << FRACBITS).
        //Scales are clamped to [256, 64 * FRACUNIT], so |a| < 2^22.
        ds_p->scalestep = rw_scalestep = R_DivSmall(ds_p->scale2-rw_scale, stop-start);
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

        rw_midtexturemid += FixedMod( (sidedef->rowoffset << FRACBITS), textureheight[midtexture]);

        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = oam_spare->screenheightarray;
        ds_p->sprbottomclip = oam_spare->negonearray;
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
            ds_p->sprbottomclip = oam_spare->negonearray;
            ds_p->bsilheight = INT_MAX;
            ds_p->sprtopclip = oam_spare->screenheightarray;
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
            rw_toptexturemid += FixedMod( (sidedef->rowoffset << FRACBITS), textureheight[toptexture]);
        }

        if (worldlow > worldbottom) // bottom texture
        {
            bottomtexture = texturetranslation[sidedef->bottomtexture];
            rw_bottomtexturemid = linedef->flags & ML_DONTPEGBOTTOM ? worldtop : worldlow;

            rw_bottomtexturemid += FixedMod( (sidedef->rowoffset << FRACBITS), textureheight[bottomtexture]);
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

        rw_offset += (sidedef->textureoffset + curline->offset) << FRACBITS;

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

    didsolidcol = 0;
    R_RenderSegLoop(rw_x);

    /* cph - if a column was made solid by this wall, we _must_ save full clipping info */
    if (backsector && didsolidcol)
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
        //Clip values are shorts (and OAM needs 16 or 32 bit access).
        BlockCopy16(_g->lastopening, oam_spare->ceilingclip+start, sizeof(short)*(rw_stopx-start));
        ds_p->sprtopclip = _g->lastopening - start;
        _g->lastopening += rw_stopx - start;
    }

    if ((ds_p->silhouette & SIL_BOTTOM || maskedtexture) && !ds_p->sprbottomclip)
    {
        //Clip values are shorts (and OAM needs 16 or 32 bit access).
        BlockCopy16(_g->lastopening, oam_spare->floorclip+start, sizeof(short)*(rw_stopx-start));
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

static R_BSP_OPT void R_RecalcLineFlags(void)
{
    linedata_t* linedata = &_g->linedata[curline->linenum];

    const side_t* side = SIDE(curline->sidenum);

    linedata->r_validcount = (_g->gametic & RF_VALIDMASK);

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
        linedata->r_flags = RF_CLOSED;
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
            linedata->r_flags = 0; return;
        } else
            linedata->r_flags = RF_IGNORE;
    }
}



// CPhipps -
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those
// columns which aren't solid, and updates the solidcol[] array appropriately

static R_BSP_OPT void R_ClipWallSegment(int first, int last, bool solid)
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

static R_BSP_OPT void R_AddLine (const seg_t *line)
{
    const fixed_t v1x = (fixed_t)line->v1.x << FRACBITS;
    const fixed_t v1y = (fixed_t)line->v1.y << FRACBITS;

    // Backface culling before the angle calculations: skip the seg if the
    // view is behind it (cross product of v1->v2 and v1->view > 0).
    // Edge-on segs (cross product 0) are left to the span test below.
    if ((long long)(viewy - v1y) * (line->v2.x - line->v1.x) > (long long)(viewx - v1x) * (line->v2.y - line->v1.y))
        return;

    angle_t angle1 = R_PointToAngle2(viewx, viewy, v1x, v1y);
    angle_t angle2 = R_PointToAngle2(viewx, viewy, (fixed_t)line->v2.x << FRACBITS, (fixed_t)line->v2.y << FRACBITS);

    // Clip to view edges.
    const angle_t span = angle1 - angle2;

    // Back side, i.e. backface culling
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

    angle_t tspan = angle1 + clipangle;
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
    const int x1 = viewangletox[angle1];
    const int x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
        return;

    backsector = SG_BACKSECTOR(line);

    curline = line;
    linedef = &_g->lines[curline->linenum];
    linedata_t* linedata = &_g->linedata[curline->linenum];

    if (linedata->r_validcount != (_g->gametic & RF_VALIDMASK))
        R_RecalcLineFlags();

    if (linedata->r_flags & RF_IGNORE)
        return;

    R_ClipWallSegment (x1, x2, linedata->r_flags & RF_CLOSED);
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished

static R_BSP_OPT void R_Subsector(int num)
{
    int         count;
    const seg_t       *line;
    const subsector_t *sub;

    sub = &_g->subsectors[num];
    frontsector = SS_SECTOR(sub);
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

    R_AddSprites(frontsector, frontsector->lightlevel);
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
static R_BSP_OPT bool R_CheckBBox(const short *bspcoord)
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
        angle1 = R_PointToAngle2(viewx, viewy, ((fixed_t)bspcoord[check[0]]<<FRACBITS), ((fixed_t)bspcoord[check[1]]<<FRACBITS)) - viewangle;
        angle2 = R_PointToAngle2(viewx, viewy, ((fixed_t)bspcoord[check[2]]<<FRACBITS), ((fixed_t)bspcoord[check[3]]<<FRACBITS)) - viewangle;
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





static R_BSP_OPT bool R_RenderBspSubsector(int bspnum)
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
#define MAX_BSP_DEPTH 64

//Each entry is (node << 1) | side. Only nodes are pushed and
//node numbers are < NF_SUBSECTOR, so this fits in 16 bits.
#define BSP_PUSH(n, s) (stack[sp++] = (unsigned short)(((n) << 1) | (s)))

static R_BSP_OPT void R_RenderBSPNode(int bspnum)
{
    unsigned short stack[MAX_BSP_DEPTH];
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

            BSP_PUSH(bspnum, side);

            bspnum = bsp->children[side];
        }

        if(sp == 0)
        {
            //back at root node and not visible. All done!
            return;
        }

        //Back sides.
        --sp;
        side = stack[sp] & 1;
        bspnum = stack[sp] >> 1;
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
            --sp;
            side = stack[sp] & 1;
            bspnum = stack[sp] >> 1;

            bsp = &nodes[bspnum];
        }

        bspnum = bsp->children[side^1];
    }
}

#undef BSP_PUSH


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
    num_vissprite = 0;            // killough
}

//
// RDrawPlanes
// At the end of each frame.
//

static void R_DrawPlanes (void)
{
    for (int i=0; i<MAXVISPLANES; i++)
    {
        visplane_t *pl = _g->visplanes[i];

        while(pl)
        {
            if(pl->modified)
                R_DoDrawPlane(pl);

            pl = pl->next;
        }
    }

    R_UpdateFlatCache();
}

//
// R_ClearPlanes
// At begining of frame.
//

static void R_ClearPlanes(void)
{
    int i;

    // opening / clipping determination
    // 32 bit DMA fill with the short value in both halves of the word.
    static_assert(offsetof(oam_spare_t, floorclip) % 4 == 0 && offsetof(oam_spare_t, ceilingclip) % 4 == 0, "clip arrays must be word aligned for BlockSet");
    static_assert(sizeof(oam_spare->floorclip) % 4 == 0 && sizeof(oam_spare->ceilingclip) % 4 == 0, "clip arrays must be whole words for BlockSet");

    BlockSet(oam_spare->floorclip, ((unsigned int)viewheight << 16) | (unsigned int)viewheight, sizeof(oam_spare->floorclip));
    BlockSet(oam_spare->ceilingclip, 0xffffffff, sizeof(oam_spare->ceilingclip));


    for (i=0;i<MAXVISPLANES;i++)    // new code -- killough
        for (*_g->freehead = _g->visplanes[i], _g->visplanes[i] = NULL; *_g->freehead; )
            _g->freehead = &(*_g->freehead)->next;

    _g->lastopening = _g->openings;

    basexscale = FixedMul(viewsin,iprojection);
    baseyscale = FixedMul(viewcos,iprojection);
}

//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{
    R_SetupFrame (player);

    //A powerup colormap stays the same for many frames, so copy it to
    //the OBJ palette only when it changes.
    if (fixedcolormap && fixedcolormap != objpalFixedSrc)
    {
        BlockCopy(objpal_spare->fixedColormap, fixedcolormap, sizeof(objpal_spare->fixedColormap));
        objpalFixedSrc = fixedcolormap;
    }

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

void V_DrawPatchNoScale(int x, int y, const patch_t* patch)
{
    y -= patch->topoffset;
    x -= patch->leftoffset;

    byte* desttop = (byte*)_g->screens[0].data;
    desttop += (ScreenYToOffset(y) << 1) + x;

    unsigned int width = patch->width;

    for (unsigned int col = 0; col < width; col++, desttop++)
    {
        const column_t* column = (const column_t*)((const byte*)patch + patch->columnofs[col]);

        unsigned int odd_addr = (size_t)desttop & 1;

        byte* desttop_even = (byte*)((size_t)desttop & ~1);

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            const byte* source = (const byte*)column + 3;
            byte* dest = desttop_even + (ScreenYToOffset(column->topdelta) << 1);

            unsigned int count = column->length;

            while (count--)
            {
                unsigned int color = *source++;
                volatile unsigned short* dest16 = (volatile unsigned short*)dest;

                unsigned int old = *dest16;

                //The GBA must write in 16bits.
                if(odd_addr)
                    *dest16 = (old & 0xff) | (color << 8);
                else
                    *dest16 = ((color & 0xff) | (old & 0xff00));

                dest += 240;
            }

            column = (const column_t*)((const byte*)column + column->length + 4);
        }
    }
}

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
// killough 4/19/98: made static, cleaned up

static int P_DivlineSide(fixed_t x, fixed_t y, const divline_t *node)
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

static bool P_CrossSubsector(int num)
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

        if (MAPTOFIXED(line->bbox[BOXLEFT]) > _g->los.bbox[BOXRIGHT ] ||
                MAPTOFIXED(line->bbox[BOXRIGHT]) < _g->los.bbox[BOXLEFT  ] ||
                MAPTOFIXED(line->bbox[BOXBOTTOM]) > _g->los.bbox[BOXTOP   ] ||
                MAPTOFIXED(line->bbox[BOXTOP])    < _g->los.bbox[BOXBOTTOM])
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

        // Forget this line if it doesn't cross the line of sight
        const fixed_t v1x = MAPTOFIXED(line->v1.x);
        const fixed_t v1y = MAPTOFIXED(line->v1.y);
        const fixed_t v2x = MAPTOFIXED(line->v2.x);
        const fixed_t v2y = MAPTOFIXED(line->v2.y);

        if (P_DivlineSide(v1x, v1y, &_g->los.strace) == P_DivlineSide(v2x, v2y, &_g->los.strace))
            continue;

        divl.dx = v2x - (divl.x = v1x);
        divl.dy = v2y - (divl.y = v1y);

        // line isn't crossed?
        if (P_DivlineSide(_g->los.strace.x, _g->los.strace.y, &divl) == P_DivlineSide(_g->los.t2x, _g->los.t2y, &divl))
            continue;


        // cph - if bottom >= top or top < minz or bottom > maxz then it must be
        // solid wrt this LOS
        if (!(line->flags & ML_TWOSIDED) || (openbottom >= opentop) ||
                (opentop < _g->los.minz) || (openbottom > _g->los.maxz))
            return false;

        // crosses a two sided line
        /* cph 2006/07/15 - oops, we missed this in 2.4.0 & .1;
       *  use P_InterceptVector2 for those compat levels only. */
        fixed_t frac = P_InterceptVector2(&_g->los.strace, &divl);

        if (front->floorheight != back->floorheight)
        {
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
    // passed the subsector ok
    return true;
}

bool P_CrossBSPNode(int bspnum)
{
    while (!(bspnum & NF_SUBSECTOR))
    {
        const mapnode_t *bsp = nodes + bspnum;

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
// P_MobjThinker
//

void P_NightmareRespawn(mobj_t* mobj);
void P_XYMovement (mobj_t* mo);
void P_ZMovement (mobj_t* mo);


void P_MobjThinker (mobj_t* mobj, void*)
{
    // killough 11/98:
    // removed old code which looked at target references
    // (we use pointer reference counting now)

    // momentum movement
    if (mobj->momx | mobj->momy || mobj->flags & MF_SKULLFLY)
    {
        P_XYMovement(mobj);
        if (mobj->thinker.function != (think_t)P_MobjThinker) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    if (mobj->z != mobj->floorz || mobj->momz)
    {
        P_ZMovement(mobj);
        if (mobj->thinker.function != (think_t)P_MobjThinker) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    // cycle through states,
    // calling action functions at transitions

    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic

        if (!mobj->tics)
            if (!P_SetMobjState (mobj, mobj->state->nextstate) )
                return;     // freed itself
    }
    else
    {

        // check for nightmare respawn

        if (! (mobj->flags & MF_COUNTKILL) )
            return;

        if (!_g->respawnmonsters)
            return;

        mobj->movecount++;

        if (mobj->movecount < 12*35)
            return;

        if (_g->leveltime & 31)
            return;

        if (P_Random () > 4)
            return;

        P_NightmareRespawn (mobj);
    }

}


//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//

void P_RunThinkers (void)
{
    thinker_t* th = thinkercap.next;
    thinker_t* th_end = &thinkercap;

    while(th != th_end)
    {
        thinker_t* th_next = th->next;
        if(th->function)
            th->function(th, NULL);

        th = th_next;
    }
}

int I_GetTime(void)
{
    int thistimereply;

#ifndef GBA

    clock_t now = clock();

    thistimereply = (int)((double)now / ((double)CLOCKS_PER_SEC / (double)TICRATE));
#else
    thistimereply = *((unsigned short*)(0x400010C));
#endif

    if (thistimereply < _g->lasttimereply)
        _g->basetime -= 0xffff;

    _g->lasttimereply = thistimereply;


    /* Fix for time problem */
    if (!_g->basetime)
    {
        _g->basetime = thistimereply;
        thistimereply = 0;
    }
    else
    {
        thistimereply -= _g->basetime;
    }

    return thistimereply;
}


