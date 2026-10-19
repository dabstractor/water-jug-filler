# Jug Filler: Project Spec

## Purpose
Fill a 4 gallon jug with RO water and shut the valve automatically when the water reaches a set level. No WiFi, no app, no extras. A hard timer is the backstop if the sensors fail.

## Hardware
| Part | Role |
|---|---|
| Arduino Pro Micro (5V/16MHz) | Controller |
| FPD180A solenoid, 12V, ~400mA, normally closed | Water valve |
| Blue MOSFET module | Switches the valve. Gate resistor and pull-down are already on it |
| 1N4148 diode (second optional, in parallel) | Flyback across the solenoid, stripe toward +12V |
| 2x HW-390 v2.0 capacitive sensors | Full-level detection, bare tips directly in the water, mounted in the printed cap at the same depth |
| LM2596 buck converter | 12V to 5V, set and verified with a multimeter before connecting the Pro Micro |
| 12V 1A supply | Feeds the valve and the buck |
| External LED + resistor | Fault blink codes |
| 100uF cap on the 5V rail | Smooths switching noise for cleaner ADC readings |
| Optional reset button | INPUT_PULLUP to GND |

## Power and wiring
- 12V supply goes to the MOSFET module input and the LM2596 input.
- LM2596 5V output goes to the Pro Micro **VCC** pin (not RAW).
- Sensors are powered from Pro Micro VCC (5V).
- All grounds common: supply, buck, MOSFET module, Pro Micro, sensors.
- Never connect USB while the buck is powering the board. Program over USB with the buck disconnected.
- The valve is normally closed, so power loss closes it.

## Proposed pins (agent should justify any changes)
| Pin | Connection |
|---|---|
| A0 | Sensor 1 AOUT |
| A1 | Sensor 2 AOUT |
| D10 | MOSFET TRIG (HIGH = open) |
| D9 | Fault LED |
| D8 | Reset button (optional) |
| D0/D1/D2/D3 | Avoid |

The agent should give a short table of every pin it chose, what is connected, and why, before writing code.

## Sensor behavior
- Analog: **high = dry, low = wet**. Only the tip is covered at full level, so the change will be partial.
- Average 16 to 32 samples per reading.
- Re-read a dry baseline per sensor at the start of every fill. Trip when the reading drops a set amount below baseline, not at a fixed value.
- Debounce: 5 to 10 consecutive averaged readings past the threshold before a sensor counts as tripped.
- **TBD from real readings:** trip drop, plausible baseline range, debounce count. All are named constants at the top of the file.

## Fill logic
1. Start-up check: read both baselines. If either is out of range, fault that sensor and refuse to open the valve.
2. Open the valve and start the fill timer.
3. When either sensor trips, close the valve immediately.
4. Start a short window (~10 to 15s) after the first trip. If the other sensor hasn't tripped by then, fault it.
5. If a sensor reads wet within the first few seconds of a fill, treat it as a false trip or short and fault it.
6. If the timer expires with no trip from either sensor, close the valve and fault "timer, no trip".
7. **Every** code path that ends a fill sets the valve pin LOW.

## Faults
| Blinks | Meaning |
|---|---|
| 1 | Sensor 1 bad |
| 2 | Sensor 2 bad |
| 3 | Timer fired, no trip |

- Long pause between blink groups.
- Faults latch until power cycle or reset button.
- While faulted, the valve refuses to open.

## Safety requirements
- Valve pin set LOW as the very first line of setup().
- No blocking `delay()` in the fill loop. Use `millis()` timing written to survive rollover.
- The timer cutoff must be close to the real fill time, not padded. A dead sensor should behave like the timer fallback, not overflow the jug.

## Implementation notes
- Arduino framework, C/C++, single readable .ino file.
- Do not assume USB and the buck are connected at the same time.
- All tunable values are named constants at the top.

