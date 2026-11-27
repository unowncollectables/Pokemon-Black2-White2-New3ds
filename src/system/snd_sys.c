#include "types.h"
#include "constants/sound.h"
#include "gfl/sound.h"
#include "nitro/os.h"
#include "nnsys/snd.h"
#include "twl/sndex.h"

// The game's sound system: it opens the sound archive and sets up the players, plays the BGM (with the BGM stack of
// snd_bgm_stack.c, which pushes and pops a BGM with its sequence and banks loaded) and the sound effects, and changes
// the BGM with a fade, loading the next one in a thread while the old one fades out. The file's name is descriptive,
// a guess: the ROM has no string for it. Function names from swan (https://github.com/ds-pokemon-hacking/swan,
// GPL-3.0), except StopBGM, InitBGMChange, RequestBGMChange, ResetBGMChange, FinishBGMChange, UpdateBGMChange,
// StartLoadThread, StopLoadThread and IsLoadThreadDone. The data's names are swan's (g_SndPlayerStates,
// g_GFLSndThread and g_GFLSndHeap) or ours, as are the types, constants and fields

// The players of sound effects and the like, whose sequences GFL_SndSEPlay starts
#define SND_PLAYER_COUNT 5
#define SND_TRACK_COUNT 16
#define SND_THREAD_STACK_SIZE 0x400
#define SND_THREAD_PRIORITY 17
#define SND_HEAP_SIZE 0x9d000
// What NNS_SndArcSetLoadBlockSize reads at a time while a thread loads the next BGM
#define SND_LOAD_BLOCK_SIZE 0x1300
// The player of BGM2, which plays over the BGM's
#define SND_PLAYER_BGM2 6

// How the next BGM loads: its sequence and bank, then its wave archives, each in the thread
enum {
    BGM_LOAD_IDLE,
    BGM_LOAD_SEQ,
    BGM_LOAD_WAVE,
};

// The kind of file the loading thread loads
enum {
    SND_THREAD_LOAD_SEQ,
    SND_THREAD_LOAD_WAVE,
};

// A change of BGM with a fade: the next BGM and its channel mask, how far it has loaded, and the fade's frames
typedef struct {
    BOOL active;
    s32 loadState;
    u16 fadeCounter;
    u16 channelMask;
    u32 seq;
    s32 fadeInFrames;
    s32 fadeOutFrames;
} BGMChange;

// A sound effect player: the sequence it was last started with, and its handle
typedef struct {
    u32 seq;
    NNSSndHandle handle;
} SndPlayerState;

static void GFL_SndInitCore(BOOL loadGroup);
static BOOL GFL_SndSeqVerify(u32 seq);
static void StopBGM(void);
static BOOL GFL_SndBGMPlayCore(u32 seq, u32 channelMask);
static void GFL_SndBGMFadeInEx(u16 frames, s32 volume);
static void GFL_SndBGMSetChannelMask(u32 channelMask);
static void InitBGMChange(void);
static void RequestBGMChange(u32 seq, u16 channelMask, s32 fadeOutFrames, s32 fadeInFrames);
static void ResetBGMChange(void);
static void FinishBGMChange(void);
static void UpdateBGMChange(void);
static void GFL_SndResetPlayerStates(void);
static void GFL_SndSEPlayCore(u32 seq, u32 volume);
static void GFL_SndThreadProc2(void *arg);
static void GFL_SndThreadProc1(void *arg);
static void StartLoadThread(u32 seq, int kind);
static void StopLoadThread(void);
static BOOL IsLoadThreadDone(void);

