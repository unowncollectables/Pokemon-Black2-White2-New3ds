# Black 2 / White 2 New 3DS port — research notes

Researched on Sunday, 22 November 2026 (EST), with a later same-day pass that re-fetched weak claims against primary pages (READMEs, GitHub API, ByteMatch HTML, Project Pokémon threads, 3dbrew, pkmnclassic.net). A later authorized pass on Monday, 23 November 2026 copied the owner's good US/EU dumps (no ROM was downloaded) and ran a matching pokebw2 rebuild; see §9.4 and `HANDOFF.md`. Numbers and claims below are taken from those fetched sources, plus that local rebuild. Where something was not independently checked here, that is stated. Say "unknown" rather than guess.

This is a personal-use native New 3DS port study. It is not affiliated with Nintendo, Game Freak, Creatures, or The Pokémon Company. Any later work must keep ROMs, saves, and extracted assets out of the tree.

## Verdict

A matching, buildable decompilation of US Black 2 and White 2 already exists: [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2) (GPL-3.0). ByteMatch reports 100% matched code and data for the default version. A local rebuild at `345b790e6000869b481b79838bb5727309f832fb` (plus a local delinks path fix described in `HANDOFF.md`) produced `build/pokeblack2_us.nds` and `build/pokewhite2_us.nds` with SHA1s identical to the owner's good dumps and to the pokebw2 README values. There is no pret `pokeblack2` / `pokewhite2` repository.

A personal-use New 3DS native port is **feasible**, and it should start from pokebw2's C plus the existing HeartGold/SoulSilver and Platinum 3DS platform layers, with [CallanCampos/Frigate](https://github.com/CallanCampos/Frigate) as the main native-port reference (sound driver, HAL, save, DSi policy). This is a large project, not a retarget of an already-running 3DS or PSP port the way SoulSilver was (that port started from pspoke). The 3DS being 32-bit ARM11 is an advantage over Frigate's 64-bit PC/Mac hosts.

Static recompilation or a pure HLE wrapper is not the first choice while this decomp exists. Waiting for pret is unnecessary.

## 1. Decompilation and disassembly landscape

### 1.1 fuddlesworth/pokebw2 (best base)

- Repo: https://github.com/fuddlesworth/pokebw2
- Description: "Decompilation of Pokémon Black 2 and White 2"
- License: GNU GPL v3.0
- Created: 2026-10-04 18:50:35 UTC (14:50 EDT)
- Default branch: `main`
- Latest commit checked: `345b790e6000869b481b79838bb5727309f832fb`, 2026-11-21 05:17:32 UTC (00:17 EST), message starts "Normalize the remaining names: fieldmap, pml and the savedata headers"
- Size / popularity at check: about 360,076 KiB, 100 stars, 39 forks, 120 open issues
- Language: C
- Not a pret project. The README says it follows pret layout, build, and conventions.

