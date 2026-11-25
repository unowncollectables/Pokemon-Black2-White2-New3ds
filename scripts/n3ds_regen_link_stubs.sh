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
# Value-returning sound stubs live in n3ds/nitro/snd_silent.c (silent
# backend: honest counts/returns, not garbage r0). Never re-add them
# here as void: a void CountPlaying stub reports a player active
# forever and hangs the intro in POKEMON_APPEAR. The WaveOut channel
# functions live in n3ds/nitro/wave_out.c (real NDSP player) — never
# re-add those either.
SND_SILENT = {
    "NNS_SndArcGetBankInfo", "NNS_SndArcGetFileAddress", "NNS_SndArcGetFileSize",
    "NNS_SndArcGetGroupInfo", "NNS_SndArcGetSeqInfo", "NNS_SndArcGetSeqParam", "NNS_SndArcSetFileAddress",
    "NNS_SndArcGetWaveArcInfo", "NNS_SndArcLoadGroup", "NNS_SndArcLoadSeq",
    "NNS_SndArcLoadSeqEx", "NNS_SndArcLoadWaveArc", "NNS_SndArcPlayerSetup",
    "NNS_SndArcPlayerStartSeq", "NNS_SndArcPlayerStartSeqEx",
    "NNS_SndArcStrmGetCurrentPlayingPos", "NNS_SndPlayerCountPlayingSeqByPlayerNo",
    "NNS_SndPlayerCountPlayingSeqBySeqNo", "NNS_SndPlayerGetTick",
    "NNS_SndWaveOutAllocChannel", "NNS_SndWaveOutIsPlaying", "NNS_SndWaveOutStart",
    "NNS_SndWaveOutFreeChannel", "NNS_SndWaveOutStop", "NNS_SndWaveOutWaitForChannelStop",
    "NNS_SndWaveOutSetVolume", "NNS_SndWaveOutSetSpeed",
    "NNS_SndArcReadFile", "NNS_SndPlayerReadDriverTrackInfo",
}
DATA_OR_HOSTED = {
    "SDK_AUTOLOAD_DTCM_START", "NNSi_objSizeWTbl", "NNSi_objSizeHTbl",
    "NNS_GfdDefaultFuncAllocTexVram", "NNS_GfdDefaultFuncFreeTexVram",
    "NNS_GfdDefaultFuncAllocPlttVram", "NNS_GfdDefaultFuncFreePlttVram",
    "NNS_G3dGlb", "GXi_VRamLockId", "OSi_TickCounter",
    "NAME_ENTRY_PROC_FUNCTIONS", "NAME_ENTRY_SOUNDS", "NAME_ENTRY_SOUND_COUNT",
    "FIELDMAP_PROC_FUNCS", "DSProt_Callbacks", "DSProt_NopIndex",
    "NNS_FndCreateExpHeapEx", "NNS_FndDestroyExpHeap", "NNS_FndAllocFromExpHeapEx",
    "NNS_FndFreeToExpHeap", "NNS_FndGetAllocatableSizeForExpHeapEx", "NNS_FndGetSizeForMBlockExpHeap",
    "NNS_FndGetTotalFreeSizeForExpHeap", "NNS_FndInitAllocatorForExpHeap", "NNS_FndResizeForMBlockExpHeap",
    "NNS_FndVisitAllocatedForExpHeap", "NNS_G2dGetUnpackedCellBank",
    "NNS_G2dGetUnpackedCharacterData", "NNS_G2dGetUnpackedMultiCellBank",
    "FS_SetDefaultDMA", "NNS_SndArcInit", "NNS_SndArcSetup",
    "RTCCache_Init", "RTCCache_Update",
    "OS_WaitInterrupt",
}
cur = Path("n3ds/nitro/link_stubs.c")
existing = set()
if cur.is_file():
    for line in cur.read_text().splitlines():
        if line.startswith("void ") and "(void)" in line:
            existing.add(line.split()[1].split("(")[0])
new = [s.strip() for s in Path("/tmp/pbw2_undef.txt").read_text().splitlines() if s.strip() and s[0].isalpha()]
all_syms = sorted(
    sym
    for sym in existing | set(new) - skip - DATA_OR_HOSTED - SND_SILENT
    if not sym.startswith(("SPLBehavior_", "SPLManager_"))
)
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
