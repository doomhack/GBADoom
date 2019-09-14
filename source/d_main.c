/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
 *  plus functions to determine game mode (shareware, registered),
 *  parse command line parameters, configure game parameters (turbo),
 *  and call the startup functions.
 *
 *-----------------------------------------------------------------------------
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_net.h"
#include "dstrings.h"
#include "sounds.h"
#include "z_zone.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "p_checksum.h"
#include "i_main.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_main.h"
#include "d_main.h"
#include "lprintf.h"  // jff 08/03/98 - declaration of lprintf
#include "am_map.h"

#include "doom_iwad.h"

void GetFirstMap(int *ep, int *map); // Ty 08/29/98 - add "-warp x" functionality
static void D_PageDrawer(void);

// CPhipps - removed wadfiles[] stuff

boolean singletics = false; // debug flag to cancel adaptiveness

//jff 1/22/98 parms for disabling music and sound
const boolean nosfxparm = false;
const boolean nomusicparm = false;

const skill_t startskill = sk_medium;
const int startepisode = 1;
const int startmap = 1;

static const char* timedemo = NULL;

boolean advancedemo;
char    basesavegame[PATH_MAX+1];  // killough 2/16/98: savegame directory


/*
 * D_PostEvent - Event handling
 *
 * Called by I/O functions when an event is received.
 * Try event handlers for each code area in turn.
 * cph - in the true spirit of the Boom source, let the 
 *  short ciruit operator madness begin!
 */

void D_PostEvent(event_t *ev)
{
  /* cph - suppress all input events at game start
   * FIXME: This is a lousy kludge */
  if (gametic < 3)
      return;

  M_Responder(ev) ||
	  (gamestate == GS_LEVEL && (
				     HU_Responder(ev) ||
				     ST_Responder(ev) ||
				     AM_Responder(ev)
				     )
	  ) ||
	G_Responder(ev);
}

//
// D_Wipe
//
// CPhipps - moved the screen wipe code from D_Display to here
// The screens to wipe between are already stored, this just does the timing
// and screen updating

static void D_Wipe(void)
{
	boolean done;
	int wipestart = I_GetTime () - 1;
	
	do
	{
		int nowtime, tics;
		do
		{
			nowtime = I_GetTime();
			tics = nowtime - wipestart;
		} while (!tics);

		wipestart = nowtime;
		done = wipe_ScreenWipe(tics);

		I_UpdateMusic();	
		
		I_UpdateNoBlit();
		M_Drawer();                   // menu is drawn even on top of wipes
		I_FinishUpdate();             // page flip or blit buffer

	} while (!done);
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t    wipegamestate = GS_DEMOSCREEN;
extern boolean setsizeneeded;
extern int     showMessages;

void D_Display (void)
{
  static boolean isborderstate        = false;
  static boolean borderwillneedredraw = false;
  static gamestate_t oldgamestate = -1;
  boolean wipe;
  boolean viewactive = false, isborder = false;

  if (nodrawers)                    // for comparative timing / profiling
    return;

  if (!I_StartDisplay())
    return;

  // save the current screen if about to wipe
  if (wipe = gamestate != wipegamestate)
    wipe_StartScreen();

  if (gamestate != GS_LEVEL) { // Not a level
    switch (oldgamestate) {
    case -1:
    case GS_LEVEL:
      V_SetPalette(0); // cph - use default (basic) palette
    default:
      break;
    }

    switch (gamestate) {
    case GS_INTERMISSION:
      WI_Drawer();
      break;
    case GS_FINALE:
      F_Drawer();
      break;
    case GS_DEMOSCREEN:
      D_PageDrawer();
      break;
    default:
      break;
    }
  } else if (gametic != basetic) { // In a level
    boolean redrawborderstuff;

    HU_Erase();

    if (setsizeneeded) {               // change the view size if needed
      R_ExecuteSetViewSize();
      oldgamestate = -1;            // force background redraw
    }

    // Work out if the player view is visible, and if there is a border
    viewactive = (!(automapmode & am_active) || (automapmode & am_overlay));
    isborder = viewactive ? (viewheight != SCREENHEIGHT) : ((automapmode & am_active));

    if (oldgamestate != GS_LEVEL) {
      R_FillBackScreen ();    // draw the pattern into the back screen
      redrawborderstuff = isborder;
    } else {
      // CPhipps -
      // If there is a border, and either there was no border last time,
      // or the border might need refreshing, then redraw it.
      redrawborderstuff = isborder && (!isborderstate || borderwillneedredraw);
      // The border may need redrawing next time if the border surrounds the screen,
      // and there is a menu being displayed
      borderwillneedredraw = menuactive && isborder && viewactive && (viewwidth != SCREENWIDTH);
    }
    if (redrawborderstuff)
      R_DrawViewBorder();

    // Now do the drawing
    if (viewactive)
      R_RenderPlayerView (&players[displayplayer]);
    if (automapmode & am_active)
      AM_Drawer();
    ST_Drawer((viewheight != SCREENHEIGHT) || ((automapmode & am_active) && !(automapmode & am_overlay)), redrawborderstuff);
    R_DrawViewBorder();
    HU_Drawer();
  }

  isborderstate      = isborder;
  oldgamestate = wipegamestate = gamestate;

  // draw pause pic
  if (paused) {
    // Simplified the "logic" here and no need for x-coord caching - POPE
    V_DrawNamePatch((320 - V_NamePatchWidth("M_PAUSE"))/2, 4,
                    0, "M_PAUSE", CR_DEFAULT, VPT_STRETCH);
  }

  // menus go directly to the screen
  M_Drawer();          // menu is drawn even on top of everything

  D_BuildNewTiccmds();

  // normal update
  if (!wipe)
    I_FinishUpdate ();              // page flip or blit buffer
  else {
    // wipe update
    wipe_EndScreen();
    D_Wipe();
  }

  I_EndDisplay();
}

//
//  D_DoomLoop()
//
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//

static void D_DoomLoop(void)
{
	for (;;)
	{
		// frame syncronous IO operations
		
		I_StartFrame();
					
		// process one or more tics
		if (singletics)
		{
			I_StartTic ();
			G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
			
			if (advancedemo)
				D_DoAdvanceDemo ();

			M_Ticker ();
			G_Ticker ();
			P_Checksum(gametic);
			
			gametic++;
			maketic++;
		}
		else
			TryRunTics (); // will run at least one tic
		
		// killough 3/16/98: change consoleplayer to displayplayer
		if (players[displayplayer].mo) // cph 2002/08/10
			S_UpdateSounds(players[displayplayer].mo);// move positional sounds

        // Update display, next frame, with current state.
        D_Display();
    }
}

//
//  DEMO LOOP
//

static int  demosequence;         // killough 5/2/98: made static
static int  pagetic;
static const char *pagename; // CPhipps - const

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void)
{
  if (--pagetic < 0)
    D_AdvanceDemo();
}