// The tracks of the BGM that were playing in the last frame
static u16 sTrackMask;
// The sequence the loading thread loads
static u32 sThreadSeq;
// The loading thread, once started
static OSThread *sThread;
// Written once, never read
static u32 sUnused;
// The frames left of the BGM's fade in or out
static s32 sFadeFrames;
#ifdef PBW2_N3DS
// Port (audio decoupling audit, findings #5/#6): the BGM crossfade and the
// sFadeFrames report are paced by wall-clock time, never by rendered
// frames. The fade durations stay authored in 60 Hz frames — that is the
// game's design unit — but they are handed to the sequence player's
// wall-clock MoveVolume fade and polled against elapsed time here, so a
// fade lasts the same number of SECONDS at any frame rate.
// sBGMFadePhaseTick is the OS_GetTick() at the start of the current
// BGM-change fade phase (fade-out, then fade-in); sFadeStartTick is the
// same for the standalone GFL_SndBGMFadeIn/FadeOut fades.
static u64 sBGMFadePhaseTick;
static u64 sFadeStartTick;

// Whole 60 Hz frames elapsed since tick0, by the wall clock.
static s32 SndElapsedFrames(u64 tick0) {
    return (s32)((OS_GetTick() - tick0) * 60 * 64 / OS_SYSTEM_CLOCK);
}
#endif
static BOOL (*sSeqVerifyCallback)(u32 seq);
static NNSSndHeapHandle sSndHeap;
static BGMChange sBGMChange;
static SndPlayerState g_SndPlayerStates[SND_PLAYER_COUNT];
static NNSSndArc sSndArc;
OSThread g_GFLSndThread;
static SNDTrackInfo sTrackInfo[SND_TRACK_COUNT];
#ifndef PBW2_N3DS
static u32 sThreadStack[SND_THREAD_STACK_SIZE / sizeof(u32)];
#endif
u8 g_GFLSndHeap[SND_HEAP_SIZE];

void GFL_SndInit(void) {
    NNS_SndInit();
    sSndHeap = NNS_SndHeapCreate(g_GFLSndHeap, sizeof(g_GFLSndHeap));
    NNS_SndArcInit(&sSndArc, "swan_sound_data.sdat", sSndHeap, FALSE);
    GFL_SndInitCore(TRUE);
    SNDEX_Init();
}

static void GFL_SndInitCore(BOOL loadGroup) {
    NNS_SndArcPlayerSetup(sSndHeap);
    GFL_SndBGMStack_SetHeap(&sSndHeap);
    if (loadGroup) {
        // The group's info goes unused
        NNS_SndArcGetGroupInfo(0);
        NNS_SndArcLoadGroup(0, sSndHeap);
        sUnused = 0;
    }
    GFL_SndResetPlayerStates();
    GFL_SndBGMStack_Init(0);
    InitBGMChange();
    sTrackMask = 0;
    sFadeFrames = 0;
    sThread = NULL;
    sSeqVerifyCallback = NULL;
}

void GFL_SndUpdate(void) {
    NNSSndHandle *handle;
    int i;

    UpdateBGMChange();
    NNS_SndMain();
    NNS_SndUpdateDriverInfo();
    handle = GFL_SndBGMGetHandle();
    sTrackMask = 0;
    for (i = 0; i < SND_TRACK_COUNT; i++) {
        if (NNS_SndPlayerReadDriverTrackInfo(handle, i, &sTrackInfo[i]) == TRUE) {
            sTrackMask |= (u16)(1 << i);
        }
    }
#ifdef PBW2_N3DS
    // The audible fade is the player's wall-clock MoveVolume fade; the
    // report count follows the same clock (audit finding #6) instead of
    // counting rendered frames.
    if (sFadeFrames != 0 && SndElapsedFrames(sFadeStartTick) >= sFadeFrames) {
        sFadeFrames = 0;
    }
#else
    if (sFadeFrames != 0) {
        sFadeFrames--;
    }
#endif
}

void GFL_SndDestroyHeap(void) {
    NNS_SndHeapDestroy(sSndHeap);
}

void GFL_SndSetStereo(BOOL stereo) {
    NNS_SndSetMonoFlag(stereo == TRUE ? FALSE : TRUE);
}

NNSSndHandle *GFL_SndBGMGetHandle(void) {
    return GFL_SndBGMStack_GetHandle();
}

u32 GFL_SndBGMGetID(void) {
    return GFL_SndGetLastResID();
}

