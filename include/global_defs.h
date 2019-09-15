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
boolean         fastdemo;      // if true, run at full speed -- killough
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

#endif // GLOBAL_DEFS_H