//
// D_PageDrawer
//
static void D_PageDrawer(void)
{
  // proff/nicolas 09/14/98 -- now stretchs bitmaps to fullscreen!
  // CPhipps - updated for new patch drawing
  // proff - added M_DrawCredits
  if (pagename)
  {
    V_DrawNamePatch(0, 0, 0, pagename, CR_DEFAULT, VPT_STRETCH);
  }
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
  advancedemo = true;
}

/* killough 11/98: functions to perform demo sequences
 * cphipps 10/99: constness fixes
 */

static void D_SetPageName(const char *name)
{
  pagename = name;
}

static void D_DrawTitle1(const char *name)
{
  S_StartMusic(mus_intro);
  pagetic = (TICRATE*170)/35;
  D_SetPageName(name);
}

static void D_DrawTitle2(const char *name)
{
  S_StartMusic(mus_dm2ttl);
  D_SetPageName(name);
}

/* killough 11/98: tabulate demo sequences
 */

static struct
{
    void (*func)(const char *);
    const char *name;
}

const demostates[][4] =
  {
    {
      {D_DrawTitle1, "TITLEPIC"},
      {D_DrawTitle1, "TITLEPIC"},
      {D_DrawTitle2, "TITLEPIC"},
      {D_DrawTitle1, "TITLEPIC"},
    },

    {
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
    },
    {
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
    },

    {
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
    },

    {
      {D_SetPageName, "HELP2"},
      {D_SetPageName, "HELP2"},
      {D_SetPageName, "CREDIT"},
      {D_DrawTitle1,  "TITLEPIC"},
    },

    {
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {D_SetPageName, "CREDIT"},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {G_DeferedPlayDemo, "demo4"},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {NULL},
    }
  };

/*
 * This cycles through the demo sequences.
 * killough 11/98: made table-driven
 */

void D_DoAdvanceDemo(void)
{
  players[consoleplayer].playerstate = PST_LIVE;  /* not reborn */
  advancedemo = usergame = paused = false;
  gameaction = ga_nothing;

  pagetic = TICRATE * 11;         /* killough 11/98: default behavior */
  gamestate = GS_DEMOSCREEN;


  if (!demostates[++demosequence][gamemode].func)
      demosequence = 0;
  demostates[demosequence][gamemode].func
          (demostates[demosequence][gamemode].name);
}

//
// D_StartTitle
//
void D_StartTitle (void)
{
  gameaction = ga_nothing;
  demosequence = -1;
  D_AdvanceDemo();
}

