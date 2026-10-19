# PRP — P1.M1.T1.S1: Author `config.h` with every section-8 tunable

## Goal

**Feature Goal**: Create `firmware/config.h` — the single, complete, authoritative source of every firmware tunable (pins, timings, thresholds, LED patterns, platform constants) exactly as specified in PRD `spec/SPEC.md` §8, as typed `constexpr` constants with grouped, unit-commented documentation.

**Deliverable**: One new file: `firmware/config.h` (creating the `firmware/` directory). C++ header, self-contained (only `<stdint.h>`), constants only — no functions, no Arduino dependencies.

**Success Definition**:
- Every §8 constant present with its exact PRD name and default value, plus the one mandated addition `ADC_BITS = 12`.
- Header compiles cleanly under plain host `g++ -std=c++17 -Wall -Wextra -Werror` with **zero** Arduino headers/includes (this is what makes it includable by native tests later).
- Every constant's value provable via `static_assert` in a scratch translation unit.
- Comments carry units, meaning, and the governing PRD § reference per group — the comments ARE the documentation (Mode A, see below).

## User Persona

**Target User**: (1) The implementing agent of every downstream subtask (P1.M1.T1.S2 onward) — they `#include "config.h"` and reference names, never literals. (2) The human commissioning the device (PRD §9) — `config.h` is the one file they tune.

**Use Case**: Downstream firmware code and native Unity tests include this header for every constant; the native tests additionally `static_assert` its values (impossible with `#define` — this is why `constexpr` is mandated).

**Pain Points Addressed**: Magic numbers scattered across the sketch; tunables unfindable at commissioning time; thresholds silently wrong because ADC resolution assumptions differ (10-bit default vs 12-bit counts).

## Why

- PRD §3 mandates the two-file firmware layout: sketch + `config.h` holding **every** constant from §8. This is the first firmware file in a greenfield repo (verified: `plan/001_53984f832581/architecture/system_context.md` — no firmware code exists yet).
- PRD §8 "one place": all pins, timings, thresholds, and patterns live in `config.h`, grouped, commented with units and meaning.
- `constexpr` (not `#define`) is an architecture decision (`plan/001_53984f832581/architecture/external_deps.md`, "Recommended repo layout") so native tests can `static_assert` the contract.
- Everything downstream depends on this file: P1.M1.T1.S2 (skeleton `#include`s it), every pure-logic module (button, backstop, trip detection, LED engine), and the P1.M7.T1.S1 invariant suite.

## What

A single C++ header containing 31 PRD §8 constants + the mandated `ADC_BITS` addition + minimal table machinery (a POD struct + table-length constant), organized in 7 groups mirroring PRD §8's own grouping, each constant commented with unit and semantic meaning, each group header citing the governing PRD section.

### Success Criteria

- [ ] `firmware/config.h` exists; `firmware/` directory created
- [ ] All 32 scalar constants present with exact PRD §8 names and values (see Blueprint table)
- [ ] `BACKSTOP_TABLE_MIN` is a `constexpr` array of `{recoveryMin, backstopMin}` pairs: `{{2,44},{10,41},{20,38},{35,34},{60,30}}`, plus a table-length constant
- [ ] `ADC_BITS = 12` present (verified necessity: arduino-pico `analogRead` defaults to **10-bit**; all §8 count thresholds are 12-bit — see Gotchas)
- [ ] Every constant is `constexpr` with an explicit fixed-width integer type; zero `#define`s for constants; zero functions
- [ ] Header includes only `<stdint.h>`; no `<Arduino.h>`; `#pragma once` guard
- [ ] Every constant has a unit + meaning comment; every group header cites its PRD §
- [ ] Passes `g++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only` and the static_assert smoke test (Validation Level 2) with zero errors
- [ ] Nothing else created: no `platformio.ini`, no `test/`, no sketch file (those belong to P1.M1.T1.S2)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the repo is greenfield (no code to study). The complete constant contract is embedded verbatim in this PRP (Blueprint table + skeleton), the type rationale is documented, and validation uses only `g++` (verified present: GCC 16.2.1). An implementer who has never seen this repo can write the file from this PRP alone.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: spec/SPEC.md
  why: PRD source of truth. Section 8 is the verbatim constant contract; §2.1 pin map; §4 LED patterns; §5.1 button; §6 backstop table; §7 windows/ADC.
  pattern: Copy constant NAMES and VALUES exactly from §8; group comments cite the §.
  gotcha: §8 shows "PIN_SENSOR1 = GP26" — GP26 is the numeric GPIO 26 (A0/ADC0). Encode as 26.

