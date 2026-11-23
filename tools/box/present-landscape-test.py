#!/usr/bin/env python3
"""Offline check for Present_BlitRGB565-style centering (top 72,24; bottom 32,24)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path


def blit_column_major(fb_stride: int, land_w: int, pic: list[int], pic_w: int, pic_h: int) -> list[int]:
    ox = (land_w - pic_w) // 2
    oy = (fb_stride - pic_h) // 2
    fb = [0] * land_w * fb_stride
    for py in range(pic_h):
        for px in range(pic_w):
            sx = ox + px
            sy = oy + py
            idx = sx * fb_stride + (fb_stride - 1 - sy)
            fb[idx] = pic[py * pic_w + px]
    return fb


def min_landscape(fb: list[int], fb_stride: int, land_w: int) -> tuple[int, int]:
    best_x = land_w
    best_y = fb_stride
    for idx, value in enumerate(fb):
        if value:
            sx = idx // fb_stride
            sy = fb_stride - 1 - (idx % fb_stride)
            best_x = min(best_x, sx)
            best_y = min(best_y, sy)
    return best_x, best_y


def main() -> int:
    rgb = Path("/tmp/title_top.rgb565")
    if not rgb.is_file():
        print("skip: need /tmp/title_top.rgb565 from a title_pic_host run", file=sys.stderr)
        return 0
    raw = rgb.read_bytes()
    pic = list(struct.unpack("<" + "H" * (len(raw) // 2), raw))
    fb = blit_column_major(240, 400, pic, 256, 192)
    x, y = min_landscape(fb, 240, 400)
    if (x, y) != (72, 24):
        print(f"top: expected (72, 24), got ({x}, {y})")
        return 1
    fb_b = blit_column_major(240, 320, pic, 256, 192)
    x, y = min_landscape(fb_b, 240, 320)
    if (x, y) != (32, 24):
        print(f"bottom: expected (32, 24), got ({x}, {y})")
        return 1
    print("present-landscape-test: ok (top 72,24 bottom 32,24)")
    return 0


if __name__ == "__main__":
    sys.exit(main())