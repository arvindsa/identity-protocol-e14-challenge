/*
 * atca_config.h -- CryptoAuthLib configuration for MAX32630FTHR (LPSDK)
 *
 * Replaces the cmake-generated version.  We only enable what we need:
 *   - I2C HAL
 *   - ATECC508A device support
 *   - No heap, no RTOS, no mbedTLS
 */
#ifndef ATCA_CONFIG_H
#define ATCA_CONFIG_H

/* HAL selection */
#define ATCA_HAL_I2C

/* Device support */
#define ATCA_ATECC508A_SUPPORT

/* Polling (no-poll would use fixed max-execution-time delays instead) */
/* leaving ATCA_NO_POLL undefined -> library uses polling loop */

/*
 * Disable AES-GCM -- ATECC508A has no AES hardware engine and including
 * AES-GCM without also enabling ATCAB_AES_GFM_EN triggers a #error in
 * atca_config_check.h when ATCA_ECC_SUPPORT is active.
 */
#define ATCAB_AES_GCM_EN 0

/* Timer stubs are provided in hal_maxim_i2c.c */
#define atca_delay_ms   hal_delay_ms
#define atca_delay_us   hal_delay_us

/* No dynamic allocation -- the library falls back to static when this is set */
/* #define ATCA_NO_HEAP */

/* POST delay: after a failed wake, wait this long for power-on self-test */
#ifndef ATCA_POST_DELAY_MSEC
#define ATCA_POST_DELAY_MSEC 25
#endif

/* Max command packet size for ATECC508A is 151 bytes */
#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE 151U
#endif

/* Enable param checking */
#define ATCA_CHECK_PARAMS_EN 1

/* Preprocessor warnings off (avoids noise on GCC ARM) */
#define ATCA_PREPROCESSOR_WARNING 0

/* atcacert: not needed for basic bring-up */
#define ATCACERT_COMPCERT_EN        0
#define ATCACERT_FULLSTOREDCERT_EN  0
#define ATCACERT_INTEGRATION_EN     0

/* calib_delete not needed */
#define CALIB_DELETE_EN 0

/* Multipart buffer not needed */
#define MULTIPART_BUF_EN 0

#endif /* ATCA_CONFIG_H */
