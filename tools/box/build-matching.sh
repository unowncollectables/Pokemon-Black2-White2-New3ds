#!/usr/bin/env bash
# Reproducible pokebw2 matching rebuild for USA/Europe Black 2 and White 2.
# Run from anywhere. Does not download ROMs; uses dumps already in orig/ or
# hardlinked from /workspace/b2w2-3ds/baserom-private/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
POKE="${POKEBW2_DIR:-$ROOT/pokebw2}"
PRIVATE="$ROOT/baserom-private"

EXPECTED_B2='e51e6dfb8678a3d19dcd2a10691b96a569ca0abb'
EXPECTED_W2='b5d7490be7b415b8f1e672a53e978a9cc667e56a'
# Pinned when this script was written; configure/build still uses whatever
# commit is checked out. Record it in the log.
PINNED_COMMIT='345b790e6000869b481b79838bb5727309f832fb'

need() {
  command -v "$1" >/dev/null 2>&1 || { echo "missing required tool: $1" >&2; exit 1; }
}

sha1_of() {
  sha1sum -b "$1" | awk '{print $1}'
}

need python3
need ninja
need clang
need llvm-objcopy
need sha1sum

if [[ ! -d "$POKE/.git" ]]; then
  echo "pokebw2 checkout not found at $POKE" >&2
  exit 1
fi

cd "$POKE"
HEAD="$(git rev-parse HEAD)"
echo "pokebw2 HEAD $HEAD"
if [[ "$HEAD" != "$PINNED_COMMIT" ]]; then
  echo "warning: HEAD is not the commit this script was pinned to ($PINNED_COMMIT)" >&2
fi

mkdir -p orig
for pair in "baserom_b2_us.nds:$EXPECTED_B2" "baserom_w2_us.nds:$EXPECTED_W2"; do
  name="${pair%%:*}"
  expect="${pair##*:}"
  dest="$POKE/orig/$name"
  if [[ ! -f "$dest" ]]; then
    src="$PRIVATE/$name"
    if [[ -f "$src" ]]; then
      ln -f "$src" "$dest" 2>/dev/null || cp -f "$src" "$dest"
    else
      echo "missing $dest (and $src)" >&2
      exit 1
    fi
  fi
  got="$(sha1_of "$dest")"
  echo "input $name SHA1 $got"
  if [[ "$got" != "$expect" ]]; then
    echo "SHA1 mismatch for $name: expected $expect" >&2
    exit 1
  fi
done

# Commit 345b790 renamed savedata/iss/pml sources but left stale paths in
# delinks.txt, so ninja links objects that have no compile rule. Apply the
# local path fix when the stale entries are still present.
PATCH="$ROOT/patches/pokebw2-345b790-delinks-rename.patch"
if grep -q '^src/system/save_table.c:' config/b2_us/arm9/delinks.txt 2>/dev/null; then
  echo "applying $PATCH"
  git apply "$PATCH"
fi

echo "configure.py b2_us w2_us"
python3 configure.py b2_us w2_us
echo "ninja"
ninja
echo "verify outputs"
ok=0
for pair in "build/pokeblack2_us.nds:$EXPECTED_B2:Black 2" "build/pokewhite2_us.nds:$EXPECTED_W2:White 2"; do
  IFS=':' read -r path expect label <<<"$pair"
  if [[ ! -f "$path" ]]; then
    echo "missing output $path ($label)" >&2
    ok=1
    continue
  fi
  got="$(sha1_of "$path")"
  echo "output $path ($label) SHA1 $got"
  if [[ "$got" != "$expect" ]]; then
    echo "OUTPUT SHA1 mismatch for $label: expected $expect" >&2
    ok=1
  else
    echo "MATCH $label"
  fi
done
exit "$ok"
