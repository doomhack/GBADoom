#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "libtimidity/timidity.h"

#include "doomstat.h"
#include "doomdef.h"
#include "m_fixed.h"
#include "am_map.h"
#include "g_game.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "r_defs.h"
#include "i_sound.h"
#include "m_menu.h"
#include "p_spec.h"
#include "p_enemy.h"
#include "p_map.h"
#include "p_maputl.h"

#include "p_mobj.h"
#include "p_tick.h"

#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"

#include "s_sound.h"

#include "st_lib.h"
#include "st_stuff.h"

#include "v_video.h"

#include "w_wad.h"

#include "wi_stuff.h"

#include "mmus2mid.h"


typedef struct globals_t
{
#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H

//******************************************************************************
//am_map.c
//******************************************************************************

enum automapmode_e automapmode; // Mode that the automap is in

// location of window on screen
int  f_x;
int  f_y;

// size of window on screen
int  f_w;
int  f_h;

mpoint_t m_paninc;    // how far the window pans each tic (map coords)

fixed_t m_x, m_y;     // LL x,y window location on the map (map coords)
fixed_t m_x2, m_y2;   // UR x,y window location on the map (map coords)

//
// width/height of window on map (map coords)
//
fixed_t  m_w;
fixed_t  m_h;

// based on level size
fixed_t  min_x;
fixed_t  min_y;
fixed_t  max_x;
fixed_t  max_y;

fixed_t  max_w;          // max_x-min_x,
fixed_t  max_h;          // max_y-min_y

fixed_t  min_scale_mtof; // used to tell when to stop zooming out
fixed_t  max_scale_mtof; // used to tell when to stop zooming in

// old location used by the Follower routine
mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
fixed_t scale_mtof;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
fixed_t scale_ftom;

player_t *plr;           // the player represented by an arrow

int lastlevel, lastepisode;

boolean leveljuststarted;       // kluge until AM_LevelInit() is called
boolean stopped;


//******************************************************************************
//d_client.c
//******************************************************************************

ticcmd_t         netcmds[MAXPLAYERS];
ticcmd_t* localcmds;
int maketic;
int lastmadetic;

//******************************************************************************
//d_main.c
//******************************************************************************

boolean singletics; // debug flag to cancel adaptiveness
boolean advancedemo;


boolean isborderstate;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t    wipegamestate;
gamestate_t oldgamestate;

int  demosequence;         // killough 5/2/98: made static
int  pagetic;
const char *pagename; // CPhipps - const


//******************************************************************************
//doomstat.c
//******************************************************************************

// Game Mode - identify IWAD as shareware, retail etc.
GameMode_t gamemode;
GameMission_t   gamemission;


//******************************************************************************
//f_finale.c
//******************************************************************************


// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
int finalestage; // cph -
int finalecount; // made static
const char*   finaletext; // cph -
const char*   finaleflat; // made static const

int             castnum;
int             casttics;
const state_t*  caststate;
int             castframes;
int             castonmelee;


int midstage;                 // whether we're in "mid-stage"
int  laststage;

boolean         castattacking;
boolean         castdeath;

//******************************************************************************
//g_game.c
//******************************************************************************

const byte *demobuffer;   /* cph - only used for playback */
int demolength; // check for overrun (missing DEMOMARKER)

const byte *demo_p;

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
boolean         respawnmonsters;
int             gameepisode;
int             gamemap;
boolean         paused;

// CPhipps - moved *_loadgame vars here
boolean command_loadgame;

boolean         usergame;      // ok to save / end game
boolean         timingdemo;    // if true, exit with report on completion
boolean         nodrawers;     // for comparative timing purposes
int             starttime;     // for comparative timing purposes
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];

int             gametic;
int             basetic;       /* killough 9/29/98: for demo sync */
int             totalkills, totallive, totalitems, totalsecret;    // for intermission
boolean         demoplayback;
int             demover;
boolean         singledemo;           // quit after playing a demo from cmdline
wbstartstruct_t wminfo;               // parms for world map / intermission
boolean         haswolflevels;// jff 4/18/98 wolf levels present
byte            *savebuffer;          // CPhipps - static
int             totalleveltimes;      // CPhipps - total time for all completed levels
int             longtics;


