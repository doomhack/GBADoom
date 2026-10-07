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

#ifndef TIMIDITY_RESAMPLE_H
#define TIMIDITY_RESAMPLE_H

#define resample_voice TIMI_NAMESPACE(resample_voice)
#define freq_to_increment TIMI_NAMESPACE(freq_to_increment)
#define bend_factor TIMI_NAMESPACE(bend_factor)

/* Resample count samples of voice v and add them, scaled by amp, to buf.
   Returns the number of samples mixed, less than count if the voice ended. */
extern sint32 resample_voice(MidSong *song, int v, sint32 *buf, sint32 count, sint32 amp);

/* Sample increment (FRACTION_BITS) to play sp at frequency (milli-Hz). */
extern sint32 freq_to_increment(const MidSample *sp, sint32 frequency);

/* 8.24 factor for a pitch bend of i (semitones << 13), up or down. */
extern sint32 bend_factor(sint32 i);

#endif /* TIMIDITY_RESAMPLE_H */
