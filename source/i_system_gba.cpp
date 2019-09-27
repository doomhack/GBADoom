#include <stdarg.h>
#include <stdio.h>
#include <cstring>

#include "doomtype.h"

#include "i_system_e32.h"

#include "lprintf.h"

#include <gba.h>

#define DCNT_PAGE 0x0010

#define VID_PAGE1 VRAM
#define VID_PAGE2 0x600A000

//**************************************************************************************

unsigned int vid_width = 0;
unsigned int vid_height = 0;

unsigned int screen_width = 0;
unsigned int screen_height = 0;

unsigned int y_pitch = 0;

unsigned int page = 0;

//**************************************************************************************

void I_InitScreen_e32()
{
    //Gives 480px on a 5(mx) and 320px on a Revo.
    vid_width = 120;

    vid_height = screen_height = 160;

    // the vblank interrupt must be enabled for VBlankIntrWait() to work
    // since the default dispatcher handles the bios flags no vblank handler
    // is required
    irqInit();
    irqEnable(IRQ_VBLANK);

    consoleDemoInit();
}

//**************************************************************************************

void I_BlitScreenBmp_e32()
{

}

//**************************************************************************************

void I_StartWServEvents_e32()
{

}

//**************************************************************************************

void I_PollWServEvents_e32()
{

}

//**************************************************************************************

void I_ClearWindow_e32()
{

}

unsigned short* I_GetBackBuffer()
{
    if(REG_DISPCNT & DCNT_PAGE)
        return (unsigned short*)VID_PAGE1;

    return (unsigned short*)VID_PAGE2;
}

//**************************************************************************************

void I_CreateWindow_e32()
{
    SetMode(MODE_4 | BG_ALL_ON);
}

//**************************************************************************************

void I_CreateBackBuffer_e32()
{
    I_CreateWindow_e32();
}

//**************************************************************************************

void I_FinishUpdate_e32(const byte* srcBuffer, const byte* pallete, const unsigned int width, const unsigned int height)
{
    VBlankIntrWait();

    REG_DISPCNT ^= DCNT_PAGE;
}

//**************************************************************************************

void I_SetPallete_e32(const byte* pallete)
{
    unsigned short* pal_ram = (unsigned short*)0x5000000;

    unsigned short pal_temp[256];

    for(int i = 0; i< 256; i++)
    {
        pal_temp[i] = RGB8(pallete[3*i],pallete[(3*i)+1], pallete[(3*i)+2]);
    }

    VBlankIntrWait();

    memcpy(pal_ram, pal_temp, 256*2);
}

//**************************************************************************************

int I_GetVideoWidth_e32()
{
    return vid_width;
}

//**************************************************************************************

int I_GetVideoHeight_e32()
{
    return vid_height;
}

//**************************************************************************************

void I_ProcessKeyEvents()
{
    I_PollWServEvents_e32();
}

//**************************************************************************************

#define MAX_MESSAGE_SIZE 2048

void I_Error (const char *error, ...)
{
    consoleDemoInit();

    char msg[MAX_MESSAGE_SIZE];

    va_list v;
    va_start(v, error);

    vsprintf(msg, error, v);

    va_end(v);

    printf("%s", msg);


    fflush( stderr );
    fflush( stdout );

    gets(msg);

    I_Quit_e32();
}

//**************************************************************************************

void I_Quit_e32()
{

}

//**************************************************************************************

