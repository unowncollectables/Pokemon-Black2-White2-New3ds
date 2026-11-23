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

Game C, both versions, no ROM: **1218/1218**. That count is with Arm GCC 14.2
(the same default error on incompatible pointer types as GCC 14 and later)
and `-Wno-incompatible-pointer-types`. Library C on that compiler: **514/572**.
The maintainer's devkitARM 16.1.0 (GCC 16), after the same flag, counted
**516/572**. The gap is `__memcpy`, which GCC 14 does not declare and
devkitARM's sysroot does. `scripts/n3ds_game_objects.py` rewrites
`n3ds/include/pbw2_build_status.h` on every build, so the shell shows the
compiler that just ran.

Azahar on `bfda6ba` launched both shells and wrote no `.sav`. This step has
not been run in Azahar.

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

Expect, for about 20 seconds each, no crash and no `.sav`:

- log: `sdmc already mounted` (or `sdmc mounted`), not `sdmc not mounted`
- log: `silent (no ARM7 player this milestone)`, and no `ndspInit failed`
- log: `nitrofs not staged` until the copy above, then `nitrofs a/0/0/2 open`
- top screen: RTC from the 3DS clock, and the same NitroFS line

The logo and the title screen are still not drawn. `src/system/main.c` is not
linked.

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
- GPU. `GX_Init` clears the host IO buffer. Nothing is drawn from it yet.
  `GxShim_Present` still only records screen flags.
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

## What blocks the logo and the title screen

`src/system/main.c` `main` calls `SystemInit`, enables GX interrupts, then
`GameInit`. `GameInit` loads overlays 12 and 27 and queues `EVENT_TITLE_FUNCS`.
That is the title path. It is not in the `.3dsx`.

Linking it today would not show a logo. The calls go through Nitro OS, GX,
the filesystem, and the overlay loader. Much of that C compiles, and then
touches DS registers (`0x04000000` and the rest) or is still MWCC assembly.
The 3DS process does not have those registers. The missing piece is a Nitro
compatibility layer: OS, memory, GX/G3, the ROM filesystem, pad, RTC. Frigate
has that layer for a PC host and already boots this decomp through the title
screen into the field. The SoulSilver and Platinum 3DS renderers speak a
different API, so they are the source of the runtime rules above, not a
drop-in GX.

Until that layer exists, the honest boot is the shell you can read: which
game, whether the console is a New 3DS, and the compile counts.

## Recommended next steps

1. On a devkitPro machine, `bash scripts/n3ds_build.sh` and open
   `black2_3ds.3dsx` in Azahar. Confirm both screens, ZL/ZR, the C-stick, START
   leaving, and that no `.sav` appears. Then the same for White 2, and that it
   does not open Black 2's directory.
2. Replace the Nitro modules the title path touches, in this order: OS and
   IRQ (so `main`'s loop can wait a frame), pad, archive/FS backed by the
   user's ROM in memory, then GX 2D enough for the logo and the title
   background. Keep DSi entry points stubbed. Keep sound silent.
3. Link `src/system/main.c` as `Pbw2_GameMain` (the shell already owns `main`)
   only after those calls return instead of writing DS MMIO. Static-link
   overlays 12 and 27 with it, then the rest of the 344.
4. Do not write the save from the platform when the game starts calling
   `Save_GameWrite`. Remove the refusal in that function at the same time the
   game becomes the caller, and keep the bytes the cartridge format.
5. First settings toggles, when a feature exists, default off. Sharp text uses
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