// CPhipps - made lots of key/button state vars static
boolean gamekeydown[NUMKEYS];
int     turnheld;       // for accelerative turning

// Game events info
buttoncode_t special_event; // Event triggered by local player, to send
byte  savegameslot;         // Slot to load if gameaction == ga_loadgame
char         savedescription[SAVEDESCLEN];  // Description to save in savegame if gameaction == ga_savegame

// killough 2/8/98: make corpse queue variable in size
int    bodyqueslot;
mobj_t **bodyque;
int bodyquecount;


gamestate_t prevgamestate;

skill_t d_skill;
int     d_episode;
int     d_map;

boolean secretexit;



//******************************************************************************
//hu_stuff.c
//******************************************************************************

// font sets
patchnum_t hu_font[HU_FONTSIZE];

// widgets
hu_textline_t  w_title;
hu_stext_t     w_message;
int        message_counter;

boolean    message_on;
boolean    message_dontfuckwithme;
boolean    headsupactive;

//******************************************************************************
//i_audio.c
//******************************************************************************

boolean sound_inited;

short* music_buffer;
unsigned int current_music_buffer;

unsigned int music_looping;
unsigned int music_volume;
unsigned int music_init;

unsigned int music_sample_counts[2];

channel_info_t channelinfo[MAX_CHANNELS + 1];

MidIStream* midiStream;
MidSong* midiSong;




unsigned long lasttimereply;
unsigned long basetime;



//******************************************************************************
//i_video.c
//******************************************************************************

const unsigned char* current_pallete;
int newpal;

//******************************************************************************
//m_menu.c
//******************************************************************************

//
// defaulted values
//
int showMessages;    // Show messages has default, 0 = off, 1 = on

int messageToPrint;  // 1 = message to be printed

// CPhipps - static const
const char* messageString; // ...and here is the message string!

int messageLastMenuActive;

boolean messageNeedsInput; // timed message = no input from user

int saveStringEnter;
int saveSlot;        // which slot to save in
int saveCharIndex;   // which char we're editing
// old save description before edit

boolean menuactive;    // The menus are up

short itemOn;           // menu item skull is on (for Big Font menus)
short skullAnimCounter; // skull animation counter
short whichSkull;       // which skull to draw (he blinks)

const menu_t* currentMenu; // current menudef


int epi;

//******************************************************************************
//m_random.c
//******************************************************************************

int	rndindex;
int	prndindex;


//******************************************************************************
//mmus2mis.c
//******************************************************************************

TrackInfo* track;

//******************************************************************************
//p_ceiling.c
//******************************************************************************

// the list of ceilings moving currently, including crushers
ceilinglist_t *activeceilings;

//******************************************************************************
//p_enemy.c
//******************************************************************************

mobj_t *current_actor;

fixed_t dropoff_deltax, dropoff_deltay, floorz;

int current_allaround;

mobj_t* corpsehit;
fixed_t viletryx;
fixed_t viletryy;

// killough 2/7/98: Remove limit on icon landings:
mobj_t **braintargets;
int    numbraintargets_alloc;
int    numbraintargets;

brain_t brain;   // killough 3/26/98: global state of boss brain

//******************************************************************************
//p_map.c
//******************************************************************************

mobj_t    *tmthing;
fixed_t   tmx;
fixed_t   tmy;
int pe_x; // Pain Elemental position for Lost Soul checks // phares
int pe_y; // Pain Elemental position for Lost Soul checks // phares
int ls_x; // Lost Soul position for Lost Soul checks      // phares
int ls_y; // Lost Soul position for Lost Soul checks      // phares

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean   floatok;

/* killough 11/98: if "felldown" true, object was pushed down ledge */
boolean   felldown;

// The tm* items are used to hold information globally, usually for
// line or object intersection checking

fixed_t   tmbbox[4];  // bounding box for line intersection checks
fixed_t   tmfloorz;   // floor you'd hit if free to fall
fixed_t   tmceilingz; // ceiling of sector you're in
fixed_t   tmdropoffz; // dropoff on other side of line you're crossing

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls

