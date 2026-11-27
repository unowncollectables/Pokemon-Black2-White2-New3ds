#include "types.h"
#include "gfl/g3d.h"
#include "gfl/net.h"
#include "gfl/net_system.h"
#include "gfl/overlay.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "nitro/gx.h"
#include "nitro/hw.h"
#include "nitro/mi.h"
#include "nitro/os.h"
#include "pml/poke_party.h"
#include "pml/species_names.h"
#include "save/config.h"
#include "save/high_link.h"
#include "save/join_avenue.h"
#include "system/boot_proc.h"
#include "system/brightness.h"
#include "system/game_beacon.h"
#include "system/gfl_use.h"
#include "system/machine_use.h"
#include "system/main.h"

#ifdef PBW2_N3DS
#include "g3_raster.h"
#include "log.h"
#endif
#include "system/printsys.h"
#include "system/savedata/save_control.h"
#include "system/savedata/trainer_data_savedata.h"
#include "system/text_speed.h"
#include "system/wih_dwc.h"
#include "system/wipe.h"

// The game's main file: the main loop, the game's setup and its per-frame updates. The file name is a guess, as the
// ROM does not embed it. Names from swan (https://github.com/ds-pokemon-hacking/swan, GPL-3.0), except those of the
// statics

static BOOL IsMainMemoryPriorityFree(void);
static void GFLAppIRQHBlank(void);
static void GFLAppIRQVBlank(void);
static void GameUpdateVBlank(void);
static void GameInit(void);
static void GameUpdate(void);
static void setMonoSnd(const Config *config);

// Not static: it is the first object of .bss, and the boot code's parameter block (0x02004fb8), which is not
// decompiled, points at the start of .bss through this symbol
BOOL g_MainMemoryPriorityBlocked;

#ifdef PBW2_N3DS
static void LogFramePhases(u64 start, u64 updateEnd, u64 swapEnd, u64 waitEnd, u64 transferEnd, u64 finish) {
    static struct {
        u64 start;
        u64 update;
        u64 submit;
        u64 wait;
        u64 transfers;
        u64 render;
        unsigned samples;
    } profile;
    if (!profile.samples) {
        profile.start = start;
    }
    profile.update += updateEnd - start;
    profile.submit += swapEnd - updateEnd;
    profile.wait += waitEnd - swapEnd;
    profile.transfers += transferEnd - waitEnd;
    profile.render += finish - transferEnd;
    ++profile.samples;
    if (finish - profile.start >= OS_MilliSecondsToTicks(4000)) {
        Log_Line("PERF2", "main-loop %u samples avg us: update=%u submit=%u wait+irq=%u transfers=%u render=%u",
                 profile.samples, (unsigned)(OS_TicksToMicroSeconds(profile.update) / profile.samples),
                 (unsigned)(OS_TicksToMicroSeconds(profile.submit) / profile.samples),
                 (unsigned)(OS_TicksToMicroSeconds(profile.wait) / profile.samples),
                 (unsigned)(OS_TicksToMicroSeconds(profile.transfers) / profile.samples),
                 (unsigned)(OS_TicksToMicroSeconds(profile.render) / profile.samples));
        profile.update = profile.submit = profile.wait = profile.transfers = profile.render = 0;
        profile.samples = 0;
    }
}
#endif

void main(void) {
    SystemInit();
    GFLAppInit();
    OS_SetIrqFunction(OS_IE_H_BLANK, GFLAppIRQHBlank);
    OS_SetIrqFunction(OS_IE_V_BLANK, GFLAppIRQVBlank);
    OS_EnableIrqMask(OS_IE_V_BLANK | OS_IE_H_BLANK);
    (void)OS_EnableIrq();
    GX_HBlankIntr(TRUE);
    GX_VBlankIntr(TRUE);
    OS_EnableInterrupts();
    GameInit();

    while (TRUE) {
        BOOL priority;
#ifdef PBW2_N3DS
        u64 phaseStart = OS_GetTick();
#endif

        Machine_ProcessExitRequest();
        GFLAppUpdate();
        GameUpdate();
        GflUse_NoOpFrameHook();
#ifdef PBW2_N3DS
        u64 updateEnd = OS_GetTick();
        PbW2_G3RasterDeferSwap(true);
#endif
        GFL_G3DSysCheckSwapBuffers();
#ifdef PBW2_N3DS
        u64 swapEnd = OS_GetTick();
#endif

        // The ARM9 has the main memory first while it waits for the VBlank, and the sound thread waits with it
        priority = IsMainMemoryPriorityFree();
        if (priority) {
            MI_SetMainMemoryPriority(MI_PROCESSOR_ARM9);
        }
        if (!OS_IsThreadTerminated(&g_GFLSndThread)) {
            OS_SleepThreadDirect(&g_GFLSndThread, NULL);
        }
        OS_WaitIrq(TRUE, OS_IE_V_BLANK);
#ifdef PBW2_N3DS
        u64 waitEnd = OS_GetTick();
#endif
        GameUpdateVBlank();
#ifdef PBW2_N3DS
        u64 transferEnd = OS_GetTick();
        PbW2_G3RasterDeferSwap(false);
        LogFramePhases(phaseStart, updateEnd, swapEnd, waitEnd, transferEnd, OS_GetTick());
#endif
        if (!OS_IsThreadTerminated(&g_GFLSndThread)) {
            OS_WakeupThreadDirect(&g_GFLSndThread);
        }
        if (priority) {
            MI_SetMainMemoryPriority(MI_PROCESSOR_ARM7);
        }
    }
}

