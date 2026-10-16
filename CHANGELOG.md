# Changelog

## V3 (changes since `master`)

V3 is a large rework of GBADoom: the ROM no longer has a WAD built in, music and
sound come straight from the WAD instead of a Maxmod soundbank, and most of the
renderer has been optimised. GbaWadUtil now lives in this repository and does much
more of the work ahead of time.

### Highlights

- **Patch any WAD into a prebuilt ROM.** `make` builds an engine-only ROM that ends in
  a 32-byte IWAD header. GbaWadUtil attaches a WAD (and optional PWAD) to it, with no
  rebuild needed. It can also re-patch a ROM that already has a WAD. The engine checks
  the WAD format version at startup and stops with an error if the WAD is missing or
  from the wrong version of GbaWadUtil.
- **Original music and sound from the WAD.** Maxmod and the hand-converted `.it`/`.xm`
  tracker music are gone. The music is the game's own `D_*` lumps, played by a
  fixed-point, mono port of libtimidity using GUS instrument patches (`GUSBANK`). Sound
  effects are the WAD's `DS*` lumps. This means PWADs with custom music and sounds work.
- **Full-resolution floors and ceilings.** Flats were drawn at half horizontal
  resolution. They are now drawn at full resolution.
- **Distance lighting restored.** Floors and ceilings get Doom's diminishing light
  again, as in vanilla. Walls, masked textures and sprites use it too, all from
  precomputed tables.
- **Correct brightness and gamma.** Fixes the screen looking darker than intended.
  Palettes are converted ahead of time for each of Doom's 5 gamma levels, using
  vanilla's gamma ramps.
- **Sharp full-screen pictures.** The title, intermission, finale, credit and help
  screens are scaled down to 240x160 ahead of time with proper filtering. This fixes
  the blocky look and the slow menus and intermission screens.
- **Much faster renderer** (see Performance).

### Rendering

- Wall textures are now put together by GbaWadUtil into deduplicated 128-byte columns
  (`COLPOOL`/`TEXCOLS`/`COLRUNS`). This removes the runtime column compositing and the
  column cache, saves 8 KB of VRAM, and makes performance steadier. For example, the
  E1M1 computer room no longer slows down.
- Masked mid-textures draw only the opaque runs of the shared column.
- All 32 light colormaps sit together in VRAM, so changing colormap only means changing
  a pointer. The powerup colormaps (invulnerability, light amp) are kept in spare
  palette RAM.
- Fixed a rendering bug that crept in during the visplane rework.

### Performance

- Visplanes were redesigned and shrunk, and limit clearing uses block fills.
- `R_RenderSegLoop`, `R_MakeSpans` and `R_DoDrawPlane` were optimised. `R_DoDrawPlane`
  skips columns that haven't changed, and `R_MakeSpans` skips duplicate spans and works
  out each span's area once.
- Lines that face away from the player are culled early.
- Reciprocal and divide calculations are faster, including inline reciprocals and a
  small reciprocal table.
- Drawing high-resolution columns and fuzz (spectre) columns is faster.
- Sprite clipping against drawsegs uses a summary of each drawseg's screen range.
- The status bar redraws only the widgets that changed, then copies them to the other
  screen buffer. Previously the whole bar was redrawn.
- Things that never think (decorations and the like) are no longer kept in the thinker
  list, so the game doesn't step over them every tic.
- IWRAM code is now built at `-O2` (it was `-Os`). The per-pixel loops and the BSP
  chain are built at `-O3`. The levels are set in `include/code_opt.h`.
- Windows/Qt builds of the hot path are no longer optimised, which makes debugging
  easier.

### Audio

- New software mixer (`source/s_mix.c`) combines the music and up to 8 sound effects
  into one 8-bit mono stream at 13379 Hz. It runs in the VBlank interrupt.
- The inner mixing loops are ARM assembly in IWRAM (`s_mix_gba.s`). A C version
  (`s_mix_ref.c`) gives bit-identical output and is used for the Qt build.
- Instrument samples are resampled by taking the nearest sample rather than
  interpolating. This roughly halves the mixing cost with no audible difference. GUS
  patches are upsampled ahead of time where that helps.
- The Qt build plays the same audio output through waveOut.
- Both type 0 and type 1 MIDI are supported.

### GbaWadUtil (now part of this repo)

- The source has moved into `tools/GbaWadUtil/` (Qt 6) and keeps its own history. A
  prebuilt Windows exe, the GUS patches (`dgguspat/`) and the batch scripts are in
  `GbaWadUtil/`.
- New options: `-rom`/`-romout` to patch a ROM, `-gus <cfg>` to build the instrument
  bank, `-pwad` to merge a PWAD over its IWAD.
- Instrument configs: `timidity.cfg` (modern sound, the default in the batch scripts),
  plus `timid_d1.cfg` and `timid_d2.cfg` (retro sound).
- Pre-processes:
  - map lumps, made more compact;
  - wall texture columns;
  - palettes for every gamma level;
  - full-screen pictures;
  - MUS to MIDI;
  - sound effects, resampled with a windowed-sinc filter.
- Keeps only the lumps the engine actually reads, merges all flat and sprite
  namespaces, and stores identical lumps only once. This makes WADs and ROMs smaller.
- Lowercase flat names, as used in TNT, are fixed.
- New `rom-build-all.bat`/`wad-build-all.bat` scripts build every supported game in one
  go.

### Memory and ROM size

- Map data structures were shrunk. Doom II MAP15 dropped from 272 KB to 220 KB of
  heap.
- Removed: the block allocator (`z_bmalloc`), leftover netcode, unused strings, and HUD
  messages too long for the screen.
- Other changes made the ROM smaller.
- The number of flats allowed went up from 256 to 1024. Some PWADs were hitting the old
  limit.

### Stability and bug fixes

- Fixed crashes from stack overflow, which newer devkitARM/GCC versions made worse by
  putting more into IWRAM. A custom linker script (`gbadoom.ld`) now makes the link fail
  if less than 2 KB of IWRAM is left for the stack.
- The stack is now much shallower:
  - chained line actions are deferred;
  - sound propagation is no longer recursive;
  - a sound buffer moved to EWRAM;
  - `I_Error` uses a new lightweight printf.
- Fixed broken lifts and ceilings. The linked lists of active plats and ceilings were
  kept incorrectly. They are now fixed-size arrays with overflow checks.
- Fixed thinker functions, and fixed build errors with modern GCC and C23.
- Cleaned up warnings, made code const-correct and tidied it.

### Build and repo

- `build.bat` builds the GBA target from cmd or PowerShell.
- The Qt build still compiles a WAD in. Generate it with `GbaWadUtil -cfile`.
- Removed: the Maxmod soundbank and `music/` tracker files, stray save files, and other
  leftover files (`CodeProphet.lib`, `make.sh`, `msys2.bat`).
- Added `CLAUDE.md` (architecture notes) and `IWRAM_BUDGET.md` (what's in IWRAM and how
  deep the stack gets).