line_t    *ceilingline;
line_t        *blockline;    /* killough 8/11/98: blocking linedef */
line_t        *floorline;    /* killough 8/1/98: Highest touched floor */
int         tmunstuck;     /* killough 8/1/98: whether to allow unsticking */

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

// 1/11/98 killough: removed limit on special lines crossed
line_t **spechit;                // new code -- killough
int spechit_max;          // killough

int numspechit;

// Temporary holder for thing_sectorlist threads
msecnode_t* sector_list;                             // phares 3/16/98

boolean telefrag;   /* killough 8/9/98: whether to telefrag at exit */



/* killough 8/2/98: make variables static */
fixed_t   bestslidefrac;
line_t*   bestslideline;
mobj_t*   slidemo;
fixed_t   tmxmove;
fixed_t   tmymove;



mobj_t*   linetarget; // who got hit (or NULL)
mobj_t*   shootthing;

/* killough 8/2/98: for more intelligent autoaiming */
uint_64_t aim_flags_mask;

// Height if not aiming up or down
fixed_t   shootz;

int       la_damage;
fixed_t   attackrange;

fixed_t   aimslope;

// slopes to top and bottom of target
// killough 4/20/98: make static instead of using ones in p_sight.c

fixed_t  topslope;
fixed_t  bottomslope;


mobj_t *bombsource, *bombspot;
int bombdamage;

boolean crushchange, nofit;

mobj_t*   usething;


//******************************************************************************
//p_maputl.c
//******************************************************************************

fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;

// moved front and back outside P-LineOpening and changed    // phares 3/7/98
// them to these so we can pick up the new friction value
// in PIT_CheckLine()
sector_t *openfrontsector; // made global                    // phares
sector_t *openbacksector;  // made global

divline_t trace;



//******************************************************************************
//p_mobj.c
//******************************************************************************

mapthing_t itemrespawnque[ITEMQUESIZE];
int        itemrespawntime[ITEMQUESIZE];
int        iquehead;
int        iquetail;

struct { int first, next; } *doomed_hash;

//******************************************************************************
//p_plats.c
//******************************************************************************

platlist_t *activeplats;       // killough 2/14/98: made global again


//******************************************************************************
//p_pspr.c
//******************************************************************************

fixed_t bulletslope;


//******************************************************************************
//p_saveg.c
//******************************************************************************

byte *save_p;

//******************************************************************************
//p_setup.c
//******************************************************************************


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

int      numvertexes;
vertex_t *vertexes;

int      numsegs;
seg_t    *segs;

int      numsectors;
sector_t *sectors;

int      numsubsectors;
subsector_t *subsectors;

int      numnodes;
node_t   *nodes;

int      numlines;
line_t   *lines;

int      numsides;
side_t   *sides;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

int       bmapwidth, bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
long      *blockmap;              // was short -- killough

// offsets in blockmap are from here
long      *blockmaplump;          // was short -- killough

fixed_t   bmaporgx, bmaporgy;     // origin of block map

mobj_t    **blocklinks;           // for thing chains

//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

int rejectlump;// cph - store reject lump num if cached
const byte *rejectmatrix; // cph - const*

// Maintain single and multi player starting spots.

// 1/11/98 killough: Remove limit on deathmatch starts
mapthing_t *deathmatchstarts;      // killough
size_t     num_deathmatchstarts;   // killough

mapthing_t *deathmatch_p;
mapthing_t playerstarts[MAXPLAYERS];


//******************************************************************************
//p_sight.c
//******************************************************************************

los_t los; // cph - made static

//******************************************************************************
//p_spec.c
//******************************************************************************

anim_t*     lastanim;
anim_t		anims[MAXANIMS];

pusher_t* tmpusher; // pusher structure for blockmap searches


//******************************************************************************
//p_switch.c
//******************************************************************************

int		switchlist[MAXSWITCHES * 2];
int numswitches;                           // killough

button_t  buttonlist[MAXBUTTONS];


//******************************************************************************
//p_tick.c
//******************************************************************************

int leveltime; // tics in game play for par

// killough 8/29/98: we maintain several separate threads, each containing
// a special class of thinkers, to allow more efficient searches.
thinker_t thinkerclasscap[th_all+1];

