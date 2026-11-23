# AGENTS.md

Start here, then read [docs/HANDOFF.md](docs/HANDOFF.md) before changing
anything. The phase list is [docs/PLAN.md](docs/PLAN.md). If a doc disagrees
with the code, trust the code and fix the doc in the same change.

The matching decomp's own notes are [docs/DECOMP_AGENTS.md](docs/DECOMP_AGENTS.md)
and [CLAUDE.md](CLAUDE.md). They still apply when you edit `src/` or `lib/`
without `PBW2_N3DS`.

## Goal

A native New 3DS port of Pokémon Black 2 and White 2 (US/EU). Personal use,
private repo, GPL-3.0. Owner: GitHub `unowncollectables`. Black 2 leads;
White 2 is built from the same tree in the same change. The DS matching
rebuild stays.

## Hard rules

1. Never commit ROMs, assets extracted from a ROM, saves, builds, logs, or
   personal information (`.nds`, `extract/`, `.sav`, `.3dsx`, `.cia`, `.elf`,
   log dumps). The user supplies their own dump. Never download a ROM.
2. Keep the repo private. Do not force-push. Merges need the user's yes.
3. The save format does not change. Only the game writes a `.sav`. The
   platform must not create one, and it must not write one on boot, exit, or
   from a settings toggle.
4. Every new gameplay feature is a default-off toggle in the in-game Settings
   screen (L+R+Y). Risky features stay off until the user turns them on. No
   randomizer, cheats, or fast-forward. The game does not reset or relaunch
   itself.
5. Credit reused GPL code in [n3ds/THIRD_PARTY.md](n3ds/THIRD_PARTY.md).
6. Do not build or commit a CIA. The user builds installers with their own
   Builder. Do not bump a version by hand.
7. New 3DS only (804 MHz, L2, core 2). Do not design around Old 3DS limits.

Full box layout, build commands, architecture, history, CI, and next steps:
[docs/HANDOFF.md](docs/HANDOFF.md).
