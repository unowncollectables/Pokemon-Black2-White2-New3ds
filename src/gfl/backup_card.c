#include "types.h"
#include "gfl/backup_card.h"
#include "gfl/std.h"
#include "nitro/card.h"
#include "nitro/os.h"

#ifdef PBW2_N3DS
#include "save_shim.h"

static BOOL sHostWriteResult;
#endif

static void setCardWrittenVerified(void *arg);
static void unlockBackupAndReportError(u16 lockId, BOOL noResponse, BackupWriteErrorCallback callback);

// Whether the last write has ended
static BOOL sWriteDone = TRUE;

static BOOL sLocked;

BOOL identifyGameCardBackup(void) {
#ifdef PBW2_N3DS
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
    BOOL result = Save_GameWrite(dest, src, size);
    if (!result && callback != NULL) {
        callback(FALSE);
    }
    return result;
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
    BOOL result = Save_GameRead(src, dest, size);
    if (!result && callback != NULL) {
        callback();
    }
    return result;
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
    (void)callback;
    /* The host performs the request synchronously and reports its result
     * through the same completion call as the cartridge path. */
    sHostWriteResult = Save_GameWrite(dest, src, size);
    return 0;
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
    if (result != NULL) {
        *result = sHostWriteResult;
    }
    if (!sHostWriteResult && callback != NULL) {
        callback(FALSE);
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
