# Research notes — P1.M7.T2.S1 (bench validation runbook)

Physical bench task. Research goal: give an operator (human or agent with the
assembled hardware + USB + multimeter) a procedure whose every step has an
EXACT expected serial line / LED cadence, plus safe fault-injection methods.

## 1. Firmware observability — normative log lines (verified in source)

From `logFillEvent()` in `firmware/jug_filler/jug_filler.ino` (~line 1377) and
README.md "Serial logging" (normative table):

| Line (printf shape) | Emitted when |
|---|---|
| `JugFiller boot (reset cause: POWER_OR_RESET)` / `(… WATCHDOG)` | setup(), once per boot/reset |
| `BTN short <ms>ms` / `BTN long <ms>ms` | accepted BOOT release (any state) |
| `S1=<n> S2=<n>` | 1 Hz commissioning line, latest averaged pair (12-bit counts) |
| `FILL start backstop=<m>min recovery=<ms>ms` | fill authorized (recovery 0 = fresh boot) |
| `BASE ok s1=<n> s2=<n>` | dry baselines accepted |
| `FAULT baseline sensor=<n> code=<n>` | §7.1 baseline out of range (1-based) |
| `VALVE open budget=<m>min` | valve opened (budget incl. stacked +5s) |
| `TRIP sensor=<n> fillms=<ms>` | first sensor trip |
| `CONFIRM pass sensor=<n> durms=<ms>` | second sensor confirmed |
| `DONE attemptms=<ms> openms=<ms>` | fill complete |
| `ACK done` | DONE acknowledged → IDLE |
| `PLUS5 budget=<m>min` | long-press +5 during FILLING |
| `VALVE close` | any fill end; ALWAYS precedes the reason line |
| `ABORT attemptms=<ms>` | short-press abort |
| `FAULT earlywet sensor=<n> code=<n>` | wet inside 5 s of open |
| `FAULT earlytrip sensor=<n> code=<n>` | trip before 60 s elapsed |
| `FAULT backstop code=3` | backstop expired, no trip |
| `FAULT confirm sensor=<n> code=<n>` | other sensor never tripped in 12 s |

## 2. Timing landmines (config.h — all verified)

- `BASELINE_SETTLE_MS=2000`: press → ~2 s → `BASE ok` → `VALVE open`.
- `EARLY_WET_WINDOW_S=5`: any wet reading in first 5 s after open → earlywet fault.
- `TRIP_MIN_ELAPSED_S=60`: a trip accepted before 60 s of fill → earlytrip fault.
  ⇒ Flow 2 hand-dips are only legal ≥ ~65 s after `VALVE open`.
- `DEBOUNCE_COUNT=8` @ `READING_INTERVAL_MS=100`: hold a dip ≥ ~1.5–2 s.
- `CONFIRM_WINDOW_S=12`: second dip must complete within 12 s of first trip.
- LED cadences (§4/§7): DONE = 3×(200 on/200 off)+1000 off (period 2200 ms);
  FAULT = N×(250 on/250 off), 2000 ms pause; ack = 3×(100/100); IDLE off;
  FILLING/CONFIRM solid. WS2812 mirror: blue FILLING/CONFIRM, green DONE, red FAULT.
- `FRESH_BOOT_BACKSTOP_MIN=30`; table `{2,44},{10,41},{20,38},{35,34},{60,30}`.
  RecoveryClock::recordFillEnd() fires on done/abort/fault (verified ~line 637:
  "Every fill end — done, abort or fault"). ⇒ Flow 1 restart within <2 min
  shows `FILL start backstop=44min recovery=<nonzero>ms` — that IS the
  "recomputed timer" evidence (30 → 44, recovery > 0).
- Watchdog: `watchdog_enable(8000, true)` LAST in setup(); `pause_on_debug=true`
  pauses ONLY under an attached debugger — a compiled-in hang on a bench with
  no debugger attached DOES trip at ~8 s. Watchdog reset = fresh boot → banner
  `WATCHDOG`, tank history cleared (recovery=0ms, backstop=30), IDLE, no auto-fill.

