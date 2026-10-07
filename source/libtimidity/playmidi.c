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
 *
 * playmidi.c -- random stuff in need of rearrangement
 *
 * GBADoom: mono, fixed point, and events come from the ROM MIDI stream
 * (readmidi.c) instead of a pre-built event list.
 */

#include "timidity_internal.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "resample.h"
#include "mix.h"
#include "timi_tables.h"

static void adjust_amplification(MidSong *song)
{
  song->master_volume = (song->amplification << 8) / 100;
}

static void reset_voices(MidSong *song)
{
  int i;
  for (i=0; i<MID_MAX_VOICES; i++)
    song->voice[i].status=VOICE_FREE;
}

/* Process the Reset All Controllers event */
static void reset_controllers(MidSong *song, int c)
{
  song->channel[c].volume=90; /* Some standard says, although the SCC docs say 0. */
  song->channel[c].expression=127; /* SCC-1 does this. */
  song->channel[c].sustain=0;
  song->channel[c].pitchbend=0x2000;
  song->channel[c].pitchfactor=0; /* to be computed */
}

static void reset_midi(MidSong *song)
{
  int i;
  for (i=0; i<16; i++)
    {
      reset_controllers(song, i);
      /* The rest of these are unaffected by the Reset All Controllers event */
      song->channel[i].program=0;
      song->channel[i].pitchsens=2;
      song->channel[i].mono=0;
    }
  reset_voices(song);
}

static void select_sample(MidSong *song, int v, const MidInstrument *ip)
{
  sint32 f, cdiff, diff;
  int s,i;
  const MidSample *sp, *closest;

  s=ip->samples;
  sp=ip->sample;

  if (s==1)
    {
      song->voice[v].sample=sp;
      return;
    }

  f=song->voice[v].orig_frequency;
  for (i=0; i<s; i++, sp++)
    {
      if (sp->low_freq <= f && sp->high_freq >= f)
	{
	  song->voice[v].sample=sp;
	  return;
	}
    }

  /*
     No suitable sample found! We'll select the sample whose root
     frequency is closest to the one we want. (Actually we should
     probably convert the low, high, and root frequencies to MIDI
     note values and compare those.)
   */
  cdiff=0x7FFFFFFF;
  closest=sp=ip->sample;
  for(i=0; i<s; i++, sp++)
    {
      diff=sp->root_freq - f;
      if (diff<0) diff=-diff;
      if (diff<cdiff)
	{
	  cdiff=diff;
	  closest=sp;
	}
    }
  song->voice[v].sample=closest;
}

static void recompute_freq(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];
  MidChannel *cp = &song->channel[vp->channel];
  int
    sign=(vp->sample_increment < 0), /* for bidirectional loops */
    pb=cp->pitchbend;
  sint32 a;

  if (!vp->sample->pitch_ratio)
    return;

  if (pb==0x2000 || pb<0 || pb>0x3FFF)
    vp->frequency = vp->orig_frequency;
  else
    {
      pb-=0x2000;
      if (!(cp->pitchfactor))
	{
	  /* Damn. Somebody bent the pitch. */
	  cp->pitchfactor = bend_factor(pb * cp->pitchsens);
	}

      vp->frequency = (sint32)(((long long)vp->orig_frequency * cp->pitchfactor) >> 24);
    }

  a = freq_to_increment(vp->sample, vp->frequency);

  if (sign)
    a = -a; /* need to preserve the loop direction */

  vp->sample_increment = a;
}

void recompute_amp(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];
  sint32 tempamp;

  tempamp= (vp->velocity *
	    song->channel[vp->channel].volume *
	    song->channel[vp->channel].expression); /* 21 bits */

  /* Mono: tempamp * volume (4.12) * master_volume (8.8) / 2^21, in 16.16 */
  vp->left_amp = (sint32)(((long long)tempamp * vp->sample->volume * song->master_volume) >> 25);
}

