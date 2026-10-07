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
 * mix.c
 *
 * GBADoom: mono only, fixed point. Envelope and tremolo are applied every
 * CONTROL_RATIO samples, and resample_voice() mixes the span in between.
 */

#include "timidity_internal.h"
#include "instrum.h"
#include "playmidi.h"
#include "timi_tables.h"
#include "resample.h"
#include "mix.h"

/* Returns 1 if envelope runs out */
int recompute_envelope(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];
  int stage;

  stage = vp->envelope_stage;

  if (stage>5)
    {
      /* Envelope ran out. */
      vp->status = VOICE_FREE;
      return 1;
    }

  if (vp->sample->modes & MODES_ENVELOPE)
    {
      if (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED)
	{
	  if (stage>2)
	    {
	      /* Freeze envelope until note turns off. Trumpets want this. */
	      vp->envelope_increment=0;
	      return 0;
	    }
	}
    }
  vp->envelope_stage=stage+1;

  if (vp->envelope_volume==vp->sample->envelope_offset[stage] ||
      (stage > 2 && vp->envelope_volume <
       vp->sample->envelope_offset[stage]))
    return recompute_envelope(song, v);
  vp->envelope_target = vp->sample->envelope_offset[stage];
  vp->envelope_increment = vp->sample->envelope_rate[stage];
  if (vp->envelope_target < vp->envelope_volume)
    vp->envelope_increment = -vp->envelope_increment;
  return 0;
}

void apply_envelope_to_amp(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];
  uint32 la = vp->left_amp; /* 16.16 */

  /* Anything louder clips to MAX_AMP_VALUE anyway. Keeps the multiplies
     below in 32 bits. */
  if (la > (MAX_AMP_VALUE << 4) + 15)
    la = (MAX_AMP_VALUE << 4) + 15;

  if (vp->tremolo_phase_increment)
    la = (la * (uint32)vp->tremolo_volume) >> 15;

  if (vp->sample->modes & MODES_ENVELOPE)
    la = (la * vol_table[(vp->envelope_volume >> 23) & 0x7F]) >> 15;

  /* 16.16 -> AMP_BITS */
  la >>= 16 - AMP_BITS;

  if (la > MAX_AMP_VALUE)
    la = MAX_AMP_VALUE;

  vp->left_mix = la;
}

static int update_envelope(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];

  vp->envelope_volume += vp->envelope_increment;
  /* Why is there no ^^ operator?? */
  if (((vp->envelope_increment < 0) &&
       (vp->envelope_volume <= vp->envelope_target)) ||
      ((vp->envelope_increment > 0) &&
	   (vp->envelope_volume >= vp->envelope_target)))
    {
      vp->envelope_volume = vp->envelope_target;
      if (recompute_envelope(song, v))
	return 1;
    }
  return 0;
}

static void update_tremolo(MidSong *song, int v)
{
  MidVoice *vp = &song->voice[v];
  sint32 depth = vp->sample->tremolo_depth << 7;

  if (vp->tremolo_sweep)
    {
      /* Update sweep position */
      vp->tremolo_sweep_position += vp->tremolo_sweep;
      if (vp->tremolo_sweep_position >= (1 << SWEEP_SHIFT))
	vp->tremolo_sweep=0; /* Swept to max amplitude */
      else
	{
	  /* Need to adjust depth */
	  depth *= vp->tremolo_sweep_position;
	  depth >>= SWEEP_SHIFT;
	}
    }

  vp->tremolo_phase += vp->tremolo_phase_increment;

  /* 1 - (sine + 1) * depth / 2^17, in 1.15 */
  vp->tremolo_volume = 32768 -
    (sint32)(((uint32)(timi_sine(vp->tremolo_phase >> RATE_SHIFT) + 32768) * (uint32)depth) >> 17);

  /* I'm not sure about the +1.0 there -- it makes tremoloed voices'
     volumes on average the lower the higher the tremolo amplitude. */
}

/* Returns 1 if the note died */
TIMI_IWRAM static int update_signal(MidSong *song, int v)
{
  if (song->voice[v].envelope_increment && update_envelope(song, v))
    return 1;

  if (song->voice[v].tremolo_phase_increment)
    update_tremolo(song, v);

  apply_envelope_to_amp(song, v);
  return 0;
}

/* Ramp a note out in c (<= MAX_DIE_TIME) samples. The note is resampled
   at unity amp in one go and then ramped, since a resample_voice() call
   per sample costs more than mixing dozens of samples. */
static void ramp_out(MidSong *song, sint32 *lp, int v, sint32 c)
{
  sint32 left, li, i, n;
  sint32 tmp[MAX_DIE_TIME];

  left=song->voice[v].left_mix;
  li=-(left/c);
  if (!li) li=-1;

  for (i=0; i<c; i++)
    tmp[i]=0;

  n=resample_voice(song, v, tmp, c, 1);

  for (i=0; i<n; i++)
    {
      left += li;
      if (left<0)
	return;
      lp[i] += tmp[i] * left;
    }
}

/**************** interface function ******************/

TIMI_IWRAM void mix_voice(MidSong *song, sint32 *buf, int v, sint32 c)
{
  MidVoice *vp = song->voice + v;

  if (vp->status==VOICE_DIE)
    {
      if (c>=MAX_DIE_TIME)
	c=MAX_DIE_TIME;
      ramp_out(song, buf, v, c);
      vp->status=VOICE_FREE;
    }
  else if (vp->envelope_increment || vp->tremolo_phase_increment)
    {
      sint32 cc = vp->control_counter;

      if (!cc)
	{
	  cc = CONTROL_RATIO;
	  if (update_signal(song, v))
	    return;	/* Envelope ran out */
	}

      while (c)
	{
	  sint32 n = (cc < c) ? cc : c;

	  if (resample_voice(song, v, buf, n, vp->left_mix) < n)
	    return; /* Out of data */

	  buf += n;
	  c -= n;
	  cc -= n;

	  if (!cc && c)
	    {
	      cc = CONTROL_RATIO;
	      if (update_signal(song, v))
		return;	/* Envelope ran out */
	    }
	}

      vp->control_counter = cc;
    }
  else
    resample_voice(song, v, buf, c, vp->left_mix);
}