- file: plan/001_53984f832581/architecture/external_deps.md
  why: Verified platform facts. Item "3. ADC" proves analogRead default resolution is 10-bit → ADC_BITS=12 mandate. "Recommended repo layout" fixes firmware/config.h placement and the constexpr-not-define rule. "Risks & gotchas" items 1,3,5 directly annotate constants.
  section: "arduino-pico core APIs" > "3. ADC"; "Recommended repo layout (ONE recommendation)"; "Risks & gotchas for implementers"

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  why: The pure-logic/hardware seam config.h must fit: native tests #include the sketch which includes config.h, so config.h MUST compile under plain g++ with no Arduino headers.
  section: "1. The one architectural decision: pure-logic/hardware seam"

- file: plan/001_53984f832581/architecture/system_context.md
  why: Greenfield confirmation — this is the FIRST firmware file; no existing patterns to follow.
  section: "Verdict: GREENFIELD" + "Consequences for planning" item 1

- url: https://arduino-pico.readthedocs.io/en/latest/analog.html
  why: Official arduino-pico analog docs — confirms analogRead returns 0–4095 only after analogReadResolution(12); default is 10-bit (0–1023).
  critical: Without the resolution call, every dry baseline reads ~750–980 and every fill faults. ADC_BITS documents this contract; the sketch (P1.M3.T1.S2) will call analogReadResolution(ADC_BITS).

- url: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_watchdog/include/hardware/watchdog.h
  why: RP2040 watchdog max timeout is 8388 ms (erratum RP2040-E1).
  critical: WATCHDOG_TIMEOUT_S=8 (8000 ms) is legal but near the ceiling — the static_assert smoke test enforces WATCHDOG_TIMEOUT_S*1000 <= 8388 so nobody can raise it past hardware limits.
```

### Current Codebase tree

```bash
water-jug-filler/
├── .envrc                    # direnv: export PRD_FILE=spec/SPEC.md
├── .gitignore
├── spec/
│   └── SPEC.md               # PRD (read-only source of truth)
└── plan/001_53984f832581/    # orchestrator plan (read-only: tasks.json, prd_snapshot.md, architecture/)
# NOTE: no firmware/, no test/, no platformio.ini — this task creates firmware/
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── config.h    # ← THIS TASK. Every §8 tunable as constexpr, grouped, unit-commented.
│                    #   Included by firmware/jug_filler.ino (P1.M1.T1.S2) and thereby by
│                    #   every native test (test/*.cpp include the sketch). Constants only.
├── ... (test/, platformio.ini, sketch — NOT in this task; P1.M1.T1.S2 creates them)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: config.h MUST NOT include <Arduino.h> or any Arduino/core header.
//   Native tests (plain host g++, no Arduino toolchain) include the sketch which
//   includes this file. Only <stdint.h> is permitted — it works in both contexts.

// CRITICAL: analogRead() in arduino-pico defaults to 10-BIT (0–1023). Every §8 count
//   constant (BASELINE_MIN/MAX, TRIP_DROP_COUNTS) is in 12-bit counts (0–4095).
//   ADC_BITS=12 exists precisely to carry this contract; the sketch MUST call
//   analogReadResolution(ADC_BITS) in setup() (owned by P1.M1.T1.S2 / P1.M3.T1.S2).

// GOTCHA: constexpr, never #define — downstream static_asserts need real typed
//   constants; #define also breaks namespaces/debuggers/scoping.

// GOTCHA: explicit fixed-width types (uint8_t/uint16_t/uint32_t). The Arduino pin
//   APIs take pin_size_t (widening from uint8_t is silent and safe); millis() is
//   uint32_t. Avoid bare `int`/`long` — sizes differ between host and AVR-ish habits.

// GOTCHA: arduino-pico compiles with -std=gnu++17 — static_assert without a message
//   is fine, but include a message anyway for maximum portability.

// GOTCHA: RP2040 watchdog hard ceiling = 8388 ms (RP2040-E1). 8 s is intentionally
//   close; do NOT raise (smoke test enforces).

