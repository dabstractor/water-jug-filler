# Jug Filler — Technical Specification

## 0. Summary
Fill a 4 gallon jug with RO water and shut the valve automatically when the water reaches a set level. No WiFi, no app, no persistence. The valve only ever opens in response to a BOOT button press from a human. A hard timer backstop — computed from how recovered the RO tank is — bounds the fill if the sensors fail.

## 1. Hardware / BOM
| Part | Role |
|---|---|
| Waveshare RP2040-Zero (RP2040, 2MB flash, 3.3 V logic) | Controller. Onboard BOOT + RESET buttons (both used). Onboard WS2812 RGB LED on GP16 — the sole status/fault indicator (§4). Exposed pins: GP0–GP15 and GP26–GP29. **GP16–GP25 exist only as microscopic back pads — treat as unusable.** |
| FPD180A solenoid, 12V, ~400mA, normally closed | Water valve. NC = power loss closes it |
| Logic-level MOSFET module (IRLZ44N / IRL520 class) | Switches the valve from a 3.3 V gate — see §2. Gate pull-down is on the module |
| 1N4148 diode (second optional, in parallel) | Flyback across the solenoid, stripe toward +12V |
| 2x HW-390 v2.0 capacitive sensors | Full-level detection, bare tips in the water at the same depth in the printed cap. Powered at **3.3 V** |
| LM2596 buck converter | 12V→5V into the board's 5V pin; the board's LDO makes 3.3 V. Set and verify with a multimeter before connecting the board |
| 12V 1A supply | Feeds the valve and the buck |
| 100uF electrolytic on the 5V rail | Decoupling for cleaner ADC readings |
| 100nF ceramic from each sensor AOUT to GND | Tames the analog lines into the RP2040 ADC |

## 2. Wiring, pins, power
### 2.1 Canonical pin map
| Board pin | Connects to | Notes |
|---|---|---|
| **GP26 / ADC0** | Sensor 1 AOUT | GP26–GP29 are the RP2040's only ADC-capable pins (ADC0–ADC3) |
| **GP27 / ADC1** | Sensor 2 AOUT | |
| **GP15** | MOSFET module TRIG/SIG | HIGH = valve open |
| **3V3 pin** | Sensor VCC (both sensors) | Never 5V — GPIO/ADC are 3.3V-only, not 5V tolerant |
| **5V pin** | LM2596 5V output | Board makes its own 3.3V |
| **GND** | All grounds common: 12V supply, buck, MOSFET module, sensors | |
| BOOTSEL (onboard, no wiring) | Control button | Read in firmware via arduino-pico's `BOOTSEL` pseudo-pin (`pinMode(BOOTSEL, INPUT_PULLUP)`, `digitalRead`). Cannot enter bootloader unless RESET is held too |
| RESET (onboard, no wiring) | Full device reset | |
| GP16 (onboard, no wiring) | WS2812 RGB LED — sole status/fault indicator | Driven in firmware via a NeoPixel library (e.g. Adafruit_NeoPixel, supported on arduino-pico) |

Spares: GP28/GP29 (ADC2/ADC3), GP14, and GP2–GP13, all unwired. Reserved: GP0/GP1 (UART0 TX/RX) for debug serial. Unusable: GP16–GP25 (back pads).

### 2.2 Power path & cautions
- 12V → MOSFET module power-input terminals and LM2596 input. Buck 5V → board **5V** pin. Sensors from **3V3**.
- **Never connect USB while the buck is powering the board.** On the RP2040-Zero the 5V pin (VSYS) ties directly to USB VBUS with no diode: connecting both backfeeds one supply into the other (PC USB port damage risk). Bench/commissioning runs are USB-powered with the buck disconnected; in service the board runs from the buck with USB unplugged. Program over USB with the buck disconnected.
- The buck's 5V feeds the board's LDO; total 3.3V load (board + sensors + the single onboard WS2812, ≤60mA at full white) is well within it. Verify the buck output with a multimeter (4.9–5.1V) before the board's first power-up.
- **MOSFET module wiring (two-terminal-block module):** 12V+ / 12V− → the module's power-input block; solenoid wires → the switched output block; header TRIG/PWM → GP15; header GND → GND. Confirm input vs output blocks from the silkscreen and a multimeter before connecting the solenoid — the output must be dead until TRIG is driven high.
- **MOSFET gate drive requirement:** an IRF520-class "blue module" is NOT guaranteed on at a 3.3V gate. Use a logic-level module, or qualify the module in use by test: with TRIG at 3.3V and the real solenoid at 12V, the module's indicator LED must light, the valve must pull in crisply, and the FET must stay cool.
- The module's onboard gate pull-down keeps the valve closed while pins float during boot/BOOTSel/flashing/brownout.
- Route the sensor AOUT wires away from the valve/solenoid wiring to keep switching noise out of the analog readings.

