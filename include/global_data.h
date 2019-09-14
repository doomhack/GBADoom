#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "doomstat.h"
#include "m_fixed.h"
#include "am_map.h"


typedef struct globals_t
{
    #include "global_defs.h"
} globals_t;

void InitGlobals();

extern globals_t* _g;

#endif // GLOBAL_DATA_H
