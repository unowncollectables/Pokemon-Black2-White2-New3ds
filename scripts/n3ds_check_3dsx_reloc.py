#!/usr/bin/env python3
"""Fail the N3DS link if ARM ABS32 addends encode bad 3DSX absolute-reloc subtypes."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def check_elf_abs_addends(elf: Path) -> list[str]:
    err: list[str] = []
    out = subprocess.check_output(["arm-none-eabi-readelf", "-rW", str(elf)], text=True, errors="replace")
    for line in out.splitlines():
        if "R_ARM_ABS32" not in line:
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            addend = int(parts[3], 16)
        except ValueError:
            continue
        if addend >= 0xF0000000:
            err.append(f"R_ARM_ABS32 addend 0x{addend:08x} (subtype in high nibble): {line.strip()}")
    return err


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: n3ds_check_3dsx_reloc.py path.elf [ignored]", file=sys.stderr)
        return 2
    elf = Path(sys.argv[1])
    if not elf.is_file():
        threedsx = Path(sys.argv[1])
        elf = threedsx.with_suffix(".elf")
    if not elf.is_file():
        print(f"FAIL missing ELF: {elf}", file=sys.stderr)
        return 1
    err = check_elf_abs_addends(elf)
    if err:
        print("n3ds_check_3dsx_reloc: FAILED", file=sys.stderr)
        for line in err:
            print(line, file=sys.stderr)
        return 1
    print("n3ds_check_3dsx_reloc: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())