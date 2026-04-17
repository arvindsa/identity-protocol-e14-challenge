/*
 * Test -- Membrane Keypad Scan (MAX32630FTHR, ID device)
 *
 * 4x4 matrix keypad -- SPI header pins repurposed as GPIO (no SPI peripheral
 * on ID device); CTS/RTS repurposed (no UART flow control needed):
 *   Rows (output, drive low to scan): P5_0, P5_1, P5_2, P5_3
 *   Cols (input, internal pull-up):   P5_4, P5_6, P3_2, P3_3
 *
 * When a key is pressed it pulls the corresponding column low while the
 * matching row is driven low.  All other rows remain high-Z (input) during
 * each row's scan window to avoid column bus contention.
 *
 * Debounce: two consecutive identical scans 10 ms apart are required before
 * a keypress is reported.  Key-up is reported after two consecutive idle scans.
 *
 * Keypad layout:
 *   Row 0: 1  2  3  A
 *   Row 1: 4  5  6  B
 *   Row 2: 7  8  9  C
 *   Row 3: *  0  #  D
 *
 * Output via DAPLink serial (UART1, P2_0/P2_1, 115200 baud):
 *   INIT          -- printed once on startup
 *   KEY c (r,c)   -- key label c, row r, column c (0-based) on press
 *   UP            -- key released
 *
 * Pass criterion: every key press prints the correct row/column pair.
 */

#include <stdio.h>
#include "mxc_errors.h"
#include "gpio.h"
#include "tmr_utils.h"
#include "uart.h"

/* ---- Board_Init override ------------------------------------------------- */
int Board_Init(void) { return E_NO_ERROR; }

/* ---- Keypad configuration ------------------------------------------------ */
#define KEYPAD_ROWS  4
#define KEYPAD_COLS  4

/* Row pins: P5_0, P5_1, P5_2, P5_3 (SPI header pins, no SPI peripheral on ID device) */
static const gpio_cfg_t row_cfg[KEYPAD_ROWS] = {
    { PORT_5, PIN_0, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL },
    { PORT_5, PIN_1, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL },
    { PORT_5, PIN_2, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL },
    { PORT_5, PIN_3, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL },
};

/* Column pins: P5_4, P5_6, P3_2, P3_3 -- input with internal pull-up */
static const gpio_cfg_t col_cfg[KEYPAD_COLS] = {
    { PORT_5, PIN_4, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP },
    { PORT_5, PIN_6, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP },
    { PORT_3, PIN_2, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP },
    { PORT_3, PIN_3, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP },
};

#define KEY_NONE  -1   /* no key detected this scan, cant use 0 */

/* Key label lookup: row-major, matches physical layout
 *   Row 0: 1 2 3 A
 *   Row 1: 4 5 6 B
 *   Row 2: 7 8 9 C
 *   Row 3: * 0 # D
 */
static const char key_map[KEYPAD_ROWS][KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' },
};

/*
 * keypad_init -- configure all row pins as outputs (high), all col pins as
 * inputs with pull-ups.
 */
static void keypad_init(void)
{
    for (int r = 0; r < KEYPAD_ROWS; r++) {
        GPIO_Config(&row_cfg[r]);
        GPIO_OutSet(&row_cfg[r]);   /* idle high */
    }
    for (int c = 0; c < KEYPAD_COLS; c++) {
        GPIO_Config(&col_cfg[c]);
    }
}

/*
 * keypad_scan -- drive each row low in turn, read columns, restore row.
 * Returns encoded key index (row*4+col) or KEY_NONE.
 *
 * Rows not being scanned are set to input (high-Z) to avoid bus contention
 * when multiple keys are wired to the same column.
 */
static int keypad_scan(void)
{
    for (int r = 0; r < KEYPAD_ROWS; r++) {
        /* Set all rows to input (high-Z) */
        for (int i = 0; i < KEYPAD_ROWS; i++) {
            gpio_cfg_t hi_z = row_cfg[i];
            hi_z.func = GPIO_FUNC_GPIO;
            hi_z.pad  = GPIO_PAD_INPUT;
            GPIO_Config(&hi_z);
        }

        /* Drive the target row low (output) */
        gpio_cfg_t out = row_cfg[r];
        out.func = GPIO_FUNC_GPIO;
        out.pad  = GPIO_PAD_NORMAL;
        GPIO_Config(&out);
        GPIO_OutClr(&out);

        /* Small settling time */
        TMR_Delay(MXC_TMR0, USEC(10));

        /* Read columns */
        for (int c = 0; c < KEYPAD_COLS; c++) {
            if (GPIO_InGet(&col_cfg[c]) == 0) {
                GPIO_OutSet(&out);
                GPIO_Config(&row_cfg[r]);
                GPIO_OutSet(&row_cfg[r]);
                return r * KEYPAD_COLS + c;
            }
        }

        /* Restore row */
        GPIO_OutSet(&out);
        GPIO_Config(&row_cfg[r]);
        GPIO_OutSet(&row_cfg[r]);
    }

    return KEY_NONE;
}

/* ---- Debounce ------------------------------------------------------------ */
/*
 * Two consecutive identical scan results 10 ms apart constitute a confirmed
 * state.  Transitions are only reported on confirmation.
 */
#define DEBOUNCE_MS  10

static int debounce(void)
{
    int first = keypad_scan();
    TMR_Delay(MXC_TMR0, MSEC(DEBOUNCE_MS));
    int second = keypad_scan();
    return (first == second) ? first : KEY_NONE - 1;  /* -2 = unstable */
}

/* ---- Main ---------------------------------------------------------------- */
int main(void)
{
    /* Init UART1 MAP_A for printf (DAPLink serial, P2_0/P2_1, 115200 baud).
     * CONSOLE_UART=1 in Makefile routes _write() here. */
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

    keypad_init();

    printf("INIT\r\n");

    int last_key = KEY_NONE;

    while (1) {
        int key = debounce();

        if (key >= 0 && key != last_key) {
            /* New confirmed press */
            int row = key / KEYPAD_COLS;
            int col = key % KEYPAD_COLS;
            printf("KEY %c (%d,%d)\r\n", key_map[row][col], row, col);
            last_key = key;

        } else if (key == KEY_NONE && last_key != KEY_NONE) {
            /* Key released (confirmed idle) */
            printf("UP\r\n");
            last_key = KEY_NONE;
        }
        /* key == -2 (unstable): ignore, loop again immediately */
    }
}
