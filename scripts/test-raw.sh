#!/bin/sh
# Raw 9100 smoke test. Job ends when the TCP socket closes.
set -eu
HOST="${1:-kxr540.local}"
PORT="${2:-9100}"

msg() { printf '%s\n' "$*"; }

if command -v nc >/dev/null 2>&1; then
  :
else
  msg "nc not found"
  exit 1
fi

# GNU nc: -q 1; OpenBSD/macOS libressl nc: -N; busybox: -w 1 and hope.
send() {
  if nc -h 2>&1 | grep -q -- '-q'; then
    printf 'HELLO PANASONIC\r\n' | nc -q 1 "$HOST" "$PORT"
  elif nc -h 2>&1 | grep -q -- '-N'; then
    printf 'HELLO PANASONIC\r\n' | nc -N "$HOST" "$PORT"
  else
    printf 'HELLO PANASONIC\r\n' | nc -w 2 "$HOST" "$PORT"
  fi
}

msg "raw print → $HOST:$PORT"
send
msg "done (socket closed)"
