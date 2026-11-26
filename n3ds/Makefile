# New 3DS shell for Black 2 and White 2.
# Uses devkitARM's compiler, libctru and 3dsx.specs.
# PBW2_LOCAL_SYSBASE=1 links devkitPro's libsysbase plus compat/syscall_shim.c
# PBW2_LINK_GAME=1 links compiled pokebw2 objects as Pbw2_GameMain (shell keeps main).

.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO, for example /opt/devkitpro)
endif
ifeq ($(strip $(DEVKITARM)),)
DEVKITARM := $(DEVKITPRO)/devkitARM
endif

ifneq ($(wildcard $(DEVKITARM)/bin/arm-none-eabi-gcc),)
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
else
CC := arm-none-eabi-gcc
endif

CTRULIB ?= $(DEVKITPRO)/libctru
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
ARCH_OK := $(shell echo 'int x;' | $(CC) $(ARCH) -c -x c - -o /dev/null 2>/dev/null && echo yes)
ifneq ($(ARCH_OK),yes)
ARCH := -march=armv6k+fp -mtune=mpcore -mfloat-abi=hard -mtp=soft
endif

CFLAGS := -g -O2 -Wall -Wno-unused-function -mword-relocations -ffunction-sections -fdata-sections \
	$(ARCH) -D__3DS__ -DPBW2_N3DS \
	-I$(CURDIR)/../build/n3ds -I$(CURDIR)/include -I$(CURDIR)/source -I$(CURDIR)/nitro \
	-iquote$(CURDIR)/../include -I$(CURDIR)/../lib/nitro/include -I$(CURDIR)/../lib/twl/include \
	-I$(CTRULIB)/include $(PBW2_EXTRA_INC)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections
ifeq ($(PBW2_LINK_GAME),1)
endif
LIBPATHS := -L$(CTRULIB)/lib
# PBW2_NO_SPEEDUP=1 skips osSetSpeedupEnable: emulator-only test builds.
# Azahar/Lotus abort on the guest's ConfigureNew3DSCPU request, so the
# shipped build keeps speedup on and this flag is only for emu testing.
ifeq ($(PBW2_NO_SPEEDUP),1)
CFLAGS += -DPBW2_NO_SPEEDUP=1
endif
ifeq ($(PBW2_LOCAL_SYSBASE),1)
CFLAGS += -I$(CURDIR)/compat
LIBPATHS += -L$(PBW2_SYSBASE)
LIBS := -Wl,--start-group -lctru -lsysbase -lc -lm -Wl,--end-group
SHIM := compat/syscall_shim.c
else
LIBS := -lctru -lm
SHIM :=
endif

SHELL_SRC := source/main.c source/log.c source/workers.c source/audio.c source/save_shim.c \
	source/settings.c source/present.c source/input.c source/io_thread.c source/game_present.c \
	nitro/os_host.c nitro/rtc_host.c nitro/fs_host.c nitro/gx_host.c nitro/host.c \
	nitro/title_pic.c $(SHIM)

ifeq ($(PBW2_LINK_GAME),1)
GAME_SYM := $(shell echo $(GAME) | tr A-Z a-z)
LIB_GAME_G2D := \
	g2d_Load.o g2d_NCG_load.o g2d_NCL_load.o g2d_NSC_load.o g2d_NAN_load.o g2d_NCE_load.o g2d_NMC_load.o \
	g2d_Animation.o g2d_CellAnimation.o g2d_CellTransferManager.o g2d_Image.o g2d_MultiCellAnimation.o \
	g2d_Node.o g2d_OAM.o g2d_OamSoftwareSpriteDraw.o g2d_Renderer.o g2d_RendererCore.o g2d_SRTControl.o \
	g2d_PaletteTable.o g2di_Mtx32.o g2di_VRamLocation.o
LIB_GAME_GFD := \
	gfd_VramTransferManager.o gfd_FramePlttVramMan.o gfd_FrameTexVramMan.o gfd_LinkedListPlttVramMan.o \
	gfd_LinkedListTexVramMan.o gfd_PlttVramMan.o gfd_TexVramMan.o gfdi_LinkedListVramMan_Common.o
LIB_GAME_G3D := \
	res_struct_accessor.o res_struct_accessor_anm.o kernel.o mem.o anm.o glbstate.o util.o \
	nsbca.o nsbma.o nsbta.o nsbtp.o nsbva.o sbc.o cgtool.o basic.o maya.o si3d.o 3dsmax.o xsi.o
