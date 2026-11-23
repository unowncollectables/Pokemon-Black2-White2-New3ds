# Black 2 / White 2 New 3DS port — plan

Drafted Sunday, 22 November 2026 (EST) from `/workspace/b2w2-3ds/docs/RESEARCH.md`. Research only so far: no builds, no ROM downloads, no git remotes. Effort numbers are order-of-magnitude person-weeks for someone who already shipped the HeartGold/SoulSilver and Platinum New 3DS ports. They are not a calendar commitment.

This is a personal-use native port. It is not affiliated with Nintendo, Game Freak, Creatures, or The Pokémon Company. Keep ROMs, saves, and extracted assets out of the tree.

## 1. Goal

Ship two New 3DS-only CIAs, **Pokémon Black 2** and **Pokémon White 2**, built from one source tree:

- The game's decompiled C, from [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2), compiled for the 3DS ARM11 (32-bit).
- The existing HeartGold/SoulSilver and Platinum `platform/` runtime (G3/GX shim, compose, NDSP, ROM cache, settings, UDS/WFC, logging), with Gen 5-specific hooks.
- [CallanCampos/Frigate](https://github.com/CallanCampos/Frigate) as the main native-port reference for sound, save, DSi policy, and HAL shape — not as 3DS code.
- All communication features are **core phases**, not optional add-ons: local wireless, GTS, Mystery Gift, Wi-Fi trades and battles, C-Gear, Entralink, Game Sync / Pokémon Dream World, Xtransceiver, IR, Memory Link, and Dream Radar receive.

Target hardware is New 3DS / New 3DS XL / New 2DS XL only. Plan around 804 MHz, 256 MB FCRAM, 2 MB L2, four ARM11 cores, C-Stick, and ZL/ZR. Do not plan around old 3DS limits. If the CIA does not set the official New 3DS clock/L2 flags, the process stays at 268 MHz even on New 3DS hardware ([3dbrew Hardware](https://3dbrew.org/wiki/Hardware)).

## 2. What the user must supply

Before any build or playthrough work:

1. **Own clean dumps of both carts.** The user owns both Black 2 and White 2. Dump each from the cartridge (or an already-owned clean dump). Do not download ROMs.
2. **SHA1 must match the pokebw2 README** (US product codes IREO / IRDO in CTRMap docs):
   - Black 2, `orig/baserom_b2_us.nds`: `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb`
   - White 2, `orig/baserom_w2_us.nds`: `b5d7490be7b415b8f1e672a53e978a9cc667e56a`
3. The README labels those hashes "USA/Europe". Whether every European language dump matches them was **not verified here**. If a cart hash does not match, stop and audit the version before building; do not "almost" use a nearby dump. Frigate mentions a separate proposed path for Spanish (`IRES`); that is out of scope until someone adds it.
4. A **New 3DS-family console** with the usual homebrew/CIA install path the SS/Platinum ports already use.
5. For online features: the ability to set DNS (in-game WFC settings and/or a 3DS network profile) to community servers. Poké Classic Network's published DNS is `178.62.43.212` (Kaeru WFC), with the secondary DNS `0.0.0.0` or the same ([pkmnclassic.net](https://pkmnclassic.net/), fetched 2026-11-22). Game Sync / Dream World need a self-hosted [entralinked](https://github.com/kuroppoi/entralinked) and/or [dreamworld-reawakened](https://github.com/minibug1021/dreamworld-reawakened) instance on a PC on the same network (the 3DS browser cannot run the Flash Dream World site).
6. For Memory Link: the user's own **Black or White** save (entralinked can Memory Link without a second DS).
7. For Dream Radar: the user's own Dream Radar data if they want official legendary/item transfers; the official cart-slot path is not expected to write a CIA SD `.sav` (see Phase 7h).

Toolchain already on the shared box, not invoked in this research pass: `source /workspace/devkitpro/env.sh`; Azahar at `/workspace/tools/estyseesghosts/azahar/build/bin/Release/azahar`.

## 3. Architecture

```
pokebw2 game C  ──compile for ARM11──►  B2 CIA and W2 CIA
        ▲                                ▲
        │ BLACK2 / WHITE2 defines        │ New 3DS exclusive flags
        │                                │ 804 MHz + L2 + extra RAM
SS/Platinum platform/  ──reuse──►  g3gpu, g3soft, gpucomp, gpuscale,
                                   render3ds, romcache, logring,
                                   wm_uds / uds_ctru, settings, hunt
Frigate port/          ──reference only──►  ARM7 SND player, save RTC,
                                            DSi policy, HAL split
```

Policy that carries over from HGSS at `4d4d2cb` (see RESEARCH.md §3.4):

- Join every long-lived thread at exit (process ~32-thread limit; PR #582). This is not an old-3DS limit.
- Settings via L+R+Y; features default off until tested.
- Never write the 512 KiB `.sav` outside the game. Hunt, settings, logs, mobile-PC flags go in sidecar files.
- Sharp-text layers use the same screen flags (`P3DS_GtSync`) already applied that frame.
- One builder; no ROM, save, or binary commits; test on save copies.

DSi policy from Frigate `port/docs/research/assets-arm7-sound.md`: default `OS_IsRunOnTwl() == FALSE` (NITRO behaviour) so camera, DSP, SSP, SNDEX, NWM, and FATFS paths stay off and the heap stays on the NTR budget. A later Settings flag can turn on selected TWL-style features (webcam / Xtransceiver) once those shims exist. Do not emulate DS NWM; use 3DS sockets and UDS.

License: pokebw2 and the SS/Platinum platform are GPL-3.0. A port tree that includes those sources is GPL-3.0.

## 4. Both games from one tree

pokebw2 already builds Black 2 and White 2 from shared sources with `BLACK2` / `WHITE2` defines. Black 2 is the primary version.

From pokebw2 `docs/code-organization.md`: White 2 has 41,423 functions, of which 41,311 are byte-identical to Black 2 aside from relocations, and **112 differ**. That 41,423 figure does not match ByteMatch's 43,418 for the tracked default version; quote both, and treat the 112-function delta as the version-split to keep compiling. Frigate's asset note (not re-measured here, no ROMs) says 8 of 318 NitroFS files differ between B2 and W2.

Practical rules, same idea as the HGSS HeartGold filter:

- One repo, two CIAs (`pokeblack2_us.cia` / `pokewhite2_us.cia`, names TBD).
- Bring up **Black 2 first** on hardware, but **keep White 2 compiling at every milestone** (CI that compiles both, even when only B2 is play-tested that week).
- Version-exclusive story, legendaries, and encounters stay in the game C; do not fork the platform.
- Memory Link and Join Avenue are B2W2 features (Join Avenue delivery is B2W2-only in entralinked's README). Test them on both CIAs once comms land.
- Do not ship a "both games in one CIA" switcher unless a later decision says so. Two CIAs keep saves and HOME-menu titles clean.

## 5. New 3DS-only runtime rules

Verified on 3dbrew (see RESEARCH.md §3.5):

| Resource | Use it |
| --- | --- |
| 804 MHz (3× clock) + 2 MB L2 | Set the official New 3DS exclusive / `ConfigureNew3DSCPU` flags in the CIA and at process start. Confirm with a boot banner or log line that the clock is actually 804, not 268. |
| 256 MB FCRAM (+ 4 MB extra block) | Size the ROM cache and SDAT working set for New 3DS. The **userland** budget (how many MB the APPLICATION process actually gets) is still **unknown** here — measure it in Phase 1 before locking cache sizes. |
| Cores 0–3 | Keep the game thread and GPU submit on familiar cores; put NDSP / sequence player, ROM-cache IO, and compose helpers on the extra cores. Still join every worker. |
| C-Stick, ZL, ZR | First-class bindings from Phase 2, remappable in Settings. Suggested defaults (tweak after play): ZL or ZR = run/bike, C-Stick = camera or circle-pad duplicate. **Conflict:** New 3DS `ir:rst` (C-Stick) and `ir:USER` are mutually exclusive ([3dbrew IR Services](https://www.3dbrew.org/wiki/IR_Services)). Plan a Settings mode that releases C-Stick/`ir:rst` when C-Gear IR is in use. |
| GPU still 268 MHz PICA | New 3DS does not buy a faster GPU. Budget for mono 60 fps as the honest default; stereo 60 was already near the ceiling in SoulSilver. Wide/sharp-text are Settings toggles. |

## 6. Phased plan

Phases can overlap (audio research can start during field bring-up; comms stubs start in Phase 1). Do not call a later phase "done" if its exit criteria below are unmet.

### Phase 0 — Hygiene and tree (about 1 week)

- Personal-use tree under `/workspace/b2w2-3ds` (or a later named repo). No git remotes, no ROM/save/binary commits, unless the owner later asks for a private remote.
- Vendor or submodule pokebw2 at a pinned commit (research pinned `345b790`, 2026-11-21 00:17 EST, plus ByteMatch page commit `1a66adc`). Copy `platform/` from HGSS/Platinum, not the other way around.
- Document THIRD_PARTY (pokebw2, pret-style layout, SS/Platinum platform, melonDS-derived cores, Frigate as reference).
- Confirm the user's two dumps hash-match the table in §2. Refuse to proceed on a mismatch.

### Phase 1 — Compile and boot (about 4–8 weeks)

Exit: both versions compile; Black 2 boots on New 3DS (and Azahar) to the title / intro; 804 MHz + L2 actually enabled; ROM cache can read the large hybrid `.nds` from SD; threads are joinable from day one; a 512 KiB `.sav` can be opened (even if CONTINUE is not done yet).

Work:

- Host/3DS shims for pokebw2's `lib/nitro`, `lib/twl`, `lib/nnsys` (start from the SS/Platinum Nitro shims; Frigate's HAL split is the map).
- Overlay loader for 344 ARM9 overlays (`ov000`–`ov343`). Empty unclassified slots stay empty.
- No ARM7 binary: replace ARM7 services at the ARM9 SDK boundary. Sound is a stub until Phase 5.
- Default `OS_IsRunOnTwl() == FALSE`.
- New 3DS exclusive CIA + clock/L2 bring-up; log the clock.
- Measure userland RAM and set a ROM-cache budget. Frigate's note (not re-measured): useful ROM data below `0x112A723C` (~276 MiB) on B2, NitroFS 318 files / 271 MiB / 308 NARCs / 69,850 members. The full image is a ~512 MiB TWL hybrid — do not map the whole file.

This is larger than the SS "retarget pspoke" bring-up. The 32-bit ARM11 still avoids most of Frigate's 64-bit ABI work.

### Phase 2 — Video and input (about 4–8 weeks, overlaps Phase 1)

Exit: title, menus, and early field render on both screens at a stable frame pace; touch, circle pad, ABXY, L/R, Start/Select, C-Stick, ZL/ZR all reach the GFL input layer; lid and sleep behave.

Work:

- Reuse `g3gpu`, `g3soft`, `gpucomp`, `gpuscale`, `render3ds`, `widecomp` as the compositor/scaler. Gen 5 GFL + NNS G3D, cameras, battle view, and effects are new work. Overlay 169 in VRAM at `0x06898020` (pokebw2 `docs/configs.md`) needs an explicit 3DS mapping plan.
- Bottom-screen C-Gear chrome can be a placeholder until Phase 6–7.
- Input map is first-class for New 3DS extras (see §5).

### Phase 3 — Save and early field (about 2–4 weeks)

Exit: NEW GAME and CONTINUE; party, bag, trainer card, Pokédex open; the on-disk file is a plain 512 KiB `.sav` with correct CRC16-CCITT and footer magic `0x31053527`; daily events keep a sane RTC offset.

Format (Frigate research note, not re-measured): main 0x26000 at 0x0, backup at 0x26000, 21 extra saves 0x4C000–0x7E000, Dream Radar block at 0x7F000 (152 bytes; see Phase 7h). Import must adopt the save's stored RTC offset.

### Phase 4 — Battle, PC, and mid-game systems (about 4–8 weeks)

Exit: wild battles, trainer battles, catch, party summary, PC deposit/withdraw work on hardware; no blocker on the first few gyms.

Frigate left trainer-AI CPU-time unresolved on PC (notes as of last push 2026-11-04). 804 MHz plus extra cores should help, but budget it; do not assume it vanishes. Keep a timing log.

### Phase 5 — Audio (about 4–8 weeks; start the player early)

Exit: overworld music, battle music, SFX, and cries play through NDSP without stalling the game thread; soft reset does not leak threads or kill the DSP; mute/volume/pitch/pan (ISS) have a plan, even if not complete.

This is the largest technical unknown after 3D. ARM7 is not decompiled in pokebw2 (stock TwlSDK 5.3 ARM7; sequencer lives there). ARM9 NNS SND / ISS / GFL sound *is* decompiled. Build a host sequence player from Frigate `port/audio/` (and ADR 0006) feeding the existing SS NDSP path, on an extra core, with the DSP watchdog and a joinable fade-out at `hardExit`.

Gen 5 SDAT is large (`swan_sound_data.sdat` ~89.4 MB in Frigate's note). Streams, in-memory musical banks, Chatot at 8,180 Hz, and Xtransceiver mic come in as the modes that need them ship, not all on day one of this phase.

### Phase 6 — Story completion (about 4–8 weeks)

Exit: a New Game on Black 2 can be driven to the credits on New 3DS hardware; White 2 is play-tested at least through early- and mid-game plus version-exclusive checks; Pokéstar Studios, PWT, Entralink field, Hidden Grotto, Join Avenue, and other post-Aspertia systems have at least a pass/fail log.

Frigate's public notes reached Aspertia through Alder / Routes 19–20, not credits. ntrnative's root README claims scripted runs of B2W2 to credits; its game-specific doc is narrower — **do not take "to credits" as verified**. This phase is real playtesting, not a script claim.

### Phase 7 — Communication (core; design stubs in Phase 1, about 8–16 weeks staggered)

Do not treat this as DLC after credits. Stub DWC/WM early so the game C links; then land each sub-phase against the exit tests below. Official Nintendo WFC is gone. Community servers can change or go offline; the port should make the DNS and server name configurable in Settings (default to PCN's published DNS, same as HGSS `wifi_dns`).

HGSS `docs/WIRELESS.md` (local `/workspace/soulsilver3ds/docs/WIRELESS.md`, 2026-11-02): 3DS↔3DS local Union Room trade works over `wm_uds.c`; online GTS and Mystery Gift WFC work with `wireless=2` and DNS `178.62.43.212`; **a native 3DS app cannot speak DS local wireless to a real DS**. Online (or a same-port 3DS peer) is the path to a legit cart. Reuse that stack; Gen 5 still has more modes.

#### 7a. Local 3DS↔3DS wireless (UDS) — feasibility: high

Trades and battles between two New 3DS consoles running this port (and, if protocol work allows, later between B2 and W2 CIAs). Reuse `uds_ctru.c` / `wm_uds.c`. Exit: a living trade and a living battle on hardware.

Cannot talk to a real DS or to a 3DS running official DS-mode firmware local wireless. Say so in Settings help.

#### 7b. Online WFC: GTS, Mystery Gift, Wi-Fi trades/battles, battle videos — feasibility: high, server-dependent

Wire the game's own DWC / NitroWiFi / `poke_net` through 3DS sockets, the way HGSS already does. Point DNS at [Poké Classic Network](https://pkmnclassic.net/) (`178.62.43.212`, Kaeru WFC + Wiimmfi; maintenance Thursdays 03:00 UTC). PCN's front page (fetched 2026-11-22) claims GTS, battle videos, and other related Gen IV/V services. It does **not** claim Dream World.

Exit: GTS deposit/seek, a Mystery Gift distribution (when one is live), a Wi-Fi trade, a Wi-Fi battle, and a battle-video list against a live server. If a server is down, that is a documented outage, not a silent stub.

Pretendo is 3DS Nintendo Network, **not** DS WFC — wrong layer.

#### 7c. Game Sync, C-Gear skins, musicals, Join Avenue, Memory Link — feasibility: medium-high (self-hosted)

[kuroppoi/entralinked](https://github.com/kuroppoi/entralinked) README (fetched 2026-11-22): standalone Game Sync emulator for BW and B2W2; delivers Pokémon, items, C-Gear skins, Pokédex skins, musicals, and (B2W2 only) Join Avenue visitors, without editing the save; **Memory Link with a Black or White save** if there is no second DS. Java 17, built-in DNS, dashboard after tucking in.

Exit: tuck-in and receive against a local entralinked; Memory Link imports from the user's BW save; Join Avenue visitors land on both CIAs. Hosting can stay on the user's PC. A public host is a later nice-to-have, not a blocker.

#### 7d. Pokémon Dream World / PDW — feasibility: medium, incomplete upstream

[minibug1021/dreamworld-reawakened](https://github.com/minibug1021/dreamworld-reawakened) `readme.md` (fetched 2026-11-22): self-hosted Game Sync (`game_sync_server/server.py`, user sets the printed DNS in Nintendo WFC Settings, then tucks in) plus the PGL site on port 8080. Front-end is Ruffle first, with Basilisk + archived Flash or standalone Flash as alternatives. Documented still-broken: storing Tree of Dreams Pokémon, logging befriended Pokémon, Dream Pal requests, logout, promotions, E-Z Mail, GBU, Reports.

The 3DS browser will not run this site. The handheld only does Game Sync; the user uses a PC for the Dream World UI. Exit: tuck-in from the CIA, interact on the PC site, receive a Pokémon/item back in-game for the features that already work. Do not claim feature parity with 2011–2014 PDW. Track upstream's broken list.

#### 7e. C-Gear, Entralink, Passerby, Unova Link (non-IR) — feasibility: medium, needs a protocol pass

These are Gen 5-specific and live in pokebw2 `communication/`, `entralink/`, and related overlays. First target: two CIAs over UDS for any local C-Gear / Entralink modes that are really local wireless. Online/Passerby modes need a doc pass against the decomp and whatever community servers still implement. Exit: a written protocol note plus at least one working local mode on hardware. Unknown until that pass: exact parity with a real DS C-Gear.

#### 7f. Xtransceiver — feasibility: medium / partly unknown

Voice/video plus the DSi camera path. 3DS has mics and cameras; Frigate's default is TWL off, with a later flag for webcam-style work. Exit: a two-system 3DS↔3DS call on this port (audio first, video if the camera shim is honest). Interop with a real DSi/DS is unknown and not the first target.

#### 7g. Infrared (C-Gear IR, related IR devices) — feasibility: 3DS↔3DS medium; real DS unknown

3DS hardware exposes IR via `ir:U` / IRUSER (libctru `iruSendData` / `iruRecvData`). IRUSER and `ir:rst` (C-Stick) are mutually exclusive — see §5. First target: two copies of this port. Interop with a real DS C-Gear or a Pokewalker is **unknown** (HGSS wireless notes already treat Pokewalker IR as a maybe). Do not advertise real-DS IR until it is demonstrated.

#### 7h. Dream Radar / 3DS Link — feasibility: official cart path low; import path high

Project Pokémon thread [White 2 Dream Radar flags?](https://projectpokemon.org/home/forums/topic/40323-white-2-dream-radar-flags/) (fetched 2026-11-22): BlackShark, 2017-03-19, "The DR itself only touches the 152 bytes at 0x7F000." `CRGF` at `0x7F008`, CRC16-CCITT, payload and keys as documented in RESEARCH.md §8.6. Legendary-received flags at `0x25E04` are set by the game on receive.

The official Dream Radar app talks to a **cartridge in the slot**. A CIA's SD `.sav` is not that cartridge. Official transfer is **not expected to work** (not tested here). Plan:

- An in-game 3DS Link receive that already exists in the decomp, fed by a file/import of a valid 152-byte block (and checksums / `0x25E00` side fields).
- Optionally a tiny companion homebrew that writes that block, or a documented path through [suloku/BW_tool](https://github.com/suloku/BW_tool), always onto a **copy** of the save, with the game itself performing the receive/write.
- Never invent Pokémon into the `.sav` from Settings.

Exit: a known-good Dream Radar block can be received in-game on the CIA; the `.sav` stays a 512 KiB cart image.

### Phase 8 — Quality and Settings features (about 3–6 weeks)

Default off until tested:

- Sharp text / sharp game text, with synced screen flags.
- Wide view (Hor+): reuse compositor ideas; Gen 5 HUD and battle layouts are new (Frigate ADR 0030 is a PC proposal, not 3DS code).
- Hunt / shiny tracker: sidecar file, async joinable writer, Gen 5 species IDs.
- Mobile PC: rewrite against Gen 5 party/box APIs (module map: box in ov255). Default off, no auto-save, no `.sav` writes. Sidecar flag file.
- Logging: `logring`, joined at exit.
- Followers: data from pokebw2 `src/field/actor/`; do not reuse Gen 4 LUTs blindly.

### Phase 9 — Dual CIA, polish, docs (about 2–4 weeks)

Banners from the user's ROM at build time; HOME menu titles for both games; Settings help that tells the truth about DS-local, IR, Dream Radar, and community DNS; AGENTS.md / HANDOFF.md updated for builders; a short playtest checklist for B2 and W2.

## 7. Rough size

| Block | Person-weeks (order of magnitude) |
| --- | --- |
| 0 Hygiene | 1 |
| 1 Boot / New 3DS tree | 4–8 |
| 2 Video + input | 4–8 |
| 3 Save + early field | 2–4 |
| 4 Battle / PC | 4–8 |
| 5 Audio | 4–8 |
| 6 Story to credits | 4–8 |
| 7 Comms (all core sub-phases) | 8–16 |
| 8 Quality features | 3–6 |
| 9 Dual CIA / polish | 2–4 |
| **Total (with overlap)** | **about 6–12 months of focused work** |

Variance is dominated by the ARM7 sound player, Gen 5 3D/VRAM (overlay 169), and the harder comms (7e–7g, and PDW completeness). A 3DS 32-bit build should be cheaper than Frigate's 64-bit PC port on ABI, and more expensive than the SS port (which started from an already-running PSP port).

## 8. Biggest risks

1. **ARM7 sequencer is not decompiled.** Sound is a rewrite/port of a host player, not a retarget of matching ARM7 C.
2. **New 3DS clock flags.** Shipping without them silently runs at 268 MHz and will feel like a failed port.
3. **GPU is not faster** on New 3DS. Gen 5 is heavier than HGSS. Mono 60 may be the honest default; stereo and wide can stall.
4. **Userland RAM budget unknown.** A too-greedy ROM cache or a fully resident 89 MB SDAT can OOM even with 256 MB of physical FCRAM.
5. **Overlay 169 / VRAM and DSi LTD** (`0x02700000`) need explicit host mappings.
6. **Thread leak vs ~32-slot limit** on soft reset — already burned SS once.
7. **Community servers** (PCN / Kaeru / Wiimmfi / self-hosted Game Sync) can change or vanish. Make DNS configurable; do not hard-code a single future.
8. **No DS local-wireless interop.** Users with a cart in a DS will trade/battle over the internet, not local UDS.
9. **IRUSER vs C-Stick** mutex; real-DS IR unproven.
10. **Official Dream Radar will not see a CIA save** as a cart.
11. **112 B2/W2 function diffs plus differing NARCs** can rot if only Black 2 is tested.
12. **Trainer AI cost** (open in Frigate) may still hitch battles.

## 9. Alternatives (fallback only)

A matching, reportedly SHA1-checked decomp already exists, so these are not the plan (details in RESEARCH.md §5 and §8.7):

- Waiting for a pret `pokeblack2` — no such repo exists; naming is not required to compile C for ARM11.
- Static recomp (ndsrecomp, gen4recomp's `ndsrec`/`armrec` used for BW, not B2W2) — worse fit than already-matched C on 32-bit ARM11.
- HLE / Azahar frontend — not a native port; Azahar is already on the box for oracle tests.

If pokebw2's matching rebuild failed on the user's dumps, stop and find out why (wrong hash, wrong version) before pivoting.

## 10. Success criteria (personal-use v1)

- Black 2 and White 2 CIAs, New 3DS-only, 804 MHz + L2 on.
- New Game to credits on Black 2; White 2 verified for compile, early/mid game, and version-exclusive checks.
- Audio present; soft reset does not kill sound.
- 512 KiB `.sav` compatible with a cart dump / melonDS / DeSmuME, written only by the game.
- 3DS↔3DS local trade and battle (UDS).
- Online GTS, Mystery Gift, and Wi-Fi trade/battle against a configured community DNS, when that server is up.
- Game Sync tuck-in / receive against entralinked; Dream World path documented and working for the subset dreamworld-reawakened already supports.
- Memory Link from the user's BW save.
- Dream Radar receive via imported 0x7F000 block.
- Settings tell the truth about what does not work (DS local wireless, official Radar cart slot, incomplete PDW, unproven real-DS IR).

