/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/mic.h"
#include "stub_log.h"

/* Microphone: the 3DS port has no microphone input path (no ARM7 sampling,
 * no mic buffer delivery), so sampling can never start. Report it honestly
 * as a fatal error instead of pretending success: callers that check the
 * result (SndMic_StartRecording, ctvt_mic) treat it as "mic unavailable"
 * and skip recording. MIC_Init is a no-op void stub in link_stubs.c. */
MICResult MIC_StartLimitedSampling(const MICAutoParam *param) {
    (void)param;
    PBW2_STUB_LOG("mic", "MIC_StartLimitedSampling: no microphone on this platform");
    return MIC_RESULT_FATAL_ERROR;
}

MICResult MIC_StartAutoSamplingAsync(const MICAutoParam *param, MICCallback callback, void *arg) {
    (void)param;
    (void)callback;
    (void)arg;
    PBW2_STUB_LOG("mic", "MIC_StartAutoSamplingAsync: no microphone on this platform");
    return MIC_RESULT_FATAL_ERROR;
}

MICResult MIC_StartLimitedSamplingAsync(const MICAutoParam *param, MICCallback callback, void *arg) {
    (void)param;
    (void)callback;
    (void)arg;
    PBW2_STUB_LOG("mic", "MIC_StartLimitedSamplingAsync: no microphone on this platform");
    return MIC_RESULT_FATAL_ERROR;
}

/* Stops: nothing is ever sampling, so there is nothing to stop — report
 * success so cleanup paths proceed to free their buffers. */
MICResult MIC_StopAutoSampling(void) {
    PBW2_STUB_LOG("mic", "MIC_StopAutoSampling: nothing sampling");
    return MIC_RESULT_SUCCESS;
}

MICResult MIC_StopLimitedSampling(void) {
    PBW2_STUB_LOG("mic", "MIC_StopLimitedSampling: nothing sampling");
    return MIC_RESULT_SUCCESS;
}

void *MIC_GetLastSamplingAddress(void) {
    PBW2_STUB_LOG("mic", "MIC_GetLastSamplingAddress: no microphone on this platform");
    return NULL;
}
