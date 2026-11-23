# Third-party code and credits

Per-file credits stay in the decomp's headers. This is the overview.

- [fuddlesworth/pokebw2](https://github.com/fuddlesworth/pokebw2) (GPL-3.0),
  commit `345b790e6000869b481b79838bb5727309f832fb`. The matching decompilation
  of Pokémon Black 2 and White 2, imported with its history. Its own credits
  (swan, pret, NitroSDK reconstructions, ds-decomp, and the rest) are in the
  upstream README at that commit and in [docs/code-organization.md](docs/code-organization.md).
- [unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds)
  at `4d4d2cb`, and [unowncollectables/3ds-platinum](https://github.com/unowncollectables/3ds-platinum).
  The New 3DS runtime lessons used here: join every worker before GPU and NDSP
  teardown, present sharp text with the frame's screen flags, do not write the
  save from the platform, 804 MHz / L2 / core 2 in the RSF. The RSF text is
  adapted from the SoulSilver port's `soulsilver3ds.rsf`, which itself credits
  GabeSud0/pokeplatinum-3ds (GPL-3.0). No SoulSilver or Platinum renderer
  sources are copied; those speak a different API.
- [CallanCampos/Frigate](https://github.com/CallanCampos/Frigate) (GPL-3.0).
  The native PC/Mac port of this decomp. Used as the reference for which Nitro
  modules a host replaces (`port/hal/os`, `rtc`, `fs`, `gx`). `n3ds/nitro/` is
  written for the 3DS. No Frigate source is copied.
- [devkitPro/libctru](https://github.com/devkitPro/libctru),
  [devkitPro/devkitarm-crtls](https://github.com/devkitPro/devkitarm-crtls),
  and [devkitPro/3dstools](https://github.com/devkitPro/3dstools). The 3DS
  toolchain the shell links against. Not vendored.
- New files under `n3ds/` and `scripts/n3ds_*.py` are GPL-3.0, same as the
  decomp.
