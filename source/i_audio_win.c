//
// Qt build sound output: waveOut, polled from I_UpdateSound() every frame.
// Plays the same 8 bit mixer output as the GBA. Kept apart from i_audio.c
// because windows.h and Doom's headers (LONG etc) don't mix.
//

#ifndef GBA

#include <windows.h>
#include <mmsystem.h>

#include "s_mix.h"

void I_StartAudioDevice(void);
void I_UpdateSound(void);

#define WAVE_BUFFERS 16

static HWAVEOUT waveOut;
static WAVEHDR waveHeaders[WAVE_BUFFERS];
// Words, since S_MixOutput() writes four samples at a time.
static uint32_t waveData[WAVE_BUFFERS][SND_MIX_SAMPLES / 4];

void I_StartAudioDevice(void)
{
    WAVEFORMATEX fmt = {0};

    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = SND_MIX_RATE;
    fmt.wBitsPerSample = 8;
    fmt.nBlockAlign = 1;
    fmt.nAvgBytesPerSec = SND_MIX_RATE;

    if(waveOutOpen(&waveOut, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        waveOut = NULL;
        return;
    }

    for(int i = 0; i < WAVE_BUFFERS; i++)
    {
        waveHeaders[i].lpData = (LPSTR)waveData[i];
        waveHeaders[i].dwBufferLength = SND_MIX_SAMPLES;
        waveOutPrepareHeader(waveOut, &waveHeaders[i], sizeof(WAVEHDR));
        waveHeaders[i].dwFlags |= WHDR_DONE;
    }

    I_UpdateSound();
}

void I_UpdateSound(void)
{
    if(!waveOut)
        return;

    for(int i = 0; i < WAVE_BUFFERS; i++)
    {
        if(!(waveHeaders[i].dwFlags & WHDR_DONE))
            continue;

        S_MixFrame((int8_t*)waveData[i]);

        // waveOut 8 bit is unsigned.
        for(int j = 0; j < SND_MIX_SAMPLES / 4; j++)
            waveData[i][j] ^= 0x80808080;

        waveHeaders[i].dwFlags &= ~WHDR_DONE;
        waveOutWrite(waveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
}

#endif
