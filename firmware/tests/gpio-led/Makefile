################################################################################
# Test — GPIO / RGB LED (MAX32630FTHR)
#
# Cycles the on-board RGB LED through Red → Green → Blue, 500 ms each.
# Active-low open-drain outputs on P2_4 (R), P2_5 (G), P2_6 (B).
#
# Pass criterion: LED visibly cycles R→G→B in a ~1.5 s period.
################################################################################

PROJECT  = gpio_led_test
TARGET   = MAX3263X
COMPILER = GCC
BOARD    = EvKit_V1

# Machine-specific SDK root — set in ../../sdk.local.mk (gitignored).
# Copy ../../sdk.local.mk.example to ../../sdk.local.mk and fill in your path.
-include ../../sdk.local.mk
ifeq ($(LPSDK_ROOT),)
$(error LPSDK_ROOT is not set. Copy firmware/sdk.local.mk.example to firmware/sdk.local.mk and fill in your LPSDK path, or export LPSDK_ROOT in your shell.)
endif

MAXIM_SDK = $(LPSDK_ROOT)/Firmware/MAX3263X
TOOL_DIR  = $(LPSDK_ROOT)/Toolchain/bin

LIBS_DIR   = $(MAXIM_SDK)/Libraries
CMSIS_ROOT = $(LIBS_DIR)/CMSIS

TARGET_UC := MAX3263X
TARGET_LC := max3263x

SRCS  = main.c
VPATH = src
IPATH = src

BOARD_DIR = $(LIBS_DIR)/Boards/$(BOARD)
include $(BOARD_DIR)/board.mk

PERIPH_DRIVER_DIR = $(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk
