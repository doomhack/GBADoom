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

extern "C"
{
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
	#include "m_argv.h"
	#include "m_misc.h"
	#include "w_wad.h"
	#include "lprintf.h"
	#include "s_sound.h"

	#include "doomdef.h"
	#include "doomstat.h"
	#include "doomtype.h"

	#include "d_main.h"

	#include "mmus2mid.h"
    #include "libtimidity/timidity.h"
	#include "m_fixed.h"

	#include "pcm_to_alaw.h"
}

#include "audio.h"


// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

static boolean sound_inited = false;
static boolean first_sound_init = true;

// Needed for calling the actual sound output.
static const unsigned int SAMPLECOUNT	= 1024;
#define MAX_CHANNELS    8

// MWM 2000-01-08: Sample rate in samples/second
const int snd_samplerate=8000;


#define MUSIC_BUFFER_SAMPLES 2048
static short music_buffer[MUSIC_BUFFER_SAMPLES * 2];
static unsigned int current_music_buffer = 0;

static unsigned int music_looping = 0;
static unsigned int music_volume = 0;
static unsigned int music_init = 0;

static unsigned int music_sample_counts[2];

static const unsigned int MUSIC_CHANNEL = MAX_CHANNELS;

void flipSongBuffer();


typedef struct 
{
	const char* data;
	const char* enddata;
	int vol;

} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS + 1];

//Resample U8 PCM to new sample rate.
char* resamplePcm(const unsigned char* inPcm, unsigned int inLen, unsigned short outFreq)
{
	const unsigned short inFreq = ((unsigned short*)inPcm)[1];

	if(inLen < 40)
		return NULL;

	const unsigned int inSamples = (inLen - 40);
	
	const double ratio = (double)outFreq / (double)inFreq;
	const double step = ((double)inFreq / (double)outFreq);

	const unsigned short outSamples = (unsigned short)ceil(ratio * inSamples);

	//Round alloc up to next multiple of 4 so we can unwind the mixer.
	const unsigned short allocLen = (unsigned short)((outSamples + 8 + 3) & 0xfffc);

	char* outData = (char*)Z_Malloc(allocLen, PU_STATIC, 0);
	
	//Fill padding with 0.
	*((unsigned int*)(&outData[allocLen-4])) = 0;

	//Header info. https://doomwiki.org/wiki/Sound
	((unsigned short*)outData)[0] = 3; //Type. Must be 3.
	((unsigned short*)outData)[1] = outFreq; //Freq.
	((unsigned int*)outData)[1] = outSamples; //Sample count.


	const fixed_t fStep = (fixed_t)(step * FRACUNIT);
	fixed_t fPos = 0;

	for(unsigned int i = 0; i < outSamples; i++)
	{
		unsigned int srcPos = (fPos >> FRACBITS);

		fPos += fStep;

		if(srcPos >= inSamples)
			srcPos = inSamples-1;

		int sample = inPcm[srcPos + 24];

		outData[i+8] = (char)(sample-128);
	}

	return outData;
}


/* cph
 * stopchan
 * Stops a sound, unlocks the data
 */

static void stopchan(int i)
{
	channelinfo[i].data=NULL;
}

//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
static int addsfx(int /*sfxid*/, int channel, const char* data, size_t len)
{
	stopchan(channel);
	
	channelinfo[channel].data = data + 8;

	/* Set pointer to end of raw data. */
	channelinfo[channel].enddata = data + len - 1;
	
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

	channelinfo[slot].vol = volume;
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
	  memset(&channelinfo[i],0,sizeof(channel_info_t));
  }
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
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

	int lump = S_sfx[id].lumpnum;

	if(!S_sfx[id].data)
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

		S_sfx[id].data = resamplePcm(wadData, lumpLen, (unsigned short)snd_samplerate);
	}


	const char* data = (const char*)S_sfx[id].data;
	const unsigned int len = ((unsigned int*)data)[1]; //No of samples is here.

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
	return channelinfo[handle].data != NULL;
}


