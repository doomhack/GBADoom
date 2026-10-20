# IWRAM and VRAM budget

Snapshot of how the GBA's 32 KB of IWRAM (0x03000000–0x03008000) is used, and
how deep the stack that lives in the leftover space can get. The
[VRAM section](#vram-oam-and-palette) covers the 96 KB of VRAM plus OAM and
palette RAM, which also hold renderer tables.

The layout (memory map, `.bss`, per-function code sizes), the static stack
analysis and the measured peaks are from the GBA build of 2026-10-20:
`5390152`. VRAM, OAM and palette RAM are unchanged since the 2026-10-12
snapshot (`37dbcc7`). Sizes are bytes.

## Summary

| | Bytes |
|---|---|
| Static IWRAM (code + data) | 27,472 |
| **Main stack (free gap)** | **5,040** |
| Stack the link guarantees (`__stack_reserve`) | 2,048 |
| Static worst case (one state action + sound IRQ), sight checks aside | ~1,740 |
| Headroom at that worst case | ~3,300 |
| Deepest measured peak, 6 timedemos (Doom 2 and Ultimate Doom demo1–3) | 1,364 |
| Sight check, deepest Doom 2 line + sound IRQ (grows with the map, see [Sight checks](#sight-checks)) | ~2,280–2,890 |

The goal of the stack work is to make the deepest path shallower. That
frees IWRAM for building more of the hot code at O2/O3 or moving more of it
into IWRAM. ARM code at O3 now fits (see
[Optimisation levels](#optimisation-levels)).

### Since the 2026-10-12 snapshot

- **IWRAM code −192 B, so the stack grows from 4,848 to 5,040 B.** All of
  it is in `r_hotpath.iwram.c` (−188) and libtimidity (−4):

  | Function | Before | Now | Change | Why |
  |---|---|---|---|---|
  | `R_RenderPlayerView` | 4,136 | 3,764 | −372 | `R_MakeSpans` no longer inlined into it (`83bf164`) |
  | `R_MakeSpans` | (inlined) | 320 | +320 | |
  | `P_MobjThinker` | 344 | 244 | −100 | corpses stop thinking (`cfaa515`) |
  | `R_Subsector` | 7,192 | 7,112 | −80 | `R_RenderSegLoop` rewrite (`1f8cb44`), light tables (`96a6403`) |
  | `R_AddSprites` | 1,112 | 1,160 | +48 | decorations animate from the game tic in `R_ProjectSprite` (`50ea3ef`) |
  | `R_RenderMaskedSegRange` | 1,044 | 1,032 | −12 | light tables (`96a6403`) |
  | others | | | +4 | `P_CrossBSPNode` +8, `R_FindPlane` +4, `R_DivSmall` −4, `update_signal` −4 |

- **Thumb code is built with `-fno-inline-functions-called-once`
  (`89f805b`).** LTO no longer folds the main loop into `main`: `main`'s
  frame is 24 B and `D_DoomLoop` (with `D_Display` and the drawers inlined)
  is 136 B, 160 B together against `main`'s old 168. `G_Ticker` went from
  120 to 72 B and `P_XYMovement` from 104 to 64.
- **The sound IRQ is 48 B deeper** (~340 B). `mid_song_render` is no longer
  inlined into `I_SoundVBlank` (72 → 48 B), and has its own 64 B frame.
- **New static analysis.** It follows the IWRAM code's long calls into ROM
  and the calls through data pointers (thinkers, state actions, line
  traversers), which the old direct-call analysis didn't. The one-action
  worst case is ~1,400 B, against 1,472 B with the same tool on `37dbcc7`
  (the ~1,600 B figure before was a hand-adjusted estimate). See
  [Main loop](#main-loop).
- **DeHackEd patches can put any action on any state** (`b8dd09b`), so the
  worst case now allows any action wherever a state is set. The action
  queue still limits it to one action at a time.

### Depth lighting (`37dbcc7`)

- **Doom's diminishing light is back.** Walls, masked mid textures and
  sprites take their colormap from the scale, and flats from the distance,
  as in vanilla. `scalelight`/`zlight` are `const` tables of colormap
  numbers in `tables.c` (ROM +4,608 B).
- **All 32 light colormaps live in VRAM** (`vram_tail->lightColormaps`, one
  8 KB block), so a drawer can switch colormap per column or span without
  copying. The 4-slot IWRAM colormap cache (`colormapSlots`,
  `R_LoadColorMap`) is gone: **IWRAM −1,040 B** (`.bss` −1,048, code +8),
  so the stack grows from 3,808 to 4,848 B.
- **To make room, the sprite-clip summary went from 16 B to 12 B an entry**
  (separate `dsclipXkey`/`dsclipLowscale`/`dsclipScale` arrays, with the
  drawseg index and masked flag in `vram1_spare->dsclipInfo`). The scan loop
  is the same instructions with a 4 B stride. On its own this change was
  pixel-identical.
- **About 0.5% slower** (mGBA, same end states): Ultimate Doom demo4
  1,787 → 1,798 realtics, Doom 2 demo1 2,039 → 2,049, demo3 6,393 → 6,428.
- Code size changes: `R_Subsector` +256 (the per-column light in the inlined
  seg loop), `R_RenderMaskedSegRange` +148, `R_MapPlane` +44,
  `R_RenderPlayerView` −128, `R_AddSprites` −68, `R_LoadColorMap` −248.

### ARM code at O2 (`807859c`)

- **`ARM_CODE_OPT_LEVEL` is now O2** (`include/code_opt.h`; HOT and WARM
  stay O3). It applies to the rest of `r_hotpath.iwram.c` and
  `s_mix.iwram.c`, and to `TIMI_IWRAM`. IWRAM code +3,640 B, stack
  7,356 → 3,712 B.
- **Ultimate Doom demo4: 1,906 → 1,881 realtics (14.89 → 15.09 fps,
  +1.3%).** End state and stack peak (1,272 B) are unchanged. O3 would give
  1,857 (15.29 fps, +2.6%) but leaves only 1,816 B of stack, below the 2 KB
  reserve and the ~1,890 B static worst case. See
  [Optimisation levels](#optimisation-levels).
- **IWRAM code can grow another 1,664 B** (3,712 − 2,048) before the link
  fails.

### Deferred state actions

- **State actions no longer nest.** Setting a state from inside an action
  used to run the new action straight away, on top of the running one. Now
  it is queued and run afterwards, so "two or three nested actions" (2,660 and
  3,476 B with the sound IRQ) are gone. The worst case is one action, about
  1,880 B (with the corrected mixer depth), and data can no longer push it higher (see
  [Deferred state actions](#deferred-state-actions)).
- **`P_SetMobjState` moved from `r_hotpath.iwram.c` to `p_mobj.c` (ROM).**
  IWRAM code −216 B, stack +216 B. Timedemos are 0.5–0.6% slower.

### Iterative sound flood

- **`P_RecursiveSound` replaced by a breadth-first queue in `P_NoiseAlert`**
  (`p_enemy.c`). The old flood recursed once per sector (40 B a level) and
  set the deepest measured peak: 24 levels under `P_FireWeapon` in Doom 2
  demo3, 1,728 B with the mixer. The flood now uses a fixed ~132 B of stack
  from `A_WeaponReady` (`P_FireWeapon` 32 → `P_SoundSpread` 64 →
  `P_LineOpening` 12) on any map. See [Sound flood](#sound-flood).
- **Costs `numsectors` × 2 B of zone heap per level** (`_g->soundqueue`,
  PU_LEVEL): 696 B on Doom 2's largest map (MAP14, 348 sectors), 1,674 B on
  Sigil E3M7 (837).
- **Same game state.** All seven timedemos end in exactly the same state as
  the action-queue build, and run equal or up to 8 realtics faster.

### 2026-10-08 to 2026-10-10

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
| `.iwram` (code) | 0x03000000–0x03006430 | 25,648 | |
| `.bss` | 0x03006430–0x03006B34 | 1,796 | cleared by crt0 |
| `.data`, `.init_array`, `.fini_array` | 0x03006B34–0x03006B50 | 28 | |
| **Main stack (User/System mode)** | 0x03006B50–0x03007F00 | **5,040** | grows down from `__sp_usr` |
| IRQ-mode stack | 0x03007F00–0x03007FA0 | 160 | `__sp_irq`; uses 16 B (see below) |
| SVC stack and BIOS area | 0x03007FA0–0x03008000 | 96 | BIOS SWIs, IRQ vector, `__irq_flags` |

The stack has no guard. If it grows past 0x03006B50 it silently overwrites
`.data`, then `.bss` (`IntrTable`, then `r_hotpath`/`s_mix` statics). A
trashed `IntrTable` sends the next VBlank to a garbage address. `I_Error` no
longer depends on `IntrTable` or `_g`, so a later `I_Error` still displays.
An overflow itself is not detected.

## Static contents

### `.iwram` code (25,648, ARM default at O2)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` (code + 8 B `.data`) | 22,092 | `R_Subsector` 7,112 (with `R_AddLine`, `R_StoreWallRange` and `R_RenderSegLoop` inlined), `R_RenderPlayerView` 3,764, `P_CrossBSPNode` 1,944, `R_DrawVisSprite` 1,308, `R_MapPlane` 1,188, `R_AddSprites` 1,160, `R_RenderMaskedSegRange` 1,032, `R_RenderBSPNode` 824, `R_DrawColumn` 664, `R_MakeSpans` 320 |
| libtimidity (`TIMI_IWRAM`) | 2,368 | `_timi_resample_voice` 1,616, `update_signal` 420, `_timi_mix_voice` 332 |
| `s_mix_gba.s` | 532 | `S_MixResample` 212, `S_MixDirect` 156, `S_MixOutput` 152 |
| `fixeddiv.s` | 412 | |
| libgba IRQ dispatcher (`IntrMain`) | 184 | |
| Linker interworking stubs, alignment | 60 | |

### `.bss` (1,796) and `.data` (28)

| Owner | Bytes | Largest items |
|---|---|---|
| `r_hotpath.iwram.c` | 684 | `spanstart` 160, `flatStats` 128, `solidcol` 120 |
| `s_mix_gba.s` | 896 | `mixbuf` 896 |
| libgba | 158 | `IntrTable` 120 |
| LTO globals (`_g`, `mainzone`, ...), crtbegin, alignment | 58 | |
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

Static analysis of the linked ELF (`objdump -d`). Each function's frame
comes from its prologue (`push` / `sub sp`). The call graph has:

- direct `bl` calls, with interworking veneers resolved to their target;
- register calls whose address comes from a literal pool. That covers the
  ARM code in IWRAM calling into ROM (`mov lr, pc; bx r3`) and Thumb long
  calls into IWRAM;
- calls through data pointers, added by hand: `P_RunThinkers` calls every
  thinker, the line traversers get their `PTR_*` callbacks (per caller where
  the call site fixes it), and `P_SetMobjState`, `P_SetPsprite`,
  `P_MovePsprites` and `P_RunPendingActions` call every state action.

A state action runs at most once on a path: inside an action,
`P_SetMobjState` / `P_SetPsprite` only queue (see
[Deferred state actions](#deferred-state-actions)). Any action is allowed at
any state change, since DeHackEd can put any action on any state. The
analysis doesn't know about flag checks (for example that missiles don't
take line teleports), so its figures are upper bounds. `I_Error` is left out
of the gameplay rows; it is covered [below](#i_error).

### Main loop

`main` 24 → `D_DoomLoop` 136 is under every row (160 B).

| Path | Depth | Notes |
|---|---|---|
| Render (`main` → `R_RenderPlayerView`) | 820 | 160 + 660: `R_RenderPlayerView` 144 → `R_RenderBSPNode` 176 → `R_Subsector` 224 → `R_AddSprites` 88 → `P_AnimatedState` 16 → `__aeabi_idivmod` 12 |
| Gameplay tick, one state action | ~1,400 | Deepest path, below |
| Level load with a state action | ~1,220 | 160 + `G_Ticker` 72 → `G_DoLoadGame` 72 → `G_DoLoadLevel` 32 → `P_SetupLevel` 96 → `P_LoadThings` 64 → `P_SetPsprite` 40 (player weapon) → `P_RunPendingActions` 40 → action (as below, 640) |
| Level load / Load Game with an `lprintf` | 656 | 160 + `G_Ticker` 72 → `G_DoLoadGame` 72 → `G_DoLoadLevel` 32 → `P_SetupLevel` 96 → `P_LoadThings` 64 → `lprintf` 160. Was 1,944 with newlib. |
| Finale music change (`S_ChangeMusic` → `lsnprintf`) | ≤ 464 | 160 + `G_Ticker` 72 → `F_StartFinale` 8 → `S_ChangeMusic` 72 → `lsnprintf` ≤ 152. Was 1,728 with `snprintf`. |
| Startup (`D_DoomMainSetup`) | ≤ 488 | `main` 24 → `D_DoomMainSetup` 104 → `R_InitSpriteDefs` 64 → `R_InstallSpriteLump` 24 → `I_Error` 272. Was 1,608 + 152. |

The deepest gameplay path, 1,400 B: a monster walks over a teleport line and
telefrags a monster, whose new state's action fires a missile that telefrags
again:

| Frames | Depth | Running total |
|---|---|---|
| `main` 24 → `D_DoomLoop` 136 → `G_Ticker` 72 → `P_RunThinkers` 16 → `P_MobjThinker` 16 → `P_XYMovement` 64 | 328 | 328 |
| → `P_TryMove` 72 → `P_CrossSpecialLine` 48 → `EV_SilentLineTeleport` 8 + 88 → `P_TeleportMove` 80 → `P_DamageMobj` 56 | 352 | 680 |
| → `P_SetMobjState` 40 → `P_RunPendingActions` 40 | 80 | 760 |
| → `A_Mushroom` 152 → `P_SpawnMissile` 32 → `P_CheckMissileSpawn` 16 | 200 | 960 |
| → `P_TryMove` 72 → … → `P_DamageMobj` 56 (as above) | 352 | 1,312 |
| → `P_SetMobjState` 40, which queues the action (what it calls adds ≤ 48) | 88 | 1,400 |

In the vanilla game a missile can't take a line teleport, so the second
telefrag can't happen there. The other way round, the analysis assumes the
inner `P_SetMobjState` queues. A 0-tic state, or any state once the queue is
full, still runs its action on top (see
[Deferred state actions](#deferred-state-actions)); a DeHackEd patch that
gives a 0-tic state a deep action can nest one more.

The same tool gives 1,472 B on `37dbcc7`. The ~1,600 B it replaces was the
hand-built 2026-10-09 figure, adjusted since.

### Deferred state actions

A "state action" is a monster or weapon state's action function, run by
`P_SetMobjState` (mobjs) or `P_SetPsprite` (weapons) when the state is set.
Actions used to nest. For example, a weapon fires a rocket that explodes at
spawn (`A_Explode`), damages an idle monster (its see state runs `A_Chase`),
and that monster walks over a teleport line and telefrags something (its
death state runs `A_Scream`). Two and three nested actions were 2,416 and
3,232 B.

In vanilla's state data the chain is at most four actions deep, but a
DeHackEd patch can put any action on any state. The
enemy-rockets cheat (`CF_ENEMY_ROCKETS`, which turns every monster attack
into `A_CyberAttack`) makes it a loop, limited only by the number of idle
monsters in reach.

Now the outermost `P_SetMobjState` / `P_SetPsprite` call sets
`_g->runningaction`. Any state set while it is on still sets `state` and
`tics` immediately, but its action goes into `_g->pendingactions`
(`MAXPENDINGACTIONS` = 32, 256 B of EWRAM) instead of running.
`P_RunPendingActions` runs the queue in order after the outer action
returns:

- Actions run from the queue queue their own follow-ons, so the chain runs
  one after another instead of nesting.
- Entries whose mobj was removed are skipped. Removal is delayed until the
  thinker loop reaches the mobj, so the pointer is still valid.
- Entries whose mobj changed state again before their turn are skipped.

Some states still run straight away, as before:

- **0-tic states.** A queued 0-tic state would never count down. In
  vanilla's data only Revenant and Arch-vile attack states are 0-tic, and
  they're always set by the mobj on itself. DeHackEd patches can add more.
- **Everything when the queue is full.** Correctness doesn't depend on its
  size.

**Measured.** Doom 2 demo1 and Ultimate Doom demo4 never queue anything, and
their end states match `66899ab` exactly. Doom 2 demo2/demo3 and Ultimate
Doom demo1–3 queue 40–129 actions each, with at most 3 waiting at once.
They play to the end; their end states differ from `66899ab`, as expected.

**Behaviour change.** A queued action runs after the action that triggered
it instead of partway through it. `P_Random` order changes, so timedemos
that queue anything diverge from older builds: re-baseline A/B checks. A
mobj whose state changes twice before its queued action runs only runs the
newer state's action. Actions also no longer run inside another mobj's
`P_TryMove`, where they could overwrite the `tm*` globals.

**Speed and placement.** Realtics, Doom 2 demo1 / Ultimate Doom demo4:

| Build | IWRAM code vs `66899ab` | demo1 | demo4 |
|---|---|---|---|
| `66899ab` (`P_SetMobjState` in `r_hotpath.iwram.c`) | 0 | 2,069 | 1,898 |
| Queue, `P_SetMobjState` in ROM (current) | −216 | 2,081 | 1,908 |
| Queue, `P_SetMobjState` in IWRAM (ARM, Os, loop inlined, drain in ROM) | +232 | 2,073 | 1,901 |

Most of the cost is running the state loop from ROM. The extra bookkeeping
is about 0.2%. The IWRAM variant trades 448 B of IWRAM for ~0.35%, about
the same return per byte as building `r_hotpath.iwram.c` at O2.

### Sound interrupt

The VBlank IRQ is the only interrupt enabled (`i_audio.c`). libgba's
`IntrMain` pushes 16 B on the IRQ stack, then **switches to System mode and
calls `I_SoundVBlank` on the main stack**. So the mixer's stack use adds to
whatever depth the main loop is at when VBlank arrives.

| Path | Depth |
|---|---|
| `lr` pushed by `IntrMain` in System mode | 4 |
| `I_SoundVBlank` (`S_MixFrame` inlined) | 48 |
| → `mid_song_render` | 64 |
| → `_timi_mix_voice` (IWRAM) | 40 |
| → `ramp_out` (ROM, voice ending) | 40 |
| → `_timi_resample_voice` (IWRAM) | 104 (96 at Os) |
| → `update_vibrato` 16 → `__aeabi_lmul` 24 (`S_MixResample` is 36) | 40 |
| **Total added to the main stack** | **~340** |

Until `89f805b`, `mid_song_render` was inlined into `I_SoundVBlank` (72 B
together, ~292 B for the mixer). The figure before that (244 B) missed
`ramp_out`, which `_timi_mix_voice` reaches through a long call that the
old direct-call analysis didn't follow. Its frame
was 120 B, with an 80 B `tmp[MAX_DIE_TIME]` buffer on the stack (the mixer
measured 356 B at the Doom 2 demo3 peak). That buffer is now
`MidSong.ramp_tmp`, in the zone-allocated song state in EWRAM.
A `static` would have gone to IWRAM `.bss`, just moving the bytes.

On Doom 2 demo2 (the heaviest music) all 73 `ramp_out` calls produce the
same samples as before, and realtics are unchanged (2,988). The extra EWRAM
accesses (~80 a call, up to ~400 cycles) do shift the game thread against
the VBlank slightly. One of 292 sound starts lands a VBlank later, and 11 of
5,150 output frames differ for that reason. That is timing, not a change in
what the mixer computes.

**The mixer can't re-enter itself.** `IntrMain` saves IME, writes
`0x04000000` to it (bit 0 clear, so IME off) before calling the handler, and
restores it only after `I_SoundVBlank` returns. Nothing in the mixer writes
`REG_IME`. A long mix or a held-off VBlank only delays the next one. Checked
over a Doom 2 demo3 timedemo with GDB breakpoints on `I_SoundVBlank`'s entry
and exit: 12,231 entries, 0 re-entries, IME off at every entry.

### Combined worst case (main + sound IRQ)

| Scenario | Depth | Spare of 5,040 |
|---|---|---|
| Level load with `lprintf` | ~1,000 | ~4,040 |
| Render | ~1,160 | ~3,880 |
| Level load with a state action | ~1,560 | ~3,480 |
| Gameplay, one state action | ~1,740 | ~3,300 |

The render row matches the measured peak of four of the six timedemos
(1,128–1,156 B, the mixer over sprite projection; see [Measured](#measured)).
A sight check can go deeper than all of these; see
[Sight checks](#sight-checks).

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
→ `consolePrintChar` 24. Called from the deepest one-action gameplay path (a
`Z_Malloc` failure in `P_SpawnMobj` after the second telefrag), it reaches
1,648 B. It turns off the sound IRQ first, so the mixer doesn't add to that.

The old `I_Error` used `vsnprintf` into a 256 B stack buffer and needed
1,656 B. The timedemo result's `%f` made `_dtoa_r` call `Balloc`. Its malloc
failed, because the zone heap takes nearly all the free memory at startup.
newlib then asserted ("Balloc succeeded", `mprec.c` line 783) and the result
was never shown.

## Measured

The stack was painted with `0xDEADBEEF` at `main` via mGBA's GDB stub, a
timedemo was run, and the low-water mark was read back.

`5390152`, 2026-10-20 (all with the sound IRQ; Ultimate Doom demo4 no longer
runs, since GbaWadUtil keeps only DEMO1–3):

| Timedemo | Gametics / realtics | Peak | What set the peak |
|---|---|---|---|
| Doom 2 demo1 | 1,198 / 1,965 | 1,156 | mixer over sprite projection (`R_AddSprites` → `P_AnimatedState`, ~812) |
| Doom 2 demo2 | 1,994 / 2,896 | **1,364** | mixer over a sight check, `P_CrossBSPNode` recursing (~1,100 B before the mixer) |
| Doom 2 demo3 | 4,464 / 6,203 | 1,240 | mixer over a sight check (`P_CrossBSPNode`, ~830) |
| Ultimate Doom demo1 | 1,703 / 2,675 | 1,156 | mixer over sprite projection |
| Ultimate Doom demo2 | 2,340 / 3,090 | 1,128 | mixer over sprite projection |
| Ultimate Doom demo3 | 3,856 / 4,793 | 1,156 | mixer over sprite projection |

The render rows match the static figures (820 + ~340 = ~1,160). Doom 2
demo1 ends in the same state as on 2026-10-16 (health 79, leveltime 207).

Earlier builds:

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

Peaks across the stack work, all with the sound IRQ. Once anything is
queued, the action-queue demos play out differently from `66899ab`, so
those two columns come from different moments. The iterative sound flood
changes no game state, so its column is the same play as the queue column:

| Timedemo | `66899ab` | Action queue | + iterative sound | What set the peak |
|---|---|---|---|---|
| Doom 2 demo1 | 1,204 | 1,200 | 1,200 | mixer over sprite drawing (nothing queued) |
| Doom 2 demo2 | 1,360 | 1,364 | 1,352 | |
| Doom 2 demo3 | 1,360 | 1,728 | 1,368 | queue build, caught with a watchpoint: weapon noise `P_RecursiveSound` 24 levels deep (~960 B) with the mixer (356 B) on top |
| Ultimate Doom demo1 | 1,204 | 1,292 | 1,292 | |
| Ultimate Doom demo2 | 1,676 | 1,512 | 1,332 | `66899ab`: `P_RecursiveSound` 5+ levels deep, with the mixer on top |
| Ultimate Doom demo3 | 1,228 | 1,224 | 1,224 | |
| Ultimate Doom demo4 | 1,504 | 1,536 | 1,272 | (nothing queued) |

None of these peaks is a nested state action, so the action queue doesn't
show in them. It caps the worst case, which these demos don't reach. The
sound flood set the top two peaks, and replacing it removed them.

Stale return addresses can mislead: the Doom 2 demo3 residue showed two
`I_SoundVBlank` frames that were left over from earlier interrupts. To get
the real chain, paint once to find the low-water address, then rerun with a
write watchpoint on that word (`watch *(int*)0x03007840`; mGBA's GDB stub
supports hardware watchpoints) and dump the stack from `$sp` when it fires.

The original layout (1,336 B stack) couldn't be painted the same way, but the
gameplay peak above was already 96 B over its limit, and on that build mGBA
reported "Jumped to invalid address" at the timedemo's final `I_Error`.

The first two rows predate the sound IRQ, so they don't include the ~292 B
the mixer adds.

## Sound flood

`P_NoiseAlert` (`p_enemy.c`, called from `P_FireWeapon`) wakes monsters by
flooding the weapon noise through adjacent sectors. Closed doors stop it,
and it can cross one `ML_SOUNDBLOCK` line. Each sector reached gets
`soundtarget` and `soundtraversed` = 1 + the fewest blocking lines crossed
to reach it.

The old `P_RecursiveSound` did this depth first, one 40 B frame per sector
on the current path. Vanilla's revisit rule (a sector reached with 1
blocking line is redone if later reached with 0) means the final marks don't
depend on the order. Line openings don't change during the walk, so a
breadth-first flood gives the same result:

1. `P_SoundReach` marks the emitter's sector and appends its index to
   `_g->soundqueue`.
2. `P_SoundSpread` floods from each queued sector through open,
   non-blocking two-sided lines into unreached sectors (mark 1).
3. From every mark-1 sector it crosses blocking lines into unreached sectors
   (mark 2), then floods on through non-blocking lines (mark 2).

Each sector is queued at most once, so `_g->soundqueue` holds `numsectors`
entries. It is allocated with the sectors in `P_LoadSectors` (PU_LEVEL,
2 B a sector). Stack use is fixed: about 132 B from `A_WeaponReady`.
`P_LineOpening` is now only called for lines into unreached sectors, so it
runs no more often than before. The last `open*` values it leaves differ,
but every reader calls `P_LineOpening` first.

## Sight checks

`P_CheckSight` (`p_sight.c`) calls `P_CrossBSPNode` (`r_hotpath.iwram.c`),
which walks the BSP along the sight line. Where the line crosses a node's
partition it recurses into the start side and then loops on to the end side.
That makes it the one remaining recursion whose depth depends on the map.
Each level is an 80 B frame (36 B of registers, 44 B of locals, with
`P_CrossSubsector` inlined), plus 32 B for `P_InterceptVector2` at the
bottom.

The nesting can't exceed the BSP tree's depth, and for a given line it is
the number of crossed partitions stacked on one start side. The simulation
below ran the same walk for the line between every pair of things on every
map, in both directions. It ignores REJECT and the walls that end a check
early, so for those lines it is an upper bound, but monsters and the player
move off their start spots, so other lines are possible:

| WAD | BSP tree depth (map) | Deepest nesting (map) | Median map |
|---|---|---|---|
SIGHT_TABLE

Stack at a sight check is the entry depth + 80 B a level + 32 B, plus ~340 B
when the mixer interrupts it:

- **Usual entry, 384 B:** a monster's `A_Chase` / `A_Look` from its
  thinker (`main` 24 → `D_DoomLoop` 136 → `G_Ticker` 72 → `P_RunThinkers`
  16 → `P_MobjThinker` 16 → `P_SetMobjState` 40 → `A_Chase` 24 →
  `P_LookForPlayers` 32 → `P_CheckSight` 24). 19 levels (Doom 2 MAP24) is
  then 384 + 1,520 + 32 + 340 = **2,276 B**.
- **Deepest entry, 1,000 B:** `P_RadiusAttack` from an action at the bottom
  of the telefrag path in [Main loop](#main-loop). 19 levels is then
  **2,892 B**.

Doom 2 demo2's measured peak (1,364 B) is the mixer over a sight check that
had reached ~1,100 B, about 9 levels from `A_Chase`.

Replacing the recursion with an explicit stack of node numbers, as
`R_RenderBSPNode` does, would cost 2 B a level instead of 80. That loop's
own stack is fixed at `MAX_BSP_DEPTH` (64) entries and it stops descending
when the stack is full; TNT MAP16's tree is 86 deep.

## Optimisation levels

`include/code_opt.h` sets three levels for the ARM code in IWRAM:

| Define | Default | Used by |
|---|---|---|
| `HOT_CODE_OPT_LEVEL` (`HOT_CODE`) | O3 | `R_DrawColumn`, `R_MapPlane` (both also `flatten`) |
| `WARM_CODE_OPT_LEVEL` (`WARM_CODE`) | O3 | the BSP chain (`R_BSP_OPT`, 20 functions) and `R_RenderSegLoop`, which inlines into it |
| `ARM_CODE_OPT_LEVEL` (`ARM_CODE_DEFAULT`) | O2 (was Os) | the rest of `r_hotpath.iwram.c`, and `TIMI_IWRAM` |

With O3 / O3 / Os the defines built byte-identical code to before they were
added. IWRAM code size for other settings (measured with ARM at Os = 22,288 B):

| HOT / WARM / ARM | IWRAM code | Change |
|---|---|---|
| O3 / O3 / Os (before) | 22,288 | 0 |
| O3 / O2 / Os | 21,184 | −1,104 |
| O2 / O2 / Os | 21,184 | −1,104 |
| O3 / O3 / O2 (current) | 25,928 | +3,640 |
| Os / Os / Os | 19,672 | −2,616 |
| O3 / O3 / O3 | 27,824 | +5,536 (fails the 2 KB stack reserve) |

Ultimate Doom demo4, HOT and WARM at O3 (identical end states and stack
peak, 1,272 B):

| ARM default | IWRAM code | Free stack | Realtics | fps |
|---|---|---|---|---|
| Os | 22,288 | 7,356 | 1,906 | 14.89 |
| **O2 (chosen)** | 25,928 | 3,712 | 1,881 | 15.09 (+1.3%) |
| O3 | 27,824 | 1,816 | 1,857 | 15.29 (+2.6%) |

At the time O3 needed the stack worst case cut by roughly 300 B first (to
fit the 2 KB reserve and the then ~1,890 B static worst case).

**O3 now fits.** On `5390152` with ARM default at O3, IWRAM code is 27,624 B
(+1,976 over O2) and the stack 3,064 B, which passes the 2 KB reserve. O3
also makes the paths shallower: render 820 → 712 B (`R_RenderPlayerView`'s
chain 660 → 552) and the mixer ~340 → ~312 B (`_timi_mix_voice`'s chain
224 → 196). The gameplay worst case stays ~1,400 B, so with the mixer it is
~1,712 B, leaving ~1,350 B. Not timed yet.

The HOT functions come out the same size at O2 and O3. Changing a level
changes which functions can inline into each other (GCC won't inline across
levels), so time any change with the timedemos.

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
  statics in `r_hotpath.iwram.c`, `s_mix_gba.s` and `TIMI_IWRAM` functions.
  At the static worst case (~1.74 KB), about 3.3 KB can still be added
  before the stack has no margin. The old ~4 KB floor came from the
  three-nested-action case (3,476 B), which the action queue removed. The
  one path that grows with the map is the sight check (see
  [Sight checks](#sight-checks)).
- **The link fails below a 2 KB stack.** `gbadoom.ld` sets
  `__stack_reserve = 0x800` and asserts `__iheap_start + __stack_reserve <=
  __sp_usr`. IWRAM code and data can grow by 2,992 B (5,040 − 2,048) before
  the build stops with "IWRAM overflow: less than __stack_reserve (2 KB) left
  for the main stack". That leaves ~300 B above the ~1.74 KB static worst
  case and ~680 B above the deepest measured peak (1,364 B). Raise the
  reserve if a new path turns out deeper.
- **Avoid recursion whose depth depends on the map** (sector, line or BSP
  walks driven by game logic). `P_RecursiveSound` was one, and it set the
  deepest measured peak until it was replaced (see [Sound flood](#sound-flood)).
  `P_CrossBSPNode` is the one left, 80 B a level (see
  [Sight checks](#sight-checks)).
- **Keep buffers off the mixer's stack.** The mixer runs on top of whatever
  the game is doing when VBlank hits, and can't re-enter (see
  [Sound interrupt](#sound-interrupt)). Scratch buffers belong in `MidSong`
  or `_g` (EWRAM), as `ramp_out`'s now does, not on the stack or in IWRAM
  `.bss`.
- **Start state actions only through `P_SetMobjState` / `P_SetPsprite`.**
  They set `_g->runningaction`, which is what makes nested state changes
  queue. An action calling another action directly (as `A_Hoof` calls
  `A_Chase`) is fine; it runs on the same level.
- **`main` + `D_DoomLoop` (160 B) is under every path.** LTO inlines
  `D_Display` and many of the drawers (`WI_*`, `ST*`, `HU*`, `AM_*`,
  `M_Drawer`) into `D_DoomLoop`. Their locals stay reserved during
  `G_Ticker` too. Until `-fno-inline-functions-called-once` (`89f805b`) all
  of it was in `main`, 168 B.
- **Big stack frames on the render path.** O3 inlining made `R_Subsector`'s
  frame 224 B, and it sits under `R_RenderBSPNode` (176 B). Inlining more
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
cache always ends at the end of OBJ VRAM. The `vram1_spare_t` check is GBA
only: on 64-bit desktop builds `vissprite_ptrs` is twice the size, and the
regions there are ordinary memory. The VRAM copies of ROM tables take
their array sizes from the ROM declarations, so the copy sizes always match.
To add a table, add a member. Free space is the region size minus `sizeof`
the struct, or the `unused` member in `vram_tail_t`.

### VRAM (98,304)

| Region | Range | Bytes | Used | Free |
|---|---|---|---|---|
| Page 1 framebuffer | 0x06000000–0x06009600 | 38,400 | 38,400 | 0 |
| Page 1 gap (`vram1_spare`) | 0x06009600–0x0600A000 | 2,560 | 2,484 | 76 |
| Page 2 framebuffer | 0x0600A000–0x06013600 | 38,400 | 38,400 | 0 |
| Page 2 gap + OBJ VRAM (`vram_tail`) | 0x06013600–0x06018000 | 18,944 | 18,688 | 256 |
| **Total** | | **98,304** | **97,972** | **332** |

Contents (offsets are bytes into the region, as laid out by the compiler):

| Region | Offset | Member | Bytes |
|---|---|---|---|
| `vram1_spare` | 0 | `yslope` (copy of ROM `yslope[128]`, one entry per view row) | 512 |
| | 512 | `distscale` (copy of ROM `distscale[120]`) | 480 |
| | 992 | `xtoviewangle` (copy of ROM `xtoviewangle[121]`) | 484 |
| | 1476 | `wipe_y_lookup` | 240 |
| | 1716 | `vissprite_ptrs` (`MAXVISSPRITES` × 4) | 384 |
| | 2100 | `dsclipInfo` (`MAXDRAWSEGS` × 2: drawseg index, bit 15 = masked) | 384 |
| `vram_tail` | 0 | `lightColormaps`, all 32 light levels of COLORMAP (0x06013600–0x06015600) | 8,192 |
| | 8192 | `dsclipXkey`, `dsclipLowscale`, `dsclipScale` (`MAXDRAWSEGS` × 4 each, per-frame drawseg summary for sprite clipping, 0x06015600–0x06015F00) | 2,304 |
| | 10496 | `unused` (free, 0x06015F00–0x06016000) | 256 |
| | 10752 | `flatCache`, 2 slots × 4,096 (0x06016000–0x06018000) | 8,192 |

The page 2 gap and OBJ VRAM are adjacent, so the free space is kept in one
block and the flat cache at the end. The light colormaps are one block so
colormap `n` is `lightColormaps[n]` (a constant base plus `n << 8`), with no
pointer table. The sprite-clip summary is split into arrays of 4 B fields
(plus `dsclipInfo`) rather than 16 B structs, to make room for them. Every
entry still indexes by a shift. `screenheightarray` and
`negonearray` moved from the page 2 gap to OAM to empty it. 4 flat slots
measured no faster than 2.

`R_InitBuffer` (`r_draw.c`) fills the tables and the light colormaps, and
`R_LoadFlat` fills the flat cache, all with `BlockCopy`. Nothing else writes past row 160 of either page.
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
| 0x05000000–0x05000200 | 512 | BG palette: the 256-colour game palette, DMA copied from the `GBAPAL` lump by `I_SetPallete_e32`; entries 0 and 241 are also set for the text console |
| 0x05000200–0x05000300 | 256 | `objpal_spare->fixedColormap`: the current `fixedcolormap` (invulnerability or light amplification), copied by `R_RenderPlayerView` only when it changes |
| 0x05000300–0x05000400 | 256 | free (held colormap 0 until the light colormaps moved to VRAM) |

Drawers read the fixed colormap from here when one is active. In mGBA,
lookups from palette RAM and VRAM run at IWRAM speed (sky: 19.0
cycles/pixel against 18.9 from IWRAM). On hardware, a CPU access can take one
extra cycle when the display reads the palette or VRAM at the same time.

### Free video memory

| Where | Bytes | Notes |
|---|---|---|
| `vram_tail->unused`, 0x06015F00–0x06016000 | 256 | |
| `vram1_spare` | 76 | |
| OAM | 48 | |
| OBJ palette, 0x05000300–0x05000400 | 256 | |

VRAM, OAM and palette RAM need 16- or 32-bit writes, because 8-bit writes to
VRAM store the byte to both halves of the halfword. Any table moved there
must be accessed that way.
