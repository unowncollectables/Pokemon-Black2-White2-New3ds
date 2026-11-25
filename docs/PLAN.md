# Plan

Black 2 leads. White 2 is the same tree at every phase, built in the same
change. The matching DS rebuild keeps working. Saves stay the cartridge
format and only the game writes them. New features default off in Settings
(L+R+Y). No randomizer, cheats, or fast-forward. The game does not reset or
relaunch itself.

New 3DS only: 804 MHz, L2, core 2 for audio and IO, C-stick, ZL/ZR. About 32
threads, and every worker is joined on exit. Sharp text, once it exists,
presents with the frame's screen flags.

## Boot

The production shell in `n3ds/` asks for the New 3DS clock and starts
`Pbw2_GameMain` when the game is linked. Black 2 runs the original boot,
title, and New Game flow in Azahar. Physical hardware and White 2
playthroughs remain open. Black 2 streams the title music with NDSP.

The user's ROM is read at runtime for the filesystem, and at build time only
by `python3 configure.py && ninja` for the matching DS ROM. It is not committed.

## Video

The host composites both DS 2D engines and the CPU-rendered geometry layer.
The Black 2 title uses the same VRAM capture and display path as the DS game.
Finish geometry effects, compare with DS rendering, and measure frame
timing on hardware. The sister ports are references for PICA200
acceleration, presentation order, and thread lifetime. Settings offers
LCD fit, wide fill and point-filtered scaling. Wider field visibility,
per-glyph text and higher-resolution geometry follow the baseline renderer.

## Save and field

The game writes `black2.sav` or `white2.sav` in its own SD directory. The
platform opens that file only when the game asks, and does not create it on
boot. Black 2 reaches the opening field and responds to the D-pad after the
dialogue. Verify travel between maps, menus, and Continue. Continue must
round-trip the same bytes a DS cart or an R4 would.

## Battle

Wild battles and trainer battles through the game's own battle code. No
speed toggle until one is designed as a default-off Settings row. No damage
multipliers and no encounter editor.

## Audio

Title PCM16 runs on the core 2 NDSP worker, which joins before
`ndspExit`. PCM cries play from the game's wave archives. A host player
for the ARM7 sequenced music and effects remains open.

## Story

Play from the title screen through the opening and the first towns, both
games, without the port skipping a script or writing the save except when
the game saves.

## Comms

Local play over 3DS UDS, using the game's own link protocol. C-Gear,
Entralink, Game Sync, and the other wireless features are part of the game,
not extras: they get a replacement server only where the original server is
gone, and they stay the game's own flows. No account system in the port.
Online work does not touch the save except through the game's own commands.

## Quality

Keep the original boot and game progression as the baseline. Use
SoulSilver's shipped `kCfgDefaults` in `platform/psp_ctru.c` for compatible
Settings options. Keep new options off until verified in B2W2.
Start with its 60 fps presentation, wide layouts, sharp text, even audio,
and controls. Use the corresponding Gen 5 flows for party and PC access,
Repel prompts, and other gameplay options. The Gen 5 save and reset rules
still apply to any shortcut.

Initial candidates:

- Sharp text, both screens, using the frame's present flags.
- Wide view.
- Followers.
- Shiny tracker, in its own file, not in the `.sav`.
- Mobile PC, opened by the player, never an automatic save and never a relaunch.
- Bottom-screen apps that hide while the game is using the touch screen itself.

## Polish

Timing, the settings text, logging, and the CIA banner. The owner builds the
CIA with their own builder from the `.elf` and `n3ds/cia/*.rsf`. Home Menu
banners are a later asset, not this phase. No version bump by hand.
