# Identity Protocol

A smart ID card system that performs cryptographic Bluetooth authentication at doors, eliminating repeated swipe+PIN at every entry point while maintaining strong security.

## Overview

The ID card (MAX32630FTHR + ATECC508A) unlocks once via PIN, then silently performs challenge-response crypto over Bluetooth every time you approach a door. If the card is forcibly removed, the IMU detects the tug and locks the device.

## Hardware

> **Note:** This project was built for the element14 Smart Security & Surveillance design challenge. The parts were provided as-is for the challenge; some are NRND (Not Recommended for New Designs) and should be replaced with current alternatives in any new build.

### ID Device
- **MCU**: [MAX32630FTHR](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32630fthr.html) (Maxim/Analog Devices)
- **Crypto**: [ATECC508A](https://www.microchip.com/en-us/product/atecc508a) - ECC key storage, signing, TRNG *(NRND -- use ATECC608B for new designs)*
- **Input**: [4x4 Matrix Keypad](https://robu.in/product/4x4-matrix-16-keyboard-keypad/)
- **Indicators**: RGB LED

### Door Device
- **MCU**: [MAX32630FTHR](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32630fthr.html)
- **Networking**: [Ethernet FeatherWing](https://www.adafruit.com/product/3201) (W5500)
- **Actuation**: [DC Motor + Stepper FeatherWing](https://www.adafruit.com/product/2927)
- **Display**: [ICLED FeatherWing](https://www.we-online.com/en/components/products/OPTO_ICLED_FEATHERWING) (Wurth, 7x15 LED matrix)

## Pin Connections 

### BLE — PAN1326B

| Pin  | Signal       | Function                        |
|------|--------------|---------------------------------|
| P0.0 | UART0 TX     | HCI commands → CC2564B          |
| P0.1 | UART0 RX     | HCI events ← CC2564B            |
| P0.2 | UART0 CTS    | Flow control ← CC2564B          |
| P0.3 | UART0 RTS    | Flow control → CC2564B          |
| P1.6 | nSHUTD       | CC2564B reset (active-low)      |
| P1.7 | Slow clock   | 32 kHz reference for CC2564B    |

UART0 is configured on MAP_B with hardware flow control.

### I2C Bus — I2CM2 MAP_A (shared)

| Pin  | Signal | Devices on bus                              |
|------|--------|---------------------------------------------|
| P5.7 | SDA    | BMI160 IMU (0x68), ATECC508A (0x60), MAX14690N PMIC (0x28) |
| P6.0 | SCL    | BMI160 IMU (0x68), ATECC508A (0x60), MAX14690N PMIC (0x28) |

### RGB LED (active-low, open-drain)

| Pin  | Colour |
|------|--------|
| P2.4 | Red    |
| P2.5 | Green  |
| P2.6 | Blue   |

### Debug Console

| Pin  | Signal      | Function                        |
|------|-------------|---------------------------------|
| P2.0 | UART1 TX    | printf → DAPLink virtual COM    |
| P2.1 | UART1 RX    | stdin ← DAPLink virtual COM     |

UART1 is configured on MAP_A at 115200 baud.

## Unit Tests

Standalone firmware tests under `firmware/tests/`. Each directory contains a `Makefile` and `src/main.c`. Build with the LPSDK toolchain (see `firmware/sdk.local.mk.example`).

| Directory | What it tests |
|-----------|---------------|
| `gpio-led` | RGB LED GPIO -- cycles R/G/B, confirms active-low open-drain wiring |
| `keypad-scan` | 4x4 matrix keypad -- GPIO matrix scan, prints key presses over UART |
| `ble-beacon` | BLE -- PAN1326B bring-up, advertises a beacon packet |
| `imu-bmi160` | BMI160 I2C bring-up -- reads CHIP_ID, wakes accel + gyro, streams 6-axis data |
| `tug-detection` | Tug-detection state machine -- 50 Hz jerk threshold locks the device; P2_3 switch simulates PIN unlock |



## Build Log (element14)

This project is documented as a design challenge series on element14. Each post covers one implementation phase with schematics, code walk-throughs, and results.

- [Part 1 -- Plan](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56768/identity-protocol-part-1---plan)
- [Part 2 -- Django Server](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56830/identity-protocol---part-2---django-server)
- [Part 3 -- Unboxing and Blinking with Maxim LPSDK](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56840/identity-protocol---part-3---unboxing-and-blinking-with-maxim-lpsdk)
- [Part 4 -- BLE using PAN1326B and BTstack](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56853/identity-protocol---part-4---ble-using-pan1326b-and-btstack)
- [Part 5 -- Interfacing a Keypad](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56856/identity-protocol---part-5---interfacing-a-keypad)
- [Part 6 -- BMI160 IMU Interfacing and Snatch detection](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56861/identity-protocol---part-6---snatch-detection-with-the-bmi160-imu)  
- [Part 7 -- ICLED Feather Wing](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56874/identity-protocol---part-7---colouring-on-the-icled-featherwing)
- [Part 8 -- ATECC508A](https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56881/identity-protocol---part-7---crypto-graphically-sign-with-atecc508-and-verify-with-micro-ecc)
- Part 9 -- BLE GATT Communication: *(coming soon)*
- Part 10 -- W5500 Ethernet *(coming soon)*
- Part 11 -- Low Power: MAX32630 LP modes *(coming soon)*



## License

MIT
