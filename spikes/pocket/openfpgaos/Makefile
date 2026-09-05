ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
SDK_DIR ?= $(ROOT)/out/research/openfpgaSDK-a408ddc/src/sdk
OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos

CROSS := riscv-none-elf-
CC := $(CROSS)gcc
CXX := $(CROSS)g++
READELF := $(CROSS)readelf
SIZE := $(CROSS)size
NM := $(CROSS)nm
LD := $(CROSS)ld

ARCH := -march=rv32imafc -mabi=ilp32f
CXX_SYSTEM_INCLUDES := $(shell echo | $(CXX) $(ARCH) -x c++ -E -Wp,-v - 2>&1 | sed -n 's,^ \(/.*c++.*\),-isystem \1,p')
GCC_INCLUDE := $(shell $(CXX) $(ARCH) -print-file-name=include)
LIBSTDCXX := $(shell $(CXX) $(ARCH) -print-file-name=libstdc++.a)
LIBSUPCXX := $(shell $(CXX) $(ARCH) -print-file-name=libsupc++.a)
LIBGCC := $(shell $(CXX) $(ARCH) -print-libgcc-file-name)

INCLUDES := \
  -I$(ROOT)/core/contracts/include \
  -I$(ROOT)/core/library/include \
  -I$(ROOT)/core/platform/pocket/include \
  -I$(ROOT)/core/player/include \
  -I$(ROOT)/core/runtime/include \
  -I$(ROOT)/spikes/pocket/common/include \
  -I$(SDK_DIR)/include
SYSTEM_INCLUDES := \
  -nostdinc \
  $(CXX_SYSTEM_INCLUDES) \
  -isystem $(SDK_DIR)/musl/include \
  -isystem $(GCC_INCLUDE)
COMMON_FLAGS := \
  $(ARCH) \
  -O2 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  -ffunction-sections \
  -fdata-sections \
  -fno-tree-loop-distribute-patterns
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 -fno-exceptions -fno-rtti $(INCLUDES) $(SYSTEM_INCLUDES)
CFLAGS := $(COMMON_FLAGS) -std=c11 -I$(SDK_DIR)/musl/include -I$(SDK_DIR)/include
LDFLAGS := \
  $(ARCH) \
  -nostdlib \
  -static \
  -T $(SDK_DIR)/app.ld \
  -L$(SDK_DIR)/musl/lib \
  -Wl,--gc-sections \
  -Wl,--no-warn-rwx-segments \
  -Wl,-Map,$(OUT_DIR)/rpcmp-probe.map
# The pinned SDK's libm.a is an intentionally empty archive written with a
# CRLF marker. xPack ld rejects that placeholder, and this probe uses no libm
# symbols, so link libc directly rather than passing the unusable archive.
LIBS := -Wl,--start-group -lc $(LIBSTDCXX) $(LIBSUPCXX) $(LIBGCC) -Wl,--end-group
CRT := \
  $(SDK_DIR)/musl/lib/crt1.o \
  $(SDK_DIR)/musl/lib/crti.o \
  $(SDK_DIR)/musl/lib/crtn.o

CXX_SOURCES := \
  $(ROOT)/core/contracts/src/validation.cpp \
  $(ROOT)/core/library/src/container.cpp \
  $(ROOT)/core/library/src/logical_library.cpp \
  $(ROOT)/core/runtime/src/mock_core.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_decoder.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_document_machine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_engine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_parser.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_scheduler_bridge.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_semantic.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_timeline.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_track_machine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_ym2151_router.cpp \
  $(ROOT)/core/runtime/src/device_scheduler.cpp \
  $(ROOT)/spikes/pocket/common/src/comparison_probe.cpp \
  $(ROOT)/spikes/pocket/common/src/mdx_hardware_probe.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/main.cpp
C_SOURCES := \
  $(ROOT)/spikes/pocket/openfpgaos/newlib_musl_compat.c \
  $(ROOT)/spikes/pocket/openfpgaos/pocket_sdk_adapter.c
