# Research Notes — P1.M1.T2.S2: Hardware watchdog service + unplug-safe serial logging

## 1. Verified watchdog facts (from plan/001_53984f832581/architecture/external_deps.md §2)

pico-sdk `hardware/watchdog.h` is shipped inside the arduino-pico core and includable
directly from a sketch (no library install, works in both Arduino IDE and PlatformIO):

```cpp
#include <hardware/watchdog.h>
watchdog_enable(8000, true);       // (uint32_t delay_ms, bool pause_on_debug)
watchdog_update();                 // feed — resets the ~8 s countdown
bool watchdog_caused_reboot();     // true if this boot followed a watchdog reset
```

- **RP2040 hard ceiling: 8388 ms** (erratum RP2040-E1) — verified in the pico-sdk
  header comment. `WATCHDOG_TIMEOUT_S = 8` → 8000 ms is legal but near the ceiling;
  never raise it. config.h already carries it as `constexpr uint16_t WATCHDOG_TIMEOUT_S = 8`
  (test_seam.cpp asserts it).
- No Arduino-level `wdt_*` API in this core — the pico-sdk calls ARE the API.
- `watchdog_caused_reboot()` reads the bootcause/scratch flag; it stays valid until
  the next reset. Calling it before `watchdog_enable()` is belt-and-braces.
- `pause_on_debug = true` freezes the watchdog under an attached debugger — right for
  bench work.
- Sources:
  - https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_watchdog/include/hardware/watchdog.h
  - https://raspberrypi.github.io/pico-sdk-doxygen/group__hardware__watchdog.html

## 2. Unplug-safe serial policy (external_deps.md §6, PRD §2.2/§3/§10)

- In service the board runs from the buck converter with **USB unplugged**; the fill
  logic must run regardless → **never `while(!Serial)`** (the S2 skeleton already
  ships `Serial.begin(SERIAL_BAUD)` with that comment).
- Policy this task owns and enforces via `logLine()`: keep writes **small,
  line-oriented, and bounded** so CDC TX buffers can never saturate when no host
  drains them. One fixed-size buffer, one bounded `Serial.write`, one newline byte.
- arduino-pico `Serial` (USB CDC) does expose `printf()`, but `vsnprintf` into a
  fixed stack buffer + `Serial.write(buf, len)` is chosen instead:
  standard C on both toolchains (host g++ / arm-none-eari g++), hard upper bound on
  bytes written (the unplug-safety property itself), and identical truncation
  semantics native tests can pin.
- `Serial.begin()`/`write()` with no host attached does not block on arduino-pico
  (writes are dropped when the CDC line is down): https://arduino-pico.readthedocs.io/en/latest/serial.html

## 3. Seam constraints (firmware_architecture.md §1, S2/S1 PRPs)

- Watchdog + CDC writes are hardware glue → **inside `#ifdef ARDUINO`**, including
  the `#include <hardware/watchdog.h>` (host g++ must never see it).
- Anything testable must be pure → the banner text builder
  `formatBootBanner(char*, size_t, bool)` lives UNGUARDED using only `snprintf`
  (`<stdio.h>`/`<stdarg.h>` are host-safe includes).
- **Purity grep** on the unguarded region matches `millis|delay|Serial|pinMode|
  digitalWrite|analogRead` — so unguarded-region comments must avoid those words
  too (write "CDC"/"log line", never "Serial").
- Explicit prototypes required for every function (native g++ has no .ino prototype
  generator). Variadic prototype `void logLine(const char* fmt, ...);` is fine.
- Test conventions: Unity 2.6.1, each `test/*.cpp` its own project, explicit
  `int main()` mandatory on PIO 6.2.0, `UL`-suffix literals, include
  `"../firmware/jug_filler/jug_filler.ino"`, `build_src_filter` already excludes
  firmware/ from native env. After this task `pio test -e native` runs THREE
  projects (test_seam, test_time_utils, test_log_utils).
- Precedent (S1): non-tunable constants live as local `constexpr` beside their only
  consumer — `LOGLINE_MAX = 128` (ARDUINO region, only consumer is logLine) and the
  banner buffer size 64 (setup()). **config.h is untouched** (item: no config
  surface change; log formats documented at P2.M1.T1.S1).

## 4. Setup/loop ordering (architecture boot sequence; PRD §10)

setup(): valve LOW (line 1, unmovable) → pin modes → Serial.begin →
analogReadResolution → **banner via formatBootBanner + watchdog_caused_reboot +
logLine** → **watchdog_enable LAST** (arch: "Serial.begin + banner →
watchdog_enable(8000, true)"). loop(): **watchdog_update() is the first statement**
every pass; a hang resets → setup() line 1 closes the valve → IDLE (PRD §10, §5.3).

## 5. Banner strings (normative, this task defines them)

- Watchdog reset: `JugFiller boot (reset cause: WATCHDOG)` — 38 chars
- Any other boot (power-on, RESET button): `JugFiller boot (reset cause: POWER_OR_RESET)` — 44 chars
- Both fit the 64-byte banner buffer and the 128-byte logLine buffer (no truncation).

## 6. Hand-tested expectations for test_log_utils.cpp

- `snprintf` returns the would-be length (int); when the line fits, return ==
  strlen(buf). With an 8-byte buffer the return ≥ 8, strlen == 7, last byte NUL,
  prefix "JugFill" preserved. With bufLen==0 the buffer is untouched.
- Test count: 7 functions / 7 RUN_TEST lines.
