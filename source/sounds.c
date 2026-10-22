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
 *      Created by a sound utility.
 *      Kept as a sample, DOOM2 sounds.
 *
 *-----------------------------------------------------------------------------*/

// killough 5/3/98: reformatted

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sounds.h"

//
// Information about all the sfx
//

const sfxinfo_t S_sfx[] = {
  // S_sfx[0] needs to be a dummy for odd reasons.
  { "none", false,  0, 0, -1 },
  { "pistol", false, 64, 0, -1 },
  { "shotgn", false, 64, 0, -1 },
  { "sgcock", false, 64, 0, -1 },
  { "dshtgn", false, 64, 0, -1 },
  { "dbopn", false, 64, 0, -1 },
  { "dbcls", false, 64, 0, -1 },
  { "dbload", false, 64, 0, -1 },
  { "plasma", false, 64, 0, -1 },
  { "bfg", false, 64, 0, -1 },
  { "sawup", false, 64, 0, -1 },
  { "sawidl", false, 118, 0, -1 },
  { "sawful", false, 64, 0, -1 },
  { "sawhit", false, 64, 0, -1 },
  { "rlaunc", false, 64, 0, -1 },
  { "rxplod", false, 70, 0, -1 },
  { "firsht", false, 70, 0, -1 },
  { "firxpl", false, 70, 0, -1 },
  { "pstart", false, 100, 0, -1 },
  { "pstop", false, 100, 0, -1 },
  { "doropn", false, 100, 0, -1 },
  { "dorcls", false, 100, 0, -1 },
  { "stnmov", false, 119, 0, -1 },
  { "swtchn", false, 78, 0, -1 },
  { "swtchx", false, 78, 0, -1 },
  { "plpain", false, 96, 0, -1 },
  { "dmpain", false, 96, 0, -1 },
  { "popain", false, 96, 0, -1 },
  { "vipain", false, 96, 0, -1 },
  { "mnpain", false, 96, 0, -1 },
  { "pepain", false, 96, 0, -1 },
  { "slop", false, 78, 0, -1 },
  { "itemup", true, 78, 0, -1 },
  { "wpnup", true, 78, 0, -1 },
  { "oof", false, 96, 0, -1 },
  { "telept", false, 32, 0, -1 },
  { "posit1", true, 98, 0, -1 },
  { "posit2", true, 98, 0, -1 },
  { "posit3", true, 98, 0, -1 },
  { "bgsit1", true, 98, 0, -1 },
  { "bgsit2", true, 98, 0, -1 },
  { "sgtsit", true, 98, 0, -1 },
  { "cacsit", true, 98, 0, -1 },
  { "brssit", true, 94, 0, -1 },
  { "cybsit", true, 92, 0, -1 },
  { "spisit", true, 90, 0, -1 },
  { "bspsit", true, 90, 0, -1 },
  { "kntsit", true, 90, 0, -1 },
  { "vilsit", true, 90, 0, -1 },
  { "mansit", true, 90, 0, -1 },
  { "pesit", true, 90, 0, -1 },
  { "sklatk", false, 70, 0, -1 },
  { "sgtatk", false, 70, 0, -1 },
  { "skepch", false, 70, 0, -1 },
  { "vilatk", false, 70, 0, -1 },
  { "claw", false, 70, 0, -1 },
  { "skeswg", false, 70, 0, -1 },
  { "pldeth", false, 32, 0, -1 },
  { "pdiehi", false, 32, 0, -1 },
  { "podth1", false, 70, 0, -1 },
  { "podth2", false, 70, 0, -1 },
  { "podth3", false, 70, 0, -1 },
  { "bgdth1", false, 70, 0, -1 },
  { "bgdth2", false, 70, 0, -1 },
  { "sgtdth", false, 70, 0, -1 },
  { "cacdth", false, 70, 0, -1 },
  { "skldth", false, 70, 0, -1 },
  { "brsdth", false, 32, 0, -1 },
  { "cybdth", false, 32, 0, -1 },
  { "spidth", false, 32, 0, -1 },
  { "bspdth", false, 32, 0, -1 },
  { "vildth", false, 32, 0, -1 },
  { "kntdth", false, 32, 0, -1 },
  { "pedth", false, 32, 0, -1 },
  { "skedth", false, 32, 0, -1 },
  { "posact", true, 120, 0, -1 },
  { "bgact", true, 120, 0, -1 },
  { "dmact", true, 120, 0, -1 },
  { "bspact", true, 100, 0, -1 },
  { "bspwlk", true, 100, 0, -1 },
  { "vilact", true, 100, 0, -1 },
  { "noway", false, 78, 0, -1 },
  { "barexp", false, 60, 0, -1 },
  { "punch", false, 64, 0, -1 },
  { "hoof", false, 70, 0, -1 },
  { "metal", false, 70, 0, -1 },
  { "chgun", false, 64, &S_sfx[sfx_pistol], 150 },
  { "tink", false, 60, 0, -1 },
  { "bdopn", false, 100, 0, -1 },
  { "bdcls", false, 100, 0, -1 },
  { "itmbk", false, 100, 0, -1 },
  { "flame", false, 32, 0, -1 },
  { "flamst", false, 32, 0, -1 },
  { "getpow", false, 60, 0, -1 },
  { "bospit", false, 70, 0, -1 },
  { "boscub", false, 70, 0, -1 },
  { "bossit", false, 70, 0, -1 },
  { "bospn", false, 70, 0, -1 },
  { "bosdth", false, 70, 0, -1 },
  { "manatk", false, 70, 0, -1 },
  { "mandth", false, 70, 0, -1 },
  { "sssit", false, 70, 0, -1 },
  { "ssdth", false, 70, 0, -1 },
  { "keenpn", false, 70, 0, -1 },
  { "keendt", false, 70, 0, -1 },
  { "skeact", false, 70, 0, -1 },
  { "skesit", false, 70, 0, -1 },
  { "skeatk", false, 70, 0, -1 },
  { "radio", false, 60, 0, -1 },
};
