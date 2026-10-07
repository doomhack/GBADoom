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
 * GBADoom: there is no config file to read. GbaWadUtil reads it and builds
 * the GUSBANK lump, so initialisation is just checking that.
 */

#include <string.h>

#include "timidity_internal.h"
#include "instrum.h"
#include "playmidi.h"

unsigned int mid_song_size(void)
{
  return sizeof(MidSong);
}

int mid_song_init(MidSong *song, const void *gusbank, unsigned int size)
{
  memset(song, 0, sizeof(MidSong));

  song->amplification = DEFAULT_AMPLIFICATION;
  song->master_volume = (DEFAULT_AMPLIFICATION << 8) / 100;
  song->drumchannels = DEFAULT_DRUMCHANNELS;
  song->current_event = &song->event;

  if (!bank_valid(gusbank, size))
    return 0;

  song->bank = (const uint8 *)gusbank;
  return 1;
}
