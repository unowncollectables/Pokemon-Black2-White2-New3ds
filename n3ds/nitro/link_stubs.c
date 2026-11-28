/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "stub_log.h"
#include "types.h"

/* Title-slice link stubs; extend with scripts/n3ds_regen_link_stubs.sh */

void CARD_CalcRomHashBufferLength(void) { PBW2_STUB_LOG("link", "CARD_CalcRomHashBufferLength"); }
void CARD_InitRomHashContext(void) { PBW2_STUB_LOG("link", "CARD_InitRomHashContext"); }
void CARD_NotifyRomHashReadAsync(void) { PBW2_STUB_LOG("link", "CARD_NotifyRomHashReadAsync"); }
void CARD_ReadRomHashImage(void) { PBW2_STUB_LOG("link", "CARD_ReadRomHashImage"); }
void CTRDG_DummyInit(void) { PBW2_STUB_LOG("link", "CTRDG_DummyInit"); }
void CTRDG_IsExisting(void) { PBW2_STUB_LOG("link", "CTRDG_IsExisting"); }
void DSProt_CallRunChecks1(void) { PBW2_STUB_LOG("link", "DSProt_CallRunChecks1"); }
void DSProt_CallRunChecks2(void) { PBW2_STUB_LOG("link", "DSProt_CallRunChecks2"); }
/* RunChecks3/4: same story as 1/2 — the checksum over this stub's code
 * never matches DSPROT_CHECKSUM, so the call sites take their benign
 * tamper branch (a small heap allocation / seed nudge in the game's own
 * handlers). The real checks verify the DS ROM's code and cannot pass
 * on this port. */
void DSProt_CallRunChecks3(void) { PBW2_STUB_LOG("link", "DSProt_CallRunChecks3"); }
void DSProt_CallRunChecks4(void) { PBW2_STUB_LOG("link", "DSProt_CallRunChecks4"); }
void DSProt_Init(void) { PBW2_STUB_LOG("link", "DSProt_Init"); }
void MI_Card_NDmaCopy(void) { PBW2_STUB_LOG("link", "MI_Card_NDmaCopy"); }
void MI_StopAllNDma(void) { PBW2_STUB_LOG("link", "MI_StopAllNDma"); }
void MI_StopNDma(void) { PBW2_STUB_LOG("link", "MI_StopNDma"); }
void MI_WaitNDma(void) { PBW2_STUB_LOG("link", "MI_WaitNDma"); }
/* No microphone hardware on this platform. MIC_StartLimitedSampling lives
 * in mic_stub.c and returns MIC_RESULT_FATAL_ERROR; MIC_Init is a no-op. */
void MIC_Init(void) { PBW2_STUB_LOG("link", "MIC_Init"); }
void NNS_SndArcSetLoadBlockSize(void) { PBW2_STUB_LOG("link", "NNS_SndArcSetLoadBlockSize"); }
/* NNS_SndHeapAlloc/Create/Destroy/LoadState/SaveState/GetCurrentLevel are
 * NOT stubbed here: the real lib/nnsys/src/snd/snd_heap.c is linked
 * (closure INCLUDE via lib_nnsys_path_ok). Do not re-add them. */
void NNS_SndInit(void) { PBW2_STUB_LOG("link", "NNS_SndInit"); }
void NNS_SndSetMasterVolume(void) { PBW2_STUB_LOG("link", "NNS_SndSetMasterVolume"); }
void NNS_SndSetMonoFlag(void) { PBW2_STUB_LOG("link", "NNS_SndSetMonoFlag"); }
void NNS_SndUpdateDriverInfo(void) { PBW2_STUB_LOG("link", "NNS_SndUpdateDriverInfo"); }
void OS_DestroyThread(void) { PBW2_STUB_LOG("link", "OS_DestroyThread"); }
void OS_DisableIrqMask(void) { PBW2_STUB_LOG("link", "OS_DisableIrqMask"); }
void OS_GetParentalControlInfoPtr(void) { PBW2_STUB_LOG("link", "OS_GetParentalControlInfoPtr"); }
void OS_ResetRequestIrqMask(void) { PBW2_STUB_LOG("link", "OS_ResetRequestIrqMask"); }
void OS_SetIrqMask(void) { PBW2_STUB_LOG("link", "OS_SetIrqMask"); }
void OSi_EnterDmaCallback(void) { PBW2_STUB_LOG("link", "OSi_EnterDmaCallback"); }
void OSi_UnlockVram(void) { PBW2_STUB_LOG("link", "OSi_UnlockVram"); }
void SNDEXi_Init(void) { PBW2_STUB_LOG("link", "SNDEXi_Init"); }
/* IIR filter for the DS ADC: no ADC hardware on this platform, and the mic
 * path it serves is stubbed in mic_stub.c. */
