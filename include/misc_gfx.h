#ifndef MISC_GFX_H
#define MISC_GFX_H

//Uncomment the line below of which TITLEPIC you want to use in your ROM.

#define TITLEPIC_Doom1 //Doom Shareware
//#define TITLEPIC_Doom //Doom Commercial
//#define TITLEPIC_UltDoom  //Ultimate Doom
//#define TITLEPIC_Doom2 //Doom II
//#define TITLEPIC_TNT //TNT: Evilution
//#define TITLEPIC_Plut //Plutonia
//#define TITLEPIC_PWAD //Load TITLEPIC direct from WAD, doomhack's main branch behavior


//Don't bother making these vars if loading from PWAD
#ifndef TITLEPIC_PWAD
extern const unsigned char gfx_titlepic[];
extern const unsigned int gfx_titlepic_len;
#endif
#endif // MISC_GFX_H
