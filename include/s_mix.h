#ifndef S_MIX_H
#define S_MIX_H

//
// Software mixer: libtimidity music + Doom's sound effects, mono, signed 8 bit.
// Platform independent; the GBA feeds Direct Sound A with it, the Qt build waveOut.
//

#include <stdint.h>

#include "gusbank.h"

#define SND_MIX_RATE        GUSBANK_RATE

// One GBA frame (280896 cycles) of samples at a 1254 cycle timer period.
#define SND_MIX_SAMPLES     224

#define SND_MIX_FRACBITS    GUSBANK_FRACTION_BITS

#define SND_SFX_VOICES      8

// GbaWadUtil resamples sound effects to SND_MIX_RATE, so they mix without interpolation.
typedef struct snd_sfx_voice_t
{
    const int8_t* data; // NULL when free.
    int32_t ofs;        // Position and length in samples.
    int32_t end;
    int32_t amp;
} snd_sfx_voice_t;

//
// Inner loops, in IWRAM (s_mix.iwram.c). The sample data is the top byte of
// a 16 bit sample: buf accumulates sample * 256 * amp.
//

// Mix count samples of src from ofs, stepping by incr (both SND_MIX_FRACBITS),
// with linear interpolation. Reads src[(ofs >> SND_MIX_FRACBITS) + 1]. Whenever ofs
// reaches loop_end it goes back by loop_len (INT32_MAX: never). Returns the new ofs.
int32_t S_MixResample(int32_t* buf, const int8_t* src, int32_t ofs, int32_t incr, int32_t count, int32_t amp,
                      int32_t loop_end, int32_t loop_len);

// Mix count samples of src that are already at the output rate.
void S_MixDirect(int32_t* buf, const int8_t* src, int32_t count, int32_t amp);

// Convert the accumulator to signed 8 bit (>> 21, clipped) and clear it.
// out must be word aligned and count a multiple of 4.
void S_MixOutput(int8_t* out, int32_t* buf, int32_t count);

// The SND_MIX_SAMPLES accumulator.
int32_t* S_MixBuffer(void);

//
// s_mix.c
//

void S_MixInit(void);

// Render the next SND_MIX_SAMPLES samples.
void S_MixFrame(int8_t* out);

// Start a processed DS lump (see GbaWadUtil) on a voice, replacing what it was playing.
void S_MixStartSfx(int voice, const void* lump, int volume);

void S_MixStartMusic(const void* midi, unsigned int size, int looping);
void S_MixStopMusic(void);
void S_MixPauseMusic(int paused);
void S_MixSetMusicVolume(int volume);

#endif // S_MIX_H
