/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "types.h"
#include "nnsys/snd.h"
#include "snd_arc.h"
#include "stub_log.h"

/* Silent sound backend for the parts of NNS_Snd that have no host
 * player yet (sequences: BGM and all SEs), plus the archive-info
 * getters the game dereferences. The contract for those is "accepted,
 * finishes instantly, nothing sounds": loads and starts succeed (the
 * game proceeds exactly as it would with sound), counts/ticks/
 * positions are 0, and the archive-info getters return static dummies
 * whose callers dereference them (playerNo 1, so the player index is
 * 0 and in range).
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

static const NNSSndSeqParam sDummySeqParam = { .playerNo = 1 };
static const NNSSndArcSeqInfo sDummySeqInfo = { .param = { .playerNo = 1 } };
static const NNSSndArcBankInfo sDummyBankInfo = { 0 };

const NNSSndArcBankInfo *NNS_SndArcGetBankInfo(int bankNo) {
    (void)bankNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetBankInfo (silent: dummy)");
    Log_Line("AUDIO", "GetBankInfo bank=%d", bankNo);
    return &sDummyBankInfo;
}

void *NNS_SndArcGetFileAddress(u32 fileId) {
    (void)fileId;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetFileAddress (silent: NULL)");
    Log_Line("AUDIO", "GetFileAddress fileId=%lu", (unsigned long)fileId);
    return NULL;
}

u32 NNS_SndArcGetFileSize(u32 fileId) {
    u32 size = SndArc_GetFileSize(fileId);
    PBW2_STUB_LOG("snd", "NNS_SndArcGetFileSize (real)");
    Log_Line("AUDIO", "GetFileSize fileId=%lu size=%lu", (unsigned long)fileId, (unsigned long)size);
    return size;
}

const NNSSndArcGroupInfo *NNS_SndArcGetGroupInfo(int groupNo) {
    (void)groupNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetGroupInfo (silent: NULL)");
    return NULL;
}

const NNSSndArcSeqInfo *NNS_SndArcGetSeqInfo(int seqNo) {
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetSeqInfo (silent: dummy)");
    Log_Line("AUDIO", "GetSeqInfo seq=%d", seqNo);
    return &sDummySeqInfo;
}

const NNSSndSeqParam *NNS_SndArcGetSeqParam(int seqNo) {
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetSeqParam (silent: dummy playerNo 1)");
    Log_Line("AUDIO", "GetSeqParam seq=%d", seqNo);
    return &sDummySeqParam;
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

BOOL NNS_SndArcLoadGroup(int groupNo, NNSSndHeapHandle heap) {
    (void)groupNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadGroup (silent: TRUE)");
    Log_Line("AUDIO", "LoadGroup group=%d", groupNo);
    return TRUE;
}

BOOL NNS_SndArcLoadSeq(int seqNo, NNSSndHeapHandle heap) {
    (void)seqNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadSeq (silent: TRUE)");
    Log_Line("AUDIO", "LoadSeq seq=%d", seqNo);
    return TRUE;
}

BOOL NNS_SndArcLoadSeqEx(int seqNo, u32 loadFlag, NNSSndHeapHandle heap) {
    (void)seqNo;
    (void)loadFlag;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadSeqEx (silent: TRUE)");
    Log_Line("AUDIO", "LoadSeqEx seq=%d flag=%lu", seqNo, (unsigned long)loadFlag);
    return TRUE;
}

BOOL NNS_SndArcLoadWaveArc(int waveArcNo, NNSSndHeapHandle heap) {
    (void)waveArcNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadWaveArc (silent: TRUE)");
    Log_Line("AUDIO", "LoadWaveArc waveArc=%d", waveArcNo);
    return TRUE;
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
