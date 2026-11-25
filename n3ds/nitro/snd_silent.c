/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "types.h"
#include "nnsys/snd.h"
#include "snd_arc.h"
#include "stub_log.h"

/* Silent sound backend for the parts of NNS_Snd that have no host
 * player yet (sequences: BGM and all SEs). Loads and starts succeed,
 * finish immediately and produce silence. Counts, ticks and positions
 * are zero. Archive metadata comes from SDAT when available.
 *
 * Waves are NOT silent any more: GetWaveArcInfo / GetFileSize /
 * ReadFile read the staged SDAT for real (snd_arc.c), and the
 * NNS_SndWaveOut* entry points live in wave_out.c on the NDSP path —
 * cries and the mic play. See the original note below for why this
 * file exists at all.
 *
 * Why this file exists: link_stubs.c defines every stub as `void`, but
 * several of these functions return a value in the real API. A void
 * stub leaves whatever was in r0 on return, and the game reads it as
 * the answer: NNS_SndPlayerCountPlayingSeqByPlayerNo returned its
 * argument (the player number, non-zero), so GFL_SndPlayerIsActiveAny
 * reported a player active forever and the intro stalled in
 * POKEMON_APPEAR waiting for the SE to finish; NNS_SndArcGetSeqParam
 * returned the sequence id as a pointer, and GFL_SndSeqGetPlayerIndex
 * dereferenced it (the unmapped reads at PC 0x001CEE18 in the Azahar
 * log), indexing g_SndPlayerStates out of bounds.
 *
 * Known limit: anything that waits for audio to be PLAYING (e.g. the
 * intro's WAIT_BGM_PLAYING command, unused by every script so far)
 * will wait forever against a silent backend. That needs the real
 * player, not a different stub value.
 *
 * The event entry points also Log_Line("AUDIO", ...) with their
 * arguments on EVERY call (seq/waveArc/file ids, wave params), so a
 * playthrough log inventories exactly which sounds the game requests.
 * The polled queries (counts, ticks, IsPlaying) stay first-call-only
 * to avoid flooding the log.
 */

static const NNSSndArcSeqInfo sDummySeqInfo = { .fileId = 0xffffffffu, .param = { .playerNo = 1 } };
static const NNSSndArcBankInfo sDummyBankInfo = {
    .fileId = 0xffffffffu,
    .waveArcNo = { NNS_SND_ARC_INVALID_WAVEARC_NO, NNS_SND_ARC_INVALID_WAVEARC_NO,
                   NNS_SND_ARC_INVALID_WAVEARC_NO, NNS_SND_ARC_INVALID_WAVEARC_NO },
};

const NNSSndArcBankInfo *NNS_SndArcGetBankInfo(int bankNo) {
    const NNSSndArcBankInfo *bank = SndArc_GetBankInfo(bankNo);
    return bank ? bank : &sDummyBankInfo;
}

u32 NNS_SndArcGetFileSize(u32 fileId) {
    u32 size = SndArc_GetFileSize(fileId);
    PBW2_STUB_LOG("snd", "NNS_SndArcGetFileSize (real)");
    Log_Line("AUDIO", "GetFileSize fileId=%lu size=%lu", (unsigned long)fileId, (unsigned long)size);
    return size;
}

const NNSSndArcSeqInfo *NNS_SndArcGetSeqInfo(int seqNo) {
    const NNSSndArcSeqInfo *seq = SndArc_GetSeqInfo(seqNo);
    return seq ? seq : &sDummySeqInfo;
}

const NNSSndSeqParam *NNS_SndArcGetSeqParam(int seqNo) {
    const NNSSndSeqParam *param = &NNS_SndArcGetSeqInfo(seqNo)->param;
    /* GFL_SndSeqGetPlayerIndex subtracts one to index its five effect slots. */
    return param->playerNo >= 1 && param->playerNo <= 5 ? param : &sDummySeqInfo.param;
}

/* The wave-arc info is real now (snd_arc.c reads the SDAT INFO
 * table). A wave arc with no file returns NULL, and the caller
 * (PokeVoice_LoadWaveArc) treats that as a failed load. */
static NNSSndArcWaveArcInfo sWaveArcInfo;

const NNSSndArcWaveArcInfo *NNS_SndArcGetWaveArcInfo(int waveArcNo) {
    u32 fileId = SndArc_GetWaveArcFileId(waveArcNo);
    PBW2_STUB_LOG("snd", "NNS_SndArcGetWaveArcInfo (real)");
    Log_Line("AUDIO", "GetWaveArcInfo waveArc=%d fileId=%lu", waveArcNo, (unsigned long)fileId);
    if (fileId == 0xFFFFFFFFu) return NULL;
    sWaveArcInfo.fileId = fileId;
    sWaveArcInfo.flags = 0;
    return &sWaveArcInfo;
}

