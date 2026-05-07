#!/usr/bin/env bash
#
# Builds the IrrigationManagementESP firmware (.bin) headless via arduino-cli,
# producing the same artifact as ArduinoIDE's "Sketch > Export compiled binary"
# for upload via ElegantOTA at /update.
#
# Long-term reproducibility design:
#   * All Arduino libraries live under ./libraries/ as git submodules pinned to
#     specific tags (see .versions.txt). The Library Manager is NOT used.
#   * The ESP32 core + xtensa toolchain are installed by setup-toolchain.sh,
#     which can also restore them from a vendor/ tarball without internet.
#   * arduino-cli config is taken from ./.arduino-cli.yaml so a system-wide
#     install does not interfere with this project.
#
# Usage:
#   ./build-firmware.sh                   # build firmware
#   ./build-firmware.sh --with-webapp     # also rebuild webapp/ → data/www/
#   ./build-firmware.sh --clean           # wipe build cache first
#
# Output: build/IrrigationManagementESP.ino.bin
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="$SCRIPT_DIR"
SKETCH_NAME="IrrigationManagementESP"
LIBRARIES_DIR="$SCRIPT_DIR/.libraries"
OUT_DIR="$SCRIPT_DIR/build"
CLI_BUILD_DIR="$OUT_DIR/arduino-cli"
ARDUINO_CONFIG="$SCRIPT_DIR/.arduino-cli.yaml"

# Full set of board options matching the ArduinoIDE Tools menu (see screenshot
# 2026-05-01). Every option is set explicitly so the headless build is byte-
# equivalent to the IDE build — relying on core defaults would silently drift
# whenever the ESP32 core changes them between versions.
FQBN="esp32:esp32:esp32"
FQBN+=":CPUFreq=240"
FQBN+=",DebugLevel=none"
FQBN+=",EraseFlash=none"
FQBN+=",EventsCore=1"
FQBN+=",FlashFreq=80"
FQBN+=",FlashMode=qio"
FQBN+=",FlashSize=4M"
FQBN+=",JTAGAdapter=default"
FQBN+=",LoopCore=1"
FQBN+=",PartitionScheme=default"
FQBN+=",PSRAM=disabled"
FQBN+=",UploadSpeed=921600"
FQBN+=",ZigbeeMode=default"

WITH_WEBAPP=0
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --with-webapp) WITH_WEBAPP=1 ;;
    --clean)       CLEAN=1 ;;
    -h|--help)     sed -n '2,21p' "$0"; exit 0 ;;
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

require_libraries() {
  local missing=0
  for lib in AsyncTCP ESPAsyncWebServer ElegantOTA ArduinoJson; do
    if [[ ! -f "$LIBRARIES_DIR/$lib/library.properties" ]]; then
      err "Library $lib missing in .libraries/. Run: git submodule update --init --recursive"
      missing=1
    fi
  done
  [[ $missing -eq 0 ]] || exit 1
}

require_core() {
  if ! arduino-cli --config-file "$ARDUINO_CONFIG" core list 2>/dev/null \
       | awk '{print $1}' | grep -qx 'esp32:esp32'; then
    err "ESP32 core not installed for this project. Run ./setup-toolchain.sh first."
    exit 1
  fi
}

build_webapp() {
  log "Building webapp into data/www/..."
  ( cd "$SCRIPT_DIR/webapp" && npm install && npm run build )
}

compile_sketch() {
  if [[ "$CLEAN" -eq 1 ]]; then
    log "Cleaning $CLI_BUILD_DIR..."
    rm -rf "$CLI_BUILD_DIR"
  fi
  mkdir -p "$CLI_BUILD_DIR"

  log "Compiling for $FQBN..."
  # ELEGANTOTA_USE_ASYNC_WEBSERVER=1: required by ElegantOTA async mode (CLAUDE.md).
  # compiler.cpp.extra_flags is empty per default in the ESP32 board recipe,
  # so we don't clobber any core-supplied defines.
  arduino-cli --config-file "$ARDUINO_CONFIG" compile \
    --fqbn "$FQBN" \
    --libraries "$LIBRARIES_DIR" \
    --build-path "$CLI_BUILD_DIR" \
    --build-property "compiler.cpp.extra_flags=-DELEGANTOTA_USE_ASYNC_WEBSERVER=1" \
    --warnings default \
    "$SKETCH_DIR"

  cp "$CLI_BUILD_DIR/${SKETCH_NAME}.ino.bin" "$OUT_DIR/${SKETCH_NAME}.ino.bin"
  cp "$CLI_BUILD_DIR/${SKETCH_NAME}.ino.elf" "$OUT_DIR/${SKETCH_NAME}.ino.elf" 2>/dev/null || true
  log "Done: $OUT_DIR/${SKETCH_NAME}.ino.bin"
  log "Upload via ElegantOTA at  http://<device>/update  (mode: Firmware)."
}

require_arduino_cli
require_libraries
require_core
[[ "$WITH_WEBAPP" -eq 1 ]] && build_webapp
compile_sketch
