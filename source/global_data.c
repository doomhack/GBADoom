#include <stdlib.h>
#include "global_data.h"

#include "z_zone.h"


globals_t* _g = NULL;

void InitGlobals()
{
    _g = Z_Malloc(sizeof(globals_t), PU_STATIC, NULL);

    memset(_g, 0, sizeof(globals_t));

    #include "global_init.h"
}
