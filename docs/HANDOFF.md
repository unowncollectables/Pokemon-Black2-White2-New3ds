# Handoff

Self-contained notes so another agent can take over this New 3DS port.
Phases: [PLAN.md](PLAN.md). Build extras: [BUILDING.md](BUILDING.md).
Short rules pointer: [AGENTS.md](../AGENTS.md).

## 1. Goal and rules

A native New 3DS port of Pokémon Black 2 and White 2 (US/EU). The decompiled
C runs on the ARM11. A platform layer owns the 3DS GPU, input, audio, and
where the save file lives. Personal, experimental, GPL-3.0. Owner: GitHub
`unowncollectables`. The repo is **private**.

Black 2 is the first game that should keep moving past the title. White 2 is
built from the same tree at every step. The byte-matching DS rebuild stays.

Hard rules (do not break these):

- Never commit ROMs, extracted NitroFS, assets pulled from a dump, saves,
  built binaries, logs, or personal information. `.gitignore` covers the
  common cases (`.nds`, `extract/`, `.sav`, `.3dsx`, `.cia`, `.elf`, staged
  nitrofs). Check `git status` before every commit.
- Never download a ROM. The user supplies their own dump and keeps it
  outside git.
- Save format is unchanged. Only the game writes a `.sav`. The platform
  must not create one and must not write one on boot, exit, or from a
  settings toggle.
- Every new gameplay feature is a toggle in the in-game Settings screen
  (L+R+Y). Risky ones default **off**. No randomizer, cheats, or
  fast-forward. The game does not reset or relaunch itself.
- Credit reused GPL work in `n3ds/THIRD_PARTY.md`.
- Do not build or commit a CIA. The user builds installers with their own
  Builder (see §11).
- Do not force-push. Merges need the user's yes. Branch
  `cursor/<name>-5254` for this agent's work; open a PR to `main`.
- New 3DS only. CIA/RSF metadata requests 804 MHz, the L2 cache, and
  core 2. Do not design around Old 3DS memory or a missing extra core.

## 2. Base pins

From `upstream.lock`:

- Decomp **pokebw2** `345b790e6000869b481b79838bb5727309f832fb` (the
  matching tree is a separate checkout, not this `port/` worktree).
- Black 2 US ROM SHA1 `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb`
- White 2 US ROM SHA1 `b5d7490be7b415b8f1e672a53e978a9cc667e56a`
- Sister ports: `unowncollectables/3ds-platinum` and
  `unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds@4d4d2cb`
- PC reference only (not vendored): `CallanCampos/Frigate`

Title IDs: Black 2 `000400000F7D4200`, White 2 `000400000F7D4300`.

## 3. Box layout

This machine's working tree (nothing here is committed except what lives
under `port/`):

| Path | What |
| --- | --- |
| `/workspace/b2w2-3ds` | Box workspace root |
| `/workspace/b2w2-3ds/port` | This git repo (branch `cursor/b2w2-n3ds-port-5254`) |
| `/workspace/b2w2-3ds/pokebw2` | Matching decomp checkout, pinned at `345b790` |
| `/workspace/b2w2-3ds/sd-stage/` | Staged SD tree `3ds/{black2,white2}_3ds/` (not git) |
| `/workspace/b2w2-3ds/smoke/` | Azahar smoke shots and logs, per round (not git) |
| `/workspace/devkitpro` or `/opt/devkitpro` | Toolchain (`DEVKITPRO`) |
| `/workspace/tools/estyseesghosts/azahar` | Azahar. Binary: `build/bin/Release/azahar` |

Smoke helpers (box, not all of them are in the repo):

- `/workspace/b2w2-3ds/scripts/azahar-smoke-wide.sh` — 90s Black 2 layout
  shots. Typical times: `3 6 9 12 15 18 20 25 30 35 40 45 50 55 60 65 70
  75 80 85 90`. Log filter: `*:Info HW.Memory:Critical`. Crops:
  TOP=(272,240,528,432), BOT=(272,480,528,672); stack to 256×384, then 3×
  nearest-neighbor for a readable still.
