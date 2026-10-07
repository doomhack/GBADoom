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

#ifndef TIMIDITY_INSTRUM_H
#define TIMIDITY_INSTRUM_H

/* Bits in the modes field. GbaWadUtil has already converted 16 bit,
   unsigned and reversed samples. */
#define MODES_LOOPING	(1<<2)
#define MODES_PINGPONG	(1<<3)
#define MODES_SUSTAIN	(1<<5)
#define MODES_ENVELOPE	(1<<6)

#define bank_valid TIMI_NAMESPACE(bank_valid)
#define get_instrument TIMI_NAMESPACE(get_instrument)

extern int bank_valid(const void *bank, unsigned int size);

/* Look up a melodic program (dr == 0) or drum note (dr == 1) in the bank. */
extern int get_instrument(const MidSong *song, int dr, int i, MidInstrument *ip);

#define sample_data(song, sp) ((const sample_t *)((song)->bank + (sp)->data_offset))

#endif /* TIMIDITY_INSTRUM_H */
