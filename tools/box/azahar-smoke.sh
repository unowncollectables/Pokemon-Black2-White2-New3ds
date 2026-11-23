#!/usr/bin/env bash
# Headless Azahar smoke for a .3dsx (New 3DS mode).
# Usage: azahar-smoke.sh <3dsx> <outdir> [seconds]
# Optional: SD_SUBDIR=3ds/black2_3ds  SHOTS="5 10 15 20"
set -u
TDSX=${1:?3dsx}; OUT=${2:?outdir}; SECS=${3:-20}
AZ=/workspace/tools/estyseesghosts/azahar/build/bin/Release/azahar
SHOT_EVERY=${SHOT_EVERY:-5}
RENDERER=${RENDERER:-gl}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
W=$OUT/emu; rm -rf "$W"; mkdir -p "$W/cfg/azahar-emu" "$W/data/azahar-emu/sdmc"
SDMC=$W/data/azahar-emu/sdmc
if [ -n "${SD_SUBDIR:-}" ]; then
  mkdir -p "$SDMC/$SD_SUBDIR"
  SD=$SDMC/$SD_SUBDIR
else
  SD=$SDMC
fi
# Optional staged NitroFS, copied/hardlinked into the emulated SD only.
if [ -n "${NITROFS_SRC:-}" ]; then
  if [ ! -d "$NITROFS_SRC/a" ]; then
    echo "NITROFS_SRC must contain a/: $NITROFS_SRC" >&2
    exit 1
  fi
  mkdir -p "$SD/nitrofs"
  cp -al "$NITROFS_SRC"/. "$SD/nitrofs"/ 2>/dev/null || cp -a "$NITROFS_SRC"/. "$SD/nitrofs"/
  echo "staged nitrofs -> $SD/nitrofs ($(find "$SD/nitrofs" -type f | wc -l) files)"
fi
GEXT=${TDSX##*.}
cp "$TDSX" "$W/game.$GEXT"
api=1; [ "$RENDERER" = sw ] && api=0
cat > "$W/cfg/azahar-emu/qt-config.ini" <<INI
[Core]
use_cpu_jit=true
[System]
is_new_3ds=true
is_new_3ds\\default=false
[Data%20Storage]
use_virtual_sd=true
sdmc_directory=$SDMC/
sdmc_directory\\default=false
[Renderer]
graphics_api=$api
graphics_api\\default=false
use_vsync=false
use_vsync\\default=false
frame_limit=100
resolution_factor=1
resolution_factor\\default=false
[Layout]
layout_option=0
[Audio]
output_type=1
output_type\\default=false
[Miscellaneous]
check_for_update_on_start=false
check_for_update_on_start\\default=false
[UI]
confirmClose=false
confirmClose\\default=false
firstStart=false
firstStart\\default=false
fullscreen=true
fullscreen\\default=false
[WebService]
enable_telemetry=false
enable_telemetry\\default=false
INI
DN=$(( 90 + RANDOM % 400 ))
while [ -e /tmp/.X11-unix/X$DN ]; do DN=$((DN+1)); done
Xvfb :$DN -screen 0 400x480x24 -nolisten tcp >"$OUT/xvfb.log" 2>&1 & XP=$!
sleep 1
export DISPLAY=:$DN XDG_CONFIG_HOME=$W/cfg XDG_DATA_HOME=$W/data LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb
GAME="$W/game.$GEXT"
"$AZ" -f "$GAME" >"$OUT/azahar_stdout.log" 2>&1 & EP=$!
start=$(date +%s%3N)
( for i in $(seq 1 100); do wid=$(xdotool search --name "^Azahar [0-9]" 2>/dev/null | head -1)
    if [ -n "$wid" ]; then for r in 1 2 3 4 5 6; do xdotool windowmove "$wid" 0 0; sleep 1; done; break; fi; sleep 0.2; done ) &
el(){ echo $(( $(date +%s%3N) - start )); }
shot(){ local t=$1 f="$OUT/t$(printf %06.2f "$1")"
  import -display :$DN -window root "$f.full.png" 2>/dev/null || return
  convert "$f.full.png" -crop 400x240+0+0 +repage "$f.top.png"
  convert "$f.full.png" -crop 320x240+40+240 +repage "$f.bottom.png"
  echo "shot t=$t -> $f.{top,bottom}.png"; }
SHOTS=${SHOTS:-$(seq $SHOT_EVERY $SHOT_EVERY $SECS | tr '\n' ' ')}
for t in $SHOTS; do
  tm=$(awk "BEGIN{print int($t*1000)}")
  while (( $(el) < tm )); do
    sleep 0.1
    if ! kill -0 $EP 2>/dev/null; then echo "emulator exited early at t=$(el)ms"; break 2; fi
  done
  shot "$t"
done
while (( $(el) < SECS*1000 )) && kill -0 $EP 2>/dev/null; do sleep 0.2; done
kill -0 $EP 2>/dev/null && alive=yes || alive="no (exited early)"
shot "$SECS" >/dev/null
kill $EP 2>/dev/null; sleep 2; kill -9 $EP 2>/dev/null
pkill -9 -f "$W/game.$GEXT" 2>/dev/null || true
kill $XP 2>/dev/null
# collect logs and any save the game might have written
if [ -n "${SD_SUBDIR:-}" ] && [ -d "$SD" ]; then
  [ -f "$SD/log.txt" ] && cp "$SD/log.txt" "$OUT/log.txt" && echo "log.txt -> $OUT/log.txt" || echo "no game log.txt"
  ls -la "$SD" > "$OUT/sd-listing.txt"
  find "$SD" -iname '*.sav' -print > "$OUT/saves-found.txt"
fi
cp "$W/data/azahar-emu/log/azahar_log.txt" "$OUT/azahar_log.txt" 2>/dev/null || true
echo "emulator alive at end: $alive"
echo "outputs in $OUT"