- In-repo: `tools/box/azahar-smoke.sh`, `tools/box/build-matching.sh`.
  Set `NITROFS_SRC` to the staged `nitrofs` directory and `SD_SUBDIR` to
  `3ds/black2_3ds` or `3ds/white2_3ds`.

`PBW2_LOCAL_SYSBASE=1` is only for a machine whose `arm-none-eabi-gcc` is
not devkitARM's. Leave it unset on a normal devkitPro install.

### How to bring a ROM in (never download one)

The user already has a dump, or they place one. Do not fetch a ROM from
the network or from another repo.

1. Confirm SHA1 against `upstream.lock` (`e51e6dfb…` / `b5d7490b…`).
2. Extract NitroFS locally (the matching `pokebw2` tree can do this when
   `orig/baserom_b2_us.nds` / `orig/baserom_w2_us.nds` are present).
3. Stage, writing **outside** this git repo:

```sh
bash scripts/n3ds_stage_nitrofs.sh b2 /path/to/extract/b2_us/files /workspace/b2w2-3ds/sd-stage
bash scripts/n3ds_stage_nitrofs.sh w2 /path/to/extract/w2_us/files /workspace/b2w2-3ds/sd-stage
```

Default destination if you omit the third argument is `$HOME/pbw2-sd`.
The script refuses to write inside the git worktree. Copy the resulting
`3ds/` tree onto a card so the game sits at
`sdmc:/3ds/black2_3ds/nitrofs` (or `white2_3ds`). The shell opens
`nitrofs/a/0/0/2` and logs the size. It does not write a `.sav`. The two
games do not open each other's files.

Matching DS rebuild (the only step that reads the ROM):

```sh
# orig/baserom_b2_us.nds  SHA1 e51e6dfb8678a3d19dcd2a10691b96a569ca0abb
# orig/baserom_w2_us.nds  SHA1 b5d7490be7b415b8f1e672a53e978a9cc667e56a
python3 configure.py && ninja
```

Header generation and the 3DS shell do not need the ROM file itself, only
staged NitroFS if you want in-game archives.

## 4. Build, checks, and golden default shells

Default shells (no game `main`, no `PBW2_LINK_GAME`):

```sh
bash scripts/n3ds_build.sh
# same as: make -C n3ds
# outputs (gitignored):
#   n3ds/build/b2/black2_3ds.3dsx
#   n3ds/build/w2/white2_3ds.3dsx
```

Golden default shells, last confirmed at r29 (`df983ab569`):

- Size **146976** bytes for both
- Black 2 SHA1 `b7c59f88f7fa17c5f534b759caf9bb676ea6cc05`
- White 2 SHA1 `fd82c6343651ba60694f7645d27b0797de198de1`

Re-check those after any change that can reach default `n3ds/` or a
non-`#else` path in shared `src/` / `lib/`. Docs-only changes cannot
move them.

LINK_GAME (links `Pbw2_GameMain` on a worker; this is what Azahar smokes):

```sh
PBW2_LINK_GAME=1 make -C n3ds b2          # lean: Black 2 only
PBW2_LINK_GAME=1 make -C n3ds b2 w2
python3 scripts/n3ds_check_3dsx_reloc.py n3ds/build/b2/black2_3ds.3dsx
```

`python3 scripts/n3ds_experimental_check.py` rebuilds the LINK_GAME Black
2 closure and **overwrites** `n3ds/build/b2/`. Copy default-shell
artifacts aside before you run it, then restore them (or rebuild default)
before you compare golden SHAs.

`git status` after a box build should be clean.
`build/n3ds/pbw2_build_status.h` is gitignored.

### Ifdef rules

- `PBW2_N3DS` — 3DS/GCC-only branches in `src/` and `lib/` so the decomp
  compiles with devkitARM. The matching MWCC build does **not** define it.
  The DS `#else` path must stay byte-identical to pokebw2. Audit that
  whenever you touch a shared file.
