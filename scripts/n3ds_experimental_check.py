#!/usr/bin/env python3
"""Assert game-object compile success and closure .o freshness; scan experimental ELF for DS literals."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LITERALS = ("0x02fffc80", "0x02fffe00", "0x02fffa80", "0x02000000")
CRITICAL_SOURCES = (
    "lib/nitro/src/spi/tp.c",
    "lib/nitro/src/os/os_ownerInfo.c",
    "lib/nitro/src/os/os_ownerInfoEx.c",
)


def main() -> int:
    err = 0
    r = subprocess.run([sys.executable, str(ROOT / "scripts" / "n3ds_game_objects.py")], cwd=ROOT)
    if r.returncode != 0:
        print("n3ds_experimental_check: game_objects compile failed", file=sys.stderr)
        return 1

    for game in ("black2", "white2"):
        objdir = ROOT / "build" / "n3ds" / game / "obj"
        for rel in CRITICAL_SOURCES:
            obj = objdir / Path(rel).with_suffix(".o")
            src = ROOT / rel
            if not obj.is_file():
                print(f"FAIL missing object after compile: {obj.relative_to(ROOT)}", file=sys.stderr)
                err = 1
            elif src.is_file() and obj.stat().st_mtime < src.stat().st_mtime:
                print(f"FAIL stale object: {obj.relative_to(ROOT)}", file=sys.stderr)
                err = 1

        rsp = ROOT / "build" / "n3ds" / game / "game_objects.rsp"
        if rsp.is_file():
            for line in rsp.read_text().splitlines():
                p = Path(line.strip())
                if not p.is_file():
                    print(f"FAIL closure missing: {p}", file=sys.stderr)
                    err = 1

    elf = ROOT / "n3ds" / "build" / "b2" / "black2_3ds.elf"
    env = {**dict(__import__("os").environ), "PBW2_LINK_GAME": "1"}
    subprocess.run(["make", "-C", str(ROOT / "n3ds"), "b2"], cwd=ROOT, env=env, check=False)
    if not elf.is_file():
        print("n3ds_experimental_check: link did not produce ELF", file=sys.stderr)
        return 1

    nm = subprocess.check_output(["arm-none-eabi-nm", "-a", str(elf)], text=True, errors="replace")
    for sym in ("TP_GetUserInfo", "CARD_IdentifyBackup", "PbW2_NvramUserInfo"):
        if sym not in nm:
            print(f"WARN symbol not in ELF: {sym}")

    tp_obj = ROOT / "build/n3ds/black2/obj/lib/nitro/src/spi/tp.o"
    if tp_obj.is_file():
        dump = subprocess.check_output(["arm-none-eabi-objdump", "-d", str(tp_obj)], text=True, errors="replace")
        hits = [lit for lit in LITERALS if lit in dump.lower()]
        if hits:
            print("FAIL tp.o still references DS literal(s):", ", ".join(hits), file=sys.stderr)
            err = 1

    if err:
        print("n3ds_experimental_check: FAILED", file=sys.stderr)
        return 1
    print("n3ds_experimental_check: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
