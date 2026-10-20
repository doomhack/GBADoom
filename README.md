## GBADoom

A port of prBoom to the GBA.

**What's hot?**

- Supports Doom Shareware, Retail, Ultimate and Doom2 IWADS. Plutonia and TNT should mostly work.

- No WAD is built into the ROM. GbaWadUtil patches a WAD into the prebuilt engine ROM, with no compiler needed. PWADs (Eg: Sigil) go on top of their IWAD.

- Renderer is intact, including Doom's distance lighting and full resolution floors and ceilings.

- Monster behaviour is all intact. (I.e sound propagation etc.)

- Framerate is pretty variable. Simple areas run at ~35fps and complex areas slow down, but V3's renderer is much faster than earlier versions. Doom1 Episodes 1-3 are all completely playable.

- Music and sound effects come from the WAD: the game's own music, played with GUS instrument patches, and its own sound effects. PWADs with custom music and sounds work.

- Vanilla DeHackEd patches. GbaWadUtil applies a WAD's DEHACKED lump, or a .deh file with -deh, when it builds the ROM.

**What's not?**

- My Markdown skills.

- Demo compatibility is broken.

- General optimisation. We're never going to get a perfect 35FPS but I think there is still another 25% left without changing the visual quality/correctness/game behaviour. For reference, the first time I ran a build under the emulator it ran at about 3FPS.

- Although it is based on prBoom, most of the engine enhancements (BEX, limit removing etc) have been reverted back to Vanilla. This is either for memory or performance reasons. Sadly, NUTS.wad and Okuplok are right out! Binary DeHackEd patches aren't supported either.

- No multiplayer. 



KippyKip is maintaining an upstream fork with new features and fixes. [KippyKip GBA fork](https://github.com/Kippykip/GBADoom)


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
**Toggle FPS counter:** A, B, L, UP, DOWN, B, LEFT, LEFT  

## Controls:  
**Fire:** B  
**Use / Sprint:** A  
**Walk:** D-Pad  
**Strafe:** L & R  
**Automap:** SELECT  
**Weapon up:** A + R  
**Weapon down:** A + L  
**Menu:** Start  

## Patching a ROM:

The GBADoom ROM has no WAD built in. GbaWadUtil converts a WAD and attaches it to the engine-only ROM (GBADoom.gba), so you can play any supported WAD without building anything. A Windows (x64) build of GbaWadUtil is included in the "GbaWadUtil\" directory (and in the release zip). Retail, Ultimate and Doom2 WADs have been tested. Plutonia and TNT should mostly work.

**With the window:** double-click GbaWadUtil.exe. Pick your WAD (and any PWADs or DeHackEd patches), check the engine ROM (it finds GBADoom.gba in the folder above its own), choose where to write the ROM and press **Build**.

**From the command line:** open a command prompt in the GbaWadUtil folder and type:

    GbaWadUtil.exe -in doom1.wad -rom ..\GBADoom.gba -romout ..\GBADoom1.gba -gus timidity.cfg

- **-gus** builds the music instrument bank from the GUS patches in "data\dgguspat\". Without it there is no music. timidity.cfg sounds the most modern; timid_d1.cfg and timid_d2.cfg are retro-sounding alternatives.
- **-pwad** adds PWADs on top of the IWAD, e.g. Sigil: **-in doomu.wad -pwad sigil.wad**
- **-deh** applies DeHackEd patches (.deh files). DEHACKED lumps in the WADs are applied automatically.
- **-oversample 4** resamples the instruments to 4 times as many samples, for more accurate music at the cost of a bigger ROM (each doubling doubles the instruments' size).

A ROM that already has a WAD attached can be patched again with a different one. Copy the new .gba file to your flash cart or run it in an emulator.

## Building the GBA version:

1) Install devkitPro with the GBA development components. The easiest way on Windows is the installer from here (https://github.com/devkitPro/installer/releases). build.bat expects it in C:\devkitPro; edit the paths at the top of build.bat if it's somewhere else.

2) Download or clone the GBADoom source code.

3) Open a command prompt (or PowerShell) in the GBADoom folder and type **build**. Use **build clean** to start from scratch.

4) This builds GBADoom.gba and GBADoom.elf. GBADoom.gba is the engine only: attach a WAD to it with GbaWadUtil (see "Patching a ROM" above) before playing it.

**release** builds the engine, attaches the shareware doom1.wad and puts the ROMs, GbaWadUtil and its files in "release\GBADoom\" and "release\GBADoom.zip". **release name** uses a different name.

## Building the Qt (Windows) version:

For development, the game also builds as a Windows app with Qt, which is much quicker to test with than the GBA.

1) Install Qt 6.12 with the MinGW 64-bit kit (MSVC can't compile the engine).

2) Open GBADoom.pro, at the root of the source tree, in Qt Creator and build it.

3) The Qt version loads a processed WAD file instead of a ROM. Make one with GbaWadUtil:

       GbaWadUtil.exe -in doom1.wad -out ..\wads\doom1.wad -gus timidity.cfg

   (or run one of the **build-XXXX.bat** files in "GbaWadUtil\", which write to "wads\").

4) Run GBADoom with **-iwad** and the path to that file, e.g. **-iwad C:\Projects\GBADoom\wads\doom1.wad** (set it in Qt Creator's run settings, under Command line arguments). A relative path is from the working directory, which in Qt Creator is the build folder by default.

GbaWadUtil's own source is in "tools\GbaWadUtil\": open GbaWadUtil.pro in Qt Creator with the same kit. Release builds are written straight into "GbaWadUtil\".
