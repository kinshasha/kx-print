# KX-Print

A Panasonic **KX-R540** electronic typewriter, on Wi-Fi, as a text printer.

Arduino UNO R4 WiFi (Jaycar XC9211) bit-bangs the Mini-DIN-8 computer
port — not UART, not Centronics — and accepts raw TCP **9100** plus
LPR **515**. Hostname `kxr540.local`, queue `kxr540`.

Workshop project. Parts from Jaycar. If you wanted an IPP stack and a
case mould, you are in the wrong repo.

## Safety (read this)

**Mini-DIN pin 5 is +12 V. It does not go to the UNO. Ever.**

The 5-pin DIN lead is there so that pin cannot sneak through. Full lid
card: [`SAFETY.md`](SAFETY.md). Wiring: [`docs/wiring.md`](docs/wiring.md).

- UNO on its own 5 V USB-C (1 A min, 2 A preferred)
- Share **signal GND only**
- Continuity-test the cable, all five pins, before it meets the typewriter
- Do **not** use a Mac printer / null-modem Mini-DIN-8 cable (pins are swapped)
- Label the DIN **KX-R ONLY** — it looks like MIDI and is not
- First print is **one character**

Typewriter in printer mode: **CODE + E**, LCD shows **ON LINE**.

## Architecture

```
  laptop / CUPS / phone
           |
           |  Wi-Fi
           |  kxr540.local
           |  :9100 raw    :515 LPR
           v
   ┌───────────────────┐     USB-C 5 V
   | Arduino UNO R4    |<---- PSU (not the typewriter)
   | WiFi  (RA4M1)     |
   | D4 /ACK   in      |
   | D5 DATA   out     |
   | D6 /STB   out     |
   | D7 /ONLINE out    |
   | D8 RUN    toggle  |
   | D9 CANCEL button  |
   └─────────┬─────────┘
             | 5-pin DIN  (no +12 V pin)
             v
        Mini-DIN-8
        4 /ACK  6 DATA  7 /STB  8 /ONLINE
        1,2,3 GND      5 +12 V  CUT
             |
             v
        Panasonic KX-R540
```

```
  Mini-DIN-8 (typewriter looking into jack, typical):

           6
       7       5     pin 5 = +12 V = DO NOT USE
     8           4
       3   2   1     1,2,3 = GND
```

Firmware sits in `firmware/`. Protocol notes (ACK polarity, R540
uncertainties) are in [`docs/protocol.md`](docs/protocol.md).

## Features

- Raw JetDirect-style TCP 9100
- LPR/LPD 515, RFC 1179 receive-job, cf-first or df-first
- mDNS `kxr540.local`, `_printer._tcp`
- USB serial 115200: `help`, `status`, `print`, `dry-run`, `cancel`
- RUN/PAUSE toggle (pause stops the daisywheel, not TCP)
- CANCEL short = this job; hold ~3 s = clear queue; hold at boot = Wi-Fi reset
- AP `KX-Print-Setup` when there are no creds
- 12×8 matrix: ready ✓, printing P, paused, Wi-Fi W, error !, cancelled X
- Dry-run: log bytes, no ACK (bench without the typewriter)
- UTF-8 fold for curly quotes, dashes, ellipsis, accents

## Flash

UNO R4 WiFi, PlatformIO, **Renesas** core — not an ESP32 sketch.

```bash
cd firmware
cp secrets.h.example secrets.h   # or skip and use the setup AP
# edit secrets.h
pio run -t upload
pio device monitor -b 115200
```

`platformio.ini` already has `platform = renesas-ra`, `board = uno_r4_wifi`.

Build env `dry_run` forces dry-run on at boot (`-DKX_DRY_RUN_DEFAULT=1`).

## Print

```bash
printf 'HELLO PANASONIC\r\n' | nc kxr540.local 9100
```

If GNU netcat keeps the socket open:

```bash
printf 'HELLO PANASONIC\r\n' | nc -q 1 kxr540.local 9100
```

LPR:

```bash
echo 'HELLO PANASONIC' | lpr -H kxr540.local -P kxr540 -o raw
```

CUPS device URI:

```
lpd://kxr540.local/kxr540
```

CUPS also copes with a raw socket:

```
socket://kxr540.local:9100
```

Add it as a **Generic / Text Only** queue. Do not send PDF or
PostScript; the daisywheel will type `?` at you.

Scripts: `scripts/test-raw.sh`, `scripts/test-lpr.sh`.

## Serial

115200 8N1. `help` then `status`. `print A` is the first real test once
ACK idles LOW. `dry-run on` for a bench with no typewriter.

## Docs

| File | What |
|------|------|
| [`SAFETY.md`](SAFETY.md) | Eight rules, printable for the lid |
| [`docs/hardware.md`](docs/hardware.md) | BOM, Jaycar SKUs |
| [`docs/wiring.md`](docs/wiring.md) | Pin tables, +12 V, Mac-cable hazard |
| [`docs/protocol.md`](docs/protocol.md) | Bit-bang, ACK polarity, LPR |
| [`docs/bring-up.md`](docs/bring-up.md) | Numbered first-light checklist |
| [`docs/enclosure.md`](docs/enclosure.md) | HB6013 holes and labels |

MIT. See `LICENSE`.
