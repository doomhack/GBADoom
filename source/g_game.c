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
 * DESCRIPTION:  none
 *  The original Doom description was none, basically because this file
 *  has everything. This ties up the game logic, linking the menu and
 *  input code to the underlying game by creating & respawning players,
 *  building game tics, calling the underlying thing logic.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "p_map.h"
#include "p_checksum.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "p_map.h"
#include "s_sound.h"
#include "dstrings.h"
#include "sounds.h"
#include "r_data.h"
#include "r_sky.h"
#include "p_inter.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_main.h"
#include "i_system.h"
#include "r_demo.h"

#include "global_data.h"

#define SAVEGAMESIZE  0x20000
#define SAVESTRINGSIZE  24

static size_t   savegamesize = SAVEGAMESIZE; // killough
static boolean  netdemo;
static const byte *demobuffer;   /* cph - only used for playback */
static int demolength; // check for overrun (missing DEMOMARKER)
static FILE    *demofp; /* cph - record straight to file */
static const byte *demo_p;
static short    consistancy[MAXPLAYERS][BACKUPTICS];

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
boolean         respawnmonsters;
int             gameepisode;
int             gamemap;
boolean         paused;
// CPhipps - moved *_loadgame vars here
static boolean forced_loadgame = false;
static boolean command_loadgame = false;

boolean         usergame;      // ok to save / end game
boolean         timingdemo;    // if true, exit with report on completion
boolean         fastdemo;      // if true, run at full speed -- killough
boolean         nodrawers;     // for comparative timing purposes
boolean         noblit;        // for comparative timing purposes
int             starttime;     // for comparative timing purposes
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
const int       consoleplayer = 0; // player taking events and displaying
const int       displayplayer = 0; // view being displayed
int             gametic;
int             basetic;       /* killough 9/29/98: for demo sync */
int             totalkills, totallive, totalitems, totalsecret;    // for intermission
boolean         demorecording;
boolean         demoplayback;
int             demover;
boolean         singledemo;           // quit after playing a demo from cmdline
wbstartstruct_t wminfo;               // parms for world map / intermission
boolean         haswolflevels = false;// jff 4/18/98 wolf levels present
static byte     *savebuffer;          // CPhipps - static
int             totalleveltimes;      // CPhipps - total time for all completed levels
int		longtics;

//
// controls (have defaults)
//

const int     key_right = KEYD_RIGHTARROW;
const int     key_left = KEYD_LEFTARROW;
const int     key_up = KEYD_UPARROW;
const int     key_down = KEYD_DOWNARROW;
const int     key_menu_right = KEYD_RIGHTARROW;                                      // phares 3/7/98
const int     key_menu_left = KEYD_LEFTARROW;                                       //     |
const int     key_menu_up = KEYD_UPARROW;                                         //     V
const int     key_menu_down = KEYD_DOWNARROW;
const int     key_menu_backspace = KEYD_BACKSPACE;                                  //     ^
const int     key_menu_escape = KEYD_ESCAPE;                                     //     |
const int     key_menu_enter = KEYD_ENTER;                                      // phares 3/7/98
const int     key_strafeleft = ',';
const int     key_straferight = '.';
const int     key_fire = KEYD_RCTRL;
const int     key_use = ' ';
const int     key_strafe = KEYD_RALT;
const int     key_speed = KEYD_RSHIFT;
const int     key_escape = KEYD_ESCAPE;                           // phares 4/13/98
const int     key_backspace = KEYD_BACKSPACE;
const int     key_enter = KEYD_ENTER;
const int     key_map_right = KEYD_RIGHTARROW;
const int     key_map_left = KEYD_LEFTARROW;
const int     key_map_up = KEYD_UPARROW;
const int     key_map_down = KEYD_DOWNARROW;
const int     key_map = KEYD_TAB;
const int     key_map_follow = 'f';
const int     key_map_overlay = 'o'; // cph - map overlay
const int     key_map_rotate = 'r';  // cph - map rotation
const int     key_pause = KEYD_PAUSE;
const int     destination_keys[MAXPLAYERS];
const int     key_weapontoggle = '0';
const int     key_weapon1 = '1';
const int     key_weapon2 = '2';
const int     key_weapon3 = '3';
const int     key_weapon4 = '4';
const int     key_weapon5 = '5';
const int     key_weapon6 = '6';
const int     key_weapon7 = '7';                                                //    ^
const int     key_weapon8 = '8';                                                //    |
const int     key_weapon9 = '9';                                                // phares

#define MAXPLMOVE   (forwardmove[1])
#define TURBOTHRESHOLD  0x32
#define SLOWTURNTICS  6
#define QUICKREVERSE (short)32768 // 180 degree reverse                    // phares
#define NUMKEYS   512

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2]    = {0x18, 0x28};
fixed_t angleturn[3]   = {640, 1280, 320};  // + slow turn

// CPhipps - made lots of key/button state vars static
static boolean gamekeydown[NUMKEYS];
static int     turnheld;       // for accelerative turning

// Game events info
static buttoncode_t special_event; // Event triggered by local player, to send
static byte  savegameslot;         // Slot to load if gameaction == ga_loadgame
char         savedescription[SAVEDESCLEN];  // Description to save in savegame if gameaction == ga_savegame

// killough 2/8/98: make corpse queue variable in size
int    bodyqueslot;
const int bodyquesize = 32;        // killough 2/8/98
mobj_t **bodyque = 0;                   // phares 8/10/98

static void G_DoSaveGame (boolean menu);
static const byte* G_ReadDemoHeader(const byte* demo_p, size_t size, boolean failonerror);

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
static inline signed char fudgef(signed char b)
{
  static int c;
  if (!b || longtics) return b;
  if (++c & 0x1f) return b;
  b |= 1; if (b>2) b-=2;
  return b;
}

static inline signed short fudgea(signed short b)
{
  if (!b || !longtics) return b;
  b |= 1; if (b>2) b-=2;
  return b;
}