u32 GFL_SndBGMGetRequestedID(void) {
    if (sBGMChange.active == FALSE) {
        return GFL_SndGetLastResID();
    }
    return sBGMChange.seq;
}

BOOL GFL_SndIsLoadThreadRunning(void) {
    if (sThread == NULL) {
        return FALSE;
    }
    if (OS_IsThreadTerminated(&g_GFLSndThread) == FALSE) {
        return TRUE;
    }
    return FALSE;
}

NNSSndHeapHandle GFL_SndGetHeap(void) {
    return sSndHeap;
}

BOOL GFL_IRQCartDataTransferIsEnabled(void) {
    if (reg_OS_IE & OS_IE_CARD_DATA) {
        return TRUE;
    }
    return FALSE;
}

void GFL_SndPlayerSetMuteStateEx(BOOL on, u32 playerMask) {
    if (on == TRUE) {
        GFL_SndPlayerSetVolumeEx(SND_VOLUME_MAX, playerMask);
    } else {
        GFL_SndPlayerSetVolumeEx(0, playerMask);
    }
}

void GFL_SndPlayerSetVolumeEx(u32 volume, u32 playerMask) {
    int i;

    for (i = 0; i < SND_PLAYER_COUNT; i++) {
        if ((1 << i) & playerMask) {
            NNS_SndPlayerSetPlayerVolume(i, volume);
        }
    }
}

static BOOL GFL_SndSeqVerify(u32 seq) {
    if (GFL_SndIsLoadThreadRunning() == FALSE) {
        return TRUE;
    }
    if (sSeqVerifyCallback == NULL) {
        return FALSE;
    }
    return sSeqVerifyCallback(seq);
}

void GFL_SndSetSeqVerifyCallback(BOOL (*callback)(u32 seq)) {
    sSeqVerifyCallback = callback;
}

static void StopBGM(void) {
    NNS_SndPlayerStopSeq(GFL_SndBGMStack_GetHandle(), 0);
    GFL_SndBGMStack_FreeSeq();
}

void GFL_SndBGMStopImmediate(void) {
    ResetBGMChange();
    StopBGM();
}

static BOOL GFL_SndBGMPlayCore(u32 seq, u32 channelMask) {
    BOOL result;

    if (GFL_SndSeqVerify(seq) == FALSE) {
        return FALSE;
    }
    StopBGM();
    if (GFL_SndBGMStack_LoadSeq(seq) == FALSE) {
        return FALSE;
    }
    result = NNS_SndArcPlayerStartSeqEx(GFL_SndBGMStack_GetHandle(), 0, -1, -1, seq);
    if (result == FALSE) {
        return FALSE;
    }
    if (channelMask != SND_CHANNEL_MASK_ALL) {
        GFL_SndBGMSetChannelMask(channelMask);
    }
    return result;
}

void GFL_SndBGMPlay(u32 bgm, u32 channelMask) {
    ResetBGMChange();
    GFL_SndBGMPlayCore(bgm, channelMask);
}

void GFL_SndBGMRequestChange(u32 bgm, u16 channelMask, s32 fadeOutFrames, s32 fadeInFrames) {
    RequestBGMChange(bgm, channelMask, fadeOutFrames, fadeInFrames);
}

void GFL_SndBGMStop(u32 bgm) {
    ResetBGMChange();
    if (GFL_SndSeqVerify(bgm)) {
        StopBGM();
        if (GFL_SndBGMStack_LoadSeq(bgm)) {
            NNS_SndArcPlayerStartSeqEx(GFL_SndBGMStack_GetHandle(), SND_PLAYER_BGM2, -1, -1, bgm);
        }
    }
}

u32 GFL_SndBGMGetTick(void) {
    return NNS_SndPlayerGetTick(GFL_SndBGMGetHandle());
}

void GFL_SndBGMSetPaused(BOOL paused) {
    FinishBGMChange();
    NNS_SndPlayerPause(GFL_SndBGMStack_GetHandle(), paused);
}

void GFL_SndBGMFadeIn(u16 frames) {
    GFL_SndBGMFadeInEx(frames, SND_VOLUME_MAX);
}

