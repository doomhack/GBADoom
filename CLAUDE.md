# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

GBADoom is a port of PrBoom (itself based on id Software's DOOM) to the Game Boy Advance. The renderer, monster AI, and game logic are mostly intact from PrBoom; engine enhancements like dehacked and limit-removing have been reverted to vanilla behavior to fit GBA memory/performance constraints. There is no multiplayer and demo compatibility is broken.

The same source tree also builds as a Windows desktop app (via Qt) for faster iteration than testing on GBA hardware/emulator alone.

## Building

**GBA target (DevKitArm + make):**
```
make            # requires DEVKITARM env var set, run from msys2 shell (see msys2.bat)
make clean
```
Produces `GBADoom.elf` and `GBADoom.gba`.

**Windows/Qt target:** open `GBADoom.pro` in Qt Creator (MinGW 32-bit or MSVC 32-bit). This is the primary way to iterate on gameplay/rendering logic without a GBA emulator — it builds the same source files as regular C/C++ against Qt for windowing/input instead of libgba.

**The GBA build contains no IWAD.** `make` produces an engine-only ROM that ends with a 32-byte IWAD header (`doom_iwad_header_t` in `include/doom_iwad.h`, emitted by the `.iwad` section at the end of `gbadoom.ld`). Attach a WAD with:
```
GbaWadUtil -in <wad> -gus GbaWadUtil/dgguspat/timid_d2.cfg -rom GBADoom.gba -romout <game>.gba
```
(`timid_d1.cfg` for Doom/Ultimate Doom/Sigil. Without `-gus` there is no `GUSBANK` lump and the music is silent.)
GbaWadUtil finds the header by its magic (`GBADOOM-IWAD-HDR`, 32-byte aligned), writes version/length, drops anything already after it and appends the processed WAD, so a ROM that already has a WAD can be re-patched. `doom_iwad` is a link-time address right after the header; `doom_iwad_len` reads the header. `IdentifyVersion()` errors at startup if no WAD is attached or `DOOM_IWAD_VERSION` doesn't match — bump it (and `romIwadVersion` in GbaWadUtil's `main.cpp`) whenever the processed WAD format changes.

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

### Sound

Music and sound effects both come from the WAD, mixed in software to one mono signed 8-bit stream at 13379 Hz (224 samples per GBA frame):

- GbaWadUtil converts `D_*` MUS lumps to type 0 MIDI, resamples `DS*` lumps to signed 8-bit at 13379 Hz (windowed sinc, so sound effects mix without interpolation), and with `-gus <cfg>` builds a `GUSBANK` lump: GUS patches from `GbaWadUtil/dgguspat/`, preprocessed for the fixed output rate (format in `include/gusbank.h`; keep it in step with GbaWadUtil's copy).
- `source/libtimidity/` is a fixed-point, mono port of libtimidity that reads the bank and streams MIDI from ROM (no event list in RAM).
- `source/s_mix.c` mixes music and up to 8 sfx voices; the inner loops are in `source/s_mix.iwram.c`, and libtimidity's per-voice driver is in IWRAM too (`TIMI_IWRAM`).
- `source/i_audio.c`: on the GBA, Timer 0 (1254 cycles) + DMA1 feed Direct Sound A, and the mixer runs in the VBlank interrupt (so changes from the game thread mask `REG_IME`). The Qt build plays the same output through waveOut (`source/i_audio_win.c`).

### Screen geometry

`SCREENWIDTH`/`SCREENHEIGHT` (120x160, portrait — matches the GBA screen rotated for Doom's taller-than-wide view) and `MAX_SCREENWIDTH`/`MAX_SCREENHEIGHT` are defined in `include/doomdef.h` and used throughout renderer sizing constants; don't hardcode 120/160 elsewhere.

### Fixed-point math

Doom's original fixed-point (`fixed_t`, `include/m_fixed.h`) and angle/trig tables (`include/tables.h`) are unchanged in spirit but have GBA-specific optimizations (e.g. BIOS divide via `IDiv32`, precomputed reciprocal tables in `m_recip.c`) — prefer the existing fixed-point helpers over introducing floating point, which is slow-to-absent on the GBA's ARM7TDMI.

### Source layout

- `source/`, `include/` — engine and game code (prBoom/Doom lineage, heavily trimmed and GBA-adapted).
- `source/iwad/` — generated IWAD C headers (not checked in by default; generated per the build steps above).
- `GbaWadUtil/` — the external tool (and prebuilt IWADs/binaries) used to convert `.wad` files into embeddable C source.
- `source/libtimidity/`, `include/libtimidity/` — the music player (see Sound).
- `data/` — binary data directory wired into the Makefile's `BINFILES` mechanism.
- `build/` — GBA build output/object directory (generated).