BOOL NNS_SndArcPlayerSetup(NNSSndHeapHandle heap) {
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerSetup (silent: TRUE)");
    return TRUE;
}

BOOL NNS_SndArcPlayerStartSeq(NNSSndHandle *handle, int seqNo) {
    (void)handle;
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerStartSeq (silent: TRUE, finishes instantly)");
    Log_Line("AUDIO", "StartSeq seq=%d", seqNo);
    return TRUE;
}

BOOL NNS_SndArcPlayerStartSeqEx(NNSSndHandle *handle, int playerNo, int bankNo, int playerPrio, int seqNo) {
    (void)handle;
    (void)playerNo;
    (void)bankNo;
    (void)playerPrio;
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerStartSeqEx (silent: TRUE, finishes instantly)");
    Log_Line("AUDIO", "StartSeqEx player=%d bank=%d prio=%d seq=%d", playerNo, bankNo, playerPrio, seqNo);
    return TRUE;
}

/* Streamed audio (NNS_SndStrm*): same silent contract. Setup/AllocChannel
 * report FALSE (no channel/alarm free), so callers that check the result
 * skip streaming instead of waiting on a stream that will never play. */
void NNS_SndStrmInit(NNSSndStrm *stream) {
    (void)stream;
    PBW2_STUB_LOG("snd", "NNS_SndStrmInit (silent)");
}

BOOL NNS_SndStrmAllocChannel(NNSSndStrm *stream, int numChannels, const u8 chNoList[]) {
    (void)stream;
    (void)numChannels;
    (void)chNoList;
    PBW2_STUB_LOG("snd", "NNS_SndStrmAllocChannel (silent: FALSE)");
    return FALSE;
}

void NNS_SndStrmFreeChannel(NNSSndStrm *stream) {
    (void)stream;
    PBW2_STUB_LOG("snd", "NNS_SndStrmFreeChannel (silent)");
}

BOOL NNS_SndStrmSetup(NNSSndStrm *stream, NNSSndStrmFormat format, void *buffer, u32 len, int timer, int interval,
                      NNSSndStrmCallback callback, void *arg) {
    (void)stream;
    (void)format;
    (void)buffer;
    (void)len;
    (void)timer;
    (void)interval;
    (void)callback;
    (void)arg;
    PBW2_STUB_LOG("snd", "NNS_SndStrmSetup (silent: FALSE)");
    return FALSE;
}

void NNS_SndStrmStart(NNSSndStrm *stream) {
    (void)stream;
    PBW2_STUB_LOG("snd", "NNS_SndStrmStart (silent)");
}

void NNS_SndStrmStop(NNSSndStrm *stream) {
    (void)stream;
    PBW2_STUB_LOG("snd", "NNS_SndStrmStop (silent)");
}

void NNS_SndStrmSetVolume(NNSSndStrm *stream, int volume) {
    (void)stream;
    (void)volume;
    PBW2_STUB_LOG("snd", "NNS_SndStrmSetVolume (silent)");
}

int NNS_SndPlayerCountPlayingSeqByPlayerNo(int playerNo) {
    (void)playerNo;
    PBW2_STUB_LOG("snd", "NNS_SndPlayerCountPlayingSeqByPlayerNo (silent: 0)");
    return 0;
}

int NNS_SndPlayerCountPlayingSeqBySeqNo(int seqNo) {
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndPlayerCountPlayingSeqBySeqNo (silent: 0)");
    return 0;
}

u32 NNS_SndPlayerGetTick(NNSSndHandle *handle) {
    (void)handle;
    PBW2_STUB_LOG("snd", "NNS_SndPlayerGetTick (silent: 0)");
    return 0;
}

s32 NNS_SndArcReadFile(u32 fileId, void *buffer, s32 size, s32 offset) {
    s32 read = SndArc_ReadFile(fileId, buffer, size, offset);
    PBW2_STUB_LOG("snd", "NNS_SndArcReadFile (real)");
    Log_Line("AUDIO", "ReadFile fileId=%lu size=%ld offset=%ld read=%ld", (unsigned long)fileId, (long)size,
             (long)offset, (long)read);
    return read;
}

BOOL NNS_SndPlayerReadDriverTrackInfo(NNSSndHandle *handle, int trackNo, SNDTrackInfo *trackInfo) {
    (void)handle;
    (void)trackNo;
    (void)trackInfo;
    PBW2_STUB_LOG("snd", "NNS_SndPlayerReadDriverTrackInfo (silent: FALSE)");
    return FALSE;
}

/* NNS_SndWaveOut* lives in wave_out.c now (real NDSP player). */
