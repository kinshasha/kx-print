# Wiring

Workshop notes. The dangerous pin is Mini-DIN **pin 5 (+12 V)**.

## Isolation of +12 V

```
  Mini-DIN-8 on the typewriter
  ┌─────────────────────────┐
  │  1,2,3  GND             │──── to UNO GND
  │  4      /ACK            │──── to UNO D4
  │  5      +12 V           │──── CUT. TAPE. GO NOWHERE.
  │  6      DATA            │──── to UNO D5
  │  7      /STB            │──── to UNO D6
  │  8      /ONLINE         │──── to UNO D7
  └─────────────────────────┘
```

The 5-pin DIN panel socket is the physical interlock: there is no pin for
+12 V on that connector, so a correctly-made cable cannot carry it.

## 5-pin DIN interconnect (panel ↔ flying cable)

| 5-pin DIN | signal   | Mini-DIN-8 | UNO    |
|-----------|----------|------------|--------|
| 1         | /ACK     | 4          | D4     |
| 2         | GND      | 1+2+3      | GND    |
| 3         | DATA     | 6          | D5     |
| 4         | /STB     | 7          | D6     |
| 5         | /ONLINE  | 8          | D7     |

Donor cable: Cable Matters 5-pin DIN MIDI 1.8 m, **must be 1:1** (all five
pins wired through). Label both ends **KX-R ONLY**.

### MIDI cables often skip pins

Standard MIDI only uses pins 2, 4 and 5. Cheap “MIDI” leads leave 1 and 3
unconnected. Our /ACK and DATA live on DIN 1 and 3. Continuity-test all
five conductors before it ever sees the typewriter.

## Mac printer / null-modem Mini-DIN-8 cables: do not use

Apple printer and some “null-modem” Mini-DIN-8 leads **swap pins**.
That can put **+12 V on a GPIO** (or GND on DATA, etc.).

Use a **straight modem-style** Mini-DIN-8 wiring only, built by you, from
the table above. Continuity-test against the Mini-DIN-8 pin numbers with
the typewriter **unplugged**. Never grab a Mac serial/printer cable out of
the junk box because it “fits”.

## UNO R4 WiFi (Jaycar XC9211)

| UNO pin | direction     | notes |
|---------|---------------|--------|
| D4      | INPUT         | /ACK. **No INPUT_PULLUP.** Typewriter idles ACK LOW with ~10 k pulldown. A pull-up fights it. |
| D5      | OUTPUT        | DATA. Idle HIGH. HIGH = 1, LOW = 0. |
| D6      | OUTPUT        | /STB. Idle HIGH, pulse LOW per bit. |
| D7      | OUTPUT        | /ONLINE. Idle HIGH, LOW for the whole byte. |
| D8      | INPUT_PULLUP  | RUN/PAUSE toggle (ST0570). Closed to GND = RUN. |
| D9      | INPUT_PULLUP  | CANCEL (SP0716). Active low. |
| GND     |               | Shared with typewriter GND. |

Optional 100 Ω series on /ACK, DATA, /STB, /ONLINE (not on GND). Sits on
the HP9556 experimenter board. Limits fault current if something is shorted.

Do **not** add a pull-up on ACK. If you are on the bench with no typewriter
attached, ACK will float. Use serial `dry-run on` so firmware does not wait
for it.

## Power

- UNO: USB-C 5 V, 1 A minimum, 2 A preferred. Dedicated PSU, not a flaky
  hub.
- Typewriter: its own mains lead. Leave it that way.
- Common GND only.

## Controls

- **RUN/PAUSE** (D8): open = PAUSE. Bytes stay in the RAM ring; TCP can
  keep filling until the ring is full.
- **CANCEL** short: drop the current job.
- **CANCEL** hold ~3 s: clear the whole queue.
- **CANCEL** held at power-up: wipe EEPROM Wi-Fi and open `KX-Print-Setup`.
