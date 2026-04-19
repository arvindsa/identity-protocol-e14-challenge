/*
 * Unit Test -- BMI160 IMU bring-up
 * Target: MAX32630FTHR
 *
 * Validates I2C2 communication with the BMI160 (addr 0x68) on the shared
 * I2C bus (P5_7=SDA, P6_0=SCL, I2CM2/MAP_A).
 *
 * Test sequence:
 *   1. Init I2CM2 (MAP_A, 100 kHz) -- shared with MAX14690N PMIC (0x28)
 *   2. Read BMI160 CHIP_ID register (0x00), expect 0xD1
 *   3. Power up accelerometer: write CMD 0x11 to reg 0x7E, wait 5 ms
 *   4. Power up gyroscope:     write CMD 0x15 to reg 0x7E, wait 80 ms
 *   5. Loop: read ACC X/Y/Z + GYR X/Y/Z, print via UART1 (DAPLink serial)
 *
 * LED feedback (active-low, open-drain):
 *   Red on during init
 *   Green = init passed, data printing
 *   Red   = init failed (I2C error or wrong CHIP_ID)
 *
 * Open DAPLink serial port at 115200 baud to watch output.
 */

#include <stdio.h>
#include "mxc_errors.h"
#include "gpio.h"
#include "i2cm.h"
#include "ioman.h"
#include "clkman.h"
#include "tmr_utils.h"
#include "uart.h"

/* ---- Board_Init override --------------------------------------------------
 * The EvKit_V1 Board_Init() tries to bring up the MAX14690N via I2CM0, which
 * hangs on the FTHR (wrong bus/pins). Return immediately and do our own init.
 */
int Board_Init(void)
{
    return E_NO_ERROR;
}

/* ---- RGB LED (active-low, open-drain) ------------------------------------- */
#define LED_R_PORT  PORT_2
#define LED_R_PIN   PIN_4
#define LED_G_PORT  PORT_2
#define LED_G_PIN   PIN_5

static const gpio_cfg_t led_r = { LED_R_PORT, LED_R_PIN, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };
static const gpio_cfg_t led_g = { LED_G_PORT, LED_G_PIN, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };

static void leds_init(void)
{
    GPIO_Config(&led_r);
    GPIO_Config(&led_g);
    GPIO_OutSet(&led_r);   /* off */
    GPIO_OutSet(&led_g);   /* off */
}

/* ---- I2C2 (shared bus: BMI160 + MAX14690N PMIC) --------------------------- */
#define I2C_BUS         MXC_I2CM2
#define I2C_SPEED       I2CM_SPEED_100KHZ

static int i2c_init(void)
{
    const sys_cfg_i2cm_t cfg = {
        .clk_scale = CLKMAN_SCALE_DIV_1,
        .io_cfg    = IOMAN_I2CM2(IOMAN_MAP_A, 1),   /* P5_7=SDA, P6_0=SCL */
    };
    return I2CM_Init(I2C_BUS, &cfg, I2C_SPEED);
}

/* ---- BMI160 register map -------------------------------------------------- */
#define BMI160_ADDR         0x68

#define BMI160_REG_CHIP_ID  0x00    /* Expected: 0xD1 */
#define BMI160_REG_PMU_STATUS 0x03
#define BMI160_REG_GYR_X_L  0x0C
#define BMI160_REG_ACC_X_L  0x12
#define BMI160_REG_CMD      0x7E

#define BMI160_CMD_ACC_NORMAL  0x11  /* Set accel PMU to normal mode */
#define BMI160_CMD_GYR_NORMAL  0x15  /* Set gyro  PMU to normal mode */

#define BMI160_CHIP_ID_VALUE 0xD1

/* Write one byte to a BMI160 register. Returns E_NO_ERROR on success. */
static int bmi160_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    int ret = I2CM_Write(I2C_BUS, BMI160_ADDR, NULL, 0, buf, 2);
    return (ret == 2) ? E_NO_ERROR : E_COMM_ERR;
}

/* Read one or more bytes starting at reg. Returns E_NO_ERROR on success. */
static int bmi160_read_regs(uint8_t reg, uint8_t *data, int len)
{
    int ret = I2CM_Read(I2C_BUS, BMI160_ADDR, &reg, 1, data, len);
    return (ret == len) ? E_NO_ERROR : E_COMM_ERR;
}

