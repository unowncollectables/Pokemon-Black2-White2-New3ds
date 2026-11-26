/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "types.h"
#include "nnsys/snd.h"
#include "snd_arc.h"
#include "seq_player.h"
#include "wave_out.h"
#include "audio.h"
#include "log.h"
#include "stub_log.h"

#include <3ds.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef PBW2_LINK_GAME

/* Real NNS sequence playback on the NDSP path: BGM and every SE.
 *
 * The ARM7 sequence interpreter does not exist in this tree, so the
 * host runs it: the audio worker advances each playing sequence's tick clock from
 * wall time, the event parser walks the SSEQ tracks, and NOTE-ONs
 * resolve program -> instrument (SBNK) -> wave (SWAR) -> a DS channel
 * through the wave player (wave_out.c). The sequence data is copied
 * at StartSeq time, so the sound heap may unload the bank underneath
 * a playing sequence without the parser reading freed memory; a wave
 * is resolved at note-on time and skipped (not hung on) when its bank
 * is gone.
 *
 * Format facts, verified against the staged SDAT (see work-log.md,
 * "Sequence player job"): SSEQ data starts at file offset nDataOffset
 * (0x1C); track 0 follows the 0xFE mask + 0x93 entries; 0x93/CALL/JUMP
 * addresses are indices into the data section. Quarter = 48 ticks;
 * ticks advance at tempo * 0.8 per second. SBNK instruments are
 * SNDInstData/SNDInstParam: type 1 PCM, 2 PSG, 3 NOISE, 16 drum set,
 * 17 key split. A single program's def is the 10-byte param; a
 * drum-set / key-split entry is the 12-byte SNDInstData (type, pad,
 * then the param). SWAVINFO is 12 bytes: type/loop/rate/time/
 * loopStart u16 (words)/loopLen u32 (words); the wave's total
 * length is loopStart + loopLen words. PSG (square, duty from
 * wave[0]) and noise are synthesized â€” the 3DS has no DS PSG
 * hardware. The 0x81 BANKPROG event carries only a program in
 * every real sequence (bank field always 0) â€” the track's bank is
 * the sequence's bank from its SDAT param.
 *
 * Honest degradation: with NDSP down, sequences still advance their
 * tick clock and report playing/not-playing on schedule (notes are
 * skipped, never waited on), so the game never hangs on a sound.
 * A sequence the game waits on reports its real tick and count, and
 * reaches EOT on time. Channel 15 is reserved for the PokeVoice cry
 * path (wave_out.c allocs it directly) â€” sequences never take it.
 * The wave player's linear PCM buffer is 512 KiB per channel: the
 * largest SDAT wave decodes to 451 KiB.
 *
 * Slice A limits (see work-log.md for the sized remainder): no
 * arithmetic opcodes (0xA0-0xBF â€” none appear in any inventoried
 * sequence; a track that hits one stops instead of desyncing), no
 * ADSR envelope on sampled notes (notes play at constant volume
 * for their duration; the PSG/noise synth does apply an ADSR),
 * no modulation or portamento. Sampled notes follow track pitch and
 * pitch-bend changes. PSG/noise uses the pitch at note-on. */

#define SEQ_SLOTS 32
#define SEQ_TRACKS 16
#define SEQ_TRACK_NOTES 4
#define SEQ_DS_CHANNELS 16
#define SEQ_CRY_CHANNEL 15
#define SEQ_CALL_DEPTH 4
#define SEQ_LOOP_DEPTH 4
#define SEQ_TICKS_PER_QUARTER 48

typedef struct {
    NNSSndWaveOutHandle wave;
    int dsCh; /* -1 when the note is timed but not sounding (NDSP down) */
    u8 note;
    u8 velocity;
    u32 endTick;
    int samplePitch;
    bool sampled;
    BOOL active;
} SeqNote;

typedef struct {
    BOOL active;
    BOOL muted;
    const u8 *pos;
    u32 wait;
    u8 volume;     /* 0xC1, default 127 */
    u8 expression; /* 0xD5, default 127 */
    s8 pan;        /* 0xC0, default 64 */
    s8 transpose;  /* 0xC3, default 0 */
    s8 bend;       /* 0xC4, default 0 */
    u8 bendRange;  /* 0xC5, default 2 */
    u8 program;    /* 0x81 */
    u8 priority;   /* 0xC6, default 64 */
    BOOL noteWait;   /* 0xC7, advance by note length when enabled */
    BOOL tie;      /* 0xC8 */
    u8 attack, decay, sustain, release; /* 0xD0-0xD3, stored */
    u8 modDepth, modSpeed, modType, modRange; /* 0xCA-0xCD, stored */
    s16 pitch;     /* SetTrackPitch, 64 units per semitone */
    u32 callStack[SEQ_CALL_DEPTH];
    u8 callDepth;
    struct {
        u32 startPos;
        u32 remaining;
    } loopStack[SEQ_LOOP_DEPTH];
    u8 loopDepth;
    SeqNote notes[SEQ_TRACK_NOTES];
} SeqTrack;

typedef struct {
    BOOL inUse;
    int playerNo, seqNo, bankNo, playerPrio;
    NNSSndHandle *handle;
    u8 *seqData;
    u32 seqLen;
    u16 trackMask;
    u32 tempo; /* BPM, default 120 */
    float tickAcc;
    u32 tick;
    int volume; /* SetVolume / 0xC2 master volume, default from seq param */
    int fadeVol; /* fade level 0..127, default 127 */
    int fadeTarget; /* MoveVolume target, -1 = no fade running */
    int fadeFramesLeft, fadeFramesTotal, fadeStartVol;
    u64 fadeStartTick;
    BOOL stopAtFadeEnd;
    BOOL paused;
    int tempoRatio; /* 256 = 1.0 */
    u32 noteCount, droppedNotes;
    unsigned missingWave, synthesisFailed, channelUnavailable, queueFailed;
    SeqTrack tracks[SEQ_TRACKS];
} SeqSlot;

typedef struct {
    BOOL inUse;
    int slot, track, noteIdx;
    int priority;
    u64 order;
} SeqChannel;

static SeqSlot s_slots[SEQ_SLOTS];
static SeqChannel s_channels[SEQ_DS_CHANNELS];
static u64 s_channelOrder;
static NNSSndHeapHandle s_heap; /* stashed by NNS_SndArcPlayerSetup */
static int s_playerVolume[32];
static int s_playableSeqCount[32];
static u32 s_allocChBitFlag[32];
static u64 s_lastTick;
static BOOL s_setupDone;
static BOOL s_ndspDownLogged;
/* Serializes the slot state (s_slots, s_playerVolume,
 * s_playableSeqCount, s_allocChBitFlag, s_lastTick) between the game
 * thread (public entry points) and the audio thread (SeqPlayer_Tick).
 * The tick calls into wave_out while holding this lock, so the order
 * is seq -> wave; the cry/mic paths take only the wave lock, so there
 * is no reverse nesting. LightLock is non-recursive: the three
 * delegating entry points (StartSeq, SetInitialVolume, SetTrackMuteEx)
 * do NOT take it — their targets do. */
