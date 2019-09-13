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

/*
 * M_WriteFile
 *
 * killough 9/98: rewritten to use stdio and to flash disk icon
 */

boolean M_WriteFile(char const *name, void *source, int length)
{
    return false;
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
