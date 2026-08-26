ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
SDK_DIR ?= $(ROOT)/out/research/openfpgaSDK-a408ddc/src/sdk
OUT_DIR ?= $(ROOT)/out/build/pocket-openfpgaos-desktop

SRCS_CXX := \
  $(ROOT)/core/contracts/src/validation.cpp \
  $(ROOT)/core/runtime/src/mock_core.cpp \
  $(ROOT)/spikes/pocket/common/src/comparison_probe.cpp \
  $(ROOT)/spikes/pocket/openfpgaos/desktop_main.cpp
CFLAGS := \
  -std=c++17 \
  -fno-exceptions \
  -fno-rtti \
  -I$(ROOT)/core/contracts/include \
  -I$(ROOT)/core/runtime/include \
  -I$(ROOT)/spikes/pocket/common/include

include $(SDK_DIR)/sdk.mk

.DEFAULT_GOAL := verify

verify: app_pc
	@mkdir -p "$(OUT_DIR)/data"
	OF_DATA_DIR="$(OUT_DIR)/data" ./app_pc

prepare-desktop:
	@mkdir -p "$(OUT_DIR)/data"

clean-desktop:
	@case "$(abspath $(OUT_DIR))" in \
	  "$(ROOT)/out/build/"*) rm -rf -- "$(abspath $(OUT_DIR))" ;; \
	  *) echo "refusing to clean outside $(ROOT)/out/build: $(abspath $(OUT_DIR))" >&2; exit 1 ;; \
	esac

.PHONY: verify prepare-desktop clean-desktop