boolean I_AnySoundStillPlaying(void)
{
	boolean result = false;
	int i;
	
	for (i=0; i<MAX_CHANNELS; i++)
		result |= (channelinfo[i].data != NULL);

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
		if(channelinfo[i-1].data)
		{
			channelPack <<= 4;
			channelPack |= i;
		}
	} while(--i);


	if( (!channelPack) && (!channelinfo[MUSIC_CHANNEL].data) )
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

			channel_info_t* channel = &channelinfo[chan];

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

		channel_info_t* channel = &channelinfo[MUSIC_CHANNEL];
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
		
		outSample1 = ((outSample1 >> 4) + 2048) & 4095;
		outSample2 = ((outSample2 >> 4) + 2048) & 4095;
		outSample3 = ((outSample3 >> 4) + 2048) & 4095;
		outSample4 = ((outSample4 >> 4) + 2048) & 4095;

		const unsigned int alaw1 = pcm_to_alaw2[outSample1];
		const unsigned int alaw2 = pcm_to_alaw2[outSample2];
		const unsigned int alaw3 = pcm_to_alaw2[outSample3];
		const unsigned int alaw4 = pcm_to_alaw2[outSample4];

		*streamPos = alaw1 | (alaw2 << 8) | (alaw3 << 16) | (alaw4 << 24);

	} while (streamPos++ < streamEnd);

	return 1;
}

void I_ShutdownSound(void)
{
	if (sound_inited) 
	{
		lprintf(LO_INFO, "I_ShutdownSound: ");
		lprintf(LO_INFO, "\n");
		sound_inited = false;

		StopAudio();
	}
}

//static SDL_AudioSpec audio;

void I_InitSound(void)
{
	I_SetChannels();

	AudioSpec* audio = new AudioSpec;

	// Secure and configure sound device first.
	lprintf(LO_INFO,"I_InitSound: ");

	// Open the audio device
	audio->freq = snd_samplerate;

	audio->channels = 1;
	audio->samples = (unsigned short)SAMPLECOUNT;
	audio->callback = I_UpdateSound;
	
	if ( OpenAudio(audio) < 0 )
	{
		lprintf(LO_INFO,"couldn't open audio with desired format\n");
		return;
	}
	
	lprintf(LO_INFO," configured audio device with %d samples/slice\n", SAMPLECOUNT);


	if (first_sound_init)
	{
		atexit(I_ShutdownSound);
		first_sound_init = false;
	}

	if (!nomusicparm)
		I_InitMusic();

	// Finished initialization.
	lprintf(LO_INFO,"I_InitSound: sound module ready\n");
}




//
// MUSIC API.

MidIStream* midiStream = NULL;
MidSong* midiSong = NULL;



void I_ShutdownMusic(void)
{

}

void I_InitMusic(void)
{
#ifdef __WINS__
	music_init  = (mid_init ("C:\\Doom\\dgguspat\\timidity.cfg") >= 0);
#else
	music_init = (mid_init ("D:\\Doom\\dgguspat\\timidity.cfg") >= 0);
#endif

	if(!music_init)
		lprintf(LO_INFO,"I_InitMusic: mid_init failed.\n");
	else
		lprintf(LO_INFO,"I_InitMusic: mid_init done.\n");
}


void playSongBuffer(unsigned int buffNum)
{
	if(!music_init)
		return;

	stopchan(MUSIC_CHANNEL);

	//This stops buffer underruns from killing the song.
	if(!music_sample_counts[buffNum])
	{
		music_sample_counts[buffNum] = MUSIC_BUFFER_SAMPLES;
		//return;
	}

	unsigned int buffOffset = (buffNum * MUSIC_BUFFER_SAMPLES);

	channelinfo[MUSIC_CHANNEL].vol = 0;

	/* Set pointer to end of raw data. */
	channelinfo[MUSIC_CHANNEL].enddata = (char*)(music_buffer + buffOffset + (music_sample_counts[buffNum] - 1));

	channelinfo[MUSIC_CHANNEL].data = (char*)(&music_buffer[buffOffset]);
}

