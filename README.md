# Pokémon Black 2 / White 2 for New 3DS

A personal, New 3DS-only native port of Pokémon Black 2 and White 2. The game's
decompiled C is [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2)
at `345b790e6000869b481b79838bb5727309f832fb` (GPL-3.0), imported with its
history. Both games stay one tree. The matching DS rebuild is unchanged except
for the delink path fix described below.

This is not a ROM, a CIA, or an emulator. You supply your own US/EU dumps.
Nothing in git is a cartridge image.

## What builds here without a ROM

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
bash scripts/n3ds_build.sh
```

That writes:

- `n3ds/build/b2/black2_3ds.elf` and `.3dsx`
- `n3ds/build/w2/white2_3ds.elf` and `.3dsx`

Both shells link. They are the platform, not the game's logo or title screen.
See [docs/HANDOFF.md](docs/HANDOFF.md) for what is stubbed and what still blocks
the title screen, and [docs/BUILDING.md](docs/BUILDING.md) for the exact steps.

The same command compiles every game C file for Black 2 and for White 2
(1218 of 1218 each). Those objects are not linked into the shell yet.

## What needs your ROM

Matching DS rebuild, from the decomp. Put your own dumps at:

| Game | File | SHA1 |
| --- | --- | --- |
| Black 2 (US/EU, NDSi enhanced) | `orig/baserom_b2_us.nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` |
| White 2 (US/EU, NDSi enhanced) | `orig/baserom_w2_us.nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` |

```sh
python3 configure.py
ninja
```

`configure.py` downloads the compiler tools (MWCC via wibo, dsd). `ninja`
extracts the ROM locally, rebuilds both games, and checks the SHA1s.
`orig/`, `extract/`, and `build/` are gitignored. Do not commit them.

At the imported commit, each `config/{b2,w2}_us/arm9/delinks.txt` named 66
sources that had moved (`src/system/savedata/`, `src/system/iss/`, `src/pml/`).
`ninja` stopped with a missing `build/b2_us/src/system/save_table.o`. Those
paths now point at the files.

## CIA

This tree does not build or ship a CIA. `n3ds/cia/black2.rsf` and
`n3ds/cia/white2.rsf` are the HOME Menu metadata: New 3DS, 804 MHz, L2 cache,
core 2. A banner and an icon are still required before `makerom` can pack a
CIA. Build that with your own Pokemon 3DS Builder, from the `.elf` above.

| Game | Title ID | Product |
| --- | --- | --- |
| Black 2 | `000400000F7D4200` | `CTR-H-B23D` |
| White 2 | `000400000F7D4300` | `CTR-H-W23D` |

Saves, when the game itself writes them, are `sdmc:/3ds/black2_3ds/black2.sav`
and `sdmc:/3ds/white2_3ds/white2.sav`. The shell does not create or write them.

## Docs

| Doc | What it is |
| --- | --- |
| [AGENTS.md](AGENTS.md) | Rules and file map for the next session |
| [docs/HANDOFF.md](docs/HANDOFF.md) | What works, what is stubbed, what blocks title |
| [docs/PLAN.md](docs/PLAN.md) | Phases from boot through polish |
| [docs/BUILDING.md](docs/BUILDING.md) | Commands, with and without a ROM |
| [THIRD_PARTY.md](THIRD_PARTY.md) | Credits |
| [docs/building.md](docs/building.md) | The decomp's own build notes |

## License

[GNU GPL v3.0](LICENSE). The decomp's headers stay. New port files are GPL-3.0
as well.