// GOTCHA: RGB_BRIGHTNESS ≤ 60 is a safety cap (3V3 LDO budget on RP2040-Zero with
//   WS2812 on GP16, PRD §2.2) — not a style preference. Smoke test enforces.

// GOTCHA: names come from the PRD verbatim (SCREAMING_SNAKE, e.g. PIN_SENSOR1 not
//   PIN_SENSOR_1). Downstream tasks reference these exact identifiers.
```

## Implementation Blueprint

### Data models and structure

The only "data model" is the backstop-table entry struct (a POD type — allowed; the "no functions" constraint bans behavior, not types):

```cpp
// One point of the concave piecewise-linear backstop curve (PRD §6.2).
struct BackstopTableEntry {
    uint16_t recoveryMin;   // minutes since previous fill end (independent variable)
    uint16_t backstopMin;   // max valve-open minutes granted at that recovery
};
```

### The complete constant contract (author exactly this)

| # | Group | Constant | Type | Value | Unit | Meaning / comment must state |
|---|-------|----------|------|-------|------|------------------------------|
| 1 | Pins | `PIN_SENSOR1` | `uint8_t` | `26` | GPIO# | GP26 / A0 / ADC0 → sensor 1 AOUT (PRD §2.1) |
| 2 | Pins | `PIN_SENSOR2` | `uint8_t` | `27` | GPIO# | GP27 / A1 / ADC1 → sensor 2 AOUT (PRD §2.1) |
| 3 | Pins | `PIN_VALVE` | `uint8_t` | `15` | GPIO# | GP15 → MOSFET TRIG/SIG; HIGH = valve open (PRD §2.1) |
| 4 | Pins | `PIN_LED` | `uint8_t` | `14` | GPIO# | GP14 → external green LED anode via 330 Ω (PRD §2.1) |
| 5 | Button | `BUTTON_LONG_PRESS_MS` | `uint32_t` | `1000` | ms | hold ≥ this = long press (PRD §5.1) |
| 6 | Button | `BUTTON_DEBOUNCE_MS` | `uint32_t` | `40` | ms | contact debounce time (PRD §5.1) |
| 7 | Backstop | `FRESH_BOOT_BACKSTOP_MIN` | `uint16_t` | `30` | min | no-history default (fresh boot/reset); assume full tank (PRD §6.1) |
| 8 | Backstop | `BACKSTOP_TABLE_MIN` | `const BackstopTableEntry[]` | `{{2,44},{10,41},{20,38},{35,34},{60,30}}` | min,min | concave piecewise curve, clamped both ends (PRD §6.2) |
| 9 | Backstop | `BACKSTOP_TABLE_LEN` | `uint8_t` | `sizeof`-derived = 5 | count | entries in BACKSTOP_TABLE_MIN (helper for P1.M4.T1.S1 iteration) |
| 10 | Backstop | `PLUS5_MIN` | `uint16_t` | `5` | min | long-press deadline extension, FILLING only, stacks (PRD §5.1) |
| 11 | Windows | `CONFIRM_WINDOW_S` | `uint16_t` | `12` | s | second sensor must trip within this of the first (PRD §7.5) |
| 12 | Windows | `EARLY_WET_WINDOW_S` | `uint16_t` | `5` | s | wet reading this early = false trip → fault (PRD §7.6) |
| 13 | Windows | `TRIP_MIN_ELAPSED_S` | `uint16_t` | `60` | s | trips before this elapsed = false trip → fault (PRD §7.6) |
| 14 | ADC | `ADC_BITS` | `uint8_t` | `12` | bits | resolution; sketch calls `analogReadResolution(ADC_BITS)` — arduino-pico default is 10-bit, §8 counts are 12-bit |
| 15 | ADC | `ADC_SAMPLES_PER_READING` | `uint16_t` | `32` | samples | averaged per reading (PRD §8) |
| 16 | ADC | `READING_INTERVAL_MS` | `uint32_t` | `100` | ms | cadence between averaged readings (PRD §8) |
| 17 | ADC | `BASELINE_SETTLE_MS` | `uint32_t` | `2000` | ms | settle before baseline capture (PRD §7.1) |
| 18 | ADC | `BASELINE_MIN` | `uint16_t` | `2000` | counts | plausible dry baseline lower bound, 12-bit (PRD §8; commissioning §9) |
| 19 | ADC | `BASELINE_MAX` | `uint16_t` | `4050` | counts | plausible dry baseline upper bound, 12-bit (PRD §8; commissioning §9) |
| 20 | ADC | `TRIP_DROP_COUNTS` | `uint16_t` | `350` | counts | wet threshold = baseline − this (PRD §7.3) |
| 21 | ADC | `DEBOUNCE_COUNT` | `uint16_t` | `8` | readings | consecutive wet readings to accept a trip (PRD §7.3) |
| 22 | LED | `DONE_PULSE_ON_MS` | `uint32_t` | `200` | ms | DONE attention pulse on-time (PRD §4) |
| 23 | LED | `DONE_PULSE_OFF_MS` | `uint32_t` | `200` | ms | DONE attention pulse off-time (PRD §4) |
| 24 | LED | `DONE_PULSE_COUNT` | `uint8_t` | `3` | pulses | pulses per DONE group (PRD §4) |
| 25 | LED | `DONE_GAP_MS` | `uint32_t` | `1000` | ms | off-time between DONE groups (PRD §4) |
| 26 | LED | `FAULT_BLINK_MS` | `uint32_t` | `250` | ms | fault blink on/off time (PRD §7) |
| 27 | LED | `FAULT_PAUSE_MS` | `uint32_t` | `2000` | ms | pause between fault blink groups (PRD §7) |
| 28 | LED | `ACK_BLINK_MS` | `uint32_t` | `100` | ms | +5 ack blink on/off (PRD §4 transient) |
| 29 | LED | `USE_ONBOARD_RGB` | `bool` | `true` | flag | mirror patterns on onboard WS2812 (GP16, no wiring) (PRD §4) |
| 30 | LED | `RGB_BRIGHTNESS` | `uint8_t` | `60` | 0–255 | WS2812 brightness cap, must stay ≤ 60 (3V3 LDO budget, §2.2) |
| 31 | Platform | `SERIAL_BAUD` | `uint32_t` | `115200` | baud | USB CDC serial, always on (PRD §3) |
| 32 | Platform | `WATCHDOG_TIMEOUT_S` | `uint16_t` | `8` | s | RP2040 watchdog; hard ceiling 8.388 s — do not raise (PRD §10) |

### File skeleton (follow this layout exactly)

```cpp
// firmware/config.h
//
// Jug-filler configuration — EVERY tunable in one place (PRD spec/SPEC.md §8).
// Single source of truth: downstream code (sketch + native tests) includes this
// file and must never hardcode a firmware constant elsewhere.
//
// This file is CONSTANTS ONLY — no functions, no Arduino includes (it must
// compile under plain host g++ for the native test seam).
// All values: PRD §8 defaults. Commissioning may retune ADC/sensor values (§9).