thinker_t *currentthinker;


//******************************************************************************
//p_user.c
//******************************************************************************

boolean onground; // whether player is on ground or in air


//******************************************************************************
//r_bsp.c
//******************************************************************************

seg_t     *curline;
side_t    *sidedef;
line_t    *linedef;
sector_t  *frontsector;
sector_t  *backsector;
drawseg_t *ds_p;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
// cph - replaced by linedef rendering flags - int      doorclosed;

// killough: New code which removes 2s linedef limit
drawseg_t *drawsegs;
unsigned  maxdrawsegs;

// CPhipps -
// Instead of clipsegs, let's try using an array with one entry for each column,
// indicating whether it's blocked by a solid wall yet or not.

byte solidcol[MAX_SCREENWIDTH];


//******************************************************************************
//r_data.c
//******************************************************************************

int       firstflat, lastflat, numflats;
int       firstspritelump, lastspritelump, numspritelumps;
int       numtextures;
texture_t **textures; // proff - 04/05/2000 removed static for OpenGL
fixed_t   *textureheight; //needed for texture pegging (and TFE fix - killough)
int       *flattranslation;             // for global animation
int       *texturetranslation;


//******************************************************************************
//r_draw.c
//******************************************************************************

byte *translationtables;

draw_vars_t drawvars;

int fuzzoffset[FUZZTABLE];
int fuzzpos;

//******************************************************************************
//r_draw.c
//******************************************************************************

int validcount;         // increment every time a check is made
const lighttable_t* fixedcolormap;
// proff 11/06/98: Added for high-res
fixed_t  viewx, viewy, viewz;
angle_t  viewangle;
fixed_t  viewcos, viewsin;
player_t *viewplayer;


// killough 3/20/98: Support dynamic colormaps, e.g. deep water
// killough 4/4/98: support dynamic number of them as well

const lighttable_t* zlight[LIGHTLEVELS][MAXLIGHTZ];

const lighttable_t *fullcolormap;
const lighttable_t *colormaps;

// killough 3/20/98, 4/4/98: end dynamic colormaps

int extralight;                           // bumped light from gun blasts

boolean setsizeneeded;

//******************************************************************************
//r_patch.c
//******************************************************************************

rpatch_t *patches;

rpatch_t *texture_composites;

//******************************************************************************
//r_plane.c
//******************************************************************************

visplane_t *visplanes[MAXVISPLANES];   // killough
visplane_t *freetail;                  // killough
visplane_t **freehead;     // killough
visplane_t *floorplane, *ceilingplane;

size_t maxopenings;
int *openings,*lastopening; // dropoff overflow

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

int floorclip[MAX_SCREENWIDTH], ceilingclip[MAX_SCREENWIDTH]; // dropoff overflow

// spanstart holds the start of a plane span; initialized to 0 at start

int spanstart[MAX_SCREENHEIGHT];                // killough 2/8/98

//
// texture mapping
//

const lighttable_t **planezlight;
fixed_t planeheight;

// killough 2/8/98: make variables static

fixed_t basexscale, baseyscale;
fixed_t xoffs,yoffs;    // killough 2/28/98: flat offsets

//******************************************************************************
//r_segs.c
//******************************************************************************


// True if any of the segs textures might be visible.
boolean  segtextured;
boolean  markfloor;      // False if the back side is the same plane.
boolean  markceiling;
boolean  maskedtexture;
int      toptexture;
int      bottomtexture;
int      midtexture;

angle_t         rw_normalangle; // angle to line origin
int             rw_angle1;
fixed_t         rw_distance;

//
// regular wall
//
int      rw_x;
int      rw_stopx;
angle_t  rw_centerangle;
fixed_t  rw_offset;
fixed_t  rw_scale;
fixed_t  rw_scalestep;
fixed_t  rw_midtexturemid;
fixed_t  rw_toptexturemid;
fixed_t  rw_bottomtexturemid;
int      rw_lightlevel;
int      worldtop;
int      worldbottom;
int      worldhigh;
int      worldlow;
fixed_t  pixhigh;
fixed_t  pixlow;
fixed_t  pixhighstep;
fixed_t  pixlowstep;
fixed_t  topfrac;
fixed_t  topstep;
fixed_t  bottomfrac;
fixed_t  bottomstep;
int      *maskedtexturecol; // dropoff overflow