void G_BuildTiccmd(ticcmd_t* cmd)
{
  boolean strafe;
  boolean bstrafe;
  int speed;
  int tspeed;
  int forward;
  int side;
  int newweapon;                                          // phares
  /* cphipps - remove needless I_BaseTiccmd call, just set the ticcmd to zero */
  memset(cmd,0,sizeof*cmd);
  cmd->consistancy = consistancy[consoleplayer][_g->maketic%BACKUPTICS];

  strafe = gamekeydown[key_strafe];

  speed = (gamekeydown[key_speed]); // phares

  forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
  if (gamekeydown[key_right] || gamekeydown[key_left])
    turnheld ++;
  else
    turnheld = 0;

  if (turnheld < SLOWTURNTICS)
    tspeed = 2;             // slow turn
  else
    tspeed = speed;

  // let movement keys cancel each other out

  if (strafe)
    {
      if (gamekeydown[key_right])
        side += sidemove[speed];
      if (gamekeydown[key_left])
        side -= sidemove[speed];
    }
  else
    {
      if (gamekeydown[key_right])
        cmd->angleturn -= angleturn[tspeed];
      if (gamekeydown[key_left])
        cmd->angleturn += angleturn[tspeed];
    }

  if (gamekeydown[key_up])
    forward += forwardmove[speed];
  if (gamekeydown[key_down])
    forward -= forwardmove[speed];
  if (gamekeydown[key_straferight])
    side += sidemove[speed];
  if (gamekeydown[key_strafeleft])
    side -= sidemove[speed];

    // buttons
  cmd->chatchar = HU_dequeueChatChar();

  if (gamekeydown[key_fire])
    cmd->buttons |= BT_ATTACK;

  if (gamekeydown[key_use])
    {
      cmd->buttons |= BT_USE;
    }

  // Toggle between the top 2 favorite weapons.                   // phares
  // If not currently aiming one of these, switch to              // phares
  // the favorite. Only switch if you possess the weapon.         // phares

  // killough 3/22/98:
  //
  // Perform automatic weapons switch here rather than in p_pspr.c,
  // except in demo_compatibility mode.
  //
  // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

  if ((players[consoleplayer].attackdown && // killough
       !P_CheckAmmo(&players[consoleplayer])) || gamekeydown[key_weapontoggle])
    newweapon = P_SwitchWeapon(&players[consoleplayer]);           // phares
  else
    {                                 // phares 02/26/98: Added gamemode checks
      newweapon =
        gamekeydown[key_weapon1] ? wp_fist :    // killough 5/2/98: reformatted
        gamekeydown[key_weapon2] ? wp_pistol :
        gamekeydown[key_weapon3] ? wp_shotgun :
        gamekeydown[key_weapon4] ? wp_chaingun :
        gamekeydown[key_weapon5] ? wp_missile :
        gamekeydown[key_weapon6] && _g->gamemode != shareware ? wp_plasma :
        gamekeydown[key_weapon7] && _g->gamemode != shareware ? wp_bfg :
        gamekeydown[key_weapon8] ? wp_chainsaw :
        (gamekeydown[key_weapon9] && _g->gamemode == commercial) ? wp_supershotgun :
        wp_nochange;

      // killough 3/22/98: For network and demo consistency with the
      // new weapons preferences, we must do the weapons switches here
      // instead of in p_user.c. But for old demos we must do it in
      // p_user.c according to the old rules. Therefore demo_compatibility
      // determines where the weapons switch is made.

      // killough 2/8/98:
      // Allow user to switch to fist even if they have chainsaw.
      // Switch to fist or chainsaw based on preferences.
      // Switch to shotgun or SSG based on preferences.

        {
          const player_t *player = &players[consoleplayer];

          // only select chainsaw from '1' if it's owned, it's
          // not already in use, and the player prefers it or
          // the fist is already in use, or the player does not
          // have the berserker strength.

          if (newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
              player->readyweapon!=wp_chainsaw &&
              (player->readyweapon==wp_fist ||
               !player->powers[pw_strength] ||
               P_WeaponPreferred(wp_chainsaw, wp_fist)))
            newweapon = wp_chainsaw;

          // Select SSG from '3' only if it's owned and the player
          // does not have a shotgun, or if the shotgun is already
          // in use, or if the SSG is not already in use and the
          // player prefers it.

          if (newweapon == wp_shotgun && _g->gamemode == commercial &&
              player->weaponowned[wp_supershotgun] &&
              (!player->weaponowned[wp_shotgun] ||
               player->readyweapon == wp_shotgun ||
               (player->readyweapon != wp_supershotgun &&
                P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
            newweapon = wp_supershotgun;
        }
      // killough 2/8/98, 3/22/98 -- end of weapon selection changes
    }

  if (newweapon != wp_nochange)
    {
      cmd->buttons |= BT_CHANGE;
      cmd->buttons |= newweapon<<BT_WEAPONSHIFT;
    }

  if (forward > MAXPLMOVE)
    forward = MAXPLMOVE;
  else if (forward < -MAXPLMOVE)
    forward = -MAXPLMOVE;
  if (side > MAXPLMOVE)
    side = MAXPLMOVE;
  else if (side < -MAXPLMOVE)
    side = -MAXPLMOVE;

  cmd->forwardmove += fudgef((signed char)forward);
  cmd->sidemove += side;
  cmd->angleturn = fudgea(cmd->angleturn);

  // CPhipps - special events (game new/load/save/pause)
  if (special_event & BT_SPECIAL) {
    cmd->buttons = special_event;
    special_event = 0;
  }
}

//
// G_RestartLevel
//

void G_RestartLevel(void)
{
  special_event = BT_SPECIAL | (BTS_RESTARTLEVEL & BT_SPECIALMASK);
}

#include "z_bmalloc.h"
//
// G_DoLoadLevel
//

static void G_DoLoadLevel (void)
{
  int i;

  // Set the sky map.
  // First thing, we have a dummy sky texture name,
  //  a flat. The data is in the WAD only because
  //  we look for an actual index, instead of simply
  //  setting one.

  skyflatnum = R_FlatNumForName ( SKYFLATNAME );

  // DOOM determines the sky texture to be used
  // depending on the current episode, and the game version.
  if (_g->gamemode == commercial)
    // || gamemode == pack_tnt   //jff 3/27/98 sorry guys pack_tnt,pack_plut
    // || gamemode == pack_plut) //aren't gamemodes, this was matching retail
    {
      skytexture = R_TextureNumForName ("SKY3");
      if (gamemap < 12)
        skytexture = R_TextureNumForName ("SKY1");
      else
        if (gamemap < 21)
          skytexture = R_TextureNumForName ("SKY2");
    }
  else //jff 3/27/98 and lets not forget about DOOM and Ultimate DOOM huh?
    switch (gameepisode)
      {
      case 1:
        skytexture = R_TextureNumForName ("SKY1");
        break;
      case 2:
        skytexture = R_TextureNumForName ("SKY2");
        break;
      case 3:
        skytexture = R_TextureNumForName ("SKY3");
        break;
      case 4: // Special Edition sky
        skytexture = R_TextureNumForName ("SKY4");
        break;
      }//jff 3/27/98 end sky setting fix

  /* cph 2006/07/31 - took out unused levelstarttic variable */

  if (_g->wipegamestate == GS_LEVEL)
    _g->wipegamestate = -1;             // force a wipe

  gamestate = GS_LEVEL;

  for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (playeringame[i] && players[i].playerstate == PST_DEAD)
        players[i].playerstate = PST_REBORN;
      memset (players[i].frags,0,sizeof(players[i].frags));
    }

  // initialize the msecnode_t freelist.                     phares 3/25/98
  // any nodes in the freelist are gone by now, cleared
  // by Z_FreeTags() when the previous level ended or player
  // died.

  {
    DECLARE_BLOCK_MEMORY_ALLOC_ZONE(secnodezone);
    NULL_BLOCK_MEMORY_ALLOC_ZONE(secnodezone);
      //extern msecnode_t *headsecnode; // phares 3/25/98
      //headsecnode = NULL;
  }

  P_SetupLevel (gameepisode, gamemap, 0, gameskill);

  gameaction = ga_nothing;
  Z_CheckHeap ();

  // clear cmd building stuff
  memset (gamekeydown, 0, sizeof(gamekeydown));

  special_event = 0; paused = false;

  // killough 5/13/98: in case netdemo has consoleplayer other than green
  ST_Start();
  HU_Start();
}


//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//

boolean G_Responder (event_t* ev)
{
  // any other key pops up menu if in demos
  //
  // killough 8/2/98: enable automap in -timedemo demos
  //
  // killough 9/29/98: make any key pop up menu regardless of
  // which kind of demo, and allow other events during playback

  if (gameaction == ga_nothing && (demoplayback || gamestate == GS_DEMOSCREEN))
    {
      // killough 9/29/98: allow user to pause demos during playback
      if (ev->type == ev_keydown && ev->data1 == key_pause)
  {
    if (paused ^= 2)
      S_PauseSound();
    else
      S_ResumeSound();
    return true;
  }

      // killough 10/98:
      // Don't pop up menu, if paused in middle
      // of demo playback, or if automap active.
      // Don't suck up keys, which may be cheats

      return gamestate == GS_DEMOSCREEN &&
  !(paused & 2) && !(_g->automapmode & am_active) &&
  ((ev->type == ev_keydown) ||
   (ev->type == ev_mouse && ev->data1) ||
   (ev->type == ev_joystick && ev->data1)) ?
  M_StartControlPanel(), true : false;
    }

  if (gamestate == GS_FINALE && F_Responder(ev))
    return true;  // finale ate the event

  switch (ev->type)
    {
    case ev_keydown:
      if (ev->data1 == key_pause)           // phares
        {
          special_event = BT_SPECIAL | (BTS_PAUSE & BT_SPECIALMASK);
          return true;
        }
      if (ev->data1 <NUMKEYS)
        gamekeydown[ev->data1] = true;
      return true;    // eat key down events

    case ev_keyup:
      if (ev->data1 <NUMKEYS)
        gamekeydown[ev->data1] = false;
      return false;   // always let key up events filter down

    default:
      break;
    }
  return false;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//

void G_Ticker (void)
{
  int i;
  static gamestate_t prevgamestate;

  P_MapStart();
  // do player reborns if needed
  for (i=0 ; i<MAXPLAYERS ; i++)
    if (playeringame[i] && players[i].playerstate == PST_REBORN)
      G_DoReborn (i);
  P_MapEnd();

  // do things to change the game state
  while (gameaction != ga_nothing)
    {
      switch (gameaction)
        {
        case ga_loadlevel:
    // force players to be initialized on level reload
    for (i=0 ; i<MAXPLAYERS ; i++)
      players[i].playerstate = PST_REBORN;
          G_DoLoadLevel ();
          break;
        case ga_newgame:
          G_DoNewGame ();
          break;
        case ga_loadgame:
          G_DoLoadGame ();
          break;
        case ga_savegame:
          G_DoSaveGame (false);
          break;
        case ga_playdemo:
          G_DoPlayDemo ();
          break;
        case ga_completed:
          G_DoCompleted ();
          break;
        case ga_victory:
          F_StartFinale ();
          break;
        case ga_worlddone:
          G_DoWorldDone ();
          break;
        case ga_nothing:
          break;
        }
    }

  if (paused & 2 || (!demoplayback && menuactive))
    basetic++;  // For revenant tracers and RNG -- we must maintain sync
  else {
    // get commands, check consistancy, and build new consistancy check
    int buf = (gametic)%BACKUPTICS;

    for (i=0 ; i<MAXPLAYERS ; i++) {
      if (playeringame[i])
        {
          ticcmd_t *cmd = &players[i].cmd;

          memcpy(cmd, &_g->netcmds[i][buf], sizeof *cmd);

          if (demoplayback)
            G_ReadDemoTiccmd (cmd);
          if (demorecording)
            G_WriteDemoTiccmd (cmd);

          // check for turbo cheats
          // killough 2/14/98, 2/20/98 -- only warn in netgames and demos

          if ((demoplayback) && cmd->forwardmove > TURBOTHRESHOLD &&
              !(gametic&31) && ((gametic>>5)&3) == i )
            {
        extern char *player_names[];
        /* cph - don't use sprintf, use doom_printf */
              doom_printf ("%s is turbo!", player_names[i]);
            }
        }
    }

    // check for special buttons
    for (i=0; i<MAXPLAYERS; i++) {
      if (playeringame[i])
        {
          if (players[i].cmd.buttons & BT_SPECIAL)
            {
              switch (players[i].cmd.buttons & BT_SPECIALMASK)
                {
                case BTS_PAUSE:
                  paused ^= 1;
                  if (paused)
                    S_PauseSound ();
                  else
                    S_ResumeSound ();
                  break;

                case BTS_SAVEGAME:
                  if (!savedescription[0])
                    strcpy(savedescription, "NET GAME");
                  savegameslot =
                    (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
                  gameaction = ga_savegame;
                  break;

      // CPhipps - remote loadgame request
                case BTS_LOADGAME:
                  savegameslot =
                    (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
                  gameaction = ga_loadgame;
      forced_loadgame = false; // Force if a netgame
      command_loadgame = false;
                  break;

      // CPhipps - Restart the level
    case BTS_RESTARTLEVEL:
                  if (demoplayback)
                    break;     // CPhipps - Ignore in demos or old games
      gameaction = ga_loadlevel;
      break;
                }
        players[i].cmd.buttons = 0;
            }
        }
    }
  }

  // cph - if the gamestate changed, we may need to clean up the old gamestate
  if (gamestate != prevgamestate) {
    switch (prevgamestate) {
    case GS_LEVEL:
      // This causes crashes at level end - Neil Stevens
      // The crash is because the sounds aren't stopped before freeing them
      // the following is a possible fix
      // This fix does avoid the crash wowever, with this fix in, the exit
      // switch sound is cut off
      // S_Stop();
      // Z_FreeTags(PU_LEVEL, PU_PURGELEVEL-1);
      break;
    case GS_INTERMISSION:
      WI_End();
    default:
      break;
    }
    prevgamestate = gamestate;
  }

  // e6y
  // do nothing if a pause has been pressed during playback
  // pausing during intermission can cause desynchs without that
  if (paused & 2 && gamestate != GS_LEVEL)
    return;

  // do main actions
  switch (gamestate)
    {
    case GS_LEVEL:
      P_Ticker ();
      ST_Ticker ();
      AM_Ticker ();
      HU_Ticker ();
      break;

    case GS_INTERMISSION:
      WI_Ticker ();
      break;

    case GS_FINALE:
      F_Ticker ();
      break;

    case GS_DEMOSCREEN:
      D_PageTicker ();
      break;
    }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//

static void G_PlayerFinishLevel(int player)
{
  player_t *p = &players[player];
  memset(p->powers, 0, sizeof p->powers);
  memset(p->cards, 0, sizeof p->cards);
  p->mo = NULL;           // cph - this is allocated PU_LEVEL so it's gone
  p->extralight = 0;      // cancel gun flashes
  p->fixedcolormap = 0;   // cancel ir gogles
  p->damagecount = 0;     // no palette changes
  p->bonuscount = 0;
}

// CPhipps - G_SetPlayerColour
// Player colours stuff
//
// G_SetPlayerColour

#include "r_draw.h"

void G_ChangedPlayerColour(int pn, int cl)
{

}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//

void G_PlayerReborn (int player)
{
  player_t *p;
  int i;
  int frags[MAXPLAYERS];
  int killcount;
  int itemcount;
  int secretcount;

  memcpy (frags, players[player].frags, sizeof frags);
  killcount = players[player].killcount;
  itemcount = players[player].itemcount;
  secretcount = players[player].secretcount;

  p = &players[player];

  // killough 3/10/98,3/21/98: preserve cheats across idclev
  {
    int cheats = p->cheats;
    memset (p, 0, sizeof(*p));
    p->cheats = cheats;
  }

  memcpy(players[player].frags, frags, sizeof(players[player].frags));
  players[player].killcount = killcount;
  players[player].itemcount = itemcount;
  players[player].secretcount = secretcount;

  p->usedown = p->attackdown = true;  // don't do anything immediately
  p->playerstate = PST_LIVE;
  p->health = initial_health;  // Ty 03/12/98 - use dehacked values
  p->readyweapon = p->pendingweapon = wp_pistol;
  p->weaponowned[wp_fist] = true;
  p->weaponowned[wp_pistol] = true;
  p->ammo[am_clip] = initial_bullets; // Ty 03/12/98 - use dehacked values

  for (i=0 ; i<NUMAMMO ; i++)
    p->maxammo[i] = maxammo[i];
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//

static boolean G_CheckSpot(int playernum, mapthing_t *mthing)
{
  fixed_t     x,y;
  subsector_t *ss;
  int         i;

  if (!players[playernum].mo)
    {
      // first spawn of level, before corpses
      for (i=0 ; i<playernum ; i++)
        if (players[i].mo->x == mthing->x << FRACBITS
            && players[i].mo->y == mthing->y << FRACBITS)
          return false;
      return true;
    }

  x = mthing->x << FRACBITS;
  y = mthing->y << FRACBITS;

  // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
  // corpse to detect collisions with other players in DM starts
  //
  // Old code:
  // if (!P_CheckPosition (players[playernum].mo, x, y))
  //    return false;

  players[playernum].mo->flags |=  MF_SOLID;
  i = P_CheckPosition(players[playernum].mo, x, y);
  players[playernum].mo->flags &= ~MF_SOLID;
  if (!i)
    return false;

  // flush an old corpse if needed
  // killough 2/8/98: make corpse queue have an adjustable limit
  // killough 8/1/98: Fix bugs causing strange crashes

  if (bodyquesize > 0)
    {
      static int queuesize;
      if (queuesize < bodyquesize)
	{
	  bodyque = realloc(bodyque, bodyquesize*sizeof*bodyque);
	  memset(bodyque+queuesize, 0,
		 (bodyquesize-queuesize)*sizeof*bodyque);
	  queuesize = bodyquesize;
	}
      if (bodyqueslot >= bodyquesize)
	P_RemoveMobj(bodyque[bodyqueslot % bodyquesize]);
      bodyque[bodyqueslot++ % bodyquesize] = players[playernum].mo;
    }
  else
    if (!bodyquesize)
      P_RemoveMobj(players[playernum].mo);

  // spawn a teleport fog
  ss = R_PointInSubsector (x,y);
  { // Teleport fog at respawn point
    fixed_t xa,ya;
    int an;
    mobj_t      *mo;

/* BUG: an can end up negative, because mthing->angle is (signed) short.
 * We have to emulate original Doom's behaviour, deferencing past the start
 * of the array, into the previous array (finetangent) */
    an = ( ANG45 * ((signed)mthing->angle/45) ) >> ANGLETOFINESHIFT;
    xa = finecosine[an];
    ya = finesine[an];

    mo = P_SpawnMobj(x+20*xa, y+20*ya, ss->sector->floorheight, MT_TFOG);

    if (players[consoleplayer].viewz != 1)
      S_StartSound(mo, sfx_telept);  // don't start sound on first frame
  }

  return true;
}


// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer (int playernum)
{
  int j, selections = deathmatch_p - deathmatchstarts;

  if (selections < MAXPLAYERS)
    I_Error("G_DeathMatchSpawnPlayer: Only %i deathmatch spots, %d required",
    selections, MAXPLAYERS);

  for (j=0 ; j<20 ; j++)
    {
      int i = P_Random(pr_dmspawn) % selections;
      if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
        {
          deathmatchstarts[i].type = playernum+1;
          P_SpawnPlayer (playernum, &deathmatchstarts[i]);
          return;
        }
    }

  // no good spot, so the player will probably get stuck
  P_SpawnPlayer (playernum, &playerstarts[playernum]);
}

//
// G_DoReborn
//

void G_DoReborn (int playernum)
{
    gameaction = ga_loadlevel;      // reload the level from scratch
}

// DOOM Par Times
const int pars[4][10] = {
  {0},
  {0,30,75,120,90,165,180,180,30,165},
  {0,90,90,90,120,90,360,240,30,170},
  {0,90,45,90,150,90,90,165,30,135}
};

// DOOM II Par Times
const int cpars[32] = {
  30,90,120,120,90,150,120,120,270,90,  //  1-10
  210,150,150,150,210,150,420,150,210,150,  // 11-20
  240,150,180,150,150,300,330,420,300,180,  // 21-30
  120,30          // 31-32
};

static boolean secretexit;

void G_ExitLevel (void)
{
  secretexit = false;
  gameaction = ga_completed;
}

// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!

void G_SecretExitLevel (void)
{
  if (_g->gamemode!=commercial || haswolflevels)
    secretexit = true;
  else
    secretexit = false;
  gameaction = ga_completed;
}

//
// G_DoCompleted
//

void G_DoCompleted (void)
{
  int i;

  gameaction = ga_nothing;

  for (i=0; i<MAXPLAYERS; i++)
    if (playeringame[i])
      G_PlayerFinishLevel(i);        // take away cards and stuff

  if (_g->automapmode & am_active)
    AM_Stop();

  if (_g->gamemode != commercial) // kilough 2/7/98
    switch(gamemap)
      {
  // cph - Remove ExM8 special case, so it gets summary screen displayed
      case 9:
        for (i=0 ; i<MAXPLAYERS ; i++)
          players[i].didsecret = true;
        break;
      }

  wminfo.didsecret = players[consoleplayer].didsecret;
  wminfo.epsd = gameepisode -1;
  wminfo.last = gamemap -1;

  // wminfo.next is 0 biased, unlike gamemap
  if (_g->gamemode == commercial)
    {
      if (secretexit)
        switch(gamemap)
          {
          case 15:
            wminfo.next = 30; break;
          case 31:
            wminfo.next = 31; break;
          }
      else
        switch(gamemap)
          {
          case 31:
          case 32:
            wminfo.next = 15; break;
          default:
            wminfo.next = gamemap;
          }
    }
  else
    {
      if (secretexit)
        wminfo.next = 8;  // go to secret level
      else
        if (gamemap == 9)
          {
            // returning from secret level
            switch (gameepisode)
              {
              case 1:
                wminfo.next = 3;
                break;
              case 2:
                wminfo.next = 5;
                break;
              case 3:
                wminfo.next = 6;
                break;
              case 4:
                wminfo.next = 2;
                break;
              }
          }
        else
          wminfo.next = gamemap;          // go to next level
    }

  wminfo.maxkills = totalkills;
  wminfo.maxitems = totalitems;
  wminfo.maxsecret = totalsecret;
  wminfo.maxfrags = 0;

  if ( _g->gamemode == commercial )
    wminfo.partime = TICRATE*cpars[gamemap-1];
  else
    wminfo.partime = TICRATE*pars[gameepisode][gamemap];

  wminfo.pnum = consoleplayer;

  for (i=0 ; i<MAXPLAYERS ; i++)
    {
      wminfo.plyr[i].in = playeringame[i];
      wminfo.plyr[i].skills = players[i].killcount;
      wminfo.plyr[i].sitems = players[i].itemcount;
      wminfo.plyr[i].ssecret = players[i].secretcount;
      wminfo.plyr[i].stime = leveltime;
      memcpy (wminfo.plyr[i].frags, players[i].frags,
              sizeof(wminfo.plyr[i].frags));
    }

  /* cph - modified so that only whole seconds are added to the totalleveltimes
   *  value; so our total is compatible with the "naive" total of just adding
   *  the times in seconds shown for each level. Also means our total time
   *  will agree with Compet-n.
   */
  wminfo.totaltimes = (totalleveltimes += (leveltime - leveltime%35));

  gamestate = GS_INTERMISSION;
  _g->automapmode &= ~am_active;

  // lmpwatch.pl engine-side demo testing support
  // print "FINISHED: <mapname>" when the player exits the current map
  if (nodrawers && (demoplayback || timingdemo)) {
    if (_g->gamemode == commercial)
      lprintf(LO_INFO, "FINISHED: MAP%02d\n", gamemap);
    else
      lprintf(LO_INFO, "FINISHED: E%dM%d\n", gameepisode, gamemap);
  }

  WI_Start (&wminfo);
}

//
// G_WorldDone
//

void G_WorldDone (void)
{
  gameaction = ga_worlddone;

  if (secretexit)
    players[consoleplayer].didsecret = true;

  if (_g->gamemode == commercial)
    {
      switch (gamemap)
        {
        case 15:
        case 31:
          if (!secretexit)
            break;
        case 6:
        case 11:
        case 20:
        case 30:
          F_StartFinale ();
          break;
        }
    }
  else if (gamemap == 8)
    gameaction = ga_victory; // cph - after ExM8 summary screen, show victory stuff
}

void G_DoWorldDone (void)
{
  idmusnum = -1;             //jff 3/17/98 allow new level's music to be loaded
  gamestate = GS_LEVEL;
  gamemap = wminfo.next+1;
  G_DoLoadLevel();
  gameaction = ga_nothing;
  AM_clearMarks();           //jff 4/12/98 clear any marks on the automap
}

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 32

extern boolean setsizeneeded;

//CPhipps - savename variable redundant

/* killough 12/98:
 * This function returns a signature for the current wad.
 * It is used to distinguish between wads, for the purposes
 * of savegame compatibility warnings, and options lookups.
 */

static uint_64_t G_UpdateSignature(uint_64_t s, const char *name)
{
  int i, lump = W_CheckNumForName(name);
  if (lump != -1 && (i = lump+10) < numlumps)
    do
      {
  int size = W_LumpLength(i);
  const byte *p = W_CacheLumpNum(i);
  while (size--)
    s <<= 1, s += *p++;
  W_UnlockLumpNum(i);
      }
    while (--i > lump);
  return s;
}

static uint_64_t G_Signature(void)
{
  static uint_64_t s = 0;
  static boolean computed = false;
  char name[9];
  int episode, map;

  if (!computed) {
   computed = true;
   if (_g->gamemode == commercial)
    for (map = haswolflevels ? 32 : 30; map; map--)
      sprintf(name, "map%02d", map), s = G_UpdateSignature(s, name);
   else
    for (episode = _g->gamemode==retail ? 4 :
     _g->gamemode==shareware ? 1 : 3; episode; episode--)
      for (map = 9; map; map--)
  sprintf(name, "E%dM%d", episode, map), s = G_UpdateSignature(s, name);
  }
  return s;
}

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

void G_ForcedLoadGame(void)
{
  // CPhipps - net loadgames are always forced, so we only reach here
  //  in single player
  gameaction = ga_loadgame;
  forced_loadgame = true;
}

// killough 3/16/98: add slot info
// killough 5/15/98: add command-line
void G_LoadGame(int slot, boolean command)
{
  if (!demoplayback && !command) {
    // CPhipps - handle savegame filename in G_DoLoadGame
    //         - Delay load so it can be communicated in net game
    //         - store info in special_event
    special_event = BT_SPECIAL | (BTS_LOADGAME & BT_SPECIALMASK) |
      ((slot << BTS_SAVESHIFT) & BTS_SAVEMASK);
    forced_loadgame = false; // CPhipps - always force load netgames
  } else {
    // Do the old thing, immediate load
    gameaction = ga_loadgame;
    forced_loadgame = false;
    savegameslot = slot;
    demoplayback = false;
  }
  command_loadgame = command;
  R_SmoothPlaying_Reset(NULL); // e6y
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(const char *msg)
{
  Z_Free(savebuffer);                // Free the savegame buffer
  M_ForcedLoadGame(msg);             // Print message asking for 'Y' to force
  if (command_loadgame)              // If this was a command-line -loadgame
    {
      D_StartTitle();                // Start the title screen
      gamestate = GS_DEMOSCREEN;     // And set the game state accordingly
    }
}

// CPhipps - size of version header
#define VERSIONSIZE   16

void G_DoLoadGame(void)
{
  int  length, i;
  // CPhipps - do savegame filename stuff here
  char name[PATH_MAX+1];     // killough 3/22/98

  G_SaveGameName(name,sizeof(name),savegameslot, demoplayback);

  gameaction = ga_nothing;

  length = M_ReadFile(name, &savebuffer);
  if (length<=0)
    I_Error("Couldn't read file %s: %s", name, "(Unknown Error)");
  save_p = savebuffer + SAVESTRINGSIZE;


  save_p += VERSIONSIZE;

  // CPhipps - always check savegames even when forced,
  //  only print a warning if forced
  {  // killough 3/16/98: check lump name checksum (independent of order)
    uint_64_t checksum = 0;

    checksum = G_Signature();

    if (memcmp(&checksum, save_p, sizeof checksum)) {
      if (!forced_loadgame) {
        char *msg = malloc(strlen(save_p + sizeof checksum) + 128);
        strcpy(msg,"Incompatible Savegame!!!\n");
        if (save_p[sizeof checksum])
          strcat(strcat(msg,"Wads expected:\n\n"), save_p + sizeof checksum);
        strcat(msg, "\nAre you sure?");
        G_LoadGameErr(msg);
        free(msg);
        return;
      } else
  lprintf(LO_WARN, "G_DoLoadGame: Incompatible savegame\n");
    }
    save_p += sizeof checksum;
   }

  save_p += strlen(save_p)+1;

  save_p++;

  gameskill = *save_p++;
  gameepisode = *save_p++;
  gamemap = *save_p++;

  for (i=0 ; i<MAXPLAYERS ; i++)
    playeringame[i] = *save_p++;
  save_p += MIN_MAXPLAYERS-MAXPLAYERS;         // killough 2/28/98

  idmusnum = *save_p++;           // jff 3/17/98 restore idmus music
  if (idmusnum==255) idmusnum=-1; // jff 3/18/98 account for unsigned byte

  /* killough 3/1/98: Read game options
   * killough 11/98: move down to here
   */
  save_p = (char*)G_ReadOptions(save_p);

  // load a base level
  G_InitNew (gameskill, gameepisode, gamemap);

  /* get the times - killough 11/98: save entire word */
  memcpy(&leveltime, save_p, sizeof leveltime);
  save_p += sizeof leveltime;


    memcpy(&totalleveltimes, save_p, sizeof totalleveltimes);
    save_p += sizeof totalleveltimes;


  // killough 11/98: load revenant tracer state
  basetic = gametic - *save_p++;

  // dearchive all the modifications
  P_MapStart();
  P_UnArchivePlayers ();
  P_UnArchiveWorld ();
  P_UnArchiveThinkers ();
  P_UnArchiveSpecials ();
  P_UnArchiveRNG ();    // killough 1/18/98: load RNG information
  P_UnArchiveMap ();    // killough 1/22/98: load automap information
  P_MapEnd();
  R_SmoothPlaying_Reset(NULL); // e6y

  if (*save_p != 0xe6)
    I_Error ("G_DoLoadGame: Bad savegame");

  // done
  Z_Free (savebuffer);

  if (setsizeneeded)
    R_ExecuteSetViewSize ();

  // draw the pattern into the back screen
  R_FillBackScreen ();

  /* killough 12/98: support -recordfrom and -loadgame -playdemo */
  if (!command_loadgame)
    singledemo = false;  /* Clear singledemo flag if loading from menu */
  else
    if (singledemo) {
      gameaction = ga_loadgame; /* Mark that we're loading a game before demo */
      G_DoPlayDemo();           /* This will detect it and won't reinit level */
    } else /* Command line + record means it's a recordfrom */
      if (demorecording)
        G_BeginRecording();
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, char *description)
{
  strcpy(savedescription, description);
  if (demoplayback) {
    /* cph - We're doing a user-initiated save game while a demo is
     * running so, go outside normal mechanisms
     */
    savegameslot = slot;
    G_DoSaveGame(true);
  }
  // CPhipps - store info in special_event
  special_event = BT_SPECIAL | (BTS_SAVEGAME & BT_SPECIALMASK) |
    ((slot << BTS_SAVESHIFT) & BTS_SAVEMASK);

}

// Check for overrun and realloc if necessary -- Lee Killough 1/22/98
void (CheckSaveGame)(size_t size, const char* file, int line)
{
  size_t pos = save_p - savebuffer;

#ifdef RANGECHECK
  /* cph 2006/08/07 - after-the-fact sanity checking of CheckSaveGame calls */
  static size_t prev_check;
  static const char* prevf;
  static int prevl;

  if (pos > prev_check)
    I_Error("CheckSaveGame at %s:%d called for insufficient buffer (%u < %u)", prevf, prevl, prev_check, pos);
  prev_check = size + pos;
  prevf = file;
  prevl = line;
#endif

  size += 1024;  // breathing room
  if (pos+size > savegamesize)
    save_p = (savebuffer = realloc(savebuffer,
           savegamesize += (size+1023) & ~1023)) + pos;
}

/* killough 3/22/98: form savegame name in one location
 * (previously code was scattered around in multiple places)
 * cph - Avoid possible buffer overflow problems by passing
 * size to this function and using snprintf */

void G_SaveGameName(char *name, size_t size, int slot, boolean demoplayback)
{

}

static void G_DoSaveGame (boolean menu)
{
  char name[PATH_MAX+1];
  char name2[VERSIONSIZE];
  char *description;
  int  length, i;

  gameaction = ga_nothing; // cph - cancel savegame at top of this function,
    // in case later problems cause a premature exit

  G_SaveGameName(name,sizeof(name),savegameslot, demoplayback && !menu);

  description = savedescription;

  save_p = savebuffer = malloc(savegamesize);

  CheckSaveGame(SAVESTRINGSIZE+VERSIONSIZE+sizeof(uint_64_t));
  memcpy (save_p, description, SAVESTRINGSIZE);
  save_p += SAVESTRINGSIZE;
  memset (name2,0,sizeof(name2));

  save_p += VERSIONSIZE;

  { /* killough 3/16/98, 12/98: store lump name checksum */
    uint_64_t checksum = G_Signature();
    memcpy(save_p, &checksum, sizeof checksum);
    save_p += sizeof checksum;
  }

  // killough 3/16/98: store pwad filenames in savegame
  {
    // CPhipps - changed for new wadfiles handling
    size_t i;
    for (i = 0; i<numwadfiles; i++)
      {
        const char *const w = wadfiles[i].name;
        CheckSaveGame(strlen(w)+2);
        strcpy(save_p, w);
        save_p += strlen(save_p);
        *save_p++ = '\n';
      }
    *save_p++ = 0;
  }

  CheckSaveGame(GAME_OPTION_SIZE+MIN_MAXPLAYERS+14);

  *save_p++ = -1;

  *save_p++ = gameskill;
  *save_p++ = gameepisode;
  *save_p++ = gamemap;

  for (i=0 ; i<MAXPLAYERS ; i++)
    *save_p++ = playeringame[i];

  for (;i<MIN_MAXPLAYERS;i++)         // killough 2/28/98
    *save_p++ = 0;

  *save_p++ = idmusnum;               // jff 3/17/98 save idmus state

  save_p = G_WriteOptions(save_p);    // killough 3/1/98: save game options

  /* cph - FIXME - endianness? */
  /* killough 11/98: save entire word */
  memcpy(save_p, &leveltime, sizeof leveltime);
  save_p += sizeof leveltime;

    memcpy(save_p, &totalleveltimes, sizeof totalleveltimes);
    save_p += sizeof totalleveltimes;

  // killough 11/98: save revenant tracer state
  *save_p++ = (gametic-basetic) & 255;

  // killough 3/22/98: add Z_CheckHeap after each call to ensure consistency
  Z_CheckHeap();
  P_ArchivePlayers();
  Z_CheckHeap();

  // phares 9/13/98: Move mobj_t->index out of P_ArchiveThinkers so the
  // indices can be used by P_ArchiveWorld when the sectors are saved.
  // This is so we can save the index of the mobj_t of the thinker that
  // caused a sound, referenced by sector_t->soundtarget.
  P_ThinkerToIndex();

  P_ArchiveWorld();
  Z_CheckHeap();
  P_ArchiveThinkers();

  // phares 9/13/98: Move index->mobj_t out of P_ArchiveThinkers, simply
  // for symmetry with the P_ThinkerToIndex call above.

  P_IndexToThinker();

  Z_CheckHeap();
  P_ArchiveSpecials();
  P_ArchiveRNG();    // killough 1/18/98: save RNG information
  Z_CheckHeap();
  P_ArchiveMap();    // killough 1/22/98: save automap information

  *save_p++ = 0xe6;   // consistancy marker

  length = save_p - savebuffer;

  Z_CheckHeap();
  doom_printf( "%s", M_WriteFile(name, savebuffer, length)
         ? GGSAVED /* Ty - externalised */
         : "Game save failed!"); // CPhipps - not externalised

  free(savebuffer);  // killough
  savebuffer = save_p = NULL;

  savedescription[0] = 0;
}

static skill_t d_skill;
static int     d_episode;
static int     d_map;

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
  d_skill = skill;
  d_episode = episode;
  d_map = map;
  gameaction = ga_newgame;
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(void)
{
  // killough 3/1/98: Initialize options based on config file
  // (allows functions above to load different values for demos
  // and savegames without messing up defaults).

  demoplayback = false;
  singledemo = false;            // killough 9/29/98: don't stop after 1 demo
  netdemo = false;

  // killough 2/21/98:
  memset(playeringame+1, 0, sizeof(*playeringame)*(MAXPLAYERS-1));

  rngseed += I_GetRandomTimeSeed() + gametic; // CPhipps
}

void G_DoNewGame (void)
{
  G_ReloadDefaults();            // killough 3/1/98
  G_InitNew (d_skill, d_episode, d_map);
  gameaction = ga_nothing;

  //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
  ST_Start();
}

// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.

void G_SetFastParms(int fast_pending)
{
  static int fast = 0;            // remembers fast state
  int i;
  if (fast != fast_pending) {     /* only change if necessary */
    if ((fast = fast_pending))
      {
        for (i=S_SARG_RUN1; i<=S_SARG_PAIN2; i++)
          if (states[i].tics != 1) // killough 4/10/98
            states[i].tics >>= 1;  // don't change 1->0 since it causes cycles
        mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
      }
    else
      {
        for (i=S_SARG_RUN1; i<=S_SARG_PAIN2; i++)
          states[i].tics <<= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
      }
  }
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

void G_InitNew(skill_t skill, int episode, int map)
{
  int i;

  if (paused)
    {
      paused = false;
      S_ResumeSound();
    }

  if (skill > sk_nightmare)
    skill = sk_nightmare;

  if (episode < 1)
    episode = 1;

  if (_g->gamemode == retail)
    {
      if (episode > 4)
        episode = 4;
    }
  else
    if (_g->gamemode == shareware)
      {
        if (episode > 1)
          episode = 1; // only start episode 1 on shareware
      }
    else
      if (episode > 3)
        episode = 3;

  if (map < 1)
    map = 1;
  if (map > 9 && _g->gamemode != commercial)
    map = 9;

  G_SetFastParms(skill == sk_nightmare);  // killough 4/10/98

  M_ClearRandom();

  respawnmonsters = skill == sk_nightmare;

  // force players to be initialized upon first level load
  for (i=0 ; i<MAXPLAYERS ; i++)
    players[i].playerstate = PST_REBORN;

  usergame = true;                // will be set false if a demo
  paused = false;
  _g->automapmode &= ~am_active;
  gameepisode = episode;
  gamemap = map;
  gameskill = skill;

  totalleveltimes = 0; // cph

  //jff 4/16/98 force marks on automap cleared every new level start
  AM_clearMarks();

  G_DoLoadLevel ();
}

//
// DEMO RECORDING
//

#define DEMOMARKER    0x80

void G_ReadDemoTiccmd (ticcmd_t* cmd)
{
  unsigned char at; // e6y: tasdoom stuff

  if (*demo_p == DEMOMARKER)
    G_CheckDemoStatus();      // end of demo data stream
  else if (demoplayback && demo_p + (longtics?5:4) > demobuffer + demolength)
  {
    lprintf(LO_WARN, "G_ReadDemoTiccmd: missing DEMOMARKER\n");
    G_CheckDemoStatus();
  }
  else
    {
      cmd->forwardmove = ((signed char)*demo_p++);
      cmd->sidemove = ((signed char)*demo_p++);
      if (!longtics) {
        cmd->angleturn = ((unsigned char)(at = *demo_p++))<<8;
      } else {
	unsigned int lowbyte = (unsigned char)*demo_p++;
        cmd->angleturn = (((signed int)(*demo_p++))<<8) + lowbyte;
      }
      cmd->buttons = (unsigned char)*demo_p++;
    }
}

/* Demo limits removed -- killough
 * cph - record straight to file
 */
void G_WriteDemoTiccmd (ticcmd_t* cmd)
{
  char buf[5];
  char *p = buf;

  *p++ = cmd->forwardmove;
  *p++ = cmd->sidemove;
  if (!longtics) {
    *p++ = (cmd->angleturn+128)>>8;
  } else {
    signed short a = cmd->angleturn;
    *p++ = a & 0xff;
    *p++ = (a >> 8) & 0xff;
  }
  *p++ = cmd->buttons;
  if (fwrite(buf, p-buf, 1, demofp) != 1)
    I_Error("G_WriteDemoTiccmd: error writing demo");

  /* cph - alias demo_p to it so we can read it back */
  demo_p = buf;
  G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same
}

//
// G_RecordDemo
//

void G_RecordDemo (const char* name)
{

}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

extern int forceOldBsp;

byte *G_WriteOptions(byte *demo_p)
{
  byte *target = demo_p + GAME_OPTION_SIZE;

  *demo_p++ = 1;  // part of monster AI

  *demo_p++ = 1;  // ice & mud

  *demo_p++ = 0;      // weapon recoil

  *demo_p++ = 1;      // MT_PUSH Things

  *demo_p++ = 0;

  *demo_p++ = 1;  // whether player bobs or not

  // killough 3/6/98: add parameters to savegame, move around some in demos
  *demo_p++ = 0;
  *demo_p++ = 0;
  *demo_p++ = 0;

  *demo_p++ = 0;        // killough 3/31/98

  // killough 3/26/98: Added rngseed. 3/31/98: moved here
  *demo_p++ = (byte)((rngseed >> 24) & 0xff);
  *demo_p++ = (byte)((rngseed >> 16) & 0xff);
  *demo_p++ = (byte)((rngseed >>  8) & 0xff);
  *demo_p++ = (byte)( rngseed        & 0xff);

  // Options new to v2.03 begin here

  *demo_p++ = 1;   // killough 7/19/98

  *demo_p++ = 0;


  *demo_p++ = 0;
  *demo_p++ = 0;

  *demo_p++ = (distfriend >> 8) & 0xff;  // killough 8/8/98
  *demo_p++ =  distfriend       & 0xff;  // killough 8/8/98

  *demo_p++ = 0;         // killough 9/8/98

  *demo_p++ = 1;    // killough 9/9/98

  *demo_p++ = 1;         // killough 10/98

  *demo_p++ = 1;             // killough 9/9/98


  *demo_p++ = 0;


  *demo_p++ = 0;

  *demo_p++ = 0; // cph 2002/07/20

  //----------------
  // Padding at end
  //----------------
  while (demo_p < target)
    *demo_p++ = 0;

  if (demo_p != target)
    I_Error("G_WriteOptions: GAME_OPTION_SIZE is too small");

  return target;
}

/* Same, but read instead of write
 * cph - const byte*'s
 */

const byte *G_ReadOptions(const byte *demo_p)
{
  const byte *target = demo_p + GAME_OPTION_SIZE;

  demo_p++;

  demo_p++;

  demo_p++;

  demo_p++;

  demo_p++;

  demo_p++;

  // killough 3/6/98: add parameters to savegame, move from demo
  demo_p++;
  demo_p++;
  demo_p++;

  demo_p++;              // killough 3/31/98

  // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

  rngseed  = *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;


      demo_p++;   // killough 7/19/98

      demo_p++;

      demo_p += 2;

      demo_p++;      // killough 8/8/98
      demo_p++;

      demo_p++;     // killough 9/8/98

      demo_p++; // killough 9/9/98

      demo_p++;      // killough 10/98

      demo_p++;          // killough 9/9/98


      demo_p++;


      demo_p++;

      forceOldBsp = *demo_p++; // cph 2002/07/20

  return target;
}

void G_BeginRecording (void)
{
  int i;
  byte *demostart, *demo_p;
  demostart = demo_p = malloc(1000);
  longtics = 0;


    { /* Write version code into demo */
      unsigned char v;
      v = 214;
      longtics = 1;

      *demo_p++ = v;
    }

    // signature
    *demo_p++ = 0x1d;
    *demo_p++ = 'M';
    *demo_p++ = 'B';
    *demo_p++ = 'F';
    *demo_p++ = 0xe6;
    *demo_p++ = '\0';

    /* killough 2/22/98: save compatibility flag in new demos
     * cph - FIXME? MBF demos will always be not in compat. mode */
    *demo_p++ = 0;

    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = 0;
    *demo_p++ = consoleplayer;

    demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options

    for (i=0 ; i<MAXPLAYERS ; i++)
      *demo_p++ = playeringame[i];

    // killough 2/28/98:
    // We always store at least MIN_MAXPLAYERS bytes in demo, to
    // support enhancements later w/o losing demo compatibility

    for (; i<MIN_MAXPLAYERS; i++)
      *demo_p++ = 0;




  if (fwrite(demostart, 1, demo_p-demostart, demofp) != (size_t)(demo_p-demostart))
    I_Error("G_BeginRecording: Error writing demo header");
  free(demostart);
}

//
// G_PlayDemo
//

static const char *defdemoname;

void G_DeferedPlayDemo (const char* name)
{
  defdemoname = name;
  gameaction = ga_playdemo;
}

static int demolumpnum = -1;

//e6y: Check for overrun
static boolean CheckForOverrun(const byte *start_p, const byte *current_p, size_t maxsize, size_t size, boolean failonerror)
{
  size_t pos = current_p - start_p;
  if (pos + size > maxsize)
  {
    if (failonerror)
      I_Error("G_ReadDemoHeader: wrong demo header\n");
    else
      return true;
  }
  return false;
}

static const byte* G_ReadDemoHeader(const byte *demo_p, size_t size, boolean failonerror)
{
  skill_t skill;
  int i, episode, map;

  // e6y
  // The local variable should be used instead of demobuffer,
  // because demobuffer can be uninitialized
  const byte *header_p = demo_p;

  const byte *option_p = NULL;      /* killough 11/98 */

  basetic = gametic;  // killough 9/29/98

  // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
  // Old demos turn on demo_compatibility => compatibility; new demos load
  // compatibility flag, and other flags as well, as a part of the demo.

  //e6y: check for overrun
  if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
    return NULL;

  demover = *demo_p++;
  longtics = 0;

  // e6y
  // Handling of unrecognized demo formats
  // Versions up to 1.2 use a 7-byte header - first byte is a skill level.
  // Versions after 1.2 use a 13-byte header - first byte is a demoversion.
  // BOOM's demoversion starts from 200
  if (!((demover >=   0  && demover <=   4) ||
        (demover >= 104  && demover <= 111) ||
        (demover >= 200  && demover <= 214)))
  {
    I_Error("G_ReadDemoHeader: Unknown demo format %d.", demover);
  }

  if (demover < 200)     // Autodetect old demos
    {
      if (demover >= 111) longtics = 1;

      // killough 3/2/98: force these variables to be 0 in demo_compatibility

      // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
      // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

      if ((skill=demover) >= 100)         // For demos from versions >= 1.4
        {
          //e6y: check for overrun
          if (CheckForOverrun(header_p, demo_p, size, 8, failonerror))
            return NULL;

          skill = *demo_p++;
          episode = *demo_p++;
          map = *demo_p++;
          demo_p++;
          demo_p++;
          demo_p++;
          demo_p++;
          demo_p++;
        }
      else
        {
          //e6y: check for overrun
          if (CheckForOverrun(header_p, demo_p, size, 2, failonerror))
            return NULL;

          episode = *demo_p++;
          map = *demo_p++;
        }

    }
  else    // new versions of demos
    {
      demo_p += 6;               // skip signature;
      switch (demover) {
      case 200: /* BOOM */
      case 201:
        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
          return NULL;
	  break;
      case 202:
        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
          return NULL;

	  break;
      case 203:
	/* LxDoom or MBF - determine from signature
	 * cph - load compatibility level */
	switch (*(header_p + 2)) {
	case 'B': /* LxDoom */
	  /* cph - DEMOSYNC - LxDoom demos recorded in compatibility modes support dropped */
	  break;
	case 'M':
	  demo_p++;
	  break;
	}
	break;
      case 210:
	demo_p++;
	break;
      case 211:
	demo_p++;
	break;
      case 212:
	demo_p++;
	break;
      case 213:
	demo_p++;
	break;
      case 214:
        longtics = 1;
	demo_p++;
	break;
      }
      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, 5, failonerror))
        return NULL;

      skill = *demo_p++;
      episode = *demo_p++;
      map = *demo_p++;
      demo_p++;
      demo_p++;

	option_p = demo_p;

      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, GAME_OPTION_SIZE, failonerror))
        return NULL;

      demo_p = G_ReadOptions(demo_p);  // killough 3/1/98: Read game options

      if (demover == 200)              // killough 6/3/98: partially fix v2.00 demos
        demo_p += 256-GAME_OPTION_SIZE;
    }

      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, MAXPLAYERS, failonerror))
        return NULL;

      for (i=0 ; i < MAXPLAYERS; i++)
        playeringame[i] = *demo_p++;
      demo_p += MIN_MAXPLAYERS - MAXPLAYERS;


  if (gameaction != ga_loadgame) { /* killough 12/98: support -loadgame */
    G_InitNew(skill, episode, map);
  }

  for (i=0; i<MAXPLAYERS;i++)         // killough 4/24/98
    players[i].cheats = 0;

  return demo_p;
}

void G_DoPlayDemo(void)
{
  char basename[9];

  ExtractFileBase(defdemoname,basename);           // killough
  basename[8] = 0;

  /* cph - store lump number for unlocking later */
  demolumpnum = W_GetNumForName(basename);
  demobuffer = W_CacheLumpNum(demolumpnum);
  demolength = W_LumpLength(demolumpnum);

  demo_p = G_ReadDemoHeader(demobuffer, demolength, true);

  gameaction = ga_nothing;
  usergame = false;

  demoplayback = true;
  R_SmoothPlaying_Reset(NULL); // e6y

  starttime = I_GetTime_RealTime ();
}

/* G_CheckDemoStatus
 *
 * Called after a death or level completion to allow demos to be cleaned up
 * Returns true if a new demo loop action will take place
 */
boolean G_CheckDemoStatus (void)
{
  P_ChecksumFinal();

  if (demorecording)
    {
      demorecording = false;
      fputc(DEMOMARKER, demofp);
      I_Error("G_CheckDemoStatus: Demo recorded");
      return false;  // killough
    }

  if (timingdemo)
    {
      int endtime = I_GetTime_RealTime ();
      // killough -- added fps information and made it work for longer demos:
      unsigned realtics = endtime-starttime;
      I_Error ("Timed %u gametics in %u realtics = %-.1f frames per second",
               (unsigned) gametic,realtics,
               (unsigned) gametic * (double) TICRATE / realtics);
    }

  if (demoplayback)
    {
      if (singledemo)
        exit(0);  // killough

      if (demolumpnum != -1) {
  // cph - unlock the demo lump
  W_UnlockLumpNum(demolumpnum);
  demolumpnum = -1;
      }
      G_ReloadDefaults();    // killough 3/1/98
      D_AdvanceDemo ();
      return true;
    }
  return false;
}

// killough 1/22/98: this is a "Doom printf" for messages. I've gotten
// tired of using players->message=... and so I've added this dprintf.
//
// killough 3/6/98: Made limit static to allow z_zone functions to call
// this function, without calling realloc(), which seems to cause problems.

#define MAX_MESSAGE_SIZE 1024

// CPhipps - renamed to doom_printf to avoid name collision with glibc
void doom_printf(const char *s, ...)
{
  static char msg[MAX_MESSAGE_SIZE];
  va_list v;
  va_start(v,s);
#ifdef HAVE_VSNPRINTF
  vsnprintf(msg,sizeof(msg),s,v);        /* print message in buffer */
#else
  vsprintf(msg,s,v);
#endif
  va_end(v);
  players[consoleplayer].message = msg;  // set new message
}