- `PBW2_LINK_GAME` — set only on the experimental game link. Host VRAM
  window, real `MI_UncompressLZ8`, compositor present path, and related
  runtime stay behind this flag so default shells remain golden.
- Do not change shared non-n3ds code unless the DS `#else` is untouched
  or you also re-verify matching.

Game C, both versions, no ROM: 1218/1218 (with
`-Wno-incompatible-pointer-types`). Library C is 514/572 (GCC 14.2) or
516/572 (devkitARM 16.1.0 / GCC 16). The gap is mostly `__memcpy` and
MWCC `asm` blocks.

## 5. n3ds runtime architecture

`src/` and `lib/` are pokebw2. `n3ds/` is the 3DS program.
`scripts/n3ds_game_objects.py` compiles game/SDK C for ARM11.
`n3ds/source/` is the shell (main, workers, log, input, settings, save
shim, silent audio, present). With `PBW2_LINK_GAME=1`, `Pbw2_GameMain`
runs on a worker; overlays 12, 27, and 264 load from SD when staged.

This decomp speaks Nitro, not pspoke. SoulSilver and Platinum on 3DS
speak `psp_*`. Take their lessons (threads, present flags, save file,
804 MHz RSF, logging, Citro3D). Do not drop their renderer in as the GX
implementation.

Important host files under `n3ds/nitro/` and `n3ds/source/`:

| File | Role |
| --- | --- |
| `ds2d_compose.c` | Software 2D compositor (GPL-3.0, from Platinum `widecomp.c`) |
| `vram_host.c` | Host BG/OBJ/PLTT/LCDC/OAM; `PbW2_VramMap` / `PbW2_VramResolveArm` |
| `vram_dma.c` | DMA into host VRAM |
| `mi_host.c` | MI, including `MI_UncompressLZ8` (LZ10/LZ11) under `PBW2_LINK_GAME` |
| `affine_track.c` | Affine PA–PD / ref tracking for mode-5 bitmaps |
| `gx_host.c` | GX shims, DISPCNT/POWCNT, present hook |
| `g3_intercept.c` | 3D command intercept |
| `g3_gecom_host.c` | Real `NNS_G3dGeBufferOP_N` / flush / `NNS_G3dInit`; FIFO capture |
| `g3_gx_stub.c` | FIFO/display-list consumption stubs until Citro3D replay exists |
| `g3d_glb_stubs.c` | First-call stubs: `NNS_G3dDraw`, TexLoad/Bind, GetMdlSet, MTX |
| `game_present.c` | LINK_GAME present; default path must stay r15-identical |
| `overlay_host.c` | Overlay load from `sdmc:/3ds/<game>_3ds/overlays/` |
| `fs_host.c` | Path FS under staged `nitrofs/` |
| `pad_host.c` | Input. Must not self-reset |
| `save_shim.c` | Must not create or write a `.sav` |
| `settings.c` | L+R+Y Settings. No gameplay toggles yet; do not write `config.txt` until a real default-off toggle exists |

Display / POWCNT (keep this polarity):

- Official NDS and this port: `GX_DISP_SELECT_MAIN_SUB` (bit 15 set,
  `pow & 0x8000`) → main engine on the **top** 3DS LCD.
- `GX_DISP_SELECT_SUB_MAIN` (bit 15 clear) → sub engine on top, main on
  the bottom.
- Compose: `main_upper = (pow & 0x8000) != 0`.
- Copyright and CDEMO use `MAIN_SUB`. `Title_Init` uses `SUB_MAIN` (2D
  logo on top, 3D/main on the bottom). After 1740 frames the title
  switches to `MAIN_SUB` to put 3D on top.

Host BG window under `PBW2_LINK_GAME` is `PBW2_BG_SIZE=0x00680000`
(`0x06000000`–`0x06800000`), which includes sub BG `0x06200000` and sub
OBJ `0x06600000`, stopping just before LCDC. `PbW2_VramMap` returns the
raw ARM pointer if unmapped — that is silent data loss, not a crash.

