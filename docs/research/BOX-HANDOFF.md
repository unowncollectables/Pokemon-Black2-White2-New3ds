# Black 2 / White 2 New 3DS port — handoff

Last updated Monday, 23 November 2026, 00:22 EST. Phase 1 matching rebuild completed the same night as the good-dump copy (t5s2).

## Where things are

| Path | What it is |
| --- | --- |
| `/workspace/b2w2-3ds/docs/RESEARCH.md` | Survey, reuse map, prior art, verified numbers, leftover unknowns, dump diagnosis |
| `/workspace/b2w2-3ds/docs/PLAN.md` | Phased plan for **both** games, New 3DS-only, comms as core |
| `/workspace/b2w2-3ds/docs/HANDOFF.md` | This file |
| `/workspace/b2w2-3ds/baserom-private/` | Private good US/EU dumps (`baserom_b2_us.nds`, `baserom_w2_us.nds`). Gitignored; never push. |
| `/workspace/b2w2-3ds/pokebw2` | Local clone of fuddlesworth/pokebw2, pinned at `345b790`. Working tree has a local delinks path fix only. |
| `/workspace/b2w2-3ds/patches/pokebw2-345b790-delinks-rename.patch` | That local fix, applied by the build script when stale paths are still present |
| `/workspace/b2w2-3ds/scripts/build-matching.sh` | Reproducible matching rebuild (both games, SHA1 check) |
| `/workspace/b2w2-3ds/logs/build-matching.log` | configure/ninja log from the first matching run |
| `/workspace/soulsilver3ds` | Local HGSS 3DS tree (older than GitHub `4d4d2cb`; HEAD was `10a6cf3` on 2026-11-02). Prefer GitHub `unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds` at `4d4d2cb85f8d26617c47802227dfff19ffc37cc9` (2026-11-22 22:02 EST) for HANDOFF / AGENTS / PR #582. At that commit the files are root `HANDOFF.md`, root `AGENTS.md`, `docs/CHANGELOG.md`, `docs/THIRD_PARTY.md` — there is no `docs/HANDOFF.md`. |
| `/workspace/soulsilver-ref` | Additional SS reference copy |
| `/workspace/platinum3ds` | Local Platinum 3DS tree (`unowncollectables/3ds-platinum`), origin of shared `platform/` |
| `/workspace/devkitpro` | Shared box toolchain (`source /workspace/devkitpro/env.sh`) — not required for the NDS matching rebuild |
| `/workspace/tools/estyseesghosts/azahar/build/bin/Release/azahar` | Box Azahar build, for later oracle tests — not used this pass |

## Current state

