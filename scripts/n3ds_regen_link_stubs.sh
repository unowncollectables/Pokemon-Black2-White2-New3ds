#!/bin/sh
# After PBW2_LINK_GAME=1 link fails, collect undefined symbols and extend link_stubs.c.
set -e
cd "$(dirname "$0")/.."
LOG="${1:-/tmp/n3ds_link.log}"
if [ ! -f "$LOG" ]; then
    echo "usage: $0 link.log" >&2
    exit 1
fi
grep "undefined reference" "$LOG" | sed "s/.*undefined reference to \`\([^']*\)'.*/\1/" | sort -u > /tmp/pbw2_undef.txt
python3 - <<'PY'
from pathlib import Path
skip = {"FX_Div", "FX_Sqrt", "FX_Atan2Idx", "FX_Inv", "__memcpy", "MATH_QSort", "MATH_CountPopulation"}
cur = Path("n3ds/nitro/link_stubs.c")
existing = set()
if cur.is_file():
    for line in cur.read_text().splitlines():
        if line.startswith("void ") and "(void)" in line:
            existing.add(line.split()[1].split("(")[0])
new = [s.strip() for s in Path("/tmp/pbw2_undef.txt").read_text().splitlines() if s.strip() and s[0].isalpha()]
all_syms = sorted(existing | set(new) - skip)
lines = [
    "/* SPDX-License-Identifier: GPL-3.0-or-later */",
    '#include "stub_log.h"',
    "",
    "/* Title-slice link stubs; extend with scripts/n3ds_regen_link_stubs.sh */",
    "",
]
for sym in all_syms:
    lines.append(f"void {sym}(void) {{ PBW2_STUB_LOG(\"link\", \"{sym}\"); }}")
cur.write_text("\n".join(lines) + "\n")
print(f"wrote {len(all_syms)} stubs to {cur}")
PY