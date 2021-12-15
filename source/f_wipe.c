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
 *      Mission begin melt/wipe screen special effect.
 *
 *-----------------------------------------------------------------------------
 */

//Most of this code is backported from https://github.com/next-hack/nRF52840Doom


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "z_zone.h"
#include "doomdef.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "f_wipe.h"
#include "global_data.h"
#include "i_system_e32.h"

extern short* wipe_y_lookup;


#ifdef GBA
    #include <gba.h>
#endif

//
// SCREEN WIPE PACKAGE
//

int wipe_StartScreen(void)
{
    return 0;
}

int wipe_EndScreen(void)
{
    return 0;
}

// oh man, why aren't you commenting anything ?
// 2021-08-08 next-hack: commented and modified to use the dual buffer.
static int wipe_doMelt(int ticks)
{
    boolean done = true;

    unsigned short* backbuffer = I_GetBackBuffer();
    unsigned short* frontbuffer = I_GetFrontBuffer();

    while (ticks--)
    {
        for (int i = 0; i < SCREENWIDTH; i++)
        {
            if (wipe_y_lookup[i] < 0)
            {
                wipe_y_lookup[i]++;
                done = false;
                continue;
            }

            // scroll down columns, which are still visible
            if (wipe_y_lookup[i] < SCREENHEIGHT)
            {
                /* cph 2001/07/29 -
                 *  The original melt rate was 8 pixels/sec, i.e. 25 frames to melt
                 *  the whole screen, so make the melt rate depend on SCREENHEIGHT
                 *  so it takes no longer in high res
                 */
                int dy = (wipe_y_lookup[i] < 16) ? wipe_y_lookup[i] + 1 : SCREENHEIGHT / 25;
                // At most dy shall be so that the column is shifted by SCREENHEIGHT (i.e. just
                // invisible)
                if (wipe_y_lookup[i] + dy >= SCREENHEIGHT)
                    dy = SCREENHEIGHT - wipe_y_lookup[i];

                unsigned short* s = &frontbuffer[i] + ((SCREENHEIGHT - dy - 1) * SCREENPITCH);

                unsigned short* d = &frontbuffer[i] + ((SCREENHEIGHT - 1) * SCREENPITCH);

                // scroll down the column. Of course we need to copy from the bottom... up to
                // SCREENHEIGHT - yLookup - dy

                for (int j = SCREENHEIGHT - wipe_y_lookup[i] - dy; j; j--)
                {
                    *d = *s;
                    d += -SCREENPITCH;
                    s += -SCREENPITCH;
                }

                // copy new screen. We need to copy only between y_lookup and + dy y_lookup
                s = &backbuffer[i] +  wipe_y_lookup[i] * SCREENPITCH;
                d = &frontbuffer[i] + wipe_y_lookup[i] * SCREENPITCH;

                for (int j = 0 ; j < dy; j++)
                {
                    *d = *s;
                    d += SCREENPITCH;
                    s += SCREENPITCH;
                }

                wipe_y_lookup[i] += dy;
                done = false;
            }
        }
    }
    return done;
}

void wipe_initMelt()
{
    // setup initial column positions (y<0 => not ready to scroll yet)
    wipe_y_lookup[0] = -(M_Random() % 16);
    for (int i = 1; i < SCREENWIDTH; i++)
    {
        int r = (M_Random() % 3) - 1;

        wipe_y_lookup[i] = wipe_y_lookup[i - 1] + r;

        if (wipe_y_lookup[i] > 0)
            wipe_y_lookup[i] = 0;
        else if (wipe_y_lookup[i] == -16)
            wipe_y_lookup[i] = -15;
    }
}


int wipe_ScreenWipe(int ticks)
{
    // do a piece of wipe-in
    return wipe_doMelt(ticks);
}
