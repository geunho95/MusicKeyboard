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

---

## RP2350 빌드 & 플래시

> **BOOTSEL 버튼을 누를 필요 없습니다.** `picotool -f` 옵션으로 USB CDC를 통해 바로 플래시할 수 있습니다.

### 사전 준비

```bash
# pico-sdk 경로 설정 (한 번만)
export PICO_SDK_PATH=/path/to/pico-sdk
```

### rp2350_smoke (LCD + 매트릭스 UI 테스트)

```bash
cd firmware/rp2350_smoke/build
cmake .. -DPICO_BOARD=pico2
make -j4

# 플래시 (Pico가 USB로 연결된 상태에서)
picotool load -f musickeyboard_rp2350_smoke.uf2
picotool reboot
```

### rp2350_bringup (풀 앱: LCD + 버튼 + 오디오)

```bash
cd firmware/rp2350_bringup/build
cmake .. -DPICO_BOARD=pico2
make -j4

picotool load -f musickeyboard_rp2350_bringup.uf2
picotool reboot
```

### `picotool` 설치

```bash
# macOS (Homebrew)
brew install picotool

# 또는 소스 빌드: https://github.com/raspberrypi/picotool
```

---

## 브링업 현황 & 할 일

- [docs/bringup_status.md](docs/bringup_status.md) — 하드웨어 구성, 핀맵, 오디오 스택 상세
- [docs/todo.md](docs/todo.md) — 앞으로 해야 할 일 목록
