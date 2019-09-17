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
 *      Archiving: SaveGame I/O.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "m_random.h"
#include "am_map.h"
#include "p_enemy.h"
#include "lprintf.h"

#include "global_data.h"


// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
#define PADSAVEP()    do { _g->save_p += (4 - ((int) _g->save_p & 3)) & 3; } while (0)
//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
  int i;

  CheckSaveGame(sizeof(player_t) * MAXPLAYERS); // killough
  for (i=0 ; i<MAXPLAYERS ; i++)
    if (_g->playeringame[i])
      {
        int      j;
        player_t *dest;

        PADSAVEP();
        dest = (player_t *) _g->save_p;
        memcpy(dest, &_g->players[i], sizeof(player_t));
        _g->save_p += sizeof(player_t);
        for (j=0; j<NUMPSPRITES; j++)
          if (dest->psprites[j].state)
            dest->psprites[j].state =
              (state_t *)(dest->psprites[j].state-states);
      }
}

//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
  int i;

  for (i=0 ; i<MAXPLAYERS ; i++)
    if (_g->playeringame[i])
      {
        int j;

        PADSAVEP();

        memcpy(&_g->players[i], _g->save_p, sizeof(player_t));
        _g->save_p += sizeof(player_t);

        // will be set when unarc thinker
        _g->players[i].mo = NULL;
        _g->players[i].message = NULL;
        _g->players[i].attacker = NULL;

        for (j=0 ; j<NUMPSPRITES ; j++)
          if (_g->players[i]. psprites[j].state)
            _g->players[i].psprites[j].state =
              &states[(int)_g->players[i].psprites[j].state];
      }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
  int            i;
  const sector_t *sec;
  const line_t   *li;
  const side_t   *si;
  short          *put;

  // killough 3/22/98: fix bug caused by hoisting save_p too early
  // killough 10/98: adjust size for changes below
  size_t size =
    (sizeof(short)*5 + sizeof sec->floorheight + sizeof sec->ceilingheight)
    * _g->numsectors + sizeof(short)*3*_g->numlines + 4;

  for (i=0; i<_g->numlines; i++)
    {
      if (_g->lines[i].sidenum[0] != NO_INDEX)
        size +=
    sizeof(short)*3 + sizeof si->textureoffset + sizeof si->rowoffset;
      if (_g->lines[i].sidenum[1] != NO_INDEX)
  size +=
    sizeof(short)*3 + sizeof si->textureoffset + sizeof si->rowoffset;
    }

  CheckSaveGame(size); // killough

  PADSAVEP();                // killough 3/22/98

  put = (short *)_g->save_p;

  // do sectors
  for (i=0, sec = _g->sectors ; i<_g->numsectors ; i++,sec++)
    {
      // killough 10/98: save full floor & ceiling heights, including fraction
      memcpy(put, &sec->floorheight, sizeof sec->floorheight);
      put = (void *)((char *) put + sizeof sec->floorheight);
      memcpy(put, &sec->ceilingheight, sizeof sec->ceilingheight);
      put = (void *)((char *) put + sizeof sec->ceilingheight);

      *put++ = sec->floorpic;
      *put++ = sec->ceilingpic;
      *put++ = sec->lightlevel;
      *put++ = sec->special;            // needed?   yes -- transfer types
      *put++ = sec->tag;                // needed?   need them -- killough
    }

  // do lines
  for (i=0, li = _g->lines ; i<_g->numlines ; i++,li++)
    {
      int j;

      *put++ = li->flags;
      *put++ = li->special;
      *put++ = li->tag;

      for (j=0; j<2; j++)
        if (li->sidenum[j] != NO_INDEX)
          {
      si = &_g->sides[li->sidenum[j]];

      // killough 10/98: save full sidedef offsets,
      // preserving fractional scroll offsets

      memcpy(put, &si->textureoffset, sizeof si->textureoffset);
      put = (void *)((char *) put + sizeof si->textureoffset);
      memcpy(put, &si->rowoffset, sizeof si->rowoffset);
      put = (void *)((char *) put + sizeof si->rowoffset);

            *put++ = si->toptexture;
            *put++ = si->bottomtexture;
            *put++ = si->midtexture;
          }
    }
  _g->save_p = (byte *) put;
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
  int          i;
  sector_t     *sec;
  line_t       *li;
  const short  *get;

  PADSAVEP();                // killough 3/22/98

  get = (short *) _g->save_p;

  // do sectors
  for (i=0, sec = _g->sectors ; i<_g->numsectors ; i++,sec++)
    {
      // killough 10/98: load full floor & ceiling heights, including fractions

      memcpy(&sec->floorheight, get, sizeof sec->floorheight);
      get = (void *)((char *) get + sizeof sec->floorheight);
      memcpy(&sec->ceilingheight, get, sizeof sec->ceilingheight);
      get = (void *)((char *) get + sizeof sec->ceilingheight);

      sec->floorpic = *get++;
      sec->ceilingpic = *get++;
      sec->lightlevel = *get++;
      sec->special = *get++;
      sec->tag = *get++;
      sec->ceilingdata = 0; //jff 2/22/98 now three thinker fields, not two
      sec->floordata = 0;
      sec->lightingdata = 0;
      sec->soundtarget = 0;
    }

  // do lines
  for (i=0, li = _g->lines ; i<_g->numlines ; i++,li++)
    {
      int j;

      li->flags = *get++;
      li->special = *get++;
      li->tag = *get++;
      for (j=0 ; j<2 ; j++)
        if (li->sidenum[j] != NO_INDEX)
          {
            side_t *si = &_g->sides[li->sidenum[j]];

      // killough 10/98: load full sidedef offsets, including fractions

      memcpy(&si->textureoffset, get, sizeof si->textureoffset);
      get = (void *)((char *) get + sizeof si->textureoffset);
      memcpy(&si->rowoffset, get, sizeof si->rowoffset);
      get = (void *)((char *) get + sizeof si->rowoffset);

            si->toptexture = *get++;
            si->bottomtexture = *get++;
            si->midtexture = *get++;
          }
    }
  _g->save_p = (byte *) get;
}

