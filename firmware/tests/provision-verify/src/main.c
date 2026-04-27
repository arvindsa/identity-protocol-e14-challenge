/*
 * Provision + Verify End-to-End Test (MAX32630FTHR)
 * =================================================
 * Bundles the ATECC508A kit-protocol bridge and the autonomous sign/verify
 * round-trip in a single firmware so that a single board can be:
 *
 *   1. Provisioned over serial -- host runs tools/provision_atecc508a.py
 *      against this firmware's UART; the bridge protocol (w/i/z/t:/s:/?)
 *      drives the chip from the PC and locks it.
 *   2. Probed for its public key over serial -- 'k' command reads slot 0's
 *      public key (X || Y, 64 bytes) directly from the chip and prints it.
 *   3. Self-tested end-to-end -- 'd' command generates a 32-byte nonce from
 *      ADC thermal noise (AIN0-AIN3, floating), SHA-256 hashes it, signs the
 *      digest with slot 0 on the ATECC508A, reads the chip's public key, and
 *      verifies the signature with micro-ecc -- no host involvement.
 *
 * Protocol (ASCII lines terminated with \n):
 *   Bridge mode (forwarded to ATECC):
 *     w           wake
 *     i           idle
 *     z           sleep
 *     t:HHHH...   talk -- wake + send + receive + idle
 *     s:HHHH...   sign 32-byte digest via Nonce-passthrough + Sign external
 *     ?           ping (re-emits READY)
 *   Autonomous mode (this test's added commands):
 *     k           read & print public key from slot 0
 *                   reply: PUBKEY: <128 hex chars>           (success)
 *                          PUBKEY: FAIL (err=0xNN)            (failure)
 *     d           run the ADC-nonce + sign + verify demo
 *                   reply: NONCE : <64 hex chars>
 *                          DIGEST: <64 hex chars>
 *                          SIG   : <128 hex chars>
 *                          PUBKEY: <128 hex chars>
 *                          VERIFY: PASS | FAIL
 *     h           print one-line help
 *
 * The bridge half always replies in the kit-protocol envelope (00(...) or FA()).
 * The autonomous half replies in plain ASCII so the output is human-readable
 * over a serial monitor.
 *
 * Hardware:
 *   UART1 MAP_A: P2_0=TX, P2_1=RX, 115200 baud (DAPLink serial)
 *   I2CM1 MAP_A: P3_4=SDA, P3_5=SCL, 100 kHz (ATECC508A, 3.3 V)
 *   I2CM2 MAP_A: P5_7=SDA, P6_0=SCL (PMIC -- LDO3 enable)
 *   ADC: AIN0-AIN3 left floating (thermal-noise entropy source)
 *   LED: green = idle/ready | red blink = ATECC error during talk
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "mxc_errors.h"
#include "gpio.h"
#include "i2cm.h"
#include "ioman.h"
#include "clkman.h"
#include "tmr_utils.h"
#include "uart.h"
#include "adc.h"

/* CryptoAuthLib */
#include "cryptoauthlib.h"
#include "atca_basic.h"
#include "atca_hal.h"
#include "crypto/atca_crypto_sw_sha2.h"

/* micro-ecc */
#include "uECC.h"

/* ---- Board_Init override -------------------------------------------------- */
int Board_Init(void) { return E_NO_ERROR; }

/* ---- RGB LED (active-low, open-drain) ------------------------------------- */
static const gpio_cfg_t led_r = { PORT_2, PIN_4, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };
static const gpio_cfg_t led_g = { PORT_2, PIN_5, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };

static void leds_init(void)
{
    GPIO_Config(&led_r);
    GPIO_Config(&led_g);
    GPIO_OutSet(&led_r);   /* off */
    GPIO_OutSet(&led_g);   /* off */
}

/* ---- PMIC: enable LDO3 (3.3 V for ATECC508A) ------------------------------ */
#define PMIC_ADDR      0x28
#define LDO3_VSET_REG  0x17
#define LDO3_CFG_REG   0x16
#define LDO3_VSET_3V3  0x19
#define LDO3_ENABLED   0x02

