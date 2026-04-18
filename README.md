# MusicKeyboard

Research-driven skeleton for a standalone lo-fi sampler instrument.

The current direction is:

- Primary hardware target: `RP2350`
- UI host: `Ulanzi D200`
- Audio input: `I2S MEMS mic`
- Audio output: `I2S DAC / amp`
- Performance model: `speed = pitch`, short samples, loop-building, tactile control

This workspace now contains three layers:

- [sampler_research_notes.md](/Users/tvd/dev/MusicKeyboard/sampler_research_notes.md): raw research notes
- [project_direction.md](/Users/tvd/dev/MusicKeyboard/project_direction.md): project direction and scope
- [docs/standalone_sampler_skeleton.md](/Users/tvd/dev/MusicKeyboard/docs/standalone_sampler_skeleton.md): architecture distilled from the research

The initial code scaffold lives under [firmware](/Users/tvd/dev/MusicKeyboard/firmware) and is organized so that:

- core sampler logic stays board-agnostic
- `RP2350 + D200` integration is isolated behind HAL-style interfaces
- the project can be smoke-tested on the host before embedded drivers exist

For the immediate physical prototype, use the current-hardware bring-up profile:

- [docs/rp2350_minimal_bringup_profile.md](/Users/tvd/dev/MusicKeyboard/docs/rp2350_minimal_bringup_profile.md)

That profile assumes:

- direct GPIO buttons instead of `D200` for now
- `WK TF` style SPI microSD module
- temporary PWM audio until `MAX98357A` arrives

For host-side sample playback experiments, drop a WAV file into:

- `samples/source.wav`

Reference note:

- [samples/README.md](/Users/tvd/dev/MusicKeyboard/samples/README.md)

RP2350/Pico SDK environment notes:

- [docs/pico_sdk_env.md](/Users/tvd/dev/MusicKeyboard/docs/pico_sdk_env.md)

Quick host build:

```bash
cmake -S firmware -B firmware/build
cmake --build firmware/build
./firmware/build/musickeyboard_sampler
```