void SNDEXi_SetIirFilter(void) { PBW2_STUB_LOG("link", "SNDEXi_SetIirFilter"); }

/* Slice 2e: online / wireless / camera / DSP stubs (void, logged). The IR,
 * voice chat, camera and DSP code has no hardware to talk to on this
 * platform, and its callers only run inside online features (wifi
 * sessions, Union Room link, Xtransceiver, photo). Wi-Fi Stage 1 retired
 * the DWC/NHTTP/GDS/Gdb/Sc families from this file: the real lib/dwc,
 * lib/nitrowifi, lib/dpw and lib/poke_net objects now link (see
 * scripts/n3ds_link_closure.py and n3ds/nitro/net_bottom.c). What remains
 * stubbed here (VCT voice chat, WM local radio, IR) still has no backend.
 * These are void stubs following the existing precedent in this file;
 * if one fires during field play it shows in the stub log
 * and gets a typed stub then. DSProt_CallRunChecks1-4 are void like the
 * other stubs (the checksum never matches, so CHECKED_CALL call sites take
 * their benign tamper branch). DSProt_CallCrash/CallRunChecks below are
 * NOT void stubs: pokelist.c's DSPROT_CHECKED_RUN dereferences the tamper
 * path's return value, so CallRunChecks has a real implementation and
 * CallCrash a typed signature. Do NOT re-add the 9 typed GFL_Net*
 * functions here — they live in net_stubs.c with honest returns. */

void CAMERA_ClearBufferCore(void) { PBW2_STUB_LOG("link", "CAMERA_ClearBufferCore"); }
void CAMERA_EndCore(void) { PBW2_STUB_LOG("link", "CAMERA_EndCore"); }
void CAMERA_GetBytesAtOnceCore(void) { PBW2_STUB_LOG("link", "CAMERA_GetBytesAtOnceCore"); }
void CAMERA_GetMaxLinesRoundCore(void) { PBW2_STUB_LOG("link", "CAMERA_GetMaxLinesRoundCore"); }
void CAMERA_I2CActivateCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CActivateCore"); }
void CAMERA_I2CAutoExposureCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CAutoExposureCore"); }
void CAMERA_I2CAutoWhiteBalanceCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CAutoWhiteBalanceCore"); }
void CAMERA_I2CEffectExCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CEffectExCore"); }
void CAMERA_I2CInitCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CInitCore"); }
void CAMERA_I2CSizeExCore(void) { PBW2_STUB_LOG("link", "CAMERA_I2CSizeExCore"); }
void CAMERA_InitCore(void) { PBW2_STUB_LOG("link", "CAMERA_InitCore"); }
void CAMERA_IsBusyCore(void) { PBW2_STUB_LOG("link", "CAMERA_IsBusyCore"); }
void CAMERA_SetBufferErrorCallbackCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetBufferErrorCallbackCore"); }
void CAMERA_SetOutputFormatCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetOutputFormatCore"); }
void CAMERA_SetRebootCallbackCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetRebootCallbackCore"); }
void CAMERA_SetTransferLinesCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetTransferLinesCore"); }
void CAMERA_SetTrimmingCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetTrimmingCore"); }
void CAMERA_SetVsyncCallbackCore(void) { PBW2_STUB_LOG("link", "CAMERA_SetVsyncCallbackCore"); }
void CAMERA_SizeToHeightCore(void) { PBW2_STUB_LOG("link", "CAMERA_SizeToHeightCore"); }
void CAMERA_SizeToWidthCore(void) { PBW2_STUB_LOG("link", "CAMERA_SizeToWidthCore"); }
void CAMERA_StartCaptureCore(void) { PBW2_STUB_LOG("link", "CAMERA_StartCaptureCore"); }
void CAMERA_StopCaptureCore(void) { PBW2_STUB_LOG("link", "CAMERA_StopCaptureCore"); }
void CAMERA_StopCore(void) { PBW2_STUB_LOG("link", "CAMERA_StopCore"); }
void DSPi_IsShutterSoundPlayingCore(void) { PBW2_STUB_LOG("link", "DSPi_IsShutterSoundPlayingCore"); }
void DSPi_LoadG711Core(void) { PBW2_STUB_LOG("link", "DSPi_LoadG711Core"); }
void DSPi_LoadGraphicsCore(void) { PBW2_STUB_LOG("link", "DSPi_LoadGraphicsCore"); }
void DSPi_OpenStaticComponentG711Core(void) { PBW2_STUB_LOG("link", "DSPi_OpenStaticComponentG711Core"); }
void DSPi_OpenStaticComponentGraphicsCore(void) { PBW2_STUB_LOG("link", "DSPi_OpenStaticComponentGraphicsCore"); }
void DSPi_PlayShutterSoundCore(void) { PBW2_STUB_LOG("link", "DSPi_PlayShutterSoundCore"); }
void DSPi_ScalingCore(void) { PBW2_STUB_LOG("link", "DSPi_ScalingCore"); }
void DSPi_StopSoundCore(void) { PBW2_STUB_LOG("link", "DSPi_StopSoundCore"); }
void DSPi_UnloadG711Core(void) { PBW2_STUB_LOG("link", "DSPi_UnloadG711Core"); }
void DSPi_UnloadGraphicsCore(void) { PBW2_STUB_LOG("link", "DSPi_UnloadGraphicsCore"); }
/* DS Protect's overlay-165 entry points, with their real signatures
 * (lib/dsprot/include/dsprot/dsprot.h). The anti-piracy checks have
 * nothing to check on this host — the ROM is the user's own — so
 * CallRunChecks simply runs the game's callback and returns what it
 * returns, which is what the real function does once its checks pass.
 * CallCrash stays a logged no-op returning NULL: its only remaining
 * callers (dsprot_detect.c) ignore the return value, and the host
 * DSPROT_CHECKED_RUN never routes here. */
