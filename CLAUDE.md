# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

GBADoom is a port of PrBoom (itself based on id Software's DOOM) to the Game Boy Advance. The renderer, monster AI, and game logic are mostly intact from PrBoom; engine enhancements like BEX and limit-removing have been reverted to vanilla behavior to fit GBA memory/performance constraints. Vanilla DeHackEd patches are applied by GbaWadUtil (see Dehacked). There is no multiplayer and demo compatibility is broken.

The same source tree also builds as a Windows desktop app (via Qt) for faster iteration than testing on GBA hardware/emulator alone.

## Building

**GBA target (DevKitArm + make):**
```
build           # build.bat: sets DEVKITPRO/DEVKITARM and runs devkitPro's make, from cmd or PowerShell
build clean
```
Produces `GBADoom.elf` and `GBADoom.gba`.

**Windows/Qt target:** open `GBADoom.pro` in Qt Creator (MinGW 32-bit or MSVC 32-bit). This is the primary way to iterate on gameplay/rendering logic without a GBA emulator — it builds the same source files as regular C/C++ against Qt for windowing/input instead of libgba.

**The GBA build contains no IWAD.** `make` produces an engine-only ROM that ends with a 32-byte IWAD header (`doom_iwad_header_t` in `include/doom_iwad.h`, emitted by the `.iwad` section at the end of `gbadoom.ld`). Attach a WAD with:
```
GbaWadUtil -in <wad> -gus GbaWadUtil/dgguspat/timidity.cfg -rom GBADoom.gba -romout <game>.gba
```
(`timidity.cfg` is the preferred, modern-sounding instrument mapping and what `rom-build-all.bat`/`wad-build-all.bat` use for every game; `timid_d1.cfg`/`timid_d2.cfg` are retro-sounding alternatives. Without `-gus` there is no `GUSBANK` lump and the music is silent. A PWAD such as Sigil goes on top of its IWAD: `-in doomu.wad -pwad sigil.wad`.)
GbaWadUtil finds the header by its magic (`GBADOOM-IWAD-HDR`, 32-byte aligned), writes version/length, drops anything already after it and appends the processed WAD, so a ROM that already has a WAD can be re-patched. `doom_iwad` is a link-time address right after the header; `doom_iwad_len` reads the header. `IdentifyVersion()` errors at startup if no WAD is attached or `DOOM_IWAD_VERSION` doesn't match — bump it whenever the processed WAD format changes, then rebuild GbaWadUtil (it includes `doom_iwad.h`).

**GbaWadUtil itself** lives in this repo: source in `tools/GbaWadUtil/` (Qt 6 console app, `GbaWadUtil.pro`), and the ready-to-run Windows build in `GbaWadUtil/` alongside what it needs at runtime (`gbadoom.wad`, which it loads from its own directory and merges into every WAD; `dgguspat/`; the `.bat` scripts and DLLs). Release builds of the `.pro` are written straight into `GbaWadUtil/`, so commit the rebuilt exe with any change to the tool. The tool includes the engine's `include/gusbank.h`, `include/doom_iwad.h`, `include/info.h`, `include/deh_text.h` and `include/sounds.h` directly (keep them free of engine-only includes, they compile as C++ too); the map/texture structs in its `doomtypes.h` are still hand-mirrored (only their sizes are checked, by the `static_assert`s in `r_defs.h`).

**GbaWadUtil keeps only the lumps the engine reads** (`KeepUsedLumps` in `tools/GbaWadUtil/usedlumps.cpp`, run last): named lumps from tables there that mirror the engine's (menu, status bar and intermission patch names, `S_sfx`, `musicNames`, `sprnames`, `animdefs`, the finale flats, by their possibly dehacked names), the loadable maps, and the textures and flats those maps use (`RemoveUnusedTextures` also keeps the skies, the other half of each switch and whole animations). Every flat and sprite namespace in the input (`FF_START`, `SS_START`, ...) is merged into one `F_START`/`S_START` block, last copy of a name wins. Anything else, such as `DEHACKED` (already applied, see Dehacked), is dropped. **When the engine starts reading a lump by a new name, add it to those tables**, or the ROM dies in `W_GetNumForName`.

**The Qt build still compiles the IWAD in:** use `GbaWadUtil -in <wad> -cfile <name>.c` (or `GbaWadUtil\build_*.bat`), copy the output to `source/iwad/`, and pick it in `source/doom_iwad.c` (the `#include` there is `#ifndef GBA`).

There is no automated test suite — verification is done by running the game (Qt build for quick checks, GBA emulator/hardware for platform-accurate checks).

## Architecture

### Single global state block (`_g`)

Almost all state that would traditionally be a file-static or a loose global is instead a field on one big `globals_t` struct (`include/global_data.h`), allocated once at startup and accessed everywhere through a single pointer, `_g`:

```c
globals_t* _g = NULL;
_g = Z_Malloc(sizeof(globals_t), PU_STATIC, NULL);   // source/global_data.c
```

- New persistent state should be added as a field inside the `globals_t` struct in `global_data.h` (grouped under the comment banner for the `.c` file that owns it), not as a new global variable.
- Default/non-zero initial values go in `include/global_init.h`, which is `#include`d inside `InitGlobals()` and executes as a flat sequence of `_g->field = value;` statements (everything else is zero-initialized via `memset`).
- This design exists for GBA memory-layout control (one contiguous allocation, easy to know its size/placement) rather than convenience — don't reintroduce plain globals or `static` locals for state that needs to persist across frames.

### Fixed-size pools instead of dynamic allocation

Gameplay-critical collections are fixed-size arrays sized by constants in the relevant header (`MAXPLATS`, `MAXCEILINGS` in `p_spec.h`; `MAXDRAWSEGS`, `MAXVISSPRITES`, `MAXOPENINGS` in `r_defs.h`; `MAXVISPLANES` in `r_plane.h`; `MAXINTERCEPTS` in `p_maputl.h`), rather than linked lists or realloc'd buffers. When touching code in these subsystems (active plats/ceilings, visplanes, vissprites, drawsegs, line intercepts), respect the fixed capacity and add/keep overflow checks — there have been real bugs here from treating these as unbounded (see recent history around active plat/ceiling lists).

### GBA vs. desktop platform split

- Platform-specific code is isolated behind `#ifdef GBA` inside otherwise-shared source files (see `include/gba_functions.h` for the pattern: `IDiv32`, `BlockCopy`, `BlockSet`, `SaveSRAM`/`LoadSRAM` all have a GBA BIOS/DMA implementation and a portable fallback).
- `i_system_gba.cpp` / `i_video` GBA paths back the GBA build; `i_system_e32.cpp`/`i_system_e32.h` is a legacy/unused Psion-era backend kept around but not part of the active platform matrix; the Qt (`i_system_win.h`) backend backs the Windows dev build.
- `GBADoom.pro` (Qt build) and the `Makefile`/DevKitArm build compile mostly the same `source/*.c` list — check both when adding or removing a source file, since the Qt `.pro` file lists sources/headers explicitly rather than globbing.

### IWRAM-critical rendering path

`source/r_hotpath.iwram.c` contains the hot inner-loop rendering code and is compiled with special handling because **the whole file must fit in the GBA's small IWRAM region**: the Makefile has dedicated `%.iwram.o` rules that force `-fno-lto -marm` (ARM mode, no whole-program LTO) instead of the default Thumb+LTO flags used elsewhere, and the file itself forces `#pragma GCC optimize ("Os")` under `#ifdef GBA` to keep code size down. Be careful about adding code here — growing this file risks it no longer fitting in IWRAM.

IWRAM code and data share the 32 KB with the main stack, which grows down from `__sp_usr` into whatever is left. `gbadoom.ld` asserts that at least `__stack_reserve` (2 KB) stays free, so the link fails with "IWRAM overflow" before IWRAM growth can eat the stack. `IWRAM_BUDGET.md` tracks what's in IWRAM and how deep the stack gets.

### Wall textures

GbaWadUtil composites every TEXTURE1/TEXTURE2 texture into 128-byte columns (short textures repeated to tile, holes filled from the nearest opaque row), dedupes them into a `COLPOOL` lump and writes per-texture `u16` tables to `TEXCOLS`: `colid[width]` then `runid[width]`. Walls and the sky fetch `texcolpool + (texture->colids[x & widthmask] << 7)` (`R_GetTextureColumn` in `r_hotpath.iwram.c`); there is no runtime compositing or column cache. Masked midtextures (`R_DrawMaskedTextureColumn`) draw the same pool column, limited to the runs of opaque rows in `COLRUNS` (`texrun_t` pairs ending in `topdelta` 0xff). Nothing reads wall patches at runtime, so GbaWadUtil strips `PNAMES` and the `P_START`..`P_END` namespace. Tables are per texture number, so switches and animations (which only swap texture numbers) need nothing special.

### Full-screen pictures

`TITLEPIC`, `INTERPIC`, `WIMAP0-2`, `CREDIT`, `HELP2`, `VICTORY2`, `ENDPIC` and `BOSSBACK` are not patches in the processed WAD: GbaWadUtil (`ProcessPages`) draws each 320x200 patch with its offsets, box-filters it down to 240x160 in linear light and matches each pixel to the nearest `PLAYPAL` 0 colour. The result is raw palette indices, one row after another (`RAWPAGE_WIDTH` in `include/doom_iwad.h`), which is exactly the Mode 4 framebuffer, so `V_DrawRawPage` copies it with one DMA per row. Don't make that one DMA for the whole screen: it would stall the VBlank interrupt that restarts the sound DMA for longer than `mix_out`'s padding. The E3 ending's `PFUB2` and `PFUB1` are drawn side by side and downscaled together as one 480x160 `BUNNYPIC` (no seam); `F_BunnyScroll` copies a 240 wide window of it with `V_DrawRawStrip`, which shifts halfwords when the window starts on an odd pixel. Everything drawn over these pictures (intermission animations, `END0-6`, cast sprites, menus) is still a patch. The read-this screens `HELP1` and Doom 2's `HELP` are removed, since the menu has no read-this page.

### Lighting

Doom's diminishing light is as in vanilla: walls, masked midtextures and sprites pick a colormap per column/sprite from their scale (`scalelight`), flats per span from their distance (`zlight`). Both are plain `const` arrays of colormap numbers in `tables.c`, with the formulas they came from in comments there; regenerate them if `projectiony` changes (`r_hotpath.iwram.c` asserts it is 128). `scalelight` is indexed by `scale >> LIGHTSCALESHIFT_GBA`, with the difference between this port's `projectiony` and Doom's 160 folded into the table. All 32 light colormaps sit in one VRAM block (`vram_tail->lightColormaps`, filled by `R_InitBuffer`), so changing colormap is a pointer, never a copy. The powerup `fixedcolormap` is read from `objpal_spare->fixedColormap`.

### Palette

GbaWadUtil replaces `PLAYPAL` (the last one, so a PWAD's custom palette wins) with `GBAPAL`: all 14 palettes in GBA RGB15 for each of Doom's 5 gamma levels (vanilla's `gammatable`), each channel converted on its own to the 5 bit level that displays closest to it (`To5Bit` in `wadprocessor.cpp`; the GBA shows 5 bits as `(c << 3) | (c >> 2)`). This leaves dark scenes 4–9% darker than PC, because Doom's dark greys sit just above GBA levels (36 shows as 33). Keeping brightness by mixing levels across channels was tried and gave visible green/purple tints on greys, so don't. Layout constants are in `include/doom_iwad.h`. `V_SetPalLump(gamma)` picks a level's block, and `I_SetPallete_e32` DMAs one 512 byte palette to palette RAM, with no conversion at runtime. The Qt build expands the same RGB15 palettes for display. The brightness menu is the gamma level (0–4); there are no palette lumps in `gbadoom.wad`.

### Sound

Music and sound effects both come from the WAD, mixed in software to one mono signed 8-bit stream at 13379 Hz (224 samples per GBA frame):

- GbaWadUtil converts `D_*` MUS lumps to type 0 MIDI, resamples `DS*` lumps to signed 8-bit at 13379 Hz (windowed sinc, so sound effects mix without interpolation), and with `-gus <cfg>` builds a `GUSBANK` lump: GUS patches from `GbaWadUtil/dgguspat/`, preprocessed for the fixed output rate (format in `include/gusbank.h`, which GbaWadUtil includes too).
- `source/libtimidity/` is a fixed-point, mono port of libtimidity that reads the bank and streams MIDI from ROM (no event list in RAM).
- `source/s_mix.c` mixes music and up to 8 sfx voices; the inner loops are ARM assembly in IWRAM on the GBA (`source/s_mix_gba.s`) and C elsewhere (`source/s_mix_ref.c`, which is also the reference: keep the two bit-identical), and libtimidity's per-voice driver is in IWRAM too (`TIMI_IWRAM`). Instrument samples are resampled by taking the nearest sample (`S_MixResample`), not interpolated: by ear it sounds the same through the 8-bit output and roughly halves the kernel's cost.
- `source/i_audio.c`: on the GBA, Timer 0 (1254 cycles) + DMA1 feed Direct Sound A, and the mixer runs in the VBlank interrupt (so changes from the game thread mask `REG_IME`). The Qt build plays the same output through waveOut (`source/i_audio_win.c`).

### Dehacked

The dehackable tables are not compiled into the engine; they come from the WAD, so the GBA and Qt builds get DeHackEd patches the same way. GbaWadUtil holds the defaults (`tools/GbaWadUtil/infotables.cpp`: states, mobjinfo, weaponinfo, the misc/ammo values), applies vanilla DeHackEd patches (`dehacked.cpp`: `DEHACKED` lumps in WAD order, then `-deh` files; `Thing`, `Frame`, `Pointer`, `Weapon`, `Ammo`, `Misc`, `Text`; BEX, `Sound`, `Sprite`, `Cheat` and binary patches are skipped with a warning) and writes `GBASTATE`, `GBAMOBJ`, `GBAWEAP`, `GBAMISC` and `GBATEXT`. `P_InitInfoTables` (`source/info.c`) points `_g->states` etc. at them in place (ROM on the GBA); the `states`, `mobjinfo`, `weaponinfo` and `miscinfo` macros in `global_data.h` and the misc-value macros in `p_inter.h` keep the old names working.

- `include/info.h` is shared with the tool: enums, the fixed-size lump structs (`static_assert`ed) and `INFO_ACTIONS`. `state_t.action` is an `actionnum_t` index into the engine's `actions[]`, not a function pointer. DeHackEd numbers are table indexes, so add new states, things and actions at the end.
- `include/deh_text.h` lists the strings a `Text` block can replace (pickups, key messages, level names, finale texts, cast names, cheat messages, the finale flats and skies): use them as `D_TEXT(id)` / `D_TEXTNUM(n)` (`dstrings.h`), never as literals. `DEH_VANILLA_TEXTS` holds vanilla's text for the ones GBADoom reworded, since patches name a string by its vanilla text. GbaWadUtil rewraps finale text that's too wide for the screen; `F_TextWrite` clips anything still off-screen.
- `Text` blocks naming a sprite, sound or music don't touch the engine: GbaWadUtil gives the old name the new name's lumps.
- `Max Health` is vanilla's health bonus cap (default 200); medikits stop at `MAXHEALTH` (100).

### Screen geometry

`SCREENWIDTH`/`SCREENHEIGHT` (120x160, portrait — matches the GBA screen rotated for Doom's taller-than-wide view) and `MAX_SCREENWIDTH`/`MAX_SCREENHEIGHT` are defined in `include/doomdef.h` and used throughout renderer sizing constants; don't hardcode 120/160 elsewhere.

### Fixed-point math

Doom's original fixed-point (`fixed_t`, `include/m_fixed.h`) and angle/trig tables (`include/tables.h`) are unchanged in spirit but have GBA-specific optimizations (e.g. BIOS divide via `IDiv32`, precomputed reciprocal tables in `m_recip.c`) — prefer the existing fixed-point helpers over introducing floating point, which is slow-to-absent on the GBA's ARM7TDMI.

### Source layout

- `source/`, `include/` — engine and game code (prBoom/Doom lineage, heavily trimmed and GBA-adapted).
- `source/iwad/` — generated IWAD C headers (not checked in by default; generated per the build steps above).
- `tools/GbaWadUtil/` — source of the WAD converter (see Building).
- `GbaWadUtil/` — its prebuilt Windows exe, runtime data and build scripts; IWADs go here (the commercial ones are gitignored).
- `source/libtimidity/`, `include/libtimidity/` — the music player (see Sound).
- `data/` — binary data directory wired into the Makefile's `BINFILES` mechanism.
- `build/` — GBA build output/object directory (generated).