static void GFL_SndBGMFadeInEx(u16 frames, s32 volume) {
    FinishBGMChange();
    NNS_SndPlayerMoveVolume(GFL_SndBGMStack_GetHandle(), 0, 0);
    NNS_SndPlayerMoveVolume(GFL_SndBGMStack_GetHandle(), volume, frames);
    sFadeFrames = frames;
#ifdef PBW2_N3DS
    sFadeStartTick = OS_GetTick();
#endif
}

void GFL_SndBGMFadeOut(u16 frames) {
    FinishBGMChange();
    NNS_SndPlayerMoveVolume(GFL_SndBGMStack_GetHandle(), 0, frames);
    sFadeFrames = frames;
#ifdef PBW2_N3DS
    sFadeStartTick = OS_GetTick();
#endif
}

BOOL GFL_SndBGMIsFading(void) {
    if (sFadeFrames != 0) {
        return TRUE;
    }
    return FALSE;
}

void GFL_SndBGMPush(void) {
    FinishBGMChange();
    NNS_SndPlayerSetTrackMute(GFL_SndBGMStack_GetHandle(), SND_CHANNEL_MASK_ALL, FALSE);
    NNS_SndPlayerSetTrackModDepth(GFL_SndBGMStack_GetHandle(), SND_CHANNEL_MASK_ALL, 0);
    NNS_SndPlayerSetTrackModSpeed(GFL_SndBGMStack_GetHandle(), SND_CHANNEL_MASK_ALL, 16);
    GFL_SndBGMStack_Push();
}

void GFL_SndBGMPop(void) {
    FinishBGMChange();
    if (GFL_SndBGMStack_GetDepth() != 0) {
        StopBGM();
        GFL_SndBGMStack_Pop();
    }
}

static void GFL_SndBGMSetChannelMask(u32 channelMask) {
    u16 muted = channelMask ^ SND_CHANNEL_MASK_ALL;

    NNS_SndPlayerSetTrackMute(GFL_SndBGMStack_GetHandle(), SND_CHANNEL_MASK_ALL, FALSE);
    if (muted != 0) {
        NNS_SndPlayerSetTrackMuteEx(GFL_SndBGMStack_GetHandle(), muted, NNS_SND_SEQ_MUTE_NO_STOP);
    }
}

void GFL_SndBGMSetParams(u16 trackMask, s32 tempoRatio, s32 pitch, s32 pan) {
    NNSSndHandle *handle = GFL_SndBGMStack_GetHandle();

    if (tempoRatio != -1) {
        NNS_SndPlayerSetTempoRatio(handle, tempoRatio);
    }
    if (pitch != -1) {
        NNS_SndPlayerSetTrackPitch(handle, trackMask, pitch);
    }
    if (pan != -1) {
        NNS_SndPlayerSetTrackPan(handle, trackMask, pan);
    }
}

void GFL_SndBGMSetVolume(u16 trackMask, s32 volume) {
    NNS_SndPlayerSetTrackVolume(GFL_SndBGMStack_GetHandle(), trackMask, volume);
}

BOOL GFL_SndBGMIsPlaying(void) {
    if (GFL_SndGetLastResID() == 0) {
        return FALSE;
    }
    if (NNS_SndPlayerCountPlayingSeqBySeqNo(GFL_SndGetLastResID()) != 0) {
        return TRUE;
    }
    return FALSE;
}

static void InitBGMChange(void) {
    sBGMChange.fadeInFrames = 60;
    sBGMChange.fadeOutFrames = 60;
    ResetBGMChange();
}