static LightLock s_seqLock;
static bool s_audioThreadTick;

void SeqPlayer_Init(void) {
    LightLock_Init(&s_seqLock);
}

void SeqPlayer_SetAudioThreadTick(bool on) {
    __atomic_store_n(&s_audioThreadTick, on, __ATOMIC_RELEASE);
}

static u16 seq_rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }

static u32 seq_rd32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u32 seq_rd_varlen(const u8 *p, u32 *adv) {
    u32 v = 0, i = 0;
    for (;;) {
        u8 x = p[i++];
        v = (v << 7) | (x & 0x7f);
        if (!(x & 0x80)) break;
        if (i >= 4) break;
    }
    *adv = i;
    return v;
}

static SeqSlot *seq_slot_from_handle(NNSSndHandle *handle) {
    int i;
    if (!handle || !handle->player) return NULL;
    for (i = 0; i < SEQ_SLOTS; ++i) {
        if (s_slots[i].inUse && (struct NNSSndSeqPlayer *)&s_slots[i] == handle->player) return &s_slots[i];
    }
    return NULL;
}

static int seq_count_on_player(int playerNo) {
    int i, n = 0;
    for (i = 0; i < SEQ_SLOTS; ++i) {
        if (s_slots[i].inUse && s_slots[i].playerNo == playerNo) ++n;
    }
    return n;
}

static int seq_count_seq(int seqNo) {
    int i, n = 0;
    for (i = 0; i < SEQ_SLOTS; ++i) {
        if (s_slots[i].inUse && s_slots[i].seqNo == seqNo) ++n;
    }
    return n;
}

/* Resolve one note to a playable wave. Returns FALSE when the note has
 * no instrument (empty program, unloaded bank, oversize sample) â€” the
 * caller skips it. The result is either a sampled wave (PCM/ADPCM
 * from a SWAR) or a synthesized one (PSG square / noise, generated
 * at note-on time â€” the 3DS has no DS PSG hardware).
 *
 * SBNK instrument types (SNDInstData / SNDInstParam, lib/nitro):
 *   1 PCM, 2 PSG, 3 NOISE, 4 DIRECTPCM, 5 NULL, 16 drum set,
 *   17 key split. A single program's def is the 10-byte param
 *   directly; a drum-set / key-split entry is the 12-byte
 *   SNDInstData (type u8, pad u8, then the param), so the wave
 *   fields sit 2 bytes further in.
 * SWAVINFO is 12 bytes: type/loop/rate/time/loopStart u16 (words)/
 * loopLen u32 (words). The wave's total length is
 * loopStart + loopLen words â€” loopLen alone is only the loop
 * section, and using it as the total breaks every looping wave
 * (loopStart lands past the end of the buffer). */
typedef struct {
    int kind; /* 0 sampled, 1 PSG square, 2 noise */
    int format; /* sampled only: 0 PCM8, 1 PCM16, 2 IMA-ADPCM */
    int rate;
    BOOL loop;
    const u8 *data; /* sampled only; ADPCM: points at the 4-byte header */
    u32 samples;    /* sampled only: total samples incl. ADPCM header samples */
    u32 loopStart;  /* sampled only: loop start in samples */
    u8 defNote, pan;
    u8 attack, decay, sustain, release;
    int duty; /* PSG only: wave[0], 0-7 -> (duty+1)/8 pulse width */
} SeqWave;

static BOOL seq_resolve_wave(int bankNo, int program, int note, SeqWave *out) {
    const NNSSndArcBankInfo *bank = SndArc_GetBankInfo(bankNo);
    const u8 *sbnk;
    u32 nCount, rec, off;
    int frec;
    /* The resolved instrument: type + SNDInstParam fields. */
    int instType;
    u16 wave0, wave1;
    u8 defNote, atk, dec, sus, rel, pan;

    if (!bank) return FALSE;
    sbnk = NNS_SndArcGetFileAddress(bank->fileId);
    if (!sbnk) return FALSE;
    nCount = seq_rd32(sbnk + 0x38);
    if (program < 0 || (u32)program >= nCount) return FALSE;
    rec = seq_rd32(sbnk + 0x3c + 4 * (u32)program);
    frec = (int)(rec & 0xff);
    off = (rec >> 8) & 0xffff;
    if (frec == 0 || frec == 5) return FALSE;
    if (frec == 1 || frec == 2 || frec == 3 || frec == 4) {
        /* 10-byte SNDInstParam at the record offset. */
        instType = frec;
        wave0 = seq_rd16(sbnk + off);
        wave1 = seq_rd16(sbnk + off + 2);
        defNote = sbnk[off + 4];
        atk = sbnk[off + 5];
        dec = sbnk[off + 6];
        sus = sbnk[off + 7];
        rel = sbnk[off + 8];
        pan = sbnk[off + 9];
    } else if (frec == 16 || frec == 17) {
        /* Drum set / key split: entries are 12-byte SNDInstData. */
        const u8 *def;
        if (frec == 16) {
            int lower = sbnk[off], upper = sbnk[off + 1];
            if (note < lower || note > upper) return FALSE;
            def = sbnk + off + 2 + 12 * (u32)(note - lower);
        } else {
            int i, region = -1;
            for (i = 0; i < 8; ++i) {
                u8 b = sbnk[off + (u32)i];
                if (b == 0) break;
                region = i;
                if (note <= b) break;
            }
            if (region < 0) return FALSE;
            def = sbnk + off + 8 + 12 * (u32)region;
        }
        instType = def[0];
        wave0 = seq_rd16(def + 2);
        wave1 = seq_rd16(def + 4);
        defNote = def[6];
        atk = def[7];
        dec = def[8];
        sus = def[9];
        rel = def[10];
        pan = def[11];
    } else {
        return FALSE;
    }
    out->defNote = defNote;
    out->pan = pan;
    out->attack = atk;
    out->decay = dec;
    out->sustain = sus;
    out->release = rel;
    if (instType == 2) {
        out->kind = 1; /* PSG square */
        out->duty = (int)wave0;
        return TRUE;
    }
    if (instType == 3) {
        out->kind = 2; /* noise */
        out->duty = 0;
        return TRUE;
    }
    if (instType != 1) return FALSE; /* DIRECTPCM and anything else: not yet */
    /* PCM: wave0 = swav, wave1 = swar. */
    {
        u16 swav = wave0, swarNo = wave1;
        u16 waveArcNo;
        u32 fileId, nSamp, infoOff;
        const u8 *swar, *info;
        int format, rate, loopStartWords;
        u32 loopLenWords, totalWords;
        if (swarNo >= NNS_SND_ARC_BANK_TO_WAVEARC_NUM) return FALSE;
        waveArcNo = bank->waveArcNo[swarNo];
        if (waveArcNo == NNS_SND_ARC_INVALID_WAVEARC_NO) return FALSE;
        fileId = SndArc_GetWaveArcFileId((int)waveArcNo);
        if (fileId == 0xFFFFFFFFu) return FALSE;
        swar = NNS_SndArcGetFileAddress(fileId);
        if (!swar) return FALSE;
        nSamp = seq_rd32(swar + 0x38);
        if ((u32)swav >= nSamp) return FALSE;
        infoOff = seq_rd32(swar + 0x3c + 4 * (u32)swav);
        info = swar + infoOff;
        format = info[0];
        if (format < 0 || format > 2) return FALSE;
        out->kind = 0;
        out->format = format;
        out->loop = info[1] ? TRUE : FALSE;
        rate = seq_rd16(info + 2);
        if (rate <= 0) return FALSE;
        out->rate = rate;
        loopStartWords = seq_rd16(info + 6);
        loopLenWords = seq_rd32(info + 8);
        totalWords = (u32)loopStartWords + loopLenWords;
        out->data = info + 12;
        if (format == 2) {
            out->samples = totalWords * 8; /* 4 bytes/word, 2 samples/byte */
            out->loopStart = (u32)loopStartWords * 8;
            if (out->samples <= 8 || (out->samples & 7) || (out->loopStart & 7)) return FALSE;
            if (out->samples > 262144) return FALSE; /* wave_out 512 KiB cap */
        } else if (format == 1) {
            out->samples = totalWords * 2;
            out->loopStart = (u32)loopStartWords * 2;
            if (out->samples > 262144) return FALSE;
        } else {
            out->samples = totalWords * 4;
            out->loopStart = (u32)loopStartWords * 4;
            if (out->samples > 524288) return FALSE;
        }
        if (out->samples == 0) return FALSE;
        return TRUE;
    }
}