static void start_note(MidSong *song, MidEvent *e, int i)
{
  MidInstrument ip;
  MidVoice *vp = &song->voice[i];

  if (ISDRUMCHANNEL(song, e->channel))
    {
      if (!get_instrument(song, 1, e->a, &ip))
	return; /* No instrument? Then we can't play. */

      if (ip.sample->note_to_use) /* Do we have a fixed pitch? */
	vp->orig_frequency = freq_table[(int)(ip.sample->note_to_use)];
      else
	vp->orig_frequency = freq_table[e->a & 0x7F];

      /* drums are supposed to have only one sample */
      vp->sample = ip.sample;
    }
  else
    {
      if (!get_instrument(song, 0, song->channel[e->channel].program, &ip))
	return; /* No instrument? Then we can't play. */

      if (ip.sample->note_to_use) /* Fixed-pitch instrument? */
	vp->orig_frequency = freq_table[(int)(ip.sample->note_to_use)];
      else
	vp->orig_frequency = freq_table[e->a & 0x7F];
      select_sample(song, i, &ip);
    }

  vp->status = VOICE_ON;
  vp->channel = e->channel;
  vp->note = e->a;
  vp->velocity = e->b;
  vp->sample_offset = 0;
  vp->sample_increment = 0; /* make sure it isn't negative */

  vp->tremolo_phase = 0;
  vp->tremolo_phase_increment = vp->sample->tremolo_phase_increment;
  vp->tremolo_sweep = vp->sample->tremolo_sweep_increment;
  vp->tremolo_sweep_position = 0;
  vp->tremolo_volume = 32768;

  vp->vibrato_sweep = vp->sample->vibrato_sweep_increment;
  vp->vibrato_sweep_position = 0;
  vp->vibrato_control_ratio = vp->sample->vibrato_control_ratio;
  vp->vibrato_control_counter = vp->vibrato_phase = 0;

  recompute_freq(song, i);
  recompute_amp(song, i);
  if (vp->sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      vp->envelope_stage = 0;
      vp->envelope_volume = 0;
      vp->control_counter = 0;
      recompute_envelope(song, i);
      apply_envelope_to_amp(song, i);
    }
  else
    {
      vp->envelope_increment = 0;
      apply_envelope_to_amp(song, i);
    }
}

static void kill_note(MidSong *song, int i)
{
  song->voice[i].status = VOICE_DIE;
}

