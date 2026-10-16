Utility to process Doom IWAD files for GBA Doom.

This is part of the GBADoom repository. It includes the engine's include/gusbank.h and include/doom_iwad.h, so the two can't disagree on those formats. Release builds go to GBADoom's GbaWadUtil/ directory, where the exe finds gbadoom.wad and the GUS patches (dgguspat/).

It can write either a new WAD file and/or a C Source file.

This is required to reduce the memory footprint of some of the data structures in Doom to get it to fit in 256Kb.

We will pre-calculate more fields so that the lumps stored in the WAD can be used directly from the ROM rather than having to load and convert them in memory.

For example:

In the WAD file a vertex is stored as two short values. (X and Y). When it is loaded into memory, it is shifted left by 16 to convert to 16.16 fixed point. By re-writing this lump and storing it in fixed point, we do not need to copy this into memory at runtime.

Usage: GbaWadUtil -in iwad_file_path -out gba_iwad_path -cfile iwad_cfile_path.

To attach the WAD to a GBADoom ROM without rebuilding it:

GbaWadUtil -in iwad_file_path -rom gbadoom_rom_path -romout output_rom_path

The ROM must be built with IWAD header support (ends with a "GBADOOM-IWAD-HDR" header). Anything already after the header is replaced, so a ROM with a WAD attached can be patched again with a different WAD.


PWADs go on top of the IWAD with -pwad pwad_path [pwad_path ...].

DeHackEd patches are applied from the WADs' DEHACKED lumps (IWAD first, then each PWAD), then from -deh patch_path [patch_path ...] in order. Vanilla patches are supported (patch format 6, DeHackEd 2.3 to 3.1): the Thing, Frame, Pointer, Weapon, Ammo, Misc and Text sections. Sound, Sprite and Cheat sections, BEX extensions and binary patches are skipped with a warning. The patched tables are written to the GBASTATE, GBAMOBJ, GBAWEAP, GBAMISC and GBATEXT lumps, which the engine reads instead of having them compiled in.

Only the lumps the engine reads are written out (see usedlumps.cpp): unused textures, flats, sprites, sounds, music and any lumps the engine doesn't know about are dropped.

It is a command line util so it can be integrated into a build process.
