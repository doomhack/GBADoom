/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by Colin Phipps, Florian Schulze
 *  
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
 *  Misc system stuff needed by Doom, implemented for POSIX systems.
 *  Timers and signals.
 *
 *-----------------------------------------------------------------------------
 */


#include <sys/time.h>

#include <sys/stat.h>
#include <errno.h>

#include "doomtype.h"
#include "m_fixed.h"
#include "i_system.h"
#include "doomdef.h"
#include "lprintf.h"

#include "i_system.h"
#include "i_system_e32.h"

#include "global_data.h"


/* CPhipps - believe it or not, it is possible with consecutive calls to 
 * gettimeofday to receive times out of order, e.g you query the time twice and
 * the second time is earlier than the first. Cheap'n'cheerful fix here.
 * NOTE: only occurs with bad kernel drivers loaded, e.g. pc speaker drv
 */
int I_GetTime(void)
{
    int thistimereply;

#ifndef __arm__
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    thistimereply = (tv.tv_sec * TICRATE + (tv.tv_usec * TICRATE) / 1000000);

    thistimereply = (thistimereply & 0xffff);
#else
    thistimereply = I_GetTime_e32();
#endif

    if (thistimereply < _g->lasttimereply)
    {
        _g->basetime -= 0xffff;
    }

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

/* cphipps - I_GetVersionString
 * Returns a version string in the given buffer 
 */
const char* I_GetVersionString(char* buf, size_t sz)
{
  sprintf(buf,"GBADoom v%s",VERSION);
  return buf;
}
