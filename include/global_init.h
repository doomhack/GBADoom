#ifndef GLOBAL_INIT_H
#define GLOBAL_INIT_H

//******************************************************************************
//am_map.c
//******************************************************************************

_g->leveljuststarted = 1;
_g->scale_mtof = (fixed_t)INITSCALEMTOF;
_g->stopped = true;

_g->lastlevel = -1;
_g->lastepisode = -1;


//******************************************************************************
//d_main.c
//******************************************************************************

_g->wipegamestate = GS_DEMOSCREEN;
_g->oldgamestate = -1;


//******************************************************************************
//doomstat.c
//******************************************************************************

// Game Mode - identify IWAD as shareware, retail etc.
_g->gamemode = indetermined;
_g->gamemission = doom;


//******************************************************************************
//m_menu.c
//******************************************************************************

_g->showMessages = 1;    // Show messages has default, 0 = off, 1 = on
_g->screenblocks = 10;    // has default




#endif // GLOBAL_INIT_H