CXX_OBJECTS := $(patsubst $(ROOT)/%.cpp,$(OUT_DIR)/%.o,$(CXX_SOURCES))
C_OBJECTS := $(patsubst $(ROOT)/%.c,$(OUT_DIR)/%.o,$(C_SOURCES))
SDK_INIT_OBJECT := $(OUT_DIR)/sdk/of_init.o
OBJECTS := $(CXX_OBJECTS) $(C_OBJECTS) $(SDK_INIT_OBJECT)
ELF := $(OUT_DIR)/rpcmp-probe.elf

M5_OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos-m5
M5_CXX_SOURCES := \
  $(ROOT)/core/contracts/src/validation.cpp \
  $(ROOT)/core/library/src/container.cpp \
  $(ROOT)/core/library/src/logical_library.cpp \
  $(ROOT)/core/player/src/mdx_library_session.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_decoder.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_document_machine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_engine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_parser.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_semantic.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_timeline.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_track_machine.cpp \
  $(ROOT)/core/runtime/engines/mdx/mdx_ym2151_router.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/m5_session_probe.cpp
M5_CXX_OBJECTS := $(patsubst $(ROOT)/%.cpp,$(M5_OUT_DIR)/%.o,$(M5_CXX_SOURCES))
M5_C_SOURCES := $(ROOT)/spikes/pocket/openfpgaos/pocket_sdk_adapter.c
M5_C_OBJECTS := $(patsubst $(ROOT)/%.c,$(M5_OUT_DIR)/%.o,$(M5_C_SOURCES))
M5_SDK_INIT_OBJECT := $(M5_OUT_DIR)/sdk/of_init.o
M5_LIBRARY := $(M5_OUT_DIR)/m5-library.rpcmlib
M5_LIBRARY_OBJECT := $(M5_OUT_DIR)/m5-library.o
M5_ELF := $(M5_OUT_DIR)/rpcmp-m5-session.elf
M5_BUDGET := $(M5_OUT_DIR)/budget.json

M5_AUDIO_OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos-m5-audio
M5_AUDIO_CXX_SOURCES := \
  $(filter-out $(ROOT)/spikes/pocket/openfpgaos/m5_session_probe.cpp,$(M5_CXX_SOURCES)) \
  $(ROOT)/core/platform/pocket/src/device_queue_mmio.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/m5_audio_probe.cpp
M5_AUDIO_CXX_OBJECTS := $(patsubst $(ROOT)/%.cpp,$(M5_AUDIO_OUT_DIR)/%.o,$(M5_AUDIO_CXX_SOURCES))
M5_AUDIO_C_OBJECTS := $(patsubst $(ROOT)/%.c,$(M5_AUDIO_OUT_DIR)/%.o,$(M5_C_SOURCES))
M5_AUDIO_SDK_INIT_OBJECT := $(M5_AUDIO_OUT_DIR)/sdk/of_init.o
M5_AUDIO_LIBRARY := $(M5_AUDIO_OUT_DIR)/m5-library.rpcmlib
M5_AUDIO_LIBRARY_OBJECT := $(M5_AUDIO_OUT_DIR)/m5-library.o
M5_AUDIO_ELF := $(M5_AUDIO_OUT_DIR)/rpcmp-m5-audio.elf
M5_AUDIO_BUDGET := $(M5_AUDIO_OUT_DIR)/budget.json

M5_FILE_OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos-m5-file
M5_FILE_CXX_SOURCES := \
  $(filter-out $(ROOT)/spikes/pocket/openfpgaos/m5_audio_probe.cpp,$(M5_AUDIO_CXX_SOURCES)) \
  $(ROOT)/spikes/pocket/common/src/m5_library_loader.cpp \
  $(ROOT)/spikes/pocket/common/src/m5_playback.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/m5_file_probe.cpp
