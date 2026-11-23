/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Load arm9 overlays from staged bins under sdmc:/3ds/<game>_3ds/overlays/. */
#include "log.h"
#include "nitro/fs.h"
#include "nitro/mi.h"
#include "stub_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PBW2_WHITE2)
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#endif

#define PBW2_OVERLAY_SLOTS 8

typedef struct {
    u32 id;
    u8 *mem;
    u32 ramSize;
    u32 bssSize;
    u32 dsRamAddress;
    int loaded;
} HostedOverlay;

static HostedOverlay s_slots[PBW2_OVERLAY_SLOTS];
static u8 *s_table;
static u32 s_tableBytes;

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
    if (!s_table || id * FS_OVERLAY_HEADER_SIZE + FS_OVERLAY_HEADER_SIZE > s_tableBytes) {
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
    u32 headerBytes;
    FILE *fp;

    overlay_root(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/arm9_ovt.bin");
    fp = fopen(path, "rb");
    if (!fp) {
        PBW2_STUB_LOG("overlay", "no arm9_ovt.bin at %s", path);
        return;
    }
    fseek(fp, 0, SEEK_END);
    headerBytes = (u32)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (headerBytes < FS_OVERLAY_HEADER_SIZE) {
        fclose(fp);
        return;
    }
    s_table = malloc(headerBytes);
    if (!s_table) {
        fclose(fp);
        return;
    }
    if (fread(s_table, 1, headerBytes, fp) != headerBytes) {
        free(s_table);
        s_table = NULL;
        fclose(fp);
        return;
    }
    fclose(fp);
    s_tableBytes = headerBytes;
    Log_Line("overlay", "arm9_ovt %u bytes", headerBytes);
}

static HostedOverlay *slot_for(u32 id) {
    int i;
    for (i = 0; i < PBW2_OVERLAY_SLOTS; i++) {
        if (s_slots[i].loaded && s_slots[i].id == id) {
            return &s_slots[i];
        }
    }
    for (i = 0; i < PBW2_OVERLAY_SLOTS; i++) {
        if (!s_slots[i].loaded) {
            return &s_slots[i];
        }
    }
    return NULL;
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
    HostedOverlay *slot;
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
    slot = slot_for(id);
    if (!slot) {
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (no slot)", (unsigned)id);
        return FALSE;
    }
    dsAddr = (u32)(uintptr_t)info.ramAddress;
    hostMem = (u8 *)malloc(info.ramSize + info.bssSize);
    if (!hostMem) {
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (nomem)", (unsigned)id);
        return FALSE;
    }
    slot->id = id;
    slot->ramSize = info.ramSize;
    slot->bssSize = info.bssSize;
    slot->dsRamAddress = dsAddr;
    slot->mem = hostMem;
    binarySize = info.ramSize;
    if (info.flag & FS_OVERLAY_FLAG_COMP) {
        binarySize = info.compressedSize;
    }
    overlay_root(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/ov%03u.bin", (unsigned)id);
    ok = read_file(path, hostMem, binarySize);
    if (!ok) {
        PBW2_STUB_LOG("FS_LoadOverlay", "id=%u missing %s", (unsigned)id, path);
        free(hostMem);
        memset(slot, 0, sizeof *slot);
        Log_Line("overlay", "GFL_OvlLoad id=%u fail (missing bin)", (unsigned)id);
        return FALSE;
    }
    if (info.bssSize) {
        MI_CpuFill8(hostMem + info.ramSize, 0, info.bssSize);
    }
    info.ramAddress = hostMem;
    slot->loaded = 1;
    Log_Line("overlay", "GFL_OvlLoad id=%u ok host=%p ds=0x%08x size=%u bss=%u", (unsigned)id, (void *)hostMem,
             (unsigned)dsAddr, (unsigned)info.ramSize, (unsigned)info.bssSize);
    return TRUE;
}

BOOL FS_UnloadOverlay(MIProcessor target, FSOverlayID id) {
    HostedOverlay *slot;
    int i;
    (void)target;
    for (i = 0; i < PBW2_OVERLAY_SLOTS; i++) {
        if (s_slots[i].loaded && s_slots[i].id == id) {
            slot = &s_slots[i];
            if (slot->mem) {
                free(slot->mem);
            }
            memset(slot, 0, sizeof *slot);
            return TRUE;
        }
    }
    return FALSE;
}

void FSi_InitOverlay(void) {
    PBW2_OverlayHostInit();
}
