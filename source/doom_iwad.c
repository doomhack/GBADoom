#include "doom_iwad.h"

//On GBA the IWAD isn't compiled in. GbaWadUtil -rom appends it to the ROM.
#ifndef GBA

#include <stdio.h>
#include <stdlib.h>

#include "lprintf.h"

//A WAD written by GbaWadUtil -out, loaded whole by I_LoadIwad.
const unsigned char* doom_iwad = NULL;
unsigned int doom_iwad_len = 0;

void I_LoadIwad(const char* path)
{
    if(!path)
    {
        I_Error("No IWAD. Build one with GbaWadUtil -out and run GBADoom -iwad <file>.");
        exit(1);
    }

    FILE* f = fopen(path, "rb");

    if(!f)
    {
        I_Error("Can't open IWAD %s.", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char* buf = malloc(len > 0 ? len : 1);

    if(len <= 0 || !buf || fread(buf, 1, len, f) != (size_t)len)
    {
        I_Error("Can't read IWAD %s.", path);
        exit(1);
    }

    fclose(f);

    doom_iwad = buf;
    doom_iwad_len = len;
}

#endif