int didsolidcol; /* True if at least one column was marked solid */

//******************************************************************************
//r_sky.c
//******************************************************************************

//
// sky mapping
//
int skyflatnum;
int skytexture;


//******************************************************************************
//r_things.c
//******************************************************************************

// variables used to look up and range check thing_t sprites patches

spritedef_t *sprites;
int numsprites;


spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
int maxframe;

vissprite_t *vissprites, **vissprite_ptrs;  // killough
size_t num_vissprite, num_vissprite_alloc, num_vissprite_ptrs;

int   *mfloorclip;   // dropoff overflow
int   *mceilingclip; // dropoff overflow
fixed_t spryscale;
fixed_t sprtopscreen;

//******************************************************************************
//s_sound.c
//******************************************************************************

// the set of channels available
channel_t *channels;

// These are not used, but should be (menu).
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume;

// Maximum volume of music. Useless so far.
int snd_MusicVolume;

// whether songs are mus_paused
boolean mus_paused;

// music currently being played
const musicinfo_t *mus_playing;

//jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

//******************************************************************************
//sounds.c
//******************************************************************************

sfx_runtime sfx_data[128];


//******************************************************************************
//st_stuff.c
//******************************************************************************

// main player in game
player_t *plyr;

// ST_Start() has just been called
boolean st_firsttime;

// whether left-side main status bar is active
boolean st_statusbaron;


// 0-9, tall numbers
patchnum_t tallnum[10];

// tall % sign
patchnum_t tallpercent;

// 0-9, short, yellow (,different!) numbers
patchnum_t shortnum[10];

// face status patches
patchnum_t faces[ST_NUMFACES];

// face background
patchnum_t faceback; // CPhipps - single background, translated for different players

//e6y: status bar background
patchnum_t stbarbg;

// main bar right
patchnum_t armsbg;

// weapon ownership patches
patchnum_t arms[6][2];

// ready-weapon widget
st_number_t w_ready;

// health widget
st_percent_t st_health;

// arms background
st_binicon_t  w_armsbg;

// weapon ownership widgets
st_multicon_t w_arms[6];

// face status widget
st_multicon_t w_faces;

// keycard widgets
st_multicon_t w_keyboxes[3];

// armor widget
st_percent_t  st_armor;

// ammo widgets
st_number_t   st_ammo[4];

// max ammo widgets
st_number_t   w_maxammo[4];

// used to use appopriately pained face
int      st_oldhealth;

// used for evil grin
boolean  oldweaponsowned[NUMWEAPONS];

 // count until face changes
int      st_facecount;

// current face index, used by w_faces
int      st_faceindex;

// holds key-type for each key box on bar
int      keyboxes[3];

// a random number per tick
int      st_randomnumber;

int st_palette;


//******************************************************************************
//v_video.c
//******************************************************************************

// Each screen is [SCREENWIDTH*SCREENHEIGHT];
screeninfo_t screens[NUM_SCREENS];

//******************************************************************************
//w_wad.c
//******************************************************************************

int        numlumps;         // killough

//******************************************************************************
//wi_stuff.c
//******************************************************************************

// used to accelerate or skip a stage
int   acceleratestage;           // killough 3/28/98: made global

// wbs->pnum
int    me;

 // specifies current state
stateenum_t  state;

// contains information passed into intermission
wbstartstruct_t* wbs;

wbplayerstruct_t* plrs;  // wbs->plyr[]

// used for general timing
int    cnt;

// used for timing of background animation
int    bcnt;

int    cnt_time;
int    cnt_total_time;
int    cnt_par;
int    cnt_pause;

// 0-9 graphic
patchnum_t num[10];

int  sp_state;

int *cnt_kills;
int *cnt_items;
int *cnt_secret;


//******************************************************************************
#endif // GLOBAL_DEFS_H

} globals_t;

void InitGlobals();

extern globals_t* _g;

#endif // GLOBAL_DATA_H
