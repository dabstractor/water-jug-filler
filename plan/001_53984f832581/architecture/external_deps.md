# External Dependencies — arduino-pico core & toolchain (VERIFIED findings)

> Method note: async research subagents were spawned first; both were killed by an
> infrastructure failure mid-run ("stale-run reconciliation"). Their partial evidence
> was salvaged from run logs, and every BINDING item below was then re-verified
> directly from primary sources (official docs + core source on GitHub) by the
> architect. Sources listed per item.

## arduino-pico core APIs

### 1. BOOTSEL button — VERIFIED with a **BINDING PRD CORRECTION**
- PRD §2.1 prescribes `pinMode(BOOTSEL, INPUT_PULLUP)` + `digitalRead(BOOTSEL)`.
  **This pseudo-pin API does not exist in arduino-pico and would silently
  configure/read GPIO 0–1** (the object converts to bool → int pin number).
- Verified from core source `cores/rp2040/Bootsel.cpp`: `BOOTSEL` is a **global
  `__Bootsel` object** whose `operator bool()` returns **`true` when the button is
  pressed** (it momentarily floats the QSPI flash-CS line and reads the level,
  with interrupts parked and the other core idled — safe to call from the main loop).
- Correct pattern (also the pattern in the official docs example, bootsel.html):
  ```cpp
  bool pressed = BOOTSEL;      // true == pressed; no pinMode needed
  if (BOOTSEL) { /* pressed */ }
  ```
- Bootloader entry requires BOOTSEL held **during RESET**; runtime reads can never
  enter the USB bootloader — matches PRD intent. Works on the RP2040-Zero because
  its BOOT button follows the standard QSPI-CS circuit.
- Sources: https://arduino-pico.readthedocs.io/en/latest/bootsel.html ,
  https://github.com/earlephilhower/arduino-pico/blob/master/cores/rp2040/Bootsel.cpp

### 2. Hardware watchdog — VERIFIED
- pico-sdk `hardware/watchdog.h` is shipped inside arduino-pico and includable from
  a sketch:
  ```cpp
  #include <hardware/watchdog.h>
  watchdog_enable(8000, true);   // ms, pause_on_debug
  watchdog_update();             // feed
  bool watchdog_caused_reboot(); // boot-cause for the banner log
  ```
- **RP2040 maximum timeout = 8388 ms** (≈8.3 s, erratum RP2040-E1) — verified in the
  pico-sdk header comment. PRD's `WATCHDOG_TIMEOUT_S = 8` (8000 ms) is legal but
  close to the ceiling: do not raise it.
- There is no separate Arduino-level `wdt_*` API in this core; use the pico-sdk calls.
- Sources: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_watchdog/include/hardware/watchdog.h

### 3. ADC — VERIFIED, **second binding correction**
- `analogRead(pin)` returns 0…4095 **only after `analogReadResolution(12)`**;
  **the default resolution is 10-bit (0…1023)** — verified in the official analog
  docs. PRD §8 constants (BASELINE_MIN=2000, BASELINE_MAX=4050, TRIP_DROP_COUNTS=350)
  are 12-bit counts; **without the explicit resolution call every dry baseline would
  read ~750–980 and every fill would baseline-fault.**
  ```cpp
  void setup() { analogReadResolution(12); ... }   // REQUIRED
  ```
- `analogReadResolution(int bits)` exists (docs, Analog Input section); 3.3 V
  nominally reads 4095. `analogReadTemp(vref)` also exists (unused here).
- Pin map: A0=GP26/ADC0, A1=GP27/ADC1, A2=GP28, A3=GP29 — GP26–GP29 are the only
  ADC-capable GPIOs. Matches PRD §2.1.
- Source: https://arduino-pico.readthedocs.io/en/latest/analog.html

### 4. millis() rollover — VERIFIED (standard)
- `millis()` is `uint32_t`, wraps at ~49.7 days. Rollover-safe arithmetic
  `(uint32_t)(now - since) >= interval` is the standard pattern; all timed logic
  must take `nowMs` as an injected parameter (see firmware_architecture.md seam).
- Source: https://arduino-pico.readthedocs.io/en/latest/millis.html

### 5. Onboard WS2812 (GP16) — VERIFIED (library), method names to confirm at impl
- arduino-pico bundles a PIO-based `WS2812` library (`libraries/WS2812`):
  `WS2812(pin, count, pio)` with `begin()` / pixel-set / `show()` methods.
  Adafruit_NeoPixel also works on this core. Exact pixel-method names
  (`set_pixel_color` vs `fill`) vary by core version — confirm against the
  installed core during P1.M5.T3.S1; brightness must be enforced **by scaling the
  RGB values written** (RGB_BRIGHTNESS ≤ 60/255) rather than relying on a library
  brightness API.
- Source: https://github.com/earlephilhower/arduino-pico/tree/master/libraries/WS2812

### 6. USB CDC Serial when unplugged — POLICY (partially verified)
- With native USB CDC, `Serial.begin(115200)` is always safe; **never** gate the
  loop on `while(!Serial)` — in service the board runs from the buck with USB
  unplugged (PRD §2.2) and the fill logic must run regardless.
