#ifndef VRAM_SPARE_H
#define VRAM_SPARE_H

//*****************************************
//Video memory not used by Mode 4.
//
//Mode 4 uses 240x160 bytes per page, so each
//page has a 2560 byte gap before the next
//0xA000 boundary. OBJs are disabled, so OBJ
//VRAM and OAM are free too. Tables live here
//to save IWRAM.
//
//The compiler lays out each region and the
//static_asserts below fail the build if one
//overflows. Free space is REGION_SIZE - sizeof.
//
//VRAM and OAM need 16 or 32 bit writes
//(8 bit writes are duplicated or ignored), so
//only use short/int/fixed_t/pointer members,
//or byte arrays that are only written with
//16/32 bit copies.
//
//See IWRAM_BUDGET.md.
//*****************************************

#include "doomdef.h"
#include "m_fixed.h"
#include "tables.h"
#include "r_defs.h"

#define VRAM_PAGE_SPARE_SIZE (0xA000 - 0x9600)
#define VRAM_TAIL_SIZE (0x18000 - 0x13600)
#define OAM_SPARE_SIZE 1024

#define FLAT_SIZE 4096
#define FLAT_CACHE_SLOTS 2

//Per frame summary of a drawseg that can clip sprites, so
//R_DrawSprite can reject most drawsegs without reading them
//from EWRAM. Only shorts and fixed_t, so stores are 16/32 bit.
typedef struct drawseg_clip_s
{
    //x2 << 16 | (255 - x1). See R_DrawSprite for the overlap test.
    unsigned int xkey;
    fixed_t lowscale, scale;    //min/max of scale1, scale2.
    short index;                //into _g->drawsegs.
    short masked;               //has a masked mid texture.
} drawseg_clip_t;

//Gap after page 1: 0x06009600.
typedef struct vram1_spare_s
{
    //VRAM copies of ROM tables (VRAM is faster than ROM).
    //Sized from the ROM tables so the copies always match.
    fixed_t yslope[sizeof(yslope) / sizeof(yslope[0])];
    fixed_t distscale[sizeof(distscale) / sizeof(distscale[0])];
    angle_t xtoviewangle[sizeof(xtoviewangle) / sizeof(xtoviewangle[0])];

    short wipe_y_lookup[SCREENWIDTH];
    vissprite_t* vissprite_ptrs[MAXVISSPRITES];
} vram1_spare_t;

//Gap after page 2 plus all of OBJ VRAM: 0x06013600 - 0x06018000.
//These are contiguous, so keep the free space in one block at
//the start and the flat cache at the end.
typedef struct vram_tail_s
{
    short unused[(VRAM_TAIL_SIZE - FLAT_CACHE_SLOTS * FLAT_SIZE - MAXDRAWSEGS * sizeof(drawseg_clip_t)) / 2];

    //Filled by R_DrawMasked each frame.
    drawseg_clip_t dsclip[MAXDRAWSEGS];

    //Flats are read from here as VRAM is faster than ROM.
    //Filled with BlockCopy.
    byte flatCache[FLAT_CACHE_SLOTS][FLAT_SIZE];
} vram_tail_t;

//OAM: 0x07000000.
typedef struct oam_spare_s
{
    short screenheightarray[SCREENWIDTH];
    short negonearray[SCREENWIDTH];
    short floorclip[SCREENWIDTH];
    short ceilingclip[SCREENWIDTH];
    fixed_t tmpbbox[4];
} oam_spare_t;

//OBJ palette: 0x05000200. Unused as OBJs are disabled (the game
//palette is the BG half). Colormaps read per pixel straight from a
//pointer (sky, fullbright sprites, weapon, fixed colormaps) are kept
//here rather than read from ROM.
#define OBJPAL_SPARE_SIZE 512

typedef struct objpal_spare_s
{
    //colormaps[0], full bright. Filled once by R_InitBuffer.
    lighttable_t fullColormap[256];

    //The powerup fixedcolormap (invulnerability or light amp).
    //Filled by R_RenderPlayerView when fixedcolormap changes.
    lighttable_t fixedColormap[256];
} objpal_spare_t;

static_assert(sizeof(vram1_spare_t) <= VRAM_PAGE_SPARE_SIZE, "vram1_spare overflows the gap after page 1");
static_assert(sizeof(objpal_spare_t) <= OBJPAL_SPARE_SIZE, "objpal_spare overflows the OBJ palette");
static_assert(sizeof(vram_tail_t) == VRAM_TAIL_SIZE, "vram_tail must exactly fill the page 2 gap and OBJ VRAM");
static_assert(sizeof(oam_spare_t) <= OAM_SPARE_SIZE, "oam_spare overflows OAM");

static_assert(sizeof(distscale) / sizeof(distscale[0]) >= SCREENWIDTH, "distscale must cover every column");
static_assert(sizeof(xtoviewangle) / sizeof(xtoviewangle[0]) >= SCREENWIDTH + 1, "xtoviewangle must cover every column edge");

#ifdef GBA
    #define vram1_spare ((vram1_spare_t*)(0x6000000 + 0x9600))
    #define vram_tail ((vram_tail_t*)(0x600A000 + 0x9600))
    #define oam_spare ((oam_spare_t*)0x7000000)
    #define objpal_spare ((objpal_spare_t*)0x5000200)
#else
    extern vram1_spare_t vram1_spare_storage;
    extern vram_tail_t vram_tail_storage;
    extern oam_spare_t oam_spare_storage;
    extern objpal_spare_t objpal_spare_storage;

    #define vram1_spare (&vram1_spare_storage)
    #define vram_tail (&vram_tail_storage)
    #define oam_spare (&oam_spare_storage)
    #define objpal_spare (&objpal_spare_storage)
#endif

#endif