static void RequestBGMChange(u32 seq, u16 channelMask, s32 fadeOutFrames, s32 fadeInFrames) {
    sBGMChange.channelMask = channelMask;
    sBGMChange.seq = seq;
    sBGMChange.fadeInFrames = fadeInFrames;
    sBGMChange.fadeOutFrames = fadeOutFrames;
    sBGMChange.fadeCounter = fadeOutFrames;
    StopLoadThread();
    sBGMChange.loadState = BGM_LOAD_IDLE;
    sBGMChange.active = TRUE;
#ifdef PBW2_N3DS
    // Start the fade-out on the sequence player's wall-clock MoveVolume
    // fade; UpdateBGMChange only polls its elapsed time to gate the new
    // BGM's load, so the change happens on a time schedule at any frame
    // rate. With no different BGM playing there is nothing to fade and
    // the load proceeds at once, as before.
    sBGMFadePhaseTick = OS_GetTick();
    if (seq != 0 && GFL_SndGetLastResID() != 0 && GFL_SndGetLastResID() != seq) {
        NNS_SndPlayerMoveVolume(GFL_SndBGMStack_GetHandle(), 0, fadeOutFrames);
    }
#endif
}

static void ResetBGMChange(void) {
    sBGMChange.active = FALSE;
    sBGMChange.channelMask = SND_CHANNEL_MASK_ALL;
    sBGMChange.seq = 0;
    sBGMChange.fadeCounter = 0;
    NNS_SndPlayerSetVolume(GFL_SndBGMStack_GetHandle(), SND_VOLUME_MAX);
    sBGMChange.loadState = BGM_LOAD_IDLE;
    StopLoadThread();
}

static void FinishBGMChange(void) {
    u32 seq;

    if (sBGMChange.active == TRUE) {
        seq = GFL_SndGetLastResID();
        if (seq != sBGMChange.seq) {
            while (sBGMChange.loadState != BGM_LOAD_IDLE) {
                UpdateBGMChange();
            }
            GFL_SndBGMPlay(sBGMChange.seq, sBGMChange.channelMask);
        }
        ResetBGMChange();
    }
}

static void UpdateBGMChange(void) {
    NNSSndHandle *handle = GFL_SndBGMStack_GetHandle();
    u32 seq = GFL_SndGetLastResID();

    if (sBGMChange.active == FALSE) {
        return;
    }
    switch (sBGMChange.loadState) {
    case BGM_LOAD_IDLE:
        if (seq != 0) {
            if (seq != sBGMChange.seq) {
#ifdef PBW2_N3DS
                // The fade-out runs on the player's wall-clock MoveVolume
                // fade (see RequestBGMChange). Poll elapsed time only:
                // while the authored duration has not elapsed, wait; once
                // it has, fall through and load the new BGM. fadeCounter
                // mirrors the frames remaining, derived from the clock.
                {
                    s32 elapsed = SndElapsedFrames(sBGMFadePhaseTick);
                    sBGMChange.fadeCounter = elapsed < sBGMChange.fadeOutFrames
                                                 ? (u16)(sBGMChange.fadeOutFrames - elapsed)
                                                 : 0;
                    if (sBGMChange.fadeCounter != 0) {
                        return;
                    }
                }
#else
                if (sBGMChange.fadeCounter != 0) {
                    sBGMChange.fadeCounter--;
                    NNS_SndPlayerSetVolume(handle, SND_VOLUME_MAX * sBGMChange.fadeCounter / sBGMChange.fadeOutFrames);
                    return;
                }
                NNS_SndPlayerSetVolume(handle, 0);
#endif
            } else {
#ifdef PBW2_N3DS
                // Fade-in: likewise a wall-clock MoveVolume fade, started
                // when the new sequence starts (BGM_LOAD_WAVE below).
                // Poll its elapsed time and finish on schedule.
                {
                    s32 elapsed = SndElapsedFrames(sBGMFadePhaseTick);
                    sBGMChange.fadeCounter = elapsed < sBGMChange.fadeInFrames
                                                 ? (u16)elapsed
                                                 : (u16)sBGMChange.fadeInFrames;
                    if (sBGMChange.fadeCounter < sBGMChange.fadeInFrames) {
                        return;
                    }
                }
                ResetBGMChange();
                return;
#else
                if (sBGMChange.fadeCounter < sBGMChange.fadeInFrames) {
                    sBGMChange.fadeCounter++;
                    NNS_SndPlayerSetVolume(handle, SND_VOLUME_MAX * sBGMChange.fadeCounter / sBGMChange.fadeInFrames);
                    return;
                }
                ResetBGMChange();
                return;
#endif
            }
        }
        StopBGM();
        if (sBGMChange.seq == 0) {
            ResetBGMChange();
            return;
        }
        NNS_SndArcSetLoadBlockSize(SND_LOAD_BLOCK_SIZE);
        StartLoadThread(sBGMChange.seq, SND_THREAD_LOAD_SEQ);
        sBGMChange.fadeCounter = 0;
        sBGMChange.loadState = BGM_LOAD_SEQ;
        break;
    case BGM_LOAD_SEQ:
        if (IsLoadThreadDone() == TRUE) {
            GFL_SndBGMStack_SaveSeqLevel();
            StartLoadThread(sBGMChange.seq, SND_THREAD_LOAD_WAVE);
            sBGMChange.loadState = BGM_LOAD_WAVE;
        } else {
            OS_Sleep(1);
        }
        break;
    case BGM_LOAD_WAVE:
        if (IsLoadThreadDone() == TRUE) {
            NNS_SndArcSetLoadBlockSize(0);
            GFL_SndBGMStack_SetLoadedSeq(sBGMChange.seq);
            NNS_SndArcPlayerStartSeqEx(GFL_SndBGMStack_GetHandle(), 0, -1, -1, sBGMChange.seq);
#ifdef PBW2_N3DS
            // Fade the new BGM in on the player's wall-clock MoveVolume
            // fade: snap to silence, then rise to full over the authored
            // fadeInFrames duration.
            NNS_SndPlayerMoveVolume(handle, 0, 0);
            NNS_SndPlayerMoveVolume(handle, SND_VOLUME_MAX, sBGMChange.fadeInFrames);
            sBGMFadePhaseTick = OS_GetTick();
            sBGMChange.fadeCounter = 0;
#else
            NNS_SndPlayerSetVolume(handle, 0);
#endif
            sBGMChange.loadState = BGM_LOAD_IDLE;
        } else {
            OS_Sleep(1);
        }
        break;
    }
}

