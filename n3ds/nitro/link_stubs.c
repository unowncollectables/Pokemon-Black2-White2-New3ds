/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "stub_log.h"
#include "types.h" /* u8/u32 for the typed Crypt_VerifySignature below */

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
void DWC_CheckHasProfile(void) { PBW2_STUB_LOG("link", "DWC_CheckHasProfile"); }
void DWC_CheckValidConsole(void) { PBW2_STUB_LOG("link", "DWC_CheckValidConsole"); }
void DWC_CreateExchangeToken(void) { PBW2_STUB_LOG("link", "DWC_CreateExchangeToken"); }
void DWC_GetGsProfileId(void) { PBW2_STUB_LOG("link", "DWC_GetGsProfileId"); }
void DWC_IsValidFriendData(void) { PBW2_STUB_LOG("link", "DWC_IsValidFriendData"); }
void DWCi_BssDescToApInfo(void) { PBW2_STUB_LOG("link", "DWCi_BssDescToApInfo"); }
void DWCi_GetBssDescParseWorkSize(void) { PBW2_STUB_LOG("link", "DWCi_GetBssDescParseWorkSize"); }
void GFL_NetCancelEnd(void) { PBW2_STUB_LOG("link", "GFL_NetCancelEnd"); }
void GFL_NetChangeoverConnect(void) { PBW2_STUB_LOG("link", "GFL_NetChangeoverConnect"); }
void GFL_NetErrShow(void) { PBW2_STUB_LOG("link", "GFL_NetErrShow"); }
void GFL_NetExit(void) { PBW2_STUB_LOG("link", "GFL_NetExit"); }
void GFL_NetFreeAligned32(void) { PBW2_STUB_LOG("link", "GFL_NetFreeAligned32"); }
void GFL_NetGetBeaconData(void) { PBW2_STUB_LOG("link", "GFL_NetGetBeaconData"); }
void GFL_NetGetBeaconMac(void) { PBW2_STUB_LOG("link", "GFL_NetGetBeaconMac"); }
void GFL_NetGetHandleNetId(void) { PBW2_STUB_LOG("link", "GFL_NetGetHandleNetId"); }
void GFL_NetGetInitData(void) { PBW2_STUB_LOG("link", "GFL_NetGetInitData"); }
void GFL_NetGetMaxConnectNum(void) { PBW2_STUB_LOG("link", "GFL_NetGetMaxConnectNum"); }
void GFL_NetGetMaxSendSize(void) { PBW2_STUB_LOG("link", "GFL_NetGetMaxSendSize"); }
void GFL_NetGetNegotiatedCount(void) { PBW2_STUB_LOG("link", "GFL_NetGetNegotiatedCount"); }
void GFL_NetGetSys(void) { PBW2_STUB_LOG("link", "GFL_NetGetSys"); }
void GFL_NetGetWork(void) { PBW2_STUB_LOG("link", "GFL_NetGetWork"); }
void GFL_NetHoldScan(void) { PBW2_STUB_LOG("link", "GFL_NetHoldScan"); }
void GFL_NetInit(void) { PBW2_STUB_LOG("link", "GFL_NetInit"); }
void GFL_NetIsHandleNegotiated(void) { PBW2_STUB_LOG("link", "GFL_NetIsHandleNegotiated"); }
void GFL_NetIwFree(void) { PBW2_STUB_LOG("link", "GFL_NetIwFree"); }
void GFL_NetMainWhileWaiting(void) { PBW2_STUB_LOG("link", "GFL_NetMainWhileWaiting"); }
void GFL_NetNotifyChildConnect(void) { PBW2_STUB_LOG("link", "GFL_NetNotifyChildConnect"); }
void GFL_NetSendCommand(void) { PBW2_STUB_LOG("link", "GFL_NetSendCommand"); }
void GFL_NetSendCommandToBits(void) { PBW2_STUB_LOG("link", "GFL_NetSendCommandToBits"); }
void GFL_NetSendCommandToMachine(void) { PBW2_STUB_LOG("link", "GFL_NetSendCommandToMachine"); }
void GFL_NetSetBG1Offset(void) { PBW2_STUB_LOG("link", "GFL_NetSetBG1Offset"); }
void GFL_NetSetIconScreen(void) { PBW2_STUB_LOG("link", "GFL_NetSetIconScreen"); }
void GFL_NetSetTimeout(void) { PBW2_STUB_LOG("link", "GFL_NetSetTimeout"); }
void GFL_NetStartAsChild(void) { PBW2_STUB_LOG("link", "GFL_NetStartAsChild"); }
void GFL_NetStartAsParent(void) { PBW2_STUB_LOG("link", "GFL_NetStartAsParent"); }
void GFL_NetStartScan(void) { PBW2_STUB_LOG("link", "GFL_NetStartScan"); }
void GFL_NetStartServer(void) { PBW2_STUB_LOG("link", "GFL_NetStartServer"); }
void MI_Card_NDmaCopy(void) { PBW2_STUB_LOG("link", "MI_Card_NDmaCopy"); }
void MI_StopAllNDma(void) { PBW2_STUB_LOG("link", "MI_StopAllNDma"); }
void MI_StopNDma(void) { PBW2_STUB_LOG("link", "MI_StopNDma"); }
void MI_WaitNDma(void) { PBW2_STUB_LOG("link", "MI_WaitNDma"); }
/* No microphone hardware on this platform. MIC_StartLimitedSampling lives
 * in mic_stub.c and returns MIC_RESULT_FATAL_ERROR; MIC_Init is a no-op. */
