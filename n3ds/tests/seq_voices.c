/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Use the production allocator with fake NDSP voice handles. */
#include "../nitro/seq_player.c"

#include <stdio.h>

struct NNSSndWaveOut {
    bool allocated;
    bool playing;
    int pan, speed, volume;
};
static struct NNSSndWaveOut s_dspVoices[SEQ_VOICE_SLOTS];

NNSSndWaveOutHandle WaveOut_AllocSequenceChannel(int channel) {
    struct NNSSndWaveOut *voice = &s_dspVoices[channel];
    if (voice->allocated) return NULL;
    voice->allocated = voice->playing = true;
    return voice;
}

NNSSndWaveOutHandle WaveOut_AllocSequenceOverflowChannel(int overflow) {
    if (overflow < 0 || overflow >= SEQ_OVERFLOW_CHANNELS) return NULL;
    struct NNSSndWaveOut *voice = &s_dspVoices[SEQ_DS_CHANNELS + overflow];
    if (voice->allocated) return NULL;
    voice->allocated = voice->playing = true;
    return voice;
}

BOOL NNS_SndWaveOutIsPlaying(NNSSndWaveOutHandle voice) {
    return voice && voice->playing;
}

void NNS_SndWaveOutFreeChannel(NNSSndWaveOutHandle voice) {
    if (voice) voice->allocated = voice->playing = false;
}

void WaveOut_SetPan(NNSSndWaveOutHandle voice, int pan) {
    voice->pan = pan;
}

void NNS_SndWaveOutSetSpeed(NNSSndWaveOutHandle voice, int speed) {
    voice->speed = speed;
}

void NNS_SndWaveOutSetVolume(NNSSndWaveOutHandle voice, int volume) {
    voice->volume = volume;
}

static NNSSndWaveOutHandle note_on(unsigned slot, int priority) {
    SeqNote *note = &s_slots[slot].tracks[0].notes[0];
    note->wave = seq_alloc_channel(&s_slots[slot], 0, 0, priority);
    note->active = note->wave != NULL;
    return note->wave;
}

static const char *check_voices(void) {
    s_allocChBitFlag[0] = (1u << 0) | (1u << 1) | (1u << SEQ_CRY_CHANNEL);
    s_dspVoices[0].allocated = true; /* Direct wave outside the sequencer. */
    s_slots[0].tick = 900;
    s_slots[1].tick = 3;
    if (note_on(0, 90) != &s_dspVoices[1]) return "skip a direct wave allocation";
    /* With the DS voices busy, a lower-priority note no longer
     * drops or steals: it sounds on the first overflow voice, the
     * high-priority note keeps its DS voice, and the cry channel
     * stays reserved. */
    if (note_on(1, 20) != &s_dspVoices[SEQ_DS_CHANNELS] ||
        !s_slots[0].tracks[0].notes[0].active || s_dspVoices[SEQ_CRY_CHANNEL].allocated)
        return "overflow voice instead of a drop, cry channel protected";
    seq_note_off(&s_slots[1], 0, 0);
    if (s_dspVoices[SEQ_DS_CHANNELS].allocated) return "overflow voice frees";

    s_dspVoices[1].playing = false;
    if (note_on(1, 20) != &s_dspVoices[1] || s_slots[0].tracks[0].notes[0].active)
        return "reuse a completed sample before its note deadline";
    s_dspVoices[0].allocated = false;
    if (note_on(0, 20) != &s_dspVoices[0]) return "claim the released direct channel";

    if (note_on(2, 20) != &s_dspVoices[1] || s_slots[1].tracks[0].notes[0].active ||
        !s_slots[0].tracks[0].notes[0].active)
        return "steal the oldest equal-priority note across sequences";

    s_slots[3].playerNo = 1;
    s_allocChBitFlag[1] = 0;
    if (note_on(3, 20) != &s_dspVoices[2]) return "zero archive mask keeps default channel access";
    seq_note_off(&s_slots[3], 0, 0);
    return NULL;
}

