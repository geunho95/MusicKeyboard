#!/bin/zsh

export PICO_SDK_PATH="/Users/tvd/sdk/pico-sdk"
export PICO_BOARD="${PICO_BOARD:-pico2}"
export ARM_GNU_TOOLCHAIN_PATH="/Users/tvd/sdk/arm-gnu-toolchain-15.2.rel1/bin"

if [[ -d "$ARM_GNU_TOOLCHAIN_PATH" ]]; then
  export PATH="$ARM_GNU_TOOLCHAIN_PATH:$PATH"
fi

if [[ ! -d "$PICO_SDK_PATH" ]]; then
  echo "pico-sdk not found at $PICO_SDK_PATH" >&2
  return 1 2>/dev/null || exit 1
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "arm-none-eabi-gcc not found in PATH" >&2
  return 1 2>/dev/null || exit 1
fi

echo "PICO_SDK_PATH=$PICO_SDK_PATH"
echo "PICO_BOARD=$PICO_BOARD"
echo "ARM_GNU_TOOLCHAIN_PATH=$ARM_GNU_TOOLCHAIN_PATH"
