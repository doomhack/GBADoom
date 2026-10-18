#ifndef DOOM_IWAD_H
#define DOOM_IWAD_H

//Bump this when the processed IWAD format changes, and rebuild GbaWadUtil
//(tools/GbaWadUtil), which writes it into the ROM's header.
//2: DS lumps kept as signed 8 bit, D_ lumps converted to MIDI, GUSBANK added.
//3: DS lumps resampled to the mixer rate (GUSBANK_RATE).
//13: PLAYPAL replaced by GBAPAL.
//14: Full-screen pictures stored as raw pixels (RAWPAGE_WIDTH), PFUB1/2 as BUNNYPIC.
//15: The dehackable tables in GBASTATE, GBAMOBJ, GBAWEAP, GBAMISC and GBATEXT.
#define DOOM_IWAD_VERSION 15

//Full-screen pictures (TITLEPIC, INTERPIC, ENDPIC etc.): GbaWadUtil draws
//each 320x200 patch and downscales it to RAWPAGE_WIDTH x RAWPAGE_HEIGHT
//palette indices (PLAYPAL 0), row by row, ready to copy to the screen.
#define RAWPAGE_WIDTH 240
#define RAWPAGE_HEIGHT 160

//The E3 ending's two scrolling pictures, PFUB2 then PFUB1, downscaled as
//one RAWPAGE_HEIGHT high picture BUNNYPIC_WIDTH wide. PFUB1/2 are removed.
#define BUNNYPIC_WIDTH (RAWPAGE_WIDTH * 2)

//GBAPAL: the WAD's PLAYPAL converted by GbaWadUtil to GBA RGB15
//(little endian, red in the low bits), ready to copy to palette RAM.
//GBAPAL_GAMMA_LEVELS blocks, one per Doom gamma level (its gammatable),
//each GBAPAL_PALETTES palettes of 256 colours.
#define GBAPAL_GAMMA_LEVELS 5
#define GBAPAL_PALETTES 14
#define GBAPAL_COLOURS 256

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
//The Qt build loads a WAD written by GbaWadUtil -out at startup.
extern const unsigned char* doom_iwad;
extern unsigned int doom_iwad_len;
void I_LoadIwad(const char* path);
#endif

//GbaWadUtil writes DOOM_IWAD_VERSION as a 32 bit lump of this name, so the
//Qt build can tell a stale WAD (the GBA build checks the ROM header).
#define DOOM_IWAD_VERSION_LUMP "GBAVER"

#endif // DOOM_IWAD_H
