# jug-filler

Firmware for an automatic water-jug filler: two analog water-level sensors, one
solenoid valve, one status LED — on a Waveshare RP2040-Zero (RP2040) board.
All tunables live in [`firmware/jug_filler/config.h`](firmware/jug_filler/config.h).

## Repo layout

```
firmware/jug_filler/   Arduino sketch (jug_filler.ino + config.h) — the only firmware files
test/                  Native host unit tests (Unity), run without any hardware
platformio.ini         PlatformIO build (board) + host-test (native) configuration
```

## Build in Arduino IDE

1. Install Arduino IDE 2.x.
2. **File → Preferences → Additional boards manager URLs**, add:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
3. **Tools → Board → Boards Manager…**, search for "Raspberry Pi Pico/RP2040"
   (Earle Philhower's arduino-pico core) and install it.
4. **Tools → Board** → select a **Waveshare RP2040-Zero** entry under the RP2040 core.
5. **File → Open** → `firmware/jug_filler/jug_filler.ino` (the folder name must match
   the sketch name — don't move files out of `firmware/jug_filler/`).
6. **Sketch → Upload**.

## Build with PlatformIO

```sh
pio run -e pico        # compile-only check (no upload); first run downloads the toolchain
```

## Run native tests

```sh
pio test -e native     # host g++ + Unity; no board or Arduino toolchain needed
```

First run downloads the native platform and the Unity library — expected, one-time.

## Serial logging

USB CDC serial at 115200, always on (PRD §3) — logging runs with USB
unplugged; lines are dropped, never stalled. Newline-terminated lines:

| Line | Meaning |
| ---- | ------- |
| `JugFiller boot (reset cause: WATCHDOG)` / `(… POWER_OR_RESET)` | boot banner; WATCHDOG = the §10 reset path fired |
| `BTN short 1234ms` / `BTN long 1500ms` | BOOT press, classified on release (hold = raw edge-to-edge, diagnostic) |
| `S1=3500 S2=3498` | 1 Hz commissioning line — latest averaged pair, 12-bit counts (§9.3) |
| `FILL start backstop=30min recovery=0ms` | fill authorized: backstop table minutes + recovery since last fill end (0 = fresh boot) |
| `BASE ok s1=3502 s2=3499` | dry baselines accepted (counts) |
| `VALVE open budget=30min` | valve opened; deadline budget incl. any stacked +5s |
| `TRIP sensor=1 fillms=75000` | first sensor trip; valve-open time |
| `CONFIRM pass sensor=2 durms=3000` | second sensor confirmed within the window |
| `DONE attemptms=81000 openms=75000` | fill complete (press→close / valve-open→close) |
| `ACK done` | DONE acknowledged → IDLE |
| `PLUS5 budget=35min` | +5 min long-press extension (stacks) |
| `VALVE close` | valve closed — any fill end; always precedes the reason line |
| `ABORT attemptms=5000` | fill aborted by short press |
| `FAULT baseline sensor=1 code=1` | baseline out of range (§7.1) |
| `FAULT earlywet sensor=2 code=2` | wet reading inside the 5 s early window (§7.6) |
| `FAULT earlytrip sensor=1 code=1` | trip before 60 s elapsed (§7.6) |
| `FAULT confirm sensor=2 code=2` | other sensor never tripped in the 12 s window (§7.5) |
| `FAULT backstop code=3` | backstop expired, no trip (§7.7) |
| `RGB mirror init FAILED (no PIO SM)` | onboard mirror disabled; external LED unaffected |

Fault codes = LED blink groups (§7): 1 sensor 1, 2 sensor 2, 3 timer. Faults
latch until RESET. Full commissioning log workflow: `docs/COMMISSIONING.md`
(planned).

## Safety note

The **first executable line of `setup()`** drives the valve pin
(`PIN_VALVE`, GP15) LOW so the valve is closed before anything else runs.
Never reorder that line. `setup()`/`loop()` contain no `delay()` and no
`while(!Serial)` anywhere — see the seam-contract comment at the top of
`firmware/jug_filler/jug_filler.ino`.
