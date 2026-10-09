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
 *  System interface for sound: the libtimidity mixer (s_mix.c) played
 *  through Direct Sound A on the GBA, or waveOut in the Qt build.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <ctype.h>

#include "doomdef.h"
#include "doomstat.h"
#include "w_wad.h"
#include "i_sound.h"
#include "s_sound.h"
#include "sounds.h"
#include "lprintf.h"
#include "global_data.h"
#include "s_mix.h"

#ifdef GBA
    #include <gba.h>
#else
    // waveOut, in i_audio_win.c. windows.h and Doom's headers don't mix.
    void I_StartAudioDevice(void);
#endif

// Music lump names (D_xxxxxx) by music number. Lump lookups are case sensitive.
static const char* const musicNames[NUMMUSIC] =
{
    NULL,
    "E1M1", "E1M2", "E1M3", "E1M4", "E1M5", "E1M6", "E1M7", "E1M8", "E1M9",
    "E2M1", "E2M2", "E2M3", "E2M4", "E2M5", "E2M6", "E2M7", "E2M8", "E2M9",
    "E3M1", "E3M2", "E3M3", "E3M4", "E3M5", "E3M6", "E3M7", "E3M8", "E3M9",
    "INTER", "INTRO", "BUNNY", "VICTOR", "INTROA", "RUNNIN", "STALKS", "COUNTD",
    "BETWEE", "DOOM", "THE_DA", "SHAWN", "DDTBLU", "IN_CIT", "DEAD", "STLKS2",
    "THEDA2", "DOOM2", "DDTBL2", "RUNNI2", "DEAD2", "STLKS3", "ROMERO", "SHAWN2",
    "MESSAG", "COUNT2", "DDTBL3", "AMPIE", "THEDA3", "ADRIAN", "MESSG2", "ROMER2",
    "TENSE", "SHAWN3", "OPENIN", "EVIL", "ULTIMA", "READ_M", "DM2TTL", "DM2INT"
};

//
// The mixer runs in the VBlank interrupt on the GBA. Mask interrupts while
// changing what it plays.
//
static unsigned int I_SoundLock(void)
{
#ifdef GBA
    unsigned int ime = REG_IME;
    REG_IME = 0;
    return ime;
#else
    return 0;
#endif
}

static void I_SoundUnlock(unsigned int ime)
{
#ifdef GBA
    REG_IME = ime;
#else
    (void)ime;
#endif
}

#ifdef GBA

#define REG_FIFO_A_ADDR 0x040000A0

// Timer 0 period for SND_MIX_RATE. 1254 * SND_MIX_SAMPLES = 280896 cycles = one frame.
#define SND_TIMER_PERIOD 1254

//
// Restart DMA on the buffer mixed last frame, then mix the other one.
// Timer 0 consumes exactly SND_MIX_SAMPLES per frame, so they stay in step.
//
static void I_SoundVBlank(void)
{
    REG_DMA1CNT = 0;
    REG_DMA1SAD = (u32)_g->mix_out[_g->mix_out_index];
    REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | DMA_SPECIAL | DMA_ENABLE;

    _g->mix_out_index ^= 1;

    S_MixFrame((int8_t*)_g->mix_out[_g->mix_out_index]);
}

static void I_StartAudioDevice(void)
{
    SNDSTAT = SNDSTAT_ENABLE;
    DMGSNDCTRL = 0;
    DSOUNDCTRL = DSOUNDCTRL_A100 | DSOUNDCTRL_AR | DSOUNDCTRL_AL | DSOUNDCTRL_ATIMER(0) | DSOUNDCTRL_ARESET;

    REG_TM0CNT_H = 0;
    REG_TM0CNT_L = 65536 - SND_TIMER_PERIOD;
    REG_TM0CNT_H = TIMER_START;

    REG_DMA1CNT = 0;
    REG_DMA1SAD = (u32)_g->mix_out[0];
    REG_DMA1DAD = REG_FIFO_A_ADDR;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | DMA_SPECIAL | DMA_ENABLE;

    _g->mix_out_index = 0;

    irqSet(IRQ_VBLANK, I_SoundVBlank);
    irqEnable(IRQ_VBLANK);
}

#endif

void I_InitSound(void)
{
    S_MixInit();

    // Find the sound lumps up front: W_CheckNumForName is a linear search.
    for(int i = 0; i < NUMSFX; i++)
    {
        const sfxinfo_t* sfx = &S_sfx[i];

        if(sfx->link)
            sfx = sfx->link;

        char name[9];
        lsnprintf(name, sizeof(name), "DS%s", sfx->name);

        for(char* c = name; *c; c++)
            *c = toupper(*c);

        _g->sfx_lumps[i] = (short)((i > 0) ? W_CheckNumForName(name) : -1);
    }

    I_StartAudioDevice();

    lprintf("I_InitSound: sound ready");
}

//
// Starting a sound replaces whatever was playing on that channel.
//
int I_StartSound(int id, int channel, int vol, int sep)
{
    (void)sep; // Mono.

    if((channel < 0) || (channel >= MAX_CHANNELS) || (id < 0) || (id >= NUMSFX))
        return -1;

    const int lump = _g->sfx_lumps[id];

    if(lump < 0)
        return -1;

    const void* data = W_CacheLumpNum(lump);

    unsigned int ime = I_SoundLock();
    S_MixStartSfx(channel, data, vol);
    I_SoundUnlock(ime);

    return channel;
}

void I_PlaySong(int handle, int looping)
{
    if(handle <= mus_None || handle >= NUMMUSIC)
        return;

    char name[9];
    lsnprintf(name, sizeof(name), "D_%s", musicNames[handle]);

    const int lump = W_CheckNumForName(name);

    if(lump < 0)
        return;

    const void* data = W_CacheLumpNum(lump);
    const unsigned int size = W_LumpLength(lump);

    unsigned int ime = I_SoundLock();
    S_MixStartMusic(data, size, looping);
    I_SoundUnlock(ime);
}

void I_PauseSong (int handle)
{
    (void)handle;

    unsigned int ime = I_SoundLock();
    S_MixPauseMusic(true);
    I_SoundUnlock(ime);
}

void I_ResumeSong (int handle)
{
    (void)handle;

    unsigned int ime = I_SoundLock();
    S_MixPauseMusic(false);
    I_SoundUnlock(ime);
}

void I_StopSong(int handle)
{
    (void)handle;

    unsigned int ime = I_SoundLock();
    S_MixStopMusic();
    I_SoundUnlock(ime);
}

void I_SetMusicVolume(int volume)
{
    unsigned int ime = I_SoundLock();
    S_MixSetMusicVolume(volume);
    I_SoundUnlock(ime);
}

