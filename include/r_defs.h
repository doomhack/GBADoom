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
 *      Refresh/rendering module, shared data struct definitions.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __R_DEFS__
#define __R_DEFS__

// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#ifdef __GNUG__
#pragma interface
#endif

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE    0
#define SIL_BOTTOM  1
#define SIL_TOP     2
#define SIL_BOTH    3

#define MAXDRAWSEGS   192

#define MAXOPENINGS (SCREENWIDTH*16)

#define MAXVISSPRITES 96

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
// like some DOOM-alikes ("wt", "WebView") do.
//
typedef struct
{
  fixed_t x, y;
} vertex_t;

// Sound origin for non-mobj sounds (see P_StartSectorSound).
typedef struct
{
  fixed_t x;
  fixed_t y;
} degenmobj_t;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//

typedef struct sector_s
{
  fixed_t floorheight;
  fixed_t ceilingheight;

  mobj_t *soundtarget;   // thing that made a sound (or null)
  mobj_t *thinglist;     // list of mobjs in sector

  // thinker_t for reversable actions
  void *floordata;    // jff 2/22/98 make thinkers on
  void *ceilingdata;  // floors, ceilings, lighting,

  const struct line_s **lines;

  unsigned short validcount;  // if == validcount, already checked
  short linecount;
  short special;
  short tag;

  unsigned char floorpic;     // < 256 flats, checked in R_InitFlats
  unsigned char ceilingpic;
  unsigned char lightlevel;

  unsigned char soundtraversed:2;   // 0 = untraversed, 1,2 = sndlines-1
  unsigned char oldsecret:1;        //jff 2/16/98 remembers if sector WAS secret (automap)

} sector_t;

//
// The SideDef.
//

typedef struct
{
    sector_t* sector;      // Sector the SideDef is facing.

    short textureoffset; // add this to the calculated texture column
    short rowoffset;     // add this to the calculated texture top

    unsigned int toptexture:10;
    unsigned int bottomtexture:10;
    unsigned int midtexture:10;
} side_t;

//
// Move clipping aid for LineDefs.
//
typedef enum
{
  ST_HORIZONTAL,
  ST_VERTICAL,
  ST_POSITIVE,
  ST_NEGATIVE
} slopetype_t;

typedef enum
{                 // cph:
    RF_IGNORE   = 1,     // Renderer can skip this line
    RF_CLOSED   = 2,     // Line blocks view
} r_flags;

#define RF_VALIDMASK 0x7ff  // Bits of gametic stored in r_validcount.

//Runtime mutable data for lines.
typedef struct linedata_s
{
    unsigned short validcount;        // if == validcount, already checked

    unsigned short r_validcount:11;   // cph: if == (gametic & RF_VALIDMASK), r_flags already done
    unsigned short r_flags:2;         // RF_IGNORE / RF_CLOSED
    unsigned short r_mapped:1;        // Seen so show on automap.
    unsigned short nospecial:1;       // Special has been used up (W1, S1 etc).
    unsigned short stairflip:1;       // Generalised stairs: StairDirection toggled.
} linedata_t;

typedef struct line_s
{
    vertex_t v1;
    vertex_t v2;     // Vertices, from v1 to v2.
    unsigned int lineno;         //line number.

    fixed_t dx, dy;        // Precalculated v2 - v1 for side checking.

    unsigned short sidenum[2];        // Visual appearance: SideDefs.
    fixed_t bbox[4];        //Line bounding box.

    unsigned short flags;           // Animation related.
    short const_special;
    short tag;
    short slopetype; // To aid move clipping.

} line_t;

#define LN_FRONTSECTOR(l) (_g->sides[(l)->sidenum[0]].sector)
#define LN_BACKSECTOR(l) ((l)->sidenum[1] != NO_INDEX ? _g->sides[(l)->sidenum[1]].sector : NULL)

#define LN_DATA(l) (_g->linedata[(l)->lineno])

// The special is const_special (ROM), cleared once used up and with
// StairDirection (1 << 8) toggled by retriggerable generalised stairs.
#define LN_SPECIAL(l) (LN_DATA(l).nospecial ? 0 : ((l)->const_special ^ (LN_DATA(l).stairflip << 8)))
#define LN_CLEARSPECIAL(l) (LN_DATA(l).nospecial = 1)
#define LN_FLIPSTAIRS(l) (LN_DATA(l).stairflip ^= 1)

#define LN_VCOUNT(l) (LN_DATA(l).validcount)


