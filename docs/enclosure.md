# Enclosure — Jaycar HB6013 (130 × 68 × 44 mm)

The UNO R4 WiFi is about 68.6 × 53.4 mm. Sit it **along the 130 mm
length** so the 53.4 mm width lives in the 68 mm span. USB-C is on a
short edge — that edge faces a hole in the end wall.

It is a snug box. File the standoffs if they fight the headers. Do not
force the USB-C connector against ABS.

## Layout (lid off, looking in)

```
          130 mm
   ┌────────────────────────────┐
   │ USB-C hole  →  UNO R4 WiFi │  68 mm
   │                 (matrix up │
   │                  if you    │
   │                  cut a     │
   │                  window)   │
   │  HP9556 strip / 100 Ω      │
   │  PS0348 5-pin DIN here  →  │
   └────────────────────────────┘
         44 mm tall (tight)
```

## Holes

1. **USB-C** — end wall, aligned with the UNO socket. Slot it so you can
   get the connector in after the board is seated. This is also how you
   power it.
2. **5-pin DIN (PS0348)** — opposite end wall, or the long side if the
   USB slot ate the end. Deburr. Nut on the inside.
3. **Toggle ST0570** — lid, left. D8 / GND.
4. **Pushbutton SP0716** — lid, right of the toggle. D9 / GND.
5. Optional **matrix window** — lid, above the UNO’s 12×8. A thin slot or
   just leave the lid translucent and live with the glow.

No hole for Mini-DIN pin 5. There is no +12 V in this box.

## Internals

- UNO on standoffs or a thin foam pad. USB-C must still mate.
- HP9556 as a tiny tag-board for the four 100 Ω resistors if you fitted
  them. Keep GND as a fat wire, not a resistor.
- Strain-relief the DIN wires with a zip-tie around a lid boss.

## Lid labels (paint pen or Dymo)

```
KX-Print          KX-R ONLY
RUN / PAUSE       CANCEL
USB-C 5 V only — not from the typewriter
```

Put the `SAFETY.md` card inside the lid.

## Cable flag

Heatshrink flag on the flying 5-pin DIN:

```
KX-R ONLY
NOT MIDI
```

If it can plug into a synthesiser, someone will.
