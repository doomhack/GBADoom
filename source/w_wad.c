/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2001 by
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
 *      Handles WAD file header, directory, lump I/O.
 *
 *-----------------------------------------------------------------------------
 */

// use config.h if autoconf made one -- josh
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <io.h>

#include "doomstat.h"
#include "d_net.h"
#include "doomtype.h"
#include "i_system.h"

#include "doom_iwad.h"

#ifdef __GNUG__
#pragma implementation "w_wad.h"
#endif
#include "w_wad.h"
#include "lprintf.h"

#include "global_data.h"

//
// GLOBALS
//
void ExtractFileBase (const char *path, char *dest)
{
    const char *src = path + strlen(path) - 1;
    int length;

    // back up until a \ or the start
    while (src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
           && *(src-1) != '\\'
           && *(src-1) != '/')
    {
        src--;
    }

    // copy up to eight characters
    memset(dest,0,8);
    length = 0;

    while ((*src) && (*src != '.') && (++length<9))
    {
        *dest++ = toupper(*src);
        *src++;
    }
    /* cph - length check removed, just truncate at 8 chars.
   * If there are 8 or more chars, we'll copy 8, and no zero termination
   */
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// Reload hack removed by Lee Killough
// CPhipps - source is an enum
//
// proff - changed using pointer to wadfile_info_t
static void W_AddFile()
{
    int         startlump;
    const wadinfo_t* header;
    int         length;
    filelump_t  *fileinfo;
    lumpinfo_t* lump_p;
    unsigned    i;


    if(doom_iwad && (doom_iwad_len > 0))
    {
        startlump = _g->numlumps;

        header = (wadinfo_t*)&doom_iwad[0];

        if (strncmp(header->identification,"IWAD",4))
            I_Error("W_AddFile: Wad file doesn't have IWAD id");


        length = header->numlumps*sizeof(filelump_t);

        fileinfo = (filelump_t*)&doom_iwad[header->infotableofs];
        _g->numlumps += header->numlumps;

        // Fill in lumpinfo
        _g->lumpinfo = realloc(_g->lumpinfo, _g->numlumps*sizeof(lumpinfo_t));

        lump_p = &_g->lumpinfo[startlump];

        for (i=startlump ; (int)i<_g->numlumps ; i++,lump_p++, fileinfo++)
        {
            lump_p->position = LONG(fileinfo->filepos);
            lump_p->size = LONG(fileinfo->size);
            lump_p->li_namespace = ns_global;              // killough 4/17/98
            strncpy (lump_p->name, fileinfo->name, 8);
        }
    }
}

// jff 1/23/98 Create routines to reorder the master directory
// putting all flats into one marked block, and all sprites into another.
// This will allow loading of sprites and flats from a PWAD with no
// other changes to code, particularly fast hashes of the lumps.
//
// killough 1/24/98 modified routines to be a little faster and smaller

static int IsMarker(const char *marker, const char *name)
{
  return !strncasecmp(name, marker, 8) ||
    (*name == *marker && !strncasecmp(name+1, marker, 7));
}

// killough 4/17/98: add namespace tags

static void W_CoalesceMarkedResource(const char *start_marker,
                                     const char *end_marker, int li_namespace)
{
    lumpinfo_t *marked = malloc(sizeof(*marked) * _g->numlumps);
    size_t i, num_marked = 0, num_unmarked = 0;
    int is_marked = 0, mark_end = 0;
    lumpinfo_t *lump = _g->lumpinfo;

    for (i=_g->numlumps; i--; lump++)
        if (IsMarker(start_marker, lump->name))       // start marker found
        { // If this is the first start marker, add start marker to marked lumps
            if (!num_marked)
            {
                strncpy(marked->name, start_marker, 8);
                marked->size = 0;  // killough 3/20/98: force size to be 0
                marked->li_namespace = ns_global;        // killough 4/17/98
                num_marked = 1;
            }
            is_marked = 1;                            // start marking lumps
        }
        else
            if (IsMarker(end_marker, lump->name))       // end marker found
            {
                mark_end = 1;                           // add end marker below
                is_marked = 0;                          // stop marking lumps
            }
            else
                if (is_marked)                            // if we are marking lumps,
                {                                       // move lump to marked list
                    marked[num_marked] = *lump;
                    marked[num_marked++].li_namespace = li_namespace;  // killough 4/17/98
                }
                else
                    _g->lumpinfo[num_unmarked++] = *lump;       // else move down THIS list

    // Append marked list to end of unmarked list
    memcpy(_g->lumpinfo + num_unmarked, marked, num_marked * sizeof(*marked));

    free(marked);                                   // free marked list

    _g->numlumps = num_unmarked + num_marked;           // new total number of lumps

    if (mark_end)                                   // add end marker
    {
        _g->lumpinfo[_g->numlumps].size = 0;  // killough 3/20/98: force size to be 0
        _g->lumpinfo[_g->numlumps].li_namespace = ns_global;   // killough 4/17/98
        strncpy(_g->lumpinfo[_g->numlumps++].name, end_marker, 8);
    }
}

// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
// by Lee Killough

unsigned W_LumpNameHash(const char *s)
{
  unsigned hash;
  (void) ((hash =        toupper(s[0]), s[1]) &&
          (hash = hash*3+toupper(s[1]), s[2]) &&
          (hash = hash*2+toupper(s[2]), s[3]) &&
          (hash = hash*2+toupper(s[3]), s[4]) &&
          (hash = hash*2+toupper(s[4]), s[5]) &&
          (hash = hash*2+toupper(s[5]), s[6]) &&
          (hash = hash*2+toupper(s[6]),
           hash = hash*2+toupper(s[7]))
         );
  return hash;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//

int (W_CheckNumForName)(register const char *name, register int li_namespace)
{
    // Hash function maps the name to one of possibly numlump chains.
    // It has been tuned so that the average chain length never exceeds 2.

    // proff 2001/09/07 - check numlumps==0, this happens when called before WAD loaded
    register int i = (_g->numlumps==0)?(-1):(_g->lumpinfo[W_LumpNameHash(name) % (unsigned) _g->numlumps].index);

    // We search along the chain until end, looking for case-insensitive
    // matches which also match a namespace tag. Separate hash tables are
    // not used for each namespace, because the performance benefit is not
    // worth the overhead, considering namespace collisions are rare in
    // Doom wads.

    while (i >= 0 && (strncasecmp(_g->lumpinfo[i].name, name, 8) ||
                      _g->lumpinfo[i].li_namespace != li_namespace))
        i = _g->lumpinfo[i].next;

    // Return the matching lump, or -1 if none found.

    return i;
}

//
// killough 1/31/98: Initialize lump hash table
//

void W_HashLumps(void)
{
    int i;

    for (i=0; i<_g->numlumps; i++)
        _g->lumpinfo[i].index = -1;                     // mark slots empty

    // Insert nodes to the beginning of each chain, in first-to-last
    // lump order, so that the last lump of a given name appears first
    // in any chain, observing pwad ordering rules. killough

    for (i=0; i<_g->numlumps; i++)
    {                                           // hash function:
        int j = W_LumpNameHash(_g->lumpinfo[i].name) % (unsigned) _g->numlumps;
        _g->lumpinfo[i].next = _g->lumpinfo[j].index;     // Prepend to list
        _g->lumpinfo[j].index = i;
    }
}

// End of lump hashing -- killough 1/31/98



// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (const char* name)     // killough -- const added
{
    int i = W_CheckNumForName (name);
    if (i == -1)
        I_Error("W_GetNumForName: %.8s not found", name);
    return i;
}



// W_Init
// Loads each of the files in the wadfiles array.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
// CPhipps - modified to use the new wadfiles array
//

void W_Init(void)
{
    // CPhipps - start with nothing

    _g->numlumps = 0; _g->lumpinfo = NULL;

    W_AddFile();

    if (!_g->numlumps)
        I_Error ("W_Init: No files found");

    //jff 1/23/98
    // get all the sprites and flats into one marked block each
    // killough 1/24/98: change interface to use M_START/M_END explicitly
    // killough 4/17/98: Add namespace tags to each entry
    // killough 4/4/98: add colormap markers
    W_CoalesceMarkedResource("S_START", "S_END", ns_sprites);
    W_CoalesceMarkedResource("F_START", "F_END", ns_flats);
    W_CoalesceMarkedResource("C_START", "C_END", ns_colormaps);
    W_CoalesceMarkedResource("B_START", "B_END", ns_prboom);

    // killough 1/31/98: initialize lump hash table
    W_HashLumps();

    /* cph 2001/07/07 - separated cache setup */
    lprintf(LO_INFO,"W_InitCache\n");
    W_InitCache();
}

void W_ReleaseAllWads(void)
{
	W_DoneCache();
    _g->numlumps = 0;
    free(_g->lumpinfo);
    _g->lumpinfo = NULL;
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
    if (lump >= _g->numlumps)
        I_Error ("W_LumpLength: %i >= numlumps",lump);

    return _g->lumpinfo[lump].size;
}

//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//

void W_ReadLump(int lump, void *dest)
{
    lumpinfo_t *l = _g->lumpinfo + lump;

    memcpy(dest, &doom_iwad[l->position], l->size);
}

const void* W_GetLumpPtr(int lump)
{
    lumpinfo_t *l = _g->lumpinfo + lump;

    return (const void*)&doom_iwad[l->position];
}
