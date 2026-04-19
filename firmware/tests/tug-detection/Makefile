################################################################################
# Test -- Tug Detection
#
# State machine:
#   LOCKED   -- LED red.  IMU sampled at 50 Hz.
#               Press switch (P2_3) to simulate PIN entry -> UNLOCKED.
#   UNLOCKED -- LED green. IMU sampled at 50 Hz.
#               Jerk > JERK_THRESHOLD -> LOCKED, LED red.
#
# Adjust JERK_THRESHOLD in src/main.c (tuned value: 21000 LSB, ~1.28 g delta).
#
# LED: Red = LOCKED | Green = UNLOCKED | Both (brief) = state transition
# UART: 115200 baud via DAPLink (P2_0/P2_1, UART1 MAP_A)
################################################################################

PROJECT  = tug_detection_test
TARGET   = MAX3263X
COMPILER = GCC
BOARD    = EvKit_V1

-include ../../sdk.local.mk
ifeq ($(LPSDK_ROOT),)
$(error LPSDK_ROOT is not set. Copy firmware/sdk.local.mk.example to firmware/sdk.local.mk.)
endif

MAXIM_SDK = $(LPSDK_ROOT)/Firmware/MAX3263X
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

flash: $(BUILD_DIR)/$(PROJECT).elf
	"$(LPSDK_ROOT)/Toolchain/bin/openocd.exe" \
	    -c "set LPSDK_TOOLCHAIN $(LPSDK_ROOT)/Toolchain" \
	    -f "../../openocd.cfg" \
	    -c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"
