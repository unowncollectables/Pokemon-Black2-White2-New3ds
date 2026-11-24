/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "types.h"
#include "nnsys/snd.h"
#include "stub_log.h"

/* Silent sound backend: the port has no ARM7/NDSP player yet (audio is
 * silent by design this milestone), so the NNS_Snd* queries must answer
 * honestly instead of returning garbage.
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
 * The contract here is "accepted, finishes instantly, nothing sounds":
 * loads and starts succeed (the game proceeds exactly as it would with
 * sound), counts/ticks/positions are 0, no wave channel exists, and the
 * archive-info getters return static dummies whose callers dereference
 * them (playerNo 1, so the player index is 0 and in range; fileId 0,
 * so GetFileAddress(NULL) still reports "not loaded").
 *
 * Known limit: anything that waits for audio to be PLAYING (e.g. the
 * intro's WAIT_BGM_PLAYING command, unused by every script so far)
 * will wait forever against a silent backend. That needs the real
 * player, not a different stub value.
 */

static const NNSSndSeqParam sDummySeqParam = { .playerNo = 1 };
static const NNSSndArcSeqInfo sDummySeqInfo = { .param = { .playerNo = 1 } };
static const NNSSndArcBankInfo sDummyBankInfo = { 0 };
static const NNSSndArcWaveArcInfo sDummyWaveArcInfo = { 0 };

const NNSSndArcBankInfo *NNS_SndArcGetBankInfo(int bankNo) {
    (void)bankNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetBankInfo (silent: dummy)");
    return &sDummyBankInfo;
}

void *NNS_SndArcGetFileAddress(u32 fileId) {
    (void)fileId;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetFileAddress (silent: NULL)");
    return NULL;
}

u32 NNS_SndArcGetFileSize(u32 fileId) {
    (void)fileId;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetFileSize (silent: 0)");
    return 0;
}

const NNSSndArcGroupInfo *NNS_SndArcGetGroupInfo(int groupNo) {
    (void)groupNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetGroupInfo (silent: NULL)");
    return NULL;
}

const NNSSndArcSeqInfo *NNS_SndArcGetSeqInfo(int seqNo) {
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetSeqInfo (silent: dummy)");
    return &sDummySeqInfo;
}

const NNSSndSeqParam *NNS_SndArcGetSeqParam(int seqNo) {
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetSeqParam (silent: dummy playerNo 1)");
    return &sDummySeqParam;
}

const NNSSndArcWaveArcInfo *NNS_SndArcGetWaveArcInfo(int waveArcNo) {
    (void)waveArcNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcGetWaveArcInfo (silent: dummy)");
    return &sDummyWaveArcInfo;
}

BOOL NNS_SndArcLoadGroup(int groupNo, NNSSndHeapHandle heap) {
    (void)groupNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadGroup (silent: TRUE)");
    return TRUE;
}

BOOL NNS_SndArcLoadSeq(int seqNo, NNSSndHeapHandle heap) {
    (void)seqNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadSeq (silent: TRUE)");
    return TRUE;
}

BOOL NNS_SndArcLoadSeqEx(int seqNo, u32 loadFlag, NNSSndHeapHandle heap) {
    (void)seqNo;
    (void)loadFlag;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadSeqEx (silent: TRUE)");
    return TRUE;
}

BOOL NNS_SndArcLoadWaveArc(int waveArcNo, NNSSndHeapHandle heap) {
    (void)waveArcNo;
    (void)heap;
    PBW2_STUB_LOG("snd", "NNS_SndArcLoadWaveArc (silent: TRUE)");
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
    return TRUE;
}

BOOL NNS_SndArcPlayerStartSeqEx(NNSSndHandle *handle, int playerNo, int bankNo, int playerPrio, int seqNo) {
    (void)handle;
    (void)playerNo;
    (void)bankNo;
    (void)playerPrio;
    (void)seqNo;
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerStartSeqEx (silent: TRUE, finishes instantly)");
    return TRUE;
}

u32 NNS_SndArcStrmGetCurrentPlayingPos(NNSSndStrmHandle *handle) {
    (void)handle;
    PBW2_STUB_LOG("snd", "NNS_SndArcStrmGetCurrentPlayingPos (silent: 0)");
    return 0;
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
    (void)fileId;
    (void)buffer;
    (void)size;
    (void)offset;
    PBW2_STUB_LOG("snd", "NNS_SndArcReadFile (silent: 0 bytes read)");
    return 0;
}

BOOL NNS_SndPlayerReadDriverTrackInfo(NNSSndHandle *handle, int trackNo, SNDTrackInfo *trackInfo) {
    (void)handle;
    (void)trackNo;
    (void)trackInfo;
    PBW2_STUB_LOG("snd", "NNS_SndPlayerReadDriverTrackInfo (silent: FALSE)");
    return FALSE;
}

NNSSndWaveOutHandle NNS_SndWaveOutAllocChannel(int channel) {
    (void)channel;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutAllocChannel (silent: NULL)");
    return NULL;
}

BOOL NNS_SndWaveOutIsPlaying(NNSSndWaveOutHandle handle) {
    (void)handle;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutIsPlaying (silent: FALSE)");
    return FALSE;
}

BOOL NNS_SndWaveOutStart(NNSSndWaveOutHandle handle, NNSSndWaveFormat format, const void *data, BOOL loop,
                         int loopStart, int samples, int rate, int volume, int speed, int pan) {
    (void)handle;
    (void)format;
    (void)data;
    (void)loop;
    (void)loopStart;
    (void)samples;
    (void)rate;
    (void)volume;
    (void)speed;
    (void)pan;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutStart (silent: FALSE)");
    return FALSE;
}
