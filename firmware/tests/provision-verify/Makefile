################################################################################
# Provision + Verify End-to-End Test (MAX32630FTHR)
#
# Combines the ATECC508A kit-protocol bridge and the autonomous sign/verify
# round-trip in a single firmware:
#   - bridge cmds (w/i/z/t:/s:/?) for tools/provision_atecc508a.py
#   - 'k' reads & prints public key (slot 0) directly from the chip
#   - 'd' runs ADC nonce -> SHA256 -> Sign(slot 0) -> read pubkey ->
#         uECC_verify, all on-device
#
# Build:
#   make
# Flash:
#   make flash
# Monitor (DAPLink serial, 115200 baud):
#   python -m serial.tools.miniterm <PORT> 115200
################################################################################

PROJECT  = provision_verify
TARGET   = MAX3263X
COMPILER = GCC
BOARD    = EvKit_V1

# Machine-specific SDK root
-include ../../sdk.local.mk
ifeq ($(LPSDK_ROOT),)
$(error LPSDK_ROOT not set. Copy firmware/sdk.local.mk.example -> firmware/sdk.local.mk)
endif

MAXIM_SDK  = $(LPSDK_ROOT)/Firmware/MAX3263X
TOOL_DIR   = $(LPSDK_ROOT)/Toolchain/bin
LIBS_DIR   = $(MAXIM_SDK)/Libraries
CMSIS_ROOT = $(LIBS_DIR)/CMSIS

TARGET_UC := MAX3263X
TARGET_LC := max3263x

# ---- CryptoAuthLib paths ---------------------------------------------------
CALIB_ROOT   := $(abspath ../../../third_party/cryptoauthlib)
CALIB_LIB    := $(CALIB_ROOT)/lib
CALIB_CALIB  := $(CALIB_LIB)/calib
CALIB_HAL    := $(CALIB_LIB)/hal
CALIB_CRYPTO := $(CALIB_LIB)/crypto

# ---- micro-ecc path --------------------------------------------------------
UECC_ROOT := $(abspath ../../../third_party/micro-ecc)

# ---- Source & include paths ------------------------------------------------
VPATH = . src
IPATH = . src

VPATH += $(CALIB_LIB)
VPATH += $(CALIB_CALIB)
VPATH += $(CALIB_HAL)
VPATH += $(CALIB_CRYPTO)
VPATH += $(CALIB_CRYPTO)/hashes
VPATH += $(UECC_ROOT)

IPATH += $(CALIB_LIB)
IPATH += $(CALIB_CALIB)
IPATH += $(CALIB_HAL)
IPATH += $(CALIB_CRYPTO)
IPATH += $(UECC_ROOT)

# ---- Application source ----------------------------------------------------
SRCS  = main.c
SRCS += hal_maxim_i2c.c

# ---- CryptoAuthLib -- core -------------------------------------------------
SRCS += \
    atca_basic.c       \
    atca_device.c      \
    atca_helpers.c     \
    atca_iface.c       \
    atca_debug.c       \
    atca_utils_sizes.c \
    cal_buffer.c       \
    atca_cfgs.c

SRCS += atca_hal.c

# ---- CryptoAuthLib -- calib (ATECC508A commands) ---------------------------
SRCS += \
    calib_basic.c      \
    calib_command.c    \
    calib_execution.c  \
    calib_helpers.c    \
    calib_info.c       \
    calib_packet.c     \
    calib_read.c       \
    calib_checkmac.c   \
    calib_counter.c    \
    calib_derivekey.c  \
    calib_gendig.c     \
    calib_genkey.c     \
    calib_hmac.c       \
    calib_lock.c       \
    calib_mac.c        \
    calib_nonce.c      \
    calib_privwrite.c  \
    calib_random.c     \
    calib_secureboot.c \
    calib_selftest.c   \
    calib_sha.c        \
    calib_sign.c       \
    calib_updateextra.c \
    calib_verify.c     \
    calib_write.c

# ---- CryptoAuthLib -- software SHA-256 (digest step) -----------------------
SRCS += \
    atca_crypto_sw_sha2.c \
    sha2_routines.c

# ---- micro-ecc (P-256 verification) ----------------------------------------
SRCS += uECC.c

# ---- Build flags -----------------------------------------------------------
PROJ_CFLAGS += -DCONSOLE_UART=1
PROJ_CFLAGS += -DCONSOLE_BAUD=115200
PROJ_CFLAGS += -DMXC_ASSERT_ENABLE
PROJ_CFLAGS += -Wno-unused-parameter -Wno-sign-compare

# micro-ecc: P-256 only, no compressed points
PROJ_CFLAGS += -DuECC_SUPPORTS_secp256r1=1
PROJ_CFLAGS += -DuECC_SUPPORT_COMPRESSED_POINT=0

# ---- Board support ---------------------------------------------------------
BOARD_DIR = $(LIBS_DIR)/Boards/$(BOARD)
IPATH += $(LIBS_DIR)/Boards/Include
VPATH += $(LIBS_DIR)/Boards/Source
include $(BOARD_DIR)/board.mk

# ---- Peripheral driver + CMSIS ---------------------------------------------
PERIPH_DRIVER_DIR = $(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# ---- Targets ---------------------------------------------------------------

flash: $(BUILD_DIR)/$(PROJECT).elf
	"$(LPSDK_ROOT)/Toolchain/bin/openocd.exe" \
	    -c "set LPSDK_TOOLCHAIN $(LPSDK_ROOT)/Toolchain" \
	    -f "../../openocd.cfg" \
	    -c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"
