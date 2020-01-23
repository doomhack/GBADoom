
## GBADoom SVN

A slightly modified version of [doomhack](https://github.com/doomhack/GBADoom)'s amazing port of prBoom to the GBA. 

![Episode 2](https://i.imgur.com/kOwpVzW.png)  

**What's different?:**
 - ~~The HUD is changed to be identical to the GBA Doom II retail HUD, therefore all HUD elements are no longer using scaling algorithms and contain all the HUD elements. This does make the screen vertical viewing space somewhat smaller, not too sure if this improves framerate or not.~~
 - ~~The MUG head turning on damage is restored.~~
 - ~~The chainsaw sounds no longer overlap.~~
 - ~~**B** is now the fire button, and sprinting/use has been changed to **A**. To match the retail GBA Doom II controls.~~
 - ~~Weapon selection order is restored to: Fist, Chainsaw, Pistol, Shotgun, Super Shotgun, Chaingun, Plasmagun, BFG 9000~~  
 - ~~Gamma is in the options now instead of a hotkey~~
 - ~~And other bug general fixes and optimisations~~
*All of the above changes have been merged into doomhack's main branch*
 - Optimised Title Screen images (titlepic is rendered via CPUBlockCopy rather than drawing from the IWAD)
 - Unused shareware IWAD content is removed to build a smaller rom

**TODO:**
 - Recreate Final Doom: Evilution / TNT.WAD's music in impulsetracker

Be sure to check [doomhack's main branch](https://github.com/doomhack/GBADoom) for future engine optimisations and bug fixes!

## Cheats:
**Chainsaw:** L, UP, UP, LEFT, L, SELECT, SELECT, UP  
**God mode:** UP, UP, DOWN, DOWN, LEFT, LEFT, RIGHT, RIGHT  
**Ammo & Keys:** L, LEFT, R, RIGHT, SELECT,UP, SELECT, UP  
**Ammo:** R, R, SELECT,R, SELECT,UP, UP, LEFT  
**No Clipping:** UP, DOWN, LEFT, RIGHT, UP, DOWN, LEFT, RIGHT  
**Invincibility:** A, B, L, R, L, R, SELECT, SELECT  
**Berserk:** B, B, R, UP, A, A, R, B  
**Invisibility:** A, A, SELECT,B, A, SELECT, L, B  
**Auto-map:** L, SELECT,R, B, A, R, L, UP  
**Lite-Amp Goggles:** DOWN,LEFT, R, LEFT, R, L, L, SELECT  
**Exit Level:** LEFT,R, LEFT, L, B, LEFT, RIGHT, A  
**Enemy Rockets (Goldeneye):** A, B, L, R, R, L, B, A  

## Controls:  
**Fire:** B  
**Use / Sprint:** A  
**Walk:** D-Pad  
**Strafe:** L & R  
**Automap:** SELECT  
**Weapon up:** A + R  
**Weapon down:** A + L  
**Menu:** Start  

## Building:
Video Tutorial: https://www.youtube.com/watch?v=YbZgZqV6JMk

To build the GBA version, you will need DevKitArm. The easiest way to get up and running for Windows users is download the installer from here (https://github.com/devkitPro/installer/releases) and install the GBA dev components.

You will also need to use GBAWadUtil, included in the "GBAWadUtil\" directory. Alternatively download the latest build from the main source: (https://github.com/doomhack/GbaWadUtil). Windows (x64) users can download the Binary release from the releases page.

1) Download or Clone this GBADoom SVN source code.
Extract the contents to a folder: (Eg: C:\DevKitPro\Projects\GBADoom)

2) Use GBAWadUtil to create a header file with the WAD data. Retail, Ultimate and Doom2 wads have been tested. Plutonia and TNT should mostly work. 
Open a command prompt.
Type the following:
**GbaWadUtil.exe -in doom.wad -cfile doom.wad.c**
And copy it to the **source\\iwad\\** directory.
Alternatively just run the **build_XXXX.bat** files and it'll create it in the source\iwad\ path.

3) Open C:\DevKitPro\Projects\GBADoom\source\doom_iwad.h in text editor or code editor of your choice.
4) Change the first line to #include "iwad/**yourfile**.c" e.g.
#include "iwad/doom1.c"
#include "iwad/doom.c"
#include "iwad/doomu.c"
#include "iwad/doom2.c"
#include "iwad/tnt.c"
#include "iwad/plutonia.c"

5) Open C:\DevKitPro\Projects\GBADoom\source\gfx\titlepic.h in text editor or code editor of your choice.
6) Uncomment the line ( // ) for the destination title screen of your IWAD.
![enter image description here](https://i.imgur.com/zSAlLDQ.png)
5) Run msys2.bat and type **make**
You may need to edit the msys2.bat with notepad and change the path to go to your real "**msys2\msys2_shell.bat**" file within it if it doesn't work.

6) The project should build GBADoom.gba and GBADoom.elf. It will take about 5 minutes or so. You may see a lot of warning messages on the screen. These are normal.

7) Copy GBADoom.gba (this is the rom file) to your flash cart or run in a emulator.
