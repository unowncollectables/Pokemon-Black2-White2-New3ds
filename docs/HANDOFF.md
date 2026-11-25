# Handoff

Self-contained notes so another agent can take over this New 3DS port.
Phases: [PLAN.md](PLAN.md). Build extras: [BUILDING.md](BUILDING.md).
Short rules pointer: [AGENTS.md](../AGENTS.md).

## 0. Nov 24 follow-up — New 3DS boot and map loading

- Black 2 passes `FieldRoutine_Open` and `FieldRoutine_MapLoad` in Azahar
  and reaches `FieldRoutine_Run` in Aspertia City (zone 427, area 210).
  The host had installed a static
  geometry buffer where NNS expected a game-owned DTCM allocation.
  Releasing it corrupted the DTCM heap before the next allocation.
  The host now returns only game-owned buffers and records each FIFO
  packet once. The actor main and movement tables link from their C objects.
- The hardware log stops in map-load phase 4 while opening `a/0/1/5`.
  Its SD copy matches the file in the emulator run. The host now caps
  NitroFS at eight underlying open SD files and keeps a position per
  logical `FSFile`. The ARM11 fixture checks 24 logical files and seek
  independence across eviction. Azahar reaches phase 6 and the Aspertia
  field routine with the cache. Retest on hardware.
- The post-fade opening script (0x1e) waited for a fanfare load on a
  Nitro thread that never started. New 3DS runs both archive-load steps
  synchronously through the existing sound backend. Black 2 proceeds
  through the scripted transition from zone 427 to zone 428. After the
  dialogue, Azahar verifies movement in all four directions. The backend
  still uses the silent sequence backend.
- Black 2 screenshots show the title logo and the legible, blinking
  `PRESS START` prompt.
  The linked build leaves boot presentation to the game. The 2D fixtures
  cover OBJ tile columns in both color depths, large text BG layouts,
  brightness modes, keypad, and native touch. Bitmap fixtures cover
  stride, clipping, wrapping, and opaque black in both color depths.
  The Black 2 New Game background also renders in Azahar.
- Both native versions link and pass relocation checks. Black 2 has
  emulator playthrough coverage. White 2 and physical hardware still
  need a playthrough. The emulator test build skips the New 3DS speedup
  request that causes Azahar to abort.

### Cartridge saves

- The game's linked backup path reads and writes the original 512 KiB
  flash image in its SD directory. Missing files read as erased flash.
  The first game write creates the image. Existing files get the same
  block and footer writes the game normally sends to the cartridge.
- Startup only inspects the save. A file with the wrong size is rejected
  instead of being resized or replaced. The game's save state machine
  receives the result of each SD write.
- Selecting New Game can create the game's own introductory save before
  field control is available. The first normal field Save records the
  player's location for Continue.
- The native build preserves the DS's four-byte alignment for the
  64-bit fields in six main save blocks. Compile-time checks enforce
  their cartridge sizes and offsets so the game's CRCs cover the
  original byte ranges.
- Azahar loads a Black 2 cartridge save through Continue and reaches
  the saved field in Striaton City with C-Gear disabled. The replay
  opens the field menu and displays the saved party.

### Black 2 graphics follow-up

- The New Game menu's red and gray background and item outlines render
  cleanly in Azahar. Full LCDC resets now clear the host's BG and OBJ
  views when all nine banks are mapped to LCDC. Previously, letters and
  tiles from the last scene remained in those views. The compositor also
  applies the menu's background alpha blend against the layer below.
- The port links the SDK camera and projection math and global geometry
  setup. Matrix and blend pixel fixtures pass on ARM11. The field uses
  world-to-screen projection for its dialogue. The Black 2 replay reaches
  movement after the opening with these changes. The map transition trace
  still includes two direct reads of the DS VBlank counter.
- The compositor applies rectangular and OBJ window masks to backgrounds,
  sprites, and the 3D layer. It samples affine sprites with their OAM
  matrices and resolves BG and OBJ ties in DS priority order. Azahar shows
  the Striaton City location banner over the field without the previous
  white bands when continuing from a cartridge save. The ARM11 pixel
  fixture covers the masks and priority order with both renderer paths.