### 2.3 Complete wiring netlist

```
12V+ ──┬── LM2596 IN+
       └── MOSFET module power-in block (+)
12V− ──┬── LM2596 IN−                 [every − point below = one GND bus]
       └── MOSFET module power-in block (−)

LM2596 OUT+ (5.0V) ── board "5V" pin
LM2596 OUT−      ── board GND
100µF electrolytic: + leg → board 5V rail, stripe(−) leg → GND rail, near the board

MOSFET OUT+ screw ── solenoid lead "+" (red if marked)
MOSFET OUT− screw ── solenoid other lead
1N4148 across solenoid leads: STRIPE → the "+" (OUT+) side   [2nd optional, parallel, same way]
MOSFET TRIG/PWM (a.k.a. SIG/S) ── GP15
MOSFET header GND ── GND bus

3V3 ──┬── sensor 1 VCC
      └── sensor 2 VCC
GP26 ── sensor 1 AOUT ──(100nF "104" to GND)
GP27 ── sensor 2 AOUT ──(100nF "104" to GND)
GND bus ── sensor 1 GND, sensor 2 GND        (DOUT pins: nothing)
```

Numbered connections:
| # | From | To |
|---|---|---|
| 1 | 12V supply + | LM2596 IN+ |
| 2 | 12V supply + | MOSFET power-in block, + screw |
| 3 | 12V supply − | LM2596 IN− |
| 4 | 12V supply − | MOSFET power-in block, − screw |
| 5 | LM2596 OUT+ (5.0V, pre-verified) | Board 5V pin |
| 6 | LM2596 OUT− | Board GND |
| 7 | 100µF + leg / stripe leg | 5V rail / GND rail (near board) |
| 8 | MOSFET OUT+ screw | Solenoid "+" lead (red if marked) |
| 9 | MOSFET OUT− screw | Solenoid other lead |
| 10 | 1N4148 stripe end | Solenoid "+" lead (OUT+ side); other end to the other lead. 2nd diode optional, parallel, same orientation |
| 11 | MOSFET TRIG/PWM header | GP15 |
| 12 | MOSFET GND header | GND bus |
| 13 | Board 3V3 | Sensor 1 VCC |
| 14 | Board 3V3 | Sensor 2 VCC |
| 15 | Sensor 1 GND | GND bus |
| 16 | Sensor 2 GND | GND bus |
| 17 | Sensor 1 AOUT | GP26 |
| 18 | Sensor 2 AOUT | GP27 |
| 19 | 100nF #1 ("104") | Sensor 1 AOUT ↔ GND |
| 20 | 100nF #2 ("104") | Sensor 2 AOUT ↔ GND |
| 21 | Sensor DOUT pins (both) | Unconnected |
| 22 | Board pins wired, total | GP26, GP27, GP15, 3V3, 5V, GND — all others unconnected (the WS2812 on GP16 is onboard; no wiring) |

Orientation rules: 1N4148 stripe to the +12V side (reversed = short across the supply when the valve opens); 100µF stripe to GND (reversed = vents); 100nF have no polarity; the solenoid itself is polarity-insensitive — the flyback diode defines the "+" lead. Any board GND pin may be used (same net). Verify the module's IN vs OUT blocks from silkscreen and multimeter before connecting the solenoid: OUT must be dead until TRIG is driven.

## 3. Firmware platform
- **arduino-pico (Earle Philhower) core**, board selection "Waveshare RP2040-Zero". C/C++. Two files: the sketch (`.ino` or `src/main.cpp`) + **`config.h`** holding every constant in §8.
- PlatformIO layout optional (platform `raspberrypi`, board `waveshare_rp2040_zero`); must also build in Arduino IDE with just the core installed.
- USB CDC serial at 115200 for logging (state transitions, raw sensor counts, backstop computations); always on. Bench sessions run on USB power with the buck disconnected (§2.2).
- **No persistence of any kind.** No EEPROM, no flash writes. All state is RAM; any reset starts fresh (see §5.3).

## 4. States and LED contract
Five states. The onboard WS2812 RGB LED (GP16) is the **sole** status/fault indicator — there is no external LED. Driven in firmware via a NeoPixel library; colors are config constants.

| State | Meaning | WS2812 output |
|---|---|---|
| **IDLE** | Powered, waiting for BOOT press | Off |
| **FILLING** | Valve open, timer running, watching sensors | Solid blue |
| **CONFIRM** | First sensor tripped, valve closed, waiting out the confirm window for the second sensor | Solid blue (continues) |
| **DONE** | Fill complete; attention pattern until acknowledged | **Green triple-pulse attention blink**: 3x (on 200ms / off 200ms), then off 1000ms, repeat |
| **FAULT** | Latched fault; see §7 | Red blink-code groups (§7) |

