#!/usr/bin/env bash
#
# Streams the ESP32's serial output to the terminal — like `tail -f` for the
# device. Auto-detects the USB serial port and survives short disconnects so
# you can keep the monitor open across a re-flash or a power cycle.
#
# Usage:
#   ./monitor-esp.sh                                   # auto-detect port, 115200 baud
#   ./monitor-esp.sh --port /dev/cu.usbserial-0001
#   ./monitor-esp.sh --baud 74880                      # ESP boot ROM speed
#   ./monitor-esp.sh --once                            # don't auto-reconnect on disconnect
#
# Prefers `tio` if installed (cleanest UX, exit with Ctrl-T Q). Falls back to
# the system `screen`, in which case exit with Ctrl-A K (and confirm with y).
#
set -euo pipefail

PORT=""
BAUD=115200
ONCE=0

for arg in "$@"; do
  case "$arg" in
    --port)    shift || true; PORT="${1:-}"; shift || true ;;
    --port=*)  PORT="${arg#--port=}" ;;
    --baud)    shift || true; BAUD="${1:-115200}"; shift || true ;;
    --baud=*)  BAUD="${arg#--baud=}" ;;
    --once)    ONCE=1 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    "") ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m!!\033[0m %s\n' "$*" >&2; }

detect_port() {
  local candidates=()
  shopt -s nullglob
  for p in /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* /dev/cu.usbmodem*; do
    candidates+=("$p")
  done
  shopt -u nullglob
  if [[ ${#candidates[@]} -eq 0 ]]; then
    err "No serial port detected. Plug in the ESP32 or pass --port /dev/cu.<name>."
    exit 1
  fi
  if [[ ${#candidates[@]} -gt 1 ]]; then
    err "Multiple serial ports detected — pass --port to disambiguate:"
    for c in "${candidates[@]}"; do err "  $c"; done
    exit 1
  fi
  printf '%s' "${candidates[0]}"
}

if [[ -z "$PORT" ]]; then
  PORT="$(detect_port)"
  log "Auto-detected serial port: $PORT"
fi

# Pick the best available terminal program.
if command -v tio >/dev/null 2>&1; then
  RUNNER=(tio -b "$BAUD" "$PORT")
  EXIT_HINT="exit with Ctrl-T Q"
elif command -v screen >/dev/null 2>&1; then
  RUNNER=(screen "$PORT" "$BAUD")
  EXIT_HINT="exit with Ctrl-A K (then y). Tip: 'brew install tio' for nicer UX."
else
  err "Neither 'tio' nor 'screen' found. Install one: brew install tio"
  exit 1
fi

# Ctrl-C should always end the loop, not just kill the inner program. Without
# this trap, screen catches SIGINT and we'd just re-spawn it.
trap 'echo; log "Stopped."; exit 0' INT TERM

log "Streaming $PORT @ ${BAUD} baud — $EXIT_HINT"

while true; do
  if [[ ! -e "$PORT" ]]; then
    if [[ "$ONCE" -eq 1 ]]; then
      err "Port $PORT not present."
      exit 1
    fi
    printf '\r[waiting for %s ...] ' "$PORT"
    sleep 0.5
    continue
  fi
  # `|| true` so a non-zero exit (e.g. screen on disconnect) doesn't kill the
  # outer loop under `set -e`.
  "${RUNNER[@]}" || true
  if [[ "$ONCE" -eq 1 ]]; then break; fi
  echo
  log "Serial closed — waiting for device to come back (Ctrl-C to quit)..."
done