- Layer brightness follows each engine's BLDCNT/BLDY registers and
  the visible BG, OBJ, 3D or backdrop plane. Window effects masks can
  suppress it. Bag, Options and seasonal transitions use these registers.
  The Black 2 Azahar replay activates the Striaton banner, opens Options
  from the field menu and returns to the field.
- Both versions link the SPL particle manager, emitter, and behavior
  implementations. They issue geometry commands through the existing
  GX implementation. The Black 2 Azahar replay reaches the opening
  dialogue with the real engine linked.
- The game refreshes its RTC cache through the native 3DS clock. The host
  converts the clock to Nitro's epoch so saved timestamps and field time
  use the original two-digit year.

### Black 2 geometry and rasterization

- The linked game now uses NitroSystem's `NNS_G3dDraw` and material and
  joint command dispatch. Black 2's title sends model vertex commands to
  the host in Azahar. The FIFO decoder accepts packets split across
  multiple writes and tracks projection, position, vector, and texture
  matrices. The ARM11 fixture verifies packed transforms, stack restore,
  and matrix readback. A playthrough with the model path enabled reaches
  the house and verifies D-pad movement in all four directions.
- The ARM11 rasterizer assembles and clips primitives, samples DS texture
  and palette banks, and publishes a 256×192 layer at the GX buffer swap.
  The compositor resolves main BG0 and display capture into VRAM D.
  Azahar shows Kyurem and the blue title scene on the lower screen.
- The Nitro SDK's immediate geometry calls reach the same host interpreter
  as packed display lists. A synchronous, conservative GX box test reports
  visible bounds to the field chunk culler. The opening Aspertia City
  flyover now shows terrain, stairs, trees, and water in Azahar. The
  field renders the buildings, paths, and NPC during the first dialogue.
  The replay with the clipping shortcut clears the outdoor scene and
  reaches the rendered house interior (zone 428), where it ends during
  the opening dialogue. Free movement with the renderer still needs a
  check. The earlier replay checked movement in all four directions before
  field visibility was restored.
  An ARM11 fixture covers box visibility, a translated box, clipping,
  depth, palette lookup, and capture to VRAM D.
- The CPU renderer approximates translucent blending. Exact capture and
  fractional-alpha composition remain open. The PICA200 backend
  uses SoulSilver's `g3gpu.c` and `shaders/g3.v.pica` as references.
- Triangles fully inside the clip volume go straight to projection.
  Triangles outside a common clip plane are discarded. The existing
  clipping pass handles triangles that cross a plane. A synthetic Azahar
  run of 12,000 small on-screen triangles used about 45% fewer emulated
  ticks with the shortcut.

### Black 2 title audio

- The title's SDK stream call opens PCM16 stream 0 in the staged
  `swan_sound_data.sdat`. The core 2 audio worker reads and interleaves
  blocks into NDSP buffers and follows the game's stop and fade requests.
  It caches 8,192 frames per channel so a stereo track with separated
  channels needs fewer SD seeks.
  Azahar captures stereo output at 32,728 Hz and silence after Start fades
  the stream. The ARM11 stream fixture covers block boundaries, seek,
  looping, and a malformed header.
- Missing DSP firmware disables the stream player while leaving the game
  playable. The hardware test needs the console's DSP firmware at
  `sdmc:/3ds/dspfirm.cdc`. The hardware log confirms stream 0 starts.

### Display settings and audio integration

- L+R+Y opens the display menu on the lower LCD. Tap a row to toggle it,
  or use the D-pad and A. B closes the menu. Each setting starts off.
  Closing the menu after a change records the choices in the game's
  `display.cfg` on SD. The next launch restores them.
  The Circle Pad cannot move the player while the menu is open. A held
  stick stays blocked after the menu closes until it returns to neutral.
  LCD fit scales to 320×240 at 4:3 and maps bottom touches back to DS
  coordinates. Wide fill stretches the top image across 400×240. Sharp
  pixels selects point filtering for scaled screens.
