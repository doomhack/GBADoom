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
 *  System interface for sound.
 *
 *-----------------------------------------------------------------------------
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"

#include "m_fixed.h"

#include "global_data.h"

#define __arm__

#ifdef __arm__

#include <gba.h>
#include <maxmod.h>    // Maxmod definitions for GBA

#include "soundbank.h"
#include "soundbank_bin.h"

#endif

// MWM 2000-01-08: Sample rate in samples/second
const int snd_samplerate=11250;


typedef struct music_map_t
{
    unsigned short doom_num;
    unsigned short mm_num;
}music_map_t;


//Mapping between the Doom music num and the maxmod music number.
static const music_map_t musicMap[NUMMUSIC] =
{
    {mus_None, 0},
    {mus_e1m1, MOD_D_E1M1},
    {mus_e1m2, MOD_D_E1M2},
    {mus_e1m3, MOD_D_E1M3},
    {mus_e1m4, MOD_D_E1M4},
    {mus_e1m5, MOD_D_E1M5},
    {mus_e1m6, MOD_D_E1M6},
    {mus_e1m7, MOD_D_E1M7},
    {mus_e1m8, MOD_D_E1M8},
    {mus_e1m9, MOD_D_E1M9},

    {mus_e2m1, MOD_D_E2M1},
    {mus_e2m2, MOD_D_E2M2},
    {mus_e2m3, MOD_D_E2M3},
    {mus_e2m4, MOD_D_E2M4},
    {mus_e2m5, MOD_D_E1M7},
    {mus_e2m6, MOD_D_E2M6},
    {mus_e2m7, MOD_D_E2M7},
    {mus_e2m8, MOD_D_E2M8},
    {mus_e2m9, MOD_D_E2M9},

    {mus_e3m1, MOD_D_E2M9},
    {mus_e3m2, MOD_D_E3M2},
    {mus_e3m3, MOD_D_E3M3},
    {mus_e3m4, MOD_D_E1M8},
    {mus_e3m5, MOD_D_E1M7},
    {mus_e3m6, MOD_D_E1M6},
    {mus_e3m7, MOD_D_E2M7},
    {mus_e3m8, MOD_D_E3M8},
    {mus_e3m9, MOD_D_E1M9},

    {mus_inter, MOD_D_E2M3},

    {mus_intro, MOD_D_INTRO},
    {mus_bunny, MOD_D_BUNNY},
    {mus_victor, MOD_D_VICTOR},
    {mus_introa, MOD_D_INTROA},
    {mus_runnin, MOD_D_RUNNIN},
    {mus_stalks, MOD_D_STALKS},
    {mus_countd, MOD_D_COUNTD},
    {mus_betwee, MOD_D_BETWEE},
    {mus_doom, MOD_D_DOOM},
    {mus_the_da, MOD_D_THE_DA},
    {mus_shawn, MOD_D_SHAWN},
    {mus_ddtblu, MOD_D_DDTBLU},
    {mus_in_cit, MOD_D_IN_CIT},
    {mus_dead, MOD_D_DEAD},
    {mus_stlks2, MOD_D_STALKS},
    {mus_theda2, MOD_D_STALKS},
    {mus_doom2, MOD_D_DOOM},
    {mus_runni2, MOD_D_RUNNIN},
    {mus_dead2, MOD_D_DEAD},
    {mus_stlks3, MOD_D_STALKS},
    {mus_romero, MOD_D_ROMERO},
    {mus_shawn2, MOD_D_SHAWN},
    {mus_messag, MOD_D_MESSAG},
    {mus_count2, MOD_D_COUNTD},
    {mus_ddtbl3, MOD_D_DDTBLU},
    {mus_ampie, MOD_D_AMPIE},
    {mus_theda3, MOD_D_THE_DA},
    {mus_adrian, MOD_D_ADRIAN},
    {mus_messg2, MOD_D_MESSAG},
    {mus_romer2, MOD_D_ROMERO},
    {mus_tense, MOD_D_TENSE},
    {mus_shawn3, MOD_D_SHAWN},
    {mus_openin, MOD_D_OPENIN},
    {mus_evil, MOD_D_EVIL},
    {mus_ultima, MOD_D_ULTIMA},
    {mus_read_m, MOD_D_READ_M},
    {mus_dm2ttl, MOD_D_DM2TTL},
    {mus_dm2int, MOD_D_DM2INT},
    {mus_openin, MOD_D_OPENIN},
};





