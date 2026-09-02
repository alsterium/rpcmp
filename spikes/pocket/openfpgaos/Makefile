ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
SDK_DIR ?= $(ROOT)/out/research/openfpgaSDK-a408ddc/src/sdk
OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos

CROSS := riscv-none-elf-
CC := $(CROSS)gcc
CXX := $(CROSS)g++
READELF := $(CROSS)readelf
SIZE := $(CROSS)size
NM := $(CROSS)nm

ARCH := -march=rv32imafc -mabi=ilp32f
CXX_SYSTEM_INCLUDES := $(shell echo | $(CXX) $(ARCH) -x c++ -E -Wp,-v - 2>&1 | sed -n 's,^ \(/.*c++.*\),-isystem \1,p')
GCC_INCLUDE := $(shell $(CXX) $(ARCH) -print-file-name=include)
LIBSTDCXX := $(shell $(CXX) $(ARCH) -print-file-name=libstdc++.a)
LIBSUPCXX := $(shell $(CXX) $(ARCH) -print-file-name=libsupc++.a)
LIBGCC := $(shell $(CXX) $(ARCH) -print-libgcc-file-name)

INCLUDES := \
  -I$(ROOT)/core/contracts/include \
  -I$(ROOT)/core/library/include \
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

.DEFAULT_GOAL := verify

$(OUT_DIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

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

clean:
	@case "$(abspath $(OUT_DIR))" in \
	  "$(ROOT)/out/build/"*) rm -rf -- "$(abspath $(OUT_DIR))" ;; \
	  *) echo "refusing to clean outside $(ROOT)/out/build: $(abspath $(OUT_DIR))" >&2; exit 1 ;; \
	esac

.PHONY: verify clean
