#ifndef DOOM_IWAD_H
#define DOOM_IWAD_H

//Bump this (and GbaWadUtil's copy) when the processed IWAD format changes.
//2: DS lumps kept as signed 8 bit, D_ lumps converted to MIDI, GUSBANK added.
//3: DS lumps resampled to the mixer rate (GUSBANK_RATE).
#define DOOM_IWAD_VERSION 12

typedef struct
{
    char magic[16];         //"GBADOOM-IWAD-HDR"
    unsigned int version;   //DOOM_IWAD_VERSION, 0 if no IWAD attached.
    unsigned int length;    //Size of the IWAD following the header.
    unsigned int reserved[2];
} doom_iwad_header_t;

#ifdef GBA
//Both defined by gbadoom.ld at the end of the ROM.
//GbaWadUtil -rom appends the IWAD after the header.
//The alignment stops GCC using byte loads for the WAD header fields.
extern const doom_iwad_header_t doom_iwad_header;
extern const unsigned char doom_iwad[] __attribute__((aligned(32)));
#define doom_iwad_len (doom_iwad_header.length)
#else
extern const unsigned char doom_iwad[];
extern const unsigned int doom_iwad_len;
#endif

#endif // DOOM_IWAD_H
