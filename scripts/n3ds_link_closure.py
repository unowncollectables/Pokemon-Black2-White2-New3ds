#!/usr/bin/env python3
"""Build game_objects.rsp: game_main plus Nitro lib objects and an src/ closure."""

from __future__ import annotations

import re
import subprocess
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

NITRO_PREFIX = "lib/nitro/"
SRC_PREFIX = "src/"


def nm_defined(path: Path) -> set[str]:
    out = subprocess.check_output(["arm-none-eabi-nm", "--defined-only", "-g", str(path)], text=True, errors="replace")
    names: set[str] = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            names.add(parts[2])
    return names


def nm_undefined(path: Path) -> set[str]:
    out = subprocess.check_output(["arm-none-eabi-nm", "-u", str(path)], text=True, errors="replace")
    names: set[str] = set()
    for line in out.splitlines():
        m = re.search(r" U (.+)$", line)
        if m:
            names.add(m.group(1))
    return names


def eligible(src: str) -> bool:
    if src == "src/system/main.c":
        return False
    if src == "lib/nitro/src/fs/fs_overlay.c":
        return False
    return src.startswith(SRC_PREFIX) or src.startswith(NITRO_PREFIX)


def symbol_index(objdir: Path) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for path in sorted(objdir.rglob("*.o")):
        rel = path.relative_to(objdir)
        src = rel.with_suffix(".c").as_posix()
        if not eligible(src):
            continue
        for name in nm_defined(path):
            index.setdefault(name, path)
    return index


def closure_paths(game: str) -> list[Path]:
    name = game.lower()
    objdir = ROOT / "build" / "n3ds" / name / "obj"
    game_main = objdir / "src" / "system" / "game_main.o"
    if not game_main.is_file():
        raise SystemExit(f"missing {game_main}")
    index = symbol_index(objdir)
    chosen: dict[Path, None] = {game_main: None}
    changed = True
    while changed:
        changed = False
        need: set[str] = set()
        for obj in chosen:
            need |= nm_undefined(obj)
        for sym in sorted(need):
            if sym.startswith("__aeabi_"):
                continue
            path = index.get(sym)
            if path and path not in chosen:
                chosen[path] = None
                changed = True
    return sorted(chosen)


def write_rsp(game: str) -> int:
    paths = closure_paths(game)
    out = ROOT / "build" / "n3ds" / game.lower() / "game_objects.rsp"
    out.write_text("\n".join(str(p) for p in paths) + "\n")
    print(f"{game}: {len(paths)} objects in {out.relative_to(ROOT)}")
    return len(paths)


def main() -> int:
    for game in ("BLACK2", "WHITE2"):
        write_rsp(game)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())