static void pmic_ldo3_enable(void)
{
    const sys_cfg_i2cm_t cfg = {
        .clk_scale = CLKMAN_SCALE_DIV_1,
        .io_cfg    = IOMAN_I2CM2(IOMAN_MAP_A, 1),
    };
    if (I2CM_Init(MXC_I2CM2, &cfg, I2CM_SPEED_100KHZ) != E_NO_ERROR) {
        /* LDO3 may already be on from a previous reset -- continue */
    }
    uint8_t buf[2];
    buf[0] = LDO3_VSET_REG; buf[1] = LDO3_VSET_3V3;
    I2CM_Write(MXC_I2CM2, PMIC_ADDR, NULL, 0, buf, 2);
    buf[0] = LDO3_CFG_REG;  buf[1] = LDO3_ENABLED;
    I2CM_Write(MXC_I2CM2, PMIC_ADDR, NULL, 0, buf, 2);
    TMR_Delay(MXC_TMR0, MSEC(5));
}

/* ---- CryptoAuthLib config ------------------------------------------------- */
static ATCAIfaceCfg cfg_atecc508a = {
    .iface_type  = ATCA_I2C_IFACE,
    .devtype     = ATECC508A,
    .atcai2c = {
        .address = 0xC0,   /* 0x60 << 1 */
        .bus     = 0,
        .baud    = 100000,
    },
    .wake_delay  = 1500,
    .rx_retries  = 20,
};

/* ---- UART helpers --------------------------------------------------------- */

static uint8_t uart_getc(void)
{
    uint8_t c;
    int num = 0;
    while (num == 0) {
        UART_Read(MXC_UART1, &c, 1, &num);
    }
    return c;
}

static int uart_readline(char *buf, int max_len)
{
    int n = 0;
    while (n < max_len - 1) {
        uint8_t c = uart_getc();
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[n++] = (char)c;
    }
    buf[n] = '\0';
    return n;
}

