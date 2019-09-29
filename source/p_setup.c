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
 *  Do all the WAD I/O, get map description,
 *  set up initial state and misc. LUTs.
 *
 *-----------------------------------------------------------------------------*/

#include <math.h>

#include "doomstat.h"
#include "m_bbox.h"
#include "g_game.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "lprintf.h" //jff 10/6/98 for debug outputs
#include "v_video.h"

#include "global_data.h"


//
// P_GetNodesVersion
//

static void P_GetNodesVersion()
{
    lprintf(LO_DEBUG,"P_GetNodesVersion: using normal BSP nodes\n");
}

//
// P_LoadVertexes
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadVertexes (int lump)
{
  const mapvertex_t *data; // cph - const
  int i;

  // Determine number of lumps:
  //  total lump length / vertex record length.
  _g->numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);

  // Allocate zone memory for buffer.
  _g->vertexes = Z_Malloc(_g->numvertexes*sizeof(vertex_t),PU_LEVEL,0);

  // Load data into cache.
  // cph 2006/07/29 - cast to mapvertex_t here, making the loop below much neater
  data = (const mapvertex_t *)W_CacheLumpNum(lump);

  // Copy and convert vertex coordinates,
  // internal representation as fixed.
  for (i=0; i<_g->numvertexes; i++)
    {
      _g->vertexes[i].x = SHORT(data[i].x)<<FRACBITS;
      _g->vertexes[i].y = SHORT(data[i].y)<<FRACBITS;
    }

  // Free buffer memory.
  W_UnlockLumpNum(lump);
}

static float GetDistance(int dx, int dy)
{
  float fx = (float)(dx)/FRACUNIT, fy = (float)(dy)/FRACUNIT;
  return (float)sqrt(fx*fx + fy*fy);
}

static int GetOffset(vertex_t *v1, vertex_t *v2)
{
  float a, b;
  int r;
  a = (float)(v1->x - v2->x) / (float)FRACUNIT;
  b = (float)(v1->y - v2->y) / (float)FRACUNIT;
  r = (int)(sqrt(a*a+b*b) * (float)FRACUNIT);
  return r;
}



//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSegs (int lump)
{
  int  i;
  const mapseg_t *data; // cph - const

  _g->numsegs = W_LumpLength(lump) / sizeof(mapseg_t);
  _g->segs = Z_Calloc(_g->numsegs,sizeof(seg_t),PU_LEVEL,0);
  data = (const mapseg_t *)W_CacheLumpNum(lump); // cph - wad lump handling updated

  if ((!data) || (!_g->numsegs))
    I_Error("P_LoadSegs: no segs in level");

  for (i=0; i<_g->numsegs; i++)
    {
      seg_t *li = _g->segs+i;
      const mapseg_t *ml = data + i;
      unsigned short v1, v2;

      int side, linedef;
      line_t *ldef;

      v1 = (unsigned short)SHORT(ml->v1);
      v2 = (unsigned short)SHORT(ml->v2);
      li->v1 = &_g->vertexes[v1];
      li->v2 = &_g->vertexes[v2];
      li->angle = (SHORT(ml->angle))<<16;
      li->offset =(SHORT(ml->offset))<<16;
      linedef = (unsigned short)SHORT(ml->linedef);
      ldef = &_g->lines[linedef];
      li->linedef = ldef;
      side = SHORT(ml->side);
      li->sidedef = &_g->sides[ldef->sidenum[side]];

      /* cph 2006/09/30 - our frontsector can be the second side of the
       * linedef, so must check for NO_INDEX in case we are incorrectly
       * referencing the back of a 1S line */
      if (ldef->sidenum[side] != NO_INDEX)
        li->frontsector = _g->sides[ldef->sidenum[side]].sector;
      else
      {
        li->frontsector = 0;
        lprintf(LO_WARN, "P_LoadSegs: front of seg %i has no sidedef\n", i);
      }

      if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=NO_INDEX)
        li->backsector = _g->sides[ldef->sidenum[side^1]].sector;
      else
        li->backsector = 0;
    }

  W_UnlockLumpNum(lump); // cph - release the data
}