void flipSongBuffer()
{	
	if(!music_init)
		return;

	music_sample_counts[current_music_buffer] = 0;

	current_music_buffer = 1 - current_music_buffer;

	playSongBuffer(current_music_buffer);
}

void I_UpdateMusic()
{
	if(!midiSong || !music_init)
		return;

	unsigned int next_music_buffer = 1 - current_music_buffer;

	if(music_sample_counts[next_music_buffer] == 0)
	{
		unsigned int buffOffset = (next_music_buffer * MUSIC_BUFFER_SAMPLES);


		music_sample_counts[next_music_buffer] = (mid_song_read_wave (midiSong, (signed char*)(&music_buffer[buffOffset]), MUSIC_BUFFER_SAMPLES * 2) >> 1);

		if( (music_sample_counts[next_music_buffer] == 0) && music_looping)
		{
			//Back to start. (Looping)
			mid_song_start(midiSong);

			music_sample_counts[next_music_buffer] = (mid_song_read_wave (midiSong, (signed char*)(&music_buffer[buffOffset]), MUSIC_BUFFER_SAMPLES * 2) >> 1);
		}
	}
}

void I_PlaySong(int /*handle*/, int looping)
{
	if(!midiSong || !music_init)
		return;

	music_looping = looping;

	current_music_buffer = 0;

	music_sample_counts[0] = (mid_song_read_wave (midiSong, (signed char*)(&music_buffer[0]), MUSIC_BUFFER_SAMPLES * 2) >> 1);
	music_sample_counts[1] = (mid_song_read_wave (midiSong, (signed char*)(&music_buffer[MUSIC_BUFFER_SAMPLES]), MUSIC_BUFFER_SAMPLES * 2) >> 1);

	playSongBuffer(0);
}


void I_PauseSong (int /*handle*/)
{
	stopchan(MUSIC_CHANNEL);
}

void I_ResumeSong (int /*handle*/)
{
	playSongBuffer(current_music_buffer);
}

void I_StopSong(int /*handle*/)
{
	if(midiSong)
	{
		mid_song_seek (midiSong, 0);
	}

	stopchan(MUSIC_CHANNEL);
}

void I_UnRegisterSong(int /*handle*/)
{
	I_StopSong(0);
	
	if(midiSong)
		mid_song_free(midiSong);

	if(midiStream)
		mid_istream_close(midiStream);

	midiSong = NULL;
	midiStream = NULL;
}

int I_RegisterSong(const void *data, size_t len)
{
	if(!music_init)
		return 0;

	MIDI *mididata = NULL;


	if ( len < 32 )
		return 0; // the data should at least as big as the MUS header

	/* Convert MUS chunk to MIDI? */
	if ( memcmp(data, "MUS", 3) == 0 )
	{
		UBYTE *mid = NULL;
		int midlen = 0;

		mididata = (MIDI*)malloc(sizeof(MIDI));
		mmus2mid((const unsigned char*)data, mididata, 89, 0);
		MIDIToMidi(mididata,&mid,&midlen);
		
		midiStream = mid_istream_open_mem (mid, midlen);

		if(midiStream)
		{
			MidSongOptions options;
			
			options.rate = snd_samplerate;
			options.format = MID_AUDIO_S16LSB;
			options.channels = 1;
			options.buffer_size = 65535;

			midiSong = mid_song_load (midiStream, &options);
			
			if(midiSong)
			{
				mid_song_set_volume (midiSong, music_volume);

				mid_song_start (midiSong);
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

	return 0;
}

// cournia - try to load a music file into SDL_Mixer
//           returns true if could not load the file
int I_RegisterMusic( const char* filename, musicinfo_t *song )
{
	if (!filename)
		return 1;
	
	if (!song)
		return 1;
	
	song->data = 0;
	song->handle = 0;
	song->lumpnum = 0;

	return 0;

}

void I_SetMusicVolume(int volume)
{
	music_volume = (volume * 4);

	if(midiSong)
	{
		mid_song_set_volume (midiSong, music_volume);
	}

}