/* Only one instance of a note can be playing on a single channel. */
static void note_on(MidSong *song)
{
  int i = MID_MAX_VOICES, lowest=-1;
  sint32 lv=0x7FFFFFFF, v;
  MidEvent *e = song->current_event;

  while (i--)
    {
      if (song->voice[i].status == VOICE_FREE)
	lowest=i; /* Can't get a lower volume than silence */
      else if (song->voice[i].channel==e->channel &&
	       (song->voice[i].note==e->a || song->channel[song->voice[i].channel].mono))
	kill_note(song, i);
    }

  if (lowest != -1)
    {
      /* Found a free voice. */
      start_note(song,e,lowest);
      return;
    }

  /* Look for the decaying note with the lowest volume */
  i = MID_MAX_VOICES;
  while (i--)
    {
      if ((song->voice[i].status != VOICE_ON) &&
	  (song->voice[i].status != VOICE_DIE))
	{
	  v = song->voice[i].left_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1)
    {
      /* This can still cause a click, but if we had a free voice to
	 spare for ramping down this note, we wouldn't need to kill it
	 in the first place... Still, this needs to be fixed. Perhaps
	 we could use a reserve of voices to play dying notes only. */

      song->voice[lowest].status=VOICE_FREE;
      start_note(song,e,lowest);
    }
}

static void finish_note(MidSong *song, int i)
{
  if (song->voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* We need to get the envelope out of Sustain stage */
      song->voice[i].envelope_stage = 3;
      song->voice[i].status = VOICE_OFF;
      recompute_envelope(song, i);
      apply_envelope_to_amp(song, i);
    }
  else
    {
      /* Set status to OFF so resample_voice() will let this voice out
	 of its loop, if any. In any case, this voice dies when it
	 hits the end of its data (ofs>=data_length). */
      song->voice[i].status = VOICE_OFF;
    }
}

static void note_off(MidSong *song)
{
  int i = MID_MAX_VOICES;
  MidEvent *e = song->current_event;

  while (i--)
    if (song->voice[i].status == VOICE_ON &&
	song->voice[i].channel == e->channel &&
	song->voice[i].note == e->a)
      {
	if (song->channel[e->channel].sustain)
	  {
	    song->voice[i].status = VOICE_SUSTAINED;
	  }
	else
	  finish_note(song, i);
	return;
      }
}

/* Process the All Notes Off event */
static void all_notes_off(MidSong *song)
{
  int i = MID_MAX_VOICES;
  int c = song->current_event->channel;

  while (i--)
    if (song->voice[i].status == VOICE_ON &&
	song->voice[i].channel == c)
      {
	if (song->channel[c].sustain)
	  song->voice[i].status = VOICE_SUSTAINED;
	else
	  finish_note(song, i);
      }
}

/* Process the All Sounds Off event */
static void all_sounds_off(MidSong *song)
{
  int i = MID_MAX_VOICES;
  int c = song->current_event->channel;

  while (i--)
    if (song->voice[i].channel == c &&
	song->voice[i].status != VOICE_FREE &&
	song->voice[i].status != VOICE_DIE)
      {
	kill_note(song, i);
      }
}

static void adjust_pressure(MidSong *song)
{
  MidEvent *e = song->current_event;
  int i = MID_MAX_VOICES;

  while (i--)
    if (song->voice[i].status == VOICE_ON &&
	song->voice[i].channel == e->channel &&
	song->voice[i].note == e->a)
      {
	song->voice[i].velocity = e->b;
	recompute_amp(song, i);
	apply_envelope_to_amp(song, i);
	return;
      }
}

static void drop_sustain(MidSong *song)
{
  int i = MID_MAX_VOICES;
  int c = song->current_event->channel;

  while (i--)
    if (song->voice[i].status == VOICE_SUSTAINED && song->voice[i].channel == c)
      finish_note(song, i);
}

static void adjust_pitchbend(MidSong *song)
{
  int c = song->current_event->channel;
  int i = MID_MAX_VOICES;

  while (i--)
    if (song->voice[i].status != VOICE_FREE && song->voice[i].channel == c)
      {
	recompute_freq(song, i);
      }
}

static void adjust_volume(MidSong *song)
{
  int c = song->current_event->channel;
  int i = MID_MAX_VOICES;

  while (i--)
    if (song->voice[i].channel == c &&
	(song->voice[i].status==VOICE_ON || song->voice[i].status==VOICE_SUSTAINED))
      {
	recompute_amp(song, i);
	apply_envelope_to_amp(song, i);
      }
}

/* Handle the event in song->event. Returns 0 at the end of the song. */
static int process_event(MidSong *song)
{
  MidEvent *e = song->current_event;

  switch(e->type)
    {
      /* Effects affecting a single note */
    case ME_NOTEON:
      if (!(e->b)) /* Velocity 0? */
	note_off(song);
      else
	note_on(song);
      break;

    case ME_NOTEOFF:
      note_off(song);
      break;

    case ME_KEYPRESSURE:
      adjust_pressure(song);
      break;

      /* Effects affecting a single channel */
    case ME_PITCH_SENS:
      song->channel[e->channel].pitchsens = e->a;
      song->channel[e->channel].pitchfactor = 0;
      break;

    case ME_PITCHWHEEL:
      song->channel[e->channel].pitchbend = e->a + e->b * 128;
      song->channel[e->channel].pitchfactor = 0;
      /* Adjust pitch for notes already playing */
      adjust_pitchbend(song);
      break;

    case ME_MAINVOLUME:
      song->channel[e->channel].volume = e->a;
      adjust_volume(song);
      break;

    case ME_EXPRESSION:
      song->channel[e->channel].expression = e->a;
      adjust_volume(song);
      break;

    case ME_PROGRAM:
      /* Drum channels only have drumset 0. */
      if (!ISDRUMCHANNEL(song, e->channel))
	song->channel[e->channel].program = e->a;
      break;

    case ME_SUSTAIN:
      song->channel[e->channel].sustain = e->a;
      if (!e->a)
	drop_sustain(song);
      break;

    case ME_RESET_CONTROLLERS:
      reset_controllers(song, e->channel);
      break;

    case ME_ALL_NOTES_OFF:
      all_notes_off(song);
      break;

    case ME_ALL_SOUNDS_OFF:
      all_sounds_off(song);
      break;

    case ME_EOT:
      /* Doom music loops back to the start. Unless no time has passed,
	 which would loop forever. */
      if (!song->loop || e->time == song->loop_time)
	return 0;

      song->loop_time = e->time;
      midi_stream_rewind(song);
      break;
    }

  midi_stream_next(song);
  return 1;
}

int mid_song_start(MidSong *song, const void *midi, unsigned int size, int loop)
{
  song->playing = 0;
  reset_midi(song);

  if (!song->bank || !midi_stream_open(song, (const uint8 *)midi, size))
    return 0;

  song->loop = loop;
  song->loop_time = -1;
  song->current_sample = 0;
  song->current_event = &song->event;

  midi_stream_next(song);

  song->playing = 1;
  return 1;
}

void mid_song_stop(MidSong *song)
{
  song->playing = 0;
  reset_voices(song);
}

void mid_song_render(MidSong *song, sint32 *buf, sint32 count)
{
  sint32 end_sample;
  int i;

  if (!song->playing)
    return;

  end_sample = song->current_sample + count;

  while (song->current_sample < end_sample)
    {
      sint32 block;

      /* Handle all events that should happen at this time */
      while (song->playing && song->current_event->time <= song->current_sample)
	{
	  if (!process_event(song))
	    song->playing = 0;
	}

      if (song->playing && song->current_event->time < end_sample)
	block = song->current_event->time - song->current_sample;
      else
	block = end_sample - song->current_sample;

      for (i = 0; i < MID_MAX_VOICES; i++)
	{
	  if(song->voice[i].status != VOICE_FREE)
	    mix_voice(song, buf, i, block);
	}

      buf += block;
      song->current_sample += block;

      if (!song->playing)
	break;
    }
}

void mid_song_set_volume(MidSong *song, int volume)
{
  int i;
  if (volume > MAX_AMPLIFICATION)
    song->amplification = MAX_AMPLIFICATION;
  else
  if (volume < 0)
    song->amplification = 0;
  else
    song->amplification = volume;
  adjust_amplification(song);
  for (i = 0; i < MID_MAX_VOICES; i++)
    if (song->voice[i].status != VOICE_FREE)
      {
	recompute_amp(song, i);
	apply_envelope_to_amp(song, i);
      }
}