static const char *check_mix(void) {
    SeqSlot *slot = &s_slots[0];
    SeqTrack *track = &slot->tracks[0];
    slot->volume = slot->fadeVol = s_playerVolume[0] = 127;
    track->volume = track->expression = 127;
    if (seq_final_volume(slot, track, 127) != 127 || (seq_final_volume(slot, track, 0) & 127))
        return "full and silent mix levels";
    track->volume = 32;
    int quiet = seq_final_volume(slot, track, 2);
    if (!(quiet >> 8) || !(quiet & 127)) return "quiet mix survives intermediate rounding";

    track->pan = 44;
    track->panOffset = 12;
    track->notes[0].instrumentPan = 80;
    seq_update_track_pan(track);
    if (s_dspVoices[0].pan != 72) return "live pan combines sequence, instrument and game adjustment";
    return NULL;
}

static const char *check_tied_sample(void) {
    SeqSlot *slot = &s_slots[0];
    SeqTrack *track = &slot->tracks[0];
    seq_set_tie(slot, 0, true);
    if (!track->tie || s_dspVoices[0].allocated) return "tie command releases existing notes";

    NNSSndWaveOutHandle wave = note_on(0, 20);
    if (!wave) return "start held sample";
    SeqNote *note = &track->notes[0];
    note->key = 60;
    note->held = note->sampled = true;
    note->endTick = 1;
    slot->tick = 50;
    u64 allocations = s_channelOrder;
    if (!seq_continue_tied_note(slot, 0, 72, 96) || note->wave != wave || s_channelOrder != allocations ||
        note->velocity != 96 || wave->speed != 65536 || wave->volume != seq_final_volume(slot, track, 96))
        return "tied note changes pitch and velocity without restarting";

    seq_set_tie(slot, 0, false);
    if (note->active || wave->allocated || track->tie) return "turning tie off releases the held sample";

    wave = note_on(0, 20);
    if (!wave) return "start a one-shot sample";
    note->held = true;
    wave->playing = false;
    if (seq_continue_tied_note(slot, 0, 72, 96) || wave->allocated)
        return "ended one-shot sample allows the next note to start again";
    return NULL;
}

static const char *check_synth_release(void) {
    SeqWave wave = {.attack = 32, .decay = 48, .sustain = 64, .release = 80};
    SeqTrack track = {.attack = 0xff, .decay = 0, .sustain = 0xff, .release = 24};
    seq_synth_apply_overrides(&wave, &track);
    if (wave.attack != 32 || wave.sustain != 64 || wave.decay || wave.release != 24)
        return "synth ADSR overrides preserve unspecified instrument fields";
    if (seq_synth_envelope(0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 0.25f) != 0.5f ||
        seq_synth_envelope(1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.25f) != 0.25f ||
        seq_synth_envelope(0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.25f) != 0.0f)
        return "a short note releases from its current attack level";
    return NULL;
}

static const char *check_overflow_pool(void) {
    /* Fresh allocator state; the default (zero) archive mask gives
     * every DS channel except the cry reserve. */
    memset(s_slots, 0, sizeof s_slots);
    memset(s_channels, 0, sizeof s_channels);
    memset(s_dspVoices, 0, sizeof s_dspVoices);
    s_channelOrder = 0;
    s_allocChBitFlag[0] = 0;

    /* 22 usable voices: 15 DS-mapped + 7 overflow. */
    for (int i = 0; i < SEQ_VOICE_SLOTS - 1; ++i) {
        if (!note_on(i, 50)) return "pool fills to 22 usable voices";
    }
    if (s_dspVoices[SEQ_CRY_CHANNEL].allocated) return "cry channel stays reserved at full pool";
    for (int i = SEQ_DS_CHANNELS; i < SEQ_VOICE_SLOTS; ++i) {
        if (!s_dspVoices[i].allocated) return "every overflow voice is used before stealing";
    }
    /* Above the cap, stealing is the only valve — and it still
     * respects priority: a quieter note cannot take a voice. */
    if (note_on(SEQ_VOICE_SLOTS - 1, 10)) return "low priority note cannot steal above the cap";
    if (note_on(SEQ_VOICE_SLOTS - 1, 90) != &s_dspVoices[0] ||
        s_slots[0].tracks[0].notes[0].active || s_dspVoices[SEQ_CRY_CHANNEL].allocated)
        return "high priority note steals the oldest voice above the cap";
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    const char *failure = check_voices();
    if (!failure) failure = check_mix();
    if (!failure) failure = check_tied_sample();
    if (!failure) failure = check_synth_release();
    if (!failure) failure = check_overflow_pool();
    FILE *out = fopen("sdmc:/seq-voices-test.txt", "w");
    if (out) {
        fprintf(out, "%s\n", failure ? failure : "PASS");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000ll);
}
