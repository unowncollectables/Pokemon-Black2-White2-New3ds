# Building

## 3DS shell and game objects, no ROM

Needs devkitPro `3ds-dev` (`DEVKITPRO=/opt/devkitpro`) and Python 3.11 or newer.
Does not read `orig/` and does not download a ROM.

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

Not built here. `n3ds/cia/black2.rsf` and `n3ds/cia/white2.rsf` are the access
descriptor: `CoreVersion 2`, `CpuSpeed 804MHz`, `EnableL2Cache true`,
`CanAccessCore2 true`, `SystemModeExt 124MB`. `makerom` also needs a banner
and an icon, which this tree does not have. The owner's Pokemon 3DS Builder
packs the CIA from the `.elf`. Do not commit the CIA.
