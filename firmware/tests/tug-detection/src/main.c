/*
 * Unit Test -- Tug Detection
 * Target: MAX32630FTHR (ID device)
 *
 * State machine:
 *   LOCKED   -- LED red.  BMI160 sampled at 50 Hz.
 *               Press switch (P2_3, active-low) to simulate PIN entry -> UNLOCKED.
 *   UNLOCKED -- LED green. BMI160 sampled at 50 Hz.
 *               Jerk (accel magnitude delta > JERK_THRESHOLD) -> LOCKED.
 *
 * Jerk threshold: adjust JERK_THRESHOLD so walking (~0.3 g peak delta) does
 * not trigger, but a sharp tug (~1 g delta) does.  Tuned value: 21000 LSB
 * (~1.28 g at +/-2 g default range, 1 g = 16384 LSB).
 *
 * LED colours (active-low, open-drain):
 *   Red   = LOCKED
 *   Green = UNLOCKED
 *   Both  = transitioning (brief flash on state change)
 *
 * UART output at 115200 baud shows raw accel delta and state transitions.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mxc_errors.h"
#include "gpio.h"
#include "i2cm.h"
#include "ioman.h"
#include "clkman.h"
#include "tmr_utils.h"
#include "uart.h"

/* ---- Board_Init override ------------------------------------------------- */
int Board_Init(void) { return E_NO_ERROR; }

/* ---- Jerk threshold ------------------------------------------------------ */
/* Raw accel delta (magnitude difference between successive 20 ms samples).
 * +/-2 g default range: 1 g = 16384 LSB.
 * Walking generates ~5000-8000 LSB peak delta; a hard tug ~15000+ LSB.      */
#define JERK_THRESHOLD  21000

/* ---- Switch (active-low, internal pull-up) -------------------------------- */
#define SW_PORT  PORT_2
#define SW_PIN   PIN_3

static const gpio_cfg_t sw_cfg = { SW_PORT, SW_PIN, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP };

static int switch_pressed(void)
{
    return GPIO_InGet(&sw_cfg) == 0;
}

/* ---- RGB LED (active-low, open-drain) ------------------------------------ */
#define LED_R_PORT  PORT_2
#define LED_R_PIN   PIN_4
#define LED_G_PORT  PORT_2
#define LED_G_PIN   PIN_5

static const gpio_cfg_t led_r = { LED_R_PORT, LED_R_PIN, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };
static const gpio_cfg_t led_g = { LED_G_PORT, LED_G_PIN, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };

static void led_red(void)   { GPIO_OutClr(&led_r); GPIO_OutSet(&led_g); }
static void led_green(void) { GPIO_OutSet(&led_r); GPIO_OutClr(&led_g); }
static void led_both(void)  { GPIO_OutClr(&led_r); GPIO_OutClr(&led_g); }
static void led_off(void)   { GPIO_OutSet(&led_r); GPIO_OutSet(&led_g); }

static void leds_init(void)
{
    GPIO_Config(&led_r);
    GPIO_Config(&led_g);
    led_off();
}

/* ---- I2C2 (shared bus: BMI160 @ 0x68) ------------------------------------ */
#define I2C_BUS    MXC_I2CM2
#define I2C_SPEED  I2CM_SPEED_100KHZ

static int i2c_init(void)
{
    const sys_cfg_i2cm_t cfg = {
        .clk_scale = CLKMAN_SCALE_DIV_1,
        .io_cfg    = IOMAN_I2CM2(IOMAN_MAP_A, 1),
    };
    return I2CM_Init(I2C_BUS, &cfg, I2C_SPEED);
}

/* ---- BMI160 register map ------------------------------------------------- */
#define BMI160_ADDR             0x68
#define BMI160_REG_CHIP_ID      0x00
#define BMI160_REG_PMU_STATUS   0x03
#define BMI160_REG_GYR_X_L     0x0C
#define BMI160_REG_ACC_X_L     0x12
#define BMI160_REG_CMD          0x7E
#define BMI160_CMD_ACC_NORMAL   0x11
#define BMI160_CMD_GYR_NORMAL   0x15
#define BMI160_CHIP_ID_VALUE    0xD1

static int bmi160_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return (I2CM_Write(I2C_BUS, BMI160_ADDR, NULL, 0, buf, 2) == 2) ? E_NO_ERROR : E_COMM_ERR;
}

static int bmi160_read_regs(uint8_t reg, uint8_t *data, int len)
{
    return (I2CM_Read(I2C_BUS, BMI160_ADDR, &reg, 1, data, len) == len) ? E_NO_ERROR : E_COMM_ERR;
}