//
// Thinkers
//

typedef enum {
  tc_end,
  tc_mobj
} thinkerclass_t;

// phares 9/13/98: Moved this code outside of P_ArchiveThinkers so the
// thinker indices could be used by the code that saves sector info.

static int number_of_thinkers;

void P_ThinkerToIndex(void)
  {
  thinker_t *th;

  // killough 2/14/98:
  // count the number of thinkers, and mark each one with its index, using
  // the prev field as a placeholder, since it can be restored later.

  number_of_thinkers = 0;
  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function == P_MobjThinker)
      th->prev = (thinker_t *) ++number_of_thinkers;
  }

// phares 9/13/98: Moved this code outside of P_ArchiveThinkers so the
// thinker indices could be used by the code that saves sector info.

void P_IndexToThinker(void)
  {
  // killough 2/14/98: restore prev pointers
  thinker_t *th;
  thinker_t *prev = &thinkercap;

  for (th = thinkercap.next ; th != &thinkercap ; prev=th, th=th->next)
    th->prev = prev;
  }

//
// P_ArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs

void P_ArchiveThinkers (void)
{
  thinker_t *th;

  CheckSaveGame(sizeof _g->brain);      // killough 3/26/98: Save boss brain state
  memcpy(_g->save_p, &_g->brain, sizeof _g->brain);
  _g->save_p += sizeof _g->brain;

  /* check that enough room is available in savegame buffer
   * - killough 2/14/98
   * cph - use number_of_thinkers saved by P_ThinkerToIndex above
   * size per object is sizeof(mobj_t) - 2*sizeof(void*) - 4*sizeof(fixed_t) plus
   * padded type (4) plus 5*sizeof(void*), i.e. sizeof(mobj_t) + 4 +
   * 3*sizeof(void*)
   * cph - +1 for the tc_end
   */
  CheckSaveGame(number_of_thinkers*(sizeof(mobj_t)-3*sizeof(fixed_t)+4+3*sizeof(void*)) +1);

  // save off the current thinkers
  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function == P_MobjThinker)
      {
        mobj_t *mobj;

        *_g->save_p++ = tc_mobj;
        PADSAVEP();
        mobj = (mobj_t *)_g->save_p;
	/* cph 2006/07/30 - 
	 * The end of mobj_t changed from
	 *  boolean invisible;
	 *  mobj_t* lastenemy;
	 *  mobj_t* above_monster;
	 *  mobj_t* below_monster;
	 *  void* touching_sectorlist;
	 * to
	 *  mobj_t* lastenemy;
	 *  void* touching_sectorlist;
         *  fixed_t PrevX, PrevY, PrevZ, padding;
	 * at prboom 2.4.4. There is code here to preserve the savegame format.
	 *
	 * touching_sectorlist is reconstructed anyway, so we now leave off the
	 * last 2 words of mobj_t, write 5 words of 0 and then write lastenemy
	 * into the second of these.
	 */
        memcpy (mobj, th, sizeof(*mobj) - 2*sizeof(void*));
        _g->save_p += sizeof(*mobj) - 2*sizeof(void*) - 4*sizeof(fixed_t);
        memset (_g->save_p, 0, 5*sizeof(void*));
        mobj->state = (state_t *)(mobj->state - states);

        // killough 2/14/98: convert pointers into indices.
        // Fixes many savegame problems, by properly saving
        // target and tracer fields. Note: we store NULL if
        // the thinker pointed to by these fields is not a
        // mobj thinker.

        if (mobj->target)
          mobj->target = mobj->target->thinker.function ==
            P_MobjThinker ?
            (mobj_t *) mobj->target->thinker.prev : NULL;

        if (mobj->tracer)
          mobj->tracer = mobj->tracer->thinker.function ==
            P_MobjThinker ?
            (mobj_t *) mobj->tracer->thinker.prev : NULL;

        // killough 2/14/98: new field: save last known enemy. Prevents
        // monsters from going to sleep after killing monsters and not
        // seeing player anymore.

        if (((mobj_t*)th)->lastenemy && ((mobj_t*)th)->lastenemy->thinker.function == P_MobjThinker) {
          memcpy (_g->save_p + sizeof(void*), &(((mobj_t*)th)->lastenemy->thinker.prev), sizeof(void*));
	}

        // killough 2/14/98: end changes

        _g->save_p += 5*sizeof(void*);

        if (mobj->player)
          mobj->player = (player_t *)((mobj->player-_g->players) + 1);
      }

  // add a terminating marker
  *_g->save_p++ = tc_end;

  // killough 9/14/98: save soundtargets
  {
    int i;
    CheckSaveGame(_g->numsectors * sizeof(mobj_t *));       // killough 9/14/98
    for (i = 0; i < _g->numsectors; i++)
    {
      mobj_t *target = _g->sectors[i].soundtarget;
      // Fix crash on reload when a soundtarget points to a removed corpse
      // (prboom bug #1590350)
      if (target && target->thinker.function == P_MobjThinker)
        target = (mobj_t *) target->thinker.prev;
      else
        target = NULL;
      memcpy(_g->save_p, &target, sizeof target);
      _g->save_p += sizeof target;
    }
  }
}

