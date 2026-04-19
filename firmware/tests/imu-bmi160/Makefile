################################################################################
# Test -- BMI160 IMU bring-up (MAX32630FTHR)
#
# Tests I2C2 communication with the BMI160 at 0x68.
# Bus: I2CM2, IOMAN_MAP_A, P5_7=SDA, P6_0=SCL (shared with MAX14690N PMIC).
#
# Output via DAPLink serial (UART1 MAP_A, P2_0/P2_1, 115200 baud):
#   - CHIP_ID read (expect 0xD1)
#   - ACC X/Y/Z and GYR X/Y/Z raw counts in a loop
#
# LED: Green = init OK | Red = I2C init or CHIP_ID mismatch
################################################################################

PROJECT  = imu_bmi160_test
TARGET   = MAX3263X
COMPILER = GCC
BOARD    = EvKit_V1

# Machine-specific SDK root -- set in ../../sdk.local.mk (gitignored).
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