void SetMainMemoryPriorityBlocked(BOOL blocked) {
    g_MainMemoryPriorityBlocked = blocked;
}

// FALSE while the network, a sound stream or SetMainMemoryPriorityBlocked needs the ARM7 to keep its priority
static BOOL IsMainMemoryPriorityFree(void) {
    if (NetSystem_CountConnected() > 1) {
        return FALSE;
    }
    if (GFL_NetIsInfrared()) {
        return FALSE;
    }
    if (GFL_NetIsWifi()) {
        return FALSE;
    }
    if (GFL_SndStreamCheckInit()) {
        return FALSE;
    }
    if (g_MainMemoryPriorityBlocked == FALSE) {
        return TRUE;
    }
    return FALSE;
}

static void GFLAppIRQHBlank(void) {
    GFL_HBlankTCBUpdate();
}

static void GFLAppIRQVBlank(void) {
    GFL_VBlankExecCallback();
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
    MI_WaitDma(3);
}

static void GameUpdateVBlank(void) {
    SystemUpdateVBlank();
    GFL_VBlankUpdate();
}

static void GameInit(void) {
#ifdef PBW2_N3DS
    Log_Line("game", "GameInit enter");
#endif
    GFL_OvlLoad(OVERLAY_ID(12));
    GFL_OvlLoad(OVERLAY_ID(27));
    GFL_TextRndInit(0);
    g_MainMemoryPriorityBlocked = FALSE;
    GFL_NetBoot(1, 0, 1, 12);
    setupDownloadPlayConfig(12);
    downloadPlayConfigForWifi();
    GCTX_ProcMgrQueueProc(OVERLAY_NONE, &EVENT_TITLE_FUNCS, NULL);

    SaveControl_Create(7);
    Config_ApplyMsgLanguage(&getTrainerDataBlkAddress(SaveControl_GetInstance())->config);
    saveMainSaveAreaAddress(SaveControl_GetInstance());
    setImportantJABlockAddresses(SaveControl_GetInstance());

    GFL_NetHeapInit();
    GFL_NetErrInit(8);
    GFL_DebugSetAssertFailCallback(AssertFailErrorDisp);
    PML_SpeciesNamesResidentInit(0);
    PML_PersonalInit(0);
    GFL_SndInit();
    PokeVoice_Init(1, getResIDFunc, getWaveInfoFunc);
    setMonoSnd(&getTrainerDataBlkAddress(SaveControl_GetInstance())->config);
#ifdef PBW2_N3DS
    Log_Line("game", "GameInit setMonoSnd done");
#endif
    initKeyBlk();
#ifdef PBW2_N3DS
    Log_Line("game", "GameInit initKeyBlk done");
#endif
    GameBeaconSys_Create(1);
#ifdef PBW2_N3DS
    Log_Line("game", "GameInit GameBeaconSys_Create done");
#endif
    PassPower_Init();
#ifdef PBW2_N3DS
    Log_Line("game", "GameInit return");
#endif
}

static void GameUpdate(void) {
    BrightnessController_Update();
    GFL_WipeMain();
    GFL_NetErrUpdate();
    GFL_SndUpdate();
    PokeVoice_Update();
    GameBeaconSys_Update();
    PassPower_Update();
}

// Sets the sound's output from the options
static void setMonoSnd(const Config *config) {
    if (getSoundFlags(config) == 0) {
        GFL_SndSetStereo(TRUE);
    } else {
        GFL_SndSetStereo(FALSE);
    }
}
