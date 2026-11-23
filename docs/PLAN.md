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

The shell in `n3ds/` links for both games and asks for the New 3DS clock.
`n3ds/nitro/` has OS, IRQ, arenas, pad bits, RTC, a path filesystem, and
`GX_Init` that clears the host IO buffer. When NitroFS is staged, the shell
draws the version logo over the title background from `a/0/2/6`, and the
lower title screen under it.

Still to do before the title process: confirm that picture in Azahar, then
link `src/system/main.c` as `Pbw2_GameMain` and overlays 12 and 27. The shell
keeps `main`. The other overlays follow. DSi entry points stay stubs. ARM7
audio stays silent.

The user's ROM is read at runtime for the filesystem, and at build time only
by `python3 configure.py && ninja` for the matching DS ROM. It is not committed.

## Video

DS 2D engines and the geometry engine, presented on the 3DS. Both screens use
one `Present_Commit` per frame. Wide view and sharp game text come later, in
Quality, and they use those same flags. The sister ports' renderer is the
lesson for present order and thread lifetime, not the function table: this
game calls Nitro GX, not pspoke.

## Save and field

The game writes `black2.sav` or `white2.sav` in its own SD directory. The
platform opens that file only when the game asks, and does not create it on
boot. Field movement, menus, and the overworld come after the title screen
can be passed. Continue must round-trip the same bytes a DS cart or an R4
would.

## Battle

Wild battles and trainer battles through the game's own battle code. No
speed toggle until one is designed as a default-off Settings row. No damage
multipliers and no encounter editor.

## Audio

A host player on NDSP for the ARM7's sequenced music and sound effects. The
audio thread is already on core 2 and is joined before `ndspExit`. Silent
remains the default until the player is correct, not a fast wrong mix.

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

Each of these is a Settings toggle, default off, until it has been played:

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