#pragma once

#include <stdint.h>   // fixed-width types; works in Arduino g++ and host g++

// ---- Fill backstop model (PRD §6) ----------------------------------------
// (struct BackstopTableEntry + BACKSTOP_TABLE_MIN curve live here)

// ... seven groups in PRD §8 order:
// 1. Pins (RP2040-Zero)            — PRD §2.1
// 2. Button                        — PRD §5.1
// 3. Fill backstop model           — PRD §6.1/§6.2
// 4. Windows                       — PRD §7
// 5. ADC / sensors                 — PRD §7/§8 (provisional; commissioning §9)
// 6. LED patterns                  — PRD §4
// 7. Platform                      — PRD §3/§10
```

Ordering within each group follows the table above. Table-length constant:

```cpp
constexpr uint8_t BACKSTOP_TABLE_LEN =
    static_cast<uint8_t>(sizeof(BACKSTOP_TABLE_MIN) / sizeof(BACKSTOP_TABLE_MIN[0]));
```

(`sizeof` arithmetic instead of `std::size` — works in every C++ standard and needs no extra includes.)

Comment style requirement (Mode A documentation — `plan/001_53984f832581/architecture/firmware_architecture.md` §6): each group header names its PRD section; each constant gets a one-line trailing or preceding comment with **unit + meaning** (see the "Meaning" column above — those texts are the intended comment content, adapt wording freely but keep unit + PRD § reference). Example of the expected density:

```cpp
// ---- ADC / sensors (provisional; set during commissioning — PRD §9) ----
// ADC_BITS: analogReadResolution(ADC_BITS) is MANDATORY in setup() — the
//   arduino-pico default is 10-bit; every count below is a 12-bit count (0–4095).
constexpr uint8_t ADC_BITS = 12;
// Averaged readings: ADC_SAMPLES_PER_READING samples averaged every READING_INTERVAL_MS.
constexpr uint16_t ADC_SAMPLES_PER_READING = 32;
constexpr uint32_t READING_INTERVAL_MS = 100;
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: CREATE firmware/config.h   (the entire deliverable — single task, no deps)
  - CREATE directory firmware/ then file firmware/config.h
  - CONTENT: the 32-row contract table above, verbatim names/values/types
  - STRUCTURE: skeleton layout above — #pragma once, <stdint.h>, 7 grouped sections
  - CONSTRAINTS:
      * constexpr only; NO #define constants; NO functions; NO <Arduino.h>
      * exact PRD identifier spelling (PIN_SENSOR1, not PIN_SENSOR_1)
      * no constants beyond the 32 in the table (scope fence — downstream tasks
        get NOTHING extra to depend on; additions would violate §8 "one place")
  - DOCS: Mode A — group comments + per-constant unit/meaning comments ARE the
    documentation surface. No separate doc file. No README changes (P1.M1.T1.S2 owns README build section).

