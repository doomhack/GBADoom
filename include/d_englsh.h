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
 *  Printed strings for translation.
 *  English language support (default).
 *  See dstrings.h for suggestions about foreign language BEX support
 *
 *-----------------------------------------------------------------------------*/

#ifndef __D_ENGLSH__
#define __D_ENGLSH__

/* The strings DeHackEd can replace (pickups, level names, finale texts,
 * cast names) are in deh_text.h. */

/* m_menu.c */
#define PRESSKEY    "press a key."
#define PRESSYN     "press A or B."
#define SAVEDEAD    "you can't save if\nyou aren't playing!\n\n" PRESSKEY

#define NIGHTMARE \
  "are you sure? this skill level\n"\
  "isn't even remotely fair.\n\n" PRESSYN

#define SWSTRING  \
  "this is the shareware version\n"\
  "of doom. You need to order\n"\
  "the entire trilogy.\n\n" PRESSKEY

#define MSGOFF      "Messages OFF"
#define MSGON       "Messages ON"

#define RUNOFF      "Take your time."
#define RUNON       "In a hurry, marine?"

#define HIGHDETAIL  "High Detail."
#define LOWDETAIL   "Low Detail."

#define ENDGAME     "are you sure you want to\nend the game?\n\n" PRESSYN

/* p_doors.c */
/* jff 02/05/98 Create messages specific to card and skull keys */
#define PD_BLUEC    "This door needs a blue card"
#define PD_REDC     "This door needs a red card"
#define PD_YELLOWC  "This door needs a yellow card"
#define PD_BLUES    "This door needs a blue skull"
#define PD_REDS     "This door needs a red skull"
#define PD_YELLOWS  "This door needs a yellow skull"
#define PD_ANY      "Any key will open this door"
#define PD_ALL3     "This door needs all three keys"
#define PD_ALL6     "This door needs all six keys"

/* g_game.c */
#define GGSAVED     "game saved."

/* The following should NOT be changed unless it seems
 * just AWFULLY necessary */

/* am_map.c */

#define AMSTR_FOLLOWON    "Follow Mode ON"
#define AMSTR_FOLLOWOFF   "Follow Mode OFF"

#define AMSTR_GRIDON      "Grid ON"
#define AMSTR_GRIDOFF     "Grid OFF"

#define AMSTR_MARKEDSPOT  "Marked Spot"
#define AMSTR_MARKSCLEARED  "All Marks Cleared"

#define AMSTR_ROTATEON    "Rotate Mode ON"
#define AMSTR_ROTATEOFF   "Rotate Mode OFF"

#define AMSTR_OVERLAYON    "Overlay Mode ON"
#define AMSTR_OVERLAYOFF   "Overlay Mode OFF"

/* st_stuff.c */
#define STSTR_BEHOLD    "inVuln Str Invis Rad Amap Lite"
#define STSTR_BEHOLDX   "Power-up Toggled"

#define STSTR_CLEV      "Changing Level..."

#define STSTR_FPSON    "FPS Counter On"
#define STSTR_FPSOFF   "FPS Counter Off"

#endif
