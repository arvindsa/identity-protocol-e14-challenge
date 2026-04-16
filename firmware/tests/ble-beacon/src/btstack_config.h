/*
 * btstack_config.h — BLE-only config for the beacon project.
 * Classic BT, A2DP, SBC, SCO are all disabled to keep code size down.
 */
#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#include <stdint.h>

/* Port features */
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_INIT_SCRIPT          /* CC256x init script present */
#define HAVE_BTSTACK_STDIN

/* BLE only */
#define ENABLE_BLE
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS

/* Logging */
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP

/* Buffer sizes */
#define HCI_ACL_PAYLOAD_SIZE            256
#define MAX_NR_HCI_CONNECTIONS          1
#define MAX_NR_L2CAP_CHANNELS           2
#define MAX_NR_L2CAP_SERVICES           2
#define MAX_NR_GATT_CLIENTS             1
#define MAX_NR_SM_LOOKUP_ENTRIES        2
#define MAX_NR_WHITELIST_ENTRIES        1
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES  2

/* TLV / NVM */
#define NVM_NUM_DEVICE_DB_ENTRIES       4
#define NVM_NUM_LINK_KEYS               4

#endif /* BTSTACK_CONFIG_H */
