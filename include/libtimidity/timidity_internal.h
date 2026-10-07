/* libTiMidity -- MIDI to WAVE converter library
 * libTiMidity is licensed under the terms of the GNU Lesser General
 * Public License: see COPYING for details.
 * Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>
 * Copyright (C) 2004 Konstantin Korikov <lostclus@ua.fm>
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

#ifndef TIMIDITY_INTERNAL_H
#define TIMIDITY_INTERNAL_H

/* hide private symbols by prefixing with "_timi_" */
#undef  TIMI_NAMESPACE
#define TIMI_NAMESPACE(x) _timi_ ## x

/* Code that runs for every voice for every control ratio chunk goes in
   IWRAM on the GBA: Thumb code from ROM is several times slower. */
#ifdef GBA
#define TIMI_IWRAM __attribute__((section(".iwram"), long_call, noinline, target("arm"), optimize("Os")))
#else
#define TIMI_IWRAM
#endif

#include "timidity.h"
#include "options.h"

#define MID_VIBRATO_SAMPLE_INCREMENTS 32

/* Samples are signed 8 bit, normalised to full scale. The player treats
   them as the top byte of a 16 bit sample. */
typedef sint8 sample_t;
typedef sint32 final_volume_t;

/* Samples live in ROM, in the GUSBANK lump. */
typedef gusbank_sample_t MidSample;

typedef struct _MidInstrument MidInstrument;
struct _MidInstrument
{
  int samples;
  const MidSample *sample;
};

typedef struct _MidChannel MidChannel;
struct _MidChannel
{
  sint32 pitchfactor; /* 8.24 pitch bend factor, 0 = needs computing */
  sint32 pitchbend;
  uint8 program, volume, sustain, expression;
  uint8 pitchsens, mono;
};

typedef struct _MidVoice MidVoice;
struct _MidVoice
{
  uint8 status, channel, note, velocity;
  const MidSample *sample;
  sint32
    orig_frequency, frequency,
    sample_offset, sample_increment,
    envelope_volume, envelope_target, envelope_increment,
    tremolo_sweep, tremolo_sweep_position,
    tremolo_phase, tremolo_phase_increment,
    vibrato_sweep, vibrato_sweep_position;

  final_volume_t left_mix;

  sint32 left_amp;       /* 16.16 */
  sint32 tremolo_volume; /* 1.15 */
  sint32
    vibrato_phase, vibrato_control_ratio, vibrato_control_counter,
    envelope_stage, control_counter;
};

typedef struct _MidEvent MidEvent;
struct _MidEvent
{
  sint32 time;
  uint8 channel, type, a, b;
};

struct _MidSong
{
  const uint8 *bank; /* GUSBANK lump */
  int playing;
  int loop;
  sint32 loop_time; /* Time the song last looped. */
  sint32 amplification;
  sint32 master_volume; /* 8.8 */
  sint32 drumchannels;
  sint32 current_sample;

  /* The next event to process. Events are read from ROM as they are due. */
  MidEvent event;
  MidEvent *current_event;

  /* MIDI stream state (readmidi.c) */
  const uint8 *midi_track, *midi_pos, *midi_end;
  sint32 divisions;
  sint32 sample_increment, sample_correction, sample_cum;
  sint32 event_time;
  uint8 laststatus, lastchan, nrpn, counting_time;
  uint8 rpn_msb[16], rpn_lsb[16];
  uint8 current_program[16], current_set[16];

  MidChannel channel[16];
  MidVoice voice[MID_MAX_VOICES];
};

#endif /* TIMIDITY_INTERNAL_H */
