# Pico SDK Environment

This project now has a local RP2350/Pico SDK setup on this machine.

## Installed Tools

- `arm-none-eabi-gcc`
- `picotool`
- `openocd`
- `cmake`
- `ninja`

Local SDK checkout:

- `/Users/tvd/sdk/pico-sdk`

## One-Time Terminal Setup Per Shell

```bash
source /Users/tvd/dev/MusicKeyboard/scripts/pico_env.zsh
```

This exports:

- `PICO_SDK_PATH=/Users/tvd/sdk/pico-sdk`
- `PICO_BOARD=pico2`
- `ARM_GNU_TOOLCHAIN_PATH=/Users/tvd/sdk/arm-gnu-toolchain-15.2.rel1/bin`

## Build Smoke Target

```bash
/Users/tvd/dev/MusicKeyboard/scripts/build_rp2350_smoke.sh
```

Output:

- `firmware/rp2350_smoke/build/musickeyboard_rp2350_smoke.uf2`

## Build Bring-Up Target

```bash
/Users/tvd/dev/MusicKeyboard/scripts/build_rp2350_bringup.sh
```

Output:

- `firmware/rp2350_bringup/build/musickeyboard_rp2350_bringup.uf2`

## Flash Smoke Target

Put the board in `BOOTSEL` mode so it mounts as `RPI-RP2` or `RP2350`, then run:

```bash
/Users/tvd/dev/MusicKeyboard/scripts/flash_rp2350_smoke.sh
```

The flash helper disables macOS metadata copying so `._*.uf2` files are not created during the copy.

## Flash Bring-Up Target

From the normal runtime firmware, the helper first tries `picotool reboot -u -f` and then copies the UF2 once the board remounts in `BOOTSEL` mode:

```bash
/Users/tvd/dev/MusicKeyboard/scripts/flash_rp2350_bringup.sh
```

## Current Port Check

At setup time the board was visible as:

- `/dev/cu.usbmodem3401`

That indicates a serial-style runtime connection, not the `BOOTSEL` mass-storage mode.

## Why This Matters

With this setup in place, we can now:

- add a real `RP2350` firmware target to this repo
- build `.uf2` images locally
- flash test firmware when the board is in `BOOTSEL`
- use `picotool` / `openocd` later for more advanced flows