//
// P_LoadSubsectors
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSubsectors (int lump)
{
  /* cph 2006/07/29 - make data a const mapsubsector_t *, so the loop below is simpler & gives no constness warnings */
  const mapsubsector_t *data;
  int  i;

  _g->numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
  _g->subsectors = Z_Calloc(_g->numsubsectors,sizeof(subsector_t),PU_LEVEL,0);
  data = (const mapsubsector_t *)W_CacheLumpNum(lump);

  if ((!data) || (!_g->numsubsectors))
    I_Error("P_LoadSubsectors: no subsectors in level");

  for (i=0; i<_g->numsubsectors; i++)
  {
    _g->subsectors[i].numlines  = (unsigned short)SHORT(data[i].numsegs );
    _g->subsectors[i].firstline = (unsigned short)SHORT(data[i].firstseg);
  }

  W_UnlockLumpNum(lump); // cph - release the data
}

//
// P_LoadSectors
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSectors (int lump)
{
  const byte *data; // cph - const*
  int  i;

  _g->numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
  _g->sectors = Z_Calloc (_g->numsectors,sizeof(sector_t),PU_LEVEL,0);
  data = W_CacheLumpNum (lump); // cph - wad lump handling updated

  for (i=0; i<_g->numsectors; i++)
    {
      sector_t *ss = _g->sectors + i;
      const mapsector_t *ms = (const mapsector_t *) data + i;

      ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
      ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
      ss->floorpic = R_FlatNumForName(ms->floorpic);
      ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
      ss->lightlevel = SHORT(ms->lightlevel);
      ss->special = SHORT(ms->special);
      ss->oldspecial = SHORT(ms->special);
      ss->tag = SHORT(ms->tag);
      ss->thinglist = NULL;
      ss->touching_thinglist = NULL;            // phares 3/14/98

      ss->nextsec = -1; //jff 2/26/98 add fields to support locking out
      ss->prevsec = -1; // stair retriggering until build completes

    }

  W_UnlockLumpNum(lump); // cph - release the data
}


//
// P_LoadNodes
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadNodes (int lump)
{
  const byte *data; // cph - const*
  int  i;

  _g->numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
  _g->nodes = Z_Malloc (_g->numnodes*sizeof(node_t),PU_LEVEL,0);
  data = W_CacheLumpNum (lump); // cph - wad lump handling updated

  if ((!data) || (!_g->numnodes))
  {
    // allow trivial maps
    if (_g->numsubsectors == 1)
      lprintf(LO_INFO,
          "P_LoadNodes: trivial map (no nodes, one subsector)\n");
    else
      I_Error("P_LoadNodes: no nodes in level");
  }

  for (i=0; i<_g->numnodes; i++)
    {
      node_t *no = _g->nodes + i;
      const mapnode_t *mn = (const mapnode_t *) data + i;
      int j;

      no->x = SHORT(mn->x)<<FRACBITS;
      no->y = SHORT(mn->y)<<FRACBITS;
      no->dx = SHORT(mn->dx)<<FRACBITS;
      no->dy = SHORT(mn->dy)<<FRACBITS;

      for (j=0 ; j<2 ; j++)
        {
          int k;
          no->children[j] = SHORT(mn->children[j]);
          for (k=0 ; k<4 ; k++)
            no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
        }
    }

  W_UnlockLumpNum(lump); // cph - release the data
}


/*
 * P_LoadThings
 *
 * killough 5/3/98: reformatted, cleaned up
 * cph 2001/07/07 - don't write into the lump cache, especially non-idepotent
 * changes like byte order reversals. Take a copy to edit.
 */

