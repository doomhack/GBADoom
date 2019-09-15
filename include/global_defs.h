#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H

//******************************************************************************
//am_map.c
//******************************************************************************

int ddt_cheating;         // killough 2/7/98: make global, rename to ddt_*
int leveljuststarted;       // kluge until AM_LevelInit() is called
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

// killough 2/22/98: Remove limit on automap marks,
// and make variables external for use in savegames.

mpoint_t *markpoints;    // where the points are
int markpointnum; // next point to be assigned (also number of points now)
int markpointnum_max;       // killough 2/22/98

boolean stopped;

int lastlevel, lastepisode;


//******************************************************************************
//d_client.c
//******************************************************************************

ticcmd_t         netcmds[MAXPLAYERS][BACKUPTICS];
ticcmd_t* localcmds;
int maketic;
int lastmadetic;

//******************************************************************************
//d_main.c
//******************************************************************************

boolean singletics; // debug flag to cancel adaptiveness
boolean advancedemo;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t    wipegamestate;

boolean isborderstate;
boolean borderwillneedredraw;
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
state_t*        caststate;
boolean         castdeath;
int             castframes;
int             castonmelee;
boolean         castattacking;

int midstage;                 // whether we're in "mid-stage"
int  laststage;



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

boolean secretexit;

skill_t d_skill;
int     d_episode;
int     d_map;




//******************************************************************************
//hu_stuff.c
//******************************************************************************

int hud_displayed;    //jff 2/23/98 turns heads-up display on/off

// font sets
patchnum_t hu_font[HU_FONTSIZE];
patchnum_t hu_fontk[HU_FONTSIZE];//jff 3/7/98 added for graphic key indicators

// widgets
hu_textline_t  w_title;
hu_stext_t     w_message;
hu_itext_t     w_inputbuffer[MAXPLAYERS];
hu_textline_t  w_coordx; //jff 2/16/98 new coord widget for automap
hu_textline_t  w_coordy; //jff 3/3/98 split coord widgets automap
hu_textline_t  w_coordz; //jff 3/3/98 split coord widgets automap
hu_textline_t  w_ammo;   //jff 2/16/98 new ammo widget for hud
hu_textline_t  w_health; //jff 2/16/98 new health widget for hud
hu_textline_t  w_armor;  //jff 2/16/98 new armor widget for hud
hu_textline_t  w_weapon; //jff 2/16/98 new weapon widget for hud
hu_textline_t  w_keys;   //jff 2/16/98 new keys widget for hud
hu_textline_t  w_gkeys;  //jff 3/7/98 graphic keys widget for hud
hu_textline_t  w_monsec; //jff 2/16/98 new kill/secret widget for hud
hu_mtext_t     w_rtext;  //jff 2/26/98 text message refresh widget

boolean    always_off;
boolean    message_on;
boolean    message_list; //2/26/98 enable showing list of messages
boolean    message_dontfuckwithme;
boolean    message_nottobefuckedwith;
int        message_counter;
boolean    headsupactive;

//jff 2/16/98 initialization strings for ammo, health, armor widgets
char hud_ammostr[80];
char hud_healthstr[80];
char hud_armorstr[80];
char hud_weapstr[80];
char hud_keysstr[80];
char hud_gkeysstr[80]; //jff 3/7/98 add support for graphic key display
char hud_monsecstr[80];


//******************************************************************************
//i_audio.c
//******************************************************************************

boolean sound_inited;

short music_buffer[MUSIC_BUFFER_SAMPLES * 2];
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

unsigned char current_pallete[3*256];
int newpal;

//******************************************************************************
//m_menu.c
//******************************************************************************

//
// defaulted values
//
int showMessages;    // Show messages has default, 0 = off, 1 = on
int screenblocks;    // has default

int screenSize;      // temp for screenblocks (0-9)

int messageToPrint;  // 1 = message to be printed

// CPhipps - static const
const char* messageString; // ...and here is the message string!

int messageLastMenuActive;

boolean messageNeedsInput; // timed message = no input from user

int saveStringEnter;
int saveSlot;        // which slot to save in
int saveCharIndex;   // which char we're editing
// old save description before edit
char saveOldString[SAVESTRINGSIZE];

boolean menuactive;    // The menus are up

char savegamestrings[10][SAVESTRINGSIZE];


//******************************************************************************
#endif // GLOBAL_DEFS_H