void MIC_Init(void) { PBW2_STUB_LOG("link", "MIC_Init"); }
void NCFG_ReadConfigEx(void) { PBW2_STUB_LOG("link", "NCFG_ReadConfigEx"); }
void NNS_SndArcSetLoadBlockSize(void) { PBW2_STUB_LOG("link", "NNS_SndArcSetLoadBlockSize"); }
void NNS_SndHandleInit(void) { PBW2_STUB_LOG("link", "NNS_SndHandleInit"); }
void NNS_SndHandleReleaseSeq(void) { PBW2_STUB_LOG("link", "NNS_SndHandleReleaseSeq"); }
/* NNS_SndHeapAlloc/Create/Destroy/LoadState/SaveState/GetCurrentLevel are
 * NOT stubbed here: the real lib/nnsys/src/snd/snd_heap.c is linked
 * (closure INCLUDE via lib_nnsys_path_ok). Do not re-add them. */
void NNS_SndInit(void) { PBW2_STUB_LOG("link", "NNS_SndInit"); }
void NNS_SndMain(void) { PBW2_STUB_LOG("link", "NNS_SndMain"); }
void NNS_SndPlayerMoveVolume(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerMoveVolume"); }
void NNS_SndPlayerPause(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerPause"); }
void NNS_SndPlayerSetPlayableSeqCount(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetPlayableSeqCount"); }
void NNS_SndPlayerSetPlayerVolume(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetPlayerVolume"); }
void NNS_SndPlayerSetTempoRatio(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTempoRatio"); }
void NNS_SndPlayerSetTrackModDepth(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackModDepth"); }
void NNS_SndPlayerSetTrackModSpeed(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackModSpeed"); }
void NNS_SndPlayerSetTrackMute(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackMute"); }
void NNS_SndPlayerSetTrackMuteEx(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackMuteEx"); }
void NNS_SndPlayerSetTrackPan(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackPan"); }
void NNS_SndPlayerSetTrackPitch(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackPitch"); }
void NNS_SndPlayerSetTrackVolume(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetTrackVolume"); }
void NNS_SndPlayerSetVolume(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerSetVolume"); }
void NNS_SndPlayerStopSeq(void) { PBW2_STUB_LOG("link", "NNS_SndPlayerStopSeq"); }
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
void allocConfigDSSoftwareFeature(void) { PBW2_STUB_LOG("link", "allocConfigDSSoftwareFeature"); }

/* Slice 2e: online / wireless / camera / DSP stubs (void, logged). The DS
 * wifi stack (DWC/WM), IR, voice chat, camera, DSP and GDS code has no
 * hardware to talk to on this platform, and its callers only run inside
 * online features (wifi sessions, Union Room link, Xtransceiver, photo).
 * These are void stubs following the existing DWC/MusComm precedent in
 * this file; if one fires during field play it shows in the stub log
 * and gets a typed stub then. DSProt_CallCrash/CallRunChecks are void
 * like CallRunChecks1-4 (the checksum never matches, so call sites take
 * their benign tamper branch). Do NOT re-add the 9 typed GFL_Net*
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
void DSProt_CallCrash(void) { PBW2_STUB_LOG("link", "DSProt_CallCrash"); }
void DSProt_CallRunChecks(void) { PBW2_STUB_LOG("link", "DSProt_CallRunChecks"); }
void DWC_AddMatchKeyInt(void) { PBW2_STUB_LOG("link", "DWC_AddMatchKeyInt"); }
void DWC_AddMatchKeyString(void) { PBW2_STUB_LOG("link", "DWC_AddMatchKeyString"); }
void DWC_CheckDirtyFlag(void) { PBW2_STUB_LOG("link", "DWC_CheckDirtyFlag"); }
void DWC_CheckFriendKey(void) { PBW2_STUB_LOG("link", "DWC_CheckFriendKey"); }
void DWC_CheckInet(void) { PBW2_STUB_LOG("link", "DWC_CheckInet"); }
void DWC_CheckProfanityExAsync(void) { PBW2_STUB_LOG("link", "DWC_CheckProfanityExAsync"); }
void DWC_CheckProfanityProcess(void) { PBW2_STUB_LOG("link", "DWC_CheckProfanityProcess"); }
void DWC_CheckUserData(void) { PBW2_STUB_LOG("link", "DWC_CheckUserData"); }
void DWC_CleanupInet(void) { PBW2_STUB_LOG("link", "DWC_CleanupInet"); }
void DWC_ClearDirtyFlag(void) { PBW2_STUB_LOG("link", "DWC_ClearDirtyFlag"); }
void DWC_ClearError(void) { PBW2_STUB_LOG("link", "DWC_ClearError"); }
void DWC_CloseAllConnectionsHard(void) { PBW2_STUB_LOG("link", "DWC_CloseAllConnectionsHard"); }
void DWC_CloseConnectionHard(void) { PBW2_STUB_LOG("link", "DWC_CloseConnectionHard"); }
void DWC_ConnectInetAsync(void) { PBW2_STUB_LOG("link", "DWC_ConnectInetAsync"); }
void DWC_ConnectToAnybodyAsync(void) { PBW2_STUB_LOG("link", "DWC_ConnectToAnybodyAsync"); }
void DWC_ConnectToGameServerAsync(void) { PBW2_STUB_LOG("link", "DWC_ConnectToGameServerAsync"); }
void DWC_CreateFriendKey(void) { PBW2_STUB_LOG("link", "DWC_CreateFriendKey"); }
void DWC_CreateFriendKeyToken(void) { PBW2_STUB_LOG("link", "DWC_CreateFriendKeyToken"); }
void DWC_CreateUserData(void) { PBW2_STUB_LOG("link", "DWC_CreateUserData"); }
void DWC_GetAIDBitmap(void) { PBW2_STUB_LOG("link", "DWC_GetAIDBitmap"); }
void DWC_GetDateTime(void) { PBW2_STUB_LOG("link", "DWC_GetDateTime"); }
void DWC_GetFriendKey(void) { PBW2_STUB_LOG("link", "DWC_GetFriendKey"); }
void DWC_GetFriendStatusData(void) { PBW2_STUB_LOG("link", "DWC_GetFriendStatusData"); }
void DWC_GetInetStatus(void) { PBW2_STUB_LOG("link", "DWC_GetInetStatus"); }
void DWC_GetLastErrorEx(void) { PBW2_STUB_LOG("link", "DWC_GetLastErrorEx"); }
void DWC_GetLinkLevel(void) { PBW2_STUB_LOG("link", "DWC_GetLinkLevel"); }
void DWC_GetMatchIntValue(void) { PBW2_STUB_LOG("link", "DWC_GetMatchIntValue"); }
void DWC_GetMyAID(void) { PBW2_STUB_LOG("link", "DWC_GetMyAID"); }
void DWC_GetNumConnectionHost(void) { PBW2_STUB_LOG("link", "DWC_GetNumConnectionHost"); }
void DWC_InitForProduction(void) { PBW2_STUB_LOG("link", "DWC_InitForProduction"); }
void DWC_InitFriendsMatch(void) { PBW2_STUB_LOG("link", "DWC_InitFriendsMatch"); }
void DWC_InitInetEx(void) { PBW2_STUB_LOG("link", "DWC_InitInetEx"); }
void DWC_IsBuddyFriendData(void) { PBW2_STUB_LOG("link", "DWC_IsBuddyFriendData"); }
void DWC_IsEqualFriendData(void) { PBW2_STUB_LOG("link", "DWC_IsEqualFriendData"); }
void DWC_IsSendableReliable(void) { PBW2_STUB_LOG("link", "DWC_IsSendableReliable"); }
void DWC_IsValidAID(void) { PBW2_STUB_LOG("link", "DWC_IsValidAID"); }
void DWC_LoginAsync(void) { PBW2_STUB_LOG("link", "DWC_LoginAsync"); }
void DWC_NASLoginAsync(void) { PBW2_STUB_LOG("link", "DWC_NASLoginAsync"); }
void DWC_NASLoginProcess(void) { PBW2_STUB_LOG("link", "DWC_NASLoginProcess"); }
void DWC_NdCancelAsync(void) { PBW2_STUB_LOG("link", "DWC_NdCancelAsync"); }
void DWC_NdCleanupAsync(void) { PBW2_STUB_LOG("link", "DWC_NdCleanupAsync"); }
void DWC_NdGetFileAsync(void) { PBW2_STUB_LOG("link", "DWC_NdGetFileAsync"); }
void DWC_NdGetFileListAsync(void) { PBW2_STUB_LOG("link", "DWC_NdGetFileListAsync"); }
void DWC_NdGetProgress(void) { PBW2_STUB_LOG("link", "DWC_NdGetProgress"); }
void DWC_NdInitAsync(void) { PBW2_STUB_LOG("link", "DWC_NdInitAsync"); }
void DWC_NdSetAttr(void) { PBW2_STUB_LOG("link", "DWC_NdSetAttr"); }
void DWC_ProcessFriendsMatch(void) { PBW2_STUB_LOG("link", "DWC_ProcessFriendsMatch"); }
void DWC_ProcessInet(void) { PBW2_STUB_LOG("link", "DWC_ProcessInet"); }
void DWC_SVLGetTokenAsync(void) { PBW2_STUB_LOG("link", "DWC_SVLGetTokenAsync"); }
void DWC_SVLProcess(void) { PBW2_STUB_LOG("link", "DWC_SVLProcess"); }
void DWC_SendReliableBitmap(void) { PBW2_STUB_LOG("link", "DWC_SendReliableBitmap"); }
void DWC_SetBuddyFriendCallback(void) { PBW2_STUB_LOG("link", "DWC_SetBuddyFriendCallback"); }
void DWC_SetConnectionClosedCallback(void) { PBW2_STUB_LOG("link", "DWC_SetConnectionClosedCallback"); }
void DWC_SetOwnStatusData(void) { PBW2_STUB_LOG("link", "DWC_SetOwnStatusData"); }
void DWC_SetRecvBuffer(void) { PBW2_STUB_LOG("link", "DWC_SetRecvBuffer"); }
void DWC_SetRecvTimeoutTime(void) { PBW2_STUB_LOG("link", "DWC_SetRecvTimeoutTime"); }
void DWC_SetUserRecvCallback(void) { PBW2_STUB_LOG("link", "DWC_SetUserRecvCallback"); }
void DWC_SetUserRecvTimeoutCallback(void) { PBW2_STUB_LOG("link", "DWC_SetUserRecvTimeoutCallback"); }
void DWC_SetUserSendCallback(void) { PBW2_STUB_LOG("link", "DWC_SetUserSendCallback"); }
void DWC_SetupGameServer(void) { PBW2_STUB_LOG("link", "DWC_SetupGameServer"); }
void DWC_ShutdownFriendsMatch(void) { PBW2_STUB_LOG("link", "DWC_ShutdownFriendsMatch"); }
void DWC_StartUtility(void) { PBW2_STUB_LOG("link", "DWC_StartUtility"); }
void DWC_UpdateConnection(void) { PBW2_STUB_LOG("link", "DWC_UpdateConnection"); }
void DWC_UpdateServersAsync(void) { PBW2_STUB_LOG("link", "DWC_UpdateServersAsync"); }
void DWCi_RootCertNintendoClass2(void) { PBW2_STUB_LOG("link", "DWCi_RootCertNintendoClass2"); }
void DpwBt_Cancel(void) { PBW2_STUB_LOG("link", "DpwBt_Cancel"); }
void DpwBt_Download(void) { PBW2_STUB_LOG("link", "DpwBt_Download"); }
void DpwBt_End(void) { PBW2_STUB_LOG("link", "DpwBt_End"); }
void DpwBt_GetResult(void) { PBW2_STUB_LOG("link", "DpwBt_GetResult"); }
void DpwBt_GetRoomNumber(void) { PBW2_STUB_LOG("link", "DpwBt_GetRoomNumber"); }
void DpwBt_GetServerState(void) { PBW2_STUB_LOG("link", "DpwBt_GetServerState"); }
void DpwBt_Init(void) { PBW2_STUB_LOG("link", "DpwBt_Init"); }
void DpwBt_IsFinished(void) { PBW2_STUB_LOG("link", "DpwBt_IsFinished"); }
void DpwBt_Process(void) { PBW2_STUB_LOG("link", "DpwBt_Process"); }
void DpwBt_SetProfile(void) { PBW2_STUB_LOG("link", "DpwBt_SetProfile"); }
void DpwBt_Upload(void) { PBW2_STUB_LOG("link", "DpwBt_Upload"); }
void DpwTr_Cancel(void) { PBW2_STUB_LOG("link", "DpwTr_Cancel"); }
void DpwTr_DeleteFinish(void) { PBW2_STUB_LOG("link", "DpwTr_DeleteFinish"); }
void DpwTr_Download(void) { PBW2_STUB_LOG("link", "DpwTr_Download"); }
void DpwTr_End(void) { PBW2_STUB_LOG("link", "DpwTr_End"); }
void DpwTr_Exchange(void) { PBW2_STUB_LOG("link", "DpwTr_Exchange"); }
void DpwTr_ExchangeFinish(void) { PBW2_STUB_LOG("link", "DpwTr_ExchangeFinish"); }
void DpwTr_GetResult(void) { PBW2_STUB_LOG("link", "DpwTr_GetResult"); }
void DpwTr_GetServerState(void) { PBW2_STUB_LOG("link", "DpwTr_GetServerState"); }
void DpwTr_GetTradeResult(void) { PBW2_STUB_LOG("link", "DpwTr_GetTradeResult"); }
void DpwTr_Init(void) { PBW2_STUB_LOG("link", "DpwTr_Init"); }
void DpwTr_IsFinished(void) { PBW2_STUB_LOG("link", "DpwTr_IsFinished"); }
void DpwTr_Process(void) { PBW2_STUB_LOG("link", "DpwTr_Process"); }
void DpwTr_ReturnFinish(void) { PBW2_STUB_LOG("link", "DpwTr_ReturnFinish"); }
void DpwTr_Search(void) { PBW2_STUB_LOG("link", "DpwTr_Search"); }
void DpwTr_SearchEx(void) { PBW2_STUB_LOG("link", "DpwTr_SearchEx"); }
void DpwTr_SetProfile(void) { PBW2_STUB_LOG("link", "DpwTr_SetProfile"); }
void DpwTr_Upload(void) { PBW2_STUB_LOG("link", "DpwTr_Upload"); }
void DpwTr_UploadFinish(void) { PBW2_STUB_LOG("link", "DpwTr_UploadFinish"); }
void Gdb_Shutdown(void) { PBW2_STUB_LOG("link", "Gdb_Shutdown"); }
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
void NHTTPAddHeaderField(void) { PBW2_STUB_LOG("link", "NHTTPAddHeaderField"); }
void NHTTPCleanup(void) { PBW2_STUB_LOG("link", "NHTTPCleanup"); }
void NHTTPCreateConnection(void) { PBW2_STUB_LOG("link", "NHTTPCreateConnection"); }
void NHTTPDestroyConnection(void) { PBW2_STUB_LOG("link", "NHTTPDestroyConnection"); }
void NHTTPGetConnectionError(void) { PBW2_STUB_LOG("link", "NHTTPGetConnectionError"); }
void NHTTPGetError(void) { PBW2_STUB_LOG("link", "NHTTPGetError"); }
void NHTTPGetHeaderAll(void) { PBW2_STUB_LOG("link", "NHTTPGetHeaderAll"); }
void NHTTPGetProgress(void) { PBW2_STUB_LOG("link", "NHTTPGetProgress"); }
void NHTTPGetResultCode(void) { PBW2_STUB_LOG("link", "NHTTPGetResultCode"); }
void NHTTPSetBasicAuthorization(void) { PBW2_STUB_LOG("link", "NHTTPSetBasicAuthorization"); }
void NHTTPSetPostDataRaw(void) { PBW2_STUB_LOG("link", "NHTTPSetPostDataRaw"); }
void NHTTPSetRootCA(void) { PBW2_STUB_LOG("link", "NHTTPSetRootCA"); }
void NHTTPStartConnection(void) { PBW2_STUB_LOG("link", "NHTTPStartConnection"); }
void NHTTPStartup(void) { PBW2_STUB_LOG("link", "NHTTPStartup"); }
void NdReportResults(void) { PBW2_STUB_LOG("link", "NdReportResults"); }
void POKE_NET_GDS_BattleDataFavorite(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataFavorite"); }
void POKE_NET_GDS_BattleDataGet(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataGet"); }
void POKE_NET_GDS_BattleDataRegist(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataRegist"); }
void POKE_NET_GDS_BattleDataSearchCondition(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataSearchCondition"); }
void POKE_NET_GDS_BattleDataSearchList1(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataSearchList1"); }
void POKE_NET_GDS_BattleDataSearchList2(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataSearchList2"); }
void POKE_NET_GDS_BattleDataSearchList3(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_BattleDataSearchList3"); }
void POKE_NET_GDS_GetLastErrorCode(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_GetLastErrorCode"); }
void POKE_NET_GDS_GetResponse(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_GetResponse"); }
void POKE_NET_GDS_GetResponseMaxSize(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_GetResponseMaxSize"); }
void POKE_NET_GDS_GetResponseSize(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_GetResponseSize"); }
void POKE_NET_GDS_GetStatus(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_GetStatus"); }
void POKE_NET_GDS_Initialize(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_Initialize"); }
void POKE_NET_GDS_MusicalShotGet(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_MusicalShotGet"); }
void POKE_NET_GDS_MusicalShotRegist(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_MusicalShotRegist"); }
void POKE_NET_GDS_Release(void) { PBW2_STUB_LOG("link", "POKE_NET_GDS_Release"); }
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

/* Slice 2g: DWC debug-server (Gdb_*) and stats (Sc_*) stubs, multiboot
 * leftovers, the crypto allocator, and the card lock. All void, logged.
 * The Gdb/Sc callers only run inside wifi sessions (wifibattlematch_net.c);
 * the MB/Crypt/OS_Lock callers only run inside download play / the IR
 * genuine-card check / the wifi evilcheck, none of which can start
 * without DWC. Typed exceptions live elsewhere: Crypt_VerifySignature
 * below (its return is read), WM_GetLinkLevel in wm_stub.c, the MB_*
 * family in mb_stub.c, GFL_NetErrGetServerMessage in net_stubs.c. */

void Crypt_SetAllocator(void) { PBW2_STUB_LOG("link", "Crypt_SetAllocator"); }
void Gdb_Cancel(void) { PBW2_STUB_LOG("link", "Gdb_Cancel"); }
void Gdb_CreateRecord(void) { PBW2_STUB_LOG("link", "Gdb_CreateRecord"); }
void Gdb_GetMyRecords(void) { PBW2_STUB_LOG("link", "Gdb_GetMyRecords"); }
void Gdb_GetResult(void) { PBW2_STUB_LOG("link", "Gdb_GetResult"); }
void Gdb_GetSpecificRecords(void) { PBW2_STUB_LOG("link", "Gdb_GetSpecificRecords"); }
void Gdb_GetState(void) { PBW2_STUB_LOG("link", "Gdb_GetState"); }
void Gdb_Startup(void) { PBW2_STUB_LOG("link", "Gdb_Startup"); }
void Gdb_Update(void) { PBW2_STUB_LOG("link", "Gdb_Update"); }
void Gdb_UpdateRecord(void) { PBW2_STUB_LOG("link", "Gdb_UpdateRecord"); }
void OS_LockCard(void) { PBW2_STUB_LOG("link", "OS_LockCard"); }
void OS_UnLockCard(void) { PBW2_STUB_LOG("link", "OS_UnLockCard"); }
void Sc_AddByte(void) { PBW2_STUB_LOG("link", "Sc_AddByte"); }
void Sc_AddInt(void) { PBW2_STUB_LOG("link", "Sc_AddInt"); }
void Sc_BeginGame(void) { PBW2_STUB_LOG("link", "Sc_BeginGame"); }
void Sc_BeginGroup(void) { PBW2_STUB_LOG("link", "Sc_BeginGroup"); }
void Sc_BeginPlayers(void) { PBW2_STUB_LOG("link", "Sc_BeginPlayers"); }
void Sc_BeginTeams(void) { PBW2_STUB_LOG("link", "Sc_BeginTeams"); }
void Sc_CreateReport(void) { PBW2_STUB_LOG("link", "Sc_CreateReport"); }
void Sc_CreateSession(void) { PBW2_STUB_LOG("link", "Sc_CreateSession"); }
void Sc_FinishReport(void) { PBW2_STUB_LOG("link", "Sc_FinishReport"); }
void Sc_FreeReport(void) { PBW2_STUB_LOG("link", "Sc_FreeReport"); }
void Sc_GetCertificate(void) { PBW2_STUB_LOG("link", "Sc_GetCertificate"); }
void Sc_GetConnectionId(void) { PBW2_STUB_LOG("link", "Sc_GetConnectionId"); }
void Sc_GetSessionId(void) { PBW2_STUB_LOG("link", "Sc_GetSessionId"); }
void Sc_SetPlayer(void) { PBW2_STUB_LOG("link", "Sc_SetPlayer"); }
void Sc_SetReportIntention(void) { PBW2_STUB_LOG("link", "Sc_SetReportIntention"); }
void Sc_SetSessionId(void) { PBW2_STUB_LOG("link", "Sc_SetSessionId"); }
void Sc_Shutdown(void) { PBW2_STUB_LOG("link", "Sc_Shutdown"); }
void Sc_Startup(void) { PBW2_STUB_LOG("link", "Sc_Startup"); }
void Sc_SubmitReport(void) { PBW2_STUB_LOG("link", "Sc_SubmitReport"); }
void Sc_Update(void) { PBW2_STUB_LOG("link", "Sc_Update"); }

/* Crypt_VerifySignature: the wifi evilcheck verifies a downloaded party's
 * signature through this. No download ever happens, so nothing verifies:
 * return 0 (invalid), fail closed. */
int Crypt_VerifySignature(const void *data, u32 size, void *signature, const u8 *key) {
    (void)data;
    (void)size;
    (void)signature;
    (void)key;
    PBW2_STUB_LOG("link", "Crypt_VerifySignature: no downloads, nothing verifies (0)");
    return 0;
}

