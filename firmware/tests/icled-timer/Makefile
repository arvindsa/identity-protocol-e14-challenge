################################################################################
# Test -- ICLED FeatherWing, TMR4 PWM bit-bang (MAX32630FTHR, door device)
#
# Lights LED 0 green using hardware PWM on TMR4 instead of NOP bit-bang.
# P5_6 (D13) -> TMR4: formula (port*8 + pin) % 6 = (5*8+6) % 6 = 4
#
# TMR4 runs at 96 MHz / 1 = 96 MHz.  Period = 120 ticks = 1.25 us.
# T0H = 38 ticks (~396 ns), T1H = 77 ticks (~802 ns).
#
# On each period rollover (flag), the next bit's duty is written to pwm_cap32
# before the compare event fires.  IRQs are disabled during the frame to keep
# the ~38-tick T0H window safe.
#
# Hardware: wire ICLED DIN to P5_6 (D13 header pad).
#           R9 solder resistor must be open (see CLAUDE.md pin notes).
#
# Output via DAPLink serial (UART1 MAP_A, 115200 baud).
#
# Build:  make
# Flash:  make flash
################################################################################

PROJECT  = icled_timer_test
TARGET   = MAX3263X
COMPILER = GCC

-include ../../sdk.local.mk
ifeq ($(LPSDK_ROOT),)
$(error LPSDK_ROOT not set. Copy firmware/sdk.local.mk.example to firmware/sdk.local.mk)
endif

MAXIM_SDK  = $(LPSDK_ROOT)/Firmware/MAX3263X
LIBS_DIR   = $(MAXIM_SDK)/Libraries
CMSIS_ROOT = $(LIBS_DIR)/CMSIS

TARGET_UC := MAX3263X
TARGET_LC := max3263x

SRCS  = main.c
VPATH = src
IPATH = src

PROJ_CFLAGS += -DCONSOLE_UART=1
PROJ_CFLAGS += -DCONSOLE_BAUD=115200

FTHR_BOARD_DIR    := $(abspath ../../fthr-board)
BTSTACK_FTHR_PORT := $(abspath ../../../third_party/btstack/port/max32630-fthr/board/max32630-fthr)
include $(FTHR_BOARD_DIR)/board.mk

PERIPH_DRIVER_DIR = $(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

flash: $(BUILD_DIR)/$(PROJECT).elf
	"$(LPSDK_ROOT)/Toolchain/bin/openocd.exe" \
	    -c "set LPSDK_TOOLCHAIN $(LPSDK_ROOT)/Toolchain" \
	    -f "../../openocd.cfg" \
	    -c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"
