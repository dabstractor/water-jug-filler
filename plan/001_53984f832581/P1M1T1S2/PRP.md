# PRP — P1.M1.T1.S2: Dual-build skeleton (Arduino IDE + PlatformIO) with proven native test seam

## Goal

**Feature Goal**: Create the buildable firmware skeleton for the jug filler: a minimal safe sketch + relocated `config.h` inside a proper Arduino sketch folder, a `platformio.ini` with a board (compile-only) env and a native Unity test env, and one green native test that proves the pure-logic/hardware seam works BEFORE any behavioral logic lands.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — minimal safe skeleton sketch with the SEAM CONTRACT documented in its header comment (setup(): valve LOW as literal line 1 → pinModes → `Serial.begin` → `analogReadResolution`; loop(): safe placeholder; everything hardware inside `#ifdef ARDUINO`).
- `git mv firmware/config.h → firmware/jug_filler/config.h` (contents byte-for-byte untouched — see Why for the mandatory reason).
- `platformio.ini` (repo root) — `[env:pico]` raspberrypi/`waveshare_rp2040_zero`/arduino, `[env:native]` native/Unity.
- `test/test_seam.cpp` — Unity test asserting config constants + calling one unguarded-region probe function.
- `README.md` — seed with build instructions (Arduino IDE + PlatformIO + native tests).
- `.gitignore` — append `.pio/` only.

**Success Definition**:
- `pio test -e native` → **PASSED**, exit 0 (command + layout verified working on this machine — see Validation Level 1).
- `pio run -e pico` → compile-only **SUCCESS** (board id verified installed in the local platform catalog; first run downloads toolchain).
- Arduino IDE can open and build the sketch directly (manual human gate; steps in README — layout now satisfies the mandatory Arduino sketch-folder naming rule).
- The unguarded region compiles on host g++ with zero Arduino symbols; hardware code exists only inside `#ifdef ARDUINO`.
- Nothing beyond the skeleton: no watchdog, no LED engine, no button, no banner logging (owned by later subtasks).

## User Persona

**Target User**: (1) The implementing agent of every downstream firmware subtask — this skeleton is the file they extend; the seam contract comment tells them exactly where their code goes. (2) The human commissioning the device — they open the sketch in Arduino IDE (PRD §3: "just the core installed") and later run `pio test -e native` during development.

**Use Case**: Developer/agent edits only `jug_filler.ino`: pure logic goes in the unguarded region (host-testable via `test/*.cpp`), hardware glue goes in the `#ifdef ARDUINO` region. Every `pio test -e native` run proves the split still compiles on both toolchains.

**User Journey**: clone repo → `pio test -e native` (green in seconds, no hardware, no Arduino toolchain) → `pio run -e pico` (compile check for the real board) → or open `firmware/jug_filler/jug_filler.ino` in Arduino IDE and Upload with the "Waveshare RP2040-Zero" board selected.

**Pain Points Addressed**: Arduino IDE refusing to build because the `.ino` isn't in a same-named folder (the naive `firmware/jug_filler.ino` layout — see Gotcha #1); native tests that silently stop exercising the real sketch; test-runner link failures from PIO 6.2.0's missing auto-generated Unity main; valve ever floating HIGH at boot.

## Why