/* DS channel 15 is reserved for the cry path. */
static SeqNote *seq_channel_note(const SeqChannel *channel) {
    return &s_slots[channel->slot].tracks[channel->track].notes[channel->noteIdx];
}

static void seq_free_channel(int ch) {
    SeqChannel *channel = &s_channels[ch];
    SeqNote *note = seq_channel_note(channel);
    NNS_SndWaveOutFreeChannel(note->wave);
    note->active = FALSE;
    note->wave = NULL;
    note->dsCh = -1;
    channel->inUse = FALSE;
}

static NNSSndWaveOutHandle seq_claim_channel(SeqSlot *slot, int trackIdx, int noteIdx, int priority, int ch) {
    NNSSndWaveOutHandle wave = WaveOut_AllocSequenceChannel(ch);
    if (wave) {
        s_channels[ch] = (SeqChannel){TRUE, (int)(slot - s_slots), trackIdx, noteIdx, priority, s_channelOrder++};
        slot->tracks[trackIdx].notes[noteIdx].dsCh = ch;
    }
    return wave;
}

static NNSSndWaveOutHandle seq_alloc_channel(SeqSlot *slot, int trackIdx, int noteIdx, int priority) {
    u32 allowed = s_allocChBitFlag[slot->playerNo] & ~(1u << SEQ_CRY_CHANNEL);
    NNSSndWaveOutHandle wave;
    int victim = -1;

    for (int ch = 0; ch < SEQ_DS_CHANNELS; ++ch) {
        if (!(allowed & (1u << ch)) || s_channels[ch].inUse) continue;
        wave = seq_claim_channel(slot, trackIdx, noteIdx, priority, ch);
        if (wave) return wave;
    }

    for (int ch = 0; ch < SEQ_DS_CHANNELS; ++ch) {
        SeqChannel *channel = &s_channels[ch];
        if (!(allowed & (1u << ch)) || !channel->inUse) continue;
        if (!NNS_SndWaveOutIsPlaying(seq_channel_note(channel)->wave)) {
            seq_free_channel(ch);
            wave = seq_claim_channel(slot, trackIdx, noteIdx, priority, ch);
            if (wave) return wave;
            continue;
        }
        if (channel->priority > priority) continue;
        if (victim < 0 || channel->priority < s_channels[victim].priority ||
            (channel->priority == s_channels[victim].priority && channel->order < s_channels[victim].order))
            victim = ch;
    }

    if (victim < 0) return NULL;
    /* Sequence ticks are local to each song. Compare global note onset order. */
    seq_free_channel(victim);
    return seq_claim_channel(slot, trackIdx, noteIdx, priority, victim);
}

static int seq_final_volume(SeqSlot *slot, SeqTrack *track, int velocity) {
    int v = velocity * track->volume / 127;
    v = v * track->expression / 127;
    v = v * slot->volume / 127;
    v = v * s_playerVolume[slot->playerNo] / 127;
    v = v * slot->fadeVol / 127;
    return v < 0 ? 0 : v > 127 ? 127 : v;
}

static void seq_update_volume(SeqSlot *slot, unsigned trackMask) {
    for (unsigned t = 0; t < SEQ_TRACKS; ++t) {
        if (!(trackMask & (1u << t))) continue;
        SeqTrack *track = &slot->tracks[t];
        for (unsigned n = 0; n < SEQ_TRACK_NOTES; ++n) {
            SeqNote *note = &track->notes[n];
            if (note->active && note->wave)
                NNS_SndWaveOutSetVolume(note->wave, seq_final_volume(slot, track, note->velocity));
        }
    }
}

static void seq_note_off(SeqSlot *slot, int trackIdx, int noteIdx) {
    SeqTrack *track = &slot->tracks[trackIdx];
    SeqNote *note = &track->notes[noteIdx];
    if (!note->active) return;
    if (note->dsCh >= 0 && note->dsCh < SEQ_DS_CHANNELS && s_channels[note->dsCh].inUse &&
        s_channels[note->dsCh].slot == (int)(slot - s_slots) &&
        s_channels[note->dsCh].noteIdx == noteIdx && s_channels[note->dsCh].track == trackIdx) {
        s_channels[note->dsCh].inUse = FALSE;
    }
    if (note->wave) {
        NNS_SndWaveOutFreeChannel(note->wave);
        note->wave = NULL;
    }
    note->active = FALSE;
    note->dsCh = -1;
}

/* Synthesize a PSG square wave or noise burst for one note into a
 * static PCM16 scratch buffer (the wave player copies it on Start,
 * so reuse across note-ons is safe). The DS has no PSG hardware on
 * the 3DS; the square wave's duty comes from the instrument's
 * wave[0] ((duty+1)/8, 3 -> 50%), the frequency from the MIDI note,
 * and the ADSR from the instrument param. Returns the sample count;
 * the release tail is included, so the caller stretches the note's
 * end tick by the release time. */
#define SEQ_SYNTH_RATE 32000
#define SEQ_SYNTH_MAX_SAMPLES 262144 /* 512 KiB PCM16 */
static s16 s_synthBuf[SEQ_SYNTH_MAX_SAMPLES];

