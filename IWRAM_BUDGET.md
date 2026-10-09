# IWRAM and VRAM budget

Snapshot of how the GBA's 32 KB of IWRAM (0x03000000–0x03008000) is used, and
how deep the stack that lives in the leftover space can get. The
[VRAM section](#vram-oam-and-palette) covers the 96 KB of VRAM plus OAM and
palette RAM, which also hold renderer tables.

Measured on the GBA build of 2026-10-09 (commit `66899ab`, the `lprintf` /
`I_Error` rewrite). The memory map, the paths that used to go through
newlib's printf, and the timedemo measurement are new for this build. The
gameplay state-action paths carry over from the earlier static analysis,
adjusted for `main`'s larger frame. Sizes are bytes.

## Summary

| | Bytes |
|---|---|
| Static IWRAM (code + data) | 25,372 |
| **Main stack (free gap)** | **7,140** |
| Worst realistic stack use (gameplay + sound IRQ) | ~1,850 |
| Headroom at worst realistic depth | ~5,300 |
| Measured peak, Doom 2 demo1 timedemo (incl. sound IRQ and the final `I_Error`) | 1,192 |

The goal of the stack work is to make the deepest path shallower. That
frees IWRAM for building more of the hot code at O2/O3 or moving more of it
into IWRAM.

### Since the 2026-10-08 snapshot

- **`printf` replaced (`66899ab`).** Every newlib printf-family call is gone.
  Console output and lump-name formatting now go through `lprintf.c` (see
  [Formatted output](#formatted-output)). That removed the old worst common
  path (level load with `lprintf`, 2,188 B including the sound IRQ). The
  realistic worst case is now one state action plus the sound IRQ. The
  measured timedemo peak fell from 1,528 to 1,192 B, and the ROM is
  30,372 B smaller.
- **`I_Error` rewritten.** It now needs 272 B instead of 1,656 B and can run
  with no free heap. The old one never displayed the timedemo result (see
  [`I_Error`](#i_error)).
- **Static IWRAM +24 B (stack −24 B).** `r_hotpath.iwram.c` grew in
  `P_CrossBSPNode` (+12), `R_PointInSector` (+4) and `R_RenderMaskedSegRange`
  (+4), plus 4 B of alignment.
- **`main`'s frame is 168 B, up from 152.** 8 B came from commits before
  `66899ab` and 8 B from `66899ab`. That adds 16 B to every path.

## Memory map

| Region | Range | Bytes | Notes |
|---|---|---|---|
| `.iwram` (code) | 0x03000000–0x030057E8 | 22,504 | |
| `.bss` | 0x030057E8–0x03006300 | 2,840 | cleared by crt0 |
| `.data`, `.init_array`, `.fini_array` | 0x03006300–0x0300631C | 28 | |
| **Main stack (User/System mode)** | 0x0300631C–0x03007F00 | **7,140** | grows down from `__sp_usr` |
| IRQ-mode stack | 0x03007F00–0x03007FA0 | 160 | `__sp_irq`; uses 16 B (see below) |
| SVC stack and BIOS area | 0x03007FA0–0x03008000 | 96 | BIOS SWIs, IRQ vector, `__irq_flags` |

The stack has no guard. If it grows past 0x0300631C it silently overwrites
`.data`, then `.bss` (`IntrTable`, then `r_hotpath`/`s_mix` statics). A
trashed `IntrTable` sends the next VBlank to a garbage address. `I_Error` no
longer depends on `IntrTable` or `_g`, so a later `I_Error` still displays.
An overflow itself is not detected.

## Static contents

### `.iwram` code (22,504)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` | 19,596 | `R_Subsector` 6,956 (with `R_AddLine`, `R_StoreWallRange` and `R_RenderSegLoop` inlined), `R_RenderPlayerView` 3,176, `R_MapPlane` 1,144, `R_AddSprites` 1,008, `P_CrossBSPNode` 936, `R_RenderBSPNode` 832, `R_RenderMaskedSegRange` 704, `R_DrawColumn` 664 |
| libtimidity (`TIMI_IWRAM`) | 1,616 | `_timi_resample_voice` 944, `update_signal` 372, `_timi_mix_voice` 300 |
| `s_mix.iwram.c` | 628 | `S_MixResample` 264, `S_MixOutput` 188, `S_MixDirect` 164 |
| `fixeddiv.s` | 412 | |
| libgba IRQ dispatcher (`IntrMain`) | 184 | |
| Linker interworking stubs, alignment | 68 | |

### `.bss` (2,840) and `.data` (28)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` | 1,728 | `colormapSlots` 1,024 (4 × 256), `spanstart` 160, `flatStats` 128, `solidcol` 120, `colormapSlotSrc` 16 |
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
printf is gone, but the console still writes through libsysbase's `write()`,
which uses the handle table.

## Stack depth

Static analysis of the linked ELF. Each function's frame comes from its
prologue (`push` / `sub sp`), cross-checked against GCC's `-fstack-usage` for
`r_hotpath.iwram.c`. Function-pointer calls (thinkers, state actions, menu
routines, traversers, column drawers, the voice mixer) are resolved by name.

### Main loop

| Path | Depth | Notes |
|---|---|---|
| Render (`main` → `R_RenderPlayerView`) | 840 | 168 + 672: `R_RenderPlayerView` 136 → `R_RenderBSPNode` 184 → `R_Subsector` 224 → `R_AddSprites` 120 → `FixedDiv` 8. A texture-cache miss under `R_Subsector` is 24 B shallower. |
| Gameplay tick, one state action | 1,600 | `G_Ticker` → `P_SetMobjState` → action → move/teleport → damage → action. Deepest common path. |
| Level load / Load Game with an `lprintf` | 696 | `main` 168 → `G_Ticker` 128 → `G_DoLoadGame` 80 → `G_DoLoadLevel` 104 → `lprintf` 216. Was 1,944 with newlib. |
| Finale music change (`S_ChangeMusic` → `lsnprintf`) | ~500 | `main` → `G_Ticker` → `F_Ticker` 328 → `S_ChangeMusic` 72 → `lsnprintf` ~96. Was 1,728 with `snprintf`. |
| Startup (`D_DoomMainSetup`) | ≤ 456 + 168 | the deepest direct call chain ends in `I_Error`. Was 1,608 + 152. |
| Two nested state actions | 2,416 | rare |
| Three nested state actions | 3,232 | very rare |

The state-action rows are the earlier figures plus `main`'s 16 B growth.
They follow function pointers, which the direct-call analysis used for the
other rows can't resolve.

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

| Scenario | Depth | Spare of 7,140 |
|---|---|---|
| Level load with `lprintf` | 940 | 6,200 |
| Render | 1,084 | 6,056 |
| Gameplay, one state action | 1,844 | 5,296 |
| Two nested state actions | 2,660 | 4,480 |
| Three nested state actions | 3,476 | 3,664 |

### `I_Error`

`I_Error` (`i_system_gba.cpp`) is a terminal error screen. It does not touch
`_g`, the heap, `IntrTable` or the interrupt dispatcher:

1. `REG_IME = 0`, then it stops the sound DMA and Timer 0 and turns off sound
   output, so the mixer can't run on top of it.
2. `consoleDemoInit()`, then `lvprintf` writes the message straight to the
   console.
3. It halts with `REG_IE = IRQ_VBLANK` and IME still off. BIOS `Halt` wakes on
   `IE & IF`, so the loop acknowledges `REG_IF` and halts again.

Its own depth is 272 B. The deepest part is `consoleDemoInit` →
`consoleInit` → `setvbuf` → `__swhatbuf_r`. Printing the message is 224 B:
`I_Error` 48 → `L_Format` 56 → `write` 16 → `_write_r` 24 → `con_write` 56
→ `consolePrintChar` 24. Called from the deepest one-action gameplay path, it
reaches about 1.9 KB.

The old `I_Error` used `vsnprintf` into a 256 B stack buffer and needed
1,656 B. The timedemo result's `%f` made `_dtoa_r` call `Balloc`. Its malloc
failed, because the zone heap takes nearly all the free memory at startup.
newlib then asserted ("Balloc succeeded", `mprec.c` line 783) and the result
was never shown.

## Measured

The stack was painted with `0xDEADBEEF` at `main` via mGBA's GDB stub, a
timedemo was run, and the low-water mark was read back:

| Build | Gameplay peak | Peak including the final `I_Error` |
|---|---|---|
| After the EWRAM move, old `I_Error` | 1,432 | 4,232 |
| After `I_Error` → `fputs`, 256 B buffer | 1,432 | 1,800 |
| `2339533` (before the printf rewrite), with sound IRQ | 1,528 | never displayed: newlib `Balloc` assertion |
| `66899ab` (`lprintf` / `I_Error` rewrite), with sound IRQ | **1,192** | **1,192** |

The last two rows are the Doom 2 demo1 timedemo (1,198 gametics, 2,072
realtics) on a copy of each commit with `timedemo = "demo1"`. Stale return
addresses at the low-water mark show what set each peak:

- **`2339533`:** `_svfprintf_r` / `__ssputs_r` (a `sprintf` building a lump
  name) with `I_SoundVBlank` on top.
- **`66899ab`:** the mixer (`_timi_mix_voice` → `_timi_resample_voice`)
  interrupting game code.

The demo never reaches the static one-state-action worst case.

The original layout (1,336 B stack) couldn't be painted the same way, but the
gameplay peak above was already 96 B over its limit, and on that build mGBA
reported "Jumped to invalid address" at the timedemo's final `I_Error`.

The first two rows predate the sound IRQ, so they don't include the 244 B
the mixer adds.

## Formatted output

`source/lprintf.c` has one small formatter, `L_Format`, behind three entry
points. It supports `%s`, `%.Ns`, `%d`, `%.Nd` (zero padded to N digits, at
most 11 characters) and `%%`. Anything else is printed as it is. There is no
float output.

| Function | Output | Stack |
|---|---|---|
| `lprintf(fmt, ...)` | console, then a newline | 216 |
| `lvprintf(fmt, va_list)` | console (used by `I_Error`) | `lprintf` minus 40 |
| `lsnprintf(buf, size, fmt, ...)` | buffer; always terminated, returns the length | ~96 |

Console output is written piece by piece with `write(1, …)`. That goes
straight to libgba's console devoptab, with no message buffer, no stdio, no
malloc and no locale. `%.8s` is the one modifier with a real use: WAD lump
names are `char[8]` and aren't terminated when a name is 8 characters long.

## What to watch

- **Every byte added to IWRAM comes off the stack.** That includes code or
  statics in `r_hotpath.iwram.c`, `s_mix.iwram.c` and `TIMI_IWRAM` functions.
  At the realistic worst case (~1.85 KB), about 5.3 KB can still be added
  before the stack has no margin. Keeping the stack at least ~4 KB (above
  the 3,476 B three-nested-action case) leaves about 3 KB for further IWRAM
  growth.
- **`main`'s frame (168 B) is under every path.** LTO inlines `D_DoomLoop`,
  `D_Display` and many of the drawers (`WI_*`, `ST*`, `HU*`, `AM_*`,
  `M_Drawer`) into `main`. Their locals stay reserved during `G_Ticker` too.
- **Big stack frames on the render path.** O3 inlining made `R_Subsector`'s
  frame 224 B, and it sits under `R_RenderBSPNode` (184 B). Inlining more
  into that chain grows the render depth as well as the code.
- **Large stack locals** in code that runs during gameplay or in the VBlank
  handler. The mixer runs on top of the deepest game path, so keep
  `I_SoundVBlank`'s call chain shallow.
- **Don't reintroduce newlib's printf family** (`printf`, `sprintf`,
  `snprintf`, `vsnprintf`). It costs 1.3–3.4 KB of stack per call
  (`_svfprintf_r` alone is 816 B, `__sbprintf` 1,168 B), mallocs for `%f`,
  and adds about 30 KB of ROM. Use `lprintf`, `lsnprintf` or `I_Error`
  instead (see [Formatted output](#formatted-output)).
- **Extra IRQ sources**: if more interrupts are enabled, they can nest on the
  same stack on top of the mixer.

## VRAM, OAM and palette

The display runs in Mode 4 (8-bit bitmap, two pages, OBJs off), set in
`I_CreateWindow_e32`. That leaves gaps after each page, all of OBJ VRAM, OAM
and the OBJ palette unused by the hardware. The renderer keeps lookup tables,
clip arrays, colormaps and a flat cache in them.

The layouts are structs in `include/vram_spare.h`, reached through
fixed-address macros:

| Struct | Macro | Region |
|---|---|---|
| `vram1_spare_t` | `vram1_spare` | gap after page 1 |
| `vram_tail_t` | `vram_tail` | gap after page 2 plus all of OBJ VRAM (contiguous) |
| `oam_spare_t` | `oam_spare` | OAM |
| `objpal_spare_t` | `objpal_spare` | OBJ palette |

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

`R_ClearPlanes` resets `floorclip` and `ceilingclip` with a 32-bit DMA fill
(`BlockSet`, whose source word is on the IWRAM stack). `R_StoreWallRange`
saves clip ranges to the openings array with 16-bit DMA (`BlockCopy16`).

### Palette RAM (1,024 at 0x05000000)

| Range | Bytes | Use |
|---|---|---|
| 0x05000000–0x05000200 | 512 | BG palette: the 256-colour game palette (`I_SetPallete_e32`); entries 0 and 241 are also set for the text console |
| 0x05000200–0x05000300 | 256 | `objpal_spare->fullColormap`: colormap 0 (full bright), filled once by `R_InitBuffer`. Used for the sky, fullbright sprites and the weapon. |
| 0x05000300–0x05000400 | 256 | `objpal_spare->fixedColormap`: the current `fixedcolormap` (invulnerability or light amplification), copied by `R_RenderPlayerView` only when it changes |

`R_FastColormap` hands drawers these copies instead of loading a colormap
slot. In mGBA, lookups from palette RAM run at IWRAM speed (sky: 19.0
cycles/pixel against 18.9 from IWRAM). On hardware, a CPU access can take one
extra cycle when the display reads the palette at the same time.

### Free video memory

| Where | Bytes | Notes |
|---|---|---|
| `vram_tail->unused`, 0x06013600–0x06015400 | 7,680 | one contiguous block |
| `vram1_spare` | 460 | |
| OAM | 48 | |

VRAM, OAM and palette RAM need 16- or 32-bit writes, because 8-bit writes to
VRAM store the byte to both halves of the halfword. Any table moved there
must be accessed that way.
