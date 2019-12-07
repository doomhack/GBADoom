#include "m_cheat.h"

typedef struct c_cheat
{
    char* name;
    int sequence[8];
} c_cheat;

static const c_cheat cheat[] =
{
    {"Chainsaw", {1,2,3,4,5,6,7,8}},
    {"God mode", {1,2,3,4,5,6,7,8}},
    {"Ammo & Keys", {1,2,3,4,5,6,7,8}},
    {"Ammo", {1,2,3,4,5,6,7,8}},
    {"No Clipping", {1,2,3,4,5,6,7,8}},
    {"Invincibility", {1,2,3,4,5,6,7,8}},
    {"Berserk", {1,2,3,4,5,6,7,8}},
    {"Invisibility", {1,2,3,4,5,6,7,8}},
    {"Auto-map", {1,2,3,4,5,6,7,8}},
    {"Lite-Amp Goggles", {1,2,3,4,5,6,7,8}},
    {"Exit Level", {1,2,3,4,5,6,7,8}},

    //Because Goldeneye!
    {"Enemy Rockets", {1,2,3,4,5,6,7,8}},
};

boolean C_Responder (event_t *ev)
{

}
