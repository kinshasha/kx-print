# Protocol

This is **not UART**. It is a 4-wire bit-banged handshake. Firmware follows
the KX-R60 Mini-DIN-8 sequence used by
[xunker/panasonic_typewriter_interface](https://github.com/xunker/panasonic_typewriter_interface)
(tested on a KX-R435). The KX-R540 uses the same-era Mini-DIN-8 port;
public docs do not confirm R540 timing independently, so we treat it as
**KX-R60-class**. First bring-up should probe ACK idle before a long job
(firmware already prints a warning if ACK is not LOW).

If some other write-up disagrees with this file, **this file + `kx_protocol.cpp`
win**.

## Pinout (Mini-DIN-8), matches the spec and xunker

| Pin | Name     | Notes |
|-----|----------|--------|
| 1,2,3 | GND    | Combine |
| 4     | /ACK   | Typewriter → host |
| 5     | **+12 V** | **Never connect** |
| 6     | DATA   | Host → typewriter (also called TXD) |
| 7     | /STB   | Host → typewriter |
| 8     | /ONLINE| Host → typewriter |

## Printer mode

On KX-R530 / KX-R435 manuals: **CODE + E**. The LCD shows **ON LINE**.
Leave it there. If the machine is in typewriter mode, ACK will not dance
and you will get the 1.5 s timeout.

## Idle levels (host)

| Line     | Idle  |
|----------|-------|
| /ONLINE  | HIGH  |
| /STB     | HIGH  |
| DATA     | HIGH  |
| /ACK     | **LOW** (typewriter, ~10 k pulldown) |

Host GPIO: D5/D6/D7 idle HIGH. D4 is `INPUT`, **no pull-up**.

## Byte send (LSB first)

For each character:

1. Drive **/ONLINE LOW** (stays low for the whole byte).
2. For bits 0..7:
   - Set DATA (HIGH = 1, LOW = 0)
   - Wait **50 µs**
   - **/STB HIGH → LOW**
   - Wait until **ACK pin is HIGH**
   - **/STB LOW → HIGH**
   - Wait until **ACK pin is LOW**
3. Drive **/ONLINE HIGH**. DATA and /STB already idle HIGH.

ACK timeout: **1500 ms** per wait. That is ours (watchdog). The original
adapter code waits forever; we will not. On timeout: safe idle, abort the
job, serial error, LED `!`.

## ACK polarity: do not mix this up

**KX-R60 Mini-DIN-8 (this project):**
ACK idles **LOW**, pulses **HIGH** while /STB is low, returns **LOW** after
/STB rises.

**RP-K105 / DE-9 thermal writers (KX-W50TH, RK-H500, tdeck notes):**
the handshake is drawn the other way (wait ACK low then high, and some
host circuits invert through a 74LS240). **Do not use that sequence on a
KX-R540.**

xunker comments sometimes say “ACK active low”. That names the *signal*.
The *pin idle* on Mini-DIN-8 is LOW. Firmware waits HIGH then LOW, not
the thermal-writer polarity.

## What we assumed (R540 unconfirmed)

- DATA polarity HIGH=1, LOW=0 (xunker TXD). If the first test character
  prints as garbage bits, this is the first thing to flip, but do that
  only after ACK idle is confirmed LOW and CODE+E is on.
- /ONLINE is per byte, not per job. Matches the RP-K100 / KX-R60 write-up.
- 50 µs setup is a floor, not a bit-rate. The daisywheel is the clock;
  we wait ACK. Throughput is a few tens of characters per second, not
  9600 baud.
- Form feed (0x0C) is emulated as CR+LF pairs to the next top-of-form
  (default 66 lines). A daisywheel needs CR or the carriage walks a
  staircase. Spec said “enough LFs”; CR+LF is the practical version.
- No init/escape sequence is sent before the first character. If R540
  needs one, we have not seen it published.

## Text filter (host side, before bits)

- ASCII 0x20-0x7E pass through
- LF without a preceding CR becomes CR+LF; CR/LF already paired are kept
- TAB → 4 spaces
- FF 0x0C → CR+LF to next TOF
- compact UTF-8 fold: curly quotes, en/em dash, ellipsis, Latin-1 accents, €
- anything else → `?`
- NUL dropped

## Network side (not the typewriter protocol)

- Raw TCP **9100**: bytes in, same text filter, stream to the ring
- LPR **515**: RFC 1179 receive-job. Control file is read and thrown away.
  Data file is streamed. Control-file-first *or* data-file-first (CUPS
  often sends `df` first). ACK octet is `0x00`. Source ports 721-731 are
  **not** enforced (modern lpr/CUPS will not bind them).
- SRAM is 32 kB. There is no whole-job spool. The ring is 2 kB. TCP
  back-pressure does the rest.

## First bring-up probe

On the first byte of a job (unless dry-run) firmware reads ACK:

- LOW → `ACK idle LOW. KX-R60-class, good to send.`
- HIGH → warning: cable, pulldown, CODE+E, or you picked up a Mac cable.

Then send **one character**. See `docs/bring-up.md`.
