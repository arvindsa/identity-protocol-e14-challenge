# Test — BLE Beacon

Minimal BTstack bring-up: initialises the CC2564B over HCI and advertises as `ID-Beacon` on BLE. Green LED lights when advertising is running.

## Prerequisites

### 1. Init the BTstack submodule

```bash
git submodule update --init third_party/btstack
```

### 2. Generate the CC256x init script

BTstack's CC256x chipset driver requires a board-specific init script generated from TI's `.bts` firmware patches. This file is not committed to BTstack's
git repository — you must generate it once from TI's patch files. Download TI's CC256x service pack from the TI website (search for `CC256xB Service Pack`). You need two `.bts` files from the archive:

- `initscripts-TIInit_6.7.16_bt_spec_4.1.bts`
- `initscripts-TIInit_6.7.16_ble_add-on.bts`

Place the unzipped folder next to the BTstack chipset directory and run theconversion script:

```bash
cd third_party/btstack/chipset/cc256x

python convert_bts_init_scripts.py \
    cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_bt_spec_4.1.bts \
    cc256xb_bt_sp_v1.8/initscripts-TIInit_6.7.16_ble_add-on.bts \
    bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
```

This writes `bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c` into the `cc256x/` directory, which is where the Makefile expects it. 

### 3. Configure your SDK path

Copy the example and fill in your LPSDK path:

```bash
cp firmware/sdk.local.mk.example firmware/sdk.local.mk
cp firmware/sdk.local.ps1.example firmware/sdk.local.ps1
# edit both files — set LPSDK_ROOT to your LPSDK install directory
```

## Build and flash

```powershell
cd firmware/tests/ble-beacon
.\build_and_flash.ps1
```

Or from MSYS bash:

```bash
export PATH="/d/path/to/maxim/Toolchain/msys/1.0/bin:/d/path/to/maxim/Toolchain/bin:$PATH"
make -C firmware/tests/ble-beacon
```

## Console output

Open the DAPLink virtual COM port at 115200 baud. You should see:

```
HCI working — starting BLE advertising
Advertising as "ID-Beacon"
```

See `docs/ble-bringup-flowchart.svg` (in the private repo) for the full bring-up sequence.
