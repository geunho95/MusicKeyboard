# RP2350 Minimal Bring-Up Profile

## Purpose

This profile is the practical starting point for the current hardware on hand:

- `RP2350` dev board
- `WK TF` style microSD module
- temporary `buzzer / piezo / small amplified speaker`
- a handful of direct-wired buttons

It intentionally avoids waiting for:

- `ICS43434`
- `MAX98357A`
- `D200`

The goal is to bring up:

- boot and debug
- transport timing
- button scanning
- SD-backed project/sample file access later
- temporary audible feedback

## Phased Hardware Plan

### Phase A: What We Can Build Now

```text
Buttons
  -> GPIO inputs with pull-ups
  -> app control events

microSD module
  -> SPI0
  -> project/sample storage

temporary audio out
  -> PWM pin
  -> buzzer / piezo / small active speaker
```

### Phase B: Swap In Final Audio Parts Later

```text
ICS43434
  -> I2S mic input

MAX98357A
  -> I2S mono speaker amp

D200
  -> UART UI host
```

Phase A should validate the firmware shape before the real audio path arrives.

## Recommended Pin Map

### Current Bring-Up Wiring

| Function | RP2350 GPIO | Notes |
|---|---:|---|
| `microSD SCK` | `GP2` | `SPI0_SCK` |
| `microSD MOSI / DI` | `GP3` | `SPI0_TX` |
| `microSD MISO / DO` | `GP4` | `SPI0_RX` |
| `microSD CS` | `GP5` | dedicated chip select |
| `temp audio pwm` | `GP15` | buzzer / piezo / active speaker |
| `blue button 1` | `GP16` | chord trigger 1 |
| `blue button 2` | `GP17` | chord trigger 2 |
| `blue button 3` | `GP18` | chord trigger 3 |
| `blue button 4` | `GP19` | chord trigger 4 |
| `red button` | `GP20` | loop queue / record |

### Reserved For Later

| Future Function | RP2350 GPIO |
|---|---:|
| `D200 UART TX` | `GP0` |
| `D200 UART RX` | `GP1` |
| `I2S out BCLK` | `GP10` |
| `I2S out LRCK` | `GP11` |
| `I2S out DIN` | `GP12` |
| `I2S mic SCK` | `GP13` |
| `I2S mic WS` | `GP14` |
| `I2S mic SD` | `GP21` |

This keeps the current prototype simple without painting us into a corner later.

## Button Skeleton

Start with five buttons:

- `GP16`: blue chord button 1
- `GP17`: blue chord button 2
- `GP18`: blue chord button 3
- `GP19`: blue chord button 4
- `GP20`: red loop / record button

Suggested expansion later:

- `GP6`
- `GP7`

All buttons should use:

- one side to `GPIO`
- one side to `GND`
- internal pull-up enabled in firmware

## Temporary Audio Guidance

There are three different "small speaker" cases:

### `active buzzer`

Use simple GPIO / `tone()` style tests first.

Best for:

- boot chirps
- step clock confirmation
- button feedback

### `passive piezo`

Use PWM audio or tone generation.

Best for:

- crude melodic feedback
- metronome

### `small active speaker module`

Use PWM audio into the module input.

Best for:

- rough sample-engine bring-up before I2S output exists

### Do Not Do This

Do not connect a raw `4Ω / 8Ω` speaker directly to a GPIO pin.

## Implemented Button Behavior

### Red Button (GP20) — Loop Record

| 상태 | 동작 |
|------|------|
| `IDLE` | 트랜스포트 시작 + 다음 루프 경계에서 녹음 ARM |
| `ARMED` | 취소 (트랜스포트 정지) |
| `ACTIVE` | 녹음 정지 + 트랜스포트 정지 |

### Blue Buttons (GP16–GP19) — Chord Trigger

버튼을 누르면 3보이스 화음이 즉시 재생된다. 같은 버튼을 다시 누르면 이전 보이스를 정지하고 처음부터 재생(retrigger).

| 버튼 | 코드 | 레이트 (루트·장3도·단3도) |
|------|------|--------------------------|
| 1 (GP16) | C major | 1.000 · 1.260 · 1.498 |
| 2 (GP17) | A minor | 0.841 · 1.000 · 1.260 |
| 3 (GP18) | F major | 0.667 · 0.841 · 1.000 |
| 4 (GP19) | G major | 0.749 · 0.944 · 1.122 |

녹음 ACTIVE 상태에서 누르면 현재 스텝에 해당 코드가 기록된다.

## Onboard LED Feedback (GP25 PWM)

| 녹음 상태 | LED |
|-----------|-----|
| `IDLE` | 꺼짐 |
| `ARMED` | 100ms 주기 깜빡임 |
| `ACTIVE` | 루프 진행률에 비례한 밝기 (0% → 100%), 루프 시작마다 리셋 |

PWM wrap = 255, clkdiv = 4.0.

## microSD Notes

The common `WK TF` modules vary.

Check the board before powering it:

- if it includes a regulator and level shifting, it may expect `5V` on `VCC`
- if it is a bare `3.3V` SPI breakout, power it from `3V3`

Regardless of module type:

- the RP2350 logic side should remain `3.3V`
- `GND` must be shared

Current firmware expectation:

- format the card as `FAT32`
- put one visible `.wav` file in the card root, or use one of:
  - `source.wav`
  - `recording.wav`
  - `po33.wav`
  - `sample.wav`
- macOS hidden files such as `.DS_Store` and `._*.wav` are ignored
- supported WAV data for bring-up:
  - `PCM`
  - `8-bit` or `16-bit`
  - `mono` or `stereo` input, mixed down to mono in firmware
- RAM 한계로 최대 65536 프레임 로드 (44100Hz 기준 약 1.5초). 더 긴 샘플은 앞부분만 사용됨. 샘플을 24000Hz로 다운샘플링하면 약 2.7초까지 가능

### WAV 파일 탐색 순서

```
0:/source.wav
0:/recording.wav
0:/po33.wav
0:/sample.wav
0:/samples/source.wav
0:/samples/recording.wav
위 경로가 없으면 루트 디렉토리의 첫 번째 .wav 파일
위 모두 없으면 내장 fallback 사운드 (square wave)
```

## Firmware Implications

The firmware should treat this as a distinct bring-up profile:

1. `buttons_hal_rp2350`
2. `storage_hal_rp2350_spi_sd`
3. `audio_hal_rp2350_pwm`
4. later replace or augment with:
   - `audio_hal_rp2350_i2s`
   - `record_hal_rp2350_i2s`
   - `d200_link_rp2350_uart`

## First Bring-Up Milestones

1. ✅ Button presses print stable events over USB serial.
2. ✅ Temporary audio pin emits sound (PWM fallback square wave).
3. ✅ microSD card mounts and loads WAV sample.
4. ✅ Red button queues loop record, starts transport, and stops both on second press.
5. ✅ Blue buttons preview and write chord steps into the loop.
6. ✅ Onboard LED gives recording state feedback.

Once those work, the project is ready for the real I2S input/output parts.
