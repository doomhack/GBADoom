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
 *    Network client. Passes information to/from server, staying
 *    synchronised.
 *    Contains the main wait loop, waiting for network input or
 *    time before doing the next tic.
 *    Rewritten for LxDoom, but based around bits of the old code.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "doomtype.h"
#include "doomstat.h"
#include "d_net.h"
#include "z_zone.h"

#include "d_main.h"
#include "g_game.h"
#include "m_menu.h"
#include "p_checksum.h"

#include "protocol.h"
#include "i_network.h"
#include "i_system.h"
#include "i_main.h"
#include "i_video.h"
#include "m_argv.h"
#include "lprintf.h"

static boolean   server;
static int       remotetic; // Tic expected from the remote
ticcmd_t         netcmds[MAXPLAYERS][BACKUPTICS];
static ticcmd_t* localcmds;
int maketic;
int ticdup = 1;

doomcom_t*      doomcom;



void D_InitNetGame (void)
{
  int i;

  doomcom = Z_Malloc(sizeof *doomcom, PU_STATIC, NULL);
  doomcom->consoleplayer = 0;
  doomcom->numnodes = 0; doomcom->numplayers = 1;
  localcmds = netcmds[consoleplayer];
  netgame = (M_CheckParm("-solo-net") != 0);

  for (i=0; i<doomcom->numplayers; i++)
    playeringame[i] = true;
  for (; i<MAXPLAYERS; i++)
    playeringame[i] = false;

  consoleplayer = displayplayer = doomcom->consoleplayer;
}

void D_BuildNewTiccmds(void)
{
    static int lastmadetic;
    int newtics = I_GetTime() - lastmadetic;
    lastmadetic += newtics;
    while (newtics--)
    {
      I_StartTic();
      if (maketic - gametic > BACKUPTICS/2) break;
      G_BuildTiccmd(&localcmds[maketic%BACKUPTICS]);
      maketic++;
    }
}

void TryRunTics (void)
{
	int runtics;
	int entertime = I_GetTime();

	// Wait for tics to run
	while (1) 
	{

		D_BuildNewTiccmds();

	    runtics = (server ? remotetic : maketic) - gametic;
		if (!runtics)
		{
			if (I_GetTime() - entertime > 10)
			{
				M_Ticker(); 
				return;
			}
		}
		else
			break;
	}

	while (runtics--)
	{

		if (advancedemo)
			D_DoAdvanceDemo ();

	    M_Ticker ();
		I_GetTime_SaveMS();
		G_Ticker ();
		P_Checksum(gametic);
		gametic++;
	}
}