Transient ack: a +5min long-press during FILLING flashes white 3x quickly (on 100ms / off 100ms), then resumes solid blue.

## 5. Button & reset semantics
### 5.1 BOOT button — acts on RELEASE
Contact debounce 40ms; a press is "long" if held ≥ 1000ms (`BUTTON_LONG_PRESS_MS`). Both short and long actions fire on release.

| State | Short press (< 1s) | Long press (≥ 1s) |
|---|---|---|
| IDLE | **Start a fill** → FILLING | Nothing |
| FILLING | **Abort**: valve closed → IDLE | **+5 min** to this fill's backstop timer (stacks; repeatable) + LED ack |
| CONFIRM | Ignored | Ignored |
| DONE | **Acknowledge**: light off → IDLE | Also acknowledges |
| FAULT | Ignored | Ignored |

### 5.2 Interaction flows (normative)
**Flow 1 (abort):** plug in → BOOT press 1: fill starts → BOOT press 2 mid-fill: fill stops → BOOT press 3: fill starts again with a fresh, recomputed timer.
**Flow 2 (success):** plug in → BOOT press 1: fill starts → fill completes: green triple-pulse blink → BOOT press 2: blinking stops → BOOT press 3: next fill starts.

### 5.3 RESET button / power cycle
Full reboot. Always lands in IDLE, valve closed, **tank history cleared — next fill uses the 30-min fresh-tank backstop** (see §6). The device never auto-starts a fill after any reset, watchdog reset included.

## 6. Timer model (the backstop)
The backstop bounds how long the valve may stay open. Sensors normally end fills far earlier; the backstop exists for sensor failure, so it must hug the real fill time — **never padded**.

### 6.1 Recovery clock (RAM only)
- The clock's reference point is the moment the previous fill's valve closed (fill end, whatever ended it).
- Recovery time for a new fill = wall-clock time since that reference.
- Fresh boot / any reset: no history → the next fill uses `FRESH_BOOT_BACKSTOP_MIN = 30` (assume full tank: high pressure, fast fill).
- Abort: the recovery clock restarts at the abort instant, exactly like a normal fill end.

### 6.2 Backstop table — concave on purpose
Physics: the tank refills fast at first (low back-pressure) and progressively slower as pressure builds. So the backstop must fall steeply at short recovery times and flatten near the full-tank value. A straight line pads the mid-range (e.g. a fill started ~15 min after the last one ends) in exactly the wrong direction.

`BACKSTOP_TABLE_MIN` (piecewise-linear between points, clamped at both ends):
| Recovery since last fill end | Backstop |
|---|---|
| ≤ 2 min | 44 min |
| 10 min | 41 min |
| 20 min | 38 min |
| 35 min | 34 min |
| ≥ 60 min | 30 min |

- Every fill's backstop is computed once, at fill start. A long-press +5 modifies the active fill's deadline only.

## 7. Fill lifecycle & faults
1. On fill start (BOOT short-press from IDLE): settle `BASELINE_SETTLE_MS` (2s), read both sensors' dry baselines (median of averaged readings). If either is outside `BASELINE_MIN..MAX` → fault that sensor (also catches "jug still full / tips wet" accidental starts). Refuse to open the valve.
2. Open the valve, start the fill timer (backstop from §6), LED solid.
3. **Trip rule:** an averaged reading at or below (baseline − `TRIP_DROP_COUNTS`) for `DEBOUNCE_COUNT` consecutive readings = that sensor tripped.
4. First sensor trips → close the valve immediately → CONFIRM.
5. CONFIRM window `CONFIRM_WINDOW_S` (12s): the other sensor must trip too, else fault that sensor.
6. Any trip accepted before `TRIP_MIN_ELAPSED_S` (60s) of fill time, or any sensor reading wet during the first `EARLY_WET_WINDOW_S` (5s), is a false trip / short → fault that sensor.
7. Backstop expires with no trip → fault "timer, no trip".
8. CONFIRM passes → DONE: recovery-clock reference set to the valve-close instant; green attention blink until acknowledged.
9. **Every code path that ends a fill sets the valve pin LOW first.** The valve is only ever HIGH while in FILLING, and FILLING is only entered by a BOOT short-press from IDLE.

| Fault code (blink groups) | Meaning |
|---|---|
| 1 | Sensor 1 bad (baseline out of range, false trip, never tripped in confirm window) |
| 2 | Sensor 2 bad (same) |
| 3 | Timer fired, no trip |

