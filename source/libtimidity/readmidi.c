/* libTiMidity is licensed under the terms of the GNU Lesser General
 * Public License: see COPYING for details.
 *
 * Note that the included TiMidity source, based on timidity-0.2i, was
 * originally licensed under the GPL, but the author extended it so it
 * can also be used separately under the GNU LGPL or the Perl Artistic
 * License: see the notice by Tuukka Toivonen as it appears on the web
 * at http://ieee.uwaterloo.ca/sca/www.cgs.fi/tt/timidity/ .
 */

/*
 * TiMidity -- Experimental MIDI to WAVE converter
 * Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * GBADoom: the largest Doom songs have ~20,000 events, too many to hold in
 * RAM as an event list. Instead a type 0 MIDI file (GbaWadUtil converts MUS)
 * is read from ROM one event at a time, doing what read_midi_event() and
 * groom_list() used to do as each event is needed.
 */

#include <string.h>

#include "timidity_internal.h"
#include "readmidi.h"
#include "playmidi.h"

/* Computes how many (fractional) samples one MIDI delta-time unit contains */
static void compute_sample_increment(MidSong *song, sint32 tempo)
{
  sint32 a = (sint32)(((long long)tempo * GUSBANK_RATE * 65536) /
		      (1000000LL * song->divisions));

  song->sample_correction = a & 0xFFFF;
  song->sample_increment = a >> 16;
}

/* Read variable-length number (7 bits per byte, MSB first) */
static sint32 getvl(MidSong *song)
{
  sint32 l=0;
  uint8 c;
  for (;;)
    {
      if (song->midi_pos >= song->midi_end) return l;
      c = *song->midi_pos++;
      l += (c & 0x7f);
      if (!(c & 0x80)) return l;
      l<<=7;
    }
}

static uint8 getbyte(MidSong *song)
{
  if (song->midi_pos >= song->midi_end)
    return 0;
  return *song->midi_pos++;
}

static void skip(MidSong *song, sint32 len)
{
  if (len > song->midi_end - song->midi_pos)
    song->midi_pos = song->midi_end;
  else
    song->midi_pos += len;
}

static uint32 getbe32(const uint8 *p)
{
  return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}

/* Read a MIDI event into song->event, with its delta time in MIDI ticks in
   *dt. Returns 0 at the end of the track. */
static int read_midi_event(MidSong *song, sint32 *dt)
{
  uint8 me, type, a,b,c;
  sint32 len;
  MidEvent *e = &song->event;

  *dt = 0;

  for (;;)
    {
      if (song->midi_pos >= song->midi_end)
	return 0;

      *dt += getvl(song);
      me = getbyte(song);

      if(me==0xF0 || me == 0xF7) /* SysEx event */
	{
	  len=getvl(song);
	  skip(song, len);
	}
      else if(me==0xFF) /* Meta event */
	{
	  type = getbyte(song);
	  len=getvl(song);
	  switch(type)
	    {
	    case 0x2F: /* End of Track */
	      return 0;

	    case 0x51: /* Tempo */
	      a = getbyte(song);
	      b = getbyte(song);
	      c = getbyte(song);
	      skip(song, len - 3);
	      e->type = ME_TEMPO; e->channel = c; e->a = a; e->b = b;
	      return 1;

	    default:
	      skip(song, len);
	      break;
	    }
	}
      else
	{
	  a=me;
	  if (a & 0x80) /* status byte */
	    {
	      song->lastchan=a & 0x0F;
	      song->laststatus=(a>>4) & 0x07;
	      a = getbyte(song);
	    }
	  a &= 0x7F;
	  e->channel = song->lastchan;
	  e->a = a;
	  e->b = 0;
	  switch(song->laststatus)
	    {
	    case 0: /* Note off */
	      e->b = getbyte(song) & 0x7F;
	      e->type = ME_NOTEOFF;
	      return 1;

	    case 1: /* Note on */
	      e->b = getbyte(song) & 0x7F;
	      e->type = ME_NOTEON;
	      return 1;

	    case 2: /* Key Pressure */
	      e->b = getbyte(song) & 0x7F;
	      e->type = ME_KEYPRESSURE;
	      return 1;

	    case 3: /* Control change */
	      b = getbyte(song) & 0x7F;
	      {
		int control=255;
		uint8 ch = song->lastchan;
		switch(a)
		  {
		  case 7: control=ME_MAINVOLUME; break;
		  case 10: control=ME_PAN; break;
		  case 11: control=ME_EXPRESSION; break;
		  case 64: control=ME_SUSTAIN; b = (b >= 64); break;
		  case 120: control=ME_ALL_SOUNDS_OFF; break;
		  case 121: control=ME_RESET_CONTROLLERS; break;
		  case 123: control=ME_ALL_NOTES_OFF; break;

		  /* These should be the SCC-1 tone bank switch
		     commands. */
		  case 0: control=ME_TONE_BANK; break;

		  case 100: song->nrpn=0; song->rpn_msb[ch]=b; break;
		  case 101: song->nrpn=0; song->rpn_lsb[ch]=b; break;
		  case 99: song->nrpn=1; song->rpn_msb[ch]=b; break;
		  case 98: song->nrpn=1; song->rpn_lsb[ch]=b; break;

		  case 6:
		    if (song->nrpn)
		      break;

		    switch((song->rpn_msb[ch]<<8) | song->rpn_lsb[ch])
		      {
		      case 0x0000: /* Pitch bend sensitivity */
			control=ME_PITCH_SENS;
			break;

		      case 0x7F7F: /* RPN reset */
			/* reset pitch bend sensitivity to 2 */
			control=ME_PITCH_SENS;
			b=2;
			break;

		      default:
			break;
		      }
		    break;

		  default:
		    break;
		  }
		if (control != 255)
		  {
		    e->type = control;
		    e->a = b;
		    return 1;
		  }
	      }
	      break;

	    case 4: /* Program change */
	      e->type = ME_PROGRAM;
	      return 1;

	    case 5: /* Channel pressure - NOT IMPLEMENTED */
	      break;

	    case 6: /* Pitch wheel */
	      e->b = getbyte(song) & 0x7F;
	      e->type = ME_PITCHWHEEL;
	      return 1;

	    default:
	      break;
	    }
	}
    }
}