static void GFL_SndResetPlayerStates(void) {
    int i;

    for (i = 0; i < SND_PLAYER_COUNT; i++) {
        g_SndPlayerStates[i].seq = 0;
        NNS_SndHandleInit(&g_SndPlayerStates[i].handle);
    }
}

NNSSndHandle *GFL_SndPlayerGetHandle(s32 player) {
    return &g_SndPlayerStates[player].handle;
}

s32 GFL_SndSeqGetPlayerIndex(u32 seq) {
    if (seq < SEQ_SE_DUMMY || seq >= SEQ_SE_END) {
        return 1;
    }
    return NNS_SndArcGetSeqParam(seq)->playerNo - 1;
}

void GFL_SEPlayKeepVol(u32 se, s32 player) {
    SndPlayerState *state;

    if (GFL_SndSeqVerify(se)) {
        state = &g_SndPlayerStates[player];
        state->seq = 0;
        if (NNS_SndArcPlayerStartSeqEx(&state->handle, player + 1, -1, -1, se) == TRUE) {
            state->seq = se;
        }
    }
}

static void GFL_SndSEPlayCore(u32 seq, u32 volume) {
    s32 player;
    SndPlayerState *state;

    if (GFL_SndSeqVerify(seq)) {
        player = GFL_SndSeqGetPlayerIndex(seq);
        state = &g_SndPlayerStates[player];
        state->seq = 0;
        if (NNS_SndArcPlayerStartSeq(&state->handle, seq) == TRUE) {
            state->seq = seq;
            if (volume < 128) {
                GFL_SndPlayerSetVolume(player, volume);
            }
        }
    }
}

void GFL_SndSEPlay(u32 se) {
    GFL_SndSEPlayCore(se, -1);
}

void GFL_SndSEPlayEx(u32 se, u32 volume) {
    GFL_SndSEPlayCore(se, volume);
}

