# Bring-up

Numbered on purpose. Do not skip ahead to “print War and Peace”.

## 0. Typewriter

1. Mains in, cover off if you like, but you do not need to open it.
2. **CODE + E** until the LCD shows **ON LINE**.
3. Paper in. Margin and pitch wherever you usually live.

## 1. Cable, on the bench, typewriter unplugged from the box

1. Continuity Mini-DIN-8 pin 4 → DIN-5 pin 1 → UNO D4 (/ACK).
2. Continuity Mini-DIN-8 1,2,3 → DIN-5 pin 2 → UNO GND.
3. Continuity Mini-DIN-8 pin 6 → DIN-5 pin 3 → UNO D5 (DATA).
4. Continuity Mini-DIN-8 pin 7 → DIN-5 pin 4 → UNO D6 (/STB).
5. Continuity Mini-DIN-8 pin 8 → DIN-5 pin 5 → UNO D7 (/ONLINE).
6. **Confirm Mini-DIN-8 pin 5 goes nowhere.** Open circuit to every UNO
   pin, including VIN, 5 V, 3.3 V, and every GPIO.
7. Confirm it is not a Mac printer cable (those swap pins; see wiring).
8. Label both DIN ends **KX-R ONLY**.

## 2. Firmware, no typewriter yet

1. Copy `firmware/secrets.h.example` to `firmware/secrets.h` or skip it
   and use the setup AP.
2. `cd firmware && pio run -t upload && pio device monitor -b 115200`
3. Matrix shows **W** (no Wi-Fi yet) or a checkmark if it joined.
4. Serial: `dry-run on` then `print A`
5. You should see `dry  A` (or similar). No ACK needed.

## 3. Wi-Fi

1. If you held CANCEL at reset: AP `KX-Print-Setup`, page at
   `http://192.168.4.1`.
2. Otherwise STA using EEPROM or `secrets.h`.
3. Ping `kxr540.local`. mDNS is `ArduinoMDNS`; some Android phones will
   not resolve `.local`. Use the IP from `status`.

## 4. Idle levels, still no printing

With the cable connected and CODE+E on:

- /ONLINE, /STB, DATA should sit HIGH
- ACK should sit **LOW**
- Serial `status` line `ACK pin LOW (idle OK)`

If ACK is HIGH: wrong cable, pull-up enabled by mistake, not in ON LINE,
or a Mac lead. Stop.

## 5. One character

1. `dry-run off`
2. RUN switch closed (to GND).
3. `print A`
4. Watch the daisywheel. One A. Matrix shows P then a checkmark.

If `!` and `ACK timeout`: probe ACK during the strobe with a logic probe
if you have one. Recheck pin 4. Recheck CODE+E.

## 6. One line

```
printf 'HELLO PANASONIC\r\n' | nc kxr540.local 9100
```

If `nc` does not close the socket, use `nc -N` (OpenBSD) or `nc -q 1`
(GNU). The job ends on TCP close.

## 7. LPR

```
echo 'HELLO PANASONIC' | lpr -H kxr540.local -P kxr540 -o raw
```

CUPS device URI: `lpd://kxr540.local/kxr540`

## 8. Pause and cancel

- Open the toggle mid-line: printing stops, the rest waits in RAM.
- Short CANCEL: that job dies, matrix flashes X.
- Hold CANCEL ~3 s: queue gone.

## 9. Then, and only then, a page

Form feed is 66 lines by default. Do not feed binary, PDF, or PostScript.
It is a typewriter. Send text.
