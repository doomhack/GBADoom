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

#ifndef TIMIDITY_READMIDI_H
#define TIMIDITY_READMIDI_H

#define midi_stream_open TIMI_NAMESPACE(midi_stream_open)
#define midi_stream_rewind TIMI_NAMESPACE(midi_stream_rewind)
#define midi_stream_next TIMI_NAMESPACE(midi_stream_next)

/* Check the header of a type 0 MIDI file and point the stream at its track. */
extern int midi_stream_open(MidSong *song, const uint8 *midi, uint32 size);

/* Back to the start of the track. Event times carry on from where they were. */
extern void midi_stream_rewind(MidSong *song);

/* Read the next event (time in samples) into song->event. ME_EOT at the end. */
extern void midi_stream_next(MidSong *song);

#endif /* TIMIDITY_READMIDI_H */
