/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/card.h"
#include "nitro/mi.h"
#include "nitro/pxi.h"
#include "stub_log.h"

#include <string.h>

/* Shadow save media for B2W2 (512 KiB flash, CARD_BACKUP_TYPE_FLASH_4MBITS_EX). */
#define PBW2_CARD_SHADOW_SIZE 0x80000u

static u8 s_cardShadow[PBW2_CARD_SHADOW_SIZE];
static int s_loggedCard;
static int s_shadowInit;

static void shadow_ensure(void) {
    if (!s_shadowInit) {
        memset(s_cardShadow, 0xFF, sizeof s_cardShadow);
        s_shadowInit = 1;
    }
}

static void card_read(u32 offset, void *dest, u32 len) {
    if (!dest || len == 0) {
        return;
    }
    shadow_ensure();
    memset(dest, 0xFF, len);
    if (offset >= PBW2_CARD_SHADOW_SIZE) {
        return;
    }
    if (offset + len > PBW2_CARD_SHADOW_SIZE) {
        len = PBW2_CARD_SHADOW_SIZE - offset;
    }
    memcpy(dest, s_cardShadow + offset, len);
}

static void card_write(u32 offset, const void *src, u32 len) {
    if (!src || len == 0 || offset >= PBW2_CARD_SHADOW_SIZE) {
        return;
    }
    shadow_ensure();
    if (offset + len > PBW2_CARD_SHADOW_SIZE) {
        len = PBW2_CARD_SHADOW_SIZE - offset;
    }
    memcpy(s_cardShadow + offset, src, len);
}

void PbW2_PxiServiceArm7(int fifoTag, u32 data) {
    if (!s_loggedCard && fifoTag == PXI_FIFO_TAG_FS) {
        s_loggedCard = 1;
        PBW2_STUB_LOG("PXI", "card service active");
    }

    switch (fifoTag) {
    case PXI_FIFO_TAG_FS: {
        CARDiCommandArg *cmd = cardi_common.cmd;
        int req = (int)data;

        cmd->result = CARD_RESULT_SUCCESS;
        switch (req) {
        case CARD_REQ_INIT:
        case CARD_REQ_IDENTIFY:
            break;
        case CARD_REQ_READ_BACKUP:
            card_read(cmd->src, (void *)cmd->dst, cmd->len);
            break;
        case CARD_REQ_WRITE_BACKUP:
        case CARD_REQ_PROGRAM_BACKUP:
        case CARD_REQ_VERIFY_BACKUP:
            card_write(cmd->dst, (void *)cmd->src, cmd->len);
            break;
        case CARD_REQ_WRITE_STATUS:
        case CARD_REQ_ERASE_PAGE_BACKUP:
        case CARD_REQ_ERASE_SECTOR_BACKUP:
        case CARD_REQ_ERASE_CHIP_BACKUP:
        case CARD_REQ_ERASE_SUBSECTOR_BACKUP:
            break;
        default:
            cmd->result = CARD_RESULT_SUCCESS;
            break;
        }
        break;
    }
    case PXI_FIFO_TAG_TOUCHPANEL:
    case PXI_FIFO_TAG_SOUND:
    case PXI_FIFO_TAG_PM:
    case PXI_FIFO_TAG_MIC:
    case PXI_FIFO_TAG_RTC:
    case PXI_FIFO_TAG_NVRAM:
    case PXI_FIFO_TAG_CARD:
    case PXI_FIFO_TAG_CTRDG:
    case PXI_FIFO_TAG_SYSTEM:
    case PXI_FIFO_TAG_OS:
        break;
    default:
        break;
    }
}