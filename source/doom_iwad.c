#include "doom_iwad.h"

//On GBA the IWAD isn't compiled in. GbaWadUtil -rom appends it to the ROM.
#ifndef GBA

//Uncomment which edition you want to compile
//#include "iwad/doom1.c"
#include "iwad/doomu.c"
//#include "iwad/doom2.c"
//#include "iwad/tnt.c"
//#include "iwad/plutonia.c"
//#include "iwad/sigil.c"

const unsigned int doom_iwad_len = sizeof(doom_iwad);

#endif
