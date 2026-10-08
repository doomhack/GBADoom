# IWRAM and VRAM budget

Snapshot of how the GBA's 32 KB of IWRAM (0x03000000–0x03008000) is used, and
how deep the stack that lives in the leftover space can get. The
[VRAM section](#vram-oam-and-palette) covers the 96 KB of VRAM plus OAM and
palette RAM, which also hold renderer tables.

Measured on the GBA build of 2026-10-07 (commit `6b663e5` plus uncommitted
work: libtimidity music, VBlank-IRQ mixer, IWAD-less ROM). Sizes are bytes.

## Summary

| | Bytes |
|---|---|
| Static IWRAM (code + data) | 22,328 |
| **Main stack (free gap)** | **10,184** |
| Worst realistic stack use (gameplay + sound IRQ) | ~2,200 |
| Headroom at worst realistic depth | ~8,000 |

`I_Error` is treated as a terminal crash and left out of the stack figures
(it needs 1,656 B of its own; see below).

## Memory map

| Region | Range | Bytes | Notes |
|---|---|---|---|
| `.iwram` (code) | 0x03000000–0x03004F20 | 20,256 | |
| `.bss` | 0x03004F20–0x0300571C | 2,044 | cleared by crt0 |
| `.data`, `.init_array`, `.fini_array` | 0x0300571C–0x03005738 | 28 | |
| **Main stack (User/System mode)** | 0x03005738–0x03007F00 | **10,184** | grows down from `__sp_usr` |
| IRQ-mode stack | 0x03007F00–0x03007FA0 | 160 | `__sp_irq`; uses 16 B (see below) |
| SVC stack and BIOS area | 0x03007FA0–0x03008000 | 96 | BIOS SWIs, IRQ vector, `__irq_flags` |

The stack has no guard. If it grows past 0x03005738 it silently overwrites
`.data`, then `.bss` (`IntrTable`, then `r_hotpath`/`s_mix` statics).

## Static contents

### `.iwram` code (20,256)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` | 17,352 | `R_Subsector` 4,392, `R_RenderPlayerView` 3,472, `R_RenderSegLoop` 2,340, `R_MapPlane` 1,144, `P_CrossBSPNode` 924, `R_RenderMaskedSegRange` 700, `R_DrawColumn` 664 |
| libtimidity (`TIMI_IWRAM`) | 1,616 | `_timi_resample_voice` 944, `update_signal` 372, `_timi_mix_voice` 300 |
| `s_mix.iwram.c` | 628 | `S_MixResample` 264, `S_MixOutput` 188, `S_MixDirect` 164 |
| `fixeddiv.s` | 412 | |
| libgba IRQ dispatcher (`IntrMain`) | 184 | |
| Linker interworking stubs, alignment | 64 | |

### `.bss` (2,044) and `.data` (28)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` | 932 | `current_colormap` 256, `spanstart` 160, `flatStats` 128, `solidcol` 120 |
| `s_mix.iwram.c` | 896 | `mixbuf` 896 |
| libgba | 158 | `IntrTable` 120 |
| LTO globals, crtbegin, alignment | 58 | |
| `.data` + init/fini arrays | 28 | |

### Kept out of IWRAM on purpose

`gbadoom.ld` (used instead of devkitARM's `gba_cart.ld`, via `gbadoom.specs`)
places newlib and libsysbase `.data`/`.bss` in EWRAM (`.ewram`/`.sbss`). That
is 6,900 B: the stdio handle table (4 KB), malloc state, locale and reent data.
These used to sit in IWRAM, which left the stack only 1,336 B and caused real
overflow crashes. Nothing from those libraries is used on a hot path.

## Stack depth

Static analysis of the linked ELF. Each function's frame comes from its
prologue (`push` / `sub sp`), cross-checked against GCC's `-fstack-usage` for
`r_hotpath.iwram.c`. Function-pointer calls (thinkers, state actions, menu
routines, traversers, column drawers, the voice mixer) are resolved by name.

### Main loop

| Path | Depth | Notes |
|---|---|---|
| Render (`main` → `R_RenderPlayerView`) | 792 | 152 + 640; texture-cache miss included |
| Gameplay tick, one state action | 1,584 | `G_Ticker` → `P_SetMobjState` → action → move/teleport → damage → action |
| Gameplay tick with `snprintf` (`S_ChangeMusic` in the finale) | 1,728 | |
| Level load / Load Game with an `lprintf` | 1,944 | deepest common path: `G_DoLoadLevel` → `lprintf` → `vsprintf` |
| Startup (`D_DoomMainSetup`) | 1,608 + 152 | |
| Two nested state actions | 2,400 | rare |
| Three nested state actions | 3,216 | very rare |

A "state action" is a monster or weapon state's action function. They can
nest: an action spawns a missile that explodes on spawn and kills something,
whose death state runs another action. Each extra level costs about 820 B.
Recursion beyond three levels is theoretically possible but not seen in
practice.

### Sound interrupt

The VBlank IRQ is the only interrupt enabled (`i_audio.c`). libgba's
`IntrMain` pushes 16 B on the IRQ stack, then **switches to System mode and
calls `I_SoundVBlank` on the main stack**. So the mixer's stack use adds to
whatever depth the main loop is at when VBlank arrives.

| Path | Depth |
|---|---|
| `I_SoundVBlank` → `_timi_mix_voice` → `_timi_resample_voice` → `S_MixResample` | 240 |
| plus `lr` pushed by `IntrMain` in System mode | 4 |
| **Total added to the main stack** | **244** |

IRQs are re-enabled during the handler, but no other IRQ source is on, so
nothing nests unless the mixer runs longer than a frame.

### Combined worst case (main + sound IRQ)

| Scenario | Depth | Spare of 10,184 |
|---|---|---|
| Render | 1,036 | 9,148 |
| Gameplay, one state action | 1,828 | 8,356 |
| Level load with `lprintf` | 2,188 | 7,996 |
| Two nested state actions | 2,644 | 7,540 |
| Three nested state actions | 3,460 | 6,724 |

### Not counted: `I_Error`

`I_Error` is a terminal error screen. It uses `vsnprintf` into a 256 B buffer
and `fputs`, so its own depth is 1,656 B, mostly `%f` formatting. Called from
deep gameplay it reaches 3,216 B (about 3.5 KB with the sound IRQ on top),
which still fits.

## Measured (earlier builds)

The stack was painted with `0xDEADBEEF` at `main` via mGBA's GDB stub, a
timedemo was run, and the low-water mark was read back:

| Build | Gameplay peak | Peak including the final `I_Error` |
|---|---|---|
| After the EWRAM move, old `I_Error` | 1,432 | 4,232 |
| After `I_Error` → `fputs`, 256 B buffer | 1,432 | 1,800 |

The original layout (1,336 B stack) couldn't be painted the same way, but the
gameplay peak above was already 96 B over its limit, and on that build mGBA
reported "Jumped to invalid address" at the timedemo's final `I_Error`.

Those builds predate the sound IRQ, so the measurements don't include the
244 B the mixer adds.

## What to watch

- **Every byte added to IWRAM comes off the stack.** That includes code or
  statics in `r_hotpath.iwram.c`, `s_mix.iwram.c` and `TIMI_IWRAM` functions.
  At the realistic worst case (~2.2 KB), about 8 KB can still be added
  before the stack has no margin. Keep at least ~4 KB free for the rare
  nested-action paths.
- **Large stack locals** in code that runs during gameplay or in the VBlank
  handler. The mixer runs on top of the deepest game path, so keep
  `I_SoundVBlank`'s call chain shallow.
- **`printf`-family calls** cost 1.3–1.5 KB each (`_svfprintf_r` alone is
  816 B). stdout is unbuffered, so `printf` to the console also goes through
  `__sbprintf` (another ~1.2 KB). Prefer `fputs`, or avoid formatting on the
  GBA.
- **Extra IRQ sources**: if more interrupts are enabled, they can nest on the
  same stack on top of the mixer.

## VRAM, OAM and palette

The display runs in Mode 4 (8-bit bitmap, two pages, OBJs off), set in
`I_CreateWindow_e32`. That leaves gaps after each page, all of OBJ VRAM, OAM
and the OBJ palette unused by the hardware. The renderer keeps lookup tables
and a flat cache in some of them.

The layouts are structs in `include/vram_spare.h`, reached through
fixed-address macros:

| Struct | Macro | Region |
|---|---|---|
| `vram1_spare_t` | `vram1_spare` | gap after page 1 |
| `vram_tail_t` | `vram_tail` | gap after page 2 plus all of OBJ VRAM (contiguous) |
| `oam_spare_t` | `oam_spare` | OAM |

The compiler places the members, and `static_assert`s fail the build if a
region overflows; `vram_tail_t` must fill its region exactly, so the flat
cache always ends at the end of OBJ VRAM. The VRAM copies of ROM tables take
their array sizes from the ROM declarations, so the copy sizes always match.
To add a table, add a member. Free space is the region size minus `sizeof`
the struct, or the `unused` member in `vram_tail_t`.

### VRAM (98,304)

| Region | Range | Bytes | Used | Free |
|---|---|---|---|---|
| Page 1 framebuffer | 0x06000000–0x06009600 | 38,400 | 38,400 | 0 |
| Page 1 gap (`vram1_spare`) | 0x06009600–0x0600A000 | 2,560 | 2,100 | 460 |
| Page 2 framebuffer | 0x0600A000–0x06013600 | 38,400 | 38,400 | 0 |
| Page 2 gap + OBJ VRAM (`vram_tail`) | 0x06013600–0x06018000 | 18,944 | 11,264 | 7,680 |
| **Total** | | **98,304** | **90,164** | **8,140** |

Contents (offsets are bytes into the region, as laid out by the compiler):

| Region | Offset | Member | Bytes |
|---|---|---|---|
| `vram1_spare` | 0 | `yslope` (copy of ROM `yslope[128]`, one entry per view row) | 512 |
| | 512 | `distscale` (copy of ROM `distscale[120]`) | 480 |
| | 992 | `xtoviewangle` (copy of ROM `xtoviewangle[121]`) | 484 |
| | 1476 | `wipe_y_lookup` | 240 |
| | 1716 | `vissprite_ptrs` (`MAXVISSPRITES` × 4) | 384 |
| `vram_tail` | 0 | `unused` (free, 0x06013600–0x06015400) | 7,680 |
| | 7680 | `dsclip` (`MAXDRAWSEGS` × 16, per-frame drawseg summary for sprite clipping, 0x06015400–0x06016000) | 3,072 |
| | 10752 | `flatCache`, 2 slots × 4,096 (0x06016000–0x06018000) | 8,192 |

The page 2 gap and OBJ VRAM are adjacent, so the free space is kept in one
block at the start and the flat cache at the end. `screenheightarray` and
`negonearray` moved from the page 2 gap to OAM to empty it. 4 flat slots
measured no faster than 2.

`R_InitBuffer` (`r_draw.c`) fills the tables and `R_LoadFlat` fills the flat
cache with `BlockCopy`. Nothing else writes past row 160 of either page.
`I_CreateWindow_e32` clears exactly 240 × 160 bytes, and the wipe and patch
drawers stay inside the page. A VRAM dump after 1,200 tics of the Doom 2 demo
showed the `unused` block still all zero and both flat slots matching their
WAD data in the ROM.

The libgba console (`consoleDemoInit`: font at char base 0, map at base 4)
uses 0x06000000–0x06002800, inside page 1. An `lprintf` during play only
scribbles on the visible frame. `I_Error` switches to Mode 0, but it is
terminal.

`yslope` used to be `fixed_t[160]` (640 B) in a hand-computed 580 B slot, so
`distscale` overwrote its entries 145–159. That was harmless, since only rows
0–127 are read. It is now `yslope[128]`, matching the fixed 128-row view
(`viewheight`, 160 minus the status bar), and `r_hotpath.iwram.c`
`static_assert`s that it covers every view row. The struct layout makes this
kind of overlap impossible.

### OAM (1,024 at 0x07000000, `oam_spare`)

OBJs are disabled, so OAM is free memory with a 32-bit bus and no display
contention. `DISPCNT` bit 5 (H-blank interval free) is set, and every access
is 16- or 32-bit, as OAM requires.

| Offset | Member | Bytes |
|---|---|---|
| 0 | `screenheightarray` | 240 |
| 240 | `negonearray` | 240 |
| 480 | `floorclip` | 240 |
| 720 | `ceilingclip` | 240 |
| 960 | `tmpbbox` (`_g->tmbbox`) | 16 |
| 976 | free | 48 |

976 B used, 48 B free.

### Palette RAM (1,024 at 0x05000000)

| Range | Bytes | Use |
|---|---|---|
| 0x05000000–0x05000200 | 512 | BG palette: the 256-colour game palette (`I_SetPallete_e32`); entries 0 and 241 are also set for the text console |
| 0x05000200–0x05000400 | 512 | OBJ palette: unused |

### Free video memory

| Where | Bytes | Notes |
|---|---|---|
| `vram_tail->unused`, 0x06013600–0x06015400 | 7,680 | one contiguous block |
| OBJ palette | 512 | 16-bit bus |
| `vram1_spare` | 460 | |
| OAM | 48 | |

VRAM, OAM and palette RAM need 16- or 32-bit writes, because 8-bit writes to
VRAM store the byte to both halves of the halfword. Any table moved there
must be accessed that way.
