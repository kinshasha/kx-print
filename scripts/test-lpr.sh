#!/bin/sh
# LPR smoke test. Queue name is kxr540 (RFC 1179 receive-job on :515).
set -eu
HOST="${1:-kxr540.local}"
QUEUE="${2:-kxr540}"

msg() { printf '%s\n' "$*"; }

msg "CUPS URI: lpd://$HOST/$QUEUE"

if command -v lpr >/dev/null 2>&1; then
  echo 'HELLO PANASONIC' | lpr -H "$HOST" -P "$QUEUE" -o raw
  msg "lpr submitted"
  exit 0
fi

msg "lpr not installed; sending a tiny RFC 1179 receive-job via python3"
python3 - "$HOST" "$QUEUE" << 'PY'
import socket, sys, time
host, queue = sys.argv[1], sys.argv[2]
data = b"HELLO PANASONIC\n"
ctrl = b"Hkx-print\nPuser\nfdfA000host\nNstdin\n"
job = "000host"

def ack(s):
    b = s.recv(1)
    if b != b"\x00":
        raise SystemExit("NAK %r" % b)

s = socket.create_connection((host, 515), 8)
s.sendall(b"\x02" + queue.encode() + b"\n")
ack(s)
# data file first (CUPS-style), then control file
s.sendall(b"\x03%d dfA%s\n" % (len(data), job.encode()))
ack(s)
s.sendall(data + b"\x00")
ack(s)
s.sendall(b"\x02%d cfA%s\n" % (len(ctrl), job.encode()))
ack(s)
s.sendall(ctrl + b"\x00")
ack(s)
s.close()
print("lpr python client done")
PY