/*
 * killough 11/98
 *
 * Same as P_SetTarget() in p_tick.c, except that the target is nullified
 * first, so that no old target's reference count is decreased (when loading
 * savegames, old targets are indices, not really pointers to targets).
 */

static void P_SetNewTarget(mobj_t **mop, mobj_t *targ)
{
  *mop = NULL;
  P_SetTarget(mop, targ);
}

//
// P_UnArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//

// savegame file stores ints in the corresponding * field; this function
// safely casts them back to int.
static int P_GetMobj(mobj_t* mi, size_t s)
{
  size_t i = (size_t)mi;
  if (i >= s) I_Error("Corrupt savegame");
  return i;
}

void P_UnArchiveThinkers (void)
{
  thinker_t *th;
  mobj_t    **mobj_p;    // killough 2/14/98: Translation table
  size_t    size;        // killough 2/14/98: size of or index into table

  _g->totallive = 0;
  // killough 3/26/98: Load boss brain state
  memcpy(&_g->brain, _g->save_p, sizeof _g->brain);
  _g->save_p += sizeof _g->brain;

  // remove all the current thinkers
  for (th = thinkercap.next; th != &thinkercap; )
    {
      thinker_t *next = th->next;
      if (th->function == P_MobjThinker)
        P_RemoveMobj ((mobj_t *) th);
      else
        Z_Free (th);
      th = next;
    }
  P_InitThinkers ();

  // killough 2/14/98: count number of thinkers by skipping through them
  {
    byte *sp = _g->save_p;     // save pointer and skip header
    for (size = 1; *_g->save_p++ == tc_mobj; size++)  // killough 2/14/98
      {                     // skip all entries, adding up count
        PADSAVEP();
	/* cph 2006/07/30 - see comment below for change in layout of mobj_t */
        _g->save_p += sizeof(mobj_t)+3*sizeof(void*)-4*sizeof(fixed_t);
      }

    if (*--_g->save_p != tc_end)
      I_Error ("P_UnArchiveThinkers: Unknown tclass %i in savegame", *_g->save_p);

    // first table entry special: 0 maps to NULL
    *(mobj_p = malloc(size * sizeof *mobj_p)) = 0;   // table of pointers
    _g->save_p = sp;           // restore save pointer
  }

  // read in saved thinkers
  for (size = 1; *_g->save_p++ == tc_mobj; size++)    // killough 2/14/98
    {
      mobj_t *mobj = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);

      // killough 2/14/98 -- insert pointers to thinkers into table, in order:
      mobj_p[size] = mobj;

      PADSAVEP();
      /* cph 2006/07/30 - 
       * The end of mobj_t changed from
       *  boolean invisible;
       *  mobj_t* lastenemy;
       *  mobj_t* above_monster;
       *  mobj_t* below_monster;
       *  void* touching_sectorlist;
       * to
       *  mobj_t* lastenemy;
       *  void* touching_sectorlist;
       *  fixed_t PrevX, PrevY, PrevZ;
       * at prboom 2.4.4. There is code here to preserve the savegame format.
       *
       * touching_sectorlist is reconstructed anyway, so we now read in all 
       * but the last 5 words from the savegame (filling all but the last 2
       * fields of our current mobj_t. We then pull lastenemy from the 2nd of
       * the 5 leftover words, and skip the others.
       */
      memcpy (mobj, _g->save_p, sizeof(mobj_t)-2*sizeof(void*)-4*sizeof(fixed_t));
      _g->save_p += sizeof(mobj_t)-sizeof(void*)-4*sizeof(fixed_t);
      memcpy (&(mobj->lastenemy), _g->save_p, sizeof(void*));
      _g->save_p += 4*sizeof(void*);
      mobj->state = states + (int) mobj->state;

      if (mobj->player)
        (mobj->player = &_g->players[(int) mobj->player - 1]) -> mo = mobj;

      P_SetThingPosition (mobj);
      mobj->info = &mobjinfo[mobj->type];

      // killough 2/28/98:
      // Fix for falling down into a wall after savegame loaded:
      //      mobj->floorz = mobj->subsector->sector->floorheight;
      //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;

      mobj->thinker.function = P_MobjThinker;
      P_AddThinker (&mobj->thinker);

      if (!((mobj->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL | MF_CORPSE)))
        _g->totallive++;
    }

  // killough 2/14/98: adjust target and tracer fields, plus
  // lastenemy field, to correctly point to mobj thinkers.
  // NULL entries automatically handled by first table entry.
  //
  // killough 11/98: use P_SetNewTarget() to set fields

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
      P_SetNewTarget(&((mobj_t *) th)->target,
        mobj_p[P_GetMobj(((mobj_t *)th)->target,size)]);

      P_SetNewTarget(&((mobj_t *) th)->tracer,
        mobj_p[P_GetMobj(((mobj_t *)th)->tracer,size)]);

      P_SetNewTarget(&((mobj_t *) th)->lastenemy,
        mobj_p[P_GetMobj(((mobj_t *)th)->lastenemy,size)]);
    }

  {  // killough 9/14/98: restore soundtargets
    int i;
    for (i = 0; i < _g->numsectors; i++)
    {
      mobj_t *target;
      memcpy(&target, _g->save_p, sizeof target);
      _g->save_p += sizeof target;
      // Must verify soundtarget. See P_ArchiveThinkers.
      P_SetNewTarget(&_g->sectors[i].soundtarget, mobj_p[P_GetMobj(target,size)]);
    }
  }

  free(mobj_p);    // free translation table

  // killough 3/26/98: Spawn icon landings:
  if (_g->gamemode == commercial)
    P_SpawnBrainTargets();
}