## 3. Flow-2 vs §9.1 "<5 s dry-fire" conflict — RESOLUTION

Flow 2 requires ≥60 s of valve-open before a legal dip (TRIP_MIN_ELAPSED_S),
but §9.1 caps solenoid dry-fire at <5 s/click. Resolution: run ALL §9.4 flow
dry-runs and fault paths with the 12 V feed to the MOSFET module DISCONNECTED
(firmware/log/LED behavior identical; the module TRIG header still shows gate
state via its indicator LED if powered separately — or leave the module fully
unpowered and rely on logs+GP14 LED). Dedicated §9.1 click tests run separately
with 12 V connected, each energization aborted within ~2–3 s.

## 4. Wet-at-start fault injection — range uncertainty

Provisional `BASELINE_MIN=2000` may not fault a merely-damp sensor (wet HW-390
often reads ~2200–2800 counts, inside 2000..4050). hardware_validation.md:
sensor values UNVERIFIED for the pair in hand — §9.3 calibration exists for
this. Deterministic injection for the fault path: jumper the sensor AOUT to
GND (reads ~0 < 2000) or to 3V3 (4095 > 4050) — guaranteed out-of-range
baseline. Record actual wet counts as a calibration finding either way.

## 5. Power rules (§2.2 + architecture/hardware_validation.md — BINDING)

- RP2040-Zero 5V pin (VSYS) ties DIRECTLY to USB VBUS, no diode (Waveshare
  FAQ, verified) ⇒ NEVER USB + buck together. Bench = USB, buck disconnected
  from the board; 12 V may feed the MOSFET module alone; buck check (4.9–5.1 V
  at 5V pin) happens with USB unplugged. Program over USB with buck disconnected.
- Confirm MOSFET module input vs output blocks (silkscreen + multimeter) before
  connecting the solenoid; output must be dead until TRIG high.
- Sensors from 3V3 only (GPIO/ADC NOT 5 V tolerant).

## 6. Flashing & serial capture (verified against repo + docs)

- `[env:pico]` in platformio.ini is COMPILE-ONLY ("No upload from this project
  yet") → flash via UF2 drag-drop: `pio run -e pico` → `.pio/build/pico/firmware.uf2`
  → hold BOOTSEL while plugging USB → `RPI-RP2` mass-storage drive → copy UF2 →
  auto-reboots into the app. (Waveshare wiki documents the same path.)
- Monitor: `pio device monitor` (115200, USB CDC). `--filter time` prepends
  wall-clock timestamps (built-in filter). Capture: pipe through `tee`.
  USB CDC re-enumerates after flash → start monitor, then press RESET for a
  fresh `POWER_OR_RESET` banner (RESET itself lands IDLE/valve-closed, §5.3).
- Temporary watchdog-hang build WITHOUT touching platformio.ini:
  `PLATFORMIO_BUILD_FLAGS="-DBENCH_HANG_TEST" pio run -e pico` (PIO env var,
  documented) + a temporary `#ifdef BENCH_HANG_TEST` block in loop(), reverted
  immediately after (git diff must end clean).

## 7. Input contracts (previous items)

- P1.M6.T2.S1 (Complete): full loop wiring; README "Serial logging" table.
- P1.M7.T1.S1 (Implementing, parallel — treat as contract): pre-bench gate
  `pio test -e native && python3 test/check_setup_first_line.py` both exit 0.
  Its `test_reset_midfill_lands_idle_closed` natively proxies the watchdog
  reset behavior this task confirms physically.

## 8. URLs

- https://www.waveshare.com/wiki/RP2040-Zero — pinout, UF2 flash, VSYS/VBUS FAQ
- https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html — monitor
- https://docs.platformio.org/en/latest/core/userguide/device/monitoring/filters.html — `time` filter
- https://docs.platformio.org/en/latest/core/envvars.html — PLATFORMIO_BUILD_FLAGS
- https://docs.platformio.org/en/latest/platforms/raspberrypi.html — RP2040 upload paths
- README.md "Serial logging" (in-repo, normative)
