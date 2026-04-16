/*
 * Test — GPIO / RGB LED (MAX32630FTHR)
 *
 * Cycles the on-board RGB LED through Red → Green → Blue, 500 ms each.
 * Active-low open-drain outputs on P2_4 (R), P2_5 (G), P2_6 (B).
 *
 * Pass criterion: LED visibly cycles R→G→B in a ~1.5 s period.
 *
 * Built with the LPSDK (SFW0001500A).  See ../../sdk.local.mk for the
 * LPSDK_ROOT path.
 */

#include "gpio.h"
#include "tmr_utils.h"
#include "mxc_errors.h"

/* Override the weak Board_Init() — FTHR board doesn't match EvKit_V1 init. */
int Board_Init(void)
{
    return E_NO_ERROR;
}

/* RGB LED — active-low, open-drain */
static const gpio_cfg_t led_r = { PORT_2, PIN_4, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };
static const gpio_cfg_t led_g = { PORT_2, PIN_5, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };
static const gpio_cfg_t led_b = { PORT_2, PIN_6, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN };

static void leds_init(void)
{
    GPIO_Config(&led_r);
    GPIO_Config(&led_g);
    GPIO_Config(&led_b);
    /* All off: drive high (active-low) */
    GPIO_OutSet(&led_r);
    GPIO_OutSet(&led_g);
    GPIO_OutSet(&led_b);
}

static void all_off(void)
{
    GPIO_OutSet(&led_r);
    GPIO_OutSet(&led_g);
    GPIO_OutSet(&led_b);
}

int main(void)
{
    leds_init();

    while (1) {
        /* Red */
        all_off();
        GPIO_OutClr(&led_r);
        TMR_Delay(MXC_TMR0, MSEC(500));

        /* Green */
        all_off();
        GPIO_OutClr(&led_g);
        TMR_Delay(MXC_TMR0, MSEC(500));

        /* Blue */
        all_off();
        GPIO_OutClr(&led_b);
        TMR_Delay(MXC_TMR0, MSEC(500));
    }
}