`PbW2_VramResolveArm` must not compare a host `s_bg` pointer to the
literal `0x06000000` in a way that folds into a `R_ARM_ABS32` reloc
subtype 15. The compare uses a split `arm_base` (`0x05000000+0x01000000`).
Always run `scripts/n3ds_check_3dsx_reloc.py` after 3dsxtool.

CDEMO (overlay 264): display mode 5, BG2/BG3 16bpp DC bitmaps 256×256.
BG2 base `0x00000`, BG3 `0x20000`. Frames are LZ11 (type `0x11`),
uncompressed `0x20000`; `CDemo_LoadMovieFrame` DMAs the first `0x18000`
(256×192) at offset 0. Double-buffer via BG priority. Archives:
`ARCID_CDEMO_GFLOGO` 220 = `a/2/2/0` (151 frames); opening `a/2/2/2`.
`MI_UncompressLZ8` is a C translation of
`lib/nitro/src/mi/mi_uncompress.c` (LZ10 + LZ11), LINK_GAME only.

Title 2D: sub BG3 (`a/0/2/6` files 0/1/2) is the Pokémon Black 2 logo +
ice; sub BG2 (file 3) is scrolling ice; sub OBJ should be PRESS START;
main BG3 (files 8/9/10) is "Developed by GAME FREAK inc." Main display
mode is `GX_DISPMODE_VRAM_D` with `GX_SetCapture` of 2D3D+VRAM to bank D
for motion blur — capture and Citro3D replay are **not** wired, so the
3D Kyurem is missing.

## 6. Milestone history (r18–r29)

Smoke-round numbers, with the commit that landed the work:

| Round | SHA | What / lesson |
| --- | --- | --- |
| r18 | `3f11efe38e` | Present blit gated on `GameDisplayReady`. (Commit message says r17.) |
| r19 | `2807fdc71d` | Bitmap modes and engine routing. Copyright appeared on the **bottom** while `pow=0x8000`. |
| r20 | `521d12ce94` | **POWCNT polarity** inverted to the official mapping: bit 15 set → main on top. Copyright moved to the top LCD. **Keep this polarity.** |
| r21 | `29ea55d348` | Restored mode-5 16bpp (r20 had forced 8bpp; CDEMO went black). Format nits. |
| r22 | `d643f3f2a4` | Affine starfield work. VRAMCNT OFS was unmasked and could map `0x0620xxxx`. |
| r23 | `2e1ce9efb5` | Azahar load Error 1: typo in `PbW2_VramResolveArm` plus a **3dsx reloc subtype trap** — comparing host `s_bg` to `0x06000000` folded as `R_ARM_ABS32` subtype 15; `Load3DSXFile` failed. |
| r24 | `de9a8c168b` | Fixed the compare / split `arm_base`. Added `scripts/n3ds_check_3dsx_reloc.py`. Fixed VRAMCNT OFS decode. |
| r25 | `da0e30d98b` | Present / 1:1 shortcut still idle when PA was not identity. |
| r26 | `6e65130e41` | Identity-until-write for affine; title 2D and G3D scaffold. First LINK_GAME build failed on a missing `#include <string.h>` in `affine_track.c` (landed in r27). |
| r27 | `da31f5521e` | Mode-5 **512 B stride**; screen-base × `0x4000` (drop the upload-slot heuristic); CNT VRAM base; `g3_gecom_host.c` real GeBuffer / flush / `G3dInit`; `CpuSend32` to GXFIFO. CDEMO still looked like scanline garbage — the real bug was the LZ stub. |
| r28 | `8929ea2742` | **`MI_UncompressLZ8`** implemented (LINK_GAME only). CDEMO became a recognizable Game Freak logo and Black 2 opening. `Title_Init` reached. Unused `byte_off` warning removed. Top title LCD was still black. |
| r29 | `df983ab569` | **Sub VRAM window**: `PBW2_BG_SIZE` `0x00100000` → `0x00680000` so sub BG/OBJ map. Title 2D logo + scrolling ice on top, GAME FREAK line on the bottom. |