static int seq_track_pitch(const SeqTrack *track) {
    return track->pitch + (int)track->bend * track->bendRange;
}

static int seq_sample_speed(int pitch) {
    if (pitch <= -36 * 64) return 4096;
    if (pitch >= 36 * 64) return 262144;
    return (int)(32768.0f * powf(2.0f, (float)pitch / (12 * 64)));
}

static void seq_update_track_pitch(SeqTrack *track) {
    int pitch = seq_track_pitch(track);
    for (unsigned i = 0; i < SEQ_TRACK_NOTES; ++i) {
        SeqNote *note = &track->notes[i];
        if (note->active && note->wave && note->sampled) {
            NNS_SndWaveOutSetSpeed(note->wave, seq_sample_speed(note->samplePitch + pitch));
        }
    }
}

static u32 seq_synth_note(const SeqWave *wave, int note, int pitch, float noteSeconds,
                          float *releaseSeconds) {
    float freq = 440.0f * powf(2.0f, (float)((note - 69) * 64 + pitch) / (12 * 64));
    float atkTime = (float)wave->attack / 127.0f * 0.02f;
    float decTime = (float)wave->decay / 127.0f * 0.30f;
    float relTime = (float)wave->release / 127.0f * 0.30f;
    float sustain = (float)wave->sustain / 127.0f;
    float duty = (float)((wave->duty < 0 ? 0 : wave->duty > 7 ? 7 : wave->duty) + 1) / 8.0f;
    float total = noteSeconds + relTime;
    u32 samples = (u32)(total * (float)SEQ_SYNTH_RATE);
    u32 i;
    float phase = 0.0f;
    u32 lfsr = 0xACE1u | ((u32)(note + 1) << 16);
    if (samples > SEQ_SYNTH_MAX_SAMPLES) samples = SEQ_SYNTH_MAX_SAMPLES;
    if (samples == 0) return 0;
    *releaseSeconds = relTime;
    for (i = 0; i < samples; ++i) {
        float t = (float)i / (float)SEQ_SYNTH_RATE;
        float env;
        float v;
        if (atkTime > 0.0f && t < atkTime) {
            env = t / atkTime;
        } else if (decTime > 0.0f && t < atkTime + decTime) {
            env = 1.0f - (1.0f - sustain) * (t - atkTime) / decTime;
        } else if (t < noteSeconds) {
            env = sustain;
        } else if (relTime > 0.0f) {
            env = sustain * (1.0f - (t - noteSeconds) / relTime);
        } else {
            env = 0.0f;
        }
        if (wave->kind == 1) {
            phase += freq / (float)SEQ_SYNTH_RATE;
            if (phase >= 1.0f) phase -= 1.0f;
            v = (phase < duty) ? 1.0f : -1.0f;
        } else {
            /* 15-bit LFSR noise, DS-style. */
            lfsr = (lfsr >> 1) ^ (-(s32)(lfsr & 1u) & 0x6000u);
            v = (lfsr & 1u) ? 1.0f : -1.0f;
        }
        s_synthBuf[i] = (s16)(v * env * 32767.0f);
    }
    return samples;
}

static void seq_note_on(SeqSlot *slot, int trackIdx, int note, int velocity, u32 duration) {
    SeqTrack *track = &slot->tracks[trackIdx];
    SeqWave wave;
    int i, freeIdx = -1, oldestIdx = -1;
    u32 oldestEnd = 0xffffffffu;
    int finalVol, pan;
    NNSSndWaveOutHandle waveOut;
    /* Resolved playback parameters (sampled or synthesized). */
    NNSSndWaveFormat format;
    const void *data;
    BOOL loop;
    int loopStart, samples, rate, speed;
    int samplePitch = 0;
    u32 endTick;

    if (track->muted) return;
    /* Retrigger the same pitch on this track. */
    for (i = 0; i < SEQ_TRACK_NOTES; ++i) {
        if (track->notes[i].active && track->notes[i].note == (u8)note) seq_note_off(slot, trackIdx, i);
    }
    for (i = 0; i < SEQ_TRACK_NOTES; ++i) {
        if (!track->notes[i].active) {
            freeIdx = i;
            break;
        }
        if (track->notes[i].endTick < oldestEnd) {
            oldestEnd = track->notes[i].endTick;
            oldestIdx = i;
        }
    }
    if (freeIdx < 0) {
        if (oldestIdx < 0) return;
        seq_note_off(slot, trackIdx, oldestIdx);
        freeIdx = oldestIdx;
    }
    if (!seq_resolve_wave(slot->bankNo, track->program, note, &wave)) {
        slot->missingWave++;
        slot->droppedNotes++;
        return;
    }
    finalVol = seq_final_volume(slot, track, velocity);
    pan = (int)track->pan + (int)wave.pan - 64;
    if (pan < 0) pan = 0;
    if (pan > 127) pan = 127;
    endTick = slot->tick + duration;
    if (wave.kind == 0) {
        samplePitch = ((int)note - (int)wave.defNote + track->transpose) * 64;
        speed = seq_sample_speed(samplePitch + seq_track_pitch(track));
        format = (NNSSndWaveFormat)wave.format;
        data = wave.data;
        loop = wave.loop;
        loopStart = (int)wave.loopStart;
        samples = (int)wave.samples;
        rate = wave.rate;
    } else {
        /* PSG / noise: synthesize at the note's pitch. */
        int pitch = track->transpose * 64 + seq_track_pitch(track);
        float noteSeconds = slot->tempo > 0 ? (float)duration / ((float)slot->tempo * 0.8f) : 0.0f;
        float releaseSeconds = 0.0f;
        u32 synthSamples = seq_synth_note(&wave, note, pitch, noteSeconds, &releaseSeconds);
        if (synthSamples == 0) {
            slot->synthesisFailed++;
            slot->droppedNotes++;
            return;
        }
        endTick = slot->tick + duration +
                  (u32)(releaseSeconds * (float)slot->tempo * 0.8f);
        format = NNS_SND_WAVE_FORMAT_PCM16;
        data = s_synthBuf;
        loop = FALSE;
        loopStart = 0;
        samples = (int)synthSamples;
        rate = SEQ_SYNTH_RATE;
        speed = 0x8000;
    }
    slot->noteCount++;
    if (!Audio_IsNdspReady()) {
        /* Timed but silent: the sequence still advances and ends on
         * schedule, so the game never waits on a sound that cannot
         * play. */
        SeqNote *n;
        if (!s_ndspDownLogged) {
            s_ndspDownLogged = TRUE;
            Log_Line("AUDIO", "SeqPlayer: NDSP down, seq %d silent (timing kept)", slot->seqNo);
        }
        n = &track->notes[freeIdx];
        n->active = TRUE;
        n->wave = NULL;
        n->dsCh = -1;
        n->note = (u8)note;
        n->velocity = (u8)velocity;
        n->endTick = endTick;
        return;
    }
    waveOut = seq_alloc_channel(slot, trackIdx, freeIdx, track->priority);
    if (!waveOut) {
        slot->channelUnavailable++;
        slot->droppedNotes++;
        return;
    }
    int ch = track->notes[freeIdx].dsCh;
    if (!NNS_SndWaveOutStart(waveOut, format, data, loop, loopStart, samples, rate, finalVol,
                             speed, pan)) {
        NNS_SndWaveOutFreeChannel(waveOut);
        s_channels[ch].inUse = FALSE;
        track->notes[freeIdx].dsCh = -1;
        slot->queueFailed++;
        slot->droppedNotes++;
        return;
    }
    {
        SeqNote *n = &track->notes[freeIdx];
        n->active = TRUE;
        n->wave = waveOut;
        n->sampled = wave.kind == 0;
        n->samplePitch = samplePitch;
        n->note = (u8)note;
        n->velocity = (u8)velocity;
        n->endTick = endTick;
    }
}