void *DSProt_CallRunChecks(void *(*callback)(void *, void *), void *arg0, void *arg1) {
    return callback(arg0, arg1);
}
void *DSProt_CallCrash(void *arg0, void *arg1) {
    (void)arg0;
    (void)arg1;
    PBW2_STUB_LOG("link", "DSProt_CallCrash");
    return 0;
}
void DWC_StartUtility(void) { PBW2_STUB_LOG("link", "DWC_StartUtility"); }
void IRC_Connect(void) { PBW2_STUB_LOG("link", "IRC_Connect"); }
void IRC_GetUnitNumber(void) { PBW2_STUB_LOG("link", "IRC_GetUnitNumber"); }
void IRC_Init(void) { PBW2_STUB_LOG("link", "IRC_Init"); }
void IRC_IsConnect(void) { PBW2_STUB_LOG("link", "IRC_IsConnect"); }
void IRC_IsSender(void) { PBW2_STUB_LOG("link", "IRC_IsSender"); }
void IRC_Move(void) { PBW2_STUB_LOG("link", "IRC_Move"); }
void IRC_Send(void) { PBW2_STUB_LOG("link", "IRC_Send"); }
void IRC_SetRecvCallback(void) { PBW2_STUB_LOG("link", "IRC_SetRecvCallback"); }
void IRC_Shutdown(void) { PBW2_STUB_LOG("link", "IRC_Shutdown"); }
void MI_Camera_NDmaRecvAsync(void) { PBW2_STUB_LOG("link", "MI_Camera_NDmaRecvAsync"); }
void SSP_GetJpegEncoderBufferSize(void) { PBW2_STUB_LOG("link", "SSP_GetJpegEncoderBufferSize"); }
void SSP_StartJpegDecoder(void) { PBW2_STUB_LOG("link", "SSP_StartJpegDecoder"); }
void SSP_StartJpegEncoder(void) { PBW2_STUB_LOG("link", "SSP_StartJpegEncoder"); }
void VCT_AddConferenceClient(void) { PBW2_STUB_LOG("link", "VCT_AddConferenceClient"); }
void VCT_Cleanup(void) { PBW2_STUB_LOG("link", "VCT_Cleanup"); }
void VCT_CreateSession(void) { PBW2_STUB_LOG("link", "VCT_CreateSession"); }
void VCT_DeleteSession(void) { PBW2_STUB_LOG("link", "VCT_DeleteSession"); }
void VCT_EnableEchoCancel(void) { PBW2_STUB_LOG("link", "VCT_EnableEchoCancel"); }
void VCT_EnableVAD(void) { PBW2_STUB_LOG("link", "VCT_EnableVAD"); }
void VCT_GetVADInfo(void) { PBW2_STUB_LOG("link", "VCT_GetVADInfo"); }
void VCT_HandleData(void) { PBW2_STUB_LOG("link", "VCT_HandleData"); }
void VCT_Init(void) { PBW2_STUB_LOG("link", "VCT_Init"); }
void VCT_Main(void) { PBW2_STUB_LOG("link", "VCT_Main"); }
void VCT_ReceiveAudio(void) { PBW2_STUB_LOG("link", "VCT_ReceiveAudio"); }
void VCT_RemoveConferenceClient(void) { PBW2_STUB_LOG("link", "VCT_RemoveConferenceClient"); }
void VCT_Request(void) { PBW2_STUB_LOG("link", "VCT_Request"); }
void VCT_Response(void) { PBW2_STUB_LOG("link", "VCT_Response"); }
void VCT_SendAudio(void) { PBW2_STUB_LOG("link", "VCT_SendAudio"); }
void VCT_SetCodec(void) { PBW2_STUB_LOG("link", "VCT_SetCodec"); }
void VCT_StartStreaming(void) { PBW2_STUB_LOG("link", "VCT_StartStreaming"); }
void VCT_StopStreaming(void) { PBW2_STUB_LOG("link", "VCT_StopStreaming"); }
void WM_Disconnect(void) { PBW2_STUB_LOG("link", "WM_Disconnect"); }
void WM_End(void) { PBW2_STUB_LOG("link", "WM_End"); }
void WM_EndDataSharing(void) { PBW2_STUB_LOG("link", "WM_EndDataSharing"); }
void WM_EndMP(void) { PBW2_STUB_LOG("link", "WM_EndMP"); }
void WM_EndParent(void) { PBW2_STUB_LOG("link", "WM_EndParent"); }
void WM_EndScan(void) { PBW2_STUB_LOG("link", "WM_EndScan"); }
void WM_GetAllowedChannel(void) { PBW2_STUB_LOG("link", "WM_GetAllowedChannel"); }
void WM_GetDispersionBeaconPeriod(void) { PBW2_STUB_LOG("link", "WM_GetDispersionBeaconPeriod"); }
void WM_GetDispersionScanPeriod(void) { PBW2_STUB_LOG("link", "WM_GetDispersionScanPeriod"); }
void WM_GetMPReceiveBufferSize(void) { PBW2_STUB_LOG("link", "WM_GetMPReceiveBufferSize"); }
void WM_GetMPSendBufferSize(void) { PBW2_STUB_LOG("link", "WM_GetMPSendBufferSize"); }
void WM_GetNextTgid(void) { PBW2_STUB_LOG("link", "WM_GetNextTgid"); }
void WM_GetSharedDataAddress(void) { PBW2_STUB_LOG("link", "WM_GetSharedDataAddress"); }
void WM_Initialize(void) { PBW2_STUB_LOG("link", "WM_Initialize"); }
void WM_InitializeForListening(void) { PBW2_STUB_LOG("link", "WM_InitializeForListening"); }
void WM_MeasureChannel(void) { PBW2_STUB_LOG("link", "WM_MeasureChannel"); }
void WM_Reset(void) { PBW2_STUB_LOG("link", "WM_Reset"); }
void WM_SetGameInfo(void) { PBW2_STUB_LOG("link", "WM_SetGameInfo"); }
void WM_SetIndCallback(void) { PBW2_STUB_LOG("link", "WM_SetIndCallback"); }
void WM_SetLifeTime(void) { PBW2_STUB_LOG("link", "WM_SetLifeTime"); }
void WM_SetMPDataToPortEx(void) { PBW2_STUB_LOG("link", "WM_SetMPDataToPortEx"); }
void WM_SetParentParameter(void) { PBW2_STUB_LOG("link", "WM_SetParentParameter"); }
void WM_SetPortCallback(void) { PBW2_STUB_LOG("link", "WM_SetPortCallback"); }
void WM_StartConnectEx(void) { PBW2_STUB_LOG("link", "WM_StartConnectEx"); }
void WM_StartDataSharing(void) { PBW2_STUB_LOG("link", "WM_StartDataSharing"); }
void WM_StartMP(void) { PBW2_STUB_LOG("link", "WM_StartMP"); }
void WM_StartParent(void) { PBW2_STUB_LOG("link", "WM_StartParent"); }
void WM_StartScanEx(void) { PBW2_STUB_LOG("link", "WM_StartScanEx"); }
void WM_StepDataSharing(void) { PBW2_STUB_LOG("link", "WM_StepDataSharing"); }

/* Slice 2g (remainder): multiboot leftovers and the card lock. All void,
 * logged. The MB/OS_Lock callers only run inside download play / the IR
 * genuine-card check / the wifi evilcheck. Typed exceptions live
 * elsewhere: WM_GetLinkLevel in wm_stub.c, the MB_* family in mb_stub.c,
 * GFL_NetErrGetServerMessage in net_stubs.c.
 *
 * Wi-Fi Stage 1: the DWC family (incl. DWC_Nd), DWCi, NHTTP,
 * POKE_NET_GDS, DpwTr, DpwBt, Gdb, Sc, NdReportResults,
 * NCFG_ReadConfigEx, Crypt_VerifySignature and Crypt_SetAllocator are
 * NOT stubbed here anymore — the real lib/dwc, lib/nitrowifi, lib/dpw
 * and lib/poke_net objects link via scripts/n3ds_link_closure.py.
 * Do not re-add them. VCT_* above stays stubbed by design (see the
 * closure script). */

void OS_LockCard(void) { PBW2_STUB_LOG("link", "OS_LockCard"); }
void OS_UnLockCard(void) { PBW2_STUB_LOG("link", "OS_UnLockCard"); }