- Azahar verifies the menu toggles and the ARM11 fixture checks the
  default and scaled touch bounds. Wider field visibility, per-glyph
  sharp text, and native-resolution geometry remain open.
- The presenter skips the game layer behind the settings menu when
  display capture does not need it. Centered and scaled presentation
  clear the margins and overwrite the picture once per frame.
- The NDSP wave player copies cry PCM to linear memory before queuing
  it. SDAT wave and file metadata are read in two blocks to avoid
  thousands of small SD seeks on the first cry. The game checks the
  wave archive and sample boundaries before playback. Field music and
  sound effects still need the sequence player.
- The same SDAT INFO cache supplies sequence IDs, bank assignments,
  player parameters and the banks' wave archive references. The port
  checks record and file ID bounds before handing data to the game.
- WaveOut decodes DS IMA-ADPCM to PCM16 and queues a one-shot intro
  followed by a looped sustain when a wave has a nonzero loop point.
  Azahar NDSP telemetry verifies the sustain buffer wraps. These sample
  playback features are available to the sequence player when it is
  implemented.

### Experimental PICA200 geometry

- Enable **GPU geometry (experimental)** under L+R+Y. The default is
  the CPU rasterizer. The host batches projected triangles and draws on
  a PICA200 color/depth target. It queues the transfer back to the
  256×192 layer so existing BG0 composition and title VRAM capture can
  consume it. The DS compositor and LCD scaling still run on ARM11.
- A bounded texture cache keeps PICA textures across frames. LCDC DMA,
  CPU transfers and display capture update VRAM page revisions. Cache
  entries track texels, compressed block descriptors and palettes. The
  title reuses all 15 textures on its second GPU frame. The fixture
  covers depth, clipping, indexed textures, palette cache hits and
  invalidation, and capture to VRAM D. Azahar shows Kyurem, the blue
  title scene and the opening Aspertia field through the GPU backend.
  The 120-frame Aspertia sample records 4,962 texture cache hits and
  58 uploads.
- PICA uploads reverse the tiled row order to retain DS texture
  orientation. Negative repeating UVs shift by a whole repeat period,
  including the two-period mirror case. The fixture checks an
  asymmetric texture on both axes and an updated 4×4 descriptor.
- The GX interpreter generates signed, fixed-point texture coordinates on
  TEXCOORD, NORMAL, or vertex submission, according to the material's
  mode. Both renderers consume these coordinates. The fixture checks
  both axes and verifies when the texture matrix is read.
- NORMAL shades vertices from the enabled GX lights and the current
  material, using the vector matrix, diffuse and ambient terms, emission,
  and the optional shininess table. Both renderers use those vertex colors.
- Mixed-alpha textures write depth for opaque texels. Fractional texels
  write depth when the polygon requests it. Textures with only one alpha
  class use one draw pass. LCDC uploads that overwrite a queued texture
  replay those triangles before the bytes change.
- Translucent polygons with the same GX ID contribute once per pixel
  until another ID or an opaque fragment replaces it. PICA stores the ID
  in stencil. Alpha-zero fragments leave the stencil and depth intact.
- GX decals use texel alpha to select the color and polygon alpha for
  coverage. PICA draws decals with zero or full texel alpha. Frames with
  fractional decal texels use software for the DS fixed-point blend.
- The host reads the GX alpha-test enable and reference at the start of
  each frame. Fragments at or below the reference are discarded. An
  enabled test with fractional polygon and texture alpha uses software
  to reproduce the DS's five-bit multiplication and cutoff.
- Toon shading samples the GX table with interpolated red intensity.
  Highlight mode adds the lookup color to the textured or untextured
  intensity. Fog uses the per-pixel flag, density and Z or W depth, and
  honors color-plus-alpha and alpha-only modes. The host resolves these
  effects in software when the GPU setting is enabled.
