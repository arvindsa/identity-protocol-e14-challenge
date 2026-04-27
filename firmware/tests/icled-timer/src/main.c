#include <stdio.h>
#include "board.h"
#include "gpio.h"
#include "tmr.h"
#include "tmr_utils.h"

/*
 * ICLED FeatherWing -- icon cycle via TMR4 PWM
 *
 * P5_6 maps to TMR4: (port*8 + pin) % num_timers = (5*8+6) % 6 = 4.
 *
 * At 96 MHz with prescale=1, one tick = 10.42 ns.
 * WS2812B bit period = 1.25 us = 120 ticks.
 *   T0H = 38 ticks (~396 ns)
 *   T1H = 77 ticks (~802 ns)
 *
 * Per-bit update: flag fires at period rollover (count resets to 0, output
 * goes HIGH).  New duty is written before the compare fires.  Worst-case
 * window for a T0 bit is 38 ticks; poll+clear+write takes ~10 cycles.
 * IRQs are disabled for the full frame to protect that slack.
 */

#define TMR_PERIOD  120
#define DUTY_T0      38
#define DUTY_T1      77

#define ROWS   7
#define COLS   15
#define NLEDS  (ROWS * COLS)

static const gpio_cfg_t din_gpio = { PORT_5, PIN_6, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL };
static const gpio_cfg_t din_tmr  = { PORT_5, PIN_6, GPIO_FUNC_TMR,  GPIO_PAD_NORMAL };

/* ---- Bitmaps ---- */

/*
 * Green tick -- access granted
 *
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * r0:  .  .  .  .  .  .  .  .  .  .  .  .  .  X  X
 * r1:  .  .  .  .  .  .  .  .  .  .  .  X  X  X  .
 * r2:  .  .  .  .  .  .  .  .  .  X  X  X  .  .  .
 * r3:  .  X  X  .  .  .  .  .  X  X  .  .  .  .  .
 * r4:  .  .  X  X  .  .  .  X  X  .  .  .  .  .  .
 * r5:  .  .  .  X  X  .  X  X  .  .  .  .  .  .  .
 * r6:  .  .  .  .  X  X  X  .  .  .  .  .  .  .  .
 */
static const uint8_t tick[ROWS][COLS] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
    {0,1,1,0,0,0,0,0,1,1,0,0,0,0,0},
    {0,0,1,1,0,0,0,1,1,0,0,0,0,0,0},
    {0,0,0,1,1,0,1,1,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,1,0,0,0,0,0,0,0,0},
};

/*
 * Red cross -- blacklisted
 *
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * r0:  X  X  .  .  .  .  .  .  .  .  .  .  .  X  X
 * r1:  .  .  X  X  .  .  .  .  .  .  .  X  X  .  .
 * r2:  .  .  .  .  X  X  .  .  .  X  X  .  .  .  .
 * r3:  .  .  .  .  .  .  X  X  X  .  .  .  .  .  .
 * r4:  .  .  .  .  X  X  .  .  .  X  X  .  .  .  .
 * r5:  .  .  X  X  .  .  .  .  .  .  .  X  X  .  .
 * r6:  X  X  .  .  .  .  .  .  .  .  .  .  .  X  X
 */
static const uint8_t cross[ROWS][COLS] = {
    {1,1,0,0,0,0,0,0,0,0,0,0,0,1,1},
    {0,0,1,1,0,0,0,0,0,0,0,1,1,0,0},
    {0,0,0,0,1,1,0,0,0,1,1,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0},
    {0,0,0,0,1,1,0,0,0,1,1,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0,0,1,1,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,1,1},
};

/*
 * Yellow lock -- authenticating
 *
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * r0:  .  .  .  .  .  X  X  X  X  X  .  .  .  .  .
 * r1:  .  .  .  .  X  .  .  .  .  .  X  .  .  .  .
 * r2:  .  .  .  .  X  .  .  .  .  .  X  .  .  .  .
 * r3:  .  .  .  X  X  X  X  X  X  X  X  X  .  .  .
 * r4:  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
 * r5:  .  .  .  X  .  .  .  X  X  .  .  X  .  .  .
 * r6:  .  .  .  X  X  X  X  X  X  X  X  X  .  .  .
 */
static const uint8_t lock[ROWS][COLS] = {
    {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,0,1,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,0,1,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,1,1,0,0,1,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0},
};

/*
 * Blue key -- waiting for authentication
 *
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * r0:  .  X  X  X  .  .  .  .  .  .  .  .  .  .  .
 * r1:  X  .  .  .  X  .  .  .  .  .  .  .  .  .  .
 * r2:  X  .  .  .  X  X  X  X  X  X  X  X  X  .  .
 * r3:  X  .  .  .  X  X  X  X  X  X  X  X  X  .  .
 * r4:  X  .  .  .  X  .  X  .  X  .  X  .  .  .  .
 * r5:  X  .  .  .  X  .  .  .  .  .  .  .  .  .  .
 * r6:  .  X  X  X  .  .  .  .  .  .  .  .  .  .  .
 */
static const uint8_t key[ROWS][COLS] = {
    {0,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,1,1,1,1,1,1,1,1,1,0,0},
    {1,0,0,0,1,1,1,1,1,1,1,1,1,0,0},
    {1,0,0,0,1,0,1,0,1,0,1,0,0,0,0},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
};

