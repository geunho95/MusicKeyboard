# Standalone Sampler Skeleton

## Why This Shape

This skeleton reflects the strongest ideas from the research:

- `PO-33`-style immediacy:
  - short sample capture
  - speed-based pitch change
  - simple destructive or semi-destructive performance workflow
- `oyama/pico-midi-looper`-style control architecture:
  - small, explicit state machines
  - platform drivers separated from musical logic
- `pico-dac-sampler`-style triggerable sample slots:
  - direct sample pad style triggering
  - simple per-slot sample assignment
- `Darshana`-style embedded discipline:
  - DMA-centric I/O later
  - time-critical DSP kept separate from UI and storage
  - clear distinction between fast audio path and slower control path
- `pico-kvstore` / `pico-vfs`-style persistence:
  - presets and settings in simple key-value storage first
  - file-backed sample management later

## Primary Assumption

The scaffold assumes:

- `RP2350` is the primary embedded target
- the long-term UI host is `D200`
- the near-term bring-up profile is direct `GPIO buttons + SPI microSD + temporary PWM audio`
- `RP2350` owns audio, timing, sample playback, and persistence

This means the code is split into:

- `core`: sampler, sequencer, transport, audio-engine state
- `platform`: D200 link, audio HAL, storage HAL
- `app`: glue that maps controls to musical actions

## MVP Signal Flow

```text
D200 buttons
  -> D200 UART link
  -> app event router
  -> transport / sequencer / sample-bank state
  -> audio engine voice triggers
  -> audio HAL
  -> I2S DAC / amp

I2S mic
  -> future record HAL
  -> sample-bank slot writer
  -> sequencer / playback
```

### Bring-Up Signal Flow Before I2S Parts Arrive

```text
GPIO buttons
  -> app event router
  -> transport / sequencer state

SPI microSD
  -> storage HAL

temporary PWM audio
  -> boot chirps / metronome / crude preview path
```

## Proposed First Musical Scope

The scaffold bakes in a conservative first instrument model:

- `4` loop tracks
- `8` sample slots
- `16 steps per bar`
- up to `16 bars`
- `8` playback voices

That gives enough room for:

- kick / snare / hat / percussion style drum use
- melodic loop layers later
- one-shot and loop-slot coexistence

## Button Mapping For The Skeleton

The code skeleton uses a simple placeholder D200 mapping:

- `button 0`: play / stop
- `button 1`: next track
- `button 2`: arm step write
- `button 3`: clear selected track
- `button 4..11`: audition / assign sample slots `0..7`
- `button 12`: save project

This is not final UX. It is only enough to establish module boundaries.

## Module Boundaries

### `transport`

Owns:

- BPM
- loop length in bars
- current step
- run / stop state

Does not know:

- samples
- UI
- hardware

### `sequencer`

Owns:

- selected track
- per-track sample assignment
- mute / level / tune metadata
- step pattern data

Does not know:

- UART
- audio drivers

### `sample_bank`

Owns:

- slot metadata
- slot names
- nominal frame counts / sample rates
- occupied vs empty slot state

Later it will also own:

- trimming metadata
- loop points
- reverse flag
- bitcrush / tone defaults

### `audio_engine`

Owns:

- active voices
- sample trigger lifecycle
- future rate / interpolation / loop playback

Does not know:

- button meanings
- storage format

### `d200_link`

Owns:

- button event ingress
- future status rendering protocol

Does not know:

- sequencing rules
- sample playback rules

### `storage_hal`

Owns:

- load / save of project state
- future preset and sample metadata persistence

Likely real implementations later:

- `pico-kvstore` for presets and settings
- `pico-vfs` or LittleFS/SD for sample files

## Embedded Next Steps

After the scaffold, the highest-value next tasks are:

1. Add a current-hardware bring-up profile:
   - direct GPIO buttons
   - SPI microSD
   - temporary PWM audio
2. Replace host `d200_link` stub with real UART NDJSON transport.
3. Replace host `audio_hal` stub with RP2350 `I2S out` path.
4. Add `record_hal` for I2S mic capture into sample slots.
5. Replace silent audio-engine render with:
   - variable-rate mono playback
   - linear interpolation
   - one-shot and loop modes
6. Add storage-backed project persistence.

## Non-goals For The Skeleton

This scaffold intentionally does not implement:

- real DSP
- sample file decoding
- actual D200 rendering
- SD card integration
- advanced slicing
- time-stretch

The goal here is to freeze the architecture before the real-time code begins.
