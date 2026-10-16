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
 *  Sprite names, the action functions, and loading the dehackable
 *  tables (states, things, weapons, misc values and text) from the WAD.
 *  Their default data is in GbaWadUtil (tools/GbaWadUtil/infotables.cpp).
 *
 *-----------------------------------------------------------------------------
 */

#include "doomdef.h"
#include "sounds.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_enemy.h"
#include "p_pspr.h"
#include "info.h"
#include "dstrings.h"
#include "w_wad.h"
#include "lprintf.h"
#include "global_data.h"


// ********************************************************************
// Sprite names
// ********************************************************************
// This is the list of sprite 4-character prefixes.  They are searched
// through, with a NULL entry terminating the list.  In DOOM originally
// this NULL entry was missing, and coincidentally the next thing in
// memory was the dummy state_t[] entry that started with zero bytes.
// killough 1/17/98: add an explicit NULL entry.
// NUMSPRITES is an enum from info.h where all these are listed
// as SPR_xxxx

const char * const sprnames[NUMSPRITES+1] = {
  "TROO","SHTG","PUNG","PISG","PISF","SHTF","SHT2","CHGG","CHGF","MISG",
  "MISF","SAWG","PLSG","PLSF","BFGG","BFGF","BLUD","PUFF","BAL1","BAL2",
  "PLSS","PLSE","MISL","BFS1","BFE1","BFE2","TFOG","IFOG","PLAY","POSS",
  "SPOS","VILE","FIRE","FATB","FBXP","SKEL","MANF","FATT","CPOS","SARG",
  "HEAD","BAL7","BOSS","BOS2","SKUL","SPID","BSPI","APLS","APBX","CYBR",
  "PAIN","SSWV","KEEN","BBRN","BOSF","ARM1","ARM2","BAR1","BEXP","FCAN",
  "BON1","BON2","BKEY","RKEY","YKEY","BSKU","RSKU","YSKU","STIM","MEDI",
  "SOUL","PINV","PSTR","PINS","MEGA","SUIT","PMAP","PVIS","CLIP","AMMO",
  "ROCK","BROK","CELL","CELP","SHEL","SBOX","BPAK","BFUG","MGUN","CSAW",
  "LAUN","PLAS","SHOT","SGN2","COLU","SMT2","GOR1","POL2","POL5","POL4",
  "POL3","POL1","POL6","GOR2","GOR3","GOR4","GOR5","SMIT","COL1","COL2",
  "COL3","COL4","CAND","CBRA","COL6","TRE1","TRE2","ELEC","CEYE","FSKU",
  "COL5","TBLU","TGRN","TRED","SMBT","SMGT","SMRT","HDB1","HDB2","HDB3",
  "HDB4","HDB5","HDB6","POB1","POB2","BRS1","TLMP","TLP2",
  "TNT1", // invisible sprite                                 phares 3/9/98
  NULL
};

// ********************************************************************
// Action functions
// ********************************************************************
// state_t.action indexes this, in actionnum_t order (info.h).

const actionf_t actions[NUMACTIONS] =
{
    NULL,
#define INFO_ACTION_FUNC(a) (actionf_t)A_##a,
    INFO_ACTIONS(INFO_ACTION_FUNC)
#undef INFO_ACTION_FUNC
};

// ********************************************************************
// The dehackable tables
// ********************************************************************
// GbaWadUtil writes the states, things, weapons, misc values and
// DeHackEd text to the WAD, with any DeHackEd patches applied. They are
// used in place, from ROM on the GBA.

static const void* P_InfoLump(const char* name, int size)
{
    int lump = W_GetNumForName(name);

    if(W_LumpLength(lump) != size)
        I_Error("P_InitInfoTables: %s is %d bytes, not %d.\nRebuild the WAD with this GbaWadUtil.", name, W_LumpLength(lump), size);

    return W_CacheLumpNum(lump);
}

void P_InitInfoTables(void)
{
    states = P_InfoLump("GBASTATE", sizeof(state_t) * NUMSTATES);
    mobjinfo = P_InfoLump("GBAMOBJ", sizeof(mobjinfo_t) * NUMMOBJTYPES);
    weaponinfo = P_InfoLump("GBAWEAP", sizeof(weaponinfo_t) * NUMWEAPONS);
    miscinfo = P_InfoLump("GBAMISC", sizeof(miscinfo_t));

    // An offset to each string, then the strings.
    int lump = W_GetNumForName("GBATEXT");
    const unsigned int* text = W_CacheLumpNum(lump);

    if(W_LumpLength(lump) < (int)sizeof(*text) * NUMTEXTS || text[0] != sizeof(*text) * NUMTEXTS)
        I_Error("P_InitInfoTables: GBATEXT doesn't have %d strings.\nRebuild the WAD with this GbaWadUtil.", NUMTEXTS);

    _g->text = text;
}