- PRD §3 hard requirement: the firmware is TWO files (sketch + `config.h`) that must build in Arduino IDE with just the arduino-pico core installed, with PlatformIO layout optional. PRD §10 hard invariant: valve pin driven LOW as the very first line of `setup()`.
- **Layout correction (binding, verified)**: `plan/001_53984f832581/architecture/external_deps.md` recommends `firmware/jug_filler.ino` and claims "Arduino IDE opens firmware/ directly as a sketch folder". That claim is FALSE: the Arduino sketch specification mandates *"Every sketch must contain a .ino file with a file name matching the sketch root folder name"* (https://arduino.github.io/arduino-cli/1.4/sketch-specification; rule upheld in arduino-cli issue #948). `firmware/jug_filler.ino` (folder ≠ sketch name) makes Arduino IDE show the "needs to be inside a sketch folder named jug_filler" repair dialog and makes arduino-cli refuse to compile. The fix that satisfies every PRD requirement: **the sketch folder is `firmware/jug_filler/`** containing exactly the two firmware files (`jug_filler.ino` + `config.h`). This is why Task 1 relocates S1's `config.h`.
- The seam is THE architectural decision of the whole project (`plan/001_53984f832581/architecture/firmware_architecture.md` §1): all hardware-touching code inside `#ifdef ARDUINO`, all pure logic unguarded, tests `#include` the `.ino`. Proving it green NOW (before any logic) de-risks every one of the ~15 downstream "pure" subtasks.
- Mode A documentation rider (firmware_architecture.md §6): this subtask seeds `README.md` with build instructions; P2.M1.T3.S1 expands it later.

## What

A three-file build skeleton plus a README seed and one-line .gitignore append. The sketch contains: header comment documenting the seam contract; explicit prototypes; an unguarded region holding exactly one trivial probe function; an `#ifdef ARDUINO` region holding the minimal safe `setup()`/`loop()`. The native env is excluded from building `src_dir` (tests include the `.ino` directly).

### Success Criteria

- [ ] `firmware/jug_filler/jug_filler.ino` exists; `firmware/config.h` no longer exists; `firmware/jug_filler/config.h` exists with byte-identical content to the S1 deliverable (`git diff --find-renames` shows a pure rename)
- [ ] `pio test -e native` prints `[PASSED]` for both test functions, exit 0
- [ ] `pio run -e pico` completes with `[SUCCESS]` (compile only — never upload in this subtask)
- [ ] `setup()`'s FIRST executable statement is `digitalWrite(PIN_VALVE, LOW);` (before any `pinMode`)
- [ ] `loop()` is a safe placeholder that cannot open the valve; no `delay()`, no `while(!Serial)` anywhere
- [ ] All Arduino API usage in the `.ino` sits inside `#ifdef ARDUINO` (verifiable via Level 4 check)
- [ ] The `.ino` header comment documents the seam contract (4 rules, see Task 2 listing) and carries the stable region-marker comment lines later structural tests can anchor on
- [ ] `platformio.ini` matches the verified listing (Task 3) with `default_envs = native`
- [ ] `test/test_seam.cpp` asserts at least: `PIN_VALVE==15`, `SERIAL_BAUD==115200`, `ADC_BITS==12`, `FRESH_BOOT_BACKSTOP_MIN==30`, `WATCHDOG_TIMEOUT_S==8`, `BACKSTOP_TABLE_LEN==5`, and calls the unguarded probe function
- [ ] `README.md` covers: repo layout, Arduino IDE install/build steps (Boards Manager URL, board selection, sketch path), PlatformIO compile, native tests
- [ ] `.gitignore` gains exactly `.pio/` (nothing removed, no plan//PRD/task entries)
- [ ] No behavioral firmware logic added (time utilities = P1.M1.T2.S1; watchdog + logging = P1.M1.T2.S2; button = P1.M2; etc.)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the repo is greenfield except `firmware/config.h` (complete, read, and included in the live verification). Every artifact this task creates is given below as a complete, **verified-on-this-machine** listing (the exact `platformio.ini`, `.ino` and test pattern were built in a /tmp scratch project and `pio test -e native` passed in 0.45 s on PIO Core 6.2.0 with g++ 16.2.1). The implementer needs no Arduino/PlatformIO expertise beyond following the steps.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/config.h
  why: The S1 deliverable this task relocates (git mv) and the sketch includes. Read to know its exact contents — they must NOT change.
  pattern: constexpr constants; `#include "config.h"` from the sketch resolves because both live in the same folder after relocation.
  gotcha: Do not edit, reformat, or "improve" it. Byte-for-byte rename only.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  why: THE seam definition this skeleton implements — §1 diagram + "Rules of the seam" (4 rules, mirrored in the .ino header comment) and the setup() ordering (valve LOW → pinModes → analogReadResolution(12) → Serial).
  section: "1. The one architectural decision: pure-logic/hardware seam"
  gotcha: Its ASCII diagram shows the sketch at firmware/jug_filler.ino — superseded by the folder-name correction (see PRP "Why"); everything else in it is binding.

- file: plan/001_53984f832581/architecture/external_deps.md
  why: Verified platform facts behind the skeleton: analogReadResolution mandate (item 3), digitalWrite-before-pinMode safety (item 8), native seam + repo layout (final section), gotcha #6 "native env does not preprocess .ino prototypes" → explicit prototypes required.
  section: "Native host testing pattern (RECOMMENDED — one seam)" + "Risks & gotchas for implementers"

- file: plan/001_53984f832581/P1M1T1S2/research/notes.md
  why: The empirical log behind this PRP: PIO 6.2.0 needs explicit Unity main(); build_src_filter necessity; board id resolution evidence; the Arduino folder-name finding; both passing pio runs.
  pattern: /tmp scratch-project method + exact observed output.

- url: https://arduino.github.io/arduino-cli/1.4/sketch-specification
  why: Primary source for the MANDATORY rule "Every sketch must contain a .ino file with a file name matching the sketch root folder name" — the reason for the firmware/jug_filler/ nesting.
  critical: Violating it = Arduino IDE repair dialog (moves the file, breaking the layout) / arduino-cli compile refusal. Never flatten the layout.

- url: https://docs.platformio.org/en/latest/advanced/unit-testing/
  why: Official Unit Testing docs — pio test envs, Unity integration, test_dir semantics.
  critical: Older docs/examples show tests without main(); on PIO 6.2.0 that FAILS TO LINK ("undefined reference to main") — always write the explicit main() (verified live).

- url: https://arduino-pico.readthedocs.io/en/latest/analog.html
  why: analogRead defaults to 10-bit; only after analogReadResolution(12) does analogRead return 0–4095. setup() therefore calls analogReadResolution(ADC_BITS).
  critical: Without it every config count threshold is off ~4×.

- url: https://arduino-pico.readthedocs.io/en/latest/install.html
  why: Arduino IDE install steps for the README seed: Boards Manager JSON https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json then select a "Waveshare RP2040-Zero" board menu entry.
```

### Current Codebase tree (verified via find)

```bash
water-jug-filler/
├── .envrc                    # direnv: export PRD_FILE=spec/SPEC.md
├── .gitignore                # dist/ build/ node_modules/ venv/ .env .DS_Store *.log ...
├── firmware/
│   └── config.h              # ← S1 COMPLETE (32 constexpr tunables; this task relocates it)
├── spec/
│   └── SPEC.md               # PRD (read-only)
└── plan/001_53984f832581/    # orchestrator plan (read-only): tasks.json, prd_snapshot.md,
                              #   architecture/{system_context,external_deps,firmware_architecture,hardware_validation}.md
                              #   P1M1T1S1/{PRP.md,research/notes.md}, P1M1T1S2/{this PRP,research/notes.md}
# No .ino, no platformio.ini, no test/, no README.md yet. Toolchain verified: pio 6.2.0 (~/.local/bin/pio), g++ 16.2.1, raspberrypi platform already installed in ~/.platformio.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── jug_filler/               # Arduino sketch folder (name MUST match sketch file — see Gotcha 1)
│       ├── jug_filler.ino        # NEW. Skeleton sketch: seam-contract header, explicit prototypes,
│       │                         #   unguarded pure region (probe fn only), #ifdef ARDUINO hardware glue.
│       └── config.h              # RELOCATED from firmware/config.h (git mv, contents untouched).
├── test/
│   └── test_seam.cpp             # NEW. Unity test: config constants visible + unguarded fn callable.
├── platformio.ini                # NEW. [env:pico] compile-only board build; [env:native] host tests.
├── README.md                     # NEW (seed). Build instructions; expanded later by P2.M1.T3.S1.
└── .gitignore                    # MODIFIED: append ".pio/" line only (PlatformIO build dir).
```

### Known Gotchas of our codebase & Library Quirks

```python
# CRITICAL (verified live on PIO Core 6.2.0): [env:native] tests MUST define an explicit
#   int main() { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); }
# PIO 6.2.0 does NOT auto-generate a Unity runner — without main() the link fails:
#   /usr/bin/ld: ... undefined reference to `main'   (observed in scratch verification)
# (Older PlatformIO docs showing main-less tests are outdated for this version.)

# CRITICAL: [env:native] MUST carry build_src_filter = -<*>
#   Tests #include the .ino, compiling it INSIDE the test TU. If native also built src_dir,
#   non-inline functions would be duplicate symbols, and the native platform has no .ino
#   builder at all. (Verified: with the filter, native build is clean.)

# CRITICAL (Arduino spec, binding): the .ino must live in a folder named EXACTLY like the
#   sketch ("jug_filler"). Never flatten to firmware/jug_filler.ino — Arduino IDE will
#   offer to "fix" it by MOVING the file (breaking PlatformIO + the include of config.h).

# digitalWrite(PIN_VALVE, LOW) BEFORE pinMode is deliberate and safe: it latches the output
#   register LOW; during the Hi-Z window the MOSFET module's gate pull-down holds the valve
#   closed; pinMode(OUTPUT) then drives the pre-latched LOW with no HIGH glitch.
#   Do NOT reorder "for tidiness" — P1.M7.T1.S1 structurally asserts the line-1 ordering.

# First `pio run -e pico` downloads gcc-arm-none-eabi + arduino-pico framework
#   (hundreds of MB, minutes) — expected, one-time. `pio test -e native` downloads only the
#   tiny native platform + Unity 2.6.1 (auto via Library Manager; no lib_deps needed).

# Arduino IDE prepends Arduino.h to .ino automatically (and PlatformIO's ino-to-cpp does
#   too) — the sketch needs NO #include <Arduino.h> line. Host compilation never sees the
#   guarded region, so it also needs none.

# pio CLI surface (6.2.0): `pio boards` lists boards (NOT `pio board`). Native env uses host
#   g++ directly — no Arduino toolchain involved in `pio test -e native`.
```

## Implementation Blueprint

### Data models and structure

No data models in this subtask — the skeleton is functions only. `config.h` (already complete) is the data contract; `BackstopTableEntry` there is the only struct.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: RELOCATE firmware/config.h → firmware/jug_filler/config.h
  - RUN: mkdir -p firmware/jug_filler && git mv firmware/config.h firmware/jug_filler/config.h
  - VERIFY: git status shows rename; git diff --cached --find-renames shows NO content change
  - WHY: Arduino sketch-folder naming rule (Gotcha 1). config.h must sit INSIDE the sketch
    folder or Arduino IDE cannot resolve #include "config.h" from the sketch.
  - PRESERVE: file contents byte-for-byte (it is the S1 deliverable, already validated)

Task 2: CREATE firmware/jug_filler/jug_filler.ino
  - IMPLEMENT: exact skeleton below (copy it; then adjust nothing). Header = SEAM CONTRACT
    (4 rules). Region marker comment lines must be kept VERBATIM and STABLE — later
    structural tests (P1.M7.T1.S1) anchor on them:
      "// ---- UNGUARDED REGION: pure logic (host-testable) ----"
      "// ---- #ifdef ARDUINO REGION: hardware glue ----"
  - NAMING: file jug_filler.ino inside folder firmware/jug_filler/ (folder==sketch name, mandatory)
  - DEPENDENCIES: #include "config.h" (Task 1 location)
  - PLACEMENT: firmware/jug_filler/

Task 3: CREATE platformio.ini (repo root)
  - IMPLEMENT: exact verified listing below. Do not add build_flags, lib_deps, or extra envs.
  - VERIFIED: this exact file + layout passed `pio test -e native` on PIO Core 6.2.0
    (see research/notes.md). board id waveshare_rp2040_zero verified present in the
    raspberrypi platform catalog (RP2040, 133MHz, 2MB flash / 256KB RAM).
  - PLACEMENT: repo root

Task 4: CREATE test/test_seam.cpp
  - IMPLEMENT: exact listing below. NOTE the explicit main() — mandatory on PIO 6.2.0.
  - NAMING: test_*.cpp under test/ (each file = one test project for pio test)
  - PATTERN: #include "../firmware/jug_filler/jug_filler.ino" (relative path from test/)
  - PLACEMENT: test/

Task 5: RUN native seam proof (PRIMARY GATE)
  - RUN: pio test -e native
  - EXPECT: "test_seam.cpp:xx: <name> [PASSED]" lines, "native:* [PASSED]", exit 0
  - IF FAIL: read output; the two known failure modes are missing main() and missing
    build_src_filter (see Gotchas). Do not proceed until green.

Task 6: RUN pico compile-only check
  - RUN: pio run -e pico        # NEVER upload in this subtask; board may not be connected
  - EXPECT: first run downloads toolchain+framework, then "[SUCCESS]" and a .uf2/.bin under
    .pio/build/pico/. If the board id ever fails to resolve on a future platform release,
    fallback board ids: "rpipico" (generic Raspberry Pi Pico — pin-compatible for
    GP14/15/26/27, which are all this firmware touches).
  - NOTE: this env has no test_transport; run tests ONLY via -e native.

Task 7: APPEND .pio/ to .gitignore
  - ADD exactly one line ".pio/" under the existing "# Build artifacts" section
  - PRESERVE: every existing line untouched

Task 8: CREATE README.md (Mode A seed — build instructions only; P2.M1.T3.S1 expands)
  - SECTIONS: one-line project summary (jug filler firmware, RP2040-Zero); repo layout;
    "Build in Arduino IDE" (Boards Manager URL + select a "Waveshare RP2040-Zero" board
    entry, open firmware/jug_filler/jug_filler.ino, Upload); "Build with PlatformIO"
    (pio run -e pico); "Run native tests" (pio test -e native); one safety note (valve pin
    is driven LOW as the first line of setup() — never reorder).
  - PLACEMENT: repo root
```

**Task 2 listing — `firmware/jug_filler/jug_filler.ino` (complete file):**

```cpp
// firmware/jug_filler/jug_filler.ino
//
// ============================== SEAM CONTRACT ==============================
// This single file is BOTH the Arduino sketch and the host-testable logic
// module (architecture: plan/.../firmware_architecture.md §1). Rules:
//
//   1. UNGUARDED region = pure logic ONLY. No Arduino calls whatsoever — no
//      pinMode/digitalWrite/analogRead/millis/Serial/delay, no libraries.
//      Pure C++ that takes `uint32_t nowMs` (and sensor/event data) as
//      parameters. Native tests compile and exercise it on the host.
//   2. #ifdef ARDUINO region = hardware glue ONLY: setup()/loop(), pin
//      writes, Serial logging, watchdog, LED/WS2812 drivers.
//   3. Every function gets an explicit prototype above its definition —
//      host g++ does not run the Arduino .ino prototype generator.
//   4. Tests live in test/*.cpp as:
//      #include "../firmware/jug_filler/jug_filler.ino"
//      and run via `pio test -e native` (which excludes this folder from
//      its own build via build_src_filter in platformio.ini).
//
// SAFETY (PRD §10): the FIRST executable line of setup() drives PIN_VALVE
// LOW. Never insert anything above it. The invariant suite (P1.M7.T1.S1)
// structurally asserts this and scans for delay() in the loop region.
//
// Every later pure-logic subtask lands in the UNGUARDED region below.
// ==========================================================================

#include "config.h"

// ---- Prototypes (explicit; native g++ needs them, Arduino tolerates them) ----
uint32_t seamVersion();   // seam probe — replaced when first real logic lands
void setup();             // hardware only (#ifdef ARDUINO)
void loop();              // hardware only (#ifdef ARDUINO)

// ---- UNGUARDED REGION: pure logic (host-testable) ----

// Probe function proving the seam: callable from native Unity tests.
uint32_t seamVersion() { return 1; }

// ---- #ifdef ARDUINO REGION: hardware glue ----
#ifdef ARDUINO

void setup() {
  digitalWrite(PIN_VALVE, LOW);   // LINE 1 — PRD §10: valve closed before all else
  pinMode(PIN_VALVE, OUTPUT);     // drive the pre-latched LOW (glitch-free)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);     // external LED off at boot
  Serial.begin(SERIAL_BAUD);      // USB CDC, always on — NEVER while(!Serial) (PRD §3)
  analogReadResolution(ADC_BITS); // MANDATORY 12-bit — every config count is 12-bit
}

void loop() {
  // Safe placeholder: nothing here can open the valve (button service arrives in P1.M2).
  // P1.M1.T2.S2 makes watchdog_update() the first call here; P1.M6.T2.S1 fills the loop.
}

#endif  // ARDUINO
```

**Task 3 listing — `platformio.ini` (complete file, verified):**

```ini
; Jug-filler firmware build + host-test configuration (P1.M1.T1.S2).
; The sketch lives in an Arduino-legal sketch folder (folder name == sketch name),
; so BOTH Arduino IDE (open firmware/jug_filler/jug_filler.ino) and PlatformIO build it.

[platformio]
src_dir = firmware/jug_filler
test_dir = test
default_envs = native          ; bare `pio test` runs host tests only (never on-device)

; Compile-only build for the real board. No upload from this project yet.
[env:pico]
platform = raspberrypi
board = waveshare_rp2040_zero  ; verified in platform boards catalog (RP2040, 2MB flash)
framework = arduino

; Host unit tests: plain g++, no Arduino toolchain. Unity is auto-fetched
; (Library Manager, throwtheswitch/Unity ^2.6.1) on first run.
[env:native]
platform = native
test_framework = unity
build_src_filter =
  -<*>                        ; tests #include the .ino directly — never also compile src_dir
```

**Task 4 listing — `test/test_seam.cpp` (complete file):**

```cpp
// test/test_seam.cpp — proves the native seam BEFORE any logic lands (P1.M1.T1.S2).
// Compiles the sketch itself (via the .ino include) on the host g++; asserts that
// config constants reach native tests and the unguarded region is callable.
#include <unity.h>

#include "../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

void test_config_constants_reach_native_tests() {
    TEST_ASSERT_EQUAL_UINT8(15, PIN_VALVE);              // GP15, HIGH = open (PRD §2.1)
    TEST_ASSERT_EQUAL_UINT8(14, PIN_LED);
    TEST_ASSERT_EQUAL_UINT32(115200, SERIAL_BAUD);       // PRD §3
    TEST_ASSERT_EQUAL_UINT8(12, ADC_BITS);               // 12-bit ADC contract (PRD §8)
    TEST_ASSERT_EQUAL_UINT16(30, FRESH_BOOT_BACKSTOP_MIN);
    TEST_ASSERT_EQUAL_UINT16(8, WATCHDOG_TIMEOUT_S);     // ceiling 8.388 s — never raise
    TEST_ASSERT_EQUAL_UINT8(5, BACKSTOP_TABLE_LEN);
}

void test_unguarded_region_is_callable() {
    TEST_ASSERT_EQUAL_UINT32(1, seamVersion());          // pure region reachable from host
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner (verified live)
    UNITY_BEGIN();
    RUN_TEST(test_config_constants_reach_native_tests);
    RUN_TEST(test_unguarded_region_is_callable);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// Pattern: where future code goes (this PRP only ships the skeleton; do not add more).
//   Pure logic (P1.M1.T2.S1 time utils, P1.M2 button, P1.M3 sensors, P1.M4 backstop,
//   P1.M5 LED engine, P1.M6 FillEngine)  → UNGUARDED region, uint32_t nowMs params,
//     unit tests in test/test_<component>.cpp following test_seam.cpp's shape
//     (same include, same explicit main()).
//   Hardware glue (watchdog P1.M1.T2.S2, BOOTSEL poll P1.M2.T2.S1, ADC read P1.M3.T1.S2,
//     LED drivers P1.M5.T2/T3, loop wiring P1.M6.T2.S1) → #ifdef ARDUINO region.
//
// GOTCHA: every new test file needs its own main() with UNITY_BEGIN/RUN_TEST/UNITY_END.
// GOTCHA: keep the two region-marker comment lines byte-stable (structural test anchors).
// CRITICAL: setup()'s valve-LOW line stays literal line 1 of the function body forever.
```

### Integration Points

```yaml
BUILD:
  - platformio.ini is new at repo root; [platformio] src_dir/test_dir/default_envs govern
    every future `pio` invocation in this repo (documented in README).
  - .gitignore: append ".pio/" (PlatformIO build artifacts) — only addition permitted.

FIRMWARE:
  - config.h path changes firmware/config.h → firmware/jug_filler/config.h. No code
    references the old path yet (repo contains no other code — verified). The S1 PRP's
    g++ smoke commands remain valid: g++ scratch TU just #includes the new path.

DOCS (Mode A):
  - README.md seeded here; expanded by P2.M1.T3.S1 (which must not be blocked by this seed).

SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - watchdog_enable/watchdog_update + serial logging policy → P1.M1.T2.S2
  - BOOTSEL button → P1.M2; ADC reading service → P1.M3.T1.S2; WS2812 → P1.M5.T3.S1
  - setup() banner / watchdog_caused_reboot log → P1.M1.T2.S2 (Serial.begin alone ships now)
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Native build IS the syntax check — host g++ compiles the sketch's unguarded region:
pio test -e native
# Expected (verified on this machine, PIO Core 6.2.0 + g++ 16.2.1):
#   Building...  Testing...
#   test/test_seam.cpp:xx: test_config_constants_reach_native_tests [PASSED]
#   test/test_seam.cpp:xx: test_unguarded_region_is_callable       [PASSED]
#   native:* [PASSED]   /   1 test cases: 1 succeeded   / exit 0
# First run also prints Library Manager installing throwtheswitch/Unity — expected.

# Rename purity check (config.h untouched):
git diff --cached --find-renames --stat firmware/   # expect: rename, 0 insertions/deletions
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native -v          # verbose: shows each RUN_TEST line
# The single test project IS this subtask's unit deliverable. Expected: all PASSED.
# NOTE: never run bare `pio test -e pico` — that env has no host test transport.
```

### Level 3: Integration Testing (System Validation)

```bash
# Board compile-only (first run downloads toolchain + framework — minutes, network needed):
pio run -e pico
# Expected: environment summary "[SUCCESS]" and firmware artifact under .pio/build/pico/.
# NEVER upload here (no hardware stage in this subtask).

# Optional CI-style Arduino build check if arduino-cli is installed (not required):
#   arduino-cli board listall | grep -i "RP2040-Zero"   # find exact FQBN board id
#   arduino-cli compile --fqbn rp2040:rp2040:<board-id> firmware/jug_filler
# (FQBN board-id spelling varies by core version — resolve via listall, don't guess.)

# Manual human gate (Arduino IDE 2.x): install arduino-pico via Boards Manager
# (JSON: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json),
# select a "Waveshare RP2040-Zero" board entry, open firmware/jug_filler/jug_filler.ino,
# Verify/Compile. Expected: compiles with NO repair dialog (folder name matches sketch).
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Seam structural checks (these become automated in P1.M7.T1.S1; run manually now):
# (1) Valve-LOW is literally the first executable line of setup():
awk '/^void setup\(\)/{f=1} f && /digitalWrite\(PIN_VALVE, LOW\)/{print NR": "$0; exit} f && /pinMode|Serial|analogRead/{print "VIOLATION before valve-LOW at "NR": "$0; exit}' firmware/jug_filler/jug_filler.ino
# (2) No Arduino API usage outside the #ifdef ARDUINO region:
sed -n '1,/#ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino | grep -nE "pinMode|digitalWrite|analogRead|Serial|millis|delay|watchdog" || echo "SEAM OK: unguarded region is pure"
# Expected: prints the valve-LOW line number for (1); "SEAM OK" for (2).
# (3) No blocking constructs anywhere in the sketch:
grep -nE "\bdelay\(|while\(!Serial" firmware/jug_filler/jug_filler.ino || echo "NO BLOCKING OK"
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio test -e native` exit 0 with both tests `[PASSED]`
- [ ] `pio run -e pico` → `[SUCCESS]` (compile-only)
- [ ] `git status` clean set: 3 new files (ino, platformio.ini, README.md), 1 new test file, 1 rename (config.h), .gitignore append
- [ ] config.h rename is content-identical (`git diff --find-renames`)

### Feature Validation

- [ ] All success criteria in "What" section met (each box tickable from command output)
- [ ] setup() line-1 valve LOW verified via Level 4 awk check
- [ ] Unguarded region purity verified via Level 4 sed/grep check
- [ ] README steps complete enough for a fresh machine to build (core URL, board selection, sketch path, both toolchains, test command)
- [ ] Arduino IDE manual open/build confirmed no repair dialog (human, defer to bench if IDE unavailable — but README must be written now)

### Code Quality Validation

- [ ] Region markers + seam-contract header byte-stable for future structural tests
- [ ] Explicit prototypes block present; every function has one
- [ ] No scope creep: zero behavioral logic, zero new dependencies (no lib_deps), no extra envs
- [ ] .gitignore append-only; plan/, PRD.md, task files never ignored

### Documentation & Deployment

- [ ] README.md seeded (Mode A); notes P2.M1.T3.S1 will expand it
- [ ] No new environment variables introduced
- [ ] First-run download behavior (toolchain, Unity) documented in README so newcomers aren't surprised

---

## Anti-Patterns to Avoid

- ❌ Don't flatten to `firmware/jug_filler.ino` — Arduino IDE will demand a repair that silently moves the file and breaks PlatformIO + the config.h include (verified: sketch spec mandates folder==sketch name)
- ❌ Don't omit the explicit `main()` in test files — PIO 6.2.0 does not generate one (verified link failure)
- ❌ Don't drop `build_src_filter = -<*>` from `[env:native]` — the .ino would double-compile / fail to build
- ❌ Don't reorder setup() "for tidiness" — valve LOW is line 1 forever (PRD §10, asserted by P1.M7.T1.S1)
- ❌ Don't add `while(!Serial)`, `delay()`, watchdog, banner logs, or any feature logic — owned by later subtasks
- ❌ Don't create `lib_deps` entries for Unity — `test_framework = unity` auto-fetches it
- ❌ Don't run `pio run` bare (default env is native and src-excluded); board builds are `pio run -e pico`
- ❌ Don't edit config.h during relocation — byte-for-byte rename only

---

**Confidence Score: 9/10** — the exact artifacts shipped in this PRP (platformio.ini + .ino + test pattern + nested layout) were built and run on this machine: `pio test -e native` PASSED twice (flat layout, then the final nested layout); board id verified in the installed raspberrypi platform catalog; the only unverified steps are the first-run `pio run -e pico` toolchain download (board def standard; generic fallback documented) and the human Arduino IDE flow (spec-compliant layout; steps in README).