/* Walk one track's events until a NOTE/REST sets a wait, or the track
 * ends. Zero-wait events (controls, jumps, calls) are consumed inline. */
static void seq_process_events(SeqSlot *slot, int trackIdx) {
    SeqTrack *track = &slot->tracks[trackIdx];
    u32 guard = 0;
    while (track->active && guard++ < 2048) {
        const u8 *p;
        u8 op;
        if (track->pos < slot->seqData || track->pos >= slot->seqData + slot->seqLen) {
            track->active = FALSE;
            break;
        }
        p = track->pos;
        op = *p;
        if (op < 0x80) {
            /* NOTE-ON: note, velocity, duration varlen. */
            int note = op, vel = p[1];
            u32 adv = 0, dur = seq_rd_varlen(p + 2, &adv);
            track->pos += 2 + adv;
            seq_note_on(slot, trackIdx, note, vel, dur);
            if (!track->noteWait || !dur) continue;
            track->wait = dur;
            return;
        }
        if (op == 0x80) {
            u32 adv = 0, dur = seq_rd_varlen(p + 1, &adv);
            track->pos += 1 + adv;
            if (!dur) continue;
            track->wait = dur;
            return;
        }
        if (op == 0x81) {
            u32 adv = 0, v = seq_rd_varlen(p + 1, &adv);
            track->pos += 1 + adv;
            track->program = (u8)(v & 0x7f);
            /* The bank field is 0 in every real sequence; the track's
             * bank stays the sequence's bank. A nonzero field is
             * logged once per sequence start (see StartSeqEx). */
            continue;
        }
        if (op == 0x93) {
            track->pos += 5;
            continue;
        }
        if (op == 0x94) {
            u32 addr = (u32)p[1] | ((u32)p[2] << 8) | ((u32)p[3] << 16);
            if (addr >= slot->seqLen) {
                track->active = FALSE;
                break;
            }
            track->pos = slot->seqData + addr;
            continue;
        }
        if (op == 0x95) {
            u32 addr = (u32)p[1] | ((u32)p[2] << 8) | ((u32)p[3] << 16);
            if (addr >= slot->seqLen) {
                track->active = FALSE;
                break;
            }
            if (track->callDepth < SEQ_CALL_DEPTH) {
                track->callStack[track->callDepth++] =
                    (u32)(track->pos - slot->seqData) + 4;
            }
            track->pos = slot->seqData + addr;
            continue;
        }
        if (op == 0xFD) {
            if (track->callDepth > 0) {
                track->pos = slot->seqData + track->callStack[--track->callDepth];
            } else {
                track->pos += 1;
            }
            continue;
        }
        if (op == 0xFC) {
            track->pos += 1;
            if (track->loopDepth > 0) {
                if (track->loopStack[track->loopDepth - 1].remaining > 1) {
                    track->loopStack[track->loopDepth - 1].remaining--;
                    track->pos = slot->seqData + track->loopStack[track->loopDepth - 1].startPos;
                } else {
                    track->loopDepth--;
                }
            }
            continue;
        }
        if (op >= 0xA0 && op <= 0xBF) {
            /* Arithmetic/compare: not implemented in slice A and no
             * inventoried sequence uses them. Stop the track rather
             * than desync the parser. */
            Log_Line("AUDIO", "SeqPlayer seq=%d track=%d: arithmetic op 0x%02x, track stopped",
                     slot->seqNo, trackIdx, op);
            track->active = FALSE;
            break;
        }
        if (op >= 0xC0 && op <= 0xD6) {
            u8 v = p[1];
            track->pos += 2;
            switch (op) {
            case 0xC0: track->pan = (s8)v; break;
            case 0xC1:
                track->volume = v;
                seq_update_volume(slot, 1u << trackIdx);
                break;
            case 0xC2:
                slot->volume = (int)v;
                seq_update_volume(slot, 0xffff);
                break;
            case 0xC3: track->transpose = (s8)v; break;
            case 0xC4:
                track->bend = (s8)v;
                seq_update_track_pitch(track);
                break;
            case 0xC5:
                track->bendRange = v;
                seq_update_track_pitch(track);
                break;
            case 0xC6: track->priority = v; break;
            case 0xC7: track->noteWait = v != 0; break;
            case 0xC8: track->tie = v ? TRUE : FALSE; break;
            case 0xC9: break; /* portamento control */
            case 0xCA: track->modDepth = v; break;
            case 0xCB: track->modSpeed = v; break;
            case 0xCC: track->modType = v; break;
            case 0xCD: track->modRange = v; break;
            case 0xCE: break; /* portamento on/off */
            case 0xCF: break; /* portamento time */
            case 0xD0: track->attack = v; break;
            case 0xD1: track->decay = v; break;
            case 0xD2: track->sustain = v; break;
            case 0xD3: track->release = v; break;
            case 0xD4: /* LOOP START, count byte */
                if (v == 0) v = 1;
                if (track->loopDepth < SEQ_LOOP_DEPTH) {
                    track->loopStack[track->loopDepth].startPos =
                        (u32)(track->pos - slot->seqData);
                    track->loopStack[track->loopDepth].remaining = v;
                    track->loopDepth++;
                }
                break;
            case 0xD5:
                track->expression = v;
                seq_update_volume(slot, 1u << trackIdx);
                break;
            case 0xD6: break; /* print variable */
            default: break;
            }
            continue;
        }
        if (op == 0xE0 || op == 0xE1 || op == 0xE3) {
            u16 v = seq_rd16(p + 1);
            track->pos += 3;
            if (op == 0xE1) slot->tempo = v ? v : 120; /* TEMPO */
            continue;
        }
        if (op == 0xFF) {
            track->active = FALSE;
            break;
        }
        Log_Line("AUDIO", "SeqPlayer seq=%d track=%d: unknown op 0x%02x, track stopped",
                 slot->seqNo, trackIdx, op);
        track->active = FALSE;
        break;
    }
    if (guard >= 2048) {
        Log_Line("AUDIO", "SeqPlayer seq=%d track=%d: event guard hit, track stopped",
                 slot->seqNo, trackIdx);
        track->active = FALSE;
    }
}

