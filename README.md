**GBADoom**

A port of prBoom to the GBA. The objective of this project is to get full-fat PC Doom running on the GBA.


**What's working:**

- Supports Doom Shareware, Retail, Utimate and Doom2 IWADS.

- Renderer is largely intact. Z-Depth lighting is gone and there is mip-mapping but it's otherwise complete.

- Monster behaviour etc is all intact. (I.e sound propagation etc)

- Framerate is pretty variable. Simple areas run at ~25fps. Complex areas (Eg: E4M2) chug along at about 5 FPS. It's running around the same as the original GBA Doom1 and Doom2 ports. Doom1 Episodes 1-3 are all completely playable. Episode 4 chugs.

- Sound and music support.


**Still to do:**

- ~~Still needs too much memory. GBA only has 256Kb of work ram. Some levels will crash out as they are larger than this. Memory issues are pretty much resolved now. Only Doom2 Map15 has issues. Occasionally still get a blue screen of death after playing for a while on that level.~~

- ~Status bar is partially rendered. It's actually quite tricky because we are page-flipping we need to draw the stbar twice when it updates else it will flicker.~

- ~Too dark on real hardware. It either needs a non-linear RGB24 -> RGB15 mapping or a custom palette.~

- ~Save-load are stubbed out.~

- Demo compatability is broken.

- ~~Weapon-swith controls not implimented.~~

- General optimisation. We're never going to get a perfect 35FPS but I think there is still another 25% left without changing the visual quality/correctness/game behaviour. For reference, the first time I ran a build under the emulator it ran at about 3FPS.

- ~Sound is a bit glitchy because MaxMod doesn't have a function to tell if a sound is still playing. Channels get re-used sooner than they should. To fix this i'll have to store the lengths of all of the samples. Fun.~

- Probably a bunch of other stuff that's borked too...


**Building:**

To build the GBA version, you will need DevKitArm. The easiest way to get up and running for Windows users is download the installer from here (https://github.com/devkitPro/installer/releases) and install the GBA dev components.

You will also need GBAWadUtil (https://github.com/doomhack/GbaWadUtil). To build this, you will need Qt Framework 5.13.

1) Use GBAWadUtil to create a header file with the WAD data. Retail, Ultimate and Doom2 wads have been tested. Plutonia and TNT should work. Usage: GBAWadUtil -in path/to/iwad.wad -cfile path/to/iwadheader.h
2) This will create a big .h source file. It's the WAD data. Copy it to /source/iwad/
3) Change the #include "iwad/doom1.gba.h" line in /source/doom_iwad.c if required.
4) Ensure the DevKitArm tools are in your path.
5) Open a cmd prompt in the source folder for GBA Doom.
6) Run 'make'.
7) The project should build GBADoom.gba and GBADoom.elf.
8) Rip and tear...or just stumble around in the dark until you get a headache.
