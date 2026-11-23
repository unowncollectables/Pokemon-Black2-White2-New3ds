# Handoff

Rules and the file map: [AGENTS.md](../AGENTS.md). Phases: [PLAN.md](PLAN.md).
Build commands: [BUILDING.md](BUILDING.md).

## State

The decomp is pokebw2 `345b790`, merged with history. Both arm9 `delinks.txt`
files name real sources again (66 paths each: `src/system/savedata/`,
`src/system/iss/`, `src/pml/`). `scripts/check_delinks.py` reports 0 missing.

The New 3DS shell links for both games:

- `n3ds/build/b2/black2_3ds.elf` / `.3dsx` (title id `000400000F7D4200`)
- `n3ds/build/w2/white2_3ds.elf` / `.3dsx` (title id `000400000F7D4300`)

Those binaries are gitignored. A devkitPro machine builds them with
`bash scripts/n3ds_build.sh`. This environment could not download devkitPro
(`pkg.devkitpro.org` returned 403), so the link used Ubuntu's
`arm-none-eabi-gcc`, libctru and devkitARM's C runtime built from their
sources, with `PBW2_LOCAL_SYSBASE=1`. A normal devkitARM install does not set
that.

Game C, both versions, no ROM: **1218/1218**, with
`-Wno-incompatible-pointer-types`. Library C depends on the compiler:
Arm GCC 14.2 counted **514/572**, and devkitARM 16.1.0 (GCC 16) counted
**516/572**. The gap is `__memcpy`. `scripts/n3ds_game_objects.py` writes
`build/n3ds/pbw2_build_status.h`, which is gitignored. A box build does not
dirty the tree. The shell prints those counts when the header was on the
include path, and `-1` when it was not.

Azahar on `b8cf620` launched both shells, with and without NitroFS staged,
and wrote no `.sav`. The logo draw in this step has not been run in Azahar.
A host decode of `res/graphics/title` (the same NCGR/NSCR/NCLR layout as
archive `a/0/2/6`) produced the Black 2 logo on the blue title background
and the White 2 logo on its background.

## What the box should run

```sh
git pull
bash scripts/n3ds_build.sh
# n3ds/build/b2/black2_3ds.3dsx and n3ds/build/w2/white2_3ds.3dsx

# after the shell is on the card, from the extract you already have:
bash scripts/n3ds_stage_nitrofs.sh b2 /path/to/extract/b2_us/files
bash scripts/n3ds_stage_nitrofs.sh w2 /path/to/extract/w2_us/files
# copy ~/pbw2-sd/3ds onto the SD card
```

`git status` after `n3ds_build.sh` should be clean. The count header is
`build/n3ds/pbw2_build_status.h`. After pulling a title-picture fix, run
`python3 tools/box/present-landscape-test.py` if you have a local
`/tmp/title_top.rgb565` (optional); Azahar should show the logo centered at
(72, 24) on top and (32, 24) on bottom.

Expect, for about 20 seconds each, no crash and no `.sav`. Separate SD
directories. Headless steps for this machine are in
[tools/box/azahar-smoke.sh](../tools/box/azahar-smoke.sh): set `NITROFS_SRC`
to the staged `nitrofs` directory and `SD_SUBDIR` to `3ds/black2_3ds` or
`3ds/white2_3ds`.

- log: `sdmc already mounted` (or `sdmc mounted`), not `sdmc not mounted`
- log: `silent (no ARM7 player this milestone)`, and no `ndspInit failed`
- log: `nitrofs a/0/0/2 open`
- log: `title pic: top is the logo over the title background; bottom is the lower title screen`
- top screen: the version logo (Pokémon Black 2 or White 2) over that
  version's title background, in the 256×192 area centered at **(72, 24)** in
  the 400×240 landscape picture. Side bars are the wider 3DS panel.
- bottom screen: the same archive's lower title graphic, centered at **(32, 24)**
  in 320×240 (the Game Freak line on the source art). `Present_DrawTitle` uses
  GSP dimensions from `gfxGetFramebuffer`, not hard-coded widths.
- without NitroFS staged: the text shell, and no title-pic line

`src/system/main.c` is still not linked. This is not the 3D title scene, and
the background does not scroll yet. Overlays 12 and 27 stay out until a logo
is actually on screen.

## What the shell does

- `osSetSpeedupEnable(true)` after `gfxInitDefault`. On a CIA, the RSF's
  `CpuSpeed: 804MHz` and `EnableL2Cache: true` are what actually leave 268 MHz.
  `CanAccessCore2: true` is set. `CoreVersion: 2`.
- Audio thread and IO thread on core 2 (core 0 if core 2 is refused). Both are
  joined before `gfxExit`. NDSP is not started: this milestone has no ARM7
  player, and Azahar has no DSP firmware (`ndspInit` returned `D880A7FA`).
- C-stick through IRRST, ZL/ZR through HID. START exits. L+R+Y opens an empty
  settings screen. Nothing is written.
- If a `.sav` is already on the card, the log records its size and leaves it.
  `Save_GameWrite` refuses and, because nothing calls it, the linker drops it.
- Present commits one screen-flag word per frame. `[GX]` logs it once. A later
  sharp-text layer has to use that word.

## What is stubbed

- Sound. NDSP is not started. No ARM7 player, no music.
- GPU. `GX_Init` clears the host IO buffer. The logo is not read from those
  registers. `title_pic.c` decodes NitroFS `a/0/2/6` and the shell blits it.
  `GxShim_Present` still only records screen flags. The 3D engine is not
  running.