- Feasibility: **yes**. A matching decomp exists, the owner's good US/EU dumps are on the box, and both games **rebuild byte-for-byte** at the pinned commit (after one local config fix).
- Best game-C base: [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2) (GPL-3.0). Pinned checkout: `345b790e6000869b481b79838bb5727309f832fb`, 2026-11-20 23:17:32 -06:00 (2026-11-21 00:17 EST), message “Normalize the remaining names: fieldmap, pml and the savedata headers”. ByteMatch page (fetched 2026-11-22): 100% linked/matched; 3,867,578 / 3,867,578 code bytes; 1,824,396 / 1,824,396 data bytes; 43,418 / 43,418 functions; 1,792 units; progress commit `1a66adc`. JSON at `bytematch.dev/fuddlesworth/pokebw2.json` returned an empty body here; figures are from the HTML page.
- Matching rebuild (23 November 2026, EST): **PASS** for both games. See “Phase 1 matching rebuild” below.
- Best 3DS runtime to reuse: this owner's HGSS and Platinum `platform/` (G3, compose, NDSP, UDS/WFC, settings, sharp text, hunt, logring). Lessons at `4d4d2cb`: join workers at exit (~32-thread limit), sharp-text synced flags, features default off, never write `.sav` outside the game.
- Best native-port reference: [CallanCampos/Frigate](https://github.com/CallanCampos/Frigate) (PC/Mac, last push 2026-11-04, may lag pokebw2). [Project-Holon/eidos](https://github.com/Project-Holon/eidos) is the sister **Black/White** PC port (main story playable, credits milestone unchecked). [somefosterkid/ntrnative](https://github.com/somefosterkid/ntrnative) is secondary; prefer its `docs/black2-white2.md` over the root README.
- No public native **3DS** B2/W2 port was found. There is no pret `pokeblack2` / `pokewhite2`.
- ARM7 is not decompiled (stock TwlSDK 5.3). Sound is a host player + NDSP job.
- Communication plan is in `PLAN.md` Phase 7, not optional.
- No project git remote was created, and nothing was pushed.

## Phase 1 matching rebuild (t5s2, 23 November 2026, EST)

### Step A — copy (READ-ONLY on <owner-pc>)

Good dumps on <owner-pc> (`<machine-id>`) were not modified, renamed, or moved. No ROM was downloaded.

| <owner-pc> original (untouched) | LastWriteTime (<owner-pc> local) | Size |
| --- | --- | --- |
| `<rom-dir>\Pokemon - Black Version 2 (USA, Europe) (NDSi Enhanced).nds` | 2026-11-23 00:08:40 | 536,870,912 (`0x20000000`) |
| `<rom-dir>\Pokemon - White Version 2 (USA, Europe) (NDSi Enhanced).nds` | 2026-11-23 00:08:56 | 536,870,912 (`0x20000000`) |

Copy path: split to 90 MiB chunks `b2.00`–`b2.05` and `w2.00`–`w2.05` under `<temp-dir>\b2w2-split`, `CopyToBox` one chunk at a time into `/workspace/b2w2-3ds/baserom-private/`, reassemble as `baserom_b2_us.nds` / `baserom_w2_us.nds` (the names pokebw2 expects). Box chunk files were deleted after reassembly. The <owner-pc> temp folder was deleted and reconfirmed gone (`Test-Path` = False). Originals still present with the same sizes and LastWriteTimes.

Box SHA1s (verified with `sha1sum`):

| File | SHA1 | Result |
| --- | --- | --- |
| `baserom-private/baserom_b2_us.nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` | MATCH (pokebw2 / expected good B2) |
| `baserom-private/baserom_w2_us.nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` | MATCH (pokebw2 / expected good W2) |

Hardlinked into `pokebw2/orig/baserom_b2_us.nds` and `pokebw2/orig/baserom_w2_us.nds` (same inodes). `/workspace/b2w2-3ds/.gitignore` contains `baserom-private/`. pokebw2 already gitignores `orig/*.nds`. Keep ROMs only under `/workspace`.

The earlier files under `<rom-dir>\Roms\` remain unusable `[b]` / junk-filled dumps; see `RESEARCH.md` §9. They were not copied again.

### Step B — matching rebuild

- Clone: `git clone --depth 1 https://github.com/fuddlesworth/pokebw2.git` → `/workspace/b2w2-3ds/pokebw2`. HEAD `345b790e6000869b481b79838bb5727309f832fb`. The clone has a default `origin` remote; it was not used to push.
- Reproducible script: `/workspace/b2w2-3ds/scripts/build-matching.sh`.

**Host tools**

Prefer keeping tools under `/workspace`. These already lived in `/usr` or had to be installed there:

| Tool | Where | Notes |
| --- | --- | --- |
| Python 3.13.5 | `/usr/bin/python3` | Already present |
| ninja 1.12.1 | `/usr/bin/ninja` | Already present |
| Debian clang 19.1.7 (3+b1) | `/usr/bin/clang` | Installed via apt; must live in `/usr` |
| llvm-objcopy (GNU-objcopy compatible) | `/usr/bin/llvm-objcopy` | Installed via apt with clang/llvm; must live in `/usr` |

`configure.py` downloaded the rest into `pokebw2/tools/`: wibo `pokebw2-sjis-1`, sjiswrap `v1.2.2-pokebw2.1`, objdiff-cli `v3.8.1`, mwccarm, dsd `v0.12.1-dsi.10`. No Wine package was required; wibo is the MWCC wrapper.

**First ninja attempt (failed)**

`python3 configure.py b2_us w2_us` succeeded, then:

```
ninja: error: 'build/b2_us/src/system/save_table.o', needed by 'build/b2_us/arm9.o', missing and no known rule to make it
```

START 2026-11-23 00:16:48 EST, END 00:17:34 EST. No output ROMs. Cause: commit `345b790` moved savedata, iss, and pml sources but left 66 stale object/source paths in each of `config/b2_us/arm9/delinks.txt` and `config/w2_us/arm9/delinks.txt`.

**One honest local fix**

Updated those 66 headers in each delinks file to the post-rename paths, only when the new file exists and the old path does not:

- `src/system/<savedata unit>.c` → `src/system/savedata/<savedata unit>.c`
- `src/system/iss_*.c` → `src/system/iss/iss_*.c`
- `src/system/pml_hm_check.c` → `src/pml/hm_check.c`
- `src/system/pml_item.c` → `src/pml/item.c`
- `src/system/pml_met_data.c` → `src/pml/met_data.c`
- `src/system/pml_species_names.c` → `src/pml/species_names.c`

After the fix, 0 delinks headers pointed at missing sources. Patch saved at `patches/pokebw2-345b790-delinks-rename.patch`. The build script applies it when `src/system/save_table.c:` is still present. The pokebw2 working tree is dirty for those two files only; no commit was made and nothing was pushed.

**Successful rebuild**

Re-ran `python3 configure.py b2_us w2_us` then `ninja -j8` (8 cores). 14,039 ninja steps. Log: `logs/build-matching.log`.

| | |
| --- | --- |
| START | Monday, 23 November 2026, 00:19:51 EST |
| END | Monday, 23 November 2026, 00:22:22 EST |
| Wall time | **135 seconds** |
| ninja exit | 0 |

| Output | SHA1 | vs baserom |
| --- | --- | --- |
| `pokebw2/build/pokeblack2_us.nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` | MATCH, `cmp` byte-identical |
| `pokebw2/build/pokewhite2_us.nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` | MATCH, `cmp` byte-identical |

Both outputs are 536,870,912 bytes. ninja's own “Checking build/pokeblack2_us.nds” / “Checking build/pokewhite2_us.nds” steps also passed. Independent `sha1sum` + `cmp` confirmed the same result.

**Blockers:** none remaining for the Phase 1 byte-for-byte check. The only issue was the stale `345b790` delinks paths, now patched locally. If upstream fixes those files, the script's `git apply` is a no-op.

## Earlier user dumps (unusable) — 22 November 2026

The first pair, under `<rom-dir>\Roms\`, was inspected read-only and is **not** the baserom set. Details are in `RESEARCH.md` §9.

| File | SHA1 | Verdict |
| --- | --- | --- |
| `<rom-dir>\Roms\Pokemon Black Version 2 .nds` | `42f7a3daf0601064c9067a6a2903a83ec081442b` | No-Intro/TASVideos **[b]**; TWL area all `0xFF`. Not repairable. |
| `<rom-dir>\Roms\pokwhite2uspatched.nds` | `eeba7e06370dee9c80a9695d92e216db4a7a5b3b` | TWL area is a repeating 4096-byte pattern. Not repairable. |

## What a later builder should do first

1. Read `RESEARCH.md` and `PLAN.md` in full, then HGSS `HANDOFF.md` / `AGENTS.md` / `docs/WIRELESS.md` at `4d4d2cb`, and Frigate `port/README` plus `port/docs/research/assets-arm7-sound.md`.
2. The matching dumps are already on the box and have passed the rebuild check. To repeat it: `/workspace/b2w2-3ds/scripts/build-matching.sh`. Stop if an output SHA1 mismatches.
3. Start the rest of Phase 0–1 in `PLAN.md`: copy `platform/`, New 3DS exclusive CIA with **804 MHz + L2 actually enabled**, joinable threads, ROM cache, ARM7 stubs. Keep White 2 compiling from the first ninja/make.
4. Do not begin by writing a new renderer, a static recompiler, or a Pretendo integration (wrong network layer).
5. Do not download ROMs, do not commit ROMs/saves/binaries, do not push a remote unless the owner asks. Do not modify the <owner-pc> originals.

## User needs (for the parent to ask, if not already on file)

- Good US/EU dumps are on file and hash-verified (above). Region is the pokebw2 USA/Europe SHA1 pair.
- A New 3DS-family console for hardware tests (Azahar is not a substitute for UDS, IR, clock flags, or C-Stick/`ir:rst`).
- For Memory Link: a Black or White save the user owns.
- For Dream Radar: whether they care about official legendary transfers; if yes, they will need a documented import path, not the official cart slot.
- Whether a later private git remote is wanted. None exists yet.

## Open issues to re-check at implementation time

Listed in `RESEARCH.md` §6. The ones that will bite first:

- Userland FCRAM budget on a New 3DS APPLICATION CIA (physical 256 MB is verified; the process budget is not).
- Official Dream Radar vs a CIA SD `.sav` (expected not to work; untested).
- 3DS IR to a real DS C-Gear / Pokewalker (unknown).
- PCN / Kaeru / Wiimmfi liveness at the moment of testing (front page was fetched 2026-11-22; no client was pointed at the DNS here).
- ByteMatch 43,418 vs `code-organization.md` 41,423 function counts.
- ntrnative "to the credits" is unverified; use playtests.
- The `345b790` delinks typo may still be present upstream; keep the local patch until it is not.

## Prior-art highlights (verified in the research pass)

- pokebw2 is the only matching B2W2 decomp. `nds197/pokeblack2` is a WIP disassembly. `PlatinumMaster/pokewhite2` 404s; `kleenxfeu/pokewhite2` holds the 2022 PlatinumMaster commits and a README that says it does not build.
- aletheia (BW) is 100% / 35,197 functions (README fetched). squiddonaut/pokeblack is 0.03% matched code on decomp.dev (commit `67d91a78`, 2026-10-17).
- eidos: main story playable, credits milestone unchecked; not 3DS.
- PCN DNS `178.62.43.212`. entralinked covers Game Sync / Join Avenue / Memory Link. dreamworld-reawakened is self-hosted, with a documented broken-feature list; README filename is `readme.md`.
- Dream Radar block: 152 bytes at `0x7F000` (BlackShark, Project Pokémon, 2017-03-19).
- Bond697 2013 IDB: "3500 of gamefreak's functions hand-labelled" (thread fetched); historical only.
- Static recomp exists for BW in gen4recomp (`ndsrec`/`armrec`); not the B2W2 plan.

## PR #1 smoke (t10u, 23 November 2026, 00:51–00:58 EST)

Working copy: `/workspace/b2w2-3ds/port` at `bfda6ba4171b936bdb563a1ef507ce811212be93` (`cursor/b2w2-n3ds-port-5254`). No commits or pushes. Baseroms hardlinked into `orig/` and gitignored (`git status` clean of ROMs).

Matching rebuild: PASS. `build/pokeblack2_us.nds` / `build/pokewhite2_us.nds` SHA1s match and are byte-identical to the baseroms (00:52:18–00:55:18 EST).

`bash scripts/n3ds_build.sh` (after `source /workspace/devkitpro/env.sh`, devkitARM 16.1.0): first run failed because 9 game files died on `-Wincompatible-pointer-types` as errors (1209/1218). One local fix: `-Wno-incompatible-pointer-types` in `scripts/n3ds_game_objects.py`. Retry 00:56:42–00:57:03 EST, exit 0. Game C 1218/1218 both; library 516/572 (repo header still said 517). Shells: `n3ds/build/b2/black2_3ds.3dsx` and `n3ds/build/w2/white2_3ds.3dsx` (143,784 bytes each). Dirty only those two files; not committed.

RSF: both `n3ds/cia/black2.rsf` and `white2.rsf` set `CpuSpeed: 804MHz`, `EnableL2Cache: true`, `CoreVersion: 2`, `CanAccessCore2: true`, `SystemModeExt: 124MB`.

Azahar New 3DS smoke (~20s each, alive at end, no crash, no `.sav`):

- `/workspace/b2w2-3ds/smoke/b2/t020.00.full.png` — Black 2 platform shell (not logo/title). New 3DS: yes. Title ID `000400000F7D4200`.
- `/workspace/b2w2-3ds/smoke/w2/t020.00.full.png` — White 2, same shell, title ID `000400000F7D4300`, its own `sdmc:/3ds/white2_3ds`.
- Logs: `smoke/*/log.txt`. `archiveMountSdmc` logged "sdmc not mounted" but `log.txt` was still written. `ndspInit` failed `D880A7FA` (silent, planned). Azahar does not implement `ConfigureNew3DSCPU`.

Extract (gitignored): `extract/{b2,w2}_us/` (~279M). NitroFS `files/` 272M (318 files, including `swan_sound_data.sdat` 89.4M). 344 overlays, `ov012.bin` 123,520 bytes, `ov027.bin` 3,776 bytes. DSP images generated at **build time** as `build/include/generated/rom/dsp/*.inc` from `extract/*/dsi/ltd_autoload_0.bin`. The 3dsx does not yet read any of this at runtime.

## PR #1 round 2 smoke (t10u, 23 November 2026, 01:10–01:16 EST)

Pulled `b8cf620a1a903159656189274e5fb0b6e26e5412` (`Host the Nitro OS and filesystem, and fix the Azahar shell log.`). Local compile-flag edit discarded first (it is upstream). No pushes. Only local dirty file after the 3DS build: `n3ds/include/pbw2_build_status.h` (committed 514/572, this machine regenerated 516/572). Not committed.

Matching `ninja` 01:11:04–01:13:14 EST, 11,688 steps, exit 0. Both outputs still byte-identical to the baseroms:
- `build/pokeblack2_us.nds` `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb`
- `build/pokewhite2_us.nds` `b5d7490be7b415b8f1e672a53e978a9cc667e56a`

The `PBW2_N3DS` register macros did not affect the matching rebuild.

`bash scripts/n3ds_build.sh` 01:13:38–01:13:58 EST, exit 0, no gcc warnings. Game C 1218/1218 both; library 516/572. Shells: `n3ds/build/{b2,w2}/*_3ds.3dsx` (141,332 bytes each).

Staged NitroFS with `scripts/n3ds_stage_nitrofs.sh` to `/workspace/b2w2-3ds/sd-stage` (outside the git repo, not a user SD card): 318 files per game, probe `a/0/0/2` 5,349,632 bytes. Then hardlinked into Azahar's virtual SD only.

Azahar New 3DS smoke, ~20s each, all four alive at end, no crash, no `.sav`, separate SD dirs:

| Run | Log | Screenshot |
| --- | --- | --- |
| B2 before | `sdmc already mounted`; `silent (no ARM7 player this milestone)`; `nitrofs not staged` | `/workspace/b2w2-3ds/smoke/r2/b2-before/t020.00.full.png` |
| W2 before | same, White 2 dir | `/workspace/b2w2-3ds/smoke/r2/w2-before/t020.00.full.png` |
| B2 after | same plus `nitrofs a/0/0/2 open` | `/workspace/b2w2-3ds/smoke/r2/b2-after/t020.00.full.png` |
| W2 after | same plus `nitrofs a/0/0/2 open` | `/workspace/b2w2-3ds/smoke/r2/w2-after/t020.00.full.png` |

Screens checked: still the platform shell (not logo/title). After staging, the top screen shows `NitroFS: a/0/0/2 open` and an RTC line. Next: draw the logo from staged NitroFS, then link `Pbw2_GameMain` with overlays 12/27.
