################################################################################
# Test — BLE Beacon (BTstack on MAX32630FTHR)
#
# Uses BTstack's bundled MAX32630FTHR port and its CC2564B chipset driver.
# The CC256x init script is pre-generated in:
#   third_party/btstack/chipset/cc256x/bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
#
# To regenerate the init script:
#   cd ../../../third_party/btstack/chipset/cc256x
#   python convert_bts_init_scripts.py \
#       cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts \
#       cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts \
#       bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
################################################################################

PROJECT   = ble_beacon
PORT_NAME = max32630-fthr

TARGET    = MAX3263x
TARGET_UC := MAX3263X
TARGET_LC := max3263x
COMPILER  = GCC
PYTHON    = python

PROJ_CFLAGS += -DRO_FREQ=96000000

# BTstack root (relative to this Makefile)
BTSTACK_ROOT ?= ../../../third_party/btstack
BTSTACK_ROOT := $(abspath $(BTSTACK_ROOT))

PORT_DIR = $(BTSTACK_ROOT)/port/$(PORT_NAME)

# BTstack ships its own copy of the MAXIM SDK — use that for consistency
MAXIM_PATH = $(PORT_DIR)/maxim
LIBS_DIR   = $(MAXIM_PATH)/Firmware/$(TARGET_UC)/Libraries
CMSIS_ROOT = $(LIBS_DIR)/CMSIS

# Machine-specific SDK root — only needed for OpenOCD path (flashing).
# Set in ../../sdk.local.mk (gitignored).
-include ../../sdk.local.mk
ifeq ($(LPSDK_ROOT),)
$(error LPSDK_ROOT is not set. Copy firmware/sdk.local.mk.example to firmware/sdk.local.mk and fill in your LPSDK path, or export LPSDK_ROOT in your shell.)
endif

# ---- Source + include paths ------------------------------------------------

VPATH = . src
IPATH = . src

VPATH += $(PORT_DIR)/src
IPATH += $(PORT_DIR)/src

VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/platform/embedded
VPATH += $(BTSTACK_ROOT)/chipset/cc256x
VPATH += $(BTSTACK_ROOT)/3rd-party/micro-ecc
VPATH += $(BTSTACK_ROOT)/3rd-party/md5
VPATH += $(BTSTACK_ROOT)/src/ble/gatt-service

IPATH += $(BTSTACK_ROOT)/src
IPATH += $(BTSTACK_ROOT)/src/ble
IPATH += $(BTSTACK_ROOT)/src/classic
IPATH += $(BTSTACK_ROOT)/platform/embedded
IPATH += $(BTSTACK_ROOT)/chipset/cc256x
IPATH += $(BTSTACK_ROOT)/3rd-party/micro-ecc
IPATH += $(BTSTACK_ROOT)/3rd-party/md5
IPATH += $(BTSTACK_ROOT)/src/ble/gatt-service
IPATH += $(BTSTACK_ROOT)/3rd-party/hxcmod-player

# ---- Board support ---------------------------------------------------------
ADAPT_BOARD_DIR = $(PORT_DIR)/board/$(PORT_NAME)
BOARD_DIR       = $(LIBS_DIR)/Boards/EvKit_V1
IPATH += $(LIBS_DIR)/Boards/Include
VPATH += $(LIBS_DIR)/Boards/Source
include $(ADAPT_BOARD_DIR)/board.mk

# ---- BTstack source files --------------------------------------------------

SRCS += \
    btstack_audio.c \
    btstack_crypto.c \
    btstack_linked_list.c \
    btstack_memory.c \
    btstack_memory_pool.c \
    btstack_run_loop.c \
    btstack_run_loop_embedded.c \
    btstack_util.c \
    hci.c \
    hci_cmd.c \
    hci_dump.c \
    hci_dump_embedded_stdout.c \
    hci_transport_h4.c

SRCS += \
    ad_parser.c \
    att_dispatch.c \
    btstack_tlv.c \
    btstack_tlv_flash_bank.c \
    le_device_db_tlv.c \
    sm.c

SRCS += \
    btstack_uart_block_embedded.c \
    btstack_run_loop_embedded.c

SRCS += \
    btstack_chipset_cc256x.c \
    bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c

VPATH += $(BTSTACK_ROOT)/chipset/cc256x

SRCS += uECC.c
SRCS += md5.c

SRCS += \
    btstack_port.c \
    hal_tick.c \
    hal_flash_bank_mxc.c \
    main.c

SRCS += ble_beacon.c
SRCS += btstack_link_key_db_stub.c

# ---- Build flags -----------------------------------------------------------
PROJ_CFLAGS  += -DMXC_ASSERT_ENABLE
PROJ_CFLAGS  += -DENABLE_HCI_INIT
PROJ_CFLAGS  += --specs=nano.specs
PROJ_LDFLAGS += --specs=nano.specs

# ---- Peripheral driver + CMSIS linker/startup ------------------------------
PERIPH_DRIVER_DIR = $(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# ---- Flash helper ----------------------------------------------------------
# Flashing is handled by build_and_flash.ps1 which passes the OpenOCD path
# from sdk.local.ps1. You can also flash manually:
#   openocd.exe -c "set LPSDK_TOOLCHAIN /d/.../Toolchain" \
#               -f ../../openocd.cfg \
#               -c "program build/ble_beacon.elf verify reset exit"

bin: $(BUILD_DIR)/$(PROJECT).elf
	arm-none-eabi-objcopy -O binary $< $(BUILD_DIR)/$(PROJECT).bin
	arm-none-eabi-size $<

flash: $(BUILD_DIR)/$(PROJECT).elf
	"$(LPSDK_ROOT)/Toolchain/bin/openocd.exe" \
	    -c "set LPSDK_TOOLCHAIN $(LPSDK_ROOT)/Toolchain" \
	    -f "../../openocd.cfg" \
	    -c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"