//
// P_ArchiveSpecials
//
enum {
  tc_ceiling,
  tc_door,
  tc_floor,
  tc_plat,
  tc_flash,
  tc_strobe,
  tc_glow,
  tc_elevator,    //jff 2/22/98 new elevator type thinker
  tc_scroll,      // killough 3/7/98: new scroll effect thinker
  tc_pusher,      // phares 3/22/98:  new push/pull effect thinker
  tc_flicker,     // killough 10/4/98
  tc_endspecials
} specials_e;

//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
// T_MoveElevator, (plat_t: sector_t *), - active list      // jff 2/22/98
// T_Scroll                                                 // killough 3/7/98
// T_Pusher                                                 // phares 3/22/98
// T_FireFlicker                                            // killough 10/4/98
//

void P_ArchiveSpecials (void)
{
  thinker_t *th;
  size_t    size = 0;          // killough

  // save off the current thinkers (memory size calculation -- killough)

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (!th->function)
      {
        platlist_t *pl;
        ceilinglist_t *cl;     //jff 2/22/98 need this for ceilings too now
        for (pl=_g->activeplats; pl; pl=pl->next)
          if (pl->plat == (plat_t *) th)   // killough 2/14/98
            {
              size += 4+sizeof(plat_t);
              goto end;
            }
        for (cl=_g->activeceilings; cl; cl=cl->next) // search for activeceiling
          if (cl->ceiling == (ceiling_t *) th)   //jff 2/22/98
            {
              size += 4+sizeof(ceiling_t);
              goto end;
            }
      end:;
      }
    else
      size +=
        th->function==T_MoveCeiling  ? 4+sizeof(ceiling_t) :
        th->function==T_VerticalDoor ? 4+sizeof(vldoor_t)  :
        th->function==T_MoveFloor    ? 4+sizeof(floormove_t):
        th->function==T_PlatRaise    ? 4+sizeof(plat_t)    :
        th->function==T_LightFlash   ? 4+sizeof(lightflash_t):
        th->function==T_StrobeFlash  ? 4+sizeof(strobe_t)  :
        th->function==T_Glow         ? 4+sizeof(glow_t)    :
        th->function==T_MoveElevator ? 4+sizeof(elevator_t):
        th->function==T_Scroll       ? 4+sizeof(scroll_t)  :
        th->function==T_Pusher       ? 4+sizeof(pusher_t)  :
        th->function==T_FireFlicker? 4+sizeof(fireflicker_t) :
      0;

  CheckSaveGame(size + 1);    // killough; cph: +1 for the tc_endspecials

  // save off the current thinkers
  for (th=thinkercap.next; th!=&thinkercap; th=th->next)
    {
      if (!th->function)
        {
          platlist_t *pl;
          ceilinglist_t *cl;    //jff 2/22/98 add iter variable for ceilings

          // killough 2/8/98: fix plat original height bug.
          // Since acv==NULL, this could be a plat in stasis.
          // so check the active plats list, and save this
          // plat (jff: or ceiling) even if it is in stasis.

          for (pl=_g->activeplats; pl; pl=pl->next)
            if (pl->plat == (plat_t *) th)      // killough 2/14/98
              goto plat;

          for (cl=_g->activeceilings; cl; cl=cl->next)
            if (cl->ceiling == (ceiling_t *) th)      //jff 2/22/98
              goto ceiling;

          continue;
        }

      if (th->function == T_MoveCeiling)
        {
          ceiling_t *ceiling;
        ceiling:                               // killough 2/14/98
          *_g->save_p++ = tc_ceiling;
          PADSAVEP();
          ceiling = (ceiling_t *)_g->save_p;
          memcpy (ceiling, th, sizeof(*ceiling));
          _g->save_p += sizeof(*ceiling);
          ceiling->sector = (sector_t *)(ceiling->sector - _g->sectors);
          continue;
        }

      if (th->function == T_VerticalDoor)
        {
          vldoor_t *door;
          *_g->save_p++ = tc_door;
          PADSAVEP();
          door = (vldoor_t *) _g->save_p;
          memcpy (door, th, sizeof *door);
          _g->save_p += sizeof(*door);
          door->sector = (sector_t *)(door->sector - _g->sectors);
          //jff 1/31/98 archive line remembered by door as well
          door->line = (line_t *) (door->line ? door->line-_g->lines : -1);
          continue;
        }

      if (th->function == T_MoveFloor)
        {
          floormove_t *floor;
          *_g->save_p++ = tc_floor;
          PADSAVEP();
          floor = (floormove_t *)_g->save_p;
          memcpy (floor, th, sizeof(*floor));
          _g->save_p += sizeof(*floor);
          floor->sector = (sector_t *)(floor->sector - _g->sectors);
          continue;
        }

      if (th->function == T_PlatRaise)
        {
          plat_t *plat;
        plat:   // killough 2/14/98: added fix for original plat height above
          *_g->save_p++ = tc_plat;
          PADSAVEP();
          plat = (plat_t *)_g->save_p;
          memcpy (plat, th, sizeof(*plat));
          _g->save_p += sizeof(*plat);
          plat->sector = (sector_t *)(plat->sector - _g->sectors);
          continue;
        }

      if (th->function == T_LightFlash)
        {
          lightflash_t *flash;
          *_g->save_p++ = tc_flash;
          PADSAVEP();
          flash = (lightflash_t *)_g->save_p;
          memcpy (flash, th, sizeof(*flash));
          _g->save_p += sizeof(*flash);
          flash->sector = (sector_t *)(flash->sector - _g->sectors);
          continue;
        }

      if (th->function == T_StrobeFlash)
        {
          strobe_t *strobe;
          *_g->save_p++ = tc_strobe;
          PADSAVEP();
          strobe = (strobe_t *)_g->save_p;
          memcpy (strobe, th, sizeof(*strobe));
          _g->save_p += sizeof(*strobe);
          strobe->sector = (sector_t *)(strobe->sector - _g->sectors);
          continue;
        }

      if (th->function == T_Glow)
        {
          glow_t *glow;
          *_g->save_p++ = tc_glow;
          PADSAVEP();
          glow = (glow_t *)_g->save_p;
          memcpy (glow, th, sizeof(*glow));
          _g->save_p += sizeof(*glow);
          glow->sector = (sector_t *)(glow->sector - _g->sectors);
          continue;
        }

      // killough 10/4/98: save flickers
      if (th->function == T_FireFlicker)
        {
          fireflicker_t *flicker;
          *_g->save_p++ = tc_flicker;
          PADSAVEP();
          flicker = (fireflicker_t *)_g->save_p;
          memcpy (flicker, th, sizeof(*flicker));
          _g->save_p += sizeof(*flicker);
          flicker->sector = (sector_t *)(flicker->sector - _g->sectors);
          continue;
        }

      //jff 2/22/98 new case for elevators
      if (th->function == T_MoveElevator)
        {
          elevator_t *elevator;         //jff 2/22/98
          *_g->save_p++ = tc_elevator;
          PADSAVEP();
          elevator = (elevator_t *)_g->save_p;
          memcpy (elevator, th, sizeof(*elevator));
          _g->save_p += sizeof(*elevator);
          elevator->sector = (sector_t *)(elevator->sector - _g->sectors);
          continue;
        }

      // killough 3/7/98: Scroll effect thinkers
      if (th->function == T_Scroll)
        {
          *_g->save_p++ = tc_scroll;
          memcpy (_g->save_p, th, sizeof(scroll_t));
          _g->save_p += sizeof(scroll_t);
          continue;
        }

      // phares 3/22/98: Push/Pull effect thinkers

      if (th->function == T_Pusher)
        {
          *_g->save_p++ = tc_pusher;
          memcpy (_g->save_p, th, sizeof(pusher_t));
          _g->save_p += sizeof(pusher_t);
          continue;
        }
    }

  // add a terminating marker
  *_g->save_p++ = tc_endspecials;
}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
  byte tclass;

  // read in saved thinkers
  while ((tclass = *_g->save_p++) != tc_endspecials)  // killough 2/14/98
    switch (tclass)
      {
      case tc_ceiling:
        PADSAVEP();
        {
          ceiling_t *ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
          memcpy (ceiling, _g->save_p, sizeof(*ceiling));
          _g->save_p += sizeof(*ceiling);
          ceiling->sector = &_g->sectors[(int)ceiling->sector];
          ceiling->sector->ceilingdata = ceiling; //jff 2/22/98

          if (ceiling->thinker.function)
            ceiling->thinker.function = T_MoveCeiling;

          P_AddThinker (&ceiling->thinker);
          P_AddActiveCeiling(ceiling);
          break;
        }

      case tc_door:
        PADSAVEP();
        {
          vldoor_t *door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
          memcpy (door, _g->save_p, sizeof(*door));
          _g->save_p += sizeof(*door);
          door->sector = &_g->sectors[(int)door->sector];

          //jff 1/31/98 unarchive line remembered by door as well
          door->line = (int)door->line!=-1? &_g->lines[(int)door->line] : NULL;

          door->sector->ceilingdata = door;       //jff 2/22/98
          door->thinker.function = T_VerticalDoor;
          P_AddThinker (&door->thinker);
          break;
        }

      case tc_floor:
        PADSAVEP();
        {
          floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
          memcpy (floor, _g->save_p, sizeof(*floor));
          _g->save_p += sizeof(*floor);
          floor->sector = &_g->sectors[(int)floor->sector];
          floor->sector->floordata = floor; //jff 2/22/98
          floor->thinker.function = T_MoveFloor;
          P_AddThinker (&floor->thinker);
          break;
        }

      case tc_plat:
        PADSAVEP();
        {
          plat_t *plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
          memcpy (plat, _g->save_p, sizeof(*plat));
          _g->save_p += sizeof(*plat);
          plat->sector = &_g->sectors[(int)plat->sector];
          plat->sector->floordata = plat; //jff 2/22/98

          if (plat->thinker.function)
            plat->thinker.function = T_PlatRaise;

          P_AddThinker (&plat->thinker);
          P_AddActivePlat(plat);
          break;
        }

      case tc_flash:
        PADSAVEP();
        {
          lightflash_t *flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
          memcpy (flash, _g->save_p, sizeof(*flash));
          _g->save_p += sizeof(*flash);
          flash->sector = &_g->sectors[(int)flash->sector];
          flash->thinker.function = T_LightFlash;
          P_AddThinker (&flash->thinker);
          break;
        }

      case tc_strobe:
        PADSAVEP();
        {
          strobe_t *strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
          memcpy (strobe, _g->save_p, sizeof(*strobe));
          _g->save_p += sizeof(*strobe);
          strobe->sector = &_g->sectors[(int)strobe->sector];
          strobe->thinker.function = T_StrobeFlash;
          P_AddThinker (&strobe->thinker);
          break;
        }

      case tc_glow:
        PADSAVEP();
        {
          glow_t *glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
          memcpy (glow, _g->save_p, sizeof(*glow));
          _g->save_p += sizeof(*glow);
          glow->sector = &_g->sectors[(int)glow->sector];
          glow->thinker.function = T_Glow;
          P_AddThinker (&glow->thinker);
          break;
        }

      case tc_flicker:           // killough 10/4/98
        PADSAVEP();
        {
          fireflicker_t *flicker = Z_Malloc (sizeof(*flicker), PU_LEVEL, NULL);
          memcpy (flicker, _g->save_p, sizeof(*flicker));
          _g->save_p += sizeof(*flicker);
          flicker->sector = &_g->sectors[(int)flicker->sector];
          flicker->thinker.function = T_FireFlicker;
          P_AddThinker (&flicker->thinker);
          break;
        }

        //jff 2/22/98 new case for elevators
      case tc_elevator:
        PADSAVEP();
        {
          elevator_t *elevator = Z_Malloc (sizeof(*elevator), PU_LEVEL, NULL);
          memcpy (elevator, _g->save_p, sizeof(*elevator));
          _g->save_p += sizeof(*elevator);
          elevator->sector = &_g->sectors[(int)elevator->sector];
          elevator->sector->floordata = elevator; //jff 2/22/98
          elevator->sector->ceilingdata = elevator; //jff 2/22/98
          elevator->thinker.function = T_MoveElevator;
          P_AddThinker (&elevator->thinker);
          break;
        }

      case tc_scroll:       // killough 3/7/98: scroll effect thinkers
        {
          scroll_t *scroll = Z_Malloc (sizeof(scroll_t), PU_LEVEL, NULL);
          memcpy (scroll, _g->save_p, sizeof(scroll_t));
          _g->save_p += sizeof(scroll_t);
          scroll->thinker.function = T_Scroll;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_pusher:   // phares 3/22/98: new Push/Pull effect thinkers
        {
          pusher_t *pusher = Z_Malloc (sizeof(pusher_t), PU_LEVEL, NULL);
          memcpy (pusher, _g->save_p, sizeof(pusher_t));
          _g->save_p += sizeof(pusher_t);
          pusher->thinker.function = T_Pusher;
          pusher->source = P_GetPushThing(pusher->affectee);
          P_AddThinker(&pusher->thinker);
          break;
        }

      default:
        I_Error("P_UnarchiveSpecials: Unknown tclass %i in savegame", tclass);
      }
}

// killough 2/16/98: save/restore random number generator state information

void P_ArchiveRNG(void)
{

}

void P_UnArchiveRNG(void)
{

}

// killough 2/22/98: Save/restore automap state
// killough 2/22/98: Save/restore automap state
void P_ArchiveMap(void)
{

}

void P_UnArchiveMap(void)
{

}

