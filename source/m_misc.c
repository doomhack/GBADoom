/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
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
 *  Main loop menu stuff.
 *  Default Config File.
 *  PCX Screenshots.
 *
 *-----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>

#include "doomstat.h"
#include "m_argv.h"
#include "g_game.h"
#include "m_menu.h"
#include "am_map.h"
#include "w_wad.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "dstrings.h"
#include "m_misc.h"
#include "s_sound.h"
#include "sounds.h"
#include "lprintf.h"
#include "d_main.h"
#include "r_draw.h"
#include "r_demo.h"
#include "r_fps.h"

/*
 * M_WriteFile
 *
 * killough 9/98: rewritten to use stdio and to flash disk icon
 */

boolean M_WriteFile(char const *name, void *source, int length)
{

}

/*
 * M_ReadFile
 *
 * killough 9/98: rewritten to use stdio and to flash disk icon
 */

int M_ReadFile(char const *name, byte **buffer)
{
  return -1;
}



extern const char* S_music_files[]; // cournia

/* cph - Some MBF stuff parked here for now
 * killough 10/98
 */

default_t defaults[] =
{
  {"Game settings",{NULL},{0},UL,UL,def_none,ss_none},

  {"Sound settings",{NULL},{0},UL,UL,def_none,ss_none},

  {"sfx_volume",{&snd_SfxVolume},{8},0,15, def_int,ss_none},
  {"music_volume",{&snd_MusicVolume},{8},0,15, def_int,ss_none},
  {"snd_channels",{&default_numChannels},{8},1,8,
   def_int,ss_none}, // number of audio events simultaneously // killough

  {"Video settings",{NULL},{0},UL,UL,def_none,ss_none},

  {"videomode",{NULL, &default_videomode},{0,"8"},UL,UL,def_str,ss_none},

  /* 640x480 default resolution */
  {"screen_width",{&desired_screenwidth},{320}, 320, MAX_SCREENWIDTH,
   def_int,ss_none},
  {"screen_height",{&desired_screenheight},{240},200,MAX_SCREENHEIGHT,
   def_int,ss_none},
  {"use_fullscreen",{&use_fullscreen},{1},0,1, /* proff 21/05/2000 */
   def_bool,ss_none},
  {"usegamma",{&usegamma},{3},0,4, //jff 3/6/98 fix erroneous upper limit in range
   def_int,ss_none}, // gamma correction level // killough 1/18/98
  {"uncapped_framerate", {&movement_smooth},  {0},0,1,
   def_bool,ss_stat},

  //jff 4/3/98 allow unlimited sensitivity

// For key bindings, the values stored in the key_* variables       // phares
// are the internal Doom Codes. The values stored in the default.cfg
// file are the keyboard codes.
// CPhipps - now they're the doom codes, so default.cfg can be portable

  {"Key bindings",{NULL},{0},UL,UL,def_none,ss_none},
  {"key_right",       {&key_right},          {KEYD_RIGHTARROW},
   0,MAX_KEY,def_key,ss_keys}, // key to turn right
  {"key_left",        {&key_left},           {KEYD_LEFTARROW} ,
   0,MAX_KEY,def_key,ss_keys}, // key to turn left
  {"key_up",          {&key_up},             {KEYD_UPARROW}   ,
   0,MAX_KEY,def_key,ss_keys}, // key to move forward
  {"key_down",        {&key_down},           {KEYD_DOWNARROW},
   0,MAX_KEY,def_key,ss_keys}, // key to move backward
  {"key_menu_right",  {&key_menu_right},     {KEYD_RIGHTARROW},// phares 3/7/98
   0,MAX_KEY,def_key,ss_keys}, // key to move right in a menu  //     |
  {"key_menu_left",   {&key_menu_left},      {KEYD_LEFTARROW} ,//     V
   0,MAX_KEY,def_key,ss_keys}, // key to move left in a menu
  {"key_menu_up",     {&key_menu_up},        {KEYD_UPARROW}   ,
   0,MAX_KEY,def_key,ss_keys}, // key to move up in a menu
  {"key_menu_down",   {&key_menu_down},      {KEYD_DOWNARROW} ,
   0,MAX_KEY,def_key,ss_keys}, // key to move down in a menu
  {"key_menu_backspace",{&key_menu_backspace},{KEYD_BACKSPACE} ,
   0,MAX_KEY,def_key,ss_keys}, // delete key in a menu
  {"key_menu_escape", {&key_menu_escape},    {KEYD_ESCAPE}    ,
   0,MAX_KEY,def_key,ss_keys}, // key to leave a menu      ,   // phares 3/7/98
  {"key_menu_enter",  {&key_menu_enter},     {KEYD_ENTER}     ,
   0,MAX_KEY,def_key,ss_keys}, // key to select from menu
  {"key_strafeleft",  {&key_strafeleft},     {','}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to strafe left
  {"key_straferight", {&key_straferight},    {'.'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to strafe right

  {"key_fire",        {&key_fire},           {KEYD_RCTRL}     ,
   0,MAX_KEY,def_key,ss_keys}, // duh
  {"key_use",         {&key_use},            {' '}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to open a door, use a switch
  {"key_strafe",      {&key_strafe},         {KEYD_RALT}      ,
   0,MAX_KEY,def_key,ss_keys}, // key to use with arrows to strafe
  {"key_speed",       {&key_speed},          {KEYD_RSHIFT}    ,
   0,MAX_KEY,def_key,ss_keys}, // key to run

  {"key_savegame",    {&key_savegame},       {KEYD_F2}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to save current game
  {"key_loadgame",    {&key_loadgame},       {KEYD_F3}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to restore from saved games
  {"key_soundvolume", {&key_soundvolume},    {KEYD_F4}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to bring up sound controls
  {"key_hud",         {&key_hud},            {KEYD_F5}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to adjust HUD
  {"key_quicksave",   {&key_quicksave},      {KEYD_F6}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to to quicksave
  {"key_endgame",     {&key_endgame},        {KEYD_F7}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to end the game
  {"key_messages",    {&key_messages},       {KEYD_F8}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle message enable
  {"key_quickload",   {&key_quickload},      {KEYD_F9}        ,
   0,MAX_KEY,def_key,ss_keys}, // key to load from quicksave
  {"key_quit",        {&key_quit},           {KEYD_F10}       ,
   0,MAX_KEY,def_key,ss_keys}, // key to quit game
  {"key_gamma",       {&key_gamma},          {KEYD_F11}       ,
   0,MAX_KEY,def_key,ss_keys}, // key to adjust gamma correction
  {"key_spy",         {&key_spy},            {KEYD_F12}       ,
   0,MAX_KEY,def_key,ss_keys}, // key to view from another coop player's view
  {"key_pause",       {&key_pause},          {KEYD_PAUSE}     ,
   0,MAX_KEY,def_key,ss_keys}, // key to pause the game
  {"key_chat",        {&key_chat},           {'t'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to enter a chat message
  {"key_backspace",   {&key_backspace},      {KEYD_BACKSPACE} ,
   0,MAX_KEY,def_key,ss_keys}, // backspace key
  {"key_enter",       {&key_enter},          {KEYD_ENTER}     ,
   0,MAX_KEY,def_key,ss_keys}, // key to select from menu or see last message
  {"key_map",         {&key_map},            {KEYD_TAB}       ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle automap display
  {"key_map_right",   {&key_map_right},      {KEYD_RIGHTARROW},// phares 3/7/98
   0,MAX_KEY,def_key,ss_keys}, // key to shift automap right   //     |
  {"key_map_left",    {&key_map_left},       {KEYD_LEFTARROW} ,//     V
   0,MAX_KEY,def_key,ss_keys}, // key to shift automap left
  {"key_map_up",      {&key_map_up},         {KEYD_UPARROW}   ,
   0,MAX_KEY,def_key,ss_keys}, // key to shift automap up
  {"key_map_down",    {&key_map_down},       {KEYD_DOWNARROW} ,
   0,MAX_KEY,def_key,ss_keys}, // key to shift automap down
  {"key_map_zoomin",  {&key_map_zoomin},      {'='}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to enlarge automap
  {"key_map_zoomout", {&key_map_zoomout},     {'-'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to reduce automap
  {"key_map_gobig",   {&key_map_gobig},       {'0'}           ,
   0,MAX_KEY,def_key,ss_keys},  // key to get max zoom for automap
  {"key_map_follow",  {&key_map_follow},      {'f'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle follow mode
  {"key_map_mark",    {&key_map_mark},        {'m'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to drop a marker on automap
  {"key_map_clear",   {&key_map_clear},       {'c'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to clear all markers on automap
  {"key_map_grid",    {&key_map_grid},        {'g'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle grid display over automap
  {"key_map_rotate",  {&key_map_rotate},      {'r'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle rotating the automap to match the player's orientation
  {"key_map_overlay", {&key_map_overlay},     {'o'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle overlaying the automap on the rendered display
  {"key_reverse",     {&key_reverse},         {'/'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to spin 180 instantly
  {"key_zoomin",      {&key_zoomin},          {'='}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to enlarge display
  {"key_zoomout",     {&key_zoomout},         {'-'}           ,
   0,MAX_KEY,def_key,ss_keys}, // key to reduce display
  {"key_chatplayer1", {&destination_keys[0]}, {'g'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to chat with player 1
  // killough 11/98: fix 'i'/'b' reversal
  {"key_chatplayer2", {&destination_keys[1]}, {'i'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to chat with player 2
  {"key_chatplayer3", {&destination_keys[2]}, {'b'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to chat with player 3
  {"key_chatplayer4", {&destination_keys[3]}, {'r'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to chat with player 4
  {"key_weapontoggle",{&key_weapontoggle},    {'0'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to toggle between two most preferred weapons with ammo
  {"key_weapon1",     {&key_weapon1},         {'1'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 1 (fist/chainsaw)
  {"key_weapon2",     {&key_weapon2},         {'2'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 2 (pistol)
  {"key_weapon3",     {&key_weapon3},         {'3'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 3 (supershotgun/shotgun)
  {"key_weapon4",     {&key_weapon4},         {'4'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 4 (chaingun)
  {"key_weapon5",     {&key_weapon5},         {'5'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 5 (rocket launcher)
  {"key_weapon6",     {&key_weapon6},         {'6'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 6 (plasma rifle)
  {"key_weapon7",     {&key_weapon7},         {'7'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 7 (bfg9000)         //    ^
  {"key_weapon8",     {&key_weapon8},         {'8'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 8 (chainsaw)        //    |
  {"key_weapon9",     {&key_weapon9},         {'9'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to switch to weapon 9 (supershotgun)    // phares

  // killough 2/22/98: screenshot key
  {"key_screenshot",  {&key_screenshot},      {'*'}            ,
   0,MAX_KEY,def_key,ss_keys}, // key to take a screenshot

  {"Automap settings",{NULL},{0},UL,UL,def_none,ss_none},
  //jff 1/7/98 defaults for automap colors
  //jff 4/3/98 remove -1 in lower range, 0 now disables new map features
  {"mapcolor_back", {&mapcolor_back}, {247},0,255,  // black //jff 4/6/98 new black
   def_colour,ss_auto}, // color used as background for automap
  {"mapcolor_grid", {&mapcolor_grid}, {104},0,255,  // dk gray
   def_colour,ss_auto}, // color used for automap grid lines
  {"mapcolor_wall", {&mapcolor_wall}, {23},0,255,   // red-brown
   def_colour,ss_auto}, // color used for one side walls on automap
  {"mapcolor_fchg", {&mapcolor_fchg}, {55},0,255,   // lt brown
   def_colour,ss_auto}, // color used for lines floor height changes across
  {"mapcolor_cchg", {&mapcolor_cchg}, {215},0,255,  // orange
   def_colour,ss_auto}, // color used for lines ceiling height changes across
  {"mapcolor_clsd", {&mapcolor_clsd}, {208},0,255,  // white
   def_colour,ss_auto}, // color used for lines denoting closed doors, objects
  {"mapcolor_rkey", {&mapcolor_rkey}, {175},0,255,  // red
   def_colour,ss_auto}, // color used for red key sprites
  {"mapcolor_bkey", {&mapcolor_bkey}, {204},0,255,  // blue
   def_colour,ss_auto}, // color used for blue key sprites
  {"mapcolor_ykey", {&mapcolor_ykey}, {231},0,255,  // yellow
   def_colour,ss_auto}, // color used for yellow key sprites
  {"mapcolor_rdor", {&mapcolor_rdor}, {175},0,255,  // red
   def_colour,ss_auto}, // color used for closed red doors
  {"mapcolor_bdor", {&mapcolor_bdor}, {204},0,255,  // blue
   def_colour,ss_auto}, // color used for closed blue doors
  {"mapcolor_ydor", {&mapcolor_ydor}, {231},0,255,  // yellow
   def_colour,ss_auto}, // color used for closed yellow doors
  {"mapcolor_tele", {&mapcolor_tele}, {119},0,255,  // dk green
   def_colour,ss_auto}, // color used for teleporter lines
  {"mapcolor_secr", {&mapcolor_secr}, {252},0,255,  // purple
   def_colour,ss_auto}, // color used for lines around secret sectors
  {"mapcolor_exit", {&mapcolor_exit}, {0},0,255,    // none
   def_colour,ss_auto}, // color used for exit lines
  {"mapcolor_unsn", {&mapcolor_unsn}, {104},0,255,  // dk gray
   def_colour,ss_auto}, // color used for lines not seen without computer map
  {"mapcolor_flat", {&mapcolor_flat}, {88},0,255,   // lt gray
   def_colour,ss_auto}, // color used for lines with no height changes
  {"mapcolor_sprt", {&mapcolor_sprt}, {112},0,255,  // green
   def_colour,ss_auto}, // color used as things
  {"mapcolor_item", {&mapcolor_item}, {231},0,255,  // yellow
   def_colour,ss_auto}, // color used for counted items
  {"mapcolor_hair", {&mapcolor_hair}, {208},0,255,  // white
   def_colour,ss_auto}, // color used for dot crosshair denoting center of map
  {"mapcolor_sngl", {&mapcolor_sngl}, {208},0,255,  // white
   def_colour,ss_auto}, // color used for the single player arrow
  {"mapcolor_enemy",   {&mapcolor_enemy}, {177},0,255,
   def_colour,ss_auto},
  {"mapcolor_frnd",   {&mapcolor_frnd}, {112},0,255,
   def_colour,ss_auto},
  //jff 3/9/98 add option to not show secrets til after found
  {"map_secret_after", {&map_secret_after}, {0},0,1, // show secret after gotten
   def_bool,ss_auto}, // prevents showing secret sectors till after entered
  //jff 1/7/98 end additions for automap
  {"automapmode", {(int*)&automapmode}, {0}, 0, 31, // CPhipps - remember automap mode
   def_hex,ss_none}, // automap mode

  {"Heads-up display settings",{NULL},{0},UL,UL,def_none,ss_none},
  //jff 2/16/98 defaults for color ranges in hud and status
  {"hudcolor_titl", {&hudcolor_titl}, {5},0,9,  // gold range
   def_int,ss_auto}, // color range used for automap level title
  {"hudcolor_xyco", {&hudcolor_xyco}, {3},0,9,  // green range
   def_int,ss_auto}, // color range used for automap coordinates
  {"hudcolor_mesg", {&hudcolor_mesg}, {6},0,9,  // red range
   def_int,ss_mess}, // color range used for messages during play
  {"hudcolor_chat", {&hudcolor_chat}, {5},0,9,  // gold range
   def_int,ss_mess}, // color range used for chat messages and entry
  {"hudcolor_list", {&hudcolor_list}, {5},0,9,  // gold range  //jff 2/26/98
   def_int,ss_mess}, // color range used for message review
  {"hud_msg_lines", {&hud_msg_lines}, {1},1,16,  // 1 line scrolling window
   def_int,ss_mess}, // number of messages in review display (1=disable)
  {"hud_list_bgon", {&hud_list_bgon}, {0},0,1,  // solid window bg ena //jff 2/26/98
   def_bool,ss_mess}, // enables background window behind message review
  {"hud_distributed",{&hud_distributed},{0},0,1, // hud broken up into 3 displays //jff 3/4/98
   def_bool,ss_none}, // splits HUD into three 2 line displays

  {"health_red",    {&health_red}   , {25},0,200, // below is red
   def_int,ss_stat}, // amount of health for red to yellow transition
  {"health_yellow", {&health_yellow}, {50},0,200, // below is yellow
   def_int,ss_stat}, // amount of health for yellow to green transition
  {"health_green",  {&health_green} , {100},0,200,// below is green, above blue
   def_int,ss_stat}, // amount of health for green to blue transition
  {"armor_red",     {&armor_red}    , {25},0,200, // below is red
   def_int,ss_stat}, // amount of armor for red to yellow transition
  {"armor_yellow",  {&armor_yellow} , {50},0,200, // below is yellow
   def_int,ss_stat}, // amount of armor for yellow to green transition
  {"armor_green",   {&armor_green}  , {100},0,200,// below is green, above blue
   def_int,ss_stat}, // amount of armor for green to blue transition
  {"ammo_red",      {&ammo_red}     , {25},0,100, // below 25% is red
   def_int,ss_stat}, // percent of ammo for red to yellow transition
  {"ammo_yellow",   {&ammo_yellow}  , {50},0,100, // below 50% is yellow, above green
   def_int,ss_stat}, // percent of ammo for yellow to green transition

  //jff 2/16/98 HUD and status feature controls
  {"hud_active",    {&hud_active}, {2},0,2, // 0=off, 1=small, 2=full
   def_int,ss_none}, // 0 for HUD off, 1 for HUD small, 2 for full HUD
  //jff 2/23/98
  {"hud_displayed", {&hud_displayed},  {0},0,1, // whether hud is displayed
   def_bool,ss_none}, // enables display of HUD
  {"hud_nosecrets", {&hud_nosecrets},  {0},0,1, // no secrets/items/kills HUD line
   def_bool,ss_stat}, // disables display of kills/items/secrets on HUD

  {"Weapon preferences",{NULL},{0},UL,UL,def_none,ss_none},
  // killough 2/8/98: weapon preferences set by user:
  {"weapon_choice_1", {&weapon_preferences[0][0]}, {6}, 0,9,
   def_int,ss_weap}, // first choice for weapon (best)
  {"weapon_choice_2", {&weapon_preferences[0][1]}, {9}, 0,9,
   def_int,ss_weap}, // second choice for weapon
  {"weapon_choice_3", {&weapon_preferences[0][2]}, {4}, 0,9,
   def_int,ss_weap}, // third choice for weapon
  {"weapon_choice_4", {&weapon_preferences[0][3]}, {3}, 0,9,
   def_int,ss_weap}, // fourth choice for weapon
  {"weapon_choice_5", {&weapon_preferences[0][4]}, {2}, 0,9,
   def_int,ss_weap}, // fifth choice for weapon
  {"weapon_choice_6", {&weapon_preferences[0][5]}, {8}, 0,9,
   def_int,ss_weap}, // sixth choice for weapon
  {"weapon_choice_7", {&weapon_preferences[0][6]}, {5}, 0,9,
   def_int,ss_weap}, // seventh choice for weapon
  {"weapon_choice_8", {&weapon_preferences[0][7]}, {7}, 0,9,
   def_int,ss_weap}, // eighth choice for weapon
  {"weapon_choice_9", {&weapon_preferences[0][8]}, {1}, 0,9,
   def_int,ss_weap}, // ninth choice for weapon (worst)

  // cournia - support for arbitrary music file (defaults are mp3)
  {"Music", {NULL},{0},UL,UL,def_none,ss_none},
  {"mus_e1m1", {0,&S_music_files[mus_e1m1]}, {0,"e1m1.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m2", {0,&S_music_files[mus_e1m2]}, {0,"e1m2.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m3", {0,&S_music_files[mus_e1m3]}, {0,"e1m3.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m4", {0,&S_music_files[mus_e1m4]}, {0,"e1m4.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m5", {0,&S_music_files[mus_e1m5]}, {0,"e1m5.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m6", {0,&S_music_files[mus_e1m6]}, {0,"e1m6.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m7", {0,&S_music_files[mus_e1m7]}, {0,"e1m7.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m8", {0,&S_music_files[mus_e1m8]}, {0,"e1m8.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e1m9", {0,&S_music_files[mus_e1m9]}, {0,"e1m9.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m1", {0,&S_music_files[mus_e2m1]}, {0,"e2m1.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m2", {0,&S_music_files[mus_e2m2]}, {0,"e2m2.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m3", {0,&S_music_files[mus_e2m3]}, {0,"e2m3.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m4", {0,&S_music_files[mus_e2m4]}, {0,"e2m4.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m5", {0,&S_music_files[mus_e2m5]}, {0,"e1m7.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m6", {0,&S_music_files[mus_e2m6]}, {0,"e2m6.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m7", {0,&S_music_files[mus_e2m7]}, {0,"e2m7.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m8", {0,&S_music_files[mus_e2m8]}, {0,"e2m8.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e2m9", {0,&S_music_files[mus_e2m9]}, {0,"e3m1.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m1", {0,&S_music_files[mus_e3m1]}, {0,"e3m1.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m2", {0,&S_music_files[mus_e3m2]}, {0,"e3m2.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m3", {0,&S_music_files[mus_e3m3]}, {0,"e3m3.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m4", {0,&S_music_files[mus_e3m4]}, {0,"e1m8.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m5", {0,&S_music_files[mus_e3m5]}, {0,"e1m7.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m6", {0,&S_music_files[mus_e3m6]}, {0,"e1m6.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m7", {0,&S_music_files[mus_e3m7]}, {0,"e2m7.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m8", {0,&S_music_files[mus_e3m8]}, {0,"e3m8.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_e3m9", {0,&S_music_files[mus_e3m9]}, {0,"e1m9.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_inter", {0,&S_music_files[mus_inter]}, {0,"e2m3.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_intro", {0,&S_music_files[mus_intro]}, {0,"intro.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_bunny", {0,&S_music_files[mus_bunny]}, {0,"bunny.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_victor", {0,&S_music_files[mus_victor]}, {0,"victor.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_introa", {0,&S_music_files[mus_introa]}, {0,"intro.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_runnin", {0,&S_music_files[mus_runnin]}, {0,"runnin.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_stalks", {0,&S_music_files[mus_stalks]}, {0,"stalks.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_countd", {0,&S_music_files[mus_countd]}, {0,"countd.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_betwee", {0,&S_music_files[mus_betwee]}, {0,"betwee.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_doom", {0,&S_music_files[mus_doom]}, {0,"doom.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_the_da", {0,&S_music_files[mus_the_da]}, {0,"the_da.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_shawn", {0,&S_music_files[mus_shawn]}, {0,"shawn.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_ddtblu", {0,&S_music_files[mus_ddtblu]}, {0,"ddtblu.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_in_cit", {0,&S_music_files[mus_in_cit]}, {0,"in_cit.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_dead", {0,&S_music_files[mus_dead]}, {0,"dead.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_stlks2", {0,&S_music_files[mus_stlks2]}, {0,"stalks.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_theda2", {0,&S_music_files[mus_theda2]}, {0,"the_da.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_doom2", {0,&S_music_files[mus_doom2]}, {0,"doom.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_ddtbl2", {0,&S_music_files[mus_ddtbl2]}, {0,"ddtblu.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_runni2", {0,&S_music_files[mus_runni2]}, {0,"runnin.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_dead2", {0,&S_music_files[mus_dead2]}, {0,"dead.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_stlks3", {0,&S_music_files[mus_stlks3]}, {0,"stalks.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_romero", {0,&S_music_files[mus_romero]}, {0,"romero.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_shawn2", {0,&S_music_files[mus_shawn2]}, {0,"shawn.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_messag", {0,&S_music_files[mus_messag]}, {0,"messag.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_count2", {0,&S_music_files[mus_count2]}, {0,"countd.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_ddtbl3", {0,&S_music_files[mus_ddtbl3]}, {0,"ddtblu.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_ampie", {0,&S_music_files[mus_ampie]}, {0,"ampie.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_theda3", {0,&S_music_files[mus_theda3]}, {0,"the_da.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_adrian", {0,&S_music_files[mus_adrian]}, {0,"adrian.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_messg2", {0,&S_music_files[mus_messg2]}, {0,"messag.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_romer2", {0,&S_music_files[mus_romer2]}, {0,"romero.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_tense", {0,&S_music_files[mus_tense]}, {0,"tense.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_shawn3", {0,&S_music_files[mus_shawn3]}, {0,"shawn.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_openin", {0,&S_music_files[mus_openin]}, {0,"openin.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_evil", {0,&S_music_files[mus_evil]}, {0,"evil.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_ultima", {0,&S_music_files[mus_ultima]}, {0,"ultima.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_read_m", {0,&S_music_files[mus_read_m]}, {0,"read_m.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_dm2ttl", {0,&S_music_files[mus_dm2ttl]}, {0,"dm2ttl.mp3"},UL,UL,
   def_str,ss_none},
  {"mus_dm2int", {0,&S_music_files[mus_dm2int]}, {0,"dm2int.mp3"},UL,UL,
   def_str,ss_none},
};

int numdefaults;
static const char* defaultfile; // CPhipps - static, const

//
// M_SaveDefaults
//

void M_SaveDefaults (void)
  {
  int   i;
  FILE* f;

  f = fopen (defaultfile, "w");
  if (!f)
    return; // can't write the file, but don't complain

  // 3/3/98 explain format of file

  fprintf(f,"# Doom config file\n");
  fprintf(f,"# Format:\n");
  fprintf(f,"# variable   value\n");

  for (i = 0 ; i < numdefaults ; i++) {
    if (defaults[i].type == def_none) {
      // CPhipps - pure headers
      fprintf(f, "\n# %s\n", defaults[i].name);
    } else
    // CPhipps - modified for new default_t form
    if (!IS_STRING(defaults[i])) //jff 4/10/98 kill super-hack on pointer value
      {
      // CPhipps - remove keycode hack
      // killough 3/6/98: use spaces instead of tabs for uniform justification
      if (defaults[i].type == def_hex)
  fprintf (f,"%-25s 0x%x\n",defaults[i].name,*(defaults[i].location.pi));
      else
  fprintf (f,"%-25s %5i\n",defaults[i].name,*(defaults[i].location.pi));
      }
    else
      {
      fprintf (f,"%-25s \"%s\"\n",defaults[i].name,*(defaults[i].location.ppsz));
      }
    }

  fclose (f);
  }

/*
 * M_LookupDefault
 *
 * cph - mimic MBF function for now. Yes it's crap.
 */

struct default_s *M_LookupDefault(const char *name)
{
  int i;
  for (i = 0 ; i < numdefaults ; i++)
    if ((defaults[i].type != def_none) && !strcmp(name, defaults[i].name))
      return &defaults[i];
  I_Error("M_LookupDefault: %s not found",name);
  return NULL;
}

//
// M_LoadDefaults
//

#define NUMCHATSTRINGS 10 // phares 4/13/98

void M_LoadDefaults (void)
{
  int   i;
  int   len;
  FILE* f;
  char  def[80];
  char  strparm[100];
  char* newstring = NULL;   // killough
  int   parm;
  boolean isstring;


  // set everything to base values

  numdefaults = sizeof(defaults)/sizeof(defaults[0]);
  for (i = 0 ; i < numdefaults ; i++) {
    if (defaults[i].location.ppsz)
      *defaults[i].location.ppsz = strdup(defaults[i].defaultvalue.psz);
    if (defaults[i].location.pi)
      *defaults[i].location.pi = defaults[i].defaultvalue.i;
  }


  // check for a custom default file

  i = M_CheckParm ("-config");
  if (i && i < myargc-1)
    defaultfile = myargv[i+1];
  else 
  {
	  defaultfile = "D:\\Doom\\config.cfg";
  }

  lprintf (LO_CONFIRM, " default file: %s\n",defaultfile);

  // read the file in, overriding any set defaults

  f = fopen (defaultfile, "r");
  if (f)
    {
    while (!feof(f))
      {
      isstring = false;
      if (fscanf (f, "%79s %[^\n]\n", def, strparm) == 2)
        {

        //jff 3/3/98 skip lines not starting with an alphanum

        if (!isalnum(def[0]))
          continue;

        if (strparm[0] == '"') {
          // get a string default

          isstring = true;
          len = strlen(strparm);
          newstring = (char *) malloc(len);
          strparm[len-1] = 0; // clears trailing double-quote mark
          strcpy(newstring, strparm+1); // clears leading double-quote mark
  } else if ((strparm[0] == '0') && (strparm[1] == 'x')) {
    // CPhipps - allow ints to be specified in hex
    sscanf(strparm+2, "%x", &parm);
  } else {
          sscanf(strparm, "%i", &parm);
    // Keycode hack removed
  }

        for (i = 0 ; i < numdefaults ; i++)
          if ((defaults[i].type != def_none) && !strcmp(def, defaults[i].name))
            {
      // CPhipps - safety check
            if (isstring != IS_STRING(defaults[i])) {
        lprintf(LO_WARN, "M_LoadDefaults: Type mismatch reading %s\n", defaults[i].name);
        continue;
      }
            if (!isstring)
              {

              //jff 3/4/98 range check numeric parameters

              if ((defaults[i].minvalue==UL || defaults[i].minvalue<=parm) &&
                  (defaults[i].maxvalue==UL || defaults[i].maxvalue>=parm))
                *(defaults[i].location.pi) = parm;
              }
            else
              {
              free((char*)*(defaults[i].location.ppsz));  /* phares 4/13/98 */
              *(defaults[i].location.ppsz) = newstring;
              }
            break;
            }
        }
      }

    fclose (f);
    }
}


//
// SCREEN SHOTS
//

//
// M_ScreenShot
//
// Modified by Lee Killough so that any number of shots can be taken,
// the code is faster, and no annoying "screenshot" message appears.

// CPhipps - modified to use its own buffer for the image
//         - checks for the case where no file can be created (doesn't occur on POSIX systems, would on DOS)
//         - track errors better
//         - split into 2 functions

//
// M_DoScreenShot
// Takes a screenshot into the names file

void M_DoScreenShot (const char* fname)
{
  if (I_ScreenShot(fname) != 0)
    doom_printf("M_ScreenShot: Error writing screenshot\n");
}

#ifndef SCREENSHOT_DIR
#define SCREENSHOT_DIR "."
#endif

#ifdef HAVE_LIBPNG
#define SCREENSHOT_EXT ".png"
#else
#define SCREENSHOT_EXT ".bmp"
#endif

void M_ScreenShot(void)
{
  static int shot;
  char       lbmname[PATH_MAX + 1];
  int        startshot;

  if (!access(SCREENSHOT_DIR,2))
  {
    startshot = shot; // CPhipps - prevent infinite loop

    do {
      sprintf(lbmname,"%s/doom%02d" SCREENSHOT_EXT, SCREENSHOT_DIR, shot++);
    } while (!access(lbmname,0) && (shot != startshot) && (shot < 10000));

    if (access(lbmname,0))
    {
      S_StartSound(NULL,gamemode==commercial ? sfx_radio : sfx_tink);
      M_DoScreenShot(lbmname); // cph
      return;
    }
  }

  doom_printf ("M_ScreenShot: Couldn't create screenshot");
  return;
}
