#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

source "$SCRIPT_DIR/pico_env.zsh"

cmake -S "$PROJECT_ROOT/firmware/rp2350_bringup" \
  -B "$PROJECT_ROOT/firmware/rp2350_bringup/build" \
  -G Ninja \
  -DPICO_BOARD="$PICO_BOARD" \
  -DPICO_SDK_PATH="$PICO_SDK_PATH"

cmake --build "$PROJECT_ROOT/firmware/rp2350_bringup/build"

echo
echo "Built:"
echo "  $PROJECT_ROOT/firmware/rp2350_bringup/build/musickeyboard_rp2350_bringup.uf2"