void GFL_SndPlayerStop(s32 player) {
    NNS_SndPlayerStopSeq(&g_SndPlayerStates[player].handle, 0);
}

void GFL_SndStop(void) {
    int i;

    for (i = 0; i < SND_PLAYER_COUNT; i++) {
        GFL_SndPlayerStop(i);
    }
}

BOOL GFL_SndPlayerIsActive(s32 player) {
    if (NNS_SndPlayerCountPlayingSeqByPlayerNo(player + 1) != 0) {
        return TRUE;
    }
    return FALSE;
}

BOOL GFL_SndPlayerIsActiveAny(void) {
    int i;

    for (i = 0; i < SND_PLAYER_COUNT; i++) {
        if (GFL_SndPlayerIsActive(i) == TRUE) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL GFL_SndIsPlaying(u32 seq) {
    if (NNS_SndPlayerCountPlayingSeqBySeqNo(seq) != 0) {
        return TRUE;
    }
    return FALSE;
}

void GFL_SndPlayerSetParams(s32 player, s32 tempoRatio, s32 pitch, s32 pan) {
    if (tempoRatio != -1) {
        NNS_SndPlayerSetTempoRatio(&g_SndPlayerStates[player].handle, tempoRatio);
    }
    if (pitch != -1) {
        NNS_SndPlayerSetTrackPitch(&g_SndPlayerStates[player].handle, SND_CHANNEL_MASK_ALL, pitch);
    }
    if (pan != -1) {
        NNS_SndPlayerSetTrackPan(&g_SndPlayerStates[player].handle, SND_CHANNEL_MASK_ALL, pan);
    }
}

void GFL_SndPlayerSetVolume(s32 player, u32 volume) {
    if (volume > SND_VOLUME_MAX) {
        volume = SND_VOLUME_MAX;
    }
    NNS_SndPlayerSetVolume(&g_SndPlayerStates[player].handle, volume);
}

static void GFL_SndThreadProc2(void *arg) {
    NNS_SndArcLoadSeqEx(*(u32 *)arg, NNS_SND_ARC_LOAD_SEQ | NNS_SND_ARC_LOAD_BANK, sSndHeap);
}

static void GFL_SndThreadProc1(void *arg) {
    NNS_SndArcLoadSeqEx(*(u32 *)arg, NNS_SND_ARC_LOAD_WAVE, sSndHeap);
}

static void StartLoadThread(u32 seq, int kind) {
    sThreadSeq = seq;
    StopLoadThread();
#ifdef PBW2_N3DS
    // The native sound backend completes archive loads synchronously.
    switch (kind) {
    case SND_THREAD_LOAD_SEQ:
        GFL_SndThreadProc2(&sThreadSeq);
        break;
    case SND_THREAD_LOAD_WAVE:
        GFL_SndThreadProc1(&sThreadSeq);
        break;
    }
#else
    switch (kind) {
    case SND_THREAD_LOAD_SEQ:
        OS_CreateThread(&g_GFLSndThread, GFL_SndThreadProc2, &sThreadSeq, sThreadStack + NELEMS(sThreadStack),
                        SND_THREAD_STACK_SIZE, SND_THREAD_PRIORITY);
        break;
    case SND_THREAD_LOAD_WAVE:
        OS_CreateThread(&g_GFLSndThread, GFL_SndThreadProc1, &sThreadSeq, sThreadStack + NELEMS(sThreadStack),
                        SND_THREAD_STACK_SIZE, SND_THREAD_PRIORITY);
        break;
    }
    OS_WakeupThreadDirect(&g_GFLSndThread);
    sThread = &g_GFLSndThread;
#endif
}

static void StopLoadThread(void) {
    if (sThread != NULL && OS_IsThreadTerminated(&g_GFLSndThread) == FALSE) {
        OS_DestroyThread(&g_GFLSndThread);
        sThread = NULL;
    }
}

static BOOL IsLoadThreadDone(void) {
    if (sThread == NULL) {
        return TRUE;
    }
    return OS_IsThreadTerminated(&g_GFLSndThread);
}

BOOL GFL_SndBGMLoadStep(u32 seq, u32 *step, BOOL start) {
    if (start == TRUE) {
        StopBGM();
        StopLoadThread();
        *step = 0;
    } else {
        switch (*step) {
        case 0:
            NNS_SndArcSetLoadBlockSize(SND_LOAD_BLOCK_SIZE);
            StartLoadThread(seq, SND_THREAD_LOAD_SEQ);
            (*step)++;
            break;
        case 1:
            if (IsLoadThreadDone() == TRUE) {
                GFL_SndBGMStack_SaveSeqLevel();
                StartLoadThread(seq, SND_THREAD_LOAD_WAVE);
                (*step)++;
            } else {
                OS_Sleep(2);
            }
            break;
        case 2:
            if (IsLoadThreadDone() == TRUE) {
                NNS_SndArcSetLoadBlockSize(0);
                GFL_SndBGMStack_SetLoadedSeq(seq);
                NNS_SndArcPlayerStartSeqEx(GFL_SndBGMStack_GetHandle(), 0, -1, -1, seq);
                (*step)++;
                return TRUE;
            }
            OS_Sleep(2);
            break;
        }
    }
    return FALSE;
}

BOOL GFL_SndBGMSetLoadedData(void *seqData, void *bankData, u32 seq) {
    const NNSSndArcSeqInfo *seqInfo = NNS_SndArcGetSeqInfo(seq);
    u32 seqFileId = seqInfo->fileId;
    const NNSSndArcBankInfo *bankInfo = NNS_SndArcGetBankInfo(seqInfo->param.bankNo);
    u32 bankFileId = bankInfo->fileId;
    int i;

    GFL_SndBGMStack_SaveSeqLevel();
    for (i = 0; i < NNS_SND_ARC_BANK_TO_WAVEARC_NUM; i++) {
        if (bankInfo->waveArcNo[i] != NNS_SND_ARC_INVALID_WAVEARC_NO) {
            if (NNS_SndArcLoadWaveArc(bankInfo->waveArcNo[i], sSndHeap) == FALSE) {
                return FALSE;
            }
        }
    }
    NNS_SndArcSetFileAddress(bankFileId, bankData);
    NNS_SndArcSetFileAddress(seqFileId, seqData);
    GFL_SndBGMStack_SetLoadedSeq(seq);
    return TRUE;
}

BOOL GFL_SndReplaceWave(u16 waveArc, u16 index, const SNDWaveArc *srcWaveArc, u16 srcIndex) {
    const NNSSndArcWaveArcInfo *info = NNS_SndArcGetWaveArcInfo(waveArc);
    SNDWaveArc *loaded;

    if (info == NULL) {
        return FALSE;
    }
    loaded = (SNDWaveArc *)NNS_SndArcGetFileAddress(info->fileId);
    if (loaded == NULL) {
        return FALSE;
    }
    SND_SetWaveDataAddress(loaded, index, SND_GetWaveDataAddress(srcWaveArc, srcIndex));
    return TRUE;
}

void GFL_SndBGMStartSeq(u16 seq) {
    NNS_SndArcPlayerStartSeq(GFL_SndBGMStack_GetHandle(), seq);
}

void GFL_SndBGMStopSeq(void) {
    NNS_SndPlayerStopSeq(GFL_SndBGMStack_GetHandle(), 0);
    GFL_SndBGMStack_FreeSeq();
}

// Clears the addresses of the files of bank 0x3f2 and sequence SEQ_BGM_SHINKA, as GFL_SndBGMSetLoadedData sets them
// to data in memory
void GFL_SndBGMClearLoadedData(void) {
    u32 bankFileId = NNS_SndArcGetBankInfo(0x3f2)->fileId;
    u32 seqFileId = NNS_SndArcGetSeqInfo(SEQ_BGM_SHINKA)->fileId;

    NNS_SndArcSetFileAddress(bankFileId, NULL);
    NNS_SndArcSetFileAddress(seqFileId, NULL);
}