/* ---- Hex helpers ---------------------------------------------------------- */

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_decode(const char *hex_str, uint8_t *out, int max_bytes)
{
    int len = (int)strlen(hex_str);
    if (len % 2 != 0) return -1;
    int n = len / 2;
    if (n > max_bytes) return -1;
    for (int i = 0; i < n; i++) {
        int hi = hex_nibble(hex_str[i * 2]);
        int lo = hex_nibble(hex_str[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

static void hex_encode(const uint8_t *data, int len, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        out[i * 2]     = hex[data[i] >> 4];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static void print_hex(const char *label, const uint8_t *data, int len)
{
    printf("%s: ", label);
    for (int i = 0; i < len; i++) printf("%02X", data[i]);
    printf("\n");
}

/* ---- Kit protocol response helpers --------------------------------------- */

static void respond_ok(const uint8_t *data, int len)
{
    char hex[160];
    hex_encode(data, len, hex);
    printf("00(%s)\n", hex);
}

static void respond_ok_empty(void) { printf("00()\n"); }
static void respond_fail(void)     { printf("FA()\n"); }

/* ---- ATECC508A command execution time lookup ----------------------------- */
static uint32_t exec_time_ms(uint8_t opcode)
{
    switch (opcode) {
        case 0x01: return 35;   /* CheckMac */
        case 0x02: return  5;   /* Counter / Read */
        case 0x08: return 35;   /* DeriveKey */
        case 0x0A: return 35;   /* ECDH */
        case 0x0B: return 29;   /* GenDig */
        case 0x11: return 58;   /* HMAC */
        case 0x12: return 26;   /* Write */
        case 0x15: return 35;   /* MAC */
        case 0x16: return  7;   /* Nonce */
        case 0x17: return 32;   /* Lock */
        case 0x18: return 35;   /* PrivWrite */
        case 0x1B: return 23;   /* Random */
        case 0x1C: return  7;   /* Read */
        case 0x1D: return 35;   /* SecureBoot */
        case 0x25: return  2;   /* SelfTest partial */
        case 0x27: return  2;   /* UpdateExtra */
        case 0x2A: return 58;   /* Sign (External) */
        case 0x30: return  2;   /* Info */
        case 0x3C: return 58;   /* Verify */
        case 0x40: return 115;  /* GenKey */
        case 0x41: return 70;   /* Sign (Internal) */
        case 0x47: return 44;   /* SHA */
        case 0x48: return 35;   /* AES */
        case 0x4B: return 50;   /* KDF */
        default:   return 200;  /* unknown: conservative max */
    }
}

/* ---- ATCA CRC-16 --------------------------------------------------------- */
static uint16_t atca_crc16(const uint8_t *data, int len)
{
    uint16_t crc = 0;
    for (int i = 0; i < len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            uint8_t db = (data[i] >> bit) & 1;
            uint8_t cb = (crc >> 15) & 1;
            crc <<= 1;
            if (db ^ cb) crc ^= 0x8005;
        }
    }
    return crc;
}

/* ---- ATCA packet builder ------------------------------------------------- */
static int make_atca_pkt(uint8_t *buf, uint8_t opcode, uint8_t param1,
                          uint16_t param2, const uint8_t *data, int data_len)
{
    buf[0] = 0x03;
    buf[1] = (uint8_t)(7 + data_len);
    buf[2] = opcode;
    buf[3] = param1;
    buf[4] = (uint8_t)(param2 & 0xFF);
    buf[5] = (uint8_t)(param2 >> 8);
    if (data && data_len > 0) {
        memcpy(&buf[6], data, (size_t)data_len);
    }
    uint16_t crc = atca_crc16(&buf[1], 5 + data_len);
    buf[6 + data_len] = (uint8_t)(crc & 0xFF);
    buf[7 + data_len] = (uint8_t)(crc >> 8);
    return 8 + data_len;
}

/* ---- Talk command handler ------------------------------------------------ */
static int atecc_talk(ATCAIface iface,
                      const uint8_t *cmd_bytes, int cmd_len,
                      uint8_t *resp_buf, int *resp_len)
{
    ATCA_STATUS st;
    if (cmd_len < 2) return -1;

    uint8_t word_addr = cmd_bytes[0];
    const uint8_t *payload = &cmd_bytes[1];
    int payload_len = cmd_len - 1;
    uint8_t opcode = (cmd_len >= 3) ? cmd_bytes[2] : 0xFF;

    st = hal_i2c_control(iface, ATCA_HAL_CONTROL_WAKE, NULL, 0);
    if (st != ATCA_SUCCESS) return -1;

    st = hal_i2c_send(iface, word_addr, (uint8_t *)payload, payload_len);
    if (st != ATCA_SUCCESS) {
        hal_i2c_control(iface, ATCA_HAL_CONTROL_IDLE, NULL, 0);
        return -1;
    }

    TMR_Delay(MXC_TMR0, MSEC(exec_time_ms(opcode)));

    uint16_t rxlen = 75;
    st = hal_i2c_receive(iface, 0, resp_buf, &rxlen);
    if (st != ATCA_SUCCESS || rxlen < 4) {
        hal_i2c_control(iface, ATCA_HAL_CONTROL_IDLE, NULL, 0);
        return -1;
    }
    uint8_t count = resp_buf[0];
    if (count < 4 || count > 75 || count > (int)rxlen) {
        hal_i2c_control(iface, ATCA_HAL_CONTROL_IDLE, NULL, 0);
        return -1;
    }
    *resp_len = (int)count;

    hal_i2c_control(iface, ATCA_HAL_CONTROL_IDLE, NULL, 0);
    return 0;
}

/* ---- Nonce from ADC thermal noise ---------------------------------------- */
/*
 * Samples AIN0-AIN3 (floating, no external connection required) 32 times each
 * and mixes the 10-bit readings into a 32-bit seed via rotate-left-3 + XOR.
 * The LSBs carry thermal noise that varies on every reset; upper bits add a
 * stable DC offset that breaks the all-zeros seed edge case.  The seed is
 * then expanded to `len` bytes via xorshift32.
 */
static void nonce_from_adc(uint8_t *out, int len)
{
    static const mxc_adc_chsel_t ch[4] = {
        ADC_CH_0, ADC_CH_1, ADC_CH_2, ADC_CH_3
    };

    ADC_Init();

    uint32_t seed = 0;
    for (int round = 0; round < 32; round++) {
        for (int i = 0; i < 4; i++) {
            uint16_t val = 0;
            ADC_StartConvert(ch[i], 0, 0);
            ADC_GetData(&val);
            seed = ((seed << 3) | (seed >> 29)) ^ (uint32_t)val;
        }
    }

    if (seed == 0) seed = 0xA5A5A5A5u;

    int pos = 0;
    while (pos < len) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        int copy = len - pos < 4 ? len - pos : 4;
        memcpy(out + pos, &seed, (size_t)copy);
        pos += copy;
    }
}

/* ---- Autonomous: read & print public key from slot 0 --------------------- */
static void cmd_pubkey(void)
{
    uint8_t pubkey[64];
    ATCA_STATUS st = atcab_get_pubkey(0, pubkey);
    if (st != ATCA_SUCCESS) {
        printf("PUBKEY: FAIL (err=0x%02X)\n", st);
        return;
    }
    print_hex("PUBKEY", pubkey, sizeof(pubkey));
}

/* ---- Autonomous: ADC nonce -> sign -> verify ----------------------------- */
static void cmd_demo(void)
{
    /* 1. Nonce from ADC thermal noise */
    uint8_t nonce[32];
    nonce_from_adc(nonce, sizeof(nonce));
    print_hex("NONCE ", nonce, sizeof(nonce));

    /* 2. SHA-256(nonce) -> digest */
    uint8_t digest[32];
    ATCA_STATUS st = atcac_sw_sha2_256(nonce, sizeof(nonce), digest);
    if (st != ATCA_SUCCESS) {
        printf("SHA256: FAIL (err=0x%02X)\n", st);
        return;
    }
    print_hex("DIGEST", digest, sizeof(digest));

    /* 3. ATECC508A sign slot 0 */
    uint8_t signature[64];
    st = atcab_sign(0, digest, signature);
    if (st != ATCA_SUCCESS) {
        printf("SIG   : FAIL (err=0x%02X)\n", st);
        return;
    }
    print_hex("SIG   ", signature, sizeof(signature));

    /* 4. Read public key dynamically from chip */
    uint8_t pubkey[64];
    st = atcab_get_pubkey(0, pubkey);
    if (st != ATCA_SUCCESS) {
        printf("PUBKEY: FAIL (err=0x%02X)\n", st);
        return;
    }
    print_hex("PUBKEY", pubkey, sizeof(pubkey));

    /* 5. micro-ecc verify */
    int ok = uECC_verify(pubkey, digest, sizeof(digest), signature,
                         uECC_secp256r1());
    printf("VERIFY: %s\n", ok ? "PASS" : "FAIL");
}

/* ---- Main ---------------------------------------------------------------- */

int main(void)
{
    leds_init();
    GPIO_OutClr(&led_r);   /* red on during init */

    /* UART1 MAP_A -- DAPLink serial, 115200 baud */
    {
        const uart_cfg_t uart_cfg = {
            .parity     = UART_PARITY_DISABLE,
            .size       = UART_DATA_SIZE_8_BITS,
            .extra_stop = 0,
            .cts        = 0,
            .rts        = 0,
            .baud       = 115200,
        };
        const sys_cfg_uart_t uart_sys_cfg = {
            .clk_scale = CLKMAN_SCALE_AUTO,
            .io_cfg    = IOMAN_UART(1, IOMAN_MAP_A, IOMAN_MAP_UNUSED,
                                    IOMAN_MAP_UNUSED, 1, 0, 0),
        };
        UART_Init(MXC_UART1, &uart_cfg, &uart_sys_cfg);
    }

    pmic_ldo3_enable();

    ATCA_STATUS status = atcab_init(&cfg_atecc508a);
    if (status != ATCA_SUCCESS) {
        printf("ERR: atcab_init -> 0x%02X\n", status);
    }

    ATCAIface iface = atGetIFace(atcab_get_device());

    GPIO_OutSet(&led_r);   /* red off */
    GPIO_OutClr(&led_g);   /* green on */

    printf("READY\n");
    printf("# provision-verify -- bridge cmds: w i z t: s: ?  | autonomous: k d h\n");

    char line[320];
    uint8_t cmd_buf[160];
    uint8_t resp_buf[75];

    for (;;) {
        int len = uart_readline(line, sizeof(line));
        if (len == 0) continue;

        if (line[0] == 'w' && line[1] == '\0') {
            ATCA_STATUS st = hal_i2c_control(iface, ATCA_HAL_CONTROL_WAKE, NULL, 0);
            if (st == ATCA_SUCCESS) {
                const uint8_t wake_resp[] = {0x04, 0x11, 0x33, 0x43};
                respond_ok(wake_resp, sizeof(wake_resp));
            } else {
                respond_fail();
            }

        } else if (line[0] == 'i' && line[1] == '\0') {
            hal_i2c_control(iface, ATCA_HAL_CONTROL_IDLE, NULL, 0);
            respond_ok_empty();

        } else if (line[0] == 'z' && line[1] == '\0') {
            hal_i2c_control(iface, ATCA_HAL_CONTROL_SLEEP, NULL, 0);
            respond_ok_empty();

        } else if (line[0] == 't' && line[1] == ':') {
            int nbytes = hex_decode(&line[2], cmd_buf, sizeof(cmd_buf));
            if (nbytes < 4) { respond_fail(); continue; }

            int resp_len = 0;
            int ret = atecc_talk(iface, cmd_buf, nbytes, resp_buf, &resp_len);
            if (ret == 0) {
                respond_ok(resp_buf, resp_len);
            } else {
                respond_fail();
                GPIO_OutClr(&led_r);
                TMR_Delay(MXC_TMR0, MSEC(100));
                GPIO_OutSet(&led_r);
            }

        } else if (line[0] == 's' && line[1] == ':') {
            uint8_t digest[32];
            if (hex_decode(&line[2], digest, 32) != 32) { respond_fail(); continue; }

            int pkt_len = make_atca_pkt(cmd_buf, 0x16, 0x03, 0x0000, digest, 32);
            int resp_len = 0;
            if (atecc_talk(iface, cmd_buf, pkt_len, resp_buf, &resp_len) != 0 ||
                resp_len < 4 || resp_buf[1] != 0x00) {
                respond_fail();
                continue;
            }

            pkt_len = make_atca_pkt(cmd_buf, 0x41, 0x80, 0x0000, NULL, 0);
            resp_len = 0;
            if (atecc_talk(iface, cmd_buf, pkt_len, resp_buf, &resp_len) != 0 ||
                resp_len != 67) {
                respond_fail();
                continue;
            }
            respond_ok(&resp_buf[1], 64);

        } else if (line[0] == '?' && line[1] == '\0') {
            printf("READY\n");

        } else if (line[0] == 'k' && line[1] == '\0') {
            cmd_pubkey();

        } else if (line[0] == 'd' && line[1] == '\0') {
            cmd_demo();

        } else if (line[0] == 'h' && line[1] == '\0') {
            printf("# bridge: w i z t:HEX s:HEX ?   autonomous: k(pubkey) d(demo) h(help)\n");

        } else {
            /* Unknown command -- ignore */
        }
    }
}
