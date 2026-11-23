# Third-party code credits

## DS 2D compositor (B2W2 New 3DS port)

The software 2D engine compositor in `n3ds/nitro/ds2d_compose.c` adapts priority, text-BG,
and sprite drawing ideas from **widecomp.c** in
[unowncollectables/3ds-platinum](https://github.com/unowncollectables/3ds-platinum)
(GPL-3.0). This port reimplements that logic for the decomp-linked host without using
melonDS or Citro3D.