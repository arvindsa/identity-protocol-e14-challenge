################################################################################
# Test — Membrane Keypad Scan (MAX32630FTHR, ID device)
#
# 4×4 matrix on Port 4:
#   Rows (output): P4_0–P4_3
#   Cols (input, pull-up): P4_4–P4_7
#
# Output via DAPLink serial (UART2, P3_1 TX, 115200 baud):
#   INIT             — on startup
#   KEY <row>,<col>  — key pressed (0-based)
#   UP               — key released
#
# LED: Green = idle | Blue = key held
################################################################################

PROJECT  = keypad_scan_test
TARGET   = MAX3263X
COMPILER = GCC
BOARD    = EvKit_V1

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
VPATH = . src
IPATH = . src

PROJ_CFLAGS += -DCONSOLE_UART=1
PROJ_CFLAGS += -DCONSOLE_BAUD=115200
PROJ_CFLAGS += -DMXC_ASSERT_ENABLE

BOARD_DIR = $(LIBS_DIR)/Boards/$(BOARD)
include $(BOARD_DIR)/board.mk

PERIPH_DRIVER_DIR = $(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk
