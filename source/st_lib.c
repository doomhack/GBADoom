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
 *      The status bar widget code.
 *
 *-----------------------------------------------------------------------------*/


#include "doomdef.h"
#include "v_video.h"
#include "st_stuff.h"
#include "st_lib.h"
#include "st_gfx.h"
#include "i_system_e32.h"
#include "global_data.h"

#include "gba_functions.h"

// The status bar is drawn at full GBA resolution: bytes per screen row.
#define ST_PITCH (SCREENWIDTH*2)

//
// STlib_growArea()
//
// Grows a widget's area to cover a patch drawn at x, y,
// clipped to the status bar.
//
static void STlib_growArea(st_area_t* a, int x, int y, const patch_t* patch)
{
    int x0 = (x - patch->leftoffset) & ~1;
    int y0 = y - patch->topoffset;
    int x1 = (x - patch->leftoffset + patch->width + 1) & ~1;
    int y1 = y0 + patch->height;

    if (a->w)
    {
        x0 = MIN(x0, a->x);
        y0 = MIN(y0, a->y);
        x1 = MAX(x1, a->x + a->w);
        y1 = MAX(y1, a->y + a->h);
    }

    a->x = MAX(x0, 0);
    a->y = MAX(y0, ST_Y);
    a->w = MIN(x1, ST_PITCH) - a->x;
    a->h = MIN(y1, SCREENHEIGHT) - a->y;
}

//
// STlib_copyArea()
//
// Copies an area into the back page from src, which is laid out in
// rows of ST_PITCH bytes starting at screen row firstrow.
//
static void STlib_copyArea(const st_area_t* a, const byte* src, int firstrow)
{
    byte* dest = (byte*)_g->screens[0].data + (ScreenYToOffset(a->y) << 1) + a->x;

    src += (a->y - firstrow) * ST_PITCH + a->x;

    for (int h = a->h; h; h--)
    {
        BlockCopy16(dest, src, a->w);
        dest += ST_PITCH;
        src += ST_PITCH;
    }
}

//
// STlib_needsDraw()
//
// Returns true if the widget must be drawn into the back page.
// A changed widget is erased and drawn here, then copied to
// the other page on the next frame.
//
static bool STlib_needsDraw(st_area_t* a, bool changed, bool refresh)
{
    if (changed || refresh)
    {
        if (!refresh)
            STlib_copyArea(a, gfx_stbar, ST_Y);

        a->pending = !refresh;
        return true;
    }

    if (a->pending)
    {
        // The Qt build draws into a single buffer.
#ifdef GBA
        STlib_copyArea(a, (const byte*)I_GetFrontBuffer(), 0);
#endif
        a->pending = false;
    }

    return false;
}

//
// STlib_initNum()
//
// Initializes an st_number_t widget
//
// Passed the widget, its position, the patches for the digits, a pointer
// to the value displayed, and the width
// Returns nothing
//
void STlib_initNum
(st_number_t* n,
 int x,
 int y,
 const patch_t **pl,
 const int* num,
 int     width )
{
    n->x  = x;
    n->y  = y;
    n->oldnum = 0;
    n->width  = width;
    n->num  = num;
    n->p  = pl;

    n->a.w = 0;
    n->a.pending = false;

    for (int i = 1; i <= width; i++)
        for (int d = 0; d < 10; d++)
            STlib_growArea(&n->a, x - i*pl[0]->width, y, pl[d]);
}

/*
 * STlib_drawNum()
 *
 * Draws a number right-justified at the widget's position.
 *
 * Passed a st_number_t widget and the number
 * Returns nothing
 */
static void STlib_drawNum(const st_number_t* n, int num)
{
    int   numdigits = n->width;

    int   w = n->p[0]->width;
    int   x = n->x;

    // CPhipps - compact some code, use num instead of *n->num
    if (num < 0)
    {
        if (numdigits == 2 && num < -9)
            num = -9;
        else if (numdigits == 3 && num < -99)
            num = -99;

        num = -num;
    }

    // if non-number, do not draw it
    if (num == 1994)
        return;

    // in the special case of 0, you draw 0
    if (!num)
        // CPhipps - patch drawing updated, reformatted
        V_DrawPatchNoScale(x - w, n->y, n->p[0]);

    // draw the new number
    while (num && numdigits--)
    {
        // CPhipps - patch drawing updated, reformatted
        x -= w;
        V_DrawPatchNoScale(x, n->y, n->p[num % 10]);
        num /= 10;
    }
}

/*
 * STlib_updateNum()
 *
 * Draws a number if it changed, or refresh is true
 *
 * Passed a number widget and a refresh flag
 * Returns nothing
 */
void STlib_updateNum(st_number_t* n, bool refresh)
{
    int num = *n->num;

    if (STlib_needsDraw(&n->a, num != n->oldnum, refresh))
    {
        STlib_drawNum(n, num);
        n->oldnum = num;
    }
}

//
// STlib_initMultIcon()
//
// Initialize a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys.
//
// Passed a st_multicon_t widget, the position, the graphic patches and
// how many there are, and a pointer to the number representing what to display
// Returns nothing.
//
void STlib_initMultIcon
(st_multicon_t* i,
 int x,
 int y,
 const patch_t **il,
 int count,
 const int* inum )
{
    i->x  = x;
    i->y  = y;
    i->oldinum  = -1;
    i->inum = inum;
    i->p  = il;

    i->a.w = 0;
    i->a.pending = false;

    for (int j = 0; j < count; j++)
        STlib_growArea(&i->a, x, y, il[j]);
}

//
// STlib_updateMultIcon()
//
// Draw a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys. Displays each when the control
// numbers change or refresh is true
//
// Passed a st_multicon_t widget, and a refresh flag
// Returns nothing.
//
void STlib_updateMultIcon (st_multicon_t* mi, bool refresh)
{
    int inum = *mi->inum;

    if (STlib_needsDraw(&mi->a, inum != mi->oldinum, refresh))
    {
        if (inum != -1)  // killough 2/16/98: draw only if != -1
            V_DrawPatchNoScale(mi->x, mi->y, mi->p[inum]);

        mi->oldinum = inum;
    }
}

static const st_area_t st_wholebar = {0, ST_Y, ST_PITCH, ST_HEIGHT, false};

void ST_refreshBackground(void)
{
    STlib_copyArea(&st_wholebar, gfx_stbar, ST_Y);
}

void ST_copyFromFront(void)
{
#ifdef GBA
    STlib_copyArea(&st_wholebar, (const byte*)I_GetFrontBuffer(), 0);
#endif
}
