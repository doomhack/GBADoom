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
 *      Status bar code.
 *      Does the face/direction indicator animatin.
 *      Does palette indicators as well (red pain/berserk, bright pickup)
 *
 *-----------------------------------------------------------------------------*/

#include "doomdef.h"
#include "m_random.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "st_lib.h"
#include "r_main.h"
#include "am_map.h"
#include "global_data.h"

//
// STATUS BAR CODE
//

static void ST_Stop(void);

// Respond to keyboard input events,
//  intercept cheats.
bool ST_Responder(const event_t *ev)
{
    // Filter automap on/off.
    if (ev->type == ev_keyup && (ev->data1 & 0xffff0000) == AM_MSGHEADER)
    {
        switch(ev->data1)
        {
        case AM_MSGENTERED:
            break;

        case AM_MSGEXITED:
            break;
        }
    }

    return false;
}

static int ST_calcPainOffset(void)
{
    static int lastcalc;
    static int oldhealth = -1;
    int health = _g->player.health > 100 ? 100 : _g->player.health;

    if (health != oldhealth)
    {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lastcalc;
}

//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//

static void ST_updateFaceWidget(void)
{
    int         i;
    angle_t     badguyangle;
    angle_t     diffang;
    static int  lastattackdown = -1;
    static int  priority = 0;
    bool     doevilgrin;

    if (priority < 10)
    {
        // dead
        if (!_g->player.health)
        {
            priority = 9;
            _g->st_faceindex = ST_DEADFACE;
            _g->st_facecount = 1;
        }
    }

    if (priority < 9)
    {
        if (_g->player.bonuscount)
        {
            // picking up bonus
            doevilgrin = false;

            for (i=0;i<NUMWEAPONS;i++)
            {
                if (_g->oldweaponsowned[i] != _g->player.weaponowned[i])
                {
                    doevilgrin = true;
                    _g->oldweaponsowned[i] = _g->player.weaponowned[i];
                }
            }
            if (doevilgrin)
            {
                // evil grin if just picked up weapon
                priority = 8;
                _g->st_facecount = ST_EVILGRINCOUNT;
                _g->st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }

    }

    //Restore the face looking at enemies direction in this SVN... Cause it's handy! ~Kippykip
    if (priority < 8)
    {
        if (_g->player.damagecount && _g->player.attacker && _g->player.attacker != _g->player.mo)
        {
            // being attacked
            priority = 7;

            // haleyjd 10/12/03: classic DOOM problem of missing OUCH face
            // was due to inversion of this test:
            // if(plyr->health - st_oldhealth > ST_MUCHPAIN)
            if(_g->st_oldhealth - _g->player.health > ST_MUCHPAIN)
            {
                _g->st_facecount = ST_TURNCOUNT;
                _g->st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                badguyangle = R_PointToAngle2(_g->player.mo->x,
                                              _g->player.mo->y,
                                              _g->player.attacker->x,
                                              _g->player.attacker->y);

                if (badguyangle > _g->player.mo->angle)
                {
                    // whether right or left
                    diffang = badguyangle - _g->player.mo->angle;
                    i = diffang > ANG180;
                }
                else
                {
                    // whether left or right
                    diffang = _g->player.mo->angle - badguyangle;
                    i = diffang <= ANG180;
                } // confusing, aint it?


                _g->st_facecount = ST_TURNCOUNT;
                _g->st_faceindex = ST_calcPainOffset();

                if (diffang < ANG45)
                {
                    // head-on
                    _g->st_faceindex += ST_RAMPAGEOFFSET;
                }
                else if (i)
                {
                    // turn face right
                    _g->st_faceindex += ST_TURNOFFSET;
                }
                else
                {
                    // turn face left
                    _g->st_faceindex += ST_TURNOFFSET+1;
                }
            }
        }
    }

    if (priority < 7)
    {
        if (_g->player.damagecount)
        {
            // haleyjd 10/12/03: classic DOOM problem of missing OUCH face
            // was due to inversion of this test:
            // if(plyr->health - st_oldhealth > ST_MUCHPAIN)
            if(_g->st_oldhealth - _g->player.health > ST_MUCHPAIN)
            {
                priority = 7;
                _g->st_facecount = ST_TURNCOUNT;
                _g->st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                _g->st_facecount = ST_TURNCOUNT;
                _g->st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
            }

        }
    }

    if (priority < 6)
    {
        // rapid firing
        if (_g->player.attackdown)
        {
            if (lastattackdown==-1)
                lastattackdown = ST_RAMPAGEDELAY;
            else if (!--lastattackdown)
            {
                priority = 5;
                _g->st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
                _g->st_facecount = 1;
                lastattackdown = 1;
            }
        }
        else
            lastattackdown = -1;

    }

    if (priority < 5)
    {
        // invulnerability
        if ((_g->player.cheats & CF_GODMODE)
                || _g->player.powers[pw_invulnerability])
        {
            priority = 4;

            _g->st_faceindex = ST_GODFACE;
            _g->st_facecount = 1;

        }

    }

    // look left or look right if the facecount has timed out
    if (!_g->st_facecount)
    {
        _g->st_faceindex = ST_calcPainOffset() + (_g->st_randomnumber % 3);
        _g->st_facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    _g->st_facecount--;

}

static const int* ST_readyAmmo(void)
{
    static const int  largeammo = 1994; // means "n/a"

    if(_g->fps_show)
        return (const int*)&_g->fps_framerate;
    else if (weaponinfo[_g->player.readyweapon].ammo == am_noammo)
        return &largeammo;
    else
        return &_g->player.ammo[weaponinfo[_g->player.readyweapon].ammo];
}

static void ST_updateWidgets(void)
{
    int         i;

    _g->w_ready.num = ST_readyAmmo();

    // update keycard multiple widgets
    for (i=0;i<3;i++)
    {
        _g->keyboxes[i] = _g->player.cards[i] ? i : -1;

        //jff 2/24/98 select double key
        //killough 2/28/98: preserve traditional keys by config option

        if (_g->player.cards[i+3])
            _g->keyboxes[i] = i+3;
    }

    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();
}

void ST_Ticker(void)
{
    _g->st_randomnumber = M_Random();
    ST_updateWidgets();
    _g->st_oldhealth = _g->player.health;
}


static void ST_doPaletteStuff(void)
{
    int         palette;
    int cnt = _g->player.damagecount;

    if (_g->player.powers[pw_strength])
    {
        // slowly fade the berzerk out
        int bzc = 12 - (_g->player.powers[pw_strength]>>6);
        if (bzc > cnt)
            cnt = bzc;
    }

    if (cnt)
    {
        palette = (cnt+7)>>3;
        if (palette >= NUMREDPALS)
            palette = NUMREDPALS-1;

        /* cph 2006/08/06 - if in the menu, reduce the red tint - navigating to
       * load a game can be tricky if the screen is all red */
        if (_g->menuactive) palette >>=1;

        palette += STARTREDPALS;
    }
    else
        if (_g->player.bonuscount)
        {
            palette = (_g->player.bonuscount+7)>>3;
            if (palette >= NUMBONUSPALS)
                palette = NUMBONUSPALS-1;
            palette += STARTBONUSPALS;
        }
        else
            if (_g->player.powers[pw_ironfeet] > 4*32 || _g->player.powers[pw_ironfeet] & 8)
                palette = RADIATIONPAL;
            else
                palette = 0;

    if (palette != _g->st_palette) {
        V_SetPalette(_g->st_palette = palette); // CPhipps - use new palette function
    }
}

static void ST_drawWidgets(bool refresh)
{
    STlib_updateNum(&_g->w_ready, refresh);

    // Restore the ammo numbers for backpack stats I guess, etc ~Kippykip
    for (int i=0;i<4;i++)
    {
        STlib_updateNum(&_g->w_ammo[i], refresh);
        STlib_updateNum(&_g->w_maxammo[i], refresh);
    }

    STlib_updateNum(&_g->st_health, refresh);

    STlib_updateNum(&_g->st_armor, refresh);

    STlib_updateMultIcon(&_g->w_faces, refresh);

    for (int i=0;i<3;i++)
        STlib_updateMultIcon(&_g->w_keyboxes[i], refresh);

    for (int i=0;i<6;i++)
        STlib_updateMultIcon(&_g->w_arms[i], refresh);
}

void ST_Drawer(bool statusbaron, bool refresh)
{
    /* cph - let status bar on be controlled
   * completely by the call from D_Display
   * proff - really do it
   */

    ST_doPaletteStuff();  // Do red-/gold-shifts from damage/items

    if (statusbaron)
    {
        // st_needrefresh 2: draw the whole status bar into the back page,
        // 1: then copy it to the other page on the next frame.
        if (refresh)
            _g->st_needrefresh = 2;

        bool redraw = (_g->st_needrefresh == 2);

        if (redraw)
            ST_refreshBackground();
        else if (_g->st_needrefresh)
            ST_copyFromFront();

        if (_g->st_needrefresh)
            _g->st_needrefresh--;

        // Widgets that changed since the last frame are redrawn.
        ST_drawWidgets(redraw);
    }
}



//
// ST_loadGraphics
//
// CPhipps - Loads graphics needed for status bar if doload is true,
//  unloads them otherwise
//
static void ST_loadGraphics()
{
    int  i, facenum;
    char namebuf[9];

    // Load the numbers, tall and short
    for (i=0;i<10;i++)
    {
        //sprintf(namebuf, "STTNUM%d", i);
        sprintf(namebuf, "STGANUM%d", i); //Special GBA Doom II Red Numbers ~Kippykip
        _g->tallnum[i] = (const patch_t *) W_CacheLumpName(namebuf);

        sprintf(namebuf, "STYSNUM%d", i);
        _g->shortnum[i] = (const patch_t *) W_CacheLumpName(namebuf);
    }

    // key cards
    for (i=0;i<NUMCARDS;i++)
    {
        sprintf(namebuf, "STKEYS%d", i);
        _g->keys[i] = (const patch_t *) W_CacheLumpName(namebuf);
    }

    // arms ownership widgets
    for (i=0;i<6;i++)
    {
        sprintf(namebuf, "STGNUM%d", i+2);

        // gray #
        _g->arms[i][0] = (const patch_t *) W_CacheLumpName(namebuf);

        // yellow #
        _g->arms[i][1] = (const patch_t *) _g->shortnum[i+2];
    }

    // face states
    facenum = 0;

    for (i=0;i<ST_NUMPAINFACES;i++)
    {
        for (int j=0;j<ST_NUMSTRAIGHTFACES;j++)
        {
            sprintf(namebuf, "STFST%d%d", i, j);
            _g->faces[facenum++] = W_CacheLumpName(namebuf);
        }
        sprintf(namebuf, "STFTR%d0", i);	// turn right
        _g->faces[facenum++] = W_CacheLumpName(namebuf);
        sprintf(namebuf, "STFTL%d0", i);	// turn left
        _g->faces[facenum++] = W_CacheLumpName(namebuf);
        sprintf(namebuf, "STFOUCH%d", i);	// ouch!
        _g->faces[facenum++] = W_CacheLumpName(namebuf);
        sprintf(namebuf, "STFEVL%d", i);	// evil grin ;)
        _g->faces[facenum++] = W_CacheLumpName(namebuf);
        sprintf(namebuf, "STFKILL%d", i);	// pissed off
        _g->faces[facenum++] = W_CacheLumpName(namebuf);
    }
    _g->faces[facenum++] = W_CacheLumpName("STFGOD0");
    _g->faces[facenum++] = W_CacheLumpName("STFDEAD0");
}

static void ST_loadData(void)
{
    ST_loadGraphics();
}

static void ST_initData(void)
{
    int i;

    _g->st_statusbaron = true;

    _g->st_faceindex = 0;
    _g->st_palette = -1;

    _g->st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
        _g->oldweaponsowned[i] = _g->player.weaponowned[i];

    for (i=0;i<3;i++)
        _g->keyboxes[i] = -1;

    // draw the whole status bar on the first frame
    _g->st_needrefresh = 2;
}

static void ST_createWidgets(void)
{
    int i;

    // ready weapon ammo
    STlib_initNum(&_g->w_ready,
                  ST_AMMOX,
                  ST_AMMOY,
                  _g->tallnum,
                  ST_readyAmmo(),
                  ST_AMMOWIDTH );

    // health percentage (the % sign is part of the background)
    STlib_initNum(&_g->st_health,
                  ST_HEALTHX,
                  ST_HEALTHY,
                  _g->tallnum,
                  &_g->player.health,
                  ST_HEALTHWIDTH);

    // armor percentage - should be colored later
    STlib_initNum(&_g->st_armor,
                  ST_ARMORX,
                  ST_ARMORY,
                  _g->tallnum,
                  &_g->player.armorpoints,
                  ST_ARMORWIDTH);

    // weapons owned
    for(i=0;i<6;i++)
    {
        STlib_initMultIcon(&_g->w_arms[i],
                           ST_ARMSX+(i%3)*ST_ARMSXSPACE,
                           ST_ARMSY+(i/3)*ST_ARMSYSPACE,
                           _g->arms[i], 2,
                           &_g->player.weaponowned[i+1]);
    }

    // keyboxes 0-2
    STlib_initMultIcon(&_g->w_keyboxes[0],
                       ST_KEY0X,
                       ST_KEY0Y,
                       _g->keys, NUMCARDS,
                       &_g->keyboxes[0]);

    STlib_initMultIcon(&_g->w_keyboxes[1],
                       ST_KEY1X,
                       ST_KEY1Y,
                       _g->keys, NUMCARDS,
                       &_g->keyboxes[1]);

    STlib_initMultIcon(&_g->w_keyboxes[2],
                       ST_KEY2X,
                       ST_KEY2Y,
                       _g->keys, NUMCARDS,
                       &_g->keyboxes[2]);

    // ammo count (all four kinds)
    STlib_initNum(&_g->w_ammo[0],
                  ST_AMMO0X,
                  ST_AMMO0Y,
                  _g->shortnum,
                  &_g->player.ammo[0],
                  ST_AMMO0WIDTH);

    STlib_initNum(&_g->w_ammo[1],
                  ST_AMMO1X,
                  ST_AMMO1Y,
                  _g->shortnum,
                  &_g->player.ammo[1],
                  ST_AMMO1WIDTH);

    STlib_initNum(&_g->w_ammo[2],
                  ST_AMMO2X,
                  ST_AMMO2Y,
                  _g->shortnum,
                  &_g->player.ammo[2],
                  ST_AMMO2WIDTH);

    STlib_initNum(&_g->w_ammo[3],
                  ST_AMMO3X,
                  ST_AMMO3Y,
                  _g->shortnum,
                  &_g->player.ammo[3],
                  ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    STlib_initNum(&_g->w_maxammo[0],
                  ST_MAXAMMO0X,
                  ST_MAXAMMO0Y,
                  _g->shortnum,
                  &_g->player.maxammo[0],
                  ST_MAXAMMO0WIDTH);

    STlib_initNum(&_g->w_maxammo[1],
                  ST_MAXAMMO1X,
                  ST_MAXAMMO1Y,
                  _g->shortnum,
                  &_g->player.maxammo[1],
                  ST_MAXAMMO1WIDTH);

    STlib_initNum(&_g->w_maxammo[2],
                  ST_MAXAMMO2X,
                  ST_MAXAMMO2Y,
                  _g->shortnum,
                  &_g->player.maxammo[2],
                  ST_MAXAMMO2WIDTH);

    STlib_initNum(&_g->w_maxammo[3],
                  ST_MAXAMMO3X,
                  ST_MAXAMMO3Y,
                  _g->shortnum,
                  &_g->player.maxammo[3],
                  ST_MAXAMMO3WIDTH);

    // faces
    STlib_initMultIcon(&_g->w_faces,
                       ST_FACESX,
                       ST_FACESY,
                       _g->faces, ST_NUMFACES,
                       &_g->st_faceindex);
}

static bool st_stopped = true;

void ST_Start(void)
{
    if (!st_stopped)
        ST_Stop();
    ST_initData();
    ST_createWidgets();
    st_stopped = false;
}

static void ST_Stop(void)
{
    if (st_stopped)
        return;
    V_SetPalette(0);
    st_stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}
