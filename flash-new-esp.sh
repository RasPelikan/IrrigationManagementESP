#!/usr/bin/env bash
#
# Provisions a fresh ESP32 over USB:
#   1) Flashes the firmware: bootloader + partition table + boot_app0 + app
#      at the standard ESP32 offsets (matches what the Arduino IDE writes).
#   2) Builds a LittleFS image from data/ and flashes it at the LittleFS
#      ("spiffs") partition offset.
#
# After this, the device boots normally and serves the webapp from LittleFS.
# Subsequent updates do NOT need this script — use ElegantOTA at /update.
#
# Usage:
#   ./flash-new-esp.sh                         # auto-detect serial port
#   ./flash-new-esp.sh --port /dev/cu.usbserial-0001
#   ./flash-new-esp.sh --erase                 # erase whole flash first
#   ./flash-new-esp.sh --skip-fs               # only flash firmware
#   ./flash-new-esp.sh --skip-firmware         # only flash LittleFS
#
# Requires: ./build-firmware.sh ran successfully (merged.bin must exist) and
# ./setup-toolchain.sh installed the ESP32 core (esptool + mklittlefs).
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="IrrigationManagementESP"
ARDUINO_DATA="$HOME/Library/Arduino15-$PROJECT_NAME"
ARDUINO_CONFIG="$SCRIPT_DIR/.arduino-cli.yaml"
BUILD_DIR="$SCRIPT_DIR/build"
CLI_BUILD_DIR="$BUILD_DIR/arduino-cli"
DATA_DIR="$SCRIPT_DIR/data"
LFS_IMAGE="$BUILD_DIR/littlefs.bin"

# We flash the individual binaries (not the .merged.bin), because the ESP32
# core pads merged.bin to the full 4MB flash size — flashing it together with
# a separate LittleFS image causes esptool to error with an overlap at the
# spiffs partition offset.
#
# Standard ESP32 flash layout for PartitionScheme=default (matches what the
# Arduino IDE uploads over USB):
FW_OFFSET_BOOTLOADER="0x1000"
FW_OFFSET_PARTITIONS="0x8000"
FW_OFFSET_BOOT_APP0="0xe000"
FW_OFFSET_APP="0x10000"

# LittleFS image geometry — must match the ESP32 core's spiffs partition for
# the default 4MB scheme used by build-firmware.sh's FQBN. The values are
# parsed at runtime from default.csv (see find_littlefs_partition) so a core
# upgrade that changes the layout is picked up automatically.
LFS_BLOCK_SIZE=4096
LFS_PAGE_SIZE=256

PORT=""
ERASE=0
SKIP_FS=0
SKIP_FW=0
BAUD=921600