- Shadow mode uses a depth-fail mask and rejects the occluder's polygon
  ID. The CPU handles the shadow pass when the GPU setting is enabled.
- Edge marking selects the palette entry from the nearer polygon's ID
  at a depth boundary. The software pass colors those edges before fog.
  Dual-layer anti-alias coverage and exact depth testing remain open.
- Source A 3D capture copies the visibility bit with the color. Source
  A/B blending ignores contributions from invalid pixels. Capture
  offsets wrap within the destination bank. The synthetic fixture
  verifies these cases through both rasterizers and detects unintended
  CPU fallback. See [BUILDING.md](BUILDING.md#geometry-regression-fixture).
- GPU preparation can fall back to CPU rasterization for that frame.
  Hardware timing, 3D blending parity, direct GPU composition, wider
  field visibility and native-resolution render targets remain open.

### Audio follow-up — LLE unlock, title stream verified, wave player built (Nov 24–25, Ghost)

Observations from the cloud Azahar setup. The workstation captures used
the Windows Lotus worker and recorded the PCM waveform:

- "Azahar captures stereo output at 32,728 Hz" did **not** reproduce under
  HLE: `ndspInit` fails (`0xd880a7fa`) and the run stays at
  `audio_samples: 0`. A minimal NDSP probe homebrew (textbook `ndspInit`
  + 440 Hz sine) fails identically, so this is the emulator, not the
  game. No ARM11 stream fixture exists anywhere in the repo (grep: zero
  hits), so that claim cannot be traced to an artifact on disk.
- "Silence after Start fades the stream" did **not** reproduce either:
  samples kept climbing after the START press (2.99M → 3.13M → 3.62M).
  The title music plays through title → start menu.
- What unlocks audio is **LLE DSP with real firmware**: the session
  creation check demands `sdmc/3ds/dspfirm.cdc`. It fires before any
  prestage can happen, so the working setup is a persistent Lotus card
  `b2w2-dsp` (driver `setup-dsp-card.py` in `~/workspace/toolchain/lotus/`,
  firmware at `cards/b2w2-dsp/sdmc/3ds/dspfirm.cdc`) and runs pass
  `--audio-emulation 1 --card b2w2-dsp` explicitly; the global stays at 0
  (HLE default). Correct prestage path is `--prestage
  ~/workspace/pbw2-sd/3ds` (the `…/pbw2-sd` form lands the file one level
  too deep). The firmware is Nate's own DSP1 dump — it stays in the
  workspace and the Lotus card only, **never in the repo**.
- Verified sounding: stream 0 = `STRM_TITLE`, 32,728 Hz, 3,619,310
  samples over a 120 s title run, 0 dropped. A 620 s playthrough (Test B)
  reached Aspertia City mid-dialogue with Bianca, 17,957,086 samples, 0
  dropped, zero exceptions — and the title stream is the **only** audio
  that sounds in the whole run.
- Named inventory from the SDAT SYMB block (Test B, 167 AUDIO log
  lines): BGM sequences requested but silent — SEQ 1171
  `SEQ_BGM_GF_LOGO`, SEQ 1004 `SEQ_BGM_TITLE01`, SEQ 1327 `SEQ_ME_CALL`,
  SEQ 1104 `SEQ_BGM_SW_C_12`, SEQ 1102 `SEQ_BGM_SW_DEMO_OPENING`, SEQ
  1009 `SEQ_BGM_STARTING2` (SEQ 1008 `SEQ_BGM_STARTING` loaded, never
  started). SE sequences requested but silent — SEQ 1351
  `SEQ_SE_MESSAGE` x43 (the dialogue text blip, the dominant SE), SEQ
  1356 `SEQ_SE_DECIDE1` x9, SEQ 1357 `SEQ_SE_DECIDE2` x3, SEQ 1352
  `SEQ_SE_SELECT1` x2, SEQ 1387 `SEQ_SE_KON` x2, SEQ 1383 `SEQ_SE_BOWA2`
  x2, SEQ 1382 `SEQ_SE_NAGERU` x2, SEQ 1365 `SEQ_SE_OPEN2` x1, SEQ 2036
  `SEQ_SE_TOUJOU_INTRO` x1, SEQ 1656 `SEQ_SE_FLD_07` x1. Waves
  attempted: WaveArc 687 `WAVE_ARC_PV653_BLACK` (title cry, Black
  Kyurem), WaveArc 573 `WAVE_ARC_PMWB_534` (Cinccino cry in the opening).
- Wave player is built (`n3ds/nitro/snd_arc.c` — real SDAT INFO/FAT
  parsing; `n3ds/nitro/wave_out.c` — `NNS_SndWaveOut*` on NDSP, DS ch N
  → NDSP ch N+1, stream owns 0, PCM8/PCM16 one-shots, volume 0..127 +
  pan 0..127). The full chain runs end to end: SDAT parses (2290 wave
  arcs, 5197 files), waveArc 687 → fileId 3936, 21,652 bytes read,
  `WaveOutStart` PCM8 21,576 samples @ 11,025 Hz, NDSP ch 16, buffer
  reaches DONE, menu reached clean, zero Azahar errors. **But the cry's
  audible duration is UNVERIFIED**: the buffer completes in ~198 ms
  instead of the expected ~1,956 ms, and `samplePos` reads 0 once the
  buffer is DONE, so how much audio actually played cannot be told from
  the log. Concrete next step: use the `audio_capture` lotus_debug
  operation on a live session to measure the cry's duration/pitch in
  the real waveform.
- Honest-failure rules the player follows: `Start` FALSE / `IsPlaying`
  FALSE when NDSP is down; `Stop` / `WaitForChannelStop` / `FreeChannel`
  tolerate NULL and never-started handles (the cry release path fires
  even when nothing started). `snd_silent.c` keeps only the still-silent
  parts (sequences + `GetSeqInfo`/`GetBankInfo`/`GetSeqParam` dummies);
  `GetWaveArcInfo` / `GetFileSize` / `ReadFile` are real now. The five
  void WaveOut stubs left `link_stubs.c`; the regen script's skip set
  covers all eight WaveOut symbols.
- **Next job: the sequence player.** It is the whole of BGM + every SE.
  Open question unchanged: both `SEQ_BGM_TITLE01` and `STRM_TITLE` are
  requested at the title and only the stream sounds — whether the
  sequence is the same melody layered, a fallback, or something else
  still needs the sequence player to answer. ALSA capture is a dead end
  for Lotus sessions (the worker forces the bounded sink); verify sound
  via session telemetry (`audio_samples > 0`, `audio_rate ≈ 32728`,
  `audio_hash_fnv1a64` moving off the empty basis) or `audio_capture`.

### Muse handoff — Nov 24, 2026

Nate handed this job to GPT/Codex on a big model, working from GitHub main.
This snapshot pushes the local `ghost/b2w2-working` tip (4692909fd + this
note) to main as one commit; the full 31-commit history stays on that
local branch.

#### Original field checkpoint
- Branch tip: `ghost/b2w2-working` @ 4692909fd ("Slice 2j: finer logging
  around GFL_G3DSysCreate"), tree clean. The 2j change is diagnostic
  logging only — no behaviour change.
- Slice 2j build VERIFIED GOOD: 8,881,396 bytes, SHA1
  69e339f43f500e353946f2fa515727a2a2a713b0, reloc check OK.
- Boots → title → intro → boy/girl pick → both name entries → field proc
  START (CallCreateGameEntryPointEvent) → FieldmapProc_Init COMPLETES
  (Field_Create done, zone 427 = Aspertia City, area 210) at 60 fps,
  0 Undefined Instruction exceptions.
- Field Slice 1 VERIFIED: six GameData field subsystems (EventData,
  MapMatrix, FldActSys, EncountState, FieldFollow, Calendar) are real
  game code, zero stub hits.
- Intro-exit crash (VCOUNT never emulated) + post-intro halt
  (GFL_HeapFree(NULL)) fixed and verified in playthrough 12.
- Title logo renders CLEAN and correct. Opening demo renders (Oshawott
  in Aspertia City). Logo placement + double Title_Init VERIFIED FAITHFUL
  (logo-on-bottom early is correct; switch to MAIN_SUB at title frame
  1740; the real game's attract loop re-runs boot_proc).

#### Still open (priority order)
1. **Field play:** Black 2 responds to the D-pad after the opening
   transition and dialogue. Verify field rendering and controls on a
   physical New 3DS.
2. **3D parity and speed:** Kyurem renders. Verify the field across maps,
   complete the missing GX effects, and measure frame time on hardware
   with both rendering backends.
3. **QOL: still ZERO ported** (inventory: 55 features, 49 apply; the
   inventory lives outside this repo in the cloud workspace). Order once
   the field walks: settings first, cheap host-side items, wide/sharp
   after the replay.

#### Original Slice 2j verification recipe
Azahar, --new3ds, --prestage <sd-stage>/3ds, 620 s, --hold 1,
--press "$(cat slice2_press.txt)" (123 presses: A every 6 s + START
every 24 s to 360 s, then D-pad walk cycle + A every 10 s, 380-615 s),
out runs/b2w2-slice2j-verify. The 2j [field] lines pinpoint the hang
inside FieldRoutine_Open. Read the guest log at
session-scratch/sdmc/3ds/black2_3ds/log.txt; status.jsonl is the
reliable progress source (driver log is buffered). Continuous mode
captures ONLY once, at the end — the end frame of a 620 s run is the
white fade; use short runs for title captures.

#### Pickup notes
- Build: PBW2_LINK_GAME=1 bash scripts/n3ds_build.sh -j2 b2, with
  PATH=/opt/devkitpro/devkitARM/bin:$PATH prefixed (reloc-check calls
  arm-none-eabi-readelf bare). ~8-10 min at -j2. Closure:
  scripts/n3ds_link_closure.py (~2.5 min, needs nm on PATH).
  Toolchain: devkitARM 16.1.0; ROMs BYO-ROM (black2.nds IREO SHA1
  e51e6dfb..., white2.nds IRDO). Do NOT run n3ds_regen_link_stubs.sh
  blindly (rewrites link_stubs.c, destroys hand-written comments and
  typed void-stubs).
- Emulator: Lotus run-game.py (Azahar backend; is_new_3ds=true;
  Citra button order — START is 1024, not 8; lotus_input needs owner
  "live"). Lotus aborts B2W2 on both backends at the New 3DS speedup
  request (ThreadManager assertion) — use Azahar. Chunk mode reaches
  the title slower than continuous mode at the same frame count — do
  not mix frame counts.
- Hard rules: never commit ROMs/assets/saves/builds/logs; save format
  unchanged, only the game writes .sav; every new gameplay feature
  default-off in Settings (L+R+Y); no randomizer/cheats/fast-forward;
  game never resets/relaunches; no CIA; New 3DS only. Nate's bar:
  proper native from the start, no stubs pretending to work, tested
  in the emulator before it counts; hardware is the final word.
- Slice 2 closure ended at 1256 objects (from 277). The online stack
  is stubbed out (DWC/WM/IR/camera/DSP/GDS void stubs + honest
  GFL_Net "no network" returns): wifi features cannot connect; the
  field-walk milestone does not cover online play.
- After ANY VM restart: re-link /opt/devkitpro ->
  ~/workspace/toolchain/devkitpro, then sudo dpkg -i
  /var/cache/apt/archives/*.deb once the os-intent replay releases
  the dpkg lock (~4 min). 7 restarts so far; ~/workspace survives,
  /tmp does not.

## 1. Goal and rules

A native New 3DS port of Pokémon Black 2 and White 2 (US/EU). The decompiled
C runs on the ARM11. A platform layer owns the 3DS GPU, input, audio, and
where the save file lives. Personal, experimental, GPL-3.0. Owner: GitHub
`unowncollectables`. The repo is **public** (since Nov 23, 2026).

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
shim, NDSP streaming, present). With `PBW2_LINK_GAME=1`, `Pbw2_GameMain`
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
| `g3_pica.c` | Experimental Citro3D triangle batches, texture cache and readback |
| `g3d_glb_stubs.c` | First-call stubs: `NNS_G3dDraw`, TexLoad/Bind, GetMdlSet, MTX |
| `game_present.c` | LINK_GAME present; default path must stay r15-identical |
| `overlay_host.c` | Overlay load from `sdmc:/3ds/<game>_3ds/overlays/` |
| `fs_host.c` | Path FS under staged `nitrofs/` |
| `pad_host.c` | Input. Must not self-reset |
| `save_shim.c` | Stores cartridge flash ranges on SD when requested by the game |
| `settings.c` | L+R+Y Settings. No gameplay toggles yet; do not write `config.txt` until a real default-off toggle exists |

Display / POWCNT (keep this polarity):

- Official NDS and this port: `GX_DISP_SELECT_MAIN_SUB` (bit 15 set,
  `pow & 0x8000`) → main engine on the **top** 3DS LCD.
- `GX_DISP_SELECT_SUB_MAIN` (bit 15 clear) → sub engine on top, main on
  the bottom.
- Compose: `main_upper = (pow & 0x8000) != 0`.
- Copyright and CDEMO use `MAIN_SUB`. `Title_Init` uses `SUB_MAIN` (2D
  logo on the bottom). After 1740 frames the title
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
for motion blur. Both geometry backends feed that capture in Azahar.

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
2D compositor already reads. A hardware retest passed that fade and
reached start-menu setup after leaving the title screen.

The linked build passes START to the game. The shell-only build still
uses START to quit.

Immediate Nitro DMA fills and copies now use host memory transfers. The
linked build also uses the existing NitroSystem model and animation
resource code instead of placeholder allocators and accessors.
Black 2 reached `Title_Init` and entered the title loop in Azahar with
zero unmapped accesses in the title-loop smoke trace.

The hardware run then aborted in `StartMenu_SetBlend`, where
`G2x_SetBlendAlpha_` wrote to the DS `BLDCNT` address (`0x04000050`).
The shared GX blend helpers now translate both engines' register
addresses through `pbw2_io`. Retest the start-menu transition on hardware.

Linked builds keep diagnostics in the SD log and initialize the screens
without a text console. The first game frame takes over both screens,
and presentation continues through display-off and layer-off transitions
so a swap cannot reveal an older boot framebuffer.

1. **2D presentation.** Black 2's title prompt is legible in Azahar after
   the OBJ tile indexing fix. Verify the title, menu, and transitions on
   New 3DS hardware and run the same checks on White 2.
2. **3D presentation.** The CPU renderer and display capture show Kyurem
   in Azahar. Compare lighting and capture to the DS title, verify the
   field, and profile the renderer on New 3DS. SoulSilver and Platinum
   remain the references for Citro3D acceleration.
3. **Audio.** The linked build streams the title's PCM16 music with NDSP.
   The ARM7 sequence player, sound effects, and cries remain open.
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
5. Calls the [HOME Menu art helper](BUILDING.md#cia) with the matching
   ROM and passes the generated icon and banner to its CIA packaging step.

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
calibration. The linked game's backup calls use `save_shim.c` for its SD
save. The lower-level PXI test path still uses an in-memory shadow.

Experimental `OS_Init` on New 3DS (`PBW2_LINK_GAME` shell `OS_Init`):
`PbW2_OsThreadHostInit`, `PXI_Init`, `PbW2_RomHeaderHostInit`,
`PbW2_DtcmHostInit`, `PbW2_NvramHostInit`, `CARD_Init` (resource lock,
PXI FS callback, **no** CARD worker thread).

### Remaining compatibility work

- SDAT sequence playback for field music, fanfares, and sequenced effects.
- DS 3D dual-layer coverage and exact display capture blending.
- DSi/TWL and the two DSP firmware images (`rom/dsp/*.inc`, ROM-backed,
  not in git).
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