//
// The LineSeg.
//

/*
typedef struct
{
  vertex_t *v1, *v2;
  fixed_t offset;
  angle_t angle;
  side_t* sidedef;
  const line_t* linedef;

  // Sector references.
  // Could be retrieved from linedef, too
  // (but that would be slower -- killough)
  // backsector is NULL for one sided lines

  sector_t *frontsector, *backsector;
} seg_t;
*/

//
// The LineSeg.
//
typedef struct
{
    vertex_t v1;
    vertex_t v2;            // Vertices, from v1 to v2.

    fixed_t offset;
    angle_t angle;

    unsigned short sidenum;
    unsigned short linenum;

    unsigned short frontsectornum;
    unsigned short backsectornum;
} seg_t;

#define SG_FRONTSECTOR(s) ((s)->frontsectornum != NO_INDEX ? &_g->sectors[(s)->frontsectornum] : NULL)
#define SG_BACKSECTOR(s) ((s)->backsectornum != NO_INDEX ? &_g->sectors[(s)->backsectornum] : NULL)

//
// A SubSector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
// Read straight from the SSECTORS lump in ROM (same layout as
// mapsubsector_t, but not packed so fields load as halfwords).
// Its sector is the front sector of its first seg (SS_SECTOR).
//

typedef struct subsector_s
{
  unsigned short numlines, firstline;
} subsector_t;

#define SS_SECTOR(ss) (&_g->sectors[_g->segs[(ss)->firstline].frontsectornum])

//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT.
// from darkening PLAYPAL to all black.
// Could use even more than 32 levels.

typedef byte  lighttable_t;

//
// Masked 2s linedefs
//

typedef struct drawseg_s
{
  const seg_t *curline;
  short x1, x2;
  fixed_t scale1, scale2, scalestep;
  int silhouette;                       // 0=none, 1=bottom, 2=top, 3=both
  fixed_t bsilheight;                   // do not clip sprites above this
  fixed_t tsilheight;                   // do not clip sprites below this

  // Pointers to lists for sprite clipping,
  // all three adjusted so [x1] is first value.

  short *sprtopclip, *sprbottomclip;
  short *maskedtexturecol; // dropoff overflow
} drawseg_t;

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
typedef struct
{
    short		width;		// bounding box size
    short		height;
    short		leftoffset;	// pixels to the left of origin
    short		topoffset;	// pixels below the origin
    int			columnofs[8];	// only [width] used
    // the [0] is &columnofs[width]
} patch_t;


// posts are runs of non masked source pixels
typedef struct
{
    byte		topdelta;	// -1 is the last post in a column
    byte		length; 	// length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;

//
// A vissprite_t is a thing that will be drawn during a refresh.
// i.e. a sprite object that is partly visible.
//

typedef struct vissprite_s
{
  short x1, x2;
  fixed_t gx, gy;              // for line side calculation
  fixed_t gz;                   // global bottom for silhouette clipping
  fixed_t startfrac;           // horizontal position of x1
  fixed_t scale;
  fixed_t xiscale;             // negative if flipped
  fixed_t texturemid;
  fixed_t iscale;

  const patch_t* patch;

  unsigned int mobjflags;

  // for color translation and shadow draw, maxbright frames as well
  const lighttable_t *colormap;

} vissprite_t;

//
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//

typedef struct
{
  // Lump to use for view angles 0-7.
  short lump[8];

  // Flip bit (1 = flip) to use for view angles 0-7.
  //byte  flip[8];
  byte flipmask;

  // If false use 0 for any position.
  // Note: as eight entries are available,
  //  we might as well insert the same name eight times.
  bool rotate;

} spriteframe_t;

#define SPR_FLIPPED(s, r) (s->flipmask & (1 << r))

//
// A sprite definition:
//  a number of animation frames.
//

typedef struct
{
  int numframes;
  spriteframe_t *spriteframes;
} spritedef_t;

//
// Now what is a visplane, anyway?
//
// Go to http://classicgaming.com/doom/editing/ to find out -- killough
//

typedef union visplane_limits_t
{
    struct
    {
        byte top, bottom;
    };
    unsigned short limits;
} visplane_limits_t;

typedef struct visplane
{
  struct visplane *next;        // Next visplane in hash chain -- killough
  short picnum, lightlevel;
  short minx, maxx;
  fixed_t height;
  byte modified;

  byte pad1[3];

  visplane_limits_t limits[SCREENWIDTH];

  unsigned int pad2;

} visplane_t;

#endif
