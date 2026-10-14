#ifndef GUSBANK_BUILDER_H
#define GUSBANK_BUILDER_H

#include <QtCore>
#include "wadfile.h"

//
// Builds the GUSBANK lump that GBADoom's libtimidity player reads from ROM.
// The layout and constants come from the engine's own header, so the two can't drift.
//

#include "../../include/gusbank.h"

//The lump is written by copying these structs, so they must have no padding.
static_assert(sizeof(gusbank_sample_t) == 104, "gusbank_sample_t has padding");
static_assert(sizeof(gusbank_instrument_t) == 8, "gusbank_instrument_t has padding");
static_assert(sizeof(gusbank_header_t) == 536, "gusbank_header_t has padding");

//Build a GUSBANK lump from a timidity .cfg (bank 0 / drumset 0), including only
//the instruments that the MIDI lumps (D_*) in the WAD use. oversample (1, 2 or 4)
//stores the samples that the player pitches at runtime at that multiple of their
//rate: the player takes the nearest sample, so it costs ROM but no CPU.
bool BuildGusBank(WadFile& wadFile, const QString& cfgPath, Lump& bankLump, int oversample = 1);

#endif // GUSBANK_BUILDER_H
