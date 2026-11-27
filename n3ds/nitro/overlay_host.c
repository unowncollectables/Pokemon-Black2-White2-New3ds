/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Load arm9 overlays from staged bins under sdmc:/3ds/<game>_3ds/overlays/. */
#include "log.h"
#include "nitro/fs.h"
#include "nitro/mi.h"
#include "scene.h"
#include "stub_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PBW2_WHITE2)
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#endif

static u8 *s_table;
static u8 **s_loaded;
static u32 s_overlayCount;

static void overlay_root(char *out, size_t cap) {
    snprintf(out, cap, "%s/overlays", PBW2_SD_DIR);
}

static BOOL read_file(const char *path, void *dest, u32 size) {
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (!fp) {
        return FALSE;
    }
    n = fread(dest, 1, size, fp);
    fclose(fp);
    return n == size;
}

static BOOL load_header(u32 id, FSOverlayInfo *info) {
    u32 offset;
    if (id >= s_overlayCount) {
        memset(info, 0, sizeof *info);
        info->id = id;
        return FALSE;
    }
    offset = id * FS_OVERLAY_HEADER_SIZE;
    MI_CpuCopy8(s_table + offset, info, FS_OVERLAY_HEADER_SIZE);
    info->target = MI_PROCESSOR_ARM9;
    return info->ramSize != 0;
}

void PBW2_OverlayHostInit(void) {
    char path[160];
    long headerBytes = -1;
    FILE *fp;

    if (s_table) {
        return;
    }
    overlay_root(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/arm9_ovt.bin");
    fp = fopen(path, "rb");
    if (!fp) {
        PBW2_STUB_LOG("overlay", "no arm9_ovt.bin at %s", path);
        return;
    }
    if (fseek(fp, 0, SEEK_END) == 0) {
        headerBytes = ftell(fp);
    }
    if (headerBytes < FS_OVERLAY_HEADER_SIZE || headerBytes % FS_OVERLAY_HEADER_SIZE != 0 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return;
    }
    u32 count = (u32)headerBytes / FS_OVERLAY_HEADER_SIZE;
    u8 *table = malloc((size_t)headerBytes);
    u8 **loaded = calloc(count, sizeof(*loaded));
    if (!table || !loaded || fread(table, 1, (size_t)headerBytes, fp) != (size_t)headerBytes) {
        free(table);
        free(loaded);
        fclose(fp);
        return;
    }
    fclose(fp);
    s_table = table;
    s_loaded = loaded;
    s_overlayCount = count;
    Log_Line("overlay", "arm9_ovt %u entries", (unsigned)count);
}

BOOL FS_LoadOverlayInfo(FSOverlayInfo *info, MIProcessor target, FSOverlayID id) {
    (void)target;
    if (!load_header(id, info)) {
        return FALSE;
    }
    return TRUE;
}

BOOL FS_LoadOverlay(MIProcessor target, FSOverlayID id) {
    FSOverlayInfo info;
    char path[192];
    u32 binarySize;
    u32 dsAddr;
    u8 *hostMem;
    BOOL ok;

    (void)target;
    if (!FS_LoadOverlayInfo(&info, MI_PROCESSOR_ARM9, id)) {
        PBW2_STUB_LOG("FS_LoadOverlay", "id=%u no header", (unsigned)id);
        return FALSE;
    }
    if (s_loaded[id]) {
        return TRUE;
    }
    binarySize = (info.flag & FS_OVERLAY_FLAG_COMP) ? info.compressedSize : info.ramSize;
    if (binarySize == 0 || binarySize > info.ramSize || info.bssSize > UINT32_MAX - info.ramSize) {
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (invalid sizes)", (unsigned)id);
        return FALSE;
    }
    dsAddr = (u32)(uintptr_t)info.ramAddress;
    hostMem = (u8 *)malloc(info.ramSize + info.bssSize);
    if (!hostMem) {
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (nomem)", (unsigned)id);
        return FALSE;
    }
    overlay_root(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/ov%03u.bin", (unsigned)id);
    ok = read_file(path, hostMem, binarySize);
    if (!ok) {
        PBW2_STUB_LOG("FS_LoadOverlay", "id=%u missing %s", (unsigned)id, path);
        free(hostMem);
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (missing bin)", (unsigned)id);
        return FALSE;
    }
    if (info.bssSize) {
        MI_CpuFill8(hostMem + info.ramSize, 0, info.bssSize);
    }
    s_loaded[id] = hostMem;
    PbW2_SceneOverlayLoaded(id);
    Log_Line("overlay", "GFL_OvlLoad id=%u ok host=%p ds=0x%08x size=%u bss=%u", (unsigned)id, (void *)hostMem,
             (unsigned)dsAddr, (unsigned)info.ramSize, (unsigned)info.bssSize);
    return TRUE;
}

BOOL FS_UnloadOverlay(MIProcessor target, FSOverlayID id) {
    (void)target;
    if (id >= s_overlayCount || !s_loaded[id]) {
        return FALSE;
    }
    free(s_loaded[id]);
    s_loaded[id] = NULL;
    PbW2_SceneOverlayUnloaded(id);
    Log_Line("overlay", "GFL_OvlUnload id=%u", (unsigned)id);
    return TRUE;
}

void FSi_InitOverlay(void) {
    PBW2_OverlayHostInit();
}
