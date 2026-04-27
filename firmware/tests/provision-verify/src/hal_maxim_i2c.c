/*
 * hal_maxim_i2c.c -- CryptoAuthLib I2C HAL for MAX32630FTHR (LPSDK)
 *
 * Maps CryptoAuthLib's hal_i2c_* interface onto the LPSDK I2CM driver.
 *
 * Bus:   I2CM1 MAP_A -- P3_4 = SDA, P3_5 = SCL  (dedicated to ATECC508A, 3.3 V pull-ups)
 * Speed: 100 kHz
 *
 * The ATECC508A uses a custom wake/sleep/idle protocol:
 *   Wake  -- hold SDA low for >=60 us (send 0x00 at very low baud), then wait
 *   Idle  -- send word-address byte 0x02 with no payload
 *   Sleep -- send word-address byte 0x01 with no payload
 *
 * hal_i2c_control() handles WAKE/IDLE/SLEEP via ATCA_HAL_CONTROL_WAKE/IDLE/SLEEP.
 *
 * hal_i2c_send() / hal_i2c_receive() handle normal command/response traffic.
 * The ATECC508A protocol: [word_address(1)] [count(1)] [opcode(1)] [...] [crc(2)]
 * CryptoAuthLib passes the full packet starting from count; we prepend word_address.
 */

#include <string.h>
#include <stdlib.h>
#include "cryptoauthlib.h"
#include "atca_hal.h"

/* LPSDK peripheral headers */
#include "i2cm.h"
#include "ioman.h"
#include "clkman.h"
#include "tmr_utils.h"

/* ---- Bus configuration --------------------------------------------------- */
#define ATCA_I2C_BUS     MXC_I2CM1
#define ATCA_I2C_SPEED   I2CM_SPEED_100KHZ

/* Word-address bytes for ATECC508A protocol */
#define WORD_ADDR_RESET  0x00   /* wake token sent as address-only frame */
#define WORD_ADDR_SLEEP  0x01
#define WORD_ADDR_IDLE   0x02
#define WORD_ADDR_CMD    0x03   /* normal command */

/* Wake timing (ATECC508A datasheet Table 6-2) */
#define WAKE_LOW_US      60     /* SDA must be low for at least 60 us        */
#define WAKE_DELAY_MS    2      /* tWHI: wait 1.5 ms after SDA release; 2 ms safe */

/* ---- Static HAL state ---------------------------------------------------- */
static bool s_i2c_initialized = false;

/* ---- Heap abstraction (required by CryptoAuthLib) ------------------------ */
void *hal_malloc(size_t size) { return malloc(size); }
void  hal_free(void *ptr)     { free(ptr); }

/* ---- Timer helpers (required by CryptoAuthLib) ---------------------------- */
void hal_delay_ms(uint32_t ms)
{
    TMR_Delay(MXC_TMR0, MSEC(ms));
}

void hal_delay_us(uint32_t us)
{
    TMR_Delay(MXC_TMR0, USEC(us));
}

