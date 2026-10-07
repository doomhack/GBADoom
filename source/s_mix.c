//
// Software mixer: libtimidity music plus up to SND_SFX_VOICES sound effects,
// mixed into one mono signed 8 bit stream at SND_MIX_RATE.
//
// On the GBA S_MixFrame() runs in the VBlank interrupt, so i_audio.c masks
// interrupts around the S_Mix* calls that change voices or the song.
//

#include "doomdef.h"
#include "z_zone.h"
#include "w_wad.h"
#include "lprintf.h"
#include "global_data.h"

#include "s_mix.h"
#include "timidity.h"

// Sound effect volume (0-127) to mixer amp. A full scale sample at the
// loudest volume uses about half of the output range, leaving room for music.
#define SFX_AMP_SCALE 32

// Music volume 15 is libtimidity's default amplification.
#define MUSIC_AMP_MAX 70

void S_MixInit(void)
{
    const int lump = W_CheckNumForName("GUSBANK");

    _g->mix_song = Z_Malloc(mid_song_size(), PU_STATIC, NULL);

    if(!mid_song_init(_g->mix_song,
                      (lump >= 0) ? W_CacheLumpNum(lump) : NULL,
                      (lump >= 0) ? W_LumpLength(lump) : 0))
    {
        lprintf("S_MixInit: No GUSBANK, no music.");
    }
}

void S_MixFrame(int8_t* out)
{
    int32_t* buf = S_MixBuffer();

    if(!_g->mix_music_paused)
        mid_song_render(_g->mix_song, buf, SND_MIX_SAMPLES);

    for(int i = 0; i < SND_SFX_VOICES; i++)
    {
        snd_sfx_voice_t* v = &_g->mix_sfx[i];

        if(!v->data)
            continue;

        int32_t count = v->end - v->ofs;

        if(count > SND_MIX_SAMPLES)
            count = SND_MIX_SAMPLES;

        S_MixDirect(buf, v->data + v->ofs, count, v->amp);

        v->ofs += count;

        if(v->ofs >= v->end)
            v->data = NULL;
    }

    S_MixOutput(out, buf, SND_MIX_SAMPLES);
}

// lump is a DS lump as processed by GbaWadUtil: format (3), rate (SND_MIX_RATE),
// sample count, then signed 8 bit samples.
void S_MixStartSfx(int voice, const void* lump, int volume)
{
    if(voice < 0 || voice >= SND_SFX_VOICES)
        return;

    snd_sfx_voice_t* v = &_g->mix_sfx[voice];

    v->data = NULL;

    const uint8_t* p = (const uint8_t*)lump;

    const unsigned int rate = p[2] | (p[3] << 8);
    const unsigned int count = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);

    if(rate != SND_MIX_RATE || !count)
        return;

    v->ofs = 0;
    v->end = count;
    v->amp = volume * SFX_AMP_SCALE;

    v->data = (const int8_t*)(p + 8);
}

void S_MixStartMusic(const void* midi, unsigned int size, int looping)
{
    mid_song_start(_g->mix_song, midi, size, looping);
    _g->mix_music_paused = false;
}

void S_MixStopMusic(void)
{
    mid_song_stop(_g->mix_song);
}

void S_MixPauseMusic(int paused)
{
    _g->mix_music_paused = paused;
}

void S_MixSetMusicVolume(int volume)
{
    mid_song_set_volume(_g->mix_song, (volume * MUSIC_AMP_MAX) / 15);
}