/* cph
 * stopchan
 * Stops a sound, unlocks the data
 */

static void stopchan(int i)
{

}

//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
static int addsfx(int sfxid, int channel, const char* data, size_t len)
{	
	return channel;
}

static void updateSoundParams(int slot, int volume)
{

}

void I_UpdateSoundParams(int handle, int volume)
{
  updateSoundParams(handle, volume);
}

//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels(void)
{

}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(const sfxinfo_t *sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}



//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int I_StartSound(int id, int channel, int vol)
{
	if ((channel < 0) || (channel >= MAX_CHANNELS))
#ifdef RANGECHECK
		I_Error("I_StartSound: handle out of range");
#else
		return -1;
#endif

    int lump = _g->sfx_data[id].lumpnum;

    if(!_g->sfx_data[id].data)
	{
		// We will handle the new SFX.
		// Set pointer to raw data.
		unsigned int lumpLen = W_LumpLength(lump);

		// e6y: Crash with zero-length sounds.
		// Example wad: dakills (http://www.doomworld.com/idgames/index.php?id=2803)
		// The entries DSBSPWLK, DSBSPACT, DSSWTCHN and DSSWTCHX are all zero-length sounds
		if (lumpLen<=8) 
			return -1;

		/* Find padded length */
		//len -= 8;
		// do the lump caching outside the SDL_LockAudio/SDL_UnlockAudio pair
		// use locking which makes sure the sound data is in a malloced area and
		// not in a memory mapped one
		const unsigned char* wadData = (const unsigned char*)W_CacheLumpNum(lump);

        _g->sfx_data[id].data = (const void*)wadData;
	}


    const char* data = (const char*)_g->sfx_data[id].data;
    const unsigned int len = ((const unsigned int*)data)[1]; //No of samples is here.

	// Returns a handle (not used).
	addsfx(id, channel, data, len);
	updateSoundParams(channel, vol);

	return channel;
}



void I_StopSound (int handle)
{
#ifdef RANGECHECK
	if ((handle < 0) || (handle >= MAX_CHANNELS))
		I_Error("I_StopSound: handle out of range");
#endif

	stopchan(handle);
}


boolean I_SoundIsPlaying(int handle)
{

}


boolean I_AnySoundStillPlaying(void)
{
    return false;
}


//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the given
//  mixing buffer, and clamping it to the allowed
//  range.
//
// This function currently supports only 16bit.
//

static unsigned int I_UpdateSound(unsigned char *stream, const int len)
{

}

void I_ShutdownSound(void)
{
    if (_g->sound_inited)
	{
		lprintf(LO_INFO, "I_ShutdownSound: ");
		lprintf(LO_INFO, "\n");
        _g->sound_inited = false;
	}
}

//static SDL_AudioSpec audio;

void I_InitSound(void)
{

#ifdef __arm__
    mmInitDefault(soundbank_bin, 8);
#endif

	I_SetChannels();

	if (!nomusicparm)
		I_InitMusic();

	// Finished initialization.
	lprintf(LO_INFO,"I_InitSound: sound module ready\n");
}

//
// MUSIC API.


void I_ShutdownMusic(void)
{

}

void I_InitMusic(void)
{
}

void I_UpdateMusic()
{

}

void I_PlaySong(int handle, int looping)
{
    if(handle == mus_None)
        return;

#ifdef __arm__
    mm_pmode mode = looping ? MM_PLAY_LOOP : MM_PLAY_ONCE;

    unsigned int song = musicMap[handle].mm_num;

    mmStart(song, mode);
#endif
}


void I_PauseSong (int handle)
{
    mmPause();
}

void I_ResumeSong (int handle)
{
    mmResume();
}

void I_StopSong(int handle)
{
    mmStop();
}

void I_UnRegisterSong(int handle)
{

}

int I_RegisterSong(const void *data, size_t len)
{
	return 0;
}

void I_SetMusicVolume(int volume)
{
    mmSetModuleVolume(volume * 8);
}
