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

#include "mmus2mid.h"
//#include "libtimidity/timidity.h"
#include "m_fixed.h"

#include "global_data.h"

typedef struct
{
    int freq;
    unsigned char channels;
    unsigned short samples;
    unsigned int size;
    unsigned int (*callback)(unsigned char *stream, const int len);
} AudioSpec;

// Needed for calling the actual sound output.
static const unsigned int SAMPLECOUNT	= 1024;

// MWM 2000-01-08: Sample rate in samples/second
const int snd_samplerate=11250;

static const unsigned int MUSIC_CHANNEL = MAX_CHANNELS;

void flipSongBuffer();

/* cph
 * stopchan
 * Stops a sound, unlocks the data
 */

static void stopchan(int i)
{
    _g->channelinfo[i].data=NULL;
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
	stopchan(channel);
	
    _g->channelinfo[channel].data = data + 8;

	/* Set pointer to end of raw data. */
    _g->channelinfo[channel].enddata = data + len - 1;
	
	return channel;
}

static void updateSoundParams(int slot, int volume)
{

#ifdef RANGECHECK
	if ((handle < 0) || (handle >= MAX_CHANNELS))
		I_Error("I_UpdateSoundParams: handle out of range");
#endif

    if (volume < 0 || volume > 127)
		I_Error("volume out of bounds");

    _g->channelinfo[slot].vol = volume;
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
  // Okay, reset internal mixing channels to zero.
  for (unsigned int i=0; i<MAX_CHANNELS + 1; i++)
  {
      memset(&_g->channelinfo[i],0,sizeof(channel_info_t));
  }
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
#ifdef RANGECHECK
	if ((handle < 0) || (handle >= MAX_CHANNELS))
		I_Error("I_SoundIsPlaying: handle out of range");
#endif
    return _g->channelinfo[handle].data != NULL;
}


boolean I_AnySoundStillPlaying(void)
{
	boolean result = false;
	int i;
	
	for (i=0; i<MAX_CHANNELS; i++)
        result |= (_g->channelinfo[i].data != NULL);

	return result;
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
	const unsigned int* streamEnd = (unsigned int*)(stream + len);

	unsigned int* streamPos = (unsigned int*)stream;

	unsigned int channelPack = 0;
	unsigned int i = MAX_CHANNELS;

	do
	{
        if(_g->channelinfo[i-1].data)
		{
			channelPack <<= 4;
			channelPack |= i;
		}
	} while(--i);


    if( (!channelPack) && (!_g->channelinfo[MUSIC_CHANNEL].data) )
		return 0;

	do
    {
		int outSample1 = 0;
		int outSample2 = 0;
		int outSample3 = 0;
		int outSample4 = 0;
		
		unsigned int channelPack2 = channelPack;


		while(channelPack2)
		{
			const unsigned int chan = ((channelPack2 & 0xf) - 1);
			channelPack2 >>= 4;

            channel_info_t* channel = &_g->channelinfo[chan];

			if(channel->data == NULL)
				continue;

			const int volume = channel->vol;

			const int inSample03 = *((int*)(channel->data));
					
			outSample1 += (((inSample03 << 24)	>> 24) * volume);
			outSample2 += (((inSample03 << 16)	>> 24) * volume);
			outSample3 += (((inSample03 << 8)	>> 24) * volume);
			outSample4 += (( inSample03 >> 24)		   * volume);

			channel->data += 4;

			// Check whether we are done.
			if (channel->data > channel->enddata)
				stopchan(chan);
		}

        channel_info_t* channel = &_g->channelinfo[MUSIC_CHANNEL];
		// Check channel, if active.
		if(channel->data)
		{
			const int inSample01 = *((int*)(channel->data));
			const int inSample23 = *((int*)(channel->data+4));

			outSample1 += ((inSample01 << 16)	>> 16);
			outSample2 += ( inSample01 >> 16);
			outSample3 += ((inSample23 << 16)	>> 16);
			outSample4 += ( inSample23 >> 16);

			channel->data += 8;

			// Check whether we are done.
			if (channel->data > channel->enddata)
			{
				stopchan(MUSIC_CHANNEL);

				flipSongBuffer();
			}
		}
		
        /*
         * TODO: Write stream to out buff.
         */


	} while (streamPos++ < streamEnd);

	return 1;
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
	I_SetChannels();

    AudioSpec audio;

	// Secure and configure sound device first.
	lprintf(LO_INFO,"I_InitSound: ");

	// Open the audio device
    audio.freq = snd_samplerate;

    audio.channels = 1;
    audio.samples = (unsigned short)SAMPLECOUNT;
    audio.callback = I_UpdateSound;

    /*
	if ( OpenAudio(audio) < 0 )
	{
		lprintf(LO_INFO,"couldn't open audio with desired format\n");
		return;
	}
    */
	
	lprintf(LO_INFO," configured audio device with %d samples/slice\n", SAMPLECOUNT);


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
/*
    _g->music_init = (mid_init ("D:\\Doom\\dgguspat\\timidity.cfg") >= 0);

    _g->music_buffer = (short*)malloc(MUSIC_BUFFER_SAMPLES * 2 * 2);

    if(!_g->music_init)
		lprintf(LO_INFO,"I_InitMusic: mid_init failed.\n");
	else
		lprintf(LO_INFO,"I_InitMusic: mid_init done.\n");
        */
}