Task 2: VALIDATE with the scratch smoke test (Validation Levels 1–2 below)
  - Scratch file lives in /tmp (NOT in the repo — test/ belongs to P1.M1.T1.S2)
  - All static_asserts green, zero warnings under -Werror
  - Then delete the scratch file
```

### Integration Points

```yaml
DOWNSTREAM (do NOT create these — listed so the contract is understood):
  - P1.M1.T1.S2: firmware/jug_filler.ino starts with #include "config.h"; platformio.ini
    sets src_dir=firmware; test/*.cpp include the sketch → transitively config.h
  - P1.M3.T1.S2: setup() calls analogReadResolution(ADC_BITS)
  - P1.M4.T1.S1: backstopMinutes() iterates BACKSTOP_TABLE_MIN[0..BACKSTOP_TABLE_LEN)
  - P1.M7.T1.S1: invariant suite static_asserts config relationships (same asserts as Level 2)

NO INTEGRATION WORK in this task: no build system, no sketch, no test/, no .gitignore
changes, no docs/. The file is intentionally consumer-less until P1.M1.T1.S2 lands.
```

## Validation Loop

> NOTE: This repo has NO build system yet (that is P1.M1.T1.S2's deliverable). Validation here uses plain host `g++` (verified present: g++ 16.2.1). Do not create `platformio.ini` or `test/` in this task.

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# From repo root, after creating the file:
g++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only -x c++ firmware/config.h

# Expected: zero output, exit 0. If errors exist, READ them and fix before proceeding.
# This also proves self-containment: no Arduino headers were needed.
```

### Level 2: Contract Smoke Test (Component Validation)

Create `/tmp/config_smoke.cpp` (scratch — never committed; P1.M1.T1.S2 will fold these into `test/test_config.cpp` under Unity):

```cpp
#include "../firmware/config.h"   // run from repo root: g++ /tmp/config_smoke.cpp

// --- exact values (PRD §8) ---
static_assert(PIN_SENSOR1 == 26, "sensor1 pin");
static_assert(PIN_SENSOR2 == 27, "sensor2 pin");
static_assert(PIN_VALVE == 15, "valve pin");
static_assert(PIN_LED == 14, "led pin");
static_assert(BUTTON_LONG_PRESS_MS == 1000, "long press");
static_assert(BUTTON_DEBOUNCE_MS == 40, "debounce");
static_assert(FRESH_BOOT_BACKSTOP_MIN == 30, "fresh boot backstop");
static_assert(PLUS5_MIN == 5, "plus5");
static_assert(CONFIRM_WINDOW_S == 12, "confirm window");
static_assert(EARLY_WET_WINDOW_S == 5, "early wet window");
static_assert(TRIP_MIN_ELAPSED_S == 60, "min elapsed");
static_assert(ADC_BITS == 12, "adc bits");
static_assert(ADC_SAMPLES_PER_READING == 32, "samples");
static_assert(READING_INTERVAL_MS == 100, "reading interval");
static_assert(BASELINE_SETTLE_MS == 2000, "settle");
static_assert(BASELINE_MIN == 2000, "baseline min");
static_assert(BASELINE_MAX == 4050, "baseline max");
static_assert(TRIP_DROP_COUNTS == 350, "trip drop");
static_assert(DEBOUNCE_COUNT == 8, "debounce count");
static_assert(DONE_PULSE_ON_MS == 200, "done on");
static_assert(DONE_PULSE_OFF_MS == 200, "done off");
static_assert(DONE_PULSE_COUNT == 3, "done pulses");
static_assert(DONE_GAP_MS == 1000, "done gap");
static_assert(FAULT_BLINK_MS == 250, "fault blink");
static_assert(FAULT_PAUSE_MS == 2000, "fault pause");
static_assert(ACK_BLINK_MS == 100, "ack blink");
static_assert(USE_ONBOARD_RGB == true, "rgb enabled");
static_assert(RGB_BRIGHTNESS == 60, "rgb brightness");
static_assert(SERIAL_BAUD == 115200, "baud");
static_assert(WATCHDOG_TIMEOUT_S == 8, "watchdog");

// --- table shape (PRD §6.2) ---
static_assert(BACKSTOP_TABLE_LEN == 5, "table length");
static_assert(BACKSTOP_TABLE_MIN[0].recoveryMin == 2 && BACKSTOP_TABLE_MIN[0].backstopMin == 44, "tbl[0]");
static_assert(BACKSTOP_TABLE_MIN[1].recoveryMin == 10 && BACKSTOP_TABLE_MIN[1].backstopMin == 41, "tbl[1]");
static_assert(BACKSTOP_TABLE_MIN[2].recoveryMin == 20 && BACKSTOP_TABLE_MIN[2].backstopMin == 38, "tbl[2]");
static_assert(BACKSTOP_TABLE_MIN[3].recoveryMin == 35 && BACKSTOP_TABLE_MIN[3].backstopMin == 34, "tbl[3]");
static_assert(BACKSTOP_TABLE_MIN[4].recoveryMin == 60 && BACKSTOP_TABLE_MIN[4].backstopMin == 30, "tbl[4]");

// --- semantic invariants (these become the P1.M7.T1.S1 suite) ---
static_assert(BASELINE_MIN < BASELINE_MAX, "dry range is non-empty");
static_assert(BASELINE_MAX <= 4095, "baseline range within 12-bit full scale");
static_assert(BASELINE_MIN >= TRIP_DROP_COUNTS, "wet threshold cannot underflow uint16 math");
static_assert(WATCHDOG_TIMEOUT_S * 1000u <= 8388u, "RP2040 watchdog hard ceiling (RP2040-E1)");
static_assert(RGB_BRIGHTNESS <= 60, "WS2812 brightness safety cap (3V3 LDO budget)");
static_assert(FRESH_BOOT_BACKSTOP_MIN == BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN - 1].backstopMin,
              "full-tank backstop equals fresh-boot default (PRD §6.2 ≥60min row)");

int main() { return 0; }
```

```bash
g++ -std=c++17 -Wall -Wextra -Werror /tmp/config_smoke.cpp -o /tmp/config_smoke && /tmp/config_smoke
echo "exit: $?"     # Expected: exit: 0
rm /tmp/config_smoke.cpp /tmp/config_smoke   # keep the repo clean
```

Expected: compiles with zero warnings/errors, exits 0. A failed `static_assert` prints its message — fix config.h, never the assert.

### Level 3: Integration Testing (System Validation)

Deferred by design: the first real consumer (`firmware/jug_filler.ino`, Arduino build, `pio test -e native`) is created by **P1.M1.T1.S2**, which depends on this task. What you CAN verify now:

```bash
# (a) The header is includable twice safely (pragma once):
cat > /tmp/double_include.cpp <<'EOF'
#include "firmware/config.h"
#include "firmware/config.h"
int main() { return PIN_VALVE == 15 ? 0 : 1; }
EOF
g++ -std=c++17 -Wall -Wextra -Werror /tmp/double_include.cpp -o /tmp/double_include && /tmp/double_include
echo "exit: $?"   # Expected: exit: 0
rm -f /tmp/double_include.cpp /tmp/double_include

# (b) It is truly standalone — compiles with nothing but the C++ standard library:
g++ -std=c++17 -fsyntax-only -I. -x c++ firmware/config.h && echo "standalone OK"
```

### Level 4: Completeness Cross-Check (Domain-Specific Validation)

```bash
# Every §8 identifier from the PRD must appear exactly once as a definition.
# 32 expected definitions (31 scalars + table; BACKSTOP_TABLE_LEN is derived):
grep -c '^\s*constexpr' firmware/config.h   # Expected: 32

# No forbidden constructs:
grep -nE '#\s*define|Arduino\.h|inline|^[A-Za-z].*\(' firmware/config.h
# Expected: no output (no defines, no Arduino include, no function definitions)
# (the `(` alternation only flags lines with parentheses = function-shaped; the
#  sizeof(BACKSTOP_TABLE_MIN) line is inside BACKSTOP_TABLE_LEN's initializer and
#  won't match `^[A-Za-z]` — if it does, re-inspect manually)

# Cross-check names against the PRD itself (source of truth):
grep -oE '^[A-Z][A-Z0-9_]+ =' spec/SPEC.md | sort -u
# Every name listed must exist in firmware/config.h with the same default value.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1 passes: `g++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only -x c++ firmware/config.h`
- [ ] Level 2 passes: scratch smoke test — all 42 static_asserts green, exit 0, scratch files deleted
- [ ] Level 3 passes: double-include + standalone checks, exit 0
- [ ] Level 4 passes: `grep -c constexpr` = 32; zero forbidden constructs; names cross-checked against `spec/SPEC.md` §8

### Feature Validation

- [ ] All success criteria in "What" section met
- [ ] Every constant carries unit + meaning; every group cites its PRD §
- [ ] Names spelled exactly as PRD §8 (`PIN_SENSOR1`, `DONE_PULSE_ON_MS`, …)
- [ ] `ADC_BITS = 12` present with the 10-bit-default warning comment
- [ ] BACKSTOP_TABLE_MIN = `{{2,44},{10,41},{20,38},{35,34},{60,30}}` as `constexpr BackstopTableEntry[]`

### Code Quality Validation

- [ ] Zero `#define`, zero functions, zero Arduino includes, `#pragma once`
- [ ] Explicit fixed-width types per the contract table
- [ ] Scope respected: ONLY `firmware/config.h` added to the repo
- [ ] No `platformio.ini`, no `test/`, no sketch, no README/doc changes (downstream tasks own those)

### Documentation & Deployment

- [ ] Mode A satisfied: the header's comments are the complete tuning documentation (§8 "one place")
- [ ] Commissioning pointer present: ADC/sensor group marked provisional per PRD §9

## Anti-Patterns to Avoid

- ❌ Don't use `#define` for constants — native tests must `static_assert` real typed symbols
- ❌ Don't include `<Arduino.h>` (or any core header) — breaks the native test seam that every pure-logic subtask depends on
- ❌ Don't add "convenience" constants beyond the 32-row contract (e.g., no `PIN_WS2812 = 16` — GP16 is hardcoded onboard wiring, not a §8 tunable; no computed `CONFIRM_WINDOW_MS` — consumers derive units)
- ❌ Don't rename PRD identifiers for style ("nicer" names break every downstream task and the §8 audit)
- ❌ Don't add functions, `constexpr` helpers, or `toMs()` converters — constants only
- ❌ Don't create the sketch, build system, or tests "to prove it works" — that's P1.M1.T1.S2's scoped deliverable; the /tmp smoke test is the proof here
- ❌ Don't pad `WATCHDOG_TIMEOUT_S` or raise `RGB_BRIGHTNESS` — both are safety-bounded (ceiling 8388 ms; 60/255 LDO budget)
- ❌ Don't catch/fix failing static_asserts by editing the test — the test encodes PRD §8; fix config.h

---

**Confidence Score: 9/10** — The constant contract is fully enumerated verbatim (values, types, units, comments), the platform constraints are pre-verified in `architecture/external_deps.md`, and validation is deterministic via g++ static_asserts on the host. Residual risk is only implementer freelancing (renames/extras), fenced by the contract table, scope fence, and Level 4 cross-check against `spec/SPEC.md`.