//
// D_AddFile
//
// Rewritten by Lee Killough
//
// Ty 08/29/98 - add source parm to indicate where this came from
// CPhipps - static, const char* parameter
//         - source is an enum
//         - modified to allocate & use new wadfiles array
void D_AddFile (const char *file, wad_source_t source)
{  
  wadfiles[0].name = file;
  wadfiles[0].src = source; // Ty 08/29/98
}

//
// CheckIWAD
//
// Verify a file is indeed tagged as an IWAD
// Scan its lumps for levelnames and return gamemode as indicated
// Detect missing wolf levels in DOOM II
//
// The filename to check is passed in iwadname, the gamemode detected is
// returned in gmode, hassec returns the presence of secret levels
//
// jff 4/19/98 Add routine to test IWAD for validity and determine
// the gamemode from it. Also note if DOOM II, whether secret levels exist
// CPhipps - const char* for iwadname, made static

static void CheckIWAD2(const unsigned char* iwad_data, const unsigned int iwad_len, GameMode_t *gmode,boolean *hassec)
{
    const wadinfo_t* header = (const wadinfo_t*)iwad_data;

    int ud=0,rg=0,sw=0,cm=0,sc=0;

    if(!strncmp(header->identification, "IWAD", 4))
    {
        size_t length = header->numlumps;
        const filelump_t* fileinfo = (const filelump_t*)&iwad_data[header->infotableofs];

        while (length--)
        {
            if (fileinfo[length].name[0] == 'E' && fileinfo[length].name[2] == 'M' && fileinfo[length].name[4] == 0)
            {
              if (fileinfo[length].name[1] == '4')
                ++ud;
              else if (fileinfo[length].name[1] == '3')
                ++rg;
              else if (fileinfo[length].name[1] == '2')
                ++rg;
              else if (fileinfo[length].name[1] == '1')
                ++sw;
            }
            else if (fileinfo[length].name[0] == 'M' && fileinfo[length].name[1] == 'A' && fileinfo[length].name[2] == 'P' && fileinfo[length].name[5] == 0)
            {
              ++cm;
              if (fileinfo[length].name[3] == '3')
              {
                  if (fileinfo[length].name[4] == '1' || fileinfo[length].name[4] == '2')
                    ++sc;
              }
            }
        }
    }
    else
    {
        I_Error("CheckIWAD: IWAD tag not present");
    }

    // Determine game mode from levels present
    // Must be a full set for whichever mode is present
    // Lack of wolf-3d levels also detected here

    *gmode = indetermined;
    *hassec = false;
    if (cm>=30)
    {
      *gmode = commercial;
      *hassec = sc>=2;
    }
    else if (ud>=9)
      *gmode = retail;
    else if (rg>=18)
      *gmode = registered;
    else if (sw>=9)
      *gmode = shareware;
}

//
// IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm


static void IdentifyVersion()
{
    if(doom_iwad && (doom_iwad_len > 0))
    {
        CheckIWAD2(doom_iwad, doom_iwad_len, &gamemode, &haswolflevels);

        /* jff 8/23/98 set gamemission global appropriately in all cases
         * cphipps 12/1999 - no version output here, leave that to the caller
         */
        switch(gamemode)
        {
            case retail:
            case registered:
            case shareware:
                gamemission = doom;
                break;
            case commercial:
                gamemission = doom2;

                /*
                 * TODO: Detect Plutonia and TNT here.
                 *
                 * if(is tnt)
                 *  gamemission = pack_tnt;
                 * else if(is_plut)
                 *  gamemission = pack_plut;
                 */
            break;

            default:
                gamemission = none;
                break;
        }

        if (gamemode == indetermined)
        {
            //jff 9/3/98 use logical output routine
            lprintf(LO_WARN,"Unknown Game Version, may not work\n");
        }

        D_AddFile("doom.wad",source_iwad);
    }
}

//
// D_DoomMainSetup
//
// CPhipps - the old contents of D_DoomMain, but moved out of the main
//  line of execution so its stack space can be freed

