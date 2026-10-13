#ifndef GUSBANK_H
#define GUSBANK_H

#include <QtCore>
#include "wadfile.h"

//
// Builds the GUSBANK lump that GBADoom's libtimidity player reads from ROM.
// The layout and constants must match include/gusbank.h in GBADoom.
//

#define GUSBANK_VERSION         1
#define GUSBANK_RATE            13379
#define GUSBANK_CONTROL_RATIO   (GUSBANK_RATE / 250)
#define GUSBANK_FRACTION_BITS   12
#define GUSBANK_PITCH_SHIFT     24

#pragma pack(push, 1)

typedef struct gusbank_sample_t
{
    qint32 loop_start, loop_end, data_length;
    qint32 low_freq, high_freq, root_freq;
    qint32 pitch_ratio;
    qint32 envelope_rate[6];
    qint32 envelope_offset[6];
    qint32 tremolo_sweep_increment, tremolo_phase_increment;
    qint32 vibrato_sweep_increment, vibrato_control_ratio;
    quint32 data_offset;
    quint16 volume;
    quint8 tremolo_depth, vibrato_depth;
    quint8 modes, note_to_use;
    quint8 pad[2];
} gusbank_sample_t;

typedef struct gusbank_instrument_t
{
    quint32 num_samples;
    quint32 samples_offset;
} gusbank_instrument_t;

typedef struct gusbank_header_t
{
    char magic[8];
    quint32 version;
    quint32 rate;
    quint32 control_ratio;
    quint32 num_instruments;
    quint16 tone[128];
    quint16 drum[128];
} gusbank_header_t;

#pragma pack(pop)

//Build a GUSBANK lump from a timidity .cfg (bank 0 / drumset 0), including only
//the instruments that the MIDI lumps (D_*) in the WAD use. oversample (1, 2 or 4)
//stores the samples that the player pitches at runtime at that multiple of their
//rate: the player takes the nearest sample, so it costs ROM but no CPU.
bool BuildGusBank(WadFile& wadFile, const QString& cfgPath, Lump& bankLump, int oversample = 1);

#endif // GUSBANK_H
