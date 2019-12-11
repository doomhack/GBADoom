**GBADoom**

A port of prBoom to the GBA. The objective of this project is to get full-fat PC Doom running on the GBA.


**What's working:**

- Supports Doom Shareware, Retail, Utimate and Doom2 IWADS.

- Renderer is largely intact. Z-Depth lighting is gone and there is mip-mapping but it's otherwise complete.

- Monster behaviour etc is all intact. (I.e sound propagation etc)

- Framerate is pretty variable. Simple areas run at ~25fps. Complex areas (Eg: E4M2) chug along at about 5 FPS. It's running around the same as the original GBA Doom1 and Doom2 ports. Doom1 Episodes 1-3 are all completely playable. Episode 4 chugs.

- Sound and music support.


**Still to do:**

- Demo compatability is broken.

- General optimisation. We're never going to get a perfect 35FPS but I think there is still another 25% left without changing the visual quality/correctness/game behaviour. For reference, the first time I ran a build under the emulator it ran at about 3FPS.

- Probably a bunch of other stuff that's borked too...


**Building:**

To build the GBA version, you will need DevKitArm. The easiest way to get up and running for Windows users is download the installer from here (https://github.com/devkitPro/installer/releases) and install the GBA dev components.

You will also need GBAWadUtil (https://github.com/doomhack/GbaWadUtil). Windows (x64) users can download the Binary release from the releases page.

1) Use GBAWadUtil to create a header file with the WAD data. Retail, Ultimate and Doom2 wads have been tested. Plutonia and TNT should work. 

Copy your IWAD file to a folder (Eg: C:\Temp\Doom\)
Extract the GBAWadUtil archive to a folder (Eg: C:\Temp\GBAWadUtil\)
Open a command prompt.

Type the following:

C:\Temp\GBAWadUtil\GbaWadUtil.exe -in C:\Temp\Doom\Doom.wad -cfile C:\Temp\Doom\Doom.wad.h

2) Download or Clone the GBADoom source code. I recommend downloading from the releases page as the latest code may have issues.

Extract the code to a folder: (Eg: C:\Temp\GBADoom)

3) Copy the C:\Temp\Doom.wad.h (Generated in step 1) to C:\Temp\GBADoom\source\iwad\doom.wad.h
4) Open C:\Temp\GBADoom\source\doom_iwad.h in text editor or code editor of your choice.
5) Change the first line to #include "iwad/doom.wad.h"

5) Open a cmd prompt and type the following:


cd C:\Temp\GBADoom

make

7) The project should build GBADoom.gba and GBADoom.elf. It will take about 5 minutes or so. You may see a lot of warning messages on the screen. These are normal.

8) Copy GBADoom.gba (this is the rom file) to your flash cart or run in a emulator.
