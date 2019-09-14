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
 *      Startup and quit functions. Handles signals, inits the
 *      memory management, then calls D_DoomMain. Also contains
 *      I_Init which does other system-related startup stuff.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "m_fixed.h"
#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "lprintf.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_misc.h"
#include "i_sound.h"
#include "i_main.h"
#include "lprintf.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* Most of the following has been rewritten by Lee Killough
 *
 * I_GetTime
 * killough 4/13/98: Make clock rate adjustable by scale factor
 * cphipps - much made static
 */
static int  I_GetTime_FastDemo(void)
{
  static int fasttic;
  return fasttic++;
}



static int I_GetTime_Error(void)
{
  I_Error("I_GetTime_Error: GetTime() used before initialization");
  return 0;
}



int (*I_GetTime)(void) = I_GetTime_Error;

void I_Init(void)
{
  /* killough 4/14/98: Adjustable speedup based on realtic_clock_rate */
  if (fastdemo)
    I_GetTime = I_GetTime_FastDemo;
  else
  {
      I_GetTime = I_GetTime_RealTime;
  }

  {
    /* killough 2/21/98: avoid sound initialization if no sound & no music */
    if (!(nomusicparm && nosfxparm))
      I_InitSound();
  }
}

static void PrintVer(void)
{
  char vbuf[200];
  lprintf(LO_INFO,"%s\n",I_GetVersionString(vbuf,200));
}

/* I_EndDoom
 * Prints out ENDOOM or ENDBOOM, using some common sense to decide which.
 * cphipps - moved to l_main.c, made static
 */
static void I_EndDoom(void)
{
  int lump_eb, lump_ed, lump = -1;

  /* CPhipps - ENDOOM/ENDBOOM selection */
  lump_eb = W_CheckNumForName("ENDBOOM");/* jff 4/1/98 sign our work    */
  lump_ed = W_CheckNumForName("ENDOOM"); /* CPhipps - also maybe ENDOOM */

  if (lump_eb == -1)
    lump = lump_ed;
  else if (lump_ed == -1)
    lump = lump_eb;
  else
  { /* Both ENDOOM and ENDBOOM are present */
#define LUMP_IS_NEW(num) (!((lumpinfo[num].source == source_iwad) || (lumpinfo[num].source == source_auto_load)))
    switch ((LUMP_IS_NEW(lump_ed) ? 1 : 0 ) |
      (LUMP_IS_NEW(lump_eb) ? 2 : 0)) {
    case 1:
      lump = lump_ed;
      break;
    case 2:
      lump = lump_eb;
      break;
    default:
      /* Both lumps have equal priority, both present */
      lump = (P_Random(pr_misc) & 1) ? lump_ed : lump_eb;
      break;
    }
  }

  if (lump != -1)
  {
    const char (*endoom)[2] = (const void*)W_CacheLumpNum(lump);
    int i, l = W_LumpLength(lump) / 2;

    /* cph - colour ENDOOM by rain */
    int oldbg = -1, oldcolor = -1, bold = 0, oldbold = -1, color = 0;


    lprintf(LO_INFO,"\n");
    for (i=0; i<l; i++)
    {
      /* cph - portable ascii printout if requested */
      if (isascii(endoom[i][0]))
        lprintf(LO_INFO,"%c",endoom[i][0]);
      else /* Probably a box character, so do #'s */
        lprintf(LO_INFO,"#");
    }

    W_UnlockLumpNum(lump);
  }
}

static int has_exited;

/* I_SafeExit
 * This function is called instead of exit() by functions that might be called
 * during the exit process (i.e. after exit() has already been called)
 * Prevent infinitely recursive exits -- killough
 */

void I_SafeExit(int rc)
{
  if (!has_exited)    /* If it hasn't exited yet, exit now -- killough */
    {
      has_exited=rc ? 2 : 1;
      exit(rc);
    }
}

int main(int argc, const char * const * argv)
{
	/* Version info */
	lprintf(LO_INFO,"\n");
	PrintVer();
	lprintf(LO_INFO,"\n");


  Z_Init();                  /* 1/18/98 killough: start up memory stuff first */

  I_SetAffinityMask();

  /* cphipps - call to video specific startup code */
  I_PreInitGraphics();

  D_DoomMain ();
  return 0;
}
