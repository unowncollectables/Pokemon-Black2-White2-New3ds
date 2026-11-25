# Building

## 3DS shell and game objects, no ROM

Needs devkitPro `3ds-dev` (`DEVKITPRO=/opt/devkitpro`) and Python 3.11 or newer.
Does not read `orig/` and does not download a ROM.
Linked builds also use Citro3D and Picasso from devkitPro. Set
`PBW2_GPU_GEOMETRY=0` to build with the CPU renderer alone.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
bash scripts/n3ds_build.sh
```

Outputs, gitignored:

- `n3ds/build/b2/black2_3ds.elf`
- `n3ds/build/b2/black2_3ds.3dsx`
- `n3ds/build/w2/white2_3ds.elf`
- `n3ds/build/w2/white2_3ds.3dsx`

`scripts/n3ds_headers.py` writes `build/include/generated/` from `generated/*.txt`
and `res/text/`. `scripts/n3ds_game_objects.py` compiles `src/` and `lib/` for
`BLACK2` and `WHITE2` with `-Wno-incompatible-pointer-types` (devkitARM 16
otherwise stops on that). `src/` must be 1218/1218 or the build stops. Library
files that are still MWCC assembly are reported and skipped. The counts land
in `build/n3ds/pbw2_build_status.h`. That file is gitignored, because GCC 14
and devkitARM 16 count a different number of library files. The shell includes
it when the build created it.

Stage the extracted NitroFS (gitignored, not a ROM commit) after `ninja` has
filled `extract/`:

```sh
bash scripts/n3ds_stage_nitrofs.sh b2 extract/b2_us/files
bash scripts/n3ds_stage_nitrofs.sh w2 extract/w2_us/files
```

That copies into `$HOME/pbw2-sd/3ds/<game>_3ds/nitrofs`. Put `3ds/` on the SD
card. The shell opens `a/0/0/2` from there, then draws the logo from `a/0/2/6`.
It does not write a `.sav`. Do not commit `extract/`, `sd-stage/`, or a romfs
payload.

`make -C n3ds b2` or `make -C n3ds w2` builds one game after the objects.

### Geometry regression fixture

After the Black 2 objects are built, use the synthetic texture and capture
fixture. It requires no staged game data.

```sh
PBW2_LINK_GAME=1 make -C n3ds GAME=BLACK2 BUILD=build/g3-test test-g3
```

Run `n3ds/build/g3-test/g3_fidelity.3dsx` on the 3DS or in Azahar. It writes
`sdmc:/g3-fidelity.txt`. A passing result reports CPU and PICA200 separately
and counts 31 completed GPU frames. It checks texture-coordinate generation
from texture coordinates, normals, and vertices, including when each command
reads the texture matrix. It also checks overlap between translucent
polygons with the same and different IDs, opaque reset, alpha-zero texels,
optional depth writes for fractional texels, the GX alpha threshold,
lighting, and display capture while the main LCD is hidden.
The compositor checks cover sprite and rectangular window masks, affine
sprite masks, window clipping of the 3D layer, layer priority ties,
and brightness targets and window effects.
It exercises software fallback for a mid-frame palette rewrite,
fractional decals, alpha testing with two fractional alpha factors,
toon and highlight shading, both fog modes with Z- and W-buffer depth,
shadow mask depth and polygon-ID rejection, and edge marking.

### Settings input regression fixture

`make -C n3ds BUILD=build/input-test test-input` builds
`n3ds/build/input-test/input_capture.3dsx`. Run it on the 3DS or in Azahar
and read `sdmc:/input-capture-test.txt`. It checks Circle Pad input while
opening and closing Settings, direction changes before release, and
normal analog and digital movement once the stick returns to neutral.

### Native LCD regression fixture

`make -C n3ds BUILD=build/lcd-test test-present` builds
`n3ds/build/lcd-test/present_blit.3dsx`. Run it on the 3DS or in Azahar
and read `sdmc:/present-blit-test.txt`. It compares each RGB565 pixel
on both LCDs and checks that a smaller picture clears the old margins.

### Clock regression fixture

After compiling the Black 2 game objects, run
`make -C n3ds BUILD=build/rtc-test test-rtc`. Run the resulting
`n3ds/build/rtc-test/rtc_cache.3dsx` on the 3DS or in Azahar and read
`sdmc:/rtc-cache-test.txt`. The fixture checks leap-day conversion,
the game's cache refresh across midnight, and the start of the DS epoch.

### Wave output regression fixture

`make -C n3ds BUILD=build/audio-test test-wave` builds
`n3ds/build/audio-test/wave_out.3dsx`. Run it on the 3DS or in Azahar
and read `sdmc:/wave-out-test.txt`. It intercepts NDSP calls to check
the PCM intro and sustain buffers, ADPCM predictor output, and restart
and stop behavior. It requires no staged sound data or DSP firmware.

### If the compiler is not devkitARM's

Ubuntu's `gcc-arm-none-eabi` needs `-march=armv6k+fp` (the Makefile probes)
and a libsysbase, because its newlib calls `_open` rather than `_open_r`.
Set:

```sh
export PBW2_LOCAL_SYSBASE=1
export PBW2_SYSBASE=/path/to/dir    # directory that contains libsysbase.a
export PBW2_EXTRA_INC="-I/path/to/newlib/libc/sys/arm -I/path/to/newlib/libc/include"
```

Leave those unset on a normal devkitPro install. `pkg.devkitpro.org` answered
403 from the environment that first linked this tree, which is why that path
exists.

## Matching DS rebuild, needs your ROM

This is pokebw2's build. Linux or macOS, Python 3.11+, ninja, clang, and
`llvm-objcopy`. The first configure downloads wibo, sjiswrap, MWCC, and dsd.
More detail is in [building.md](building.md).

```sh
# your own dumps, not committed
# orig/baserom_b2_us.nds  e51e6dfb8678a3d19dcd2a10691b96a569ca0abb
# orig/baserom_w2_us.nds  b5d7490be7b415b8f1e672a53e978a9cc667e56a
python3 configure.py
ninja
```

`configure.py` with no ROM exits `no base ROMs found in orig/`. That is
expected on a machine that only builds the 3DS shell.

`ninja` extracts under `extract/` (gitignored), compiles with MWCC, links, and
checks both SHA1s. The arm9 delink paths have to exist or ninja stops on the
first missing object (`build/b2_us/src/system/save_table.o` was the old
failure). `python3 scripts/check_delinks.py` checks that without a ROM.

## CIA

The owner's Pokémon 3DS Builder packages the CIA from its ELF, the
access descriptor in `n3ds/cia/`, and two HOME Menu assets. Prepare
those assets with the same bannertool executable used by SoulSilver.

```sh
make -C n3ds home-menu-art GAME=BLACK2 ROM="/path/to/Black 2.nds" BT="/path/to/bannertool"
make -C n3ds home-menu-art GAME=WHITE2 ROM="/path/to/White 2.nds" BT="/path/to/bannertool"
```

Pass `icon.smdh` to the Builder's icon input and `banner.bnr` to its
banner input from `n3ds/build/home-menu/black2/` or `white2/`. The
icon comes from the DS cartridge banner. The flat HOME Menu banner
uses the unfiltered in-game title logo and a silent audio track.
The helper reads the supplied ROM in place and keeps all generated
art in ignored build output. Run `scripts/n3ds_home_menu_art.py`
directly to choose another output folder or prepare just the PNGs
and WAV for a Builder that already runs bannertool.
