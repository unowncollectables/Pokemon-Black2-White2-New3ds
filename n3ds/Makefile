# New 3DS shell for Black 2 and White 2.
# Uses devkitARM's compiler, libctru and 3dsx.specs.
# PBW2_LOCAL_SYSBASE=1 links devkitPro's libsysbase plus compat/syscall_shim.c
# PBW2_LINK_GAME=1 links compiled pokebw2 objects as Pbw2_GameMain (shell keeps main).

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
CFLAGS += -DPBW2_LINK_GAME=1
CFLAGS += $(shell for d in $(CURDIR)/../lib/*/include; do echo -I$$d; done)
SHELL_SRC += source/game_thread.c nitro/mi_host.c nitro/ic_dc_host.c \
	nitro/os_more_host.c nitro/os_thread_host.c nitro/rom_header_host.c nitro/pm_host.c nitro/pxi_host.c \
	nitro/pxi_service.c nitro/cp_div_host.c nitro/nvram_host.c nitro/dtcm_host.c nitro/link_syms.c nitro/overlay_host.c \
	nitro/overlay_ids.c nitro/overlay_id_decode_gen.c nitro/nitro_stubs.c nitro/nns_title_stubs.c nitro/title_link_syms.c \
	nitro/g3_gx_stub.c nitro/g3d_glb_stubs.c nitro/mtx_stub.c nitro/link_stubs.c nitro/proc_stubs.c nitro/net_stubs.c \
	nitro/twl_stub.c
GAME_RSP := $(CURDIR)/../build/n3ds/$(shell echo $(GAME) | tr A-Z a-z)/game_objects.rsp
GAME_LDFLAGS := @$(GAME_RSP)
else
GAME_LDFLAGS :=
endif

.PHONY: all b2 w2 headers game-objects link-rsp check clean

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
	@echo built $@

$(BUILD)/$(TARGET).elf: $(SHELL_SRC) include/pbw2_port.h ../build/n3ds/pbw2_build_status.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(GAMEFLAGS) $(SHELL_SRC) $(LDFLAGS) $(GAME_LDFLAGS) $(LIBPATHS) $(LIBS) \
		-Wl,-Map,$(BUILD)/$(TARGET).map -o $@

clean:
	rm -rf build