/*
 * Magenta sync -- server sync / blacklist update
 *
 * Circular arrows (↺) within a 7x7 pixel area (cols 4-10, rows 0-6),
 * centered at (r3, c7), radius ~3.  The previous design spanned 11 cols
 * which made it look oval on the 7-row display.
 *
 * Right arrowhead: extra pixel at (r1, c10) -- arc descends on the right.
 * Left arrowhead:  extra pixel at (r4, c5)  -- arc ascends on the left.
 * Gap on right side (r3-r4) and left side (r3) shows the opening.
 *
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * r0:  .  .  .  .  .  .  X  X  X  .  .  .  .  .  .
 * r1:  .  .  .  .  .  X  .  .  .  X  X  .  .  .  .
 * r2:  .  .  .  .  X  .  .  .  .  .  X  .  .  .  .
 * r3:  .  .  .  .  X  .  .  .  .  .  .  .  .  .  .
 * r4:  .  .  .  .  X  X  .  .  .  .  .  .  .  .  .
 * r5:  .  .  .  .  .  X  .  .  .  X  .  .  .  .  .
 * r6:  .  .  .  .  .  .  X  X  X  .  .  .  .  .  .
 */
static const uint8_t sync_icon[ROWS][COLS] = {
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,1,1,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,0,1,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0},
};

/* ---- Frame send ---- */

typedef struct { uint8_t r, g, b; } color_t;

static uint8_t duties[NLEDS * 24];

static void send_frame(const uint8_t bitmap[ROWS][COLS], color_t c)
{
    /* Build duty array directly from bitmap; iterate in LED-index order. */
    int idx = 0;
    for (int led = 0; led < NLEDS; led++) {
        int col = led / ROWS;
        int row = led % ROWS;
        uint8_t pr, pg, pb;
        if (bitmap[row][col]) {
            pr = c.r; pg = c.g; pb = c.b;
        } else {
            pr = pg = pb = 0;
        }
        uint8_t bytes[3] = { pg, pr, pb };   /* WS2812B order: G, R, B */
        for (int i = 0; i < 3; i++)
            for (int bit = 7; bit >= 0; bit--)
                duties[idx++] = ((bytes[i] >> bit) & 1) ? DUTY_T1 : DUTY_T0;
    }

    /* Reset strip (>50 us low). */
    GPIO_Config(&din_gpio);
    GPIO_OutClr(&din_gpio);
    TMR_Delay(MXC_TMR0, USEC(80));

    __disable_irq();

    /*
     * Start the timer with the pin still in GPIO (LOW) mode so the strip
     * continues to see LOW (reset).  Wait for the first period to end -- this
     * is the warmup period; the strip ignores it as part of the reset LOW.
     *
     * At the rollover flag the timer resets to count=0 and its output goes
     * HIGH.  We connect the pin with a single func_sel register write (~2
     * cycles) instead of GPIO_Config (~40 cycles including in_mode/out_mode
     * writes after func_sel).  Those extra cycles kept the pin HIGH before
     * TMR32_Start ran, pushing T0H from ~396 ns to ~810 ns -- well into T1H
     * territory -- corrupting the MSB of every first pixel.
     */
    TMR32_SetCount(MXC_TMR4, 0);
    TMR32_SetDuty(MXC_TMR4, duties[0]);
    TMR32_ClearFlag(MXC_TMR4);
    TMR32_Start(MXC_TMR4);

    while (!TMR32_GetFlag(MXC_TMR4));  /* warmup period */
    TMR32_ClearFlag(MXC_TMR4);

    /* count=0, output=HIGH: connect pin now -- bit 0 starts cleanly. */
    MXC_GPIO->func_sel[5] = (MXC_GPIO->func_sel[5] & ~MXC_F_GPIO_FUNC_SEL_PIN6)
                           | (MXC_V_GPIO_FUNC_SEL_MODE_TMR << MXC_F_GPIO_FUNC_SEL_PIN6_POS);

    for (int i = 1; i < NLEDS * 24; i++) {
        while (!TMR32_GetFlag(MXC_TMR4));
        TMR32_ClearFlag(MXC_TMR4);
        TMR32_SetDuty(MXC_TMR4, duties[i]);
    }

    while (!TMR32_GetFlag(MXC_TMR4));
    TMR32_Stop(MXC_TMR4);

    __enable_irq();

    /* Latch: drive low to make data visible. */
    GPIO_Config(&din_gpio);
    GPIO_OutClr(&din_gpio);
    TMR_Delay(MXC_TMR0, USEC(80));
}

int main(void)
{
    Board_Init();

    printf("icled-timer: icon cycle\n");

    TMR_Init(MXC_TMR4, TMR_PRESCALE_DIV_2_0, NULL);
    tmr32_cfg_pwm_t pwm_cfg = {
        .polarity    = TMR_PWM_NONINVERTED,
        .periodCount = TMR_PERIOD,
        .dutyCount   = 0,
    };
    TMR32_PWMConfig(MXC_TMR4, &pwm_cfg);

    color_t green  = {  0, 10,  0 };
    color_t red    = { 10,  0,  0 };
    color_t yellow = {  5,  5,  0 };
    color_t blue   = {  0,  0, 10 };
    color_t magenta = {  5,  0,  5 };

    while (1) {
        send_frame(key,       blue);   TMR_Delay(MXC_TMR0, MSEC(2000));
        send_frame(lock,      yellow); TMR_Delay(MXC_TMR0, MSEC(2000));
        send_frame(tick,      green);  TMR_Delay(MXC_TMR0, MSEC(2000));
        send_frame(cross,     red);    TMR_Delay(MXC_TMR0, MSEC(2000));
        send_frame(sync_icon, magenta); TMR_Delay(MXC_TMR0, MSEC(2000));
    }
}
