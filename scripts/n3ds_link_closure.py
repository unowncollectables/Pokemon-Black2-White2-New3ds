#!/usr/bin/env python3
"""Build game_objects.rsp for the boot/title slice (not full-game BFS)."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Whole trees that must not enter the link closure (proc tables pull them in).
EXCLUDE_SRC_PREFIXES = (
    "src/wifi/",
    "src/communication/",
    "src/tournament/",
    "src/entralink/",
    "src/musical/",
    "src/pokewood/",
    "src/battle/",
    "src/field/",
    "src/app/",
    "src/moveeffect/",
)

EXCLUDE_SRC_FILES = {
    "src/system/main.c",
    "src/gfl/rtc_cache.c",
    "src/gfl/net.c",
    "src/system/net_err.c",
    "src/system/game_sync_settings_proc.c",
}

# Host provides FS / GX / OS for these; linking Nitro copies causes multiply-defined symbols.
EXCLUDE_LIB_NITRO_FILES = {
    "lib/nitro/src/fs/fs_overlay.c",
    "lib/nitro/src/fs/fs_api.c",
    "lib/nitro/src/fs/fs_file.c",
    "lib/nitro/src/fs/fs_memfile.c",
    "lib/nitro/src/fs/fs_rom.c",
    "lib/nitro/src/gx/gx.c",
    "lib/nitro/src/os/os_arena.c",
    "lib/nitro/src/os/os_emulator.c",
    "lib/nitro/src/os/os_interrupt.c",
    "lib/nitro/src/os/os_irqHandler.c",
    "lib/nitro/src/os/os_ownerInfoEx.c",
    "lib/nitro/src/os/os_reset.c",
    "lib/nitro/src/os/os_tick.c",
    "lib/nitro/src/os/os_timer.c",
    "lib/nitro/src/gx/gx_load2d.c",
    "lib/nitro/src/fx/fx_atanidx.c",
    "lib/nitro/src/fx/fx_cp.c",
    "lib/nitro/src/spi/pm.c",
}

EXCLUDE_LIB_NITRO_PREFIXES = (
    "lib/nitro/src/wm/",
    "lib/nitro/src/mb/",
    "lib/nitro/src/ctrdg/",
    "lib/nitro/src/pxi/",
)


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


def src_path_ok(src: str) -> bool:
    if src in EXCLUDE_SRC_FILES:
        return False
    if any(src.startswith(p) for p in EXCLUDE_SRC_PREFIXES):
        return False
    return src.startswith("src/")


def lib_nitro_path_ok(src: str) -> bool:
    if src in EXCLUDE_LIB_NITRO_FILES:
        return False
    if any(src.startswith(p) for p in EXCLUDE_LIB_NITRO_PREFIXES):
        return False
    return src.startswith("lib/nitro/")


def eligible(src: str) -> bool:
    return src_path_ok(src) or lib_nitro_path_ok(src)


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
    subprocess.run([str(ROOT / "scripts" / "n3ds_gen_nitro_stubs.py")], check=False)
    for game in ("BLACK2", "WHITE2"):
        write_rsp(game)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())