- Files. `FS_OpenFile` reads `sdmc:/3ds/<game>_3ds/nitrofs/<path>` once that
  tree is staged. The IO thread still does not touch the save.
- DSi/TWL and the two DSP firmware images. Those library files do not compile
  here. The DSP images are the ROM-backed headers `rom/dsp/*.inc`.
- Overlays. All 344 stay out of the shell. `GameInit` in `src/system/main.c`
  loads overlays 12 and 27 before it queues the title process. They are not
  linked.
- Settings. The screen exists so the next feature has a place. No keys, no
  `config.txt`.

## What does not compile, and why

`build/n3ds/compile-report.txt` (gitignored) has the lines. The 55 library
failures are the same for both games:

- MWCC `asm { }` blocks in Nitro OS, FX, GX, MI, crypto, and DS Protect.
- A handful of `SDK_VERSION` style macros GCC parses differently
  (`expected ')' before string constant`).
- `lib/nitro/src/mb/mb_wm.c`, a `u16` array initialized from a string.
- `rom/dsp/dsp_g711_image.inc` and `rom/dsp/dsp_graphics_image.inc`, which
  `configure.py` extracts from the base ROM. They are not in git.

Two game files needed a `PBW2_N3DS` branch so GCC could compile them. The
matching build does not define that macro.

- `src/gfl/arc_tool.c`: the one MWCC asm function is C for the 3DS build. The
  seeks match the assembly.
- `src/field/script/trainer_script.c`: GCC rejects a file-scope array sized by
  a `const int`. The 3DS build uses a macro of the same value.

## What blocks the title process

`src/system/main.c` `main` calls `SystemInit`, enables GX interrupts, then
`GameInit`. `GameInit` loads overlays 12 and 27 and queues `EVENT_TITLE_FUNCS`.
That is the title path. It is not in the `.3dsx`.

The picture on screen is archive `a/0/2/6` drawn by the shell: sub BG 2
(member 3, the title background), sub BG 3 over it (members 0, 1 and 2, the
version logo), and main BG 3 on the bottom screen (members 9, 10 and 8).
Tile 0 and palette index 0 on the logo layer are holes, so the background
shows through. The 3D scene, the logo sprite, and the scroll are still the
title process's job.

Link `Pbw2_GameMain` only after the box has seen that logo. The calls still
go through Nitro OS, GX, and the overlay loader. Much of that C compiles,
and then touches DS registers or is still MWCC assembly. Register writes
from game C land in `pbw2_io`. DSi/TWL stays stubbed. Sound stays silent.

## Recommended next steps

1. On the devkitARM 16 machine, `bash scripts/n3ds_build.sh`. Confirm the
   tree is clean afterward. Open `black2_3ds.3dsx` in Azahar with NitroFS
   staged and confirm the logo over the title background, the lower screen,
   no crash, and no `.sav`. Then White 2, in its own directory.
2. Experimental title link: after `game-objects`, `python3
   scripts/n3ds_link_closure.py` builds a boot/title `game_objects.rsp` (~253
   objects; wifi/field/app trees excluded; `lib/nitro/src/spi/pm.c` and
   `lib/nitro/src/pxi/*` excluded — host `pm_host.c` / `pxi_host.c` instead).
   `PBW2_LINK_GAME=1 make -C n3ds b2 w2` links `Pbw2_GameMain` on a worker
   thread. Stage overlays on SD only:
   `sdmc:/3ds/<game>_3ds/overlays/arm9_ovt.bin`, `ov012.bin`, `ov027.bin`.
   After `SystemInit` the game should pass `PM_GetAmp` (no PXI wait); the
   shell stops drawing `TitlePic` once `pbw2_game_system_init_done` runs.
   Stub hits go to `log.txt` via `Log_Line` (`PBW2_STUB_LOG`). Default
   `n3ds_build.sh` does not set `PBW2_LINK_GAME`.
3. Do not write the save from the platform when the game starts calling
   `Save_GameWrite`. Remove the refusal in that function at the same time the
   game becomes the caller, and keep the bytes the cartridge format.
4. First settings toggles, when a feature exists, default off. Sharp text uses
   `Present_Flags()` from that frame.

## ROM step, one place

Matching rebuild only:

```sh
# orig/baserom_b2_us.nds  SHA1 e51e6dfb8678a3d19dcd2a10691b96a569ca0abb
# orig/baserom_w2_us.nds  SHA1 b5d7490be7b415b8f1e672a53e978a9cc667e56a
python3 configure.py && ninja
```

That is the step that reads the ROM. Header generation and the 3DS shell do
not.

## Maintainer notes

These are the box notes for this port. They name machine paths and public
sources. They do not contain dumps, saves, or keys.

- [docs/research/RESEARCH.md](research/RESEARCH.md)
- [docs/research/PLAN-initial.md](research/PLAN-initial.md)
- [docs/research/BOX-HANDOFF.md](research/BOX-HANDOFF.md)
- [tools/box/build-matching.sh](../tools/box/build-matching.sh) — matching
  rebuild on the box's pokebw2 checkout
- [tools/box/azahar-smoke.sh](../tools/box/azahar-smoke.sh) — headless New 3DS
  smoke. `NITROFS_SRC` is copied into that run's emulated SD only
- [tools/box/patches/pokebw2-345b790-delinks-rename.patch](../tools/box/patches/pokebw2-345b790-delinks-rename.patch)
  — the 66 delink renames, already applied in this tree