/* Initialise the BMI160: verify CHIP_ID, wake accel and gyro. */
static int bmi160_init(void)
{
    uint8_t chip_id = 0;

    if (bmi160_read_regs(BMI160_REG_CHIP_ID, &chip_id, 1) != E_NO_ERROR) {
        printf("BMI160: CHIP_ID read failed (I2C error)\n");
        return E_COMM_ERR;
    }

    if (chip_id != BMI160_CHIP_ID_VALUE) {
        printf("BMI160: unexpected CHIP_ID 0x%02X (expected 0x%02X)\n",
               chip_id, BMI160_CHIP_ID_VALUE);
        return E_BAD_PARAM;
    }

    printf("BMI160: CHIP_ID OK (0x%02X)\n", chip_id);

    /* Wake accelerometer -- ~5 ms startup time */
    if (bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_ACC_NORMAL) != E_NO_ERROR) {
        printf("BMI160: failed to wake accelerometer\n");
        return E_COMM_ERR;
    }
    TMR_Delay(MXC_TMR0, MSEC(5));

    /* Wake gyroscope -- ~80 ms startup time (PLL lock) */
    if (bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_GYR_NORMAL) != E_NO_ERROR) {
        printf("BMI160: failed to wake gyroscope\n");
        return E_COMM_ERR;
    }
    TMR_Delay(MXC_TMR0, MSEC(80));

    /* Verify PMU status: accel=0x01 normal, gyro=0x04 normal (bits [1:0] and [3:2]) */
    uint8_t pmu = 0;
    bmi160_read_regs(BMI160_REG_PMU_STATUS, &pmu, 1);
    printf("BMI160: PMU_STATUS=0x%02X (acc=%d gyro=%d)\n",
           pmu, pmu & 0x03, (pmu >> 2) & 0x03);

    return E_NO_ERROR;
}

/* Read 6-axis data. Raw signed 16-bit values at +/-2g / 125 dps defaults. */
static void bmi160_read_data(int16_t *ax, int16_t *ay, int16_t *az,
                              int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[12];

    /* Gyro registers come first (0x0C-0x11), then accel (0x12-0x17) */
    if (bmi160_read_regs(BMI160_REG_GYR_X_L, buf, 12) != E_NO_ERROR) {
        *gx = *gy = *gz = *ax = *ay = *az = 0;
        return;
    }

    *gx = (int16_t)(buf[0]  | (buf[1]  << 8));
    *gy = (int16_t)(buf[2]  | (buf[3]  << 8));
    *gz = (int16_t)(buf[4]  | (buf[5]  << 8));
    *ax = (int16_t)(buf[6]  | (buf[7]  << 8));
    *ay = (int16_t)(buf[8]  | (buf[9]  << 8));
    *az = (int16_t)(buf[10] | (buf[11] << 8));
}

/* ---- Entry point ---------------------------------------------------------- */

int main(void)
{
    leds_init();
    GPIO_OutClr(&led_r);   /* red on during init */

    /* Init UART1 MAP_A for printf (DAPLink serial, P2_0/P2_1, 115200 baud).
     * CONSOLE_UART=1 in Makefile routes _write() here.
     * Must use CLKMAN_SCALE_AUTO -- EvKit_V1 board.c hardcodes SCALE_DIV_4. */
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
            .io_cfg    = IOMAN_UART(1, IOMAN_MAP_A, IOMAN_MAP_UNUSED, IOMAN_MAP_UNUSED, 1, 0, 0),
        };
        UART_Init(MXC_UART1, &uart_cfg, &uart_sys_cfg);
    }

    printf("\n--- BMI160 IMU Unit Test ---\n");
    printf("I2C bus: I2CM2 MAP_A (P5_7=SDA P6_0=SCL)\n");
    printf("BMI160 address: 0x%02X\n\n", BMI160_ADDR);

    /* Init I2C2 */
    if (i2c_init() != E_NO_ERROR) {
        printf("FAIL: I2CM2 init error\n");
        GPIO_OutClr(&led_r);
        while (1) {}
    }

    /* Init BMI160 */
    if (bmi160_init() != E_NO_ERROR) {
        printf("FAIL: BMI160 init\n");
        GPIO_OutClr(&led_r);
        while (1) {}
    }

    /* Init passed */
    GPIO_OutSet(&led_r);   /* red off  */
    GPIO_OutClr(&led_g);   /* green on */
    printf("\nPASS: BMI160 init OK -- streaming data (Ctrl+C to stop)\n");
    printf("%-6s %-6s %-6s   %-6s %-6s %-6s\n",
           "ACC_X", "ACC_Y", "ACC_Z", "GYR_X", "GYR_Y", "GYR_Z");

    int16_t ax, ay, az, gx, gy, gz;
    while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);
        printf("%6d %6d %6d   %6d %6d %6d\n",
               ax, ay, az, gx, gy, gz);
        TMR_Delay(MXC_TMR0, MSEC(100));
    }
}