for arg in "$@"; do
  case "$arg" in
    --port)         shift || true; PORT="${1:-}"; shift || true ;;
    --port=*)       PORT="${arg#--port=}" ;;
    --baud=*)       BAUD="${arg#--baud=}" ;;
    --erase)        ERASE=1 ;;
    --skip-fs)      SKIP_FS=1 ;;
    --skip-firmware) SKIP_FW=1 ;;
    -h|--help)      sed -n '2,20p' "$0"; exit 0 ;;
    "") ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m!!\033[0m %s\n' "$*" >&2; }

require_arduino_cli() {
  if ! command -v arduino-cli >/dev/null 2>&1; then
    err "arduino-cli not found. Run ./setup-toolchain.sh first."
    exit 1
  fi
}

# Locate a tool inside the local ESP32 core (version-pinned dirs change with
# every core release, so we glob and pick the highest-sorting one).
find_tool() {
  local tool="$1"
  local found
  found="$(ls -d "$ARDUINO_DATA/packages/esp32/tools/$tool"/*/ 2>/dev/null | sort -r | head -1 || true)"
  if [[ -z "$found" ]]; then
    err "Tool '$tool' not found under $ARDUINO_DATA/packages/esp32/tools/. Run ./setup-toolchain.sh first."
    exit 1
  fi
  printf '%s' "${found%/}"
}

# Parse the spiffs/littlefs partition (offset, size) from the ESP32 core's
# default.csv. We pick the highest-version core dir to stay in sync with what
# build-firmware.sh actually compiled against.
find_littlefs_partition() {
  local csv
  csv="$(ls "$ARDUINO_DATA/packages/esp32/hardware/esp32"/*/tools/partitions/default.csv 2>/dev/null | sort -r | head -1 || true)"
  if [[ -z "$csv" ]]; then
    err "default.csv not found. Run ./setup-toolchain.sh first."
    exit 1
  fi
  # CSV columns: Name, Type, SubType, Offset, Size, Flags
  awk -F'[, \t]+' '/^[[:space:]]*spiffs/ { print $4, $5; exit }' "$csv"
}

detect_port() {
  local candidates=()
  shopt -s nullglob
  for p in /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* /dev/cu.usbmodem*; do
    candidates+=("$p")
  done
  shopt -u nullglob
  if [[ ${#candidates[@]} -eq 0 ]]; then
    err "No serial port detected. Plug in the ESP32 or pass --port /dev/cu.<name>."
    err "Tip: run 'ls /dev/cu.*' with the device unplugged, then plug it in and diff."
    exit 1
  fi
  if [[ ${#candidates[@]} -gt 1 ]]; then
    err "Multiple serial ports detected — pass --port to disambiguate:"
    for c in "${candidates[@]}"; do err "  $c"; done
    exit 1
  fi
  printf '%s' "${candidates[0]}"
}

# Resolve the four binaries that make a full firmware flash. build-firmware.sh
# copies the app .bin to build/ but leaves bootloader.bin and partitions.bin
# under build/arduino-cli/ (its --build-path), so we look in both places.
# boot_app0.bin is a small fixed marker shipped with the ESP32 core.
FW_BOOTLOADER=""
FW_PARTITIONS=""
FW_APP=""
FW_BOOT_APP0=""
require_firmware_bins() {
  for dir in "$BUILD_DIR" "$CLI_BUILD_DIR"; do
    [[ -z "$FW_APP"        && -f "$dir/${PROJECT_NAME}.ino.bin" ]]            && FW_APP="$dir/${PROJECT_NAME}.ino.bin"
    [[ -z "$FW_BOOTLOADER" && -f "$dir/${PROJECT_NAME}.ino.bootloader.bin" ]] && FW_BOOTLOADER="$dir/${PROJECT_NAME}.ino.bootloader.bin"
    [[ -z "$FW_PARTITIONS" && -f "$dir/${PROJECT_NAME}.ino.partitions.bin" ]] && FW_PARTITIONS="$dir/${PROJECT_NAME}.ino.partitions.bin"
  done
  FW_BOOT_APP0="$(ls "$ARDUINO_DATA/packages/esp32/hardware/esp32"/*/tools/partitions/boot_app0.bin 2>/dev/null | sort -r | head -1 || true)"

  local missing=0
  for v in FW_BOOTLOADER FW_PARTITIONS FW_APP FW_BOOT_APP0; do
    if [[ -z "${!v}" ]]; then
      err "Missing firmware binary: $v"
      missing=1
    fi
  done
  if [[ $missing -ne 0 ]]; then
    err "Run ./build-firmware.sh first (and ./setup-toolchain.sh for boot_app0.bin)."
    exit 1
  fi
}

require_data_dir() {
  if [[ ! -d "$DATA_DIR" ]]; then
    err "data/ directory not found at $DATA_DIR — nothing to put on LittleFS."
    err "Pass --skip-fs to flash firmware only."
    exit 1
  fi
}

build_littlefs_image() {
  local size_hex="$1"
  local size_dec=$(( size_hex ))
  log "Building LittleFS image from data/ (size=$size_hex / $size_dec bytes)..."
  local mklittlefs_dir
  mklittlefs_dir="$(find_tool mklittlefs)"
  "$mklittlefs_dir/mklittlefs" \
    -c "$DATA_DIR" \
    -b "$LFS_BLOCK_SIZE" \
    -p "$LFS_PAGE_SIZE" \
    -s "$size_dec" \
    "$LFS_IMAGE"
  log "LittleFS image: $LFS_IMAGE ($(du -h "$LFS_IMAGE" | awk '{print $1}'))"
}

flash_with_esptool() {
  local port="$1"; shift
  local esptool_dir
  esptool_dir="$(find_tool esptool_py)"
  local esptool="$esptool_dir/esptool"

  if [[ "$ERASE" -eq 1 ]]; then
    log "Erasing flash..."
    "$esptool" --chip esp32 --port "$port" --baud "$BAUD" erase-flash
  fi

  # Single esptool invocation = one auto-reset cycle, faster than two.
  log "Flashing: $*"
  "$esptool" --chip esp32 --port "$port" --baud "$BAUD" \
    --before default-reset --after hard-reset \
    write-flash -z "$@"
}

require_arduino_cli

[[ "$SKIP_FW" -eq 0 ]] && require_firmware_bins
[[ "$SKIP_FS" -eq 0 ]] && require_data_dir

if [[ -z "$PORT" ]]; then
  PORT="$(detect_port)"
  log "Auto-detected serial port: $PORT"
fi

# Build flash arguments: pairs of (offset, file).
flash_args=()

if [[ "$SKIP_FW" -eq 0 ]]; then
  flash_args+=(
    "$FW_OFFSET_BOOTLOADER" "$FW_BOOTLOADER"
    "$FW_OFFSET_PARTITIONS" "$FW_PARTITIONS"
    "$FW_OFFSET_BOOT_APP0"  "$FW_BOOT_APP0"
    "$FW_OFFSET_APP"        "$FW_APP"
  )
fi

if [[ "$SKIP_FS" -eq 0 ]]; then
  read -r LFS_OFFSET LFS_SIZE <<<"$(find_littlefs_partition)"
  if [[ -z "${LFS_OFFSET:-}" || -z "${LFS_SIZE:-}" ]]; then
    err "Could not determine LittleFS partition offset/size from default.csv."
    exit 1
  fi
  log "LittleFS partition: offset=$LFS_OFFSET size=$LFS_SIZE"
  build_littlefs_image "$LFS_SIZE"
  flash_args+=( "$LFS_OFFSET" "$LFS_IMAGE" )
fi

if [[ ${#flash_args[@]} -eq 0 ]]; then
  err "Both --skip-firmware and --skip-fs given — nothing to do."
  exit 2
fi

flash_with_esptool "$PORT" "${flash_args[@]}"

log "Done. The ESP32 should now boot normally; check the WiFi LED on GPIO 21."
log "Subsequent firmware updates: use ElegantOTA at http://<device>/update."
log "Subsequent webapp updates:   use the /webapp-upload page."