static BOOL seq_slot_has_tracks(SeqSlot *slot) {
    int i;
    for (i = 0; i < SEQ_TRACKS; ++i) {
        if (slot->tracks[i].active) return TRUE;
    }
    return FALSE;
}

static void seq_finish_slot(int idx) {
    SeqSlot *slot = &s_slots[idx];
    int t, n;
    if (!slot->inUse) return;
    for (t = 0; t < SEQ_TRACKS; ++t) {
        for (n = 0; n < SEQ_TRACK_NOTES; ++n) seq_note_off(slot, t, n);
    }
    if (slot->handle && slot->handle->player == (struct NNSSndSeqPlayer *)slot) slot->handle->player = NULL;
    Log_Line("AUDIO", "SeqPlayer seq=%d finished: %lu notes, %lu dropped, %lu ticks",
             slot->seqNo, (unsigned long)slot->noteCount, (unsigned long)slot->droppedNotes,
             (unsigned long)slot->tick);
    if (slot->droppedNotes)
        Log_Line("AUDIO", "SeqPlayer seq=%d drop reasons: wave=%u synth=%u channels=%u queue=%u",
                 slot->seqNo, slot->missingWave, slot->synthesisFailed, slot->channelUnavailable, slot->queueFailed);
    free(slot->seqData);
    memset(slot, 0, sizeof *slot);
}

static void seq_advance(SeqSlot *slot, float ticks) {
    int t, n;
    u32 whole;
    if (slot->paused) return;
    slot->tickAcc += ticks;
    whole = (u32)slot->tickAcc;
    if (whole == 0) return;
    slot->tickAcc -= (float)whole;
    /* Visit event deadlines in sequence order even when a slow game frame
     * spans several notes. A new note takes its length from its own onset. */
    while (whole) {
        u32 step = whole;
        for (t = 0; t < SEQ_TRACKS; ++t) {
            SeqTrack *track = &slot->tracks[t];
            if (track->active && track->wait < step) step = track->wait;
            for (n = 0; n < SEQ_TRACK_NOTES; ++n) {
                SeqNote *note = &track->notes[n];
                if (note->active) {
                    u32 remaining = note->endTick > slot->tick ? note->endTick - slot->tick : 0;
                    if (remaining < step) step = remaining;
                }
            }
        }
        slot->tick += step;
        whole -= step;
        for (t = 0; t < SEQ_TRACKS; ++t) {
            SeqTrack *track = &slot->tracks[t];
            for (n = 0; n < SEQ_TRACK_NOTES; ++n) {
                if (track->notes[n].active && slot->tick >= track->notes[n].endTick)
                    seq_note_off(slot, t, n);
            }
        }
        for (t = 0; t < SEQ_TRACKS; ++t) {
            SeqTrack *track = &slot->tracks[t];
            if (!track->active) continue;
            track->wait -= step;
            if (!track->wait) seq_process_events(slot, t);
        }
    }
}

static void seq_fade_step(SeqSlot *slot, u64 now) {
    if (slot->fadeTarget < 0 || slot->fadeFramesLeft <= 0) return;
    u64 elapsed = (now - slot->fadeStartTick) * 60 / SYSCLOCK_ARM11;
    int left = elapsed < (u64)slot->fadeFramesTotal ? slot->fadeFramesTotal - (int)elapsed : 0;
    if (left == slot->fadeFramesLeft) return;
    slot->fadeFramesLeft = left;
    if (slot->fadeFramesTotal > 0) {
        int done = slot->fadeFramesTotal - slot->fadeFramesLeft;
        slot->fadeVol = slot->fadeStartVol +
                        (slot->fadeTarget - slot->fadeStartVol) * done / slot->fadeFramesTotal;
    } else {
        slot->fadeVol = slot->fadeTarget;
    }
    if (slot->fadeVol < 0) slot->fadeVol = 0;
    if (slot->fadeVol > 127) slot->fadeVol = 127;
    seq_update_volume(slot, 0xffff);
    if (slot->fadeFramesLeft == 0) {
        slot->fadeVol = slot->fadeTarget;
        slot->fadeTarget = -1;
        if (slot->stopAtFadeEnd && slot->fadeVol == 0) seq_finish_slot((int)(slot - s_slots));
    }
}

static void seq_start_fade(SeqSlot *slot, int target, int frames, BOOL stopAtEnd) {
    if (frames <= 0) {
        slot->fadeVol = target;
        seq_update_volume(slot, 0xffff);
        slot->fadeTarget = -1;
        slot->fadeFramesLeft = 0;
        if (stopAtEnd && target == 0) seq_finish_slot((int)(slot - s_slots));
        return;
    }
    slot->fadeStartTick = svcGetSystemTick();
    slot->fadeStartVol = slot->fadeVol;
    slot->fadeTarget = target;
    slot->fadeFramesTotal = frames;
    slot->fadeFramesLeft = frames;
    slot->stopAtFadeEnd = stopAtEnd;
}

