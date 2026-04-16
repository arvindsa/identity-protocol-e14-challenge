# Identity Protocol

A smart ID card system that performs cryptographic Bluetooth authentication at doors, eliminating repeated swipe+PIN at every entry point while maintaining strong security.

## Overview

The ID card (MAX32630FTHR + ATECC508A) unlocks once via PIN, then silently performs challenge-response crypto over Bluetooth every time you approach a door. If the card is forcibly removed, the IMU detects the tug and locks the device.

## Hardware

### ID Device
- **MCU**: MAX32630FTHR (Maxim/Analog Devices)
- **Crypto**: ATECC508A — ECC key storage, signing, TRNG
- **Input**: Membrane keypad
- **Indicators**: RGB LED

### Door Device
- **MCU**: MAX32630FTHR
- **Crypto**: ATECC508A
- **Networking**: Ethernet FeatherWing
- **Actuation**: Motor FeatherWing

## Series

This project is documented as a build log on element14:
- Part 1 — The Idea
- Part 2 — Django Server
- Part 3 — Unboxing, Blinky with Maxim LPSDK
- Part 4 — BLE Bring-up with BTstack and CC2564B

## License

MIT
