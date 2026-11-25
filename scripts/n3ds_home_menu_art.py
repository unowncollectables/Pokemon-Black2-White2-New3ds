#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare the 3DS HOME Menu art from a locally supplied Black 2 or White 2 ROM."""

import argparse
import io
from pathlib import Path
import struct
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "data"))
from narc import read_narc
from nitro_2d import palette_colors, resource_data, write_rgba_png

GAME_NAMES = {b"IREO": "Black 2", b"IRDO": "White 2"}
TITLE_ARCHIVE = (b"a", b"0", b"2", b"6")


def read_at(rom, size, offset, length):
    if length < 0 or offset < 0 or offset + length > size:
        raise ValueError("ROM section is out of bounds")
    rom.seek(offset)
    data = rom.read(length)
    if len(data) != length:
        raise ValueError("ROM section is truncated")
    return data


def nitro_file(rom, size, header, path):
    fnt_offset, fnt_size, fat_offset, fat_size = struct.unpack_from("<4I", header, 0x40)
    fnt = read_at(rom, size, fnt_offset, fnt_size)
    directory = 0
    for depth, component in enumerate(path):
        cursor, next_file, _ = struct.unpack_from("<IHH", fnt, directory * 8)
        while cursor < len(fnt) and fnt[cursor]:
            flags = fnt[cursor]
            length = flags & 127
            cursor += 1
            name = fnt[cursor:cursor + length]
            cursor += length
            if flags & 128:
                child = struct.unpack_from("<H", fnt, cursor)[0]
                cursor += 2
                if name == component and depth < len(path) - 1:
                    if (child & 0xf000) != 0xf000:
                        raise ValueError("invalid NitroFS directory ID")
                    directory = child & 0xfff
                    break
            else:
                if name == component and depth == len(path) - 1:
                    if next_file * 8 + 8 > fat_size:
                        raise ValueError("NitroFS file ID is out of bounds")
                    entry = read_at(rom, size, fat_offset + next_file * 8, 8)
                    start, end = struct.unpack("<II", entry)
                    return read_at(rom, size, start, end - start)
                next_file += 1
        else:
            raise ValueError("the title archive a/0/2/6 is missing")
    raise ValueError("the title archive is missing")


def icon_png(banner):
    colors = []
    for index, color in enumerate(struct.unpack_from("<16H", banner, 0x220)):
        rgb = tuple(((color >> shift) & 31) * 255 // 31 for shift in (0, 5, 10))
        colors.append(bytes(rgb + (255,) if index else (24, 27, 32, 255)))
    icon = [[None] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            tile = y // 8 * 4 + x // 8
            packed = banner[0x20 + tile * 32 + (y % 8) * 4 + (x % 8) // 2]
            icon[y][x] = colors[(packed >> (4 * (x & 1))) & 15]
    scaled = b"".join(icon[y * 32 // 48][x * 32 // 48] for y in range(48) for x in range(48))
    return write_rgba_png(48, 48, scaled)


def banner_png(archive):
    # The title initializes sub BG3 with NCGR 0, NSCR 1 and NCLR 2.
    characters, screen, palette = read_narc(archive)[:3]
    chars = resource_data(characters, b"RGCN", b"RAHC")
    screen = resource_data(screen, b"RCSN", b"NRCS")
    colors = [bytes((*color, 255)) if index else bytes(4)
              for index, color in enumerate(palette_colors(palette))]
    width, height = struct.unpack_from("<HH", screen, 24)
    tile_size = struct.unpack_from("<I", chars, 40)[0]
    if (width, height) != (256, 256) or len(colors) != 256 or tile_size % 64 or 48 + tile_size > len(chars):
        raise ValueError("unexpected title graphics format")
    tiles = memoryview(chars)[48:48 + tile_size]
    pixels = bytearray(256 * 192 * 4)
    for tile_y in range(24):
        for tile_x in range(32):
            entry = struct.unpack_from("<H", screen, 36 + 2 * (tile_y * 32 + tile_x))[0]
            index = entry & 1023
            if (index + 1) * 64 > len(tiles):
                raise ValueError("title tile index is out of bounds")
            for y in range(8):
                for x in range(8):
                    source_y = 7 - y if entry & 0x0800 else y
                    source_x = 7 - x if entry & 0x0400 else x
                    color = tiles[index * 64 + source_y * 8 + source_x]
                    dest = ((tile_y * 8 + y) * 256 + tile_x * 8 + x) * 4
                    pixels[dest:dest + 4] = colors[color]
    visible_rows = [y for y in range(192) if any(pixels[y * 1024 + 3:(y + 1) * 1024:4])]
    if not visible_rows:
        raise ValueError("the title logo is empty")
    top, bottom = visible_rows[0], visible_rows[-1] + 1
    if bottom - top > 128:
        raise ValueError("the title logo exceeds the HOME Menu banner height")
    result = bytearray(256 * 128 * 4)
    dest = (128 - (bottom - top)) // 2 * 256 * 4
    result[dest:dest + (bottom - top) * 256 * 4] = pixels[top * 256 * 4:bottom * 256 * 4]
    return write_rgba_png(256, 128, result)


def silent_wav():
    data = io.BytesIO()
    with wave.open(data, "wb") as file:
        file.setparams((1, 2, 22050, 0, "NONE", "not compressed"))
        file.writeframes(bytes(22050))
    return data.getvalue()


def make_assets(rom_path, out, expected_game=None, bannertool=None):
    out = out.resolve()
    if out.is_relative_to(ROOT) and not out.is_relative_to(ROOT / "n3ds" / "build"):
        raise ValueError("write HOME Menu art outside the checkout or under n3ds/build")
    with rom_path.open("rb") as rom:
        size = rom_path.stat().st_size
        header = read_at(rom, size, 0, 0x200)
        game = GAME_NAMES.get(header[12:16])
        if game is None or (expected_game is not None and game != expected_game):
            raise ValueError("expected a supported English Black 2 or White 2 ROM for this target")
        banner_offset = struct.unpack_from("<I", header, 0x68)[0]
        if not banner_offset:
            raise ValueError("the ROM has no cartridge banner")
        icon = icon_png(read_at(rom, size, banner_offset, 0x240))
        banner = banner_png(nitro_file(rom, size, header, TITLE_ARCHIVE))
    out.mkdir(parents=True, exist_ok=True)
    (out / "icon.png").write_bytes(icon)
    (out / "banner.png").write_bytes(banner)
    (out / "banner.wav").write_bytes(silent_wav())
    if bannertool:
        subprocess.run([bannertool, "makesmdh", "-s", f"Pokémon {game}",
                        "-l", f"Pokémon {game} for New Nintendo 3DS", "-p", "unowncollectables",
                        "-i", str(out / "icon.png"), "-o", str(out / "icon.smdh")], check=True)
        subprocess.run([bannertool, "makebanner", "-i", str(out / "banner.png"),
                        "-a", str(out / "banner.wav"), "-o", str(out / "banner.bnr")], check=True)
    return game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--game", choices=("Black 2", "White 2"))
    parser.add_argument("--bannertool", help="also pack icon.smdh and banner.bnr with this executable")
    args = parser.parse_args()
    try:
        game = make_assets(args.rom, args.output, args.game, args.bannertool)
    except (OSError, ValueError, IndexError, struct.error, subprocess.CalledProcessError) as err:
        parser.exit(1, f"HOME Menu art: {err}\n")
    print(f"{game} HOME Menu art prepared in {args.output}")


if __name__ == "__main__":
    main()
