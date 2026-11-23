# New 3DS shell for Black 2 and White 2.
# Uses devkitARM's compiler, libctru and 3dsx.specs.
# A stock Ubuntu arm-none-eabi-gcc needs -march=armv6k+fp; this probes for that.
# PBW2_LOCAL_SYSBASE=1 links devkitPro's libsysbase plus compat/syscall_shim.c
# (only for a newlib that is not devkitARM's).

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
	$(ARCH) -D__3DS__ -I$(CURDIR)/include -I$(CURDIR)/source -I$(CTRULIB)/include $(PBW2_EXTRA_INC)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections
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

SOURCES := source/main.c source/log.c source/workers.c source/audio.c source/save_shim.c \
	source/settings.c source/present.c source/input.c source/io_thread.c $(SHIM)

.PHONY: all b2 w2 headers game-objects check clean

all: check headers game-objects b2 w2

check:
	python3 $(CURDIR)/../scripts/check_delinks.py

headers:
	python3 $(CURDIR)/../scripts/n3ds_headers.py

game-objects: headers
	python3 $(CURDIR)/../scripts/n3ds_game_objects.py

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

$(BUILD)/$(TARGET).elf: $(SOURCES) include/pbw2_port.h include/pbw2_build_status.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(GAMEFLAGS) $(SOURCES) $(LDFLAGS) $(LIBPATHS) $(LIBS) -Wl,-Map,$(BUILD)/$(TARGET).map -o $@

clean:
	rm -rf build
