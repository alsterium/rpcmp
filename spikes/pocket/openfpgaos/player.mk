# Complete M6 application. Invoke in the pinned Linux toolchain image:
# make -f spikes/pocket/openfpgaos/player.mk player-check
include $(dir $(lastword $(MAKEFILE_LIST)))Makefile
PLAYER_OUT ?= $(ROOT)/out/build/pocket-m6-player
HOST_CC ?= gcc
HOST_CXX ?= g++
PLAYER_GENERATED := $(PLAYER_OUT)/generated
PLAYER_CXX_SOURCES := \
  $(filter-out $(ROOT)/spikes/pocket/openfpgaos/m5_session_probe.cpp,$(M5_CXX_SOURCES)) \
  $(filter-out $(ROOT)/core/contracts/src/validation.cpp,$(wildcard $(ROOT)/core/contracts/src/*.cpp)) \
  $(filter-out $(ROOT)/core/player/src/mdx_library_session.cpp,$(wildcard $(ROOT)/core/player/src/*.cpp)) \
  $(ROOT)/core/platform/pocket/src/device_queue_mmio.cpp \
  $(ROOT)/core/platform/pocket/src/sound_mmio_client.cpp \
  $(ROOT)/core/platform/pocket/src/mdx_backend.cpp \
  $(ROOT)/core/platform/pocket/src/bitmap_font.cpp \
  $(ROOT)/core/platform/pocket/src/bitmap_canvas.cpp \
  $(ROOT)/core/platform/pocket/src/player_application.cpp \
  $(ROOT)/core/ui/src/player_input.cpp \
  $(ROOT)/core/ui/src/player_ui.cpp \
  $(ROOT)/core/ui/src/player_render.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/player_main.cpp
PLAYER_C_SOURCES := \
  $(ROOT)/spikes/pocket/openfpgaos/pocket_sdk_adapter.c \
  $(ROOT)/spikes/pocket/openfpgaos/player_sdk_adapter.c \
  $(ROOT)/spikes/pocket/openfpgaos/newlib_musl_compat.c
PLAYER_OBJECTS := $(patsubst $(ROOT)/%.cpp,$(PLAYER_OUT)/%.o,$(PLAYER_CXX_SOURCES)) \
  $(patsubst $(ROOT)/%.c,$(PLAYER_OUT)/%.o,$(PLAYER_C_SOURCES)) $(PLAYER_OUT)/of_init.o
PLAYER_ELF := $(PLAYER_OUT)/rpcmp-player.elf

$(PLAYER_GENERATED)/cp932_table.inc: $(ROOT)/tools/generate_cp932.py $(ROOT)/third_party/cp932/CP932.TXT
	@mkdir -p $(dir $@)
	python3 -B $< --root $(ROOT) --output $@

$(PLAYER_GENERATED)/utf8proc.o: $(ROOT)/third_party/utf8proc/utf8proc.c $(ROOT)/third_party/utf8proc/utf8proc.h $(ROOT)/third_party/utf8proc/utf8proc_data.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -O2 -DUTF8PROC_STATIC -I$(ROOT)/third_party/utf8proc -c -o $@ $<

$(PLAYER_GENERATED)/font-points: $(ROOT)/utility/src/font_points_main.cpp $(ROOT)/utility/src/metadata.cpp $(ROOT)/utility/include/rpcmp/utility/metadata.hpp $(PLAYER_GENERATED)/cp932_table.inc $(PLAYER_GENERATED)/utf8proc.o
	$(HOST_CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DUTF8PROC_STATIC \
	  -I$(ROOT)/third_party/utf8proc -I$(ROOT)/utility/include -I$(PLAYER_GENERATED) \
	  $(filter %.cpp %.o,$^) -o $@

$(PLAYER_GENERATED)/bitmap_font.inc: $(PLAYER_GENERATED)/font-points $(ROOT)/tools/bitmap_font_generate.py $(ROOT)/third_party/unifont/unifont_jp-16.0.04.hex.gz $(ROOT)/third_party/cp932/CP932.TXT
	python3 -B $(ROOT)/tools/bitmap_font_generate.py --repo $(ROOT) --normalizer $< --output $@

$(PLAYER_OUT)/core/platform/pocket/src/bitmap_font.o: $(PLAYER_GENERATED)/bitmap_font.inc
$(PLAYER_OUT)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/core/ui/include -I$(PLAYER_GENERATED) -fstack-usage -MMD -MP -c -o $@ $<
$(PLAYER_OUT)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fstack-usage -MMD -MP -c -o $@ $<
$(PLAYER_OUT)/of_init.o: $(SDK_DIR)/of_init.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fstack-usage -MMD -MP -c -o $@ $<
-include $(PLAYER_OBJECTS:.o=.d)

$(PLAYER_ELF): $(PLAYER_OBJECTS) $(CRT) $(SDK_DIR)/app.ld $(ROOT)/spikes/pocket/openfpgaos/player.mk
	$(CXX) $(ARCH) -nostdlib -static -T $(SDK_DIR)/app.ld -L$(SDK_DIR)/musl/lib \
	  -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map,$(PLAYER_OUT)/rpcmp-player.map \
	  -o $@ $(CRT) $(PLAYER_OBJECTS) $(LIBS)

.PHONY: player-check
player-check: $(PLAYER_ELF)
	$(SIZE) $<
	$(READELF) -h $< | grep -F 'Machine:' | grep -F 'RISC-V'
	test -z "$$($(NM) --undefined-only $<)"
	python3 $(ROOT)/tools/elf_budget.py --elf $< --size-tool $(SIZE) --stack-root $(PLAYER_OUT) \
	  --static-limit 56623104 --data-limit 4096 --stack-limit 524288 --output $(PLAYER_OUT)/budget.json
	sha256sum $< $(PLAYER_OUT)/budget.json
