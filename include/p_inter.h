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
 *  Thing events, and dehacked specified numbers controlling them.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __P_INTER__
#define __P_INTER__

#include "d_player.h"
#include "p_mobj.h"

#ifdef __GNUG__
#pragma interface
#endif

/* Medikits, stimpacks and berserk heal up to this. */
#define MAXHEALTH 100

/* follow a player exlusively for 3 seconds */
#define BASETHRESHOLD   (100)

bool P_GivePower(player_t *, int);
void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);
void P_DamageMobj(mobj_t *target,mobj_t *inflictor,mobj_t *source,int damage);

/* The DeHackEd Misc values, from the GBAMISC lump (miscinfo_t, info.h).
 * The ammo per clip and maximums are miscinfo->clipammo and maxammo. */
#define initial_health    (miscinfo->initial_health)
#define initial_bullets   (miscinfo->initial_bullets)
#define maxhealth         (miscinfo->maxhealth)        /* health bonuses */
#define max_armor         (miscinfo->max_armor)
#define green_armor_class (miscinfo->green_armor_class)
#define blue_armor_class  (miscinfo->blue_armor_class)
#define max_soul          (miscinfo->max_soul)
#define soul_health       (miscinfo->soul_health)
#define mega_health       (miscinfo->mega_health)
#define god_health        (miscinfo->god_health)
#define idfa_armor        (miscinfo->idfa_armor)
#define idfa_armor_class  (miscinfo->idfa_armor_class)
#define bfgcells          (miscinfo->bfgcells)

#endif
