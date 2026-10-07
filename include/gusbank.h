#ifndef GUSBANK_H
#define GUSBANK_H

//
// GUSBANK lump: GUS patches preprocessed by GbaWadUtil (-gus <cfg>) for the
// libtimidity music player. Everything that libtimidity would compute when it
// loads a .pat file is done offline for the fixed output rate below, so the
// player reads samples and parameters straight out of ROM.
//
// Must match gusbank.h in GbaWadUtil. Bump GUSBANK_VERSION (and the WAD
// version) whenever the layout or the baked-in rate changes.
//

#include <stdint.h>

#define GUSBANK_VERSION         1

// Output rate. 16777216 / 1254 = 13379Hz. 224 samples per GBA frame.
#define GUSBANK_RATE            13379
#define GUSBANK_CONTROL_RATIO   (GUSBANK_RATE / 250)

#define GUSBANK_FRACTION_BITS   12
#define GUSBANK_PITCH_SHIFT     24

typedef struct gusbank_sample_t
{
    // In samples << GUSBANK_FRACTION_BITS.
    int32_t loop_start, loop_end, data_length;

    // milli-Hz, as in the .pat file.
    int32_t low_freq, high_freq, root_freq;

    // (sample_rate << (FRACTION_BITS + PITCH_SHIFT)) / (root_freq * GUSBANK_RATE).
    // 0 means the data is already at the output rate for note_to_use.
    int32_t pitch_ratio;

    int32_t envelope_rate[6];
    int32_t envelope_offset[6];

    int32_t tremolo_sweep_increment, tremolo_phase_increment;
    int32_t vibrato_sweep_increment, vibrato_control_ratio;

    // Offset from the start of the lump to signed 8 bit data (with 2 guard samples).
    uint32_t data_offset;

    // Gain in 4.12 fixed point. Sample data is normalised to full scale.
    uint16_t volume;

    uint8_t tremolo_depth, vibrato_depth;
    uint8_t modes, note_to_use;
    uint8_t pad[2];
} gusbank_sample_t;

typedef struct gusbank_instrument_t
{
    uint32_t num_samples;
    uint32_t samples_offset; // From the start of the lump to gusbank_sample_t[num_samples].
} gusbank_instrument_t;

typedef struct gusbank_header_t
{
    char magic[8]; // "GUSBANK"
    uint32_t version;
    uint32_t rate;
    uint32_t control_ratio;
    uint32_t num_instruments;

    // Instrument index + 1, 0 = not present.
    uint16_t tone[128]; // By program (bank 0).
    uint16_t drum[128]; // By drum note (drumset 0).

    // Followed by gusbank_instrument_t[num_instruments].
} gusbank_header_t;

#endif // GUSBANK_H
