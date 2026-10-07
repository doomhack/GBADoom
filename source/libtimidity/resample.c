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
 * resample.c
 *
 * GBADoom: the plain, loop and bidir (and vibrato) resamplers are merged
 * into one function that hands spans of constant increment to the mixer's
 * inner loop (s_mix.iwram.c), which resamples and mixes in one pass.
 * No floating point: pitch is computed with the bank's pitch_ratio and the
 * bend tables.
 */

#include "timidity_internal.h"
#include "instrum.h"
#include "playmidi.h"
#include "timi_tables.h"
#include "resample.h"
#include "s_mix.h"

#define PRECALC_LOOP_COUNT(start, end, incr) (((end) - (start) + (incr) - 1) / (incr))

/* min(i, PRECALC_LOOP_COUNT(start, end, incr)), without dividing when all i
   samples are short of end, which is most of the time. */
static inline sint32 span_count(sint32 start, sint32 end, sint32 incr, sint32 i)
{
  sint32 last = start + (i - 1) * incr;

  if ((incr > 0) ? (last < end) : (last > end))
    return i;

  return PRECALC_LOOP_COUNT(start, end, incr);
}

sint32 freq_to_increment(const MidSample *sp, sint32 frequency)
{
  return (sint32)(((long long)frequency * sp->pitch_ratio) >> GUSBANK_PITCH_SHIFT);
}

sint32 bend_factor(sint32 i)
{
  uint32 c;

  if (i >= 0)
    {
      c = i >> 13;
      if (c > BEND_COARSE_MAX)
	c = BEND_COARSE_MAX;

      /* 1.15 * 20.12 -> 8.24 */
      return (sint32)(((uint32)bend_fine[(i>>5) & 0xFF] * bend_coarse[c]) >> 3);
    }

  i = -i;
  c = i >> 13;
  if (c > BEND_COARSE_MAX)
    c = BEND_COARSE_MAX;

  /* 1.15 * 16.16 -> 8.24 */
  return (sint32)(((uint32)bend_fine_inv[(i>>5) & 0xFF] * bend_coarse_inv[c]) >> 7);
}

/*********************** vibrato ***************************/

static sint32 update_vibrato(MidVoice *vp, int sign)
{
  sint32 depth, a, pb;

  if (vp->vibrato_phase++ >= 2*MID_VIBRATO_SAMPLE_INCREMENTS-1)
    vp->vibrato_phase=0;

  depth=vp->sample->vibrato_depth<<7;

  if (vp->vibrato_sweep)
    {
      /* Need to update sweep */
      vp->vibrato_sweep_position += vp->vibrato_sweep;
      if (vp->vibrato_sweep_position >= (1<<SWEEP_SHIFT))
	vp->vibrato_sweep=0;
      else
	{
	  /* Adjust depth */
	  depth *= vp->vibrato_sweep_position;
	  depth >>= SWEEP_SHIFT;
	}
    }

  a = freq_to_increment(vp->sample, vp->frequency);

  pb = (timi_sine(vp->vibrato_phase *
		  (SINE_CYCLE_LENGTH/(2*MID_VIBRATO_SAMPLE_INCREMENTS))) * depth) >> 15;

  a = (sint32)(((long long)a * bend_factor(pb)) >> 24);

  if (sign)
    a = -a; /* need to preserve the loop direction */

  return a;
}

/*************** resample and mix *****************/

TIMI_IWRAM sint32 resample_voice(MidSong *song, int v, sint32 *buf, sint32 count, sint32 amp)
{
  MidVoice *vp = &song->voice[v];
  const MidSample *sp = vp->sample;
  const sample_t *src = sample_data(song, sp);
  sint32 ofs = vp->sample_offset, incr = vp->sample_increment;
  sint32 le, ls, cc, done = 0;
  uint8 modes = sp->modes;
  int looping, bidir;

  if (!sp->pitch_ratio)
    {
      /* Pre-resampled data -- just mix it until we're out of data. */
      sint32 left = (sp->data_length >> FRACTION_BITS) - (ofs >> FRACTION_BITS);

      if (count >= left)
	{
	  if (left > 0)
	    S_MixDirect(buf, src + (ofs >> FRACTION_BITS), left, amp);

	  vp->status = VOICE_FREE;
	  return (left > 0) ? left : 0;
	}

      S_MixDirect(buf, src + (ofs >> FRACTION_BITS), count, amp);
      vp->sample_offset += count << FRACTION_BITS;
      return count;
    }

  looping = (modes & MODES_LOOPING) &&
    ((modes & MODES_ENVELOPE) ||
     (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED));

  ls = sp->loop_start;
  le = sp->loop_end;

  if (looping && le <= ls)
    looping = 0;

  bidir = looping && (modes & MODES_PINGPONG);

  if (!looping)
    {
      le = sp->data_length;
      if (incr < 0) incr = -incr; /* In case we're coming out of a bidir loop */
    }

  if (!incr)
    incr = 1;

  cc = vp->vibrato_control_counter;

  while (count)
    {
      sint32 i = count, n;
      sint32 wrap_end = INT32_MAX; /* No wrap in S_MixResample. */
      int fold = 0;

      if (vp->vibrato_control_ratio)
	{
	  if (!cc)
	    {
	      cc = vp->vibrato_control_ratio;
	      incr = update_vibrato(vp, (incr < 0));
	      if (!looping && incr < 0)
		incr = -incr;
	      if (!incr)
		incr = 1;
	    }
	  if (i > cc)
	    i = cc;
	}

      if (!looping)
	{
	  /* Play sample until end, then free the voice. */
	  if (ofs >= le)
	    {
	      vp->status = VOICE_FREE;
	      break;
	    }
	  n = span_count(ofs, le, incr, i);
	}
      else if (!bidir)
	{
	  /* Play sample until end-of-loop, skip back and continue. */
	  while (ofs >= le)
	    ofs -= le - ls;

	  /* S_MixResample skips back itself, unless one step can overshoot
	     the whole loop. */
	  if (incr <= le - ls)
	    {
	      n = i;
	      wrap_end = le;
	    }
	  else
	    n = span_count(ofs, le, incr, i);
	}
      else if (incr > 0 && ofs < ls)
	{
	  /* Play normally until inside the loop region */
	  n = span_count(ofs, ls, incr, i);
	}
      else
	{
	  /* Then do the bidirectional looping */
	  n = span_count(ofs, incr > 0 ? le : ls, incr, i);
	  fold = 1;
	}

      if (i > n)
	i = n;

      if (i > 0)
	{
	  ofs = S_MixResample(buf, src, ofs, incr, i, amp, wrap_end, le - ls);
	  buf += i;
	  done += i;
	  count -= i;
	  if (vp->vibrato_control_ratio)
	    cc -= i;
	}

      if (!looping)
	{
	  if (ofs >= le)
	    {
	      vp->status = VOICE_FREE;
	      break;
	    }
	}
      else if (fold || i <= 0)
	{
	  if (ofs >= le)
	    {
	      /* fold the overshoot back in */
	      ofs = (le << 1) - ofs;
	      incr = -incr;
	    }
	  else if (ofs <= ls)
	    {
	      ofs = (ls << 1) - ofs;
	      incr = -incr;
	    }
	}
    }

  vp->vibrato_control_counter = cc;
  vp->sample_increment = incr;
  vp->sample_offset = ofs;
  return done;
}
