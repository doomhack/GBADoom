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

#ifndef TIMIDITY_OPTIONS_H
#define TIMIDITY_OPTIONS_H

#include "gusbank.h"

/* 9 here is MIDI channel 10, which is the standard percussion channel. */
#define DEFAULT_DRUMCHANNELS (1<<9)

/* In percent. */
#define DEFAULT_AMPLIFICATION	70
#define MAX_AMPLIFICATION	800

/* Polyphony. Notes beyond this steal the quietest decaying voice. */
#define MID_MAX_VOICES	24

/* The control ratio (samples between envelope/tremolo updates) and
   everything derived from it is baked into the GUSBANK by GbaWadUtil. */
#define CONTROL_RATIO GUSBANK_CONTROL_RATIO

/* How many bits to use for the fractional part of sample positions. */
#define FRACTION_BITS GUSBANK_FRACTION_BITS

/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME 20

/* change FRACTION_BITS above, not these */
#define INTEGER_BITS (32 - FRACTION_BITS)
#define INTEGER_MASK (0xFFFFFFFF << FRACTION_BITS)
#define FRACTION_MASK (~ INTEGER_MASK)

/* These affect general volume */
#define GUARD_BITS 3
#define AMP_BITS (15-GUARD_BITS)

#define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)

#define SWEEP_SHIFT 16
#define RATE_SHIFT 5

#endif /* TIMIDITY_OPTIONS_H */
