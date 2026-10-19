# Jug Filler: Project Spec

## Purpose
Fill a 4 gallon jug with RO water and shut the valve automatically when the water reaches a set level. No WiFi, no app, no extras. A hard timer is the backstop if the sensors fail.

## Hardware
| Part | Role |
|---|---|
| Raspberry Pi Pico or other RP2040 board (3.3 V logic, no wireless variant needed) | Controller. Pin names below assume the Pico pinout |
| FPD180A solenoid, 12V, ~400mA, normally closed | Water valve |
| Logic-level MOSFET module (IRLZ44N/IRL520 class) | Switches the valve. Must fully turn on from a 3.3 V gate — see "MOSFET gate drive" below |
| 1N4148 diode (second optional, in parallel) | Flyback across the solenoid, stripe toward +12V |
| 2x HW-390 v2.0 capacitive sensors | Full-level detection, bare tips directly in the water, mounted in the printed cap at the same depth. Powered at 3.3 V |
| LM2596 buck converter | 12V to 5V into the board's VSYS; the board's own regulator makes 3.3 V. Set and verified with a multimeter before connecting the board |
| 12V 1A supply | Feeds the valve and the buck |
| External LED + resistor | Fault/status blink codes |
| 100uF cap on the 5V rail | Smooths switching noise for cleaner ADC readings |
| 100nF cap from each sensor AOUT to GND (recommended) | The RP2040 ADC is noisier than an AVR's; these tame the analog lines |
| Onboard BOOTSEL button | The user control button (start / abort / extend). No external button needed |
| Onboard RESET button, or a momentary RUN-to-GND button on boards without one | Full device reset |

## 3.3 V logic warning
RP2040 GPIO are 3.3 V only and **not** 5 V tolerant. Nothing above 3.3 V may touch any GPIO. This drives three choices: the sensors run at 3.3 V, the board is powered through VSYS at 5 V (its onboard regulator makes the 3.3 V rail), and the MOSFET module must switch from a 3.3 V gate.

## MOSFET gate drive (the one real gotcha of this switch)
The old plan drove the common blue IRF520 module from a 5 V Pro Micro pin. RP2040 GPIO tops out at 3.3 V, and an IRF520 is specified for ~10 V gates: at 3.3 V it may not turn on at all, or may sit half-on and overheat. Use a logic-level module (IRLZ44N / IRL520 class), or bench-prove the existing one before trusting it: gate to 3.3 V, drain switching the actual solenoid at 12 V — it must pull in crisply and the FET must stay cool. If in doubt, replace the module.

The module's onboard pull-down keeps the valve closed while the RP2040 boots, while its pins float during BOOTSEL/flashing, and on any brownout.

## Power and wiring
- 12V supply goes to the MOSFET module input and the LM2596 input.
- LM2596 5V output goes to the Pico **VSYS** pin (pin 39). The Pico's onboard regulator produces 3.3 V for the chip and the **3V3(OUT)** pin (pin 36).
- Sensors are powered from **3V3(OUT)**. At 3.3 V their absolute readings differ from a 5 V build, but the firmware only ever compares against a per-fill dry baseline, so behavior is unaffected.
- All grounds common: supply, buck, MOSFET module, board, sensors.
- On the Pico, VBUS is diode-OR'd with VSYS, so USB and the buck **can** be connected at the same time — useful for serial logging during a real fill. (This was forbidden on the Pro Micro; it no longer is.) Still verify the buck output with a multimeter before the board's first power-up.
- The valve is normally closed, so power loss closes it.

## Proposed pins (agent should justify any changes)
| Pin | Connection |
|---|---|
| GP26 / ADC0 | Sensor 1 AOUT |
| GP27 / ADC1 | Sensor 2 AOUT |
| GP15 | MOSFET TRIG (HIGH = open) |
| GP14 | Fault/status LED |
| GP25 | Onboard LED, mirrors the external LED (free debugging) |
| BOOTSEL | Control button (start/abort/extend). The arduino-pico core reads it with `pinMode`/`digitalRead`; it cannot enter the bootloader unless RESET is held at the same time |

Avoid / keep free:
- **GP29 / ADC3** — wired to the Pico's VSYS divider; reads garbage on this board.
- **GP23 / GP24** — Pico SMPS-mode and VBUS-sense lines.
- **GP0 / GP1** — reserved for a debug UART during bring-up.

The agent should give a short table of every pin it chose, what is connected, and why, before writing code.

## Sensor behavior
- Analog: **high = dry, low = wet**. Only the tip is covered at full level, so the change will be partial.
- The RP2040 ADC is 12-bit (0-4095) at 3.3 V and noisier than an AVR's: average **32** samples per reading (16 minimum).
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
- Enable the RP2040 hardware watchdog (~8s), fed every loop pass. A hung board resets; setup()'s first line then closes the valve, and the device comes back idle waiting for a human start — it never auto-restarts a fill after any reset.
- No blocking `delay()` in the fill loop. Use `millis()` timing written to survive rollover.
- The timer cutoff must be close to the real fill time, not padded. A dead sensor should behave like the timer fallback, not overflow the jug.

## Implementation notes
- Arduino framework on the **arduino-pico (Earle Philhower) core**, C/C++, single readable .ino file.
- The RP2040 has no EEPROM; if any state must survive power loss it goes to flash (the core offers EEPROM emulation). Whether any state is persisted is a behavioral decision.
- USB and the buck may be connected simultaneously (see Power and wiring).
- All tunable values are named constants at the top.
