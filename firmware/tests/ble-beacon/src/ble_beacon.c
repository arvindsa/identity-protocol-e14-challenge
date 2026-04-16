/*
 * BLE Beacon — Identity Protocol ID Device
 *
 * Minimal BTstack test: initialise the CC2564B over HCI, then start
 * undirected advertising so the board shows up as "ID-Beacon" in any
 * BLE scanner.
 *
 * Green LED  = advertising running
 * Red LED    = HCI init failed / BLE stack error
 * Blue LED   = toggled by BTstack's hal_led (heartbeat from btstack_port)
 *
 * Console output is on UART1 MAP_A (P2_0 TX / DAPLink virtual COM) at 115200 baud.
 * Open the DAPLink serial port to watch the init sequence.
 *
 * See docs/ble-bringup-flowchart.svg for the full bring-up sequence.
 * Everything up to and including the run-loop init is handled by
 * btstack_port.c before btstack_main() is called — Board_Init, 32 kHz
 * slow clock on P1.7, nSHUTD release, RTS poll, UART0 MAP_B init, and
 * btstack_memory_init.  This file owns the remainder of the sequence.
 */

#define BTSTACK_FILE__ "ble_beacon.c"

#include <stdio.h>
#include "btstack.h"

/* ---- GPIO for RGB LED (active-low, open-drain) -------------------------- */
#include "gpio.h"

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

/* ---- Advertising payload ------------------------------------------------ */
/*
 * AD structure layout:
 *   [len] [type] [data...]
 *
 * 0x01 Flags  : 0x06 = LE General Discoverable | BR/EDR Not Supported
 * 0x09 Complete Local Name : "ID-Beacon"
 */
static const uint8_t adv_data[] = {
    0x02, 0x01, 0x06,
    0x0A, 0x09, 'I', 'D', '-', 'B', 'e', 'a', 'c', 'o', 'n',
};

/* ---- HCI event handler -------------------------------------------------- */
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {

    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            /* ble-bringup-flowchart.svg: CC256x init script upload complete,
             * HCI_STATE_WORKING received — safe to configure advertising. */
            printf("HCI working — starting BLE advertising\n");

            /* ble-bringup-flowchart.svg: gap_advertisements_enable(1) */
            {
                bd_addr_t zero_addr = {0};
                gap_advertisements_set_params(
                    0x0100,     /* interval min  100 ms */
                    0x0200,     /* interval max  200 ms */
                    0,          /* ADV_IND: undirected connectable */
                    0,          /* direct address type: public */
                    zero_addr,  /* no directed peer */
                    0x07,       /* channels 37, 38, 39 */
                    0x00);      /* no filter policy */
            }
            gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
            gap_advertisements_enable(1);
            /* ble-bringup-flowchart.svg: "ID-Beacon" now visible on scanner */
            GPIO_OutClr(&led_g);   /* green on */
            GPIO_OutSet(&led_r);   /* red off  */
            printf("Advertising as \"ID-Beacon\"\n");
        } else if (btstack_event_state_get_state(packet) == HCI_STATE_OFF) {
            printf("HCI off\n");
        }
        break;

    case HCI_EVENT_TRANSPORT_USB_INFO:
        break;

    default:
        break;
    }
}

/* ---- btstack_main --------------------------------------------------------
 * ble-bringup-flowchart.svg: user entry point — called by bluetooth_main()
 * in btstack_port.c once the run-loop and HCI transport are ready.
 * ----------------------------------------------------------------------- */
int btstack_main(int argc, const char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    leds_init();
    GPIO_OutClr(&led_r);   /* red on while initialising */

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    /* ble-bringup-flowchart.svg: triggers CC256x init script upload;
     * HCI_STATE_WORKING arrives asynchronously in packet_handler above. */
    hci_power_control(HCI_POWER_ON);
    return 0;
}
