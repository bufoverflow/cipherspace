#!/usr/bin/env bash
set -euo pipefail

GAME_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  printf 'Error: %s\n' "$*" >&2
  exit 1
}

if (( $# > 1 )); then
  printf 'Usage: %s [path/to/game.gbc]\n' "$0" >&2
  exit 2
fi

ROM="${1:-$GAME_DIR/build/cipherspace.gbc}"
[[ -f "$ROM" && -r "$ROM" ]] || fail "ROM file is missing or unreadable: $ROM"
ROM="$(CDPATH= cd -- "$(dirname -- "$ROM")" && pwd)/$(basename -- "$ROM")"
ROM_BYTES="$(wc -c < "$ROM" | tr -d '[:space:]')"
(( ROM_BYTES >= 32768 )) || fail "ROM must be at least 32 KiB, including its cartridge header."
(( ROM_BYTES % 16384 == 0 )) || fail "ROM must contain complete 16 KiB banks."

if [[ -n "${CHROMATIC_CLI:-}" ]]; then
  CLI="$(command -v -- "$CHROMATIC_CLI")" || fail "CHROMATIC_CLI executable was not found: $CHROMATIC_CLI"
elif [[ -x "$GAME_DIR/.tools/chromatic-cli" ]]; then
  CLI="$GAME_DIR/.tools/chromatic-cli"
else
  CLI="$(command -v chromatic-cli)" || fail "Install the official Chromatic CLI or set CHROMATIC_CLI to its executable."
fi
[[ -x "$CLI" ]] || fail "Chromatic CLI is not executable: $CLI"

if [[ -x "$GAME_DIR/.venv/bin/python" ]]; then
  PYTHON="$GAME_DIR/.venv/bin/python"
else
  PYTHON="$(command -v python3)" || fail "Python 3 is required to inspect the connected-device list."
fi

SHA256="$(shasum -a 256 -- "$ROM" | awk '{print $1}')"
printf 'ROM: %s\nSize: %s bytes\nSHA-256: %s\n' "$ROM" "$ROM_BYTES" "$SHA256"

DEVICES_JSON="$("$CLI" list-devices --format json)"
DEVICE_COUNT="$(printf '%s\n' "$DEVICES_JSON" | "$PYTHON" -c '
import json, sys
data = json.load(sys.stdin)
if data.get("ok") is not True:
    raise SystemExit("Chromatic CLI could not list devices.")
devices = data.get("result", {}).get("devices")
if not isinstance(devices, list):
    raise SystemExit("Unrecognized Chromatic CLI device-list format.")
print(len(devices))
')"
if [[ "$DEVICE_COUNT" == 0 ]]; then
  fail "No Chromatic is connected. Insert the DevDay cartridge, turn on the console, and connect a USB data cable."
elif [[ "$DEVICE_COUNT" != 1 ]]; then
  fail "Connect exactly one Chromatic before flashing (found $DEVICE_COUNT)."
fi

printf '\nInspect the cartridge details below before confirming the write.\n'
"$CLI" detect-cart --all
exec "$CLI" write-homebrew "$ROM" --expect-sha256 "$SHA256"