void SeqPlayer_Tick(void) {
    static u64 windowStart, longestGap;
    static unsigned cappedUpdates;
    u64 now, gap;
    float dt = 0.0f;
    bool playing = false;
    int i;
    LightLock_Lock(&s_seqLock);
    /* now/gap/dt live inside the lock: two threads ticking at once
     * (the startup handover) must not both advance the same span. */
    now = svcGetSystemTick();
    gap = s_lastTick ? now - s_lastTick : 0;
    if (s_lastTick) {
        dt = (float)gap / (float)SYSCLOCK_ARM11;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0f) dt = 0.0f;
    }
    s_lastTick = now;
    for (i = 0; i < SEQ_SLOTS; ++i) {
        SeqSlot *slot = &s_slots[i];
        float ticks;
        if (!slot->inUse) continue;
        seq_fade_step(slot, now);
        if (!slot->inUse) continue;
        if (slot->paused) continue;
        playing = true;
        ticks = dt * (float)slot->tempo * 0.8f * ((float)slot->tempoRatio / 256.0f);
        seq_advance(slot, ticks);
        if (slot->inUse && !seq_slot_has_tracks(slot)) seq_finish_slot(i);
    }
    if (playing && windowStart) {
        if (gap > longestGap) longestGap = gap;
        if (gap > SYSCLOCK_ARM11 / 10) ++cappedUpdates;
    }
    if (!playing || !windowStart || now - windowStart >= 5ull * SYSCLOCK_ARM11) {
        if (cappedUpdates)
            Log_Line("AUDIO", "sequence updates over 100 ms: %u, longest gap %llu ms",
                     cappedUpdates, (unsigned long long)(longestGap / (SYSCLOCK_ARM11 / 1000)));
        windowStart = playing ? now : 0;
        longestGap = 0;
        cappedUpdates = 0;
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndMain(void) {
    if (!__atomic_load_n(&s_audioThreadTick, __ATOMIC_ACQUIRE)) SeqPlayer_Tick();
}

/* ---- Player setup ---- */

BOOL NNS_SndArcPlayerSetup(NNSSndHeapHandle heap) {
    int i;
    LightLock_Lock(&s_seqLock);
    s_heap = heap;
    for (i = 0; i < 32; ++i) {
        const NNSSndArcPlayerInfo *info = SndArc_GetPlayerInfo(i);
        s_playerVolume[i] = 127;
        s_playableSeqCount[i] = info && info->seqMax ? info->seqMax : 1;
        s_allocChBitFlag[i] = info ? info->allocChBitFlag : 0xffffu;
    }
    s_setupDone = TRUE;
    LightLock_Unlock(&s_seqLock);
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerSetup (sequence player)");
    return TRUE;
}

BOOL NNS_SndArcPlayerStartSeqEx(NNSSndHandle *handle, int playerNo, int bankNo, int playerPrio,
                                int seqNo) {
    const NNSSndArcSeqInfo *info;
    const u8 *seqBytes;
    u32 dataOff, fileSize;
    u8 *copy;
    int idx, active, lowest = -1, lowestPrio = 0x7fffffff, i;
    SeqSlot *slot;
    u16 mask = 0;
    u32 t0Start = 0;

    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerStartSeqEx (sequence player)");
    Log_Line("AUDIO", "StartSeqEx player=%d bank=%d prio=%d seq=%d", playerNo, bankNo, playerPrio,
             seqNo);
    LightLock_Lock(&s_seqLock);
    if (!handle || playerNo < 0 || playerNo >= 32) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    info = SndArc_GetSeqInfo(seqNo);
    if (!info) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    if (bankNo < 0) bankNo = (int)info->param.bankNo;
    if (playerPrio < 0) playerPrio = (int)info->param.playerPrio;
    active = seq_count_on_player(playerNo);
    if (active >= s_playableSeqCount[playerNo]) {
        for (i = 0; i < SEQ_SLOTS; ++i) {
            if (s_slots[i].inUse && s_slots[i].playerNo == playerNo &&
                s_slots[i].playerPrio < lowestPrio) {
                lowestPrio = s_slots[i].playerPrio;
                lowest = i;
            }
        }
        if (lowest < 0 || playerPrio < lowestPrio) {
            LightLock_Unlock(&s_seqLock);
            return FALSE;
        }
        seq_finish_slot(lowest);
    }
    if (!NNS_SndArcLoadSeqEx(seqNo,
                             NNS_SND_ARC_LOAD_SEQ | NNS_SND_ARC_LOAD_BANK | NNS_SND_ARC_LOAD_WAVE,
                             s_heap)) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    seqBytes = NNS_SndArcGetFileAddress(info->fileId);
    if (!seqBytes) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    dataOff = seq_rd32(seqBytes + 0x18);
    fileSize = SndArc_GetFileSize(info->fileId);
    if (dataOff >= fileSize) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    copy = malloc(fileSize - dataOff);
    if (!copy) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    memcpy(copy, seqBytes + dataOff, fileSize - dataOff);
    idx = -1;
    for (i = 0; i < SEQ_SLOTS; ++i) {
        if (!s_slots[i].inUse) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        free(copy);
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    slot = &s_slots[idx];
    memset(slot, 0, sizeof *slot);
    slot->inUse = TRUE;
    slot->playerNo = playerNo;
    slot->seqNo = seqNo;
    slot->bankNo = bankNo;
    slot->playerPrio = playerPrio;
    slot->seqData = copy;
    slot->seqLen = fileSize - dataOff;
    slot->tempo = 120;
    slot->volume = info->param.volume ? (int)info->param.volume : 127;
    slot->fadeVol = 127;
    slot->fadeTarget = -1;
    slot->tempoRatio = 256;
    /* Track table: 0xFE mask + 0x93 entries, or a single track. */
    if (slot->seqLen > 0 && copy[0] == 0xfe && slot->seqLen >= 3) {
        u32 p = 3;
        mask = seq_rd16(copy + 1);
        while (p + 5 <= slot->seqLen && copy[p] == 0x93) {
            int tno = copy[p + 1];
            u32 addr = (u32)copy[p + 2] | ((u32)copy[p + 3] << 8) | ((u32)copy[p + 4] << 16);
            if (tno >= 0 && tno < SEQ_TRACKS && addr < slot->seqLen) {
                slot->tracks[tno].pos = copy + addr;
                slot->tracks[tno].active = TRUE;
            }
            p += 5;
        }
        t0Start = p;
    } else {
        mask = 1;
        t0Start = 0;
    }
    slot->trackMask = mask;
    slot->tracks[0].pos = copy + t0Start;
    slot->tracks[0].active = TRUE;
    for (i = 0; i < SEQ_TRACKS; ++i) {
        SeqTrack *tr = &slot->tracks[i];
        int n;
        tr->noteWait = TRUE;
        tr->volume = 127;
        tr->expression = 127;
        tr->pan = 64;
        tr->bendRange = 2;
        tr->priority = 64;
        for (n = 0; n < SEQ_TRACK_NOTES; ++n) tr->notes[n].dsCh = -1;
    }
    /* Rebind the handle (the old sequence on it, if any, keeps playing
     * unbound â€” same as NNS_SndHandleReleaseSeq). */
    if (handle->player) {
        SeqSlot *old = seq_slot_from_handle(handle);
        if (old) old->handle = NULL;
    }
    slot->handle = handle;
    handle->player = (struct NNSSndSeqPlayer *)slot;
    for (i = 0; i < SEQ_TRACKS; ++i) {
        if (slot->tracks[i].active) seq_process_events(slot, i);
    }
    Log_Line("AUDIO", "SeqPlayer seq=%d started on player %d, %d tracks, bank %d", seqNo, playerNo,
             __builtin_popcount((unsigned)mask), bankNo);
    LightLock_Unlock(&s_seqLock);
    return TRUE;
}

/* No lock here: StartSeqEx takes it. */
BOOL NNS_SndArcPlayerStartSeq(NNSSndHandle *handle, int seqNo) {
    const NNSSndArcSeqInfo *info;
    PBW2_STUB_LOG("snd", "NNS_SndArcPlayerStartSeq (sequence player)");
    Log_Line("AUDIO", "StartSeq seq=%d", seqNo);
    info = SndArc_GetSeqInfo(seqNo);
    if (!info) return FALSE;
    return NNS_SndArcPlayerStartSeqEx(handle, (int)info->param.playerNo, (int)info->param.bankNo,
                                      (int)info->param.playerPrio, seqNo);
}

/* ---- Player control ---- */

void NNS_SndPlayerStopSeq(NNSSndHandle *handle, int fadeFrame) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    if (fadeFrame > 0) {
        seq_start_fade(slot, 0, fadeFrame, TRUE);
    } else {
        seq_finish_slot((int)(slot - s_slots));
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerPause(NNSSndHandle *handle, BOOL flag) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (slot) slot->paused = flag ? TRUE : FALSE;
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetVolume(NNSSndHandle *handle, int volume) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    slot->volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    seq_update_volume(slot, 0xffff);
    LightLock_Unlock(&s_seqLock);
}

/* No lock here: SetVolume takes it. */
void NNS_SndPlayerSetInitialVolume(NNSSndHandle *handle, int volume) {
    NNS_SndPlayerSetVolume(handle, volume);
}

void NNS_SndPlayerMoveVolume(NNSSndHandle *handle, int targetVolume, int frames) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    if (targetVolume < 0) targetVolume = 0;
    if (targetVolume > 127) targetVolume = 127;
    seq_start_fade(slot, targetVolume, frames, FALSE);
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetPlayerVolume(int playerNo, int volume) {
    LightLock_Lock(&s_seqLock);
    if (playerNo < 0 || playerNo >= 32) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    s_playerVolume[playerNo] = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    for (int i = 0; i < SEQ_SLOTS; ++i) {
        if (s_slots[i].inUse && s_slots[i].playerNo == playerNo) seq_update_volume(&s_slots[i], 0xffff);
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetPlayableSeqCount(int playerNo, int seqCount) {
    LightLock_Lock(&s_seqLock);
    if (playerNo < 0 || playerNo >= 32 || seqCount < 1) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    s_playableSeqCount[playerNo] = seqCount;
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetAllocatableChannel(int playerNo, u32 chBitFlag) {
    LightLock_Lock(&s_seqLock);
    if (playerNo < 0 || playerNo >= 32) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    s_allocChBitFlag[playerNo] = chBitFlag;
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetChannelPriority(NNSSndHandle *handle, int prio) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (slot) slot->playerPrio = prio;
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTrackMute(NNSSndHandle *handle, u16 trackBitMask, BOOL flag) {
    SeqSlot *slot;
    int t, n;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (!(trackBitMask & (1u << t))) continue;
        slot->tracks[t].muted = flag ? TRUE : FALSE;
        if (flag) {
            for (n = 0; n < SEQ_TRACK_NOTES; ++n) seq_note_off(slot, t, n);
        }
    }
    LightLock_Unlock(&s_seqLock);
}

/* No lock here: SetTrackMute takes it. */
void NNS_SndPlayerSetTrackMuteEx(NNSSndHandle *handle, u16 trackBitMask, NNSSndSeqMute muteType) {
    /* Mute types other than plain mute are treated as mute/unmute. */
    NNS_SndPlayerSetTrackMute(handle, trackBitMask, muteType != 0);
}

void NNS_SndPlayerSetTrackVolume(NNSSndHandle *handle, u16 trackBitMask, int volume) {
    SeqSlot *slot;
    int t;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (trackBitMask & (1u << t)) slot->tracks[t].volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    }
    seq_update_volume(slot, trackBitMask);
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTrackPan(NNSSndHandle *handle, u16 trackBitMask, int pan) {
    SeqSlot *slot;
    int t;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (trackBitMask & (1u << t)) slot->tracks[t].pan = pan < -128 ? -128 : pan > 127 ? 127 : pan;
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTrackPitch(NNSSndHandle *handle, u16 trackBitMask, int pitch) {
    SeqSlot *slot;
    int t;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (trackBitMask & (1u << t)) {
            SeqTrack *track = &slot->tracks[t];
            track->pitch = (s16)pitch;
            seq_update_track_pitch(track);
        }
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTrackModDepth(NNSSndHandle *handle, u16 trackBitMask, int value) {
    SeqSlot *slot;
    int t;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (trackBitMask & (1u << t)) slot->tracks[t].modDepth = (u8)value;
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTrackModSpeed(NNSSndHandle *handle, u16 trackBitMask, int value) {
    SeqSlot *slot;
    int t;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot) {
        LightLock_Unlock(&s_seqLock);
        return;
    }
    for (t = 0; t < SEQ_TRACKS; ++t) {
        if (trackBitMask & (1u << t)) slot->tracks[t].modSpeed = (u8)value;
    }
    LightLock_Unlock(&s_seqLock);
}

void NNS_SndPlayerSetTempoRatio(NNSSndHandle *handle, int ratio) {
    SeqSlot *slot;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (slot) slot->tempoRatio = ratio > 0 ? ratio : 256;
    LightLock_Unlock(&s_seqLock);
}

u32 NNS_SndPlayerGetTick(NNSSndHandle *handle) {
    SeqSlot *slot;
    u32 tick;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    tick = slot ? slot->tick : 0;
    LightLock_Unlock(&s_seqLock);
    return tick;
}

int NNS_SndPlayerCountPlayingSeqByPlayerNo(int playerNo) {
    int n;
    LightLock_Lock(&s_seqLock);
    n = seq_count_on_player(playerNo);
    LightLock_Unlock(&s_seqLock);
    return n;
}

int NNS_SndPlayerCountPlayingSeqBySeqNo(int seqNo) {
    int n;
    LightLock_Lock(&s_seqLock);
    n = seq_count_seq(seqNo);
    LightLock_Unlock(&s_seqLock);
    return n;
}

BOOL NNS_SndPlayerReadDriverTrackInfo(NNSSndHandle *handle, int trackNo, SNDTrackInfo *trackInfo) {
    SeqSlot *slot;
    SeqTrack *track;
    int n, chCount = 0;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (!slot || !trackInfo || trackNo < 0 || trackNo >= SEQ_TRACKS) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    track = &slot->tracks[trackNo];
    if (!track->active) {
        LightLock_Unlock(&s_seqLock);
        return FALSE;
    }
    memset(trackInfo, 0, sizeof *trackInfo);
    trackInfo->prgNo = track->program;
    trackInfo->volume = track->volume;
    trackInfo->volume2 = (u8)slot->volume;
    trackInfo->pitchBend = track->bend;
    trackInfo->bendRange = track->bendRange;
    trackInfo->pan = (u8)track->pan;
    trackInfo->transpose = track->transpose;
    for (n = 0; n < SEQ_TRACK_NOTES; ++n) {
        if (track->notes[n].active && track->notes[n].dsCh >= 0 && chCount < SND_CHANNEL_NUM) {
            trackInfo->channel[chCount++] = (u8)track->notes[n].dsCh;
        }
    }
    trackInfo->chCount = (u8)chCount;
    LightLock_Unlock(&s_seqLock);
    return TRUE;
}

void NNS_SndHandleInit(NNSSndHandle *handle) {
    if (handle) handle->player = NULL;
}

void NNS_SndHandleReleaseSeq(NNSSndHandle *handle) {
    SeqSlot *slot;
    if (!handle) return;
    LightLock_Lock(&s_seqLock);
    slot = seq_slot_from_handle(handle);
    if (slot) slot->handle = NULL;
    handle->player = NULL;
    LightLock_Unlock(&s_seqLock);
}

#endif