void playSongBuffer(unsigned int buffNum)
{
    /*
    if(!_g->music_init)
		return;

	stopchan(MUSIC_CHANNEL);

	//This stops buffer underruns from killing the song.
    if(!_g->music_sample_counts[buffNum])
	{
        _g->music_sample_counts[buffNum] = MUSIC_BUFFER_SAMPLES;
		//return;
	}

	unsigned int buffOffset = (buffNum * MUSIC_BUFFER_SAMPLES);

    _g->channelinfo[MUSIC_CHANNEL].vol = 0;


    _g->channelinfo[MUSIC_CHANNEL].enddata = (char*)(_g->music_buffer + buffOffset + (_g->music_sample_counts[buffNum] - 1));

    _g->channelinfo[MUSIC_CHANNEL].data = (char*)(&_g->music_buffer[buffOffset]);
    */
}

void flipSongBuffer()
{	
    /*

    if(!_g->music_init)
		return;

    _g->music_sample_counts[_g->current_music_buffer] = 0;

    _g->current_music_buffer = 1 - _g->current_music_buffer;

    playSongBuffer(_g->current_music_buffer);
    */
}

void I_UpdateMusic()
{
    /*
    if(!_g->midiSong || !_g->music_init)
		return;

    unsigned int next_music_buffer = 1 - _g->current_music_buffer;

    if(_g->music_sample_counts[next_music_buffer] == 0)
	{
		unsigned int buffOffset = (next_music_buffer * MUSIC_BUFFER_SAMPLES);


        _g->music_sample_counts[next_music_buffer] = (mid_song_read_wave (_g->midiSong, (signed char*)(&_g->music_buffer[buffOffset]), MUSIC_BUFFER_SAMPLES * 2) >> 1);

        if( (_g->music_sample_counts[next_music_buffer] == 0) && _g->music_looping)
		{
			//Back to start. (Looping)
            mid_song_start(_g->midiSong);

            _g->music_sample_counts[next_music_buffer] = (mid_song_read_wave (_g->midiSong, (signed char*)(&_g->music_buffer[buffOffset]), MUSIC_BUFFER_SAMPLES * 2) >> 1);
		}
	}
    */
}

void I_PlaySong(int handle, int looping)
{
    /*
    if(!_g->midiSong || !_g->music_init)
		return;

    _g->music_looping = looping;

    _g->current_music_buffer = 0;

    _g->music_sample_counts[0] = (mid_song_read_wave (_g->midiSong, (signed char*)(&_g->music_buffer[0]), MUSIC_BUFFER_SAMPLES * 2) >> 1);
    _g->music_sample_counts[1] = (mid_song_read_wave (_g->midiSong, (signed char*)(&_g->music_buffer[MUSIC_BUFFER_SAMPLES]), MUSIC_BUFFER_SAMPLES * 2) >> 1);

	playSongBuffer(0);
    */
}


void I_PauseSong (int handle)
{
    /*
	stopchan(MUSIC_CHANNEL);
    */
}

void I_ResumeSong (int handle)
{
    /*
    playSongBuffer(_g->current_music_buffer);
    */
}

void I_StopSong(int handle)
{
    /*
    if(_g->midiSong)
	{
        mid_song_seek (_g->midiSong, 0);
	}

	stopchan(MUSIC_CHANNEL);
    */
}

void I_UnRegisterSong(int handle)
{
    /*
	I_StopSong(0);
	
    if(_g->midiSong)
        mid_song_free(_g->midiSong);

    if(_g->midiStream)
        mid_istream_close(_g->midiStream);

    _g->midiSong = NULL;
    _g->midiStream = NULL;
    */
}

int I_RegisterSong(const void *data, size_t len)
{
    /*
    if(!_g->music_init)
		return 0;

	MIDI *mididata = NULL;


	if ( len < 32 )
		return 0; // the data should at least as big as the MUS header


	if ( memcmp(data, "MUS", 3) == 0 )
	{
		UBYTE *mid = NULL;
		int midlen = 0;

		mididata = (MIDI*)malloc(sizeof(MIDI));
		mmus2mid((const unsigned char*)data, mididata, 89, 0);
		MIDIToMidi(mididata,&mid,&midlen);
		
        _g->midiStream = mid_istream_open_mem (mid, midlen);

        if(_g->midiStream)
		{
			MidSongOptions options;
			
			options.rate = snd_samplerate;
			options.format = MID_AUDIO_S16LSB;
			options.channels = 1;
			options.buffer_size = 65535;

            _g->midiSong = mid_song_load (_g->midiStream, &options);
			
            if(_g->midiSong)
			{
                mid_song_set_volume (_g->midiSong, _g->music_volume);

                mid_song_start (_g->midiSong);
			}
			else
			{
				lprintf(LO_INFO,"I_RegisterSong: mid_song_load returned NULL\n");
			}
		}
		else
		{
			lprintf(LO_INFO,"I_RegisterSong: mid_istream_open_mem returned NULL\n");
		}

		free(mid);
		free_mididata(mididata);
		free(mididata);
	}
*/
	return 0;
}

// cournia - try to load a music file into SDL_Mixer
//           returns true if could not load the file
int I_RegisterMusic( const char* filename, musicinfo_t *song )
{
    /*
	if (!filename)
		return 1;
	
	if (!song)
		return 1;
    */
	return 0;

}

void I_SetMusicVolume(int volume)
{
    /*
    _g->music_volume = (volume * 4);

    if(_g->midiSong)
	{
        mid_song_set_volume (_g->midiSong, _g->music_volume);
	}
    */
}