/* ---- I2C init/release ---------------------------------------------------- */

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg)
{
    (void)iface;
    (void)cfg;

    if (s_i2c_initialized) {
        return ATCA_SUCCESS;
    }

    const sys_cfg_i2cm_t sys_cfg = {
        .clk_scale = CLKMAN_SCALE_DIV_1,
        .io_cfg    = IOMAN_I2CM1(IOMAN_MAP_A, 1),   /* P3_4=SDA, P3_5=SCL */
    };

    int ret = I2CM_Init(ATCA_I2C_BUS, &sys_cfg, ATCA_I2C_SPEED);
    if (ret != E_NO_ERROR) {
        return ATCA_COMM_FAIL;
    }

    s_i2c_initialized = true;
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_post_init(ATCAIface iface)
{
    (void)iface;
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_release(void *hal_data)
{
    (void)hal_data;
    /* I2CM2 is shared with BMI160/PMIC; don't actually shut it down. */
    s_i2c_initialized = false;
    return ATCA_SUCCESS;
}

/* ---- Send (command to device) -------------------------------------------- */

ATCA_STATUS hal_i2c_send(ATCAIface iface, uint8_t word_address,
                          uint8_t *txdata, int txlength)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    uint8_t addr = (uint8_t)(ATCA_IFACECFG_VALUE(cfg, atcai2c.address) >> 1);

    /*
     * Build the frame: [word_address | payload].
     * For commands txdata points to [count, opcode, ...crc] and txlength covers
     * all of it.  For idle/sleep txdata is NULL and txlength is 0.
     */
    uint8_t frame[160];   /* MAX_PACKET_SIZE + 1 */
    frame[0] = word_address;
    if (txdata != NULL && txlength > 0) {
        memcpy(&frame[1], txdata, (size_t)txlength);
    }
    int frame_len = 1 + txlength;

    int ret = I2CM_Write(ATCA_I2C_BUS, addr, NULL, 0, frame, frame_len);
    return (ret == frame_len) ? ATCA_SUCCESS : ATCA_COMM_FAIL;
}

/* ---- Receive (response from device) -------------------------------------- */

ATCA_STATUS hal_i2c_receive(ATCAIface iface, uint8_t word_address,
                             uint8_t *rxdata, uint16_t *rxlength)
{
    (void)word_address;   /* response reads don't re-send a word address */

    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    uint8_t addr = (uint8_t)(ATCA_IFACECFG_VALUE(cfg, atcai2c.address) >> 1);

    if (rxdata == NULL || rxlength == NULL || *rxlength == 0) {
        return ATCA_BAD_PARAM;
    }

    int ret = I2CM_Read(ATCA_I2C_BUS, addr, NULL, 0, rxdata, *rxlength);
    if (ret < 0) {
        return ATCA_COMM_FAIL;
    }
    *rxlength = (uint16_t)ret;
    return ATCA_SUCCESS;
}

/* ---- Control (wake / idle / sleep) --------------------------------------- */

ATCA_STATUS hal_i2c_control(ATCAIface iface, uint8_t option,
                             void *param, size_t paramlen)
{
    (void)param;
    (void)paramlen;

    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    uint8_t addr = (uint8_t)(ATCA_IFACECFG_VALUE(cfg, atcai2c.address) >> 1);

    switch (option) {

    case ATCA_HAL_CONTROL_WAKE: {
        /*
         * ATECC508A wake sequence:
         *   1. Drive SDA low for >=60 us.  The easiest way without bit-banging
         *      is to attempt an I2C write to address 0x00 (no ACK expected) --
         *      the START + address byte keeps SDA low for the required time at
         *      100 kHz (each bit ~10 us, 10 bits ~ 100 us > 60 us).
         *   2. Wait tWHI >=1.5 ms for the device to complete its wake-up.
         *   3. Read the 4-byte wake response (expect 0x04 0x11 0x33 0x43).
         */
        uint8_t zero = 0x00;
        /* Send to address 0x00 -- NACK is expected, ignore error */
        I2CM_Write(ATCA_I2C_BUS, 0x00, NULL, 0, &zero, 1);
        hal_delay_ms(WAKE_DELAY_MS);

        /* Verify wake response */
        uint8_t wake_resp[4] = {0};
        uint16_t resp_len = sizeof(wake_resp);
        int ret = I2CM_Read(ATCA_I2C_BUS, addr, NULL, 0,
                            wake_resp, (int)resp_len);
        if (ret != 4) {
            return ATCA_WAKE_FAILED;
        }
        /* Expected wake response: {0x04, 0x11, 0x33, 0x43} */
        const uint8_t expected[4] = {0x04, 0x11, 0x33, 0x43};
        if (memcmp(wake_resp, expected, 4) != 0) {
            return ATCA_WAKE_FAILED;
        }
        return ATCA_SUCCESS;
    }

    case ATCA_HAL_CONTROL_IDLE: {
        uint8_t idle = WORD_ADDR_IDLE;
        int ret = I2CM_Write(ATCA_I2C_BUS, addr, NULL, 0, &idle, 1);
        return (ret == 1) ? ATCA_SUCCESS : ATCA_COMM_FAIL;
    }

    case ATCA_HAL_CONTROL_SLEEP: {
        uint8_t sleep = WORD_ADDR_SLEEP;
        int ret = I2CM_Write(ATCA_I2C_BUS, addr, NULL, 0, &sleep, 1);
        return (ret == 1) ? ATCA_SUCCESS : ATCA_COMM_FAIL;
    }

    default:
        return ATCA_UNIMPLEMENTED;
    }
}
