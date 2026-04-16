# Identity Protocol

A smart ID card system that performs cryptographic Bluetooth authentication at doors, eliminating repeated swipe+PIN at every entry point while maintaining strong security.

## Overview

The ID card (MAX32630FTHR + ATECC508A) unlocks once via PIN, then silently performs challenge-response crypto over Bluetooth every time you approach a door. If the card is forcibly removed, the IMU detects the tug and locks the device.

## Hardware

### ID Device
- **MCU**: MAX32630FTHR (Maxim/Analog Devices)
- **Crypto**: ATECC508A - ECC key storage, signing, TRNG
- **Input**: Membrane keypad
- **Indicators**: RGB LED

### Door Device
- **MCU**: MAX32630FTHR
- **Crypto**: ATECC508A
- **Networking**: Ethernet FeatherWing
- **Actuation**: Motor FeatherWing

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

## License

MIT
