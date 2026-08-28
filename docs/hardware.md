# Hardware

Panasonic KX-R540 daisywheel + Arduino UNO R4 WiFi. Parts from Jaycar
where the catalogue has them.

## What it is

The typewriter already knows how to print. We are a Wi-Fi front end that
speaks its Mini-DIN-8 computer interface, plus a box, a switch, and a
button. No UART adapter, no Centronics hat, no +12 V.

## BOM

| Item | Jaycar / source | Notes |
|------|-----------------|--------|
| Arduino UNO R4 WiFi | XC9211 | Renesas RA4M1 + ESP32-S3 radio, 12×8 matrix, USB-C |
| Jiffy box 130 × 68 × 44 mm | HB6013 | Tight but the UNO sits along the length |
| Mini-DIN-8 male | PP0370 | Typewriter end. Pin 5 unsoldered. |
| 5-pin DIN panel socket | PS0348 | Interconnect on the box |
| 5-pin DIN MIDI cable 1.8 m | Cable Matters | Must be 1:1 — test all five pins |
| SPST toggle | ST0570 | RUN/PAUSE, D8 to GND |
| Momentary push | SP0716 | CANCEL, D9 to GND |
| Experimenter board | HP9556 | Optional 100 Ω series |
| 100 Ω ¼ W × 4 | any | Optional series on ACK/DATA/STB/ONLINE |
| 5 V USB-C PSU | 1 A min, 2 A preferred | Own supply, not the typewriter |
| USB-C cable | — | Flash + serial, or power if PSU is USB-C |
| Hookup wire, heatshrink, labels | — | Label **KX-R ONLY** |

## Why UNO R4 WiFi

GPIO is 5 V. The typewriter bus idles around 5 V. A 3.3 V board (Pico,
ESP32 as the *application* MCU) wants level shifting; this one does not.
The ESP32-S3 on the R4 is the radio coprocessor only — firmware is a
Renesas Arduino sketch (`WiFiS3`), not an ESP32 sketch.

RAM is 32 kB. We stream. Do not “just buffer the PDF”.

## Controls and indicators

- Toggle RUN/PAUSE on the lid
- CANCEL next to it
- On-board 12×8 matrix: checkmark / P / pause bars / W / `!` / X
- USB serial 115200 if you want to watch ACK timeouts with a mug of tea

## What you are not buying

- No MIDI adapter, no Mac printer cable, no Mini-DIN-8 “serial” lead from
  the Apple drawer. See `docs/wiring.md`.
- No connection to Mini-DIN pin 5.
