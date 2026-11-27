/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Use the production allocator with fake NDSP voice handles. */
#include "../nitro/seq_player.c"

#include <stdio.h>

struct NNSSndWaveOut {
    bool allocated;
    bool playing;
    int pan;
};
static struct NNSSndWaveOut s_dspVoices[SEQ_DS_CHANNELS];

NNSSndWaveOutHandle WaveOut_AllocSequenceChannel(int channel) {
    struct NNSSndWaveOut *voice = &s_dspVoices[channel];
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
    if (note_on(1, 20) || !s_slots[0].tracks[0].notes[0].active ||
        s_dspVoices[SEQ_CRY_CHANNEL].allocated)
        return "protect music priority and the cry channel";

    s_dspVoices[1].playing = false;
    if (note_on(1, 20) != &s_dspVoices[1] || s_slots[0].tracks[0].notes[0].active)
        return "reuse a completed sample before its note deadline";
    s_dspVoices[0].allocated = false;
    if (note_on(0, 20) != &s_dspVoices[0]) return "claim the released direct channel";

    if (note_on(2, 20) != &s_dspVoices[1] || s_slots[1].tracks[0].notes[0].active ||
        !s_slots[0].tracks[0].notes[0].active)
        return "steal the oldest equal-priority note across sequences";
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

int main(void) {
    archiveMountSdmc();
    const char *failure = check_voices();
    if (!failure) failure = check_mix();
    FILE *out = fopen("sdmc:/seq-voices-test.txt", "w");
    if (out) {
        fprintf(out, "%s\n", failure ? failure : "PASS");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000ll);
}