static void P_LoadThings (int lump)
{
  int  i, numthings = W_LumpLength (lump) / sizeof(mapthing_t);
  const mapthing_t *data = W_CacheLumpNum (lump);

  if ((!data) || (!numthings))
    I_Error("P_LoadThings: no things in level");

  for (i=0; i<numthings; i++)
    {
      mapthing_t mt = data[i];

      mt.x = SHORT(mt.x);
      mt.y = SHORT(mt.y);
      mt.angle = SHORT(mt.angle);
      mt.type = SHORT(mt.type);
      mt.options = SHORT(mt.options);

      if (!P_IsDoomnumAllowed(mt.type))
        continue;

      // Do spawn all other stuff.
      P_SpawnMapThing(&mt);
    }


  W_UnlockLumpNum(lump); // cph - release the data
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadLineDefs (int lump)
{
  const byte *data; // cph - const*
  int  i;

  _g->numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
  _g->lines = Z_Calloc (_g->numlines,sizeof(line_t),PU_LEVEL,0);
  data = W_CacheLumpNum (lump); // cph - wad lump handling updated

  for (i=0; i<_g->numlines; i++)
    {
      const maplinedef_t *mld = (const maplinedef_t *) data + i;
      line_t *ld = _g->lines+i;
      vertex_t *v1, *v2;

      ld->flags = (unsigned short)SHORT(mld->flags);
      ld->special = SHORT(mld->special);
      ld->tag = SHORT(mld->tag);
      v1 = ld->v1 = &_g->vertexes[(unsigned short)SHORT(mld->v1)];
      v2 = ld->v2 = &_g->vertexes[(unsigned short)SHORT(mld->v2)];
      ld->dx = v2->x - v1->x;
      ld->dy = v2->y - v1->y;

      ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
        FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

      if (v1->x < v2->x)
        {
          ld->bbox[BOXLEFT] = v1->x;
          ld->bbox[BOXRIGHT] = v2->x;
        }
      else
        {
          ld->bbox[BOXLEFT] = v2->x;
          ld->bbox[BOXRIGHT] = v1->x;
        }
      if (v1->y < v2->y)
        {
          ld->bbox[BOXBOTTOM] = v1->y;
          ld->bbox[BOXTOP] = v2->y;
        }
      else
        {
          ld->bbox[BOXBOTTOM] = v2->y;
          ld->bbox[BOXTOP] = v1->y;
        }

      /* calculate sound origin of line to be its midpoint */
      //e6y: fix sound origin for large levels
      // no need for comp_sound test, these are only used when comp_sound = 0
      //ld->soundorg.x = ld->bbox[BOXLEFT] / 2 + ld->bbox[BOXRIGHT] / 2;
      //ld->soundorg.y = ld->bbox[BOXTOP] / 2 + ld->bbox[BOXBOTTOM] / 2;

      ld->sidenum[0] = SHORT(mld->sidenum[0]);
      ld->sidenum[1] = SHORT(mld->sidenum[1]);

      { 
        /* cph 2006/09/30 - fix sidedef errors right away.
         * cph 2002/07/20 - these errors are fatal if not fixed, so apply them
         * in compatibility mode - a desync is better than a crash! */
        int j;
        
        for (j=0; j < 2; j++)
        {
          if (ld->sidenum[j] != NO_INDEX && ld->sidenum[j] >= _g->numsides) {
            ld->sidenum[j] = NO_INDEX;
            lprintf(LO_WARN, "P_LoadLineDefs: linedef %d has out-of-range sidedef number\n",_g->numlines-i-1);
          }
        }
        
        // killough 11/98: fix common wad errors (missing sidedefs):
        
        if (ld->sidenum[0] == NO_INDEX) {
          ld->sidenum[0] = 0;  // Substitute dummy sidedef for missing right side
          // cph - print a warning about the bug
          lprintf(LO_WARN, "P_LoadLineDefs: linedef %d missing first sidedef\n",_g->numlines-i-1);
        }
        
        if ((ld->sidenum[1] == NO_INDEX) && (ld->flags & ML_TWOSIDED)) {
          ld->flags &= ~ML_TWOSIDED;  // Clear 2s flag for missing left side
          // cph - print a warning about the bug
          lprintf(LO_WARN, "P_LoadLineDefs: linedef %d has two-sided flag set, but no second sidedef\n",_g->numlines-i-1);
        }
      }

      // killough 4/4/98: support special sidedef interpretation below
      if (ld->sidenum[0] != NO_INDEX && ld->special)
        _g->sides[*ld->sidenum].special = ld->special;
    }

  W_UnlockLumpNum(lump); // cph - release the lump
}

// killough 4/4/98: delay using sidedefs until they are loaded
// killough 5/3/98: reformatted, cleaned up

static void P_LoadLineDefs2(int lump)
{
  int i = _g->numlines;
  register line_t *ld = _g->lines;
  for (;i--;ld++)
    {
      ld->frontsector = _g->sides[ld->sidenum[0]].sector; //e6y: Can't be NO_INDEX here
      ld->backsector  = ld->sidenum[1]!=NO_INDEX ? _g->sides[ld->sidenum[1]].sector : 0;
    }
}

//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions

static void P_LoadSideDefs (int lump)
{
  _g->numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
  _g->sides = Z_Calloc(_g->numsides,sizeof(side_t),PU_LEVEL,0);
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSideDefs2(int lump)
{
  const byte *data = W_CacheLumpNum(lump); // cph - const*, wad lump handling updated
  int  i;

  for (i=0; i<_g->numsides; i++)
    {
      register const mapsidedef_t *msd = (const mapsidedef_t *) data + i;
      register side_t *sd = _g->sides + i;
      register sector_t *sec;

      sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
      sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;

      { /* cph 2006/09/30 - catch out-of-range sector numbers; use sector 0 instead */
        unsigned short sector_num = SHORT(msd->sector);
        if (sector_num >= _g->numsectors) {
          lprintf(LO_WARN,"P_LoadSideDefs2: sidedef %i has out-of-range sector num %u\n", i, sector_num);
          sector_num = 0;
        }
        sd->sector = sec = &_g->sectors[sector_num];
      }

      // killough 4/4/98: allow sidedef texture names to be overloaded
      // killough 4/11/98: refined to allow colormaps to work as wall
      // textures if invalid as colormaps but valid as textures.
      switch (sd->special)
        {
        case 242:                       // variable colormap via 242 linedef
          sd->bottomtexture =
            (R_ColormapNumForName(msd->bottomtexture)) < 0 ?
            R_TextureNumForName(msd->bottomtexture): 0 ;
          sd->midtexture =
            (R_ColormapNumForName(msd->midtexture)) < 0 ?
            R_TextureNumForName(msd->midtexture)  : 0 ;
          sd->toptexture =
            (R_ColormapNumForName(msd->toptexture)) < 0 ?
            R_TextureNumForName(msd->toptexture)  : 0 ;
          break;

        case 260: // killough 4/11/98: apply translucency to 2s normal texture
          sd->midtexture = strncasecmp("TRANMAP", msd->midtexture, 8) ?
            (sd->special = W_CheckNumForName(msd->midtexture)) < 0 ||
            W_LumpLength(sd->special) != 65536 ?
            sd->special=0, R_TextureNumForName(msd->midtexture) :
              (sd->special++, 0) : (sd->special=0);
          sd->toptexture = R_TextureNumForName(msd->toptexture);
          sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
          break;

        default:                        // normal cases
          sd->midtexture = R_SafeTextureNumForName(msd->midtexture, i);
          sd->toptexture = R_SafeTextureNumForName(msd->toptexture, i);
          sd->bottomtexture = R_SafeTextureNumForName(msd->bottomtexture, i);
          break;
        }
    }

  W_UnlockLumpNum(lump); // cph - release the lump
}

//
// jff 10/6/98
// New code added to speed up calculation of internal blockmap
// Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
//

#define blkshift 7               /* places to shift rel position for cell num */
#define blkmask ((1<<blkshift)-1)/* mask for rel position within cell */
#define blkmargin 0              /* size guardband around map used */
                                 // jff 10/8/98 use guardband>0
                                 // jff 10/12/98 0 ok with + 1 in rows,cols

typedef struct linelist_t        // type used to list lines in each block
{
  long num;
  struct linelist_t *next;
} linelist_t;

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit,
// though current algorithm is brute-force and unoptimal.
//

static void P_LoadBlockMap (int lump)
{
    _g->blockmaplump = W_CacheLumpNum(lump);

    W_UnlockLumpNum(lump); // cph - unlock the lump

    _g->bmaporgx = _g->blockmaplump[0]<<FRACBITS;
    _g->bmaporgy = _g->blockmaplump[1]<<FRACBITS;
    _g->bmapwidth = _g->blockmaplump[2];
    _g->bmapheight = _g->blockmaplump[3];


    // clear out mobj chains - CPhipps - use calloc
    _g->blocklinks = Z_Calloc (_g->bmapwidth*_g->bmapheight,sizeof(*_g->blocklinks),PU_LEVEL,0);

    _g->blockmap = _g->blockmaplump+4;
}

//
// P_LoadReject - load the reject table, padding it if it is too short
// totallines must be the number returned by P_GroupLines()
// an underflow will be padded with zeroes, or a doom.exe z_zone header
// 
// this function incorporates e6y's RejectOverrunAddInt code:
// e6y: REJECT overrun emulation code
// It's emulated successfully if the size of overflow no more than 16 bytes.
// No more desync on teeth-32.wad\teeth-32.lmp.
// http://www.doomworld.com/vb/showthread.php?s=&threadid=35214

static void P_LoadReject(int lumpnum, int totallines)
{
  unsigned int length, required;
  byte *newreject;

  // dump any old cached reject lump, then cache the new one
  if (_g->rejectlump != -1)
    W_UnlockLumpNum(_g->rejectlump);
  _g->rejectlump = lumpnum + ML_REJECT;
  _g->rejectmatrix = W_CacheLumpNum(_g->rejectlump);

  required = (_g->numsectors * _g->numsectors + 7) / 8;
  length = W_LumpLength(_g->rejectlump);

  if (length >= required)
    return; // nothing to do

  // allocate a new block and copy the reject table into it; zero the rest
  // PU_LEVEL => will be freed on level exit
  newreject = Z_Malloc(required, PU_LEVEL, NULL);
  _g->rejectmatrix = (const byte *)memmove(newreject, _g->rejectmatrix, length);
  memset(newreject + length, 0, required - length);
  // unlock the original lump, it is no longer needed
  W_UnlockLumpNum(_g->rejectlump);
  _g->rejectlump = -1;

  lprintf(LO_WARN, "P_LoadReject: REJECT too short (%u<%u) - padded\n",
          length, required);
}

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// cph 18/8/99: rewritten to avoid O(numlines * numsectors) section
// It makes things more complicated, but saves seconds on big levels
// figgi 09/18/00 -- adapted for gl-nodes

// cph - convenient sub-function
static void P_AddLineToSector(line_t* li, sector_t* sector)
{
  fixed_t *bbox = (void*)sector->blockbox;

  sector->lines[sector->linecount++] = li;
  M_AddToBox (bbox, li->v1->x, li->v1->y);
  M_AddToBox (bbox, li->v2->x, li->v2->y);
}

// modified to return totallines (needed by P_LoadReject)
static int P_GroupLines (void)
{
  register line_t *li;
  register sector_t *sector;
  int i,j, total = _g->numlines;

  // figgi
  for (i=0 ; i<_g->numsubsectors ; i++)
  {
    seg_t *seg = &_g->segs[_g->subsectors[i].firstline];
    _g->subsectors[i].sector = NULL;
    for(j=0; j<_g->subsectors[i].numlines; j++)
    {
      if(seg->sidedef)
      {
        _g->subsectors[i].sector = seg->sidedef->sector;
        break;
      }
      seg++;
    }
    if(_g->subsectors[i].sector == NULL)
      I_Error("P_GroupLines: Subsector a part of no sector!\n");
  }

  // count number of lines in each sector
  for (i=0,li=_g->lines; i<_g->numlines; i++, li++)
    {
      li->frontsector->linecount++;
      if (li->backsector && li->backsector != li->frontsector)
        {
          li->backsector->linecount++;
          total++;
        }
    }

  {  // allocate line tables for each sector
    line_t **linebuffer = Z_Malloc(total*sizeof(line_t *), PU_LEVEL, 0);

    // e6y: REJECT overrun emulation code
    // moved to P_LoadReject

    for (i=0, sector = _g->sectors; i<_g->numsectors; i++, sector++)
    {
      sector->lines = linebuffer;
      linebuffer += sector->linecount;
      sector->linecount = 0;
      M_ClearBox(sector->blockbox);
    }
  }

  // Enter those lines
  for (i=0,li=_g->lines; i<_g->numlines; i++, li++)
  {
    P_AddLineToSector(li, li->frontsector);
    if (li->backsector && li->backsector != li->frontsector)
      P_AddLineToSector(li, li->backsector);
  }

  for (i=0, sector = _g->sectors; i<_g->numsectors; i++, sector++)
  {
    fixed_t *bbox = (void*)sector->blockbox; // cph - For convenience, so
                                  // I can sue the old code unchanged
    int block;

    {
      //e6y: fix sound origin for large levels
      sector->soundorg.x = bbox[BOXRIGHT]/2+bbox[BOXLEFT]/2;
      sector->soundorg.y = bbox[BOXTOP]/2+bbox[BOXBOTTOM]/2;
    }

    // adjust bounding box to map blocks
    block = (bbox[BOXTOP]-_g->bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
    block = block >= _g->bmapheight ? _g->bmapheight-1 : block;
    sector->blockbox[BOXTOP]=block;

    block = (bbox[BOXBOTTOM]-_g->bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXBOTTOM]=block;

    block = (bbox[BOXRIGHT]-_g->bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
    block = block >= _g->bmapwidth ? _g->bmapwidth-1 : block;
    sector->blockbox[BOXRIGHT]=block;

    block = (bbox[BOXLEFT]-_g->bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXLEFT]=block;
  }

  return total; // this value is needed by the reject overrun emulation code
}

//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up

void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
  int   i;
  char  lumpname[9];
  int   lumpnum;

  _g->totallive = _g->totalkills = _g->totalitems = _g->totalsecret = _g->wminfo.maxfrags = 0;
  _g->wminfo.partime = 180;

  for (i=0; i<MAXPLAYERS; i++)
    _g->players[i].killcount = _g->players[i].secretcount = _g->players[i].itemcount = 0;

  // Initial height of PointOfView will be set by player think.
  _g->players[consoleplayer].viewz = 1;

  // Make sure all sounds are stopped before Z_FreeTags.
  S_Start();

  Z_FreeTags(PU_LEVEL, PU_PURGELEVEL-1);
  if (_g->rejectlump != -1) { // cph - unlock the reject table
    W_UnlockLumpNum(_g->rejectlump);
    _g->rejectlump = -1;
  }

  P_InitThinkers();

  // if working with a devlopment map, reload it
  //    W_Reload ();     killough 1/31/98: W_Reload obsolete

  // find map name
  if (_g->gamemode == commercial)
  {
    sprintf(lumpname, "map%02d", map);           // killough 1/24/98: simplify
  }
  else
  {
    sprintf(lumpname, "E%dM%d", episode, map);   // killough 1/24/98: simplify
  }

  lumpnum = W_GetNumForName(lumpname);

  _g->leveltime = 0; _g->totallive = 0;

  // note: most of this ordering is important

  // killough 3/1/98: P_LoadBlockMap call moved down to below
  // killough 4/4/98: split load of sidedefs into two parts,
  // to allow texture names to be used in special linedefs

  // figgi 10/19/00 -- check for gl lumps and load them
  P_GetNodesVersion(lumpnum,0);


  P_LoadVertexes  (lumpnum+ML_VERTEXES);
  P_LoadSectors   (lumpnum+ML_SECTORS);
  P_LoadSideDefs  (lumpnum+ML_SIDEDEFS);
  P_LoadLineDefs  (lumpnum+ML_LINEDEFS);
  P_LoadSideDefs2 (lumpnum+ML_SIDEDEFS);
  P_LoadLineDefs2 (lumpnum+ML_LINEDEFS);
  P_LoadBlockMap  (lumpnum+ML_BLOCKMAP);


    P_LoadSubsectors(lumpnum + ML_SSECTORS);
    P_LoadNodes(lumpnum + ML_NODES);
    P_LoadSegs(lumpnum + ML_SEGS);

  // reject loading and underflow padding separated out into new function
  // P_GroupLines modified to return a number the underflow padding needs
  P_LoadReject(lumpnum, P_GroupLines());

  // Note: you don't need to clear player queue slots --
  // a much simpler fix is in g_game.c -- killough 10/98

  /* cph - reset all multiplayer starts */
  memset(_g->playerstarts,0,sizeof(_g->playerstarts));

  for (i = 0; i < MAXPLAYERS; i++)
    _g->players[i].mo = NULL;

  P_MapStart();

  P_LoadThings(lumpnum+ML_THINGS);

  {
    for (i=0; i<MAXPLAYERS; i++)
      if (_g->playeringame[i] && !_g->players[i].mo)
        I_Error("P_SetupLevel: missing player %d start\n", i+1);
  }

  // killough 3/26/98: Spawn icon landings:
  if (_g->gamemode==commercial)
    P_SpawnBrainTargets();

  // set up world state
  P_SpawnSpecials();

  P_MapEnd();

}

//
// P_Init
//
void P_Init (void)
{
  P_InitSwitchList();

  P_InitPicAnims();

  R_InitSprites(sprnames);
}
