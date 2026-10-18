Utility to process Doom IWAD files for GBA Doom.

This is part of the GBADoom repository. It includes the engine's include/gusbank.h and include/doom_iwad.h, so the two can't disagree on those formats. Release builds go to GBADoom's GbaWadUtil/ directory, where the exe finds gbadoom.wad and the GUS patches in its data/ folder (data/gbadoom.wad, data/dgguspat/). gbadoom.wad is merged into every WAD and the tool stops if it's missing. -gus takes a path to a timidity .cfg, or just the name of one in data/dgguspat/ (-gus timidity.cfg).

It can write a new WAD file (for the Qt desktop build, which loads it at startup) and/or attach it to a GBADoom ROM.

This is required to reduce the memory footprint of some of the data structures in Doom to get it to fit in 256Kb.

We will pre-calculate more fields so that the lumps stored in the WAD can be used directly from the ROM rather than having to load and convert them in memory.

For example:

In the WAD file a vertex is stored as two short values. (X and Y). When it is loaded into memory, it is shifted left by 16 to convert to 16.16 fixed point. By re-writing this lump and storing it in fixed point, we do not need to copy this into memory at runtime.

Usage: GbaWadUtil -in iwad_file_path -out gba_iwad_path

To attach the WAD to a GBADoom ROM without rebuilding it:

GbaWadUtil -in iwad_file_path -rom gbadoom_rom_path -romout output_rom_path

The ROM must be built with IWAD header support (ends with a "GBADOOM-IWAD-HDR" header). Anything already after the header is replaced, so a ROM with a WAD attached can be patched again with a different WAD.


PWADs go on top of the IWAD with -pwad pwad_path [pwad_path ...].

DeHackEd patches are applied from the WADs' DEHACKED lumps (IWAD first, then each PWAD), then from -deh patch_path [patch_path ...] in order. Vanilla patches are supported (patch format 6, DeHackEd 2.3 to 3.1): the Thing, Frame, Pointer, Weapon, Ammo, Misc and Text sections. Sound, Sprite and Cheat sections, BEX extensions and binary patches are skipped with a warning. The patched tables are written to the GBASTATE, GBAMOBJ, GBAWEAP, GBAMISC and GBATEXT lumps, which the engine reads instead of having them compiled in.

Only the lumps the engine reads are written out (see usedlumps.cpp): unused textures, flats, sprites, sounds, music and any lumps the engine doesn't know about are dropped.

Every build prints what the space goes on ("Space used:", then engine, maps, wall textures, flats, sprites, graphics, sound effects, music, instruments and other; sizereport.cpp). Shared lump data counts once, so the parts add up to the file's size exactly. -size does the whole conversion and prints that report without writing anything; -rom then only needs the engine ROM, to count it in.

It is a command line util so it can be integrated into a build process.

Run with no arguments (double-click the exe) it opens a window instead (gui/). GbaWadUtil -gui followed by any of the options above opens the window filled in from them and lists the arguments it couldn't use. The window shows the command line it will run and runs builds as a GbaWadUtil child process with exactly those arguments, from the exe's folder, so a build from the window and from the command line are the same. Builds read the child's messages through QT_MESSAGE_PATTERN, so the window relies on the "Loaded", "Merged", "Wrote" and DeHackEd messages for its stages, and on the "Space used" report: keep them when changing what the tool prints. While the settings change, the window runs GbaWadUtil -size in the background (after a short pause, cancelled by the next change, cached per inputs) to show the real ROM size and its breakdown before a build. The window needs Qt6Gui, Qt6Widgets, Qt6Svg and platforms/qwindows.dll next to the exe.
