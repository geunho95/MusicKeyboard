# RP2350 Platform Status

This directory now contains the first real embedded bring-up pieces.

Implemented:

- `d200_link_rp2350.c`
  - direct GPIO buttons for bring-up
  - internal pull-up inputs
  - USB serial status output
- `audio_hal_rp2350.c`
  - temporary PWM output on `GP15`
  - ring-buffered mono preview for buzzer / piezo tests
- `storage_hal_rp2350.c`
  - fallback sample generation
  - `FAT32` root scan for visible `.wav` files
  - loads `8-bit/16-bit PCM` WAV into a mono sample buffer
- `diskio_sd_spi_rp2350.c`
  - SPI microSD block driver for FatFs on `GP2..GP5`

Planned next:

1. verify SD card interoperability with the exact `WK TF` module on hand
2. improve USB serial diagnostics during runtime
3. add save/project metadata on top of FAT
4. replace PWM preview with I2S output
5. add I2S mic capture
6. add real D200 UART transport

Design rules carried over from the research:

- keep audio callback and ISR-safe work minimal
- move time-critical DSP to SRAM when XIP jitter becomes audible
- run transport / UI / UART outside the fast audio path
- make storage writes explicit and infrequent