int midi_stream_open(MidSong *song, const uint8 *midi, uint32 size)
{
  uint32 len, tracklen;
  sint32 divisions;
  int format, tracks;

  if (size < 22 || memcmp(midi, "MThd", 4))
    return 0;

  len = getbe32(midi + 4);
  if (len < 6 || 8 + len + 8 > size)
    return 0;

  format = (midi[8] << 8) | midi[9];
  tracks = (midi[10] << 8) | midi[11];
  divisions = (sint16)((midi[12] << 8) | midi[13]);

  /* GbaWadUtil writes type 0 files. Only the first track of anything
     else would be played. */
  if (format > 2 || tracks < 1)
    return 0;

  if (divisions < 0)
    {
      /* SMPTE time -- totally untested. Got a MIDI file that uses this? */
      divisions = (sint32)(-(divisions/256)) * (sint32)(divisions & 0xFF);
    }

  if (divisions <= 0)
    return 0;

  midi += 8 + len;
  size -= 8 + len;

  if (memcmp(midi, "MTrk", 4))
    return 0;

  tracklen = getbe32(midi + 4);
  if (tracklen > size - 8)
    tracklen = size - 8;

  song->midi_track = midi + 8;
  song->midi_end = song->midi_track + tracklen;
  song->divisions = divisions;
  song->event_time = 0;
  song->sample_cum = 0;
  song->counting_time = 2; /* We strip any silence before the first NOTE ON. */

  compute_sample_increment(song, 500000);
  midi_stream_rewind(song);

  return 1;
}

void midi_stream_rewind(MidSong *song)
{
  int i;

  song->midi_pos = song->midi_track;
  song->laststatus = 0;
  song->lastchan = 0;
  song->nrpn = 0;

  /* Not a valid program, so the first program change on each channel is
     always kept. A looped song may have left a channel on another one. */
  for (i=0; i<16; i++)
    {
      song->rpn_msb[i] = song->rpn_lsb[i] = 0;
      song->current_set[i] = 0xFF;
      song->current_program[i] = 0xFF;
    }
}

/* Groom each event as it's read: convert its time to samples (handling tempo
   changes) and drop the ones that don't do anything. */
void midi_stream_next(MidSong *song)
{
  MidEvent *e = &song->event;
  sint32 dt;

  for (;;)
    {
      int skip_this_event = 0;

      if (!read_midi_event(song, &dt))
	{
	  e->type = ME_EOT;
	  e->time = song->event_time;
	  return;
	}

      switch (e->type)
	{
	case ME_TEMPO:
	  skip_this_event=1;
	  break;

	case ME_PROGRAM:
	  if (ISDRUMCHANNEL(song, e->channel))
	    {
	      /* Only drumset 0 exists. */
	      e->a = 0;
	      if (song->current_set[e->channel] != e->a)
		song->current_set[e->channel]=e->a;
	      else
		skip_this_event=1;
	    }
	  else
	    {
	      if (song->current_program[e->channel] != e->a)
		song->current_program[e->channel] = e->a;
	      else
		skip_this_event=1;
	    }
	  break;

	case ME_NOTEON:
	  if (song->counting_time)
	    song->counting_time=1;
	  break;

	case ME_TONE_BANK:
	  /* Only tone bank 0 exists. */
	  skip_this_event=1;
	  break;
	}

      /* Recompute time in samples */
      if (dt && !song->counting_time)
	{
	  sint32 samples_to_do = song->sample_increment * dt;
	  song->sample_cum += song->sample_correction * dt;
	  if (song->sample_cum & 0xFFFF0000)
	    {
	      samples_to_do += ((song->sample_cum >> 16) & 0xFFFF);
	      song->sample_cum &= 0x0000FFFF;
	    }
	  song->event_time += samples_to_do;
	}
      else if (song->counting_time==1) song->counting_time=0;

      if (e->type==ME_TEMPO)
	compute_sample_increment(song, e->channel + e->b * 256 + e->a * 65536);

      if (!skip_this_event)
	{
	  e->time = song->event_time;
	  return;
	}
    }
}