static void D_DoomMainSetup(void)
{
    lprintf(LO_INFO,"M_LoadDefaults: Load system defaults.\n");

    M_LoadDefaults();              // load before initing other systems

    IdentifyVersion();

    // jff 1/24/98 end of set to both working and command line value

    {
        // CPhipps - localise title variable
        // print title for every printed line
        // cph - code cleaned and made smaller
        const char* doomverstr;

        switch ( gamemode ) {
        case retail:
            doomverstr = "The Ultimate DOOM";
            break;
        case shareware:
            doomverstr = "DOOM Shareware";
            break;
        case registered:
            doomverstr = "DOOM Registered";
            break;
        case commercial:  // Ty 08/27/98 - fixed gamemode vs gamemission
            switch (gamemission)
            {
            case pack_plut:
                doomverstr = "DOOM 2: Plutonia Experiment";
                break;
            case pack_tnt:
                doomverstr = "DOOM 2: TNT - Evilution";
                break;
            default:
                doomverstr = "DOOM 2: Hell on Earth";
                break;
            }
            break;
        default:
            doomverstr = "Public DOOM";
            break;
        }

        /* cphipps - the main display. This shows the build date, copyright, and game type */
        lprintf(LO_ALWAYS,"PrBoom (built %s), playing: %s\n"
                          "PrBoom is released under the GNU General Public license v2.0.\n"
                          "You are welcome to redistribute it under certain conditions.\n"
                          "It comes with ABSOLUTELY NO WARRANTY. See the file COPYING for details.\n",
                version_date, doomverstr);
    }

    // init subsystems

    G_ReloadDefaults();    // killough 3/4/98: set defaults just loaded.
    // jff 3/24/98 this sets startskill if it was -1

    I_CalculateRes(240, 160);


    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"V_Init: allocate screens.\n");
    V_Init();

    // CPhipps - move up netgame init
    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"D_InitNetGame: Checking for network game.\n");
    D_InitNetGame();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"W_Init: Init WADfiles.\n");
    W_Init(); // CPhipps - handling of wadfiles init changed

    lprintf(LO_INFO,"\n");     // killough 3/6/98: add a newline, by popular demand :)

    V_InitColorTranslation(); //jff 4/24/98 load color translation lumps

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"M_Init: Init miscellaneous info.\n");
    M_Init();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"R_Init: Init DOOM refresh daemon - ");
    R_Init();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"\nP_Init: Init Playloop state.\n");
    P_Init();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"I_Init: Setting up machine state.\n");
    I_Init();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"S_Init: Setting up sound.\n");
    S_Init(snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/ );

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"HU_Init: Setting up heads up display.\n");
    HU_Init();

    I_InitGraphics();

    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"ST_Init: Init status bar.\n");
    ST_Init();

    idmusnum = -1; //jff 3/17/98 insure idmus number is blank

    if (timedemo)
    {
        singletics = true;
        timingdemo = true;            // show stats after quit
        G_DeferedPlayDemo(timedemo);
        singledemo = true;            // quit after one demo
    }

    D_StartTitle();                 // start up intro loop
}

//
// D_DoomMain
//

void D_DoomMain(void)
{
    D_DoomMainSetup(); // CPhipps - setup out of main execution stack

    D_DoomLoop ();  // never returns
}

//
// GetFirstMap
//
// Ty 08/29/98 - determine first available map from the loaded wads and run it
//

void GetFirstMap(int *ep, int *map)
{
    int i,j; // used to generate map name
    boolean done = false;  // Ty 09/13/98 - to exit inner loops
    char test[6];  // MAPxx or ExMx plus terminator for testing
    char name[6];  // MAPxx or ExMx plus terminator for display
    boolean newlevel = false;  // Ty 10/04/98 - to test for new level
    int ix;  // index for lookup

    strcpy(name,""); // initialize
    if (*map == 0) // unknown so go search for first changed one
    {
        *ep = 1;
        *map = 1; // default E1M1 or MAP01
        if (gamemode == commercial)
        {
            for (i=1;!done && i<33;i++)  // Ty 09/13/98 - add use of !done
            {
                sprintf(test,"MAP%02d",i);
                ix = W_CheckNumForName(test);
                if (ix != -1)  // Ty 10/04/98 avoid -1 subscript
                {
                    if (lumpinfo[ix].source == source_pwad)
                    {
                        *map = i;
                        strcpy(name,test);  // Ty 10/04/98
                        done = true;  // Ty 09/13/98
                        newlevel = true; // Ty 10/04/98
                    }
                    else
                    {
                        if (!*name)  // found one, not pwad.  First default.
                            strcpy(name,test);
                    }
                }
            }
        }
        else // one of the others
        {
            strcpy(name,"E1M1");  // Ty 10/04/98 - default for display
            for (i=1;!done && i<5;i++)  // Ty 09/13/98 - add use of !done
            {
                for (j=1;!done && j<10;j++)  // Ty 09/13/98 - add use of !done
                {
                    sprintf(test,"E%dM%d",i,j);
                    ix = W_CheckNumForName(test);
                    if (ix != -1)  // Ty 10/04/98 avoid -1 subscript
                    {
                        if (lumpinfo[ix].source == source_pwad)
                        {
                            *ep = i;
                            *map = j;
                            strcpy(name,test); // Ty 10/04/98
                            done = true;  // Ty 09/13/98
                            newlevel = true; // Ty 10/04/98
                        }
                        else
                        {
                            if (!*name)  // found one, not pwad.  First default.
                                strcpy(name,test);
                        }
                    }
                }
            }
        }
        //jff 9/3/98 use logical output routine
        lprintf(LO_CONFIRM,"Auto-warping to first %slevel: %s\n",
                newlevel ? "new " : "", name);  // Ty 10/04/98 - new level test
    }
}
