#!/bin/sh
# Copy a local extract's NitroFS onto a card layout. Nothing here is committed.
#
#   scripts/n3ds_stage_nitrofs.sh b2 /path/to/extract/b2_us/files
#   scripts/n3ds_stage_nitrofs.sh w2 /path/to/extract/w2_us/files /media/sd
#
# The default destination is $HOME/pbw2-sd. Copy that tree onto the 3DS SD card
# so the game directory sits at sdmc:/3ds/black2_3ds/nitrofs (or white2_3ds).
# The shell opens nitrofs/a/0/0/2 and logs the size. It does not write a .sav.
#
# Refuses to write inside this git repository.
set -e
cd "$(dirname "$0")/.."
repo=$(pwd)

game=${1:-}
src=${2:-}
dest_root=${3:-$HOME/pbw2-sd}

if [ "$game" != b2 ] && [ "$game" != w2 ]; then
    echo "usage: scripts/n3ds_stage_nitrofs.sh b2|w2 /path/to/extract/<game>_us/files [dest-root]" >&2
    exit 1
fi
if [ -z "$src" ] || [ ! -d "$src/a" ]; then
    echo "nitrofs source must be the extract files directory (it contains a/)" >&2
    exit 1
fi

case "$game" in
    b2) dir=black2_3ds ;;
    w2) dir=white2_3ds ;;
esac
dest="$dest_root/3ds/$dir/nitrofs"

case "$dest" in
    "$repo"|"$repo"/*)
        echo "refusing to write NitroFS inside the git repo: $dest" >&2
        exit 1
        ;;
esac

mkdir -p "$dest"
# Copy the file tree. Do not follow links that point at the ROM itself.
# --no-preserve=ownership: plain `cp -a` fails in containers/VMs where
# chown is not permitted (the copy itself succeeds, but cp exits non-zero
# and `set -e` aborts the stage). Ownership is meaningless on a FAT SD card
# anyway; mode and timestamps are still preserved.
cp -r --preserve=mode,timestamps --no-preserve=ownership "$src"/. "$dest"/
files=$(find "$dest" -type f | wc -l)
probe=0
if [ -f "$dest/a/0/0/2" ]; then
    probe=$(wc -c < "$dest/a/0/0/2")
fi
echo "staged $files files to $dest"
echo "probe a/0/0/2: $probe bytes"
echo "copy $dest_root/3ds onto the SD card so this directory is"
echo "  sdmc:/3ds/$dir/nitrofs"
