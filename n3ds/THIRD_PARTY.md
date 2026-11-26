# Third-party references (N3DS port)

| Component | Source | License |
|-----------|--------|---------|
| DS GXFIFO intercept and packed command layout | [Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) `soulsilver3ds/platform/g3packet.c` | GPL-3.0-or-later |
| PICA200 batching, viewport mapping and queued readback with cache-line prefetch | [Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) `soulsilver3ds/platform/g3gpu.c` and `platform/shaders/g3.v.pica` | GPL-3.0-or-later |
| Command-list-only flush with explicit vertex and texture cache maintenance (reference) | [Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) `soulsilver3ds/platform/g3gpu.c` and `gpuscale.c` | GPL-3.0-or-later |
| Nearest-then-linear sharp scaling (reference) | [Pokemon-HeartGold-SoulSilver-New3ds](https://github.com/unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds) `soulsilver3ds/platform/gpuscale.c` | GPL-3.0-or-later |
| GX lighting arithmetic and specular behavior (reference) | [melonDS GPU3D.cpp](https://github.com/melonDS-emu/melonDS/blob/master/src/GPU3D.cpp), including azusa's hardware research | GPL-3.0-or-later |
| GX toon, highlight, fog, shadow and edge behavior (reference) | [melonDS GPU3D_Soft.cpp](https://github.com/melonDS-emu/melonDS/blob/master/src/GPU3D_Soft.cpp) | GPL-3.0-or-later |
| 2D brightness factors and rounding (reference) | [melonDS GPU_ColorOp.h](https://github.com/melonDS-emu/melonDS/blob/master/src/GPU_ColorOp.h) | GPL-3.0-or-later |
| DS IMA-ADPCM predictor and loop behavior (reference) | [melonDS SPU.cpp](https://github.com/melonDS-emu/melonDS/blob/master/src/SPU.cpp) | GPL-3.0-or-later |
| Citro3D / wide compositor patterns | [3ds-platinum](https://github.com/unowncollectables/3ds-platinum) (reference; not vendored) | GPL-3.0 |