M5_FILE_CXX_OBJECTS := $(patsubst $(ROOT)/%.cpp,$(M5_FILE_OUT_DIR)/%.o,$(M5_FILE_CXX_SOURCES))
M5_FILE_C_OBJECTS := $(patsubst $(ROOT)/%.c,$(M5_FILE_OUT_DIR)/%.o,$(M5_C_SOURCES))
M5_FILE_SDK_INIT_OBJECT := $(M5_FILE_OUT_DIR)/sdk/of_init.o
M5_FILE_ELF := $(M5_FILE_OUT_DIR)/rpcmp-m5-file.elf
M5_FILE_BUDGET := $(M5_FILE_OUT_DIR)/budget.json

$(M5_FILE_OUT_DIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fstack-usage -c -o $@ $<

$(M5_FILE_OUT_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fstack-usage -c -o $@ $<

$(M5_FILE_SDK_INIT_OBJECT): $(SDK_DIR)/of_init.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(M5_FILE_ELF): $(M5_FILE_CXX_OBJECTS) $(M5_FILE_C_OBJECTS) $(M5_FILE_SDK_INIT_OBJECT) $(CRT) $(SDK_DIR)/app.ld
	$(CXX) $(ARCH) -nostdlib -static -T $(SDK_DIR)/app.ld -L$(SDK_DIR)/musl/lib \
	  -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map,$(M5_FILE_OUT_DIR)/rpcmp-m5-file.map \
	  -o $@ $(CRT) $(M5_FILE_CXX_OBJECTS) $(M5_FILE_C_OBJECTS) $(M5_FILE_SDK_INIT_OBJECT) $(LIBS)

$(M5_FILE_BUDGET): $(M5_FILE_ELF) $(ROOT)/tools/elf_budget.py
	python3 $(ROOT)/tools/elf_budget.py --elf $(M5_FILE_ELF) --size-tool $(SIZE) \
	  --stack-root $(M5_FILE_OUT_DIR) --static-limit 56623104 --data-limit 4096 \
	  --stack-limit 524288 --output $@

m5-file-budget: $(M5_FILE_BUDGET)
	$(SIZE) $(M5_FILE_ELF)
	$(READELF) -h $(M5_FILE_ELF) | grep -F 'Machine:' | grep -F 'RISC-V'
	test -z "$$($(NM) --undefined-only $(M5_FILE_ELF))"
	sha256sum $(M5_FILE_ELF) $(M5_FILE_BUDGET)

.PHONY: m5-file-budget

.DEFAULT_GOAL := verify

$(OUT_DIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(M5_OUT_DIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fstack-usage -c -o $@ $<

$(M5_OUT_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fstack-usage -c -o $@ $<

$(M5_AUDIO_OUT_DIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fstack-usage -c -o $@ $<

$(M5_AUDIO_OUT_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fstack-usage -c -o $@ $<

$(M5_LIBRARY): $(ROOT)/tests/fixtures/rpcmlib/mdx-session.rpcmlib.hex
	@mkdir -p $(dir $@)
	python3 -c "import pathlib; p=pathlib.Path('$<'); pathlib.Path('$@').write_bytes(bytes.fromhex(p.read_text()))"

$(M5_LIBRARY_OBJECT): $(M5_LIBRARY)
	cd $(M5_OUT_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)

$(M5_SDK_INIT_OBJECT): $(SDK_DIR)/of_init.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(M5_ELF): $(M5_CXX_OBJECTS) $(M5_C_OBJECTS) $(M5_SDK_INIT_OBJECT) $(M5_LIBRARY_OBJECT) $(CRT) $(SDK_DIR)/app.ld
	$(CXX) $(ARCH) -nostdlib -static -T $(SDK_DIR)/app.ld -L$(SDK_DIR)/musl/lib \
	  -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map,$(M5_OUT_DIR)/rpcmp-m5-session.map \
	  -o $@ $(CRT) $(M5_CXX_OBJECTS) $(M5_C_OBJECTS) $(M5_SDK_INIT_OBJECT) $(M5_LIBRARY_OBJECT) $(LIBS)

$(M5_BUDGET): $(M5_ELF) $(ROOT)/tools/elf_budget.py
	python3 $(ROOT)/tools/elf_budget.py --elf $(M5_ELF) --size-tool $(SIZE) \
	  --stack-root $(M5_OUT_DIR) --static-limit 56623104 --data-limit 4096 \
	  --stack-limit 524288 --output $@

$(M5_AUDIO_LIBRARY): $(ROOT)/tests/fixtures/rpcmlib/mdx-session.rpcmlib.hex
	@mkdir -p $(dir $@)
	python3 -c "import pathlib; p=pathlib.Path('$<'); pathlib.Path('$@').write_bytes(bytes.fromhex(p.read_text()))"

$(M5_AUDIO_LIBRARY_OBJECT): $(M5_AUDIO_LIBRARY)
	cd $(M5_AUDIO_OUT_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)

$(M5_AUDIO_SDK_INIT_OBJECT): $(SDK_DIR)/of_init.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(M5_AUDIO_ELF): $(M5_AUDIO_CXX_OBJECTS) $(M5_AUDIO_C_OBJECTS) $(M5_AUDIO_SDK_INIT_OBJECT) $(M5_AUDIO_LIBRARY_OBJECT) $(CRT) $(SDK_DIR)/app.ld
	$(CXX) $(ARCH) -nostdlib -static -T $(SDK_DIR)/app.ld -L$(SDK_DIR)/musl/lib \
	  -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map,$(M5_AUDIO_OUT_DIR)/rpcmp-m5-audio.map \
	  -o $@ $(CRT) $(M5_AUDIO_CXX_OBJECTS) $(M5_AUDIO_C_OBJECTS) $(M5_AUDIO_SDK_INIT_OBJECT) $(M5_AUDIO_LIBRARY_OBJECT) $(LIBS)

$(M5_AUDIO_BUDGET): $(M5_AUDIO_ELF) $(ROOT)/tools/elf_budget.py
	python3 $(ROOT)/tools/elf_budget.py --elf $(M5_AUDIO_ELF) --size-tool $(SIZE) \
	  --stack-root $(M5_AUDIO_OUT_DIR) --static-limit 56623104 --data-limit 4096 \
	  --stack-limit 524288 --output $@

$(SDK_INIT_OBJECT): $(SDK_DIR)/of_init.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ELF): $(OBJECTS) $(CRT) $(SDK_DIR)/app.ld
	$(CXX) $(LDFLAGS) -o $@ $(CRT) $(OBJECTS) $(LIBS)

verify: $(ELF)
	$(SIZE) $(ELF)
	$(READELF) -h $(ELF) | grep -F 'Class:' | grep -F 'ELF32'
	$(READELF) -h $(ELF) | grep -F 'Machine:' | grep -F 'RISC-V'
	$(READELF) -h $(ELF) | grep -F 'RVC, single-float ABI'
	test -z "$$($(NM) --undefined-only $(ELF))"
	sha256sum $(ELF)

m5-budget: $(M5_BUDGET)
	$(SIZE) $(M5_ELF)
	$(READELF) -h $(M5_ELF) | grep -F 'Machine:' | grep -F 'RISC-V'
	test -z "$$($(NM) --undefined-only $(M5_ELF))"
	sha256sum $(M5_ELF) $(M5_BUDGET)

m5-audio-budget: $(M5_AUDIO_BUDGET)
	$(SIZE) $(M5_AUDIO_ELF)
	$(READELF) -h $(M5_AUDIO_ELF) | grep -F 'Machine:' | grep -F 'RISC-V'
	test -z "$$($(NM) --undefined-only $(M5_AUDIO_ELF))"
	sha256sum $(M5_AUDIO_ELF) $(M5_AUDIO_BUDGET)

clean:
	@case "$(abspath $(OUT_DIR))" in \
	  "$(ROOT)/out/build/"*) rm -rf -- "$(abspath $(OUT_DIR))" ;; \
	  *) echo "refusing to clean outside $(ROOT)/out/build: $(abspath $(OUT_DIR))" >&2; exit 1 ;; \
	esac

.PHONY: verify m5-budget m5-audio-budget clean
