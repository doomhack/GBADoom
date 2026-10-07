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
 * GBADoom: cut down to a fixed point, mono, ROM based player. Instruments
 * come from a GUSBANK lump (see gusbank.h) built offline by GbaWadUtil and
 * MIDI files are streamed from ROM rather than loaded into an event list.
 */

#ifndef LIBTIMIDITY_H
#define LIBTIMIDITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* stdint types, so they match the mixer's (int32_t is long on the GBA). */
typedef uint8_t uint8;
typedef int8_t sint8;
typedef uint16_t uint16;
typedef int16_t sint16;
typedef uint32_t uint32;
typedef int32_t sint32;

typedef struct _MidSong MidSong;

/* Size of a MidSong, for the caller to allocate. */
extern unsigned int mid_song_size (void);

/* Returns 0 if the GUSBANK lump is missing or doesn't match this build. */
extern int mid_song_init (MidSong *song, const void *gusbank, unsigned int size);

/* Start a type 0 MIDI file held in ROM. Returns 0 if it isn't one. */
extern int mid_song_start (MidSong *song, const void *midi, unsigned int size, int loop);

extern void mid_song_stop (MidSong *song);

/* Amplification in percent. */
extern void mid_song_set_volume (MidSong *song, int volume);

/* Add count samples (mono, signed 16 bit << 13 scale) into buf. */
extern void mid_song_render (MidSong *song, sint32 *buf, sint32 count);

#ifdef __cplusplus
}
#endif

#endif /* LIBTIMIDITY_H */