LIB_GAME_SPL := spl_anim.o spl_behavior.o spl_draw.o spl_emit.o spl_emitter.o spl_list.o spl_manager.o spl_random.o
LIB_GAME_OBJS := \
	../build/n3ds/$(GAME_SYM)/obj/lib/nitro/src/gx/gx_load2d.o \
	$(foreach size,22 33 43 44,../build/n3ds/$(GAME_SYM)/obj/lib/nitro/src/fx/fx_mtx$(size).o) \
	$(foreach o,$(LIB_GAME_G2D),../build/n3ds/$(GAME_SYM)/obj/lib/nnsys/src/g2d/$(o)) \
	$(foreach o,$(LIB_GAME_GFD),../build/n3ds/$(GAME_SYM)/obj/lib/nnsys/src/gfd/$(o)) \
	$(foreach o,$(LIB_GAME_G3D),../build/n3ds/$(GAME_SYM)/obj/lib/nnsys/src/g3d/$(o)) \
	$(foreach o,$(LIB_GAME_SPL),../build/n3ds/$(GAME_SYM)/obj/lib/spl/src/$(o))
CFLAGS += -DPBW2_LINK_GAME=1
CFLAGS += $(shell for d in $(CURDIR)/../lib/*/include; do echo -I$$d; done)
SHELL_SRC += source/game_thread.c nitro/mi_host.c nitro/ic_dc_host.c \
	nitro/os_more_host.c nitro/os_thread_host.c nitro/rom_header_host.c nitro/pm_host.c nitro/pxi_host.c \
	nitro/pxi_service.c nitro/cp_div_host.c nitro/nvram_host.c nitro/dtcm_host.c nitro/link_syms.c nitro/overlay_host.c \
	nitro/overlay_ids.c nitro/overlay_id_decode_gen.c nitro/nitro_stubs.c nitro/nns_title_stubs.c nitro/title_link_syms.c \
	nitro/g3_gx_stub.c nitro/fx_host.c nitro/link_stubs.c nitro/proc_stubs.c nitro/net_stubs.c \
	nitro/snd_silent.c nitro/strm_pcm.c nitro/snd_arc.c nitro/wave_out.c nitro/seq_player.c nitro/mic_stub.c nitro/mb_stub.c nitro/wm_stub.c nitro/twl_stub.c nitro/pad_host.c nitro/vram_host.c nitro/vram_dma.c nitro/affine_track.c nitro/ds2d_compose.c \
	nitro/g3_intercept.c nitro/g3_gecom_host.c nitro/g3_raster.c
GAME_RSP := $(CURDIR)/../build/n3ds/$(shell echo $(GAME) | tr A-Z a-z)/game_objects.rsp
GAME_LDFLAGS := @$(GAME_RSP)
ifneq ($(PBW2_GPU_GEOMETRY),0)
CFLAGS += -DPBW2_GPU_GEOMETRY
SHELL_SRC += nitro/g3_pica.c
GPU_SHADER := $(BUILD)/g3_pass.o
LIBS := -lcitro3d $(LIBS)
.PHONY: test-g3
test-g3: $(BUILD)/g3_fidelity.3dsx

G3_TEST_SRC := tests/g3_fidelity.c nitro/g3_pica.c nitro/g3_intercept.c nitro/g3_raster.c nitro/ds2d_compose.c \
	nitro/vram_host.c nitro/vram_dma.c nitro/affine_track.c nitro/cp_div_host.c nitro/fx_host.c
G3_TEST_MTX := $(foreach size,43 44,../build/n3ds/$(GAME_SYM)/obj/lib/nitro/src/fx/fx_mtx$(size).o)
$(BUILD)/g3_fidelity.elf: $(G3_TEST_SRC) $(G3_TEST_MTX) $(GPU_SHADER)
	$(CC) $(CFLAGS) $(GAMEFLAGS) $^ $(LDFLAGS) -Wl,--wrap=PbW2_PicaFinish $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/g3_fidelity.3dsx: $(BUILD)/g3_fidelity.elf
	3dsxtool $< $@
endif
else
SHELL_SRC += ../lib/nitro/src/rtc/rtc_convert.c
GAME_LDFLAGS :=
endif

.PHONY: all b2 w2 headers game-objects link-rsp check clean
.PHONY: home-menu-art
BT ?= bannertool
home-menu-art:
	@case "$(GAME)" in \
		BLACK2) game="Black 2"; directory=black2 ;; \
		WHITE2) game="White 2"; directory=white2 ;; \
		*) echo "Set GAME=BLACK2 or GAME=WHITE2" >&2; exit 1 ;; \
	esac; \
	python3 ../scripts/n3ds_home_menu_art.py "$(ROM)" "build/home-menu/$$directory" --game "$$game" --bannertool "$(BT)"

.PHONY: test-input
test-input: $(BUILD)/input_capture.3dsx

INPUT_TEST_SRC := tests/input_capture.c source/input.c source/settings.c
INPUT_TEST_WRAP := -Wl,--wrap=hidScanInput,--wrap=hidKeysDown,--wrap=hidKeysHeld,--wrap=hidKeysUp,--wrap=hidCircleRead
$(BUILD)/input_capture.elf: $(INPUT_TEST_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -UPBW2_LINK_GAME -UPBW2_GPU_GEOMETRY $^ $(LDFLAGS) $(INPUT_TEST_WRAP) $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/input_capture.3dsx: $(BUILD)/input_capture.elf
	3dsxtool $< $@

.PHONY: test-present
test-present: $(BUILD)/present_blit.3dsx

$(BUILD)/present_blit.elf: tests/present_blit.c source/present.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -UPBW2_LINK_GAME $^ $(LDFLAGS) -Wl,--wrap=gfxGetFramebuffer $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/present_blit.3dsx: $(BUILD)/present_blit.elf
	3dsxtool $< $@

.PHONY: test-rtc
test-rtc: $(BUILD)/rtc_cache.3dsx

RTC_TEST_SRC := tests/rtc_cache.c nitro/rtc_host.c nitro/mi_host.c
RTC_TEST_OBJS := ../build/n3ds/black2/obj/lib/nitro/src/rtc/rtc_convert.o \
	../build/n3ds/black2/obj/src/gfl/rtc_cache.o
$(BUILD)/rtc_cache.elf: $(RTC_TEST_SRC) $(RTC_TEST_OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -Wl,--wrap=osGetTime $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/rtc_cache.3dsx: $(BUILD)/rtc_cache.elf
	3dsxtool $< $@

.PHONY: test-wave
test-wave: $(BUILD)/wave_out.3dsx

WAVE_TEST_SRC := tests/wave_out.c nitro/wave_out.c
WAVE_TEST_WRAP := -Wl,--wrap=ndspChnWaveBufAdd,--wrap=ndspChnWaveBufClear,--wrap=ndspChnSetFormat \
	-Wl,--wrap=ndspChnSetInterp,--wrap=ndspChnSetRate,--wrap=ndspChnSetMix,--wrap=ndspChnGetSamplePos,--wrap=DSP_FlushDataCache
$(BUILD)/wave_out.elf: $(WAVE_TEST_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPBW2_LINK_GAME -I$(CURDIR)/../lib/nnsys/include $^ $(LDFLAGS) $(WAVE_TEST_WRAP) $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/wave_out.3dsx: $(BUILD)/wave_out.elf
	3dsxtool $< $@

all: check headers game-objects b2 w2

check:
	python3 $(CURDIR)/../scripts/check_delinks.py

headers:
	python3 $(CURDIR)/../scripts/n3ds_headers.py

game-objects: headers
	python3 $(CURDIR)/../scripts/n3ds_overlay_id_decode.py
	python3 $(CURDIR)/../scripts/n3ds_game_objects.py

link-rsp: game-objects
	python3 $(CURDIR)/../scripts/n3ds_link_closure.py

b2: game-objects
	$(MAKE) --no-print-directory one GAME=BLACK2 TARGET=black2_3ds BUILD=build/b2

w2: game-objects
	$(MAKE) --no-print-directory one GAME=WHITE2 TARGET=white2_3ds BUILD=build/w2

ifeq ($(GAME),BLACK2)
GAMEFLAGS := -DPBW2_BLACK2 -DBLACK2
else ifeq ($(GAME),WHITE2)
GAMEFLAGS := -DPBW2_WHITE2 -DWHITE2
endif

one: $(BUILD)/$(TARGET).3dsx

$(BUILD)/$(TARGET).3dsx: $(BUILD)/$(TARGET).elf
	3dsxtool $< $@
	python3 $(CURDIR)/../scripts/n3ds_check_3dsx_reloc.py $<
	@echo built $@

$(BUILD)/$(TARGET).elf: $(SHELL_SRC) $(GPU_SHADER) include/pbw2_port.h ../build/n3ds/pbw2_build_status.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(GAMEFLAGS) $(SHELL_SRC) $(GPU_SHADER) $(LDFLAGS) $(GAME_LDFLAGS) $(LIB_GAME_OBJS) $(LIBPATHS) $(LIBS) \
		-Wl,-Map,$(BUILD)/$(TARGET).map -o $@

$(BUILD)/g3_pass.o: shaders/g3_pass.v.pica
	@mkdir -p $(BUILD)
	picasso -o $(BUILD)/g3_pass.shbin $<
	cd $(BUILD) && arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents g3_pass.shbin g3_pass.o

clean:
	rm -rf build