Default shells stayed 146976 / `b7c59f88` / `fd82c634` through r29.
LINK_GAME Black 2 at r29 is 1250620 bytes, SHA
`ecdc8b977bd80979e3a47f4c84f8b738fb4ac0a9` (BSS is not in the 3dsx).

## 7. Current state (after r29)

On a 90s Black 2 Azahar smoke with NitroFS staged and **no** `.sav`:

- TPC / Nintendo splash, then the copyright screen on the correct LCDs.
- Game Freak logo and the Black 2 opening movie play full-frame on the
  top LCD (CDEMO).
- `Title_Init` is reached. The title 2D logo and scrolling ice banner
  are recognizable on the top LCD. The bottom LCD shows a near-white
  field and "Developed by GAME FREAK inc."

Best stills from the last smoke live in
`/workspace/b2w2-3ds/smoke/r29/best/` (not in git).

## 8. Known issues and next steps (priority order)

The first New 3DS hardware run hit an ARM11 data abort on a halfword
write to the DS master brightness register (`0x0400006c`). The port now
routes both engines' brightness registers through `pbw2_io`, which the
2D compositor already reads. Black 2 in Azahar logs the fades and enters
the start menu, where both screens still render incorrectly. A hardware
retest is pending.

The linked build passes START to the game. The shell-only build still
uses START to quit.

Immediate Nitro DMA fills and copies now use host memory transfers. The
linked build also uses the existing NitroSystem model and animation
resource code instead of placeholder allocators and accessors.
Black 2 reached `Title_Init` and entered the title loop in Azahar with
zero unmapped accesses in the post-patch smoke trace. Menu input and the
hardware run still need verification.

1. **PRESS START is garbled** on sub OBJ (reads like `PPPPS SSSST`).
   Fix 2D OBJ composition / mapping / 1D char layout before chasing 3D.
2. **3D Kyurem is missing.** `g3_gecom_host.c` already captures the G3
   FIFO (`[G3] fifo words=48` on title). Still needed: FIFO replay to
   Citro3D, `NNS_G3dDraw`, host texture uploads and matrix operations,
   and `GX_SetCapture` to VRAM D (`GX_DISPMODE_VRAM_D`) so the
   captured 2D3D+VRAM actually reaches a LCD. Mine SoulSilver
   `g3packet.c` and the Platinum port **first**.
3. **Audio.** ARM7 is not decompiled. Host NDSP player; this phase is
   silent and must not call `ndspInit` until a real player exists
   (Azahar has no DSP firmware).
4. **Start menu.** Verify input and trace the broken rendering after the
   immediate DMA fix.
5. **Field**, then **battle**, then **save** (game is the only writer;
   cartridge format), then **comms**.
6. **Quality-of-life features**, each a default-off Settings toggle.
   Port ideas from the sister New 3DS repos first (see §12).

Do not start a randomizer, fast-forward, or any self-reset path.

## 9. CI

Workflows:

- `.github/workflows/compile.yml` — "Compile src/ and lib/". No ROM.
  Must pass.
- `.github/workflows/standards.yml` — "Code standards". clang-format
  **23.1.1** on C/H files changed versus the PR base under `src/`,
  `lib/`, and `include/` only (not `n3ds/`). Then `check_style.py`,
  skill-copy diffs, `apply_names.py --dry-run`, and
  `check_nonmatching_rows.py`. Must pass. `n3ds/` is not formatted by
  this job.
- `.github/workflows/build.yml` — "Build b2_us", "Build w2_us", "Build
  both ROMs on macOS". These check out the private
  `fuddlesworth/pokebw2-roms` repo with `ROMS_DEPLOY_KEY`. They fail
  with **Not Found** in this environment. That is expected. Do not
  "fix" it by adding a ROM, a key, or a public ROM source.