README (https://github.com/fuddlesworth/pokebw2/blob/main/README.md) calls it a work-in-progress, and also says both USA/Europe ROMs rebuild byte for byte from the same source tree. CI is described as compiling every C file for both versions; a full `ninja` build extracts the user's ROM, links with `mwldarm`, rebuilds the `.nds`, and checks SHA1. This research did **not** run that build.

Required dumps (NDSi Enhanced), filenames and SHA1s from the README:

| Game | Input file | SHA1 |
| --- | --- | --- |
| Black 2 | `orig/baserom_b2_us.nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` |
| White 2 | `orig/baserom_w2_us.nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` |

Output names: `build/pokeblack2_us.nds` and/or `build/pokewhite2_us.nds`. ROMs are not included.

The README labels those hashes "USA/Europe". Official US product codes used by the CTRMap docs are `IREO` (Black 2 USA) and `IRDO` (White 2 USA). Whether every European language dump matches those two SHA1s was **not verified here**. Frigate has a separate proposed path for the Spanish ROM (`IRES`). The user should hash their own cart dump and only use a file that matches the table above, unless a later audit adds another version.

### 1.2 Matching status

[ByteMatch](https://bytematch.dev/fuddlesworth/pokebw2) (page fetched 2026-11-22) reports, for the tracked default version:

- Headline: "100.00% decompiled" and "100.00% fully linked"
- Code: 3,867,578 of 3,867,578 bytes matched (3.87 MB)
- Data: 1,824,396 of 1,824,396 bytes matched (1.82 MB)
- Functions: 43,418 of 43,418 (100.0%)
- Units: 1,792, all fully matched
- Libraries shown as 100%: DPW, DS Protect, DWC (and "Libraries 100.00%")
- Progress commit cited on the page: `1a66adc` ("Add feature subdirectories and align include/ with the new modules"), 2026-11-21 01:41 UTC (2026-11-20 20:41 EST). Later naming/reorg commits landed the same day; matching is still reported at 100%.

JSON at `https://bytematch.dev/fuddlesworth/pokebw2.json` was requested; the response body was empty in this environment, so the HTML page is the source for the figures above.

[docs/nonmatching-functions.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/nonmatching-functions.md) says: "None. Every function is in C and matches, or is an asm function." The one documented exception is `ArchiveMoveImageTop` in `arc_tool.c` (maintainer exception: a dead store that no installed compiler reproduces from C). [docs/configs.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/configs.md) says "dsd's analysis and linking have no known gaps left in this ROM." Small zero-filled alignment / startup ranges remain as generated gap objects (table in that doc; 168 zero data bytes plus a 32-byte ITCM prefix).

[docs/code-organization.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/code-organization.md) also says White 2 has 41,423 functions, of which 41,311 are byte-identical to Black 2 apart from relocations, and 112 differ. That 41,423 figure does not match ByteMatch's 43,418. Unknown which count excludes what (libraries, ARM7-less modules, an older snapshot, etc.). Prefer ByteMatch for the current matching headline, and quote both.

Black 2 is the primary version. Shared sources use `BLACK2` / `WHITE2` defines.

The 120 open issues are not a sign of unmatched code by themselves. The README still tells contributors to claim an overlay's issue before starting work; after matching, much of the remaining work is naming and documentation.

### 1.3 Tooling

- Build: Linux, WSL, or macOS; Python 3.11+, ninja, LLVM (`clang`, `llvm-objcopy`). [docs/building.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/building.md).
- `configure.py` downloads pinned wibo, sjiswrap, objdiff, Metrowerks CodeWarrior, and dsd. Matching uses MWCC via wibo (and sjiswrap for Shift JIS).
- DSi hybrid support comes from the project's `dsi-hybrid` forks: [fuddlesworth/ds-rom](https://github.com/fuddlesworth/ds-rom/tree/dsi-hybrid) and [fuddlesworth/ds-decomp](https://github.com/fuddlesworth/ds-decomp/tree/dsi-hybrid), based on AetiasHax [ds-decomp](https://github.com/AetiasHax/ds-decomp) and [ds-rom](https://github.com/AetiasHax/ds-rom).
- Symbol names were imported from [ds-pokemon-hacking/swan](https://github.com/ds-pokemon-hacking/swan) (GPL-3.0; headers and ESDBs for B2/W2 code injection; last push 2025-07-03; swan README describes include headers for White 2 / B2W2). GitHub metadata for swan: created 2022-09-28, 1 star, language C. Top contributors named in the GitHub UI include HelloOO7 and PlatinumMaster.
- Library placement references [Project-Holon/aletheia](https://github.com/Project-Holon/aletheia), "From-scratch byte-matching decompilation of Pokemon Black and White" (created 2026-09-13, 47 stars, last push 2026-11-17). That is Black/White, not B2/W2.
- There is no `pret/pokeblack2` (GitHub reports the repo as not accessible / not present). pret's relevant published decomps remain pokeplatinum, pokeheartgold, pokediamond.

### 1.4 Overlay count, modules, symbol maps

`config/` has only `b2_us` and `w2_us`. Under `config/b2_us/` there is **only `arm9/`** (no `arm7/` config). ARM9 layout includes `delinks.txt`, `symbols.txt`, `relocs.txt`, `config.yaml`, `itcm`, `dtcm`, `autoload_2`, `autoload_3`, `ltd_autoload_0`, and `overlays/`.

The overlays tree `config/b2_us/arm9/overlays` contains **344 overlay directories, `ov000` through `ov343`, with no missing IDs** (GitHub git tree `10c81c709a4528f79702e4e20a8ad60f2c94bf6b`, not truncated, 1,376 entries).

[docs/module-map.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/module-map.md) maps overlays into logical `src/` modules (`app/`, `battle/`, `communication/`, `demo/`, `entralink/`, `field/`, `moveeffect/`, `musical/`, `pokewood/`, `save/`, `system/`, `tournament/`, `wifi/`, plus `gfl/` and `pml/`). It lists **81 unclassified empty overlays** with "no source yet" (`ov006`, `ov007`, `ov019`, `ov037–ov049`, `ov138`, `ov158–ov161`, `ov163`, `ov183`, `ov188`, `ov202`, `ov206`, `ov212`, `ov217–ov252`, `ov254`, `ov274–ov278`, `ov282`, `ov283`, `ov291`, `ov292`, `ov329`, `ov333–ov337`, `ov339–ov343`). Given 100% matching, these are empty overlay slots, not unmatched game code. Count of that list was not re-tallied item-by-item here beyond the document's "81".

Symbol maps:

- pokebw2: `config/b2_us/arm9/symbols.txt` (599,343 bytes) and the White 2 counterpart; `config/names.txt` (1,941,970 bytes).
- swan ESDBs: `IREO.yml` / `IRDO.yml` in [ds-pokemon-hacking/swan](https://github.com/ds-pokemon-hacking/swan) (not cross-compatible between B2 and W2).

### 1.5 ARM7

Not decompiled in pokebw2. Confirmed by the config tree (arm9 only) and by Frigate's research note [port/docs/research/assets-arm7-sound.md](https://github.com/CallanCampos/Frigate/blob/main/port/docs/research/assets-arm7-sound.md), which inspected extracted ARM7 binaries from a worktree (this research did not). That note's conclusions, quoted as Frigate's measurements:

- Nothing in `src/` or `lib/` is ARM7 code. The ARM9 side of ARM7 services **is** decompiled (`lib/nitro` SND/SPI/RTC/WM/NVRAM/CARD/PXI/OS, `lib/twl`, `lib/nnsys/src/snd`).
- ARM7 is identified as a stock TwlSDK 5.3 component (`sdk_version` 0x0503757C, same as ARM9). ARM7, ARM7i, and ARM9i are reported byte-identical between Black 2 and White 2.
- ARM7i is stock DSi wireless / storage code (Atheros SDIO, wpa_supplicant strings, NAND/SD FATFS paths).
- The sequencer runs on the ARM7 (NitroSDK SND LFO sine table found in `arm7.bin`, not on ARM9). A native port must re-create the **SND driver** (sequencer, 16 channels, mixer). Other ARM7 services (RTC, touch, power, mic, NVRAM, backup, wireless) are replaced at the ARM9 SDK API.

PXI tags listed as the ARM9↔ARM7 interface: NVRAM 4, RTC 5, TOUCHPANEL 6, SOUND 7, PM 8, MIC 9, WM 10, FS 11, OS 12, CTRDG 13, CARD 14, FATFS 20, CAMERA 21, NWM 22, SCFG 23, SNDEX 24.

### 1.6 DSi / TWL

Black 2 / White 2 are NDS/DSi hybrids built with the TWL-SDK. pokebw2 documents this in [docs/configs.md](https://github.com/fuddlesworth/pokebw2/blob/main/docs/configs.md) (section "DSi specifics"):

- ARM9 has no footer; autoload list uses TwlSDK's 4-word entries including `.sinit`.
- DSi region with ARM9i / ARM7i programs. First 0x4000 bytes of ARM9i are modcrypt-encrypted.
- DSi-only LTD ("limited") module is compressed inside ARM9i and loaded to `0x02700000` in DSi mode; ARM9 main calls into it. It holds TwlSDK DSi libraries, two Teak DSP COFF images, and DSi-only tails of some NitroSDK files (including `wcm_dsi.c`).
- Overlay 169 runs from VRAM at `0x06898020`.
- Migrated LTD sections: some NitroSDK files have NITRO code in ARM9 main and DSi code in the LTD autoload (one object, `#pragma section LTDMAIN`).

pokebw2 libraries include `lib/nitro`, `lib/nnsys`, `lib/twl`, `lib/nitrowifi`, `lib/dwc`, `lib/dsprot`, `lib/spl`, `lib/msl`, `lib/dpw`, `lib/irc`, `lib/poke_net`, and related DWC UI/util trees.

Frigate's same research file describes the ROM as a 512 MiB NDS/DSi hybrid, TwlSDK 5.3, with all needed NitroFS and overlays sitting below `0x112A723C` on Black 2 (about 276 MiB). NitroFS: 318 files (271 MiB), 308 NARCs, 69,850 members; only 8 NitroFS files differ between B2 and W2. These filesystem figures were not re-measured here.

Frigate's recommended native-port policy: ship with `OS_IsRunOnTwl() == FALSE` (NITRO behaviour), which turns off camera, DSP, SSP, SNDEX, NWM, and FATFS paths and keeps the NTR heap budget. Optional later "TWL features" flag for Xtransceiver-style webcam work. Do not emulate NWM; use host sockets if wireless is ever added.

DSi-mode features called out: Xtransceiver video chat (camera, Teak DSP, SSP JPEG, photo signatures), WPA Wi-Fi, larger heaps (`0x48AE00` vs `0x1A3E00` for heap 1), 5-level battery, SNDEX mic filter, beacon `isDSi` flag.

### 1.7 3D engine (G3 / NNS)

pokebw2 includes a full `lib/nnsys` (NitroSystem) and `lib/nitro` (includes G3/GX). Game field/battle/demo code sits in GFL and overlays (battle view, field map/camera, `demo3d`, Pokéstar, musicals, etc.). This research did not count G3 vs NNS G3D call sites.

A New 3DS port can reuse the Platinum/SoulSilver G3 command-buffer → PICA path (`g3gpu.c`, `g3soft.cpp`, `gpucomp.c`, `gpuscale.c`, `render3ds.cpp`) as a starting point, but Gen 5's GFL + NNS G3D usage, cameras, and UI are not the Gen 4 engine. Expect substantial Gen 5-specific work, not a drop-in.

Frigate implements host 2D, a geometry engine, and 3D (see `port/gfx/`, ADR 0016 on banding 3D work to worker threads). That is the other implementation to read, not to copy blindly onto the 3DS (different GPU, 32-thread process limit).

### 1.8 Sound (NNS SDAT)

From Frigate `assets-arm7-sound.md` (not re-measured here):

- One 89.4 MB SDAT (`swan_sound_data.sdat`) via NitroSystem's sound archive.
- 1,390 sequences (273 BGM, 1,066 SE), 1,949 banks, 1,949 wave archives, 8,714 waves (8,024 IMA-ADPCM, 690 PCM8).
- 7 players plus a stream player; 1 stream (title theme, PCM16 stereo at 32,728 Hz, 112 s).
- No SSAR sequence archives; no capture effects started.
- Needs per-track mute/volume/pitch/pan (interactive sound system, `src/system/iss/`), wave output (cries), streams, in-memory sequences/banks (musicals), and the microphone (Chatot at 8,180 Hz, plus Xtransceiver / tests).

ARM9 sound stack is decompiled (NitroSDK SND command/work/bank, NNS SND, GFL sound, ISS). The ARM7 sequencer is not. A 3DS port needs a host sequence player feeding NDSP (SoulSilver already has an NDSP output path and DSP-watchdog lessons). Frigate's `port/audio/` and ADR 0006 are the documented native player to study.

### 1.9 Save format

From the same Frigate research note:

- 512 KiB flash, `CARD_BACKUP_TYPE_FLASH_4MBITS_EX`.
- Main save is 0x26000 bytes at 0x0, with a backup copy at 0x26000.
- Each block has a CRC16-CCITT; a footer with magic `0x31053527` and a save count selects the newer copy.
- 21 extra saves occupy 0x4C000–0x7E000.
- A raw 524,288-byte host file matches melonDS, DeSmuME, and real-cart dumps (Frigate's claim).
- Import should adopt the save's stored RTC offset, or daily events reset.

pokebw2 has `src/system/savedata/` (serialization) and overlay 331 `save_outside.c`, plus `include/save/` structure headers. Those sources were not line-audited here.

Policy for a 3DS port (from the SoulSilver project, see §3): the on-disk file must stay a plain 512 KiB `.sav`, byte-compatible with a real cart / flashcart; features must never write the save unless that is their explicit purpose; test on copies only.

### 1.10 Older Gen 5 tools (not a decomp)

- [ds-pokemon-hacking/CTRMap-CE](https://github.com/ds-pokemon-hacking/CTRMap-CE): "CTRMap Community Edition, an extensible level editor for Pokémon Black/White/Black 2/White 2." Java, GPL-3.0, 33 stars, last push 2026-04-28. Docs: https://ds-pokemon-hacking.github.io/docs/generation-v/guides/bw_b2w2-using_ctrmap/ (supports IRAO/IRBO/IRDO/IREO).
- Hello007 / HelloOO7: author credit on the code-injection guide (Hello007, PlatinumMaster) and a top swan contributor (HelloOO7). CTRMap is the associated editor. A `HelloOO7/CTRMap` URL was not accessible through the GitHub App in this environment (private, renamed, or missing). This is ROM-hacking / injection work, not a matching decomp.
- Kingcom: often mentioned alongside CTRMap in community discussion. No current public Kingcom B2W2 decomp repo was verified here. **Unknown** whether a distinct Kingcom tree still exists.
- [turtleisaac/PokEditor](https://github.com/turtleisaac/PokEditor): "Multifunctional in-depth editor for Pokémon Gen 4 game data." Not a B2W2 decomp.
- [ds-pokemon-hacking/PMC](https://github.com/ds-pokemon-hacking/PMC) is referenced from the injection guide as the runtime for CTRMap patches.
- [kuroppoi/entralinked](https://github.com/kuroppoi/entralinked): standalone Game Sync emulator for Black & White **and** Black 2 & White 2 (Join Avenue visitors are B2W2-only; Memory Link against a BW save). Not a decomp. README fetched 2026-11-22: https://raw.githubusercontent.com/kuroppoi/entralinked/master/README.md

Useful for maps, scripts, and symbol names; not a substitute for pokebw2.

## 2. Existing native ports of B2/W2 (PC, not 3DS)

### 2.1 CallanCampos/Frigate (primary reference)

- https://github.com/CallanCampos/Frigate
- "a native PC and Mac port of Pokémon Black 2 and White 2, built from the pokebw2 decompilation and kept in sync with it. Bring your own ROM."
- Created 2026-10-17, GPL-3.0, 9 stars, 3 forks, 8 open issues.
- Last push: 2026-11-04 18:04:56 UTC (13:04 EST). Metadata updated 2026-11-21. The tree is pokebw2 plus a `port/` directory; it may lag upstream pokebw2 (upstream last push 2026-11-21).
- Status from [port/README.md](https://github.com/CallanCampos/Frigate/blob/main/port/README.md): boots natively through logos, opening, title, start menu, with sound, into the field. A new game goes through the intro into Aspertia City; the player can walk, talk, and enter/exit a house (phase 3 exit met 2026-10-24). CONTINUE works. Field menu opens party, summary, Pokédex, bag, and trainer card. Tests compare paths frame-by-frame with melonDS and check the sound driver, music, and cries. Phase 4 adds Hugh's battle, Bianca's capture demo, a wild catch, PC deposit/withdraw/swap, and save continuations, verified in both games. Phase 5 follows Alder through Route 19 and Floccesy Town to a Route 20 save. Trainer-AI decisions that depend on DS CPU time remain unresolved (comparison cuts).
- Stack: host-compiled decomp C, SDL3, custom HAL / gfx / audio. Documented in `port/docs/` (ARCHITECTURE, PLAN, RESEARCH, ADRs, `research/assets-arm7-sound.md`, etc.). `port/docs/STATUS.md` is a very large log (862,222 bytes) and was not fully read.

Frigate is the best documented proof that pokebw2's C can run natively. It is not a 3DS port. Ideas transfer; code will need a new backend (libctru / citro3d / NDSP), and Frigate's 64-bit layout work is largely unnecessary on the 3DS.

### 2.2 somefosterkid/ntrnative (secondary, treat claims carefully)

- https://github.com/somefosterkid/ntrnative
- Created 2026-11-12, last push 2026-11-22 08:13:38 UTC (03:13 EST), GPL-3.0, 0 stars, 33,000 KiB.
- Vendors patched pokebw2 in `third_party/pokebw2`. Uses [libntr](https://github.com/cybervisi0n/libntr) plus its own NitroSystem, `vcore`/`gpu`, and `acore`.
- Same B2/W2 SHA1s as pokebw2.
- **Conflicting status:** the root README says scripted runs play Black, White, Black 2, White 2, and Platinum from a new game to the credits (and lists macOS/iOS). The B2W2-specific [docs/black2-white2.md](https://github.com/somefosterkid/ntrnative/blob/main/docs/black2-white2.md) is narrower: Linux/Windows/Android "new game to the player's room, save and continue" and "about 17 of 18 route legs of the first hours of Black 2"; macOS/iOS "coming" / "Playable: Black 2's new game runs through Aspertia City with no open bugs." Online play, DS Protect, and DSi-only features are not supported. Prefer the game-specific doc. This research did not run the app.

Useful as another HAL/SND data point. Do not treat "credits" as verified for B2/W2.

## 3. Reuse survey: HGSS New 3DS and Platinum 3DS

### 3.1 Sources read

GitHub [unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) at `4d4d2cb` (`4d4d2cb85f8d26617c47802227dfff19ffc37cc9`, 2026-11-23 03:02:31 UTC / 2026-11-22 22:02 EST, PR #582). At this commit:

- `HANDOFF.md` is at the **repo root**, not `docs/HANDOFF.md` (that path is absent at `4d4d2cb`).
- `AGENTS.md` is at the repo root (37,115 bytes).
- Changelog: `docs/CHANGELOG.md`.
- Third-party: `docs/THIRD_PARTY.md`.
- Also present: `docs/ARCHITECTURE.md`, `README.md`, `soulsilver3ds/platform/`, etc.

Local box copies used as extra context (they are **older** than `4d4d2cb`):

- `/workspace/soulsilver3ds` HEAD `10a6cf38730706d99d29d97d0786f449398c2905` (2026-11-02 16:38:29 EST)
- `/workspace/soulsilver-ref`
- `/workspace/platinum3ds` (repo `unowncollectables/3ds-platinum`), especially `github_export/docs/HANDOFF.md`, `github_export/AGENTS.md`, `github_export/THIRD_PARTY.md`, and `github_export/platinum3ds/platform/`

Shared box toolchain (present, not invoked for this task):

- `source /workspace/devkitpro/env.sh`
- Emulator: `/workspace/tools/estyseesghosts/azahar/build/bin/Release/azahar`

### 3.2 What the SS/Platinum runtime actually is

From HGSS `AGENTS.md` at `4d4d2cb`: the game's decompiled C runs on the 3DS ARM11; `soulsilver3ds/platform/` (copied from the Platinum port) supplies the main loop, GPU 3D (`g3gpu.c`), melonDS-derived software 2D/3D cores, GPU 2D compositor (`gpucomp.c`), scaling and stereo (`gpuscale.c`), ROM cache, and the settings app. pspoke is the HGSS-specific compile wrapper history; Platinum has the same platform idea over pokeplatinum.

`docs/THIRD_PARTY.md` at `4d4d2cb` credits pspoke, pret/pokeheartgold, the Platinum 3DS `platform/` and melonDS-derived 2D/3D cores (GPL-3.0), and some UI design/art inspiration from GabeSud0/pokeplatinum-3ds.

### 3.3 Layer-by-layer reuse

| Layer | SS / Platinum files (representative) | Reuse vs Gen 5-specific |
| --- | --- | --- |
| Render / GX / G3 shim | `g3gpu.c`, `g3soft.cpp`, `gpucomp.c`, `gpuscale.c`, `render3ds.cpp`, `shaders/`, `widecomp.c` | **Reuse the approach and much of the compositor/scaler.** The G3 command path and PICA submission can start from this code. Gen 5 GFL + NNS G3D, cameras, battle view, and effects need new work. Do not assume Gen 4 offsets or overlays. |
| Audio output | NDSP path in platform audio shims; SS has stall-fade and DSP watchdog history | **Reuse NDSP init/output, shutdown, and watchdog.** The sequence player must be built (or ported) for NNS SDAT / ISS / streams / cries. Gen 5 SDAT is large (~89.4 MB). |
| Input | `psp_ctru.c` / pad, touch, lid, extra buttons (ZL run, etc.) | **Mostly reusable** (libctru HID/touch/C-Stick/ZL/ZR). Map to GFL input; C-Gear / Xtransceiver / mic are Gen 5-specific. |
| ROM cache / FS | `romcache3ds.c` | **Reusable idea** (read the user's `.nds` from SD, cache FAT/NARC). Wire to pokebw2's 308-entry archive table and 318 NitroFS files. Overlay load for 344 slots is Gen 5-specific. |
| Save shim | 512 KiB `.sav` policy; game owns writes | **Policy reusable; format is Gen 5-specific** (0x26000 + backup, footer magic `0x31053527`, 21 extra slots). Never write the save outside the game. Hunt/tracker/settings go in sidecar files. |
| Settings app (L+R+Y) | `ss3ds_settings_ui.h` and settings tables | **Reuse the pattern.** New keys for a new game. Feature toggles default off until proven (HGSS later turned some display options default on after test; start off). |
| Wide view | `wideapi.c` / `widecomp.c`, Hor+ / dirty-line compose | **Reuse compositor ideas.** Gen 5 projections, HUD, and battle layouts need their own work. Frigate ADR 0030 discusses engine-side Hor+ (proposed, PC). |
| Sharp text | `ss3ds_gametext.c`, `gpuscale.c` `gtDraw`, Settings "Sharp text" / "Sharp game text" | **Reuse the layering model.** At `4d4d2cb` HANDOFF (2026-11-22 note): present native text with the same screen flags `P3DS_GtSync` already applied that frame, so a bottom-screen tab change cannot flash missing or doubled text. Gen 5 fonts/windows are GFL, so the hooks are new. |
| Bottom-screen apps | HGSS Pokégear / JOHTO panel (`ss3ds_minimap.h`, `ss3ds_blnative.h`, party-in-panel) | **Mostly Gen 5-specific.** B2W2 bottom UI is C-Gear, Xtransceiver, different menus. Layout/compositor helpers may transfer. |
| Followers | `p3ds_follower_luts.h`, follower hooks; Platinum `followers/` extractor | **Idea reusable; data and actors are Gen 5-specific.** pokebw2 `src/field/actor/` includes follower code. |
| Shiny / hunt tracker | `hunt.c`, `tracker.c` | **Feature reusable.** Write `hunt.txt` (or similar) on SD, never the `.sav`. Hook Gen 5 encounter/species IDs. SS learned not to fopen/write on the game thread (async writer; and that writer must be joinable — see threads). |
| Mobile PC | hook 0293, Settings > SYSTEM, default OFF; party footer BOXES chip; official box app after Hall of Fame; sidecar `mobile_pc.txt` | **Rewrite against Gen 5 party/box APIs** (box is ov255 in the module map; party UI differs). Keep default off, no auto-save, no writes to `.sav`. |
| Logging | `logring.c` | **Reusable.** Background writer must be stoppable and joined on exit. |
| Wireless / GTS | `uds_ctru.c`, `wm_uds.c`, `ss3ds_net.c`, `docs/WIRELESS.md` | **Core, not optional.** Reuse the proven HGSS UDS + WFC/DNS stack. Gen 5 still needs DWC, GTS, C-Gear, Entralink, Game Sync/PDW, Xtransceiver, IR, and Dream Radar work — see `PLAN.md` Phase 7. Stub only as a temporary compile stand-in. |
| CIA / banner / HOME menu | `tools/banner3d`, forwarder art | **Reusable pipeline**; new art from the user's B2/W2 ROM at build time. |
| HeartGold dual-game filter | `hg_rename.py`, path filter | Pattern for building Black 2 and White 2 from one tree (pokebw2 already shares sources). |

### 3.4 Lessons from SoulSilver development (carry these as hard rules)

From HGSS `HANDOFF.md` / `AGENTS.md` at `4d4d2cb`, and the commit message of `4d4d2cb` itself (PR #582):

1. **~32 thread application limit.** After about 15 Select soft resets, hardware boot 15/16 had dead sound: no thread could be created on any core (`speedcheck -1`, `g3thr` could not start, `ndspInit` never reached). Cause: two detached port threads (log writer, hunt writer) were never joined, leaking about 2 slots per reset against the 32-thread limit.
2. **Join workers at exit.** Log writer, hunt writer, and watchdog must be stoppable, joinable, and joined from `hardExit`. Do not detach long-lived threads.
3. **Sharp-text layers use synced screen flags** (`P3DS_GtSync`) so tab/menu changes do not flash missing or doubled text.
4. **Features are Settings toggles, default off** until the owner has tested them. Config file alone is not enough.
5. **Never write the save outside the game.** Sidecar files for hunt, mobile-PC "already shown", logs, and settings.
6. **One builder, no ROM/save/binary commits, test on save copies.**
7. **Reuse first** (the SS rule was "check 3ds-platinum / pokeplatinum-3ds"). For B2W2, check SS and Platinum platform, then Frigate, before writing a new shim.
8. New 3DS stereo 60 fps is already near the GPU ceiling in SoulSilver. Gen 5 is heavier. Budget performance from day one; mono 60 may be the honest default.

### 3.5 New 3DS hardware (verified for a New 3DS-only CIA)

This port is New 3DS / New 3DS XL / New 2DS XL only. Do not plan around old 3DS limits. Sources fetched 2026-11-22:

- [3dbrew Hardware](https://3dbrew.org/wiki/Hardware): New3DS ARM11 is **4× MPCore, 4× VFPv2, able to run up to 804 MHz**, with an **optional 2 MB L2 cache**. Old 3DS is 2× MPCore at 268 MHz. GPU is still DMP PICA at 268 MHz; on-SoC VRAM is listed as 6 MB. DSP is CEVA TeakLite at 134 MHz, 24 channels, 32728 Hz.
- The same page: **New3DS exclusives can clock 804 MHz**, limited to the running application's cores. On New3DS, Home Menu runs at 804 MHz; **everything else stays at 268 MHz unless the app(let) sets the required flag**. Official kernel only exposes the 3× multiplier.
- [3dbrew New 3DS](https://www.3dbrew.org/wiki/New_3DS): FCRAM extended **from 128 MB to 256 MB**, plus another **4 MB** memory block. Extra analog **C-Stick**, **ZL and ZR**, NFC, MicroSD. Product codes KTR / RED / JAN (New 2DS XL).
- [3dbrew Memory layout](https://www.3dbrew.org/wiki/Memory_layout): New 3DS L2C-310 r3p3 (2 MB) at `0x17E10000`; FCRAM extension `0x28000000` size `0x08000000`; extra block `0x1F000000` size `0x00400000`. L2 is enabled only after `PTMSYSM:ConfigureNew3DSCPU` with the cache bit.

**Plan implication:** the CIA must be a New 3DS exclusive (or otherwise set the official clock/L2 flags). If it does not, it will run at 268 MHz on New 3DS hardware. Use the extra cores for audio / IO / compose threads, still joining every worker (the ~32-thread process limit from HGSS PR #582 is a process limit, not an old-3DS limit). How many megabytes of FCRAM a userland app actually gets (figures of ~64 MB old / ~124 MB new appear in secondary write-ups such as Copetti) was **not independently verified** from 3dbrew in this pass — treat the physical 256 MB number as verified, the app budget as unknown until measured on hardware or cited from libctru / 3dbrew application-memory docs.

## 4. NitroSDK / TwlSDK reconstruction (context)

Public ntrtwl trees (not a complete TwlSDK repo):

- https://github.com/ntrtwl/NitroSDK — "Nintendo DS Software Development Kit" (created 2023-09-02, last push 2026-11-19, 24 stars)
- https://github.com/ntrtwl/NitroSystem — "Nintendo DS Graphics and Sound Libraries" (last push 2026-09-17, 4 stars)
- https://github.com/ntrtwl/NitroWiFi
- https://github.com/ntrtwl/NitroDWC
- https://github.com/ntrtwl/libvct

A `ntrtwl/TwlSDK` repository was not accessible / not found. pokebw2 already contains reconstructed `lib/nitro`, `lib/twl`, and `lib/nnsys` used to match the game; a 3DS port should compile those as host/3DS shims rather than starting from ntrtwl, though ntrtwl remains a reference.

## 5. Alternatives (if the decomp were not buildable)

The decomp **is** reported as fully matched and SHA1-checked by its own CI/ByteMatch, so these are fallbacks, not the plan:

| Approach | Effort (rough) | Why not first |
| --- | --- | --- |
| Wait for pret or more naming | Months of calendar time, little port progress | Naming is nice; it is not required to compile C for ARM11 |
| Static recompilation (DS → 3DS) | Very high (many months to years) | Dual CPU, 344 overlays, DSi LTD, ARM7 SND, and DS-specific DMA/VRAM aliases are a poor fit compared to already-matched C |
| HLE / wrapper around an emulator core | Medium to high | That is an Azahar/melonDS frontend, not a native port; the owner already has Azahar on the box |
| Start from ntrnative/libntr or Frigate and only retarget the HAL to 3DS | High, but lower than from scratch | Reasonable **reference**, especially Frigate. Still need a 3DS GPU/audio/input backend; SS/Platinum already have that backend for a similar decomp-game architecture |

## 6. What this research did not verify

- A local matching rebuild or SHA1 check (no ROMs).
- ByteMatch JSON payload (empty body here).
- Exact EU ROM identity vs the published "USA/Europe" SHA1s.
- Full read of Frigate `STATUS.md` (862 KB) or pokebw2 `docs/data.md` / `docs/matching.md` (very large).
- ntrnative's "to the credits" claim for B2/W2.
- Instruction-level audit of NNS G3D usage or save-block field layouts beyond the Frigate note and module map.
- Whether `ArchiveMoveImageTop` or deadstrip reconstructions affect a non-matching native build (they matter for ROM matching, not necessarily for a 3DS port).
- New 3DS **userland** FCRAM budget (how much of the 256 MB an APPLICATION-mode CIA actually gets).
- Whether official Dream Radar, running on the same New 3DS, can be made to write a CIA-hosted SD `.sav` (expected no; not tested here).
- Whether 3DS `ir:U` / IRUSER can speak C-Gear or Pokewalker IR to original DS hardware.
- Live reachability of Kaeru/Wiimmfi beyond PCN's own front page (the front page was fetched; no DS client was pointed at the DNS here).
- `PlatinumMaster/pokewhite2` itself: public GitHub API and raw README returned **404** on 2026-11-22. The early-2022 text survives in `kleenxfeu/pokewhite2` (see §8.1). The original web-README wording is otherwise unverified because the repo is gone or private.

## 7. Links (quick index)

- Decomp: https://github.com/fuddlesworth/pokebw2
- Progress: https://bytematch.dev/fuddlesworth/pokebw2
- Porting notes (mostly NitroWiFi): https://github.com/fuddlesworth/pokebw2/blob/main/docs/porting-notes.md
- Config / DSi: https://github.com/fuddlesworth/pokebw2/blob/main/docs/configs.md
- Module map: https://github.com/fuddlesworth/pokebw2/blob/main/docs/module-map.md
- Frigate: https://github.com/CallanCampos/Frigate
- Frigate ARM7/sound/saves/DSi: https://github.com/CallanCampos/Frigate/blob/main/port/docs/research/assets-arm7-sound.md
- ntrnative B2W2 notes: https://github.com/somefosterkid/ntrnative/blob/main/docs/black2-white2.md
- swan: https://github.com/ds-pokemon-hacking/swan
- CTRMap-CE: https://github.com/ds-pokemon-hacking/CTRMap-CE
- aletheia (BW, not B2W2): https://github.com/Project-Holon/aletheia
- HGSS New 3DS: https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds/tree/4d4d2cb
- ds-decomp: https://github.com/AetiasHax/ds-decomp
- New 3DS hardware: https://3dbrew.org/wiki/Hardware and https://www.3dbrew.org/wiki/New_3DS
- Poké Classic Network: https://pkmnclassic.net/
- entralinked: https://github.com/kuroppoi/entralinked
- dreamworld-reawakened: https://github.com/minibug1021/dreamworld-reawakened
- Dream Radar save notes: https://projectpokemon.org/home/forums/topic/40323-white-2-dream-radar-flags/
- Bad B2 [b] dump (user B2 hash): https://tasvideos.org/Games/940/Versions/View/3786
- Good B2/W2 hashes: https://tasvideos.org/Games/940/Versions/List

## 8. Prior art & resources

Exhaustive public-web pass (2026-11-22, EST). Searched GitHub (including name queries `pokeblack`, `pokewhite`, `pokebw`, `pokeblack2`, `pokewhite2`), GitLab, Codeberg, pret.github.io, DS Pokémon Hacking docs, PokeCommunity, Project Pokémon, Reddit (`r/decomps`, `r/PokemonROMhacks`, `r/3dshacks` / emulation hits), and known revival-server and recomp projects. Discord servers (pret, Kingdom of DS Hacking) were **not** logged into; only their public sites/wikis were read. One-line verdict after each link.

### 8.1 Black 2 / White 2 decomps and disassemblies

| Resource | Verdict |
| --- | --- |
| [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2) | **Best base.** Matching C decomp of US B2 and W2; ByteMatch 100%; GPL-3.0. |
| [nds197/pokeblack2](https://github.com/nds197/pokeblack2) | pret-style **WIP disassembly** (same SHA1s). Almost all assembly (`src/` is only `main.c`; ~266 asm files). Requires a user-supplied proprietary TWL SDK. README points at pret.github.io but it is **not** a listed pret project. Not a viable native-port base beside pokebw2. |
| [PlatinumMaster/pokewhite2](https://github.com/PlatinumMaster/pokewhite2) | **Not accessible on 2026-11-22.** Public GitHub API and `raw.githubusercontent.com` both returned 404. Do not treat earlier web-search summaries of its README as a live source. |
| [kleenxfeu/pokewhite2](https://github.com/kleenxfeu/pokewhite2) | Public stand-in for that early work. GitHub `fork` is false; created 2024-06-05; `pushed_at` 2022-06-23. README fetched: "an attempted disassembly/decompilation of Pokémon White 2" and "does not build any executables at the moment." The only two commits are by **PlatinumMaster**, 2022-06-23 (`a672e725`, `237cd21a`). Historical interest only. **Not a base.** |
| [FieryMewtwo/pokeblack2](https://github.com/FieryMewtwo/pokeblack2) | Empty 2021 stub ("Decompilation of Pokémon Black 2/White 2"), 4 KiB, no language. Ignore. |
| Other GitHub name collisions (`advent259out/pokeblack2` Unity sketch, `GinjaNinja5197/PokeBlack2` empty, `969981/pokebw2` unheralded copy, etc.) | Not decomps or ports of use. |
| pret `pokeblack2` / `pokewhite2` | **Does not exist.** pret.github.io lists DS Pokémon work only through Diamond/Pearl, Platinum, and HeartGold/SoulSilver. |

### 8.2 Black / White (Gen 5, not B2W2) decomps and PC ports

| Resource | Verdict |
| --- | --- |
| [Project-Holon/aletheia](https://github.com/Project-Holon/aletheia) | Sister matching decomp of **Black and White** (not B2W2). README fetched 2026-11-22: **100% matched**, **35,197** functions, **3,396,468** instruction bytes, 1,392 files under `src/game/`, 237 overlays, links with `mwldarm`. Tooling is MIT; `src/` is distributed without a license grant (`LEGAL.md`). GitHub: 47 stars, last push 2026-11-17 17:44 UTC (12:44 EST). |
| [Project-Holon/eidos](https://github.com/Project-Holon/eidos) | Native **PC/mobile port of Black and White** on aletheia (MIT). README fetched 2026-11-22: "The main story is playable, though crashes and visual glitches can still occur." Milestone 7 has overworld/battles/menus/saves checked, but **"Fully playable from new game to credits" is unchecked**. Experimental WFC/Union Room/Mystery Gift via a custom relay. GitHub: 128 stars, last push 2026-11-20 19:28 UTC (14:28 EST). Best Gen 5 PC-port lessons after Frigate; 3DS is 32-bit so 64-bit ABI pain is less. Not a 3DS port. |
| [Project-Holon/kenon](https://github.com/Project-Holon/kenon) and [project-holon.com](https://project-holon.com/) | Modding suite (MIT, 13 stars, last push 2026-11-16) + project page for aletheia/eidos. Useful for how a Gen 5 native port exposes mods; not B2W2. |
| [squiddonaut/pokeblack](https://github.com/squiddonaut/pokeblack) | Separate English Black decomp, not B2W2. [decomp.dev report](https://decomp.dev/squiddonaut/pokeblack) fetched 2026-11-22, for commit `67d91a78` (2026-10-17): **0.0285% matched code** (1,094 / 3,834,622 bytes), **19 / 363** functions (5.23%), page title "0.03% decompiled". Far behind aletheia. |
| [Ax461/pokeblack](https://github.com/Ax461/pokeblack) | Creepypasta recreation, not the retail game. Ignore. |

### 8.3 Native / PC ports of DS Pokémon (including this owner's 3DS work)

| Resource | Verdict |
| --- | --- |
| [CallanCampos/Frigate](https://github.com/CallanCampos/Frigate) | **Primary B2W2 native PC/Mac port** from pokebw2. Field + early story/battles with melonDS oracle tests. Best HAL/sound/save/DSi documentation to steal ideas from, not 3DS code. Last push 2026-11-04 (may lag pokebw2). |
| [somefosterkid/ntrnative](https://github.com/somefosterkid/ntrnative) | Multi-game native app (BW, B2W2, and claimed Gen 4). Vendors pokebw2. B2W2-specific doc is only early-game; root README over-claims "to the credits." Secondary reference only. |
| [unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) | This owner's **New 3DS native HGSS port**. The 3DS runtime to reuse (G3, compose, audio, UDS, GTS/MG, settings, sharp text). |
| [unowncollectables/3ds-platinum](https://github.com/unowncollectables/3ds-platinum) (local `/workspace/platinum3ds`) | This owner's **New 3DS native Platinum port**; origin of the shared `platform/` layer. |
| [GabeSud0/pokeplatinum-3ds](https://github.com/GabeSud0/pokeplatinum-3ds) | Independent native 3DS Platinum port from pret. UI/banner ideas already credited in HGSS `THIRD_PARTY.md`. Watch for G3/widescreen ideas; do not copy blindly. |
| [IbrahimIrfan/pspoke](https://github.com/IbrahimIrfan/pspoke) | Native **PSP** Platinum and SoulSilver. HGSS 3DS was retargeted from this. No B2W2. |
| [cybervisi0n/libntr](https://github.com/cybervisi0n/libntr) | NitroSDK replacement for DS/PC (used by ntrnative). Reference for SDK shims; 3DS already has a proven shim in the SS/Platinum platform. |
| Reddit / "Fulldex" / widescreen DS Pokémon posts ([example](https://www.reddit.com/r/decomps/comments/1xi6o7p/pok%C3%A9mon_blackwhite_b2w2_heartgoldsoulsilver_and/)) | **Emulation** (melonDS-style frontend), not a native decomp port. Do not treat as prior native work. |
| "White 2n1" ROM-hack announcement on r/PokemonROMhacks | Unverified hack/AI announcement, not a decomp or native port. |

No public **native New 3DS port of B2 or W2** was found. This project would be the first of that kind in public sources.

### 8.4 pret, forums, and symbol/IDB maps

| Resource | Verdict |
| --- | --- |
| [pret.github.io](https://pret.github.io/) | Official pret list: no Gen 5 game. Discord channels named for DPPt and HGSS only. |
| [ds-pokemon-hacking.github.io](https://ds-pokemon-hacking.github.io/docs/generation-v/guides/getting_started/) | Public docs of the DS Pokémon Hacking community (often called Kingdom of DS Hacking in chat). Best Gen 5 **hacking** tool list, not a decomp. Discord itself not searched. |
| [ds-pokemon-hacking/swan](https://github.com/ds-pokemon-hacking/swan) | B2/W2 headers + `IREO.yml` / `IRDO.yml` ESDBs. Already imported into pokebw2 (rev `4324f73`, 2025-07-03). |
| [ds-pokemon-hacking/CTRMap-CE](https://github.com/ds-pokemon-hacking/CTRMap-CE) | Community Edition of Hello007/HelloOO7's Gen 5 level/script/code-injection editor. Essential historical RE, not runtime code. |
| [ds-pokemon-hacking/PMC](https://github.com/ds-pokemon-hacking/PMC) | Runtime for CTRMap injection patches. |
| Hello007 / HelloOO7 | Author of CTRMap and a top swan contributor; code-injection guide co-author. No matching decomp repo verified. `HelloOO7/CTRMap` was not accessible here. |
| Kingcom | Often named with CTRMap in community talk. **No current public Kingcom B2W2 decomp repo was verified.** |
| [PokeCommunity: Reverse Engineering Pokémon White 2 Game Code](https://www.pokecommunity.com/threads/reverse-engineering-pokemon-white-2-game-code.337860/) | Historical RE thread, not a complete decomp. |
| [PokeCommunity: Beater Suite Of Tools](https://www.pokecommunity.com/threads/beater-suite-of-tools-for-gen-v.444353/) | Tool announcement thread for Gen 5 editors. |
| [Project Pokémon: White 2 Hacking Info Dump](https://projectpokemon.org/home/forums/topic/26010-white-2-hacking-info-dump/) | Thread fetched 2026-11-22. Bond697 (2013-05-25) released `hacking_stuff.7z` and said the main IDA DB is `arm9-white2_decompressed_U.idb` (IDA 6.1.110315), with **"3500 of gamefreak's functions hand-labelled"** (his wording) plus a plain-text `.s`. Historical symbol map; swan/pokebw2 supersede it for a port. The 2013 archive URL was not re-downloaded here. |
| [Project Pokémon: B2W2 File System](https://projectpokemon.org/home/docs/gen-5/b2w2-file-system-r8/) | NARC/path reference. Useful when wiring the ROM cache. |
| [projectpokemon.org/rawdb/black2](https://projectpokemon.org/rawdb/black2/) | Raw DB / file index for Black 2. |

### 8.5 Gen 5 editors and asset tools (not ports)

| Resource | Verdict |
| --- | --- |
| [PlatinumMaster/BeaterScript](https://github.com/PlatinumMaster/BeaterScript) | Archived Gen 4/5 script editor. Superseded for everyday use. |
| [PlatinumMaster/SwissArmyKnife](https://github.com/PlatinumMaster/SwissArmyKnife) | Current recommended Gen 5 editor (maps, text, scripts, encounters). B2/W2 fully supported. |
| [Garhoogin/NitroPaint](https://github.com/Garhoogin/NitroPaint/) | General DS graphics editor (NCGR/NSCR/NSBMD/NSBTX, etc.). |
| [FrostFalcon/FrostsGen5Editor](https://github.com/FrostFalcon/FrostsGen5Editor) | All-in-one B2/W2 (and mostly BW) editor. |
| [turtleisaac/PokEditor](https://github.com/turtleisaac/PokEditor) | **Gen 4** data editor, not Gen 5. |
| [suloku/BW_tool](https://github.com/suloku/BW_tool) | Save tool including a **3DS Link / Dream Radar** block editor (0x7F000). Directly useful for a 3DS-Link receive path. |
| Tinke / TinkeDSi, Nitro Studio 2, Pokeweb, Pokémon DS Map Studio | Listed on the DS Pokémon Hacking getting-started guide for file/sound/map work. |

### 8.6 Replacement servers and comms research

| Resource | Verdict |
| --- | --- |
| [Poké Classic Network](https://pkmnclassic.net/) | Front page fetched 2026-11-22: Gen 4 and V **GTS, battle videos, and other related services**; DNS **`178.62.43.212` (Kaeru WFC)**; runs with Wiimmfi + Kaeru; secondary DNS must be `0.0.0.0` or the same; maintenance Thursdays 03:00 UTC (22:00 EST Wednesday / 23:00 EDT). Does **not** claim Dream World. Same DNS the HGSS 3DS port already uses for working GTS and Mystery Gift (`wifi_dns`). Counts on the page at fetch: 148 Pokémon available for offer, 21,466 battle videos (snapshot only). |
| [Revival Directory note (2026-11-09)](https://revivaldirectory.com/guides/pokemon-ds-games-online-gts-and-battle-videos) | Independent write-up of PCN coverage and the 2014 shutdown dates. |
| [kuroppoi/entralinked](https://github.com/kuroppoi/entralinked) | README fetched 2026-11-22 (`master/README.md`). Standalone **Game Sync** emulator for BW and B2W2: download Pokémon, items, C-Gear skins, Pokédex skins, musicals, and (B2W2 only) Join Avenue visitors, without editing the save. **Memory Link with a Black or White save** if the user has no second DS. Java 17, built-in DNS, dashboard after tucking in. Closest Game Sync / delivery path. |
| [minibug1021/dreamworld-reawakened](https://github.com/minibug1021/dreamworld-reawakened) | README is `readme.md` (lowercase; `README.md` 404s). Fetched 2026-11-22. Self-hosted **Dream World + Game Sync** revival (54 stars, last push 2026-11-22 20:38 UTC / 15:38 EST). Game Sync via `game_sync_server/server.py` (user sets the printed DNS in Nintendo WFC Settings, then tucks in); PGL site via `uv run main.py` on port 8080. Front-end: **Ruffle is documented first**; Basilisk + archived Flash 32.0.0.371, or standalone Flash, as alternatives. README "Things that are not working": storing Tree of Dreams Pokémon, logging befriended Pokémon, sending Dream Pal requests, logout, promotions, E-Z Mail, GBU, Reports. The 3DS browser cannot host this Flash site; the handheld only talks Game Sync. |
| Pretendo | **3DS Nintendo Network**, not DS Nintendo WFC. Wrong layer for Gen 5 GTS. |
| Wiimmfi / Kaeru WFC | General NWFC / SSL offload under PCN. The game's own DWC/NAS code should talk to these the way the HGSS port already does. |
| HGSS `docs/WIRELESS.md` (local `/workspace/soulsilver3ds/docs/WIRELESS.md`, also at 3ds-platinum) | **Proven on hardware:** 3DS↔3DS local trades over UDS; online GTS and Mystery Gift WFC. **Not possible** (as far as that doc knows): native 3DS talking DS local wireless to a real DS. Online is the path to legit carts. Reuse this stack. |
| 3DS IR: [ir:U / ir:USER](https://www.3dbrew.org/wiki/IR_Services), libctru `iruSendData`/`iruRecvData` | Hardware exists. IRUSER and `ir:rst` (New 3DS C-stick) are **mutually exclusive**. C-Gear IR to a real DS is unproven (Pokewalker IR is still "maybe" in the HGSS wireless doc). 3DS↔3DS IR between two copies of this port is the realistic first IR target. |
| Dream Radar 3DS Link | Project Pokémon thread [White 2 Dream Radar flags?](https://projectpokemon.org/home/forums/topic/40323-white-2-dream-radar-flags/) fetched 2026-11-22. BlackShark (2017-03-19): **"The DR itself only touches the 152 bytes at 0x7F000."** Layout includes `CRGF` (`0x43524746`) at `0x7F008`, CRC16-CCITT, legendaries / normal Pokémon / items, keys at `0x7F00C` and `0x7F090`. Legendary-received flags live at `0x25E04` (set by the game on receive, not by DR). A CIA port's SD `.sav` is **not** a cartridge in the slot, so the official app is not expected to transfer the usual way (not tested here). Plan an in-game receive/import of that 152-byte block (and/or a tiny companion), using this layout and [suloku/BW_tool](https://github.com/suloku/BW_tool). |

### 8.7 Static recompilation (ARM / NDS)

| Resource | Verdict |
| --- | --- |
| [RetroPortingToolKit/ndsrecomp](https://github.com/RetroPortingToolKit/ndsrecomp) | General DS static recompiler, pre-alpha v0.6.11. Firmware menu / SM64DS / MKDS bring-up, not Pokémon Gen 5. High risk vs an already-matching C decomp. |
| [mstan/arm-recomp-core](https://github.com/mstan/arm-recomp-core) | Shared ARM/Thumb core used by ndsrecomp. |
| [vs-sr-dev/ndskit](https://github.com/vs-sr-dev/ndskit) | Game-agnostic DS recomp + runtime (used for other DS games, not Pokémon). |
| [sohamb117/gen4recomp](https://github.com/sohamb117/gen4recomp) | README fetched 2026-11-22: Diamond/Pearl/Platinum (and listed Gen 3/HGSS) go through pret-style decomps compiled to wasm2c; **"Black and White use a static recompilation of the cartridge image with `ndsrec` and `armrec`."** SHA1s in that README for BW US/Europe: Black `26ad0b9967aa279c4a266ee69f52b9b2332399a5`, White `bc696a0dfb448c7b3a8a206f0f8214411a039208`. Last push 2026-10-15. **Not B2W2.** Fallback idea only; we already have matching C. |
| [ClickingMaster/ssrecomp](https://github.com/clickingmaster/ssrecomp) | Abandoned SoulSilver recomp (ARM9 recomp + melonDS for the rest). Author moved on. |

**Verdict on recomp:** do not use it as the B2W2 3DS plan. pokebw2 already provides matching C, and the 3DS is 32-bit ARM11.

### 8.8 GitLab and Codeberg

No B2W2 or Gen 5 decomp/port was found on GitLab or Codeberg. [MisterWhopper/pokeplatinum](https://codeberg.org/MisterWhopper/pokeplatinum) is a pret Platinum mirror. Other hits (Pokémon Go scripts, PokeWilds, Obtainium recomp lists) are unrelated.

### 8.9 NitroSDK / TwlSDK reconstructions

Already covered in §4: [ntrtwl/NitroSDK](https://github.com/ntrtwl/NitroSDK), [NitroSystem](https://github.com/ntrtwl/NitroSystem), NitroWiFi, NitroDWC, libvct. No public `ntrtwl/TwlSDK` repo was found. pokebw2's own `lib/nitro`, `lib/twl`, and `lib/nnsys` are the trees a port should compile against.

### 8.10 What was not found

- A public native **3DS** port of Black 2 or White 2.
- A pret-owned Gen 5 decomp.
- A verified public Kingcom B2W2 decomp.
- A Codeberg/GitLab alternative to pokebw2.
- Confirmation that 3DS IR can speak C-Gear or Pokewalker IR to original DS hardware (still unknown).
- A hosted, complete Pokémon Dream World equivalent to PCN's GTS (only self-hosted entralinked / dreamworld-reawakened).

## 9. User dumps on <owner-pc> (diagnosed 22 November 2026, EST)

Read-only inspection of the owner's own files on <owner-pc> (`<machine-id>`). Originals were not modified, renamed, or moved. No ROM was downloaded. Full 512 MiB copies were not brought onto the box; header, region hashes, and padding-repair SHA1s were computed on <owner-pc> against the originals. A <owner-pc> temp folder `<temp-dir>\b2w2-split` was never created. A short-lived diagnostic folder `<temp-dir>\b2w2-diag` was removed after the report was copied; both temp paths were confirmed absent, and both originals still had their original sizes and timestamps.

Both files are 536,870,912 bytes (`0x20000000`), unit code `0x02` (DSi-enhanced), device capacity `0x0C` (512 MiB), maker `01`, ROM-header byte `0x1D = 0x40` on both (this is not a Rev 1 cart; official US/EU listings treat these as the base version). Logo CRC16 is the standard `0xCF56` and matches. Header CRC16 matches the bytes in each header. ARM9 secure area is **decrypted** (does not start with `encryObj`; stored secure-area CRC16 does not match the decrypted bytes — normal for a decrypted dump and for pokebw2). No ARM7 overlay table (stock TwlSDK ARM7), 344 ARM9 overlays, 662 FAT entries. The ASCII `PATCH` at ARM9+`0x1084` on both files is the Nintendo SDK string `1000_TWLDWC_5_3_PATCH1_SSLGDB4`, not an anti-piracy crack. The bytes `R4` in ARM9 are ordinary code, not a flashcart marker.

### 9.1 Black 2 — `<rom-dir>\Roms\Pokemon Black Version 2 .nds`

- On-disk timestamp (<owner-pc> local clock): 27 January 2013, 11:00:02 PM.
- Title `POKEMON B2`, game code `IREO`.
- SHA1 `42f7a3daf0601064c9067a6a2903a83ec081442b`
- MD5 `1c5ce747e7ed4431e2044f123ec7c951`
- CRC32 `25ba3aa6`
- These three hashes are exactly the No-Intro / TASVideos **bad dump** `[b]` of `6149 - Pokemon - Black Version 2 (USA, Europe) (NDSi Enhanced) [b].nds` ([TASVideos #3786](https://tasvideos.org/Games/940/Versions/View/3786); No-Intro record 6149). The good dump pokebw2 wants is SHA1 `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb`.
- No-Intro describes this class of B2 `[b]` dumps as missing NDSi-specific data, with the NDSi-specific area filled by `FF`. That is what this file is.
- NTR used size (header `0x80`) `0x11831A00`. Trailing `0xFF` starts at `0x118319A8` (231.806 MiB of `0xFF` through EOF). ARM9i (`0x11883000`, size `0x21C40`), ARM7i (`0x118A5000`, size `0x470F8`), and the declared TWL size (`0x210` = `0x118EC400`) all lie inside that `0xFF` region (100% `0xFF`).
- NitroFS last file ends at `0x112A723C` — the same “useful data below `0x112A723C`” figure in Frigate's asset note. ARM7 SHA1 `364259352fffdcd15b75c2d9e997d2ed0d877190` is identical to the White 2 file (Frigate: ARM7 is the same across versions).
- Header CRC `0x7680` (matches). Secure CRC stored `0x7705`, calc over decrypted `0x4000` bytes `0x23C2` (mismatch expected).

**Verdict: (c) not usable as the pokebw2 baserom; needs a redump.** The NTR game area looks structurally complete, but the TWL/DSi payload is gone and replaced with `0xFF`. Those bytes cannot be reconstructed from this file. Replacing padding cannot reach SHA1 `e51e6dfb…`. pokebw2's SHA1 check and DSi/LTD modules will fail.

### 9.2 White 2 — `<rom-dir>\Roms\pokwhite2uspatched.nds`

- On-disk timestamp (<owner-pc> local clock): 8 October 2012, 9:10:55 PM (the day after the US retail release). The filename says “uspatched”; see below.
- Title `POKEMON W2`, game code `IRDO` (USA/Europe White 2, not the Japanese `IRDJ`).
- SHA1 `eeba7e06370dee9c80a9695d92e216db4a7a5b3b`
- MD5 `32485892a6f36d80f7d9b54fb398eb12`
- CRC32 `ea836862`
- These hashes were **not found** in TASVideos' good/bad list (the good dump is SHA1 `b5d7490be7b415b8f1e672a53e978a9cc667e56a`) nor in a public No-Intro snippet search. They are not the “Experience + Trade Evolution Patched” ROM documented on r/PokemonROMhacks (that one is SHA1 `44fd44f4140cdf3431bcae1f10c03244784a309a`).
- Header CRC `0xD75C` (matches). Secure CRC stored `0x0F68`, calc `0xBE2E` (decrypted, mismatch expected).
- NTR used size `0x117DD400`. A digest-like region at `0x117B2400` size `0x2AEE0` is present (not `FF`). Bytes from `0x117DD400` to `0x11800000` are all `0xFF` (`0x22C00` bytes).
- From **`0x11800000` through EOF** (`0x20000000`) — exactly **232.000 MiB**, including the header's ARM9i (`0x11803000`, size `0x21C38`), ARM7i (`0x11825000`, size `0x470F8`), and declared TWL size (`0x210` = `0x1186C400`) — the file is one **4096-byte pattern repeated with zero mismatches**. Pattern SHA1 `614fd80f49e87da82b74674aa104032b39fde15d`. That is why the file does not end in `0xFF`/`0x00` padding (last byte is `0x0A`, the last byte of the pattern). This is dumper/tool filler, not a real TWL binary, RSA blob, or appended save (no `0x31053527` or `CRGF` in the trailing region).
- ARM7 SHA1 matches the Black 2 file, as expected. NitroFS last file ends at `0x1125443C`. 344 overlays, 662 FAT files.
- The `PATCH` / `R4` hits are the same SDK string / ordinary ARM bytes discussed above, present in the same way on the Black 2 file. No documented AP-patch byte signature was identified, and none was reverted.

**Repair attempts** (SHA1 computed over prefix + fill to `0x20000000`, originals not written):

| Prefix cut | Fill | SHA1 | Matches pokebw2 W2? |
| --- | --- | --- | --- |
| TWL size `0x1186C400` | `0xFF` | `5485af259a5bf0583335949f2f4f556d677477e0` | No |
| TWL size `0x1186C400` | `0x00` | `432477ad45d14f312cea259fe0f7eaebb6494b21` | No |
| NTR size `0x117DD400` (same as `0x11800000`, already `FF` in between) | `0xFF` | `c057c9b5a9e2d35552e6a4d2f241c10fe8dee32b` | No |
| NTR size `0x117DD400` | `0x00` | `d02d762e2912079c542e8c12749f038e9a9790e9` | No |
| ARM9i `0x11803000` | `0xFF` / `0x00` | `1b4e29d0…` / `440f415a…` | No |

Filling the repeating pattern with `0xFF` would only produce a W2 analogue of the B2 `[b]` dump (NTR plus blank DSi area). That SHA1 is **not** the good dump, and it was not found in a public DAT listing here.

**Verdict: (c) not usable as the pokebw2 baserom; needs a redump.** The NTR area looks structurally complete, but the DSi/TWL payload is repeating 4 KiB junk rather than the real ARM9i/ARM7i/signature data. That payload cannot be reconstructed from this file. The “uspatched” name is consistent with a day-after-release scene dump; it is not evidence of a reversible AP patch in the NTR area, and no such patch was identified.

### 9.3 What a later extract could and could not do

- These files will **not** pass pokebw2's input SHA1 check and cannot produce a byte-identical rebuilt `.nds`.
- A 3DS port that defaulted to `OS_IsRunOnTwl() == FALSE` might still *try* to extract NTR ARM9, overlays, and NitroFS from them, but that is **not verified** against a good dump (no good ROM was present, and none was downloaded). Do not treat these as a substitute baserom.
- The correct fix is a new cart dump (GodMode9i / wooddumper-class tools that include the DSi area), then hash against `e51e6dfb…` (B2) and `b5d7490b…` (W2).


### 9.4 Good dumps copied and matching rebuild (23 November 2026, EST)

The owner later provided the good No-Intro / pokebw2 dumps (not the earlier files under `DS/Roms/` above). Originals on <owner-pc> were not modified. Chunks were staged under `<temp-dir>\b2w2-split`, copied to the box one at a time, reassembled under `/workspace/b2w2-3ds/baserom-private/`, and the <owner-pc> temp folder was deleted and confirmed gone.

| Box file | SHA1 | Result |
| --- | --- | --- |
| `baserom_b2_us.nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` | MATCH |
| `baserom_w2_us.nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` | MATCH |

pokebw2 was cloned at `345b790e6000869b481b79838bb5727309f832fb`. Commit `345b790` left stale paths in `config/*/arm9/delinks.txt`; a local 66-path-per-file fix was applied (patch in `/workspace/b2w2-3ds/patches/`). After that, `ninja -j8` rebuilt both games in 135 seconds (00:19:51–00:22:22 EST). Output SHA1s matched the baseroms exactly (`cmp` byte-identical):

- `build/pokeblack2_us.nds` `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb`
- `build/pokewhite2_us.nds` `b5d7490be7b415b8f1e672a53e978a9cc667e56a`

Reproduce with `/workspace/b2w2-3ds/scripts/build-matching.sh`. Full steps, tool paths (clang/llvm in `/usr`), and the first-ninja error are in `HANDOFF.md`.