- Fault cadence (red, WS2812): N x (on 250ms / off 250ms), off 2000ms, repeat.
- **Faults latch.** Cleared only by RESET or power cycle (which also clears tank history — next fill defaults to 30 min). BOOT is ignored while faulted.
- While faulted, the valve refuses to open.

## 8. config.h contract — every tunable, one place
All pins, timings, thresholds, and patterns live in `config.h`, grouped, commented with units and meaning. Defaults:

```cpp
// ---- Pins (RP2040-Zero) ----
PIN_SENSOR1 = GP26; PIN_SENSOR2 = GP27;   // AOUT, ADC0/ADC1
PIN_VALVE   = GP15;                        // HIGH = open

// ---- Button ----
BUTTON_LONG_PRESS_MS = 1000; BUTTON_DEBOUNCE_MS = 40;

// ---- Fill backstop model ----
FRESH_BOOT_BACKSTOP_MIN = 30;
BACKSTOP_TABLE_MIN = { {2,44},{10,41},{20,38},{35,34},{60,30} };
PLUS5_MIN = 5;                             // long-press extension, FILLING only

// ---- Windows ----
CONFIRM_WINDOW_S = 12; EARLY_WET_WINDOW_S = 5; TRIP_MIN_ELAPSED_S = 60;

// ---- ADC / sensors (provisional; set during commissioning — §9) ----
ADC_SAMPLES_PER_READING = 32; READING_INTERVAL_MS = 100;
BASELINE_SETTLE_MS = 2000;
BASELINE_MIN = 2000; BASELINE_MAX = 4050;  // plausible dry range, 12-bit counts
TRIP_DROP_COUNTS = 350; DEBOUNCE_COUNT = 8;

// ---- Onboard WS2812 status LED (GP16) ----
NEO_PIN = 16;                              // onboard RGB LED, sole indicator
RGB_BRIGHTNESS = 180;                      // single pixel ≤60mA at full white
COLOR_FILLING = (0,0,255); COLOR_DONE = (0,255,0); COLOR_FAULT = (255,0,0); COLOR_ACK = (255,255,255);
DONE_PULSE_{ON,OFF}_MS = 200; DONE_PULSE_COUNT = 3; DONE_GAP_MS = 1000;
FAULT_BLINK_MS = 250; FAULT_PAUSE_MS = 2000; ACK_BLINK_MS = 100;

// ---- Platform ----
SERIAL_BAUD = 115200; WATCHDOG_TIMEOUT_S = 8;
```

## 9. Commissioning & calibration
1. **Dry bench check:** flash the firmware; verify IDLE state (LED off), BOOT press logging (BOOTSEL reads), and that the WS2812 renders the §4 patterns. Dry-fire the valve with short clicks (<5s each to avoid overheating the solenoid).
2. **Power rails:** bench sessions are USB-powered with the buck disconnected from the board (12V may feed the MOSFET module alone for valve tests). With the buck powering the 5V pin (USB unplugged), confirm 4.9–5.1V at the 5V pin. Never USB and buck together (§2.2).
3. **Sensor calibration (sets the provisional constants above):** sensors in open air → log dry baselines (expect ~3000–3900 counts at 3.3V); tips submerged to mount depth → log wet values. Set `BASELINE_MIN/MAX` around the observed dry range and `TRIP_DROP_COUNTS` at roughly half the observed dry→wet delta.
4. **Dry-run flows 1 and 2** (§5.2) with sensors dipped in a cup by hand.
5. **Attended first fill:** log actual fill durations vs recovery times; set `BACKSTOP_TABLE_MIN` accordingly.

## 10. Safety invariants (non-negotiable)
- Valve pin driven LOW as the very first line of `setup()`.
- The valve opens only in FILLING; FILLING is entered only by a human BOOT press. No path — boot, watchdog reset, fault recovery, button — ever opens the valve automatically.
- RP2040 hardware watchdog (~8s), fed every loop pass. A hang resets → valve closed → IDLE awaiting a press.
- No blocking `delay()` in the fill loop; all timing via rollover-safe `millis()` arithmetic.
- Backstops hug real fill times (§6). A dead sensor must behave like the timer fallback, not overflow the jug.

## 11. Accepted limitations
- **No persistence:** reset/power loss wipes tank history; next fill defaults to the 30-min backstop.
- **Jug removed mid-fill is undetectable** before the first sensor trip (tips are dry either way; no flow meter). The fill runs until the backstop faults. Removal during the confirm window only matters after the valve already closed — not handled.
- **Done-blink persists until acknowledged** — no timeout; only a BOOT acknowledgment (or reset) clears it.
- Faults clear only via RESET/power cycle.
