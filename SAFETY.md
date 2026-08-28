# KX-Print — lid card

Tape this inside the HB6013 lid. Read it before the cable goes in.

## 1. Mini-DIN pin 5 is +12 V. NEVER connect it to the UNO.

Not through a resistor. Not “just to try”. Not even for a second.
The 5-pin DIN interconnect exists so pin 5 cannot sneak through.

## 2. UNO from its own 5 V USB-C (1 A min, 2 A preferred)

Do not power the UNO from the typewriter. Do not share the typewriter’s
+12 V or +5 V rails with the Arduino.

## 3. Share signal GND

Typewriter GND (Mini-DIN 1, 2, 3 combined) must meet UNO GND.
No GND, no return path, garbage ACK, fried luck.

## 4. Continuity-test the cable first

Every conductor. All five pins of the DIN interconnect. See `docs/wiring.md`.
Many MIDI cables omit pins 1 and 3. Those will not work.

## 5. GPIO safe idle before printing

Idle is /ONLINE HIGH, /STB HIGH, DATA HIGH. Firmware does this in `setup()`
before Wi-Fi. If you are probing with a scope, check this first.

## 6. ACK timeout

If ACK never answers, firmware drops to safe idle in 1.5 s, stops the job,
and shows `!` on the matrix. Do not sit there hammering the bus.

## 7. First test is one character

Not a novel. One `A`. Watch it strike. Then a line. Then a page.

## 8. Label the 5-pin DIN **KX-R ONLY**

It looks like MIDI. It is not MIDI. A MIDI box or a Mac printer cable
can put +12 V on a GPIO. Label both ends.

---

Printer mode on the typewriter: **CODE + E**. LCD should show **ON LINE**.