- Keep log writes small and line-oriented so CDC TX buffers never saturate when no
  host is draining them; if a stall is ever observed on hardware, drop to
  log-on-change (already the design) — P1.M1.T2.S2 owns this policy.
- Source (general behavior): https://arduino-pico.readthedocs.io/en/latest/serial.html

### 7. Board support — VERIFIED (Arduino IDE), PARTIAL (PlatformIO board id)
- Arduino IDE: arduino-pico's board menu includes the Waveshare RP2040-Zero
  variants (PRD §3 compatible). Source: arduino-pico boards.txt / readme.
- PlatformIO: platform `raspberrypi` provides `waveshare_rp2040_zero` (board
  definition present in the platform's boards catalog; final id resolution is
  verified at P1.M1.T1.S2 build time — fallback: the generic `pico` board def,
  which is pin-compatible for the pins used here: GP14/15/26/27 are identical).
- Sources: https://github.com/earlephilhower/arduino-pico ,
  https://registry.platformio.org/search?q=waveshare_rp2040_zero

### 8. GPIO state during boot/flashing — VERIFIED (design-consistent)
- During power-up, brownout, BOOTSEL/UF2 flashing and reset, RP2040 GPIOs are
  Hi-Z/floating. The MOSFET module's onboard gate pull-down therefore holds the
  valve closed while the pin floats — exactly the property PRD §2.2 relies on.
  The firmware invariant "valve LOW as line 1 of setup()" closes the remaining gap.
- Source: RP2040 datasheet (GPIO section); see hardware_validation.md.

## Native host testing pattern (RECOMMENDED — one seam)
- Tooling: PlatformIO `native` platform + bundled **Unity** framework;
  `pio test -e native` runs on the host with plain g++ — no Arduino toolchain,
  no hardware. This is a standard, documented PlatformIO feature.
- Seam (details in firmware_architecture.md): the sketch is plain C++ with explicit
  prototypes; all hardware-touching code (`setup`, `loop`, `pinMode`, `digitalRead`,
  `analogRead`, `Serial`, `WS2812`, watchdog) lives inside `#ifdef ARDUINO`;
  all behavioral logic (state machine, button classifier, backstop math, trip
  detectors, LED pattern engine, time utilities) is unguarded and takes an injected
  `uint32_t nowMs` plus input structs. Test files under `test/` do
  `#include "../firmware/jug_filler.ino"` and exercise the logic with scripted
  clocks and sensor streams.
- Alternatives (ArduinoFake / arduino-mock / arduino_ci) were rejected: they mock
  the Arduino API surface, which is unnecessary when the seam makes logic
  API-independent, and they add dependency weight to a two-file firmware project.
- Source: https://docs.platformio.org/en/latest/advanced/unit-testing/

## Recommended repo layout (ONE recommendation)
```
water-jug-filler/
├── firmware/
│   ├── jug_filler.ino     # the sketch: ARDUINO-guarded glue + ALL pure logic
│   └── config.h           # every §8 tunable (constexpr, grouped, unit-commented)
├── test/                  # native Unity tests: #include "../firmware/jug_filler.ino"
│   └── test_*.cpp
├── platformio.ini         # [env:pico] raspberrypi + [env:native]
├── docs/                  # COMMISSIONING.md (P2), bench/ evidence logs (P1.M7)
├── spec/SPEC.md           # PRD source of truth (read-only)
└── README.md
```
- Rationale: the `firmware/` folder keeps the two-file firmware contract crisp
  (sketch + config.h side by side) while Arduino IDE opens `firmware/` directly as
  a sketch folder; PlatformIO points at it via `[platformio] src_dir`. Tests and
  build metadata are auxiliary tooling, not firmware — they do not violate §3.
- `constexpr` (not `#define`) for config constants so native tests can
  `static_assert` them.

## Risks & gotchas for implementers
1. **`analogReadResolution(12)` in setup() is mandatory** or all §8 count-based
   thresholds are off by ~4× (10-bit default).
2. **`BOOTSEL` is a bool-like object, not a pin** — `pinMode`/`digitalRead` on it
   reads GPIO 0/1 silently (compiles via implicit bool→int conversion).
3. Watchdog ceiling 8388 ms: keep `WATCHDOG_TIMEOUT_S = 8`; feed FIRST in `loop()`.
4. Never `while(!Serial)`; USB is unplugged in service (buck-powered).
5. Do not raise WS2812 brightness above 60/255 (3V3 LDO budget, PRD §2.2).
6. PlatformIO `native` env does not preprocess .ino prototypes — the sketch must
   declare all function prototypes explicitly (good practice anyway).
7. Bench power discipline (see hardware_validation.md): never USB + buck together.

## Sources
- https://arduino-pico.readthedocs.io/en/latest/bootsel.html
- https://arduino-pico.readthedocs.io/en/latest/analog.html
- https://arduino-pico.readthedocs.io/en/latest/millis.html
- https://arduino-pico.readthedocs.io/en/latest/serial.html
- https://github.com/earlephilhower/arduino-pico (cores/rp2040/Bootsel.cpp, boards.txt, libraries/WS2812)
- https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_watchdog/include/hardware/watchdog.h
- https://docs.platformio.org/en/latest/advanced/unit-testing/