static int bmi160_init(void)
{
    uint8_t id = 0;
    if (bmi160_read_regs(BMI160_REG_CHIP_ID, &id, 1) != E_NO_ERROR || id != BMI160_CHIP_ID_VALUE) {
        printf("BMI160: init failed (id=0x%02X)\n", id);
        return E_COMM_ERR;
    }
    bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_ACC_NORMAL); TMR_Delay(MXC_TMR0, MSEC(5));
    bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_GYR_NORMAL); TMR_Delay(MXC_TMR0, MSEC(80));
    printf("BMI160: init OK (id=0x%02X)\n", id);
    return E_NO_ERROR;
}

/* Read accel X/Y/Z (raw 16-bit signed, +/-2 g range). */
static void bmi160_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];
    if (bmi160_read_regs(BMI160_REG_ACC_X_L, buf, 6) != E_NO_ERROR) {
        *ax = *ay = *az = 0;
        return;
    }
    *ax = (int16_t)(buf[0] | (buf[1] << 8));
    *ay = (int16_t)(buf[2] | (buf[3] << 8));
    *az = (int16_t)(buf[4] | (buf[5] << 8));
}

/* Magnitude approximation: max(|x|,|y|,|z|) + 0.5*mid -- avoids sqrt.
 * Sufficient for delta comparison; absolute calibration not needed here.    */
static int32_t accel_magnitude(int16_t ax, int16_t ay, int16_t az)
{
    int32_t x = ax < 0 ? -ax : ax;
    int32_t y = ay < 0 ? -ay : ay;
    int32_t z = az < 0 ? -az : az;
    /* Sort descending */
    if (y > x) { int32_t t = x; x = y; y = t; }
    if (z > x) { int32_t t = x; x = z; z = t; }
    if (z > y) { int32_t t = y; y = z; z = t; }
    /* Approximation: max + 0.5*mid */
    return x + (y >> 1);
}

/* ---- State machine -------------------------------------------------------- */
typedef enum { STATE_LOCKED, STATE_UNLOCKED } device_state_t;

/* Simple debounce: returns 1 on a fresh press (low edge). */
static int sw_just_pressed(void)
{
    static int prev = 1;
    int cur = switch_pressed();
    int pressed = (prev == 1) && (cur == 0);
    prev = cur;
    return pressed;
}

/* ---- Entry point ---------------------------------------------------------- */
int main(void)
{
    leds_init();
    led_both();   /* both on during init */

    /* UART1 MAP_A -- DAPLink serial, 115200 baud */
    {
        const uart_cfg_t uart_cfg = {
            .parity = UART_PARITY_DISABLE, .size = UART_DATA_SIZE_8_BITS,
            .extra_stop = 0, .cts = 0, .rts = 0, .baud = 115200,
        };
        const sys_cfg_uart_t uart_sys = {
            .clk_scale = CLKMAN_SCALE_AUTO,
            .io_cfg = IOMAN_UART(1, IOMAN_MAP_A, IOMAN_MAP_UNUSED, IOMAN_MAP_UNUSED, 1, 0, 0),
        };
        UART_Init(MXC_UART1, &uart_cfg, &uart_sys);
    }

    printf("\n--- Tug Detection Unit Test ---\n");
    printf("Jerk threshold: %d LSB (~%.2f g at +/-2g range)\n",
           JERK_THRESHOLD, (float)JERK_THRESHOLD / 16384.0f);
    printf("Switch P2_3: press to simulate PIN unlock\n\n");

    GPIO_Config(&sw_cfg);

    if (i2c_init() != E_NO_ERROR) {
        printf("FAIL: I2CM2 init\n");
        led_red();
        while (1) {}
    }

    if (bmi160_init() != E_NO_ERROR) {
        led_red();
        while (1) {}
    }

    led_off();

    device_state_t state = STATE_LOCKED;
    led_red();
    printf("State: LOCKED (press switch to unlock)\n");

    int16_t ax, ay, az;
    bmi160_read_accel(&ax, &ay, &az);
    int32_t prev_mag = accel_magnitude(ax, ay, az);

    while (1) {
        TMR_Delay(MXC_TMR0, MSEC(20));   /* 50 Hz sample rate */

        bmi160_read_accel(&ax, &ay, &az);
        int32_t mag   = accel_magnitude(ax, ay, az);
        int32_t delta = mag - prev_mag;
        if (delta < 0) delta = -delta;
        prev_mag = mag;

        if (state == STATE_UNLOCKED) {
            if (delta > JERK_THRESHOLD) {
                state = STATE_LOCKED;
                led_both();
                TMR_Delay(MXC_TMR0, MSEC(100));
                led_red();
                printf("TUG DETECTED (delta=%ld) -- LOCKED\n", (long)delta);
            }
        } else {
            /* LOCKED: watch for switch press */
            if (sw_just_pressed()) {
                state = STATE_UNLOCKED;
                led_both();
                TMR_Delay(MXC_TMR0, MSEC(100));
                led_green();
                printf("PIN accepted (switch) -- UNLOCKED\n");
            }
        }
    }
}
