#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
UF2_PATH="${1:-$PROJECT_ROOT/firmware/rp2350_bringup/build/musickeyboard_rp2350_bringup.uf2}"

find_bootsel_volume() {
  local candidate

  for candidate in "/Volumes/RPI-RP2" "/Volumes/RP2350"; do
    if [[ -d "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done

  return 1
}

if [[ ! -f "$UF2_PATH" ]]; then
  echo "UF2 not found: $UF2_PATH" >&2
  exit 1
fi

if ! BOOTSEL_VOLUME="$(find_bootsel_volume)"; then
  if /opt/homebrew/bin/picotool reboot -u -f >/dev/null 2>&1; then
    for _ in {1..40}; do
      if BOOTSEL_VOLUME="$(find_bootsel_volume)"; then
        break
      fi
      sleep 0.25
    done
  fi
fi

if [[ -z "${BOOTSEL_VOLUME:-}" ]]; then
  echo "BOOTSEL volume not mounted." >&2
  echo "Press BOOTSEL while plugging in the board, then try again." >&2
  exit 1
fi

COPYFILE_DISABLE=1 cp -X "$UF2_PATH" "$BOOTSEL_VOLUME/"
sync

echo "Copied $UF2_PATH to $BOOTSEL_VOLUME"
