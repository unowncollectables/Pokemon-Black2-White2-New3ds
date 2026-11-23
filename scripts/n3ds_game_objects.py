#!/usr/bin/env python3
"""Compile the decomp's C for the New 3DS ABI. No ROM and no link.

src/ must compile for Black 2 and White 2. Library C that is still MWCC
assembly, or that needs a ROM blob, is counted and listed. The shell is a
separate link; these objects are the game, not yet called.
"""

from __future__ import annotations

import os
import subprocess
import sys
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CRITICAL_LIB_SOURCES = (
    "lib/nitro/src/spi/tp.c",
    "lib/nitro/src/os/os_ownerInfo.c",
    "lib/nitro/src/os/os_ownerInfoEx.c",
)
STATUS = ROOT / "build" / "n3ds" / "pbw2_build_status.h"
REPORT = ROOT / "build" / "n3ds" / "compile-report.txt"


def compiler() -> str:
    arm = os.environ.get("DEVKITARM")
    if arm:
        candidate = Path(arm) / "bin" / "arm-none-eabi-gcc"
        if candidate.is_file():
            return str(candidate)
    return os.environ.get("CC", "arm-none-eabi-gcc")


def arch(cc: str) -> list[str]:
    base = ["-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft"]
    probe = subprocess.run([cc, *base, "-c", "-x", "c", "-", "-o", os.devnull],
                           input=b"int x;\n", capture_output=True)
    if probe.returncode == 0:
        return base
    return ["-march=armv6k+fp", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft"]


def includes() -> list[str]:
    found = ["-Iinclude", "-Ibuild/include/generated", "-In3ds/source"]
    for path in sorted((ROOT / "lib").glob("*/include")):
        found.append(f"-I{path.relative_to(ROOT).as_posix()}")
    return found


def sources(root: Path) -> list[Path]:
    return sorted(path.relative_to(ROOT) for path in root.rglob("*.c"))


def compile_one(cc: str, flags: list[str], source: Path, out: Path) -> tuple[str, int, str]:
    out.parent.mkdir(parents=True, exist_ok=True)
    extra = list(flags)
    if source.as_posix() == "src/system/main.c":
        extra.append("-Dmain=Pbw2_GameMain")
    extra += [f"-I{source.parent.as_posix()}"]
    if source.parent.parent != ROOT:
        extra.append(f"-I{source.parent.parent.as_posix()}")
    result = subprocess.run([cc, *extra, str(source), "-o", str(out)],
                            cwd=ROOT, capture_output=True, text=True)
    first = ""
    for line in (result.stderr or "").splitlines():
        if "error:" in line:
            first = line.split("error:", 1)[1].strip()
            break
    if result.returncode != 0:
        if out.is_file():
            out.unlink()
        if not first:
            first = ((result.stderr or result.stdout or "compile failed").strip().splitlines() or ["compile failed"])[-1]
    return source.as_posix(), result.returncode, first


def compile_game(cc: str, arch_flags: list[str], name: str) -> tuple[int, int, int, int, list[str]]:
    flags = [
        "-marm", *arch_flags, "-mword-relocations", "-ffreestanding", "-fno-builtin", "-std=c99",
        # GCC 14+ (devkitARM 16) rejects the decomp's pointer casts by default.
        "-Wno-unknown-pragmas", "-Wno-incompatible-pointer-types",
        "-DPBW2_N3DS", "-D__thumb", f"-D{name}", "-c", *includes(),
    ]
    jobs = []
    for tree, kind in ((ROOT / "src", "game"), (ROOT / "lib", "lib")):
        for source in sources(tree):
            if source.as_posix() == "src/system/main.c":
                out = ROOT / "build" / "n3ds" / name.lower() / "obj" / "src" / "system" / "game_main.o"
            else:
                out = ROOT / "build" / "n3ds" / name.lower() / "obj" / source.with_suffix(".o")
            jobs.append((source, out, kind))

    game_ok = game_n = lib_ok = lib_n = 0
    failures: list[str] = []
    reasons: Counter[str] = Counter()
    with ProcessPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        futures = [pool.submit(compile_one, cc, flags, source, out) for source, out, _ in jobs]
        kinds = {source.as_posix(): kind for source, _, kind in jobs}
        for future in as_completed(futures):
            source_name, code, error = future.result()
            kind = kinds[source_name]
            if kind == "game":
                game_n += 1
                game_ok += code == 0
            else:
                lib_n += 1
                lib_ok += code == 0
            if code != 0:
                reasons[error] += 1
                if len(failures) < 40:
                    failures.append(f"{source_name}: {error}")
    print(f"{name}: game {game_ok}/{game_n}, library {lib_ok}/{lib_n}")
    return game_ok, game_n, lib_ok, lib_n, failures + [f"{count}  {reason}" for reason, count in reasons.most_common(15)]


def critical_lib_ok(name: str) -> bool:
    objdir = ROOT / "build" / "n3ds" / name.lower() / "obj"
    for rel in CRITICAL_LIB_SOURCES:
        if not (objdir / Path(rel).with_suffix(".o")).is_file():
            return False
    return True


def write_status(counts: dict[str, tuple[int, int, int, int]]) -> None:
    b2 = counts["BLACK2"]
    w2 = counts["WHITE2"]
    STATUS.parent.mkdir(parents=True, exist_ok=True)
    STATUS.write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Regenerated by scripts/n3ds_game_objects.py. */\n"
        "#ifndef PBW2_BUILD_STATUS_H\n"
        "#define PBW2_BUILD_STATUS_H\n\n"
        f"#define PBW2_B2_GAME_C_OK {b2[0]}\n"
        f"#define PBW2_B2_GAME_C_TOTAL {b2[1]}\n"
        f"#define PBW2_W2_GAME_C_OK {w2[0]}\n"
        f"#define PBW2_W2_GAME_C_TOTAL {w2[1]}\n"
        f"#define PBW2_B2_LIB_C_OK {b2[2]}\n"
        f"#define PBW2_B2_LIB_C_TOTAL {b2[3]}\n"
        f"#define PBW2_W2_LIB_C_OK {w2[2]}\n"
        f"#define PBW2_W2_LIB_C_TOTAL {w2[3]}\n\n"
        "#endif\n"
    )


def main() -> int:
    cc = compiler()
    arch_flags = arch(cc)
    print("compiler", cc)
    print("arch", " ".join(arch_flags))
    counts = {}
    lines = []
    failed_src = False
    for define in ("BLACK2", "WHITE2"):
        game_ok, game_n, lib_ok, lib_n, notes = compile_game(cc, arch_flags, define)
        counts[define] = (game_ok, game_n, lib_ok, lib_n)
        lines.append(f"{define}: game {game_ok}/{game_n}, library {lib_ok}/{lib_n}")
        lines.extend(notes)
        if game_ok != game_n:
            failed_src = True
        if not critical_lib_ok(define):
            print(f"{define}: CRITICAL lib objects missing (tp/NVRAM ownerInfo)", file=sys.stderr)
            failed_src = True
        elif lib_ok != lib_n:
            print(f"{define}: library compile {lib_ok}/{lib_n} (non-closure failures tolerated)")
    write_status(counts)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text("\n".join(lines) + "\n")
    print(f"wrote {STATUS.relative_to(ROOT)}")
    subprocess.run([sys.executable, str(ROOT / "scripts" / "n3ds_link_closure.py")], check=False)
    return 1 if failed_src else 0


if __name__ == "__main__":
    sys.exit(main())