On head `df983ab569`, the three ROM jobs failed as expected. "Code
standards" failed on clang-format in `lib/nitro/include/nitro/os.h`
(line 539) and `lib/nitro/src/gx/gx_bgcnt.c` (the `VRAM_HOST_PTR` /
`PBW2_N3DS` wrappers). Those are shared files: format only the 3DS
branches, keep the DS `#else` identical, and re-check default shells if
you touch them. "Compile src/ and lib/" was still running at the time of
that read.

`main` exists and is the default branch. Its tip was
`95883710aa` ("Initial commit") until the user merges PR #1
(https://github.com/unowncollectables/Pokemon-Black2-White2-New3ds/pull/1).
Merge with a normal merge commit only (no squash or rebase that
rewrites, never force), and only with the user's yes.

## 10. clang-format note for successors

CI installs `clang-format==23.1.1`. If "Code standards" fails, run it
on the files it names (`clang-format -i FILE`), or guard asm, hand-laid
tables, and `GFL_ASSERT` operator lines with `// clang-format off/on`.
Do not mass-format the decomp. A `check_style.py --diff` against `main`
can choke on binary paths in the huge first-PR diff; if you hit a
UTF-8 decode error, limit the diff to text C/H files rather than
rewriting matching sources.

## 11. Builder: B2W2 target (needed, not in this repo)

This repo must not produce a CIA. The user has their own Builder. That
Builder still needs a **B2W2 target** that:

1. Builds the New 3DS `.3dsx` / ELF for the chosen game (Black 2 or
   White 2) from this tree, with the same `PBW2_N3DS` / `PBW2_LINK_GAME`
   rules as `scripts/n3ds_build.sh` / `make -C n3ds`.
2. Stages overlays and NitroFS **data** onto the SD layout
   (`sdmc:/3ds/black2_3ds/` or `white2_3ds/`): at least `nitrofs/` and,
   for LINK_GAME, `overlays/arm9_ovt.bin` plus the overlays the boot
   path loads (12, 27, 264, and later field/battle overlays as those
   phases come online).
3. Does not invent a `.sav`, does not embed a ROM in the installer, and
   does not commit or upload dumps, saves, or logs.
4. Keeps Black 2 and White 2 as two outputs of the same source tree,
   with the title IDs above and New 3DS clock / L2 / core 2 flags.

Coordinate the target's layout with the sister-port Builders so the
user is not maintaining three different SD conventions without a
reason.

## 12. Sister-port coordination

Quality features and 3D/2D host code should be mined from the New 3DS
sister ports **first**, then adapted:

- `unowncollectables/3ds-platinum` — `widecomp.c` (already the parent of
  `ds2d_compose.c`); G3 FIFO → GPU; movie overlays; followers as an
  idea only (Gen 5 data is different).
- `unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds` at `4d4d2cb`
  — Citro3D setup, `g3packet.c`, geometry queue, texture upload from DS
  VRAM keys, present integration, thread and save-file lessons.

Jules / estyseesghosts **kohjoh / sinjoh** are Old-3DS-only and
**secondary**. Do not design this port around them.

Frigate (`CallanCampos/Frigate`) is a PC port of this same decomp.
Useful as a map of which SDK modules a host has to replace
(`port/hal`). Its code is not vendored here.

Other agents may be working on those sister repos. Prefer reuse over a
new shim. When you copy GPL code, add the header and a
`n3ds/THIRD_PARTY.md` line in the same commit. Keep Black 2 and White 2
on one tree; a change that compiles only one of them is not done.

## 13. Extra technical notes (still accurate)

### Title-link closure

`python3 scripts/n3ds_link_closure.py` builds a boot/title
`game_objects.rsp` (~259 objects; includes `lib/nnsys/src/fnd/*`;
wifi/field/app trees excluded; `lib/nitro/src/spi/pm.c` and
`lib/nitro/src/pxi/*` excluded — host `pm_host.c` / `pxi_host.c`
instead). Stage overlays at `sdmc:/3ds/<game>_3ds/overlays/`. After
`SystemInit` expect `GFLAppInit enter`, working Fnd heaps (no
`0x02000000` writes from `OS_CreateExtraHeap`), then `GameInit enter`,
`GFL_OvlLoad id=12/27 ok` when overlays are staged. The shell stops
TitlePic after `pbw2_game_system_init_done`.

`HW_NVRAM_USER_INFO` is host `PbW2_NvramUserInfo` with DS-default touch
calibration. `pxi_service.c` completes CARD backup commands against an
in-memory shadow (0xFF reads, no `.sav` file).

Experimental `OS_Init` on New 3DS (`PBW2_LINK_GAME` shell `OS_Init`):
`PbW2_OsThreadHostInit`, `PXI_Init`, `PbW2_RomHeaderHostInit`,
`PbW2_DtcmHostInit`, `PbW2_NvramHostInit`, `CARD_Init` (resource lock,
PXI FS callback, **no** CARD worker thread).

### What is still stubbed

- Sound. NDSP is not started. No ARM7 player, no music.
- 3D replay. FIFO is captured. `NNS_G3dDraw`, host texture uploads,
  matrix operations, and `GX_SetCapture` to VRAM D still need work.
- DSi/TWL and the two DSP firmware images (`rom/dsp/*.inc`, ROM-backed,
  not in git).
- Settings. The screen exists so the next feature has a place. No
  `config.txt` yet.
- A large set of later overlays (field, battle, comms). 344 overlays
  should eventually be linked statically or loaded from a native
  overlay table, not left as DS overlay files the ARM9 swaps by hand.

Library compile failures (see gitignored `build/n3ds/compile-report.txt`)
are mostly MWCC `asm { }` blocks, a few `SDK_VERSION` macros GCC parses
differently, `lib/nitro/src/mb/mb_wm.c`, and the missing DSP images.

Two game files have a `PBW2_N3DS` branch so GCC can compile them:

- `src/gfl/arc_tool.c`: the one MWCC asm function is C for the 3DS
  build. The seeks match the assembly.
- `src/field/script/trainer_script.c`: GCC rejects a file-scope array
  sized by a `const int`. The 3DS build uses a macro of the same value.

### Compositor logs

Useful Azahar / host lines when debugging 2D: `vram upload BG2/3`,
`vramcnt on/+60`, `mode5-on/+60`. Mode-5 16bpp DC bitmap: `BGxCNT` bit 7
set = bitmap, bit 2 set = 16bpp direct (bit 15 opaque). Zero RGB is
treated as transparent (not bit 15) in the compose path. Affine sampling
uses BGx PA–PD and BGxX/Y (20.8); PA/PD default `0x100`.

CDEMO `boot_proc` queues overlay 264 twice (GF logo `demo=0`, then
opening `demo=2`). An immediate return usually means the script hit
`CDEMO_CMD_END`, not a missing overlay — check
`[overlay] GFL_OvlUnload id=264` spacing and `[GX] disp trace` for VRAM
growth during `CDemo_LoadMovieFrame`.

### Maintainer pointers

These name machine paths and public sources. They do not contain dumps,
saves, or keys.

- [docs/research/RESEARCH.md](research/RESEARCH.md)
- [docs/research/PLAN-initial.md](research/PLAN-initial.md)
- [docs/research/BOX-HANDOFF.md](research/BOX-HANDOFF.md)
- [tools/box/build-matching.sh](../tools/box/build-matching.sh)
- [tools/box/azahar-smoke.sh](../tools/box/azahar-smoke.sh)
- [tools/box/patches/pokebw2-345b790-delinks-rename.patch](../tools/box/patches/pokebw2-345b790-delinks-rename.patch)
  — the 66 delink renames, already applied in this tree

`scripts/check_delinks.py` reports 0 missing. Both arm9 `delinks.txt`
files name real sources (66 paths each under `src/system/savedata/`,
`src/system/iss/`, `src/pml/`).
