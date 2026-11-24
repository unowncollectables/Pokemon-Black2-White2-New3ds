#!/usr/bin/env python3
"""Build game_objects.rsp for the boot/title slice (not full-game BFS)."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Whole trees that must not enter the link closure (proc tables pull them in).
EXCLUDE_SRC_PREFIXES = (
    "src/wifi/",
    "src/communication/",
    "src/tournament/",
    "src/entralink/",
    "src/musical/",
    "src/pokewood/",
    "src/battle/",
    "src/field/",
    "src/app/",
    "src/moveeffect/",
)

# One app slice IS in the title flow (new-game name entry): the intro
# hands off to it, so it must link even though src/app/ is excluded.
# Slice 1 of the field engine (Nov 24, 2026): the GameData field
# subsystems, called by GameData_Create on every boot. Listed as exact
# file paths (each matches one object); their stubs were removed from
# link_stubs.c in the same change.
INCLUDE_SRC_PREFIXES = (
    "src/app/name_input/",
    "src/field/event/eventdata_system.c",
    "src/field/map/map_matrix.c",
    "src/field/map/map_replace.c",
    "src/field/actor/field_actor.c",
    "src/field/encounter/field_encount_st.c",
    "src/field/actor/pair_sys.c",
    "src/field/event/calender.c",
    # Slice 1b: hidden_item.c is free — its only 3 deps are already
    # linked, and it defines GetHiddenItemEventFlagNoBySCRID for real.
    "src/field/event/hidden_item.c",
    # Slice 2: field proc + Field_Create + entry-point event chain.
    "src/field/map/fieldmap_proc.c",
    "src/field/map/fieldmap.c",
    "src/field/event/event_game_clear.c",
    "src/field/event/event_mapchange.c",
    # Slice 2b: the routines' transitive closure — the whole field tree
    # (BFS only pulls files that define a referenced symbol), plus the
    # out-of-tree files the routines need: Union Room field effects,
    # Entralink gimmick/mission, and the RTC cache helper.
    "src/field/",
    "src/communication/union/union_eff.c",
    "src/communication/union/union_main.c",
    "src/entralink/fes_gimmick.c",
    "src/entralink/fest_mission_field.c",
    "src/entralink/field_fmission_info.c",
    "src/entralink/intrude_work.c",
    # NOTE: src/gfl/rtc_cache.c is deliberately NOT included: it reads the
    # DS RTC hardware (unmapped here) into its cache, so its GetCached*
    # would return zeros and break the field clock (ev_time.c). The host
    # owns the clock (rtc_host.c) and provides the pure functions the game
    # needs from that file: RTC_ConvertDaySecondsCached,
    # RTC_ConvertSecondsCached, RTC_GetDayOfYear, RTC_GetCachedDateTime.
    # Slice 2c: the field's menu/proc ecosystem — the field code takes the
    # addresses of these proc tables (event_field_proclink.c, event_battle.c)
    # and queues procs from them, so they must link for real; void stubs
    # would crash when the player opens the bag/Pokedex or enters battle.
    "src/app/",
    "src/battle/",
    "src/communication/",
    "src/tournament/",
    "src/musical/",
    "src/pokewood/",
    "src/wifi/",
    # Slice 2d: the Entralink files (fest missions, hidden hollows,
    # postman, surveys — all referenced from the field) and the battle
    # visual-effect engine (btlv_effect.c, 88 symbols).
    "src/entralink/",
    "src/moveeffect/",
)

EXCLUDE_SRC_FILES = {
    "src/system/main.c",
    "src/gfl/rtc_cache.c",
    "src/gfl/net.c",
    "src/system/net_err.c",
    "src/system/game_sync_settings_proc.c",
}

# Host provides FS / GX / OS for these; linking Nitro copies causes multiply-defined symbols.
EXCLUDE_LIB_NITRO_FILES = {
    "lib/nitro/src/fs/fs_overlay.c",
    "lib/nitro/src/fs/fs_api.c",
    "lib/nitro/src/fs/fs_file.c",
    "lib/nitro/src/fs/fs_memfile.c",
    "lib/nitro/src/fs/fs_rom.c",
    "lib/nitro/src/gx/gx.c",
    "lib/nitro/src/os/os_arena.c",
    "lib/nitro/src/os/os_emulator.c",
    "lib/nitro/src/os/os_interrupt.c",
    "lib/nitro/src/os/os_irqHandler.c",
    "lib/nitro/src/os/os_ownerInfoEx.c",
    "lib/nitro/src/os/os_reset.c",
    "lib/nitro/src/os/os_tick.c",
    "lib/nitro/src/os/os_timer.c",
    "lib/nitro/src/gx/gx_load2d.c",
    "lib/nitro/src/fx/fx_atanidx.c",
    "lib/nitro/src/fx/fx_cp.c",
    "lib/nitro/src/spi/pm.c",
}

EXCLUDE_LIB_NITRO_PREFIXES = (
    "lib/nitro/src/wm/",
    "lib/nitro/src/mb/",
    "lib/nitro/src/ctrdg/",
    "lib/nitro/src/pxi/",
)


def nm_defined(path: Path) -> set[str]:
    out = subprocess.check_output(["arm-none-eabi-nm", "--defined-only", "-g", str(path)], text=True, errors="replace")
    names: set[str] = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            names.add(parts[2])
    return names


def nm_undefined(path: Path) -> set[str]:
    out = subprocess.check_output(["arm-none-eabi-nm", "-u", str(path)], text=True, errors="replace")
    names: set[str] = set()
    for line in out.splitlines():
        m = re.search(r" U (.+)$", line)
        if m:
            names.add(m.group(1))
    return names


def src_path_ok(src: str) -> bool:
    if any(src.startswith(p) for p in INCLUDE_SRC_PREFIXES):
        return True
    if src in EXCLUDE_SRC_FILES:
        return False
    if any(src.startswith(p) for p in EXCLUDE_SRC_PREFIXES):
        return False
    return src.startswith("src/")


def lib_nitro_path_ok(src: str) -> bool:
    if src in EXCLUDE_LIB_NITRO_FILES:
        return False
    if any(src.startswith(p) for p in EXCLUDE_LIB_NITRO_PREFIXES):
        return False
    return src.startswith("lib/nitro/")


def lib_nnsys_path_ok(src: str) -> bool:
    # Exact files only: the g3d tree cannot be taken whole — glbstate.c
    # defines NNS_G3dGlb (hosted in title_link_syms.c) and util.c defines
    # NNS_G3dInit (hosted in g3_gecom_host.c); model.c and snd_heap.c are
    # the clean ones. Their missing siblings live in g3d_glb_stubs.c.
    if src in {
        "lib/nnsys/src/g3d/model.c",
        "lib/nnsys/src/snd/snd_heap.c",
    }:
        return True
    return src.startswith("lib/nnsys/src/fnd/")


def eligible(src: str) -> bool:
    return src_path_ok(src) or lib_nitro_path_ok(src) or lib_nnsys_path_ok(src)


def symbol_index(objdir: Path) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for path in sorted(objdir.rglob("*.o")):
        rel = path.relative_to(objdir)
        src = rel.with_suffix(".c").as_posix()
        if not eligible(src):
            continue
        for name in nm_defined(path):
            index.setdefault(name, path)
    return index


def closure_paths(game: str) -> list[Path]:
    name = game.lower()
    objdir = ROOT / "build" / "n3ds" / name / "obj"
    game_main = objdir / "src" / "system" / "game_main.o"
    if not game_main.is_file():
        raise SystemExit(f"missing {game_main}")
    index = symbol_index(objdir)
    chosen: dict[Path, None] = {game_main: None}
    changed = True
    while changed:
        changed = False
        need: set[str] = set()
        for obj in chosen:
            need |= nm_undefined(obj)
        for sym in sorted(need):
            if sym.startswith("__aeabi_"):
                continue
            path = index.get(sym)
            if path and path not in chosen:
                chosen[path] = None
                changed = True
    return sorted(chosen)


def write_rsp(game: str) -> int:
    paths = closure_paths(game)
    out = ROOT / "build" / "n3ds" / game.lower() / "game_objects.rsp"
    out.write_text("\n".join(str(p) for p in paths) + "\n")
    print(f"{game}: {len(paths)} objects in {out.relative_to(ROOT)}")
    return len(paths)


def main() -> int:
    subprocess.run([str(ROOT / "scripts" / "n3ds_gen_nitro_stubs.py")], check=False)
    for game in ("BLACK2", "WHITE2"):
        write_rsp(game)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())