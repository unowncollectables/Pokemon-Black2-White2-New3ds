#!/usr/bin/env python3
"""Fail if an arm9 delinks.txt names a source file that is not in the tree."""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def missing(game: str) -> list[str]:
    found = []
    path = ROOT / "config" / game / "arm9" / "delinks.txt"
    for line in path.read_text().splitlines():
        stripped = line.strip()
        if not (stripped.endswith(".c:") or stripped.endswith(".s:")):
            continue
        source = stripped[:-1]
        if not (ROOT / source).is_file():
            found.append(source)
    return found


def main() -> int:
    bad = False
    for game in ("b2_us", "w2_us"):
        gone = missing(game)
        print(f"{game}: {len(gone)} missing delink paths")
        for source in gone:
            print(f"  {source}")
        bad = bad or bool(gone)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
