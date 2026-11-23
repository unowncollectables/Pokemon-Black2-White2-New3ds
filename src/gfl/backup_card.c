#include "types.h"
#include "gfl/backup_card.h"
#include "gfl/std.h"
#include "nitro/card.h"
#include "nitro/os.h"

#ifdef PBW2_N3DS
#include "stub_log.h"
#include <string.h>

static int s_backup_host_logged;

static void backup_host_log_once(void) {
    if (!s_backup_host_logged) {
        s_backup_host_logged = 1;
        PBW2_STUB_LOG("save", "backup_card host (no async CARD worker)");
    }
}
#endif

static void setCardWrittenVerified(void *arg);
static void unlockBackupAndReportError(u16 lockId, BOOL noResponse, BackupWriteErrorCallback callback);

// Whether the last write has ended
static BOOL sWriteDone = TRUE;

static BOOL sLocked;

BOOL identifyGameCardBackup(void) {
#ifdef PBW2_N3DS
    backup_host_log_once();
    return TRUE;
#else
    s32 lock_id = OS_GetLockID();
    CARDBackupType type;

    GFL_ASSERT(lock_id != OS_LOCK_ID_ERROR);
    CARD_LockBackup(lock_id);
    type = CARD_BACKUP_TYPE_FLASH_4MBITS_EX;
    if (!CARD_IdentifyBackup(type)) {
        type = 0;
    }
    CARD_UnlockBackup(lock_id);
    OS_ReleaseLockID(lock_id);
    return type != 0 ? TRUE : FALSE;
#endif
}

BOOL writeFlashWaitSuccess(u32 dest, const void *src, u32 size, BackupWriteErrorCallback callback) {
#ifdef PBW2_N3DS
    (void)dest;
    (void)src;
    (void)size;
    (void)callback;
    return TRUE;
#else
    BOOL result;
    u16 lockId = startWriteFlash(dest, src, size, callback);

    while (!finishBackupGetResult(lockId, &result, callback)) {
    }
    return result;
#endif
}

BOOL readFlashWaitSuccess(u32 src, void *dest, u32 size, BackupReadErrorCallback callback) {
#ifdef PBW2_N3DS
    (void)src;
    (void)callback;
    if (dest != NULL && size != 0) {
        memset(dest, 0xFF, size);
    }
    return FALSE;
#else
    s32 lock_id = OS_GetLockID();
    BOOL result;

    GFL_ASSERT(lock_id != OS_LOCK_ID_ERROR);
    CARD_LockBackup(lock_id);
    CARD_ReadBackupAsync(src, dest, size, NULL, NULL);
    result = CARD_WaitBackupAsync();
    CARD_UnlockBackup(lock_id);
    OS_ReleaseLockID(lock_id);
    if (!result && callback != NULL) {
        callback();
    }
    return result;
#endif
}

static void setCardWrittenVerified(void *arg) {
    sWriteDone = TRUE;
}

u16 startWriteFlash(u32 dest, const void *src, u32 size, BackupWriteErrorCallback callback) {
#ifdef PBW2_N3DS
    (void)dest;
    (void)src;
    (void)size;
    (void)callback;
    sWriteDone = TRUE;
    return (u16)OS_GetLockID();
#else
    u32 header;
    s32 lock_id = OS_GetLockID();

    GFL_ASSERT(lock_id != OS_LOCK_ID_ERROR);
    CARD_LockBackup(lock_id);
    sLocked = TRUE;
    if (!CARD_ReadBackup(0, &header, sizeof(header))) {
        unlockBackupAndReportError(lock_id, TRUE, callback);
    }
    sWriteDone = FALSE;
    CARD_WriteAndVerifyBackupAsync(dest, src, size, setCardWrittenVerified, NULL);
    return lock_id;
#endif
}

BOOL finishBackupGetResult(u16 lockId, BOOL *result, BackupWriteErrorCallback callback) {
#ifdef PBW2_N3DS
    (void)lockId;
    (void)callback;
    if (result != NULL) {
        *result = TRUE;
    }
    return TRUE;
#else
    if (sWriteDone == TRUE) {
        CARD_UnlockBackup(lockId);
        OS_ReleaseLockID(lockId);
        sLocked = FALSE;
        switch (CARD_GetResultCode()) {
        case CARD_RESULT_SUCCESS:
            *result = TRUE;
            break;
        default:
        case CARD_RESULT_FAILURE:
        case CARD_RESULT_INVALID_PARAM:
        case CARD_RESULT_UNSUPPORTED:
        case CARD_RESULT_TIMEOUT:
        case CARD_RESULT_ERROR:
        case CARD_RESULT_CANCELED:
            *result = FALSE;
            if (callback != NULL) {
                callback(FALSE);
            }
            break;
        case CARD_RESULT_NO_RESPONSE:
            *result = FALSE;
            if (callback != NULL) {
                callback(TRUE);
            }
            break;
        }
        return TRUE;
    }
    return FALSE;
#endif
}

void cancelCardProcess(u16 lockId) {
#ifdef PBW2_N3DS
    (void)lockId;
    sLocked = FALSE;
    return;
#else
    if (!CARD_TryWaitBackupAsync()) {
        CARD_CancelBackupAsync();
        CARD_WaitBackupAsync();
    }
    if (sLocked == TRUE) {
        CARD_UnlockBackup(lockId);
        OS_ReleaseLockID(lockId);
        sLocked = FALSE;
    }
#endif
}

static void unlockBackupAndReportError(u16 lockId, BOOL noResponse, BackupWriteErrorCallback callback) {
    CARD_UnlockBackup(lockId);
    OS_ReleaseLockID(lockId);
    sLocked = FALSE;
    if (callback != NULL) {
        callback(noResponse);
    }
}

BOOL getLockIDStatus(void) {
    return sLocked;
}
