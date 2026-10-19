# PRP — P1.M5.T2.S1: Drive GP14 from the pattern engine each loop pass

## Goal

**Feature Goal**: Connect the AUTHORITATIVE external green LED (GP14 → 330 Ω → LED anode, cathode → GND; PRD §2.1/§4) to the pure `LedPatternEngine` (P1.M5.T1.S1, already landed): ONE `tick(nowMs)` and ONE `digitalWrite(PIN_LED, ...)` per loop pass, non-blocking, mode-agnostic. This task is ONLY the pin driver — mode selection (`setMode`/`triggerAck`) belongs to the fill engine (P1.M6.T1.S2 routing in P1.M6.T2.S1), and the WS2812 mirror belongs to P1.M5.T3.S1. setup() ALREADY lands `pinMode(PIN_LED, OUTPUT)` + initial LOW (P1.M1.T1.S2 skeleton) — verify, do not touch.

**Deliverable**: `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive except one rewritten trailing comment. Exactly two insertions into the ARDUINO region:
1. A bannered state block `// ---- External LED driver state (P1.M5.T2.S1) ----` containing `static LedPatternEngine s_led;` (house pattern: `s_button`, `s_adc`), placed after the ADC state block, before `void setup()`.
2. The loop's LED stage (the LAST loop stage, per architecture line 31 `→ LED drivers (GP14 + WS2812 mirror)`): `const bool ledOn = s_led.tick(nowMs); digitalWrite(PIN_LED, ledOn ? HIGH : LOW);` using the loop's EXISTING single `nowMs` snapshot — plus an updated trailing integration comment telling P1.M5.T3.S1 / P1.M6.T2.S1 where to land.

NO new files. NO new native tests (item contract: "Mock: none on device; engine patterns native-tested in P1.M5.T1.S1" — the ARDUINO region is `#ifdef`-excluded from native builds anyway). NO config.h change. NO setup() change.

**Success Definition**:
- `pio run -e pico` → `[SUCCESS]` (the new ARDUINO-region code compiles under arm-gcc/arduino-pico).
- `pio test -e native` → exit 0, ALL 11 projects `[PASSED]` (10 predecessors + `test_led_pattern` from P1.M5.T1.S1) — regression only, zero new tests.
- Structure greps: LED stage present exactly once, sits after the ADC stage and before the closing brace of `loop()`; region-marker count still exactly 2; setup() still contains the two LED lines byte-identically.
- `git status --short`: ONLY `firmware/jug_filler/jug_filler.ino` modified.
- The loop's LED stage is ready for P1.M5.T3.S1 (mirror reuses `ledOn` beside the GP14 write) and P1.M6.T2.S1 (fill-engine routing inserts ABOVE the stage so same-pass `setMode` → `tick` works) with zero rework.

## User Persona

**Target User**: The implementing agents of P1.M5.T3.S1 (WS2812 mirror) and P1.M6.T2.S1 (integration loop) — this task is infrastructure they consume.

**Use Case**: Every loop pass, the LED stage advances the pattern engine once and writes the level to GP14. When the fill engine later selects DONE, the user sees the triple-pulse attention blink on the external LED until acknowledged (PRD §11: no timeout); during FILLING/CONFIRM it is solid; in FAULT it blink-codes; in IDLE it is off. None of that logic lives here — this driver faithfully writes whatever `tick()` returns.

**User Journey** (end user, once downstream tasks land): power-on → LED off (setup LOW + engine defaults to Off) → short press → fill engine `setMode(Solid)` → external LED solid → CONFIRM still solid → DONE → `setMode(DonePulse)` → 3x pulse + 1 s gap repeating → acknowledge press → off. This task makes GP14 faithfully render that stream.

**Pain Points Addressed**: Without a driver, the engine's boolean stream goes nowhere; a `delay()`-based blink would stall the watchdog-feeding loop (PRD §10 — forbidden); re-deriving levels per-driver would drift from §4.

## Why

- PRD §4: the external green LED is AUTHORITATIVE (WS2812 only mirrors). FILLING/CONFIRM solid; DONE triple-pulse until acknowledged; FAULT blink codes — all produced by the engine this driver consumes.
- PRD §2.1: GP14 → external LED anode via 330 Ω, cathode → GND. HIGH = on (pin sources ≈7–10 mA — ample within GPIO drive strength; item contract, hardware_validation.md line 80).
- PRD §10 / architecture line 53: no blocking in `loop()` — this stage is one function call plus one pin write.
- PRD §11: DONE persists until acknowledged, no timeout — engine's modulo handles "forever"; the driver needs no timeout logic either.
- Item contract: INPUT = LedPatternEngine (P1.M5.T1.S1) + skeleton (P1.M1.T1.S2); LOGIC = pinMode in setup (ALREADY LANDED — verify only) + `digitalWrite` each loop pass; OUTPUT = driven external LED consumed by the integration loop (P1.M6.T2.S1); DOCS = none.
- Architecture (firmware_architecture.md): line 31 — loop ends with `→ LED drivers (GP14 + WS2812 mirror) → logLine events`; line 40 — hardware code only in the `#ifdef ARDUINO` region.

## What

Two edits, both inside the `#ifdef ARDUINO` region; everything else byte-stable.

1. **State block** (after the ADC state block, before `void setup()`): one file-scope static `LedPatternEngine s_led;` in a bannered block — the exact house pattern of `s_button` (P1.M2.T2.S1) and `s_adc` (P1.M3.T1.S2). No prototype needed (class instances in the ARDUINO region get none; same precedents).
2. **LED stage in loop()** (after the ADC stage, as the final stage): compute `const bool ledOn = s_led.tick(nowMs);` from the loop's EXISTING `nowMs` snapshot (do NOT call `millis()` again — one clock read per pass), then `digitalWrite(PIN_LED, ledOn ? HIGH : LOW);`. Spell out HIGH/LOW (a bare bool would convert, but pin levels are the clearer idiom).
3. **Trailing comment rewrite**: the existing 3-line `// P1.M6.T2.S1 fills the loop below this line ...` comment is REPLACED (only deletion in this task's diff) with a version recording: LED drive landed here; P1.M6.T2.S1 inserts fill-engine routing ABOVE the LED stage (mode selection must precede `s_led.tick` in the same pass); P1.M5.T3.S1 adds the mirror beside the GP14 write reusing `ledOn`; nothing may ever be inserted above `watchdog_update()`.

NOT in scope (owners — do NOT implement here): mode selection / `setMode` / `triggerAck` calls (P1.M6.T1.S2 + P1.M6.T2.S1); WS2812 color map / brightness (P1.M5.T3.S1); any setup() edit (already correct); any config.h addition (§8 frozen — `PIN_LED` exists); any logging (mode-change logs belong to the fill engine); any native test (item contract); docs (P2.M1.T3.S1).

### Success Criteria

- [ ] `static LedPatternEngine s_led;` present exactly once, inside a `// ---- External LED driver state (P1.M5.T2.S1) ----` bannered block in the ARDUINO region, after the ADC state block
- [ ] loop() LED stage present exactly once, after `// ---- end ADC sample / aggregate / commissioning log ----`: one `s_led.tick(nowMs)` + one `digitalWrite(PIN_LED, ledOn ? HIGH : LOW)`, using the existing `nowMs`
- [ ] setup() byte-stable: `pinMode(PIN_LED, OUTPUT);` and `digitalWrite(PIN_LED, LOW);` still present unchanged (verify-only contract)
- [ ] The UNGUARDED region byte-stable; region-marker count exactly 2
- [ ] No `millis()` call added inside the LED stage; no `delay(` anywhere in loop; no new log calls in this stage
- [ ] `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, all 11 projects `[PASSED]`
- [ ] `git status --short` → only ` M firmware/jug_filler/jug_filler.ino`; the diff's only removed lines are the 3 old trailing-comment lines

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the two verbatim listings below are complete and were drafted against the REAL current file (995 lines, engine block landed at line 864, ARDUINO marker at line 866 — verified this session); both edit anchors are quoted by exact text (line numbers shift while P1.M5.T1.S1 finishes landing in parallel); the consumed surface (`LedPatternEngine::tick`, `PIN_LED`) was read from the landed .ino and config.h (line 25: `constexpr uint8_t PIN_LED = 14;`); the build/test commands (`pio run -e pico`, `pio test -e native`) are proven in every prior subtask's session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified landmarks (TEXT anchors — line numbers
        shift while P1.M5.T1.S1 finishes landing in parallel):
        (1) STATE-BLOCK ANCHOR — `// ---- end ADC sampling service state
        (used by loop, kept by P1.M6.T2.S1) ----`; insert your block AFTER
        it, BEFORE `void setup() {`.
        (2) LOOP ANCHOR — `// ---- end ADC sample / aggregate /
        commissioning log ----` inside loop(); insert the LED stage after
        it, REPLACING the 3-line trailing comment that starts
        `// P1.M6.T2.S1 fills the loop below this line`.
        (3) CONSUMED CLASS — `class LedPatternEngine` (UNGUARDED region,
        ends with the `// ===== end LED pattern engine ...` banner). READ
        its public surface; do NOT modify it.
        (4) SETUP — already contains `pinMode(PIN_LED, OUTPUT);` +
        `digitalWrite(PIN_LED, LOW);` (P1.M1.T1.S2). VERIFY ONLY.
        (5) LOOP CLOCK — `const uint32_t nowMs = millis();` is taken once
        near the top of loop(); your stage reuses it.
  gotcha: if `class LedPatternEngine` is MISSING or `test/test_led_pattern/`
        does not exist, P1.M5.T1.S1 has not finished landing — STOP and
        report; do not implement against a partial dependency.

- file: firmware/jug_filler/config.h
  why: READ-ONLY. Line 25: `constexpr uint8_t PIN_LED = 14;` (GP14). Do NOT
        add anything — §8 is frozen.
  gotcha: never hardcode 14 or GP14 in the .ino — use PIN_LED by name.

- file: plan/001_53984f832581/P1M5T2S1/research/notes.md
  why: this task's research: §1 verified tree state (incl. the mid-session
        T1.S1 landing), §2 frozen upstream API, §3 design decisions
        (single tick / shared ledOn / HIGH-LOW spelling / no tests / no
        logs), §4 electrical facts, §6 edit anchors.

- file: plan/001_53984f832581/P1M5T1S1/PRP.md
  why: the CONTRACT for the engine you consume — frozen public surface,
        tick()-once-per-pass caller contract (its Integration Points name
        THIS task: `digitalWrite(PIN_LED, led.tick(nowMs))`), lazy-anchor
        semantics (why same-pass setMode→tick is exact), boundary
        convention. Treat as already-implemented-exactly.
  section: "What" (public surface) + Integration Points (DOWNSTREAM API)

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: line 31 (loop stage order — LED drivers are the LAST loop
        stage), line 40 (hardware only in #ifdef ARDUINO), line 53 (no
        blocking delay in loop)
  why: justifies LED-stage placement at the end of loop() and the region
        placement of the static.

- url: https://arduino-pico.readthedocs.io/en/latest/io.html
  why: arduino-pico digital I/O — digitalWrite(pin, HIGH/LOW), GPIO levels
        at 3.3 V; confirms HIGH = pin sources ~3.3 V (LED on with the
        §2.1 anode wiring).
  critical: GP14 is a plain digital pin — no special setup beyond pinMode
        (already done in setup()); analogWrite/LED PWM is NOT used.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # 995 lines & GROWING (T1.S1 landing in parallel):
│   │                         #   UNGUARDED: seam + time utils + ButtonClassifier
│   │                         #   + ReadingAggregator + BaselineCapture + TripDetector
│   │                         #   + FalseTripGuards + backstopMinutes + RecoveryClock/
│   │                         #   FillDeadline + LedPatternEngine (line ~694-864)
│   │                         #   ARDUINO region (line 866+): logLine, BOOTSEL state,
│   │                         #   ADC state, setup(), loop()  ← EDIT (2 insertions)
│   └── config.h              # PIN_LED=14 line 25 — READ-ONLY
├── test/
│   ├── test_seam/ test_time_utils/ test_log_utils/ test_button/     # untouched
│   ├── test_reading/ test_baseline/ test_trip/ test_guards/         # untouched
│   ├── test_backstop/        # untouched, must stay green
│   ├── test_deadline/        # untouched, must stay green
│   └── test_led_pattern/     # P1.M5.T1.S1's — must EXIST & be green (precondition;
│                             #   was NOT yet present mid-research — verify Task 0)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, arm-gcc via pio,
# host g++ (C++17), Unity 2.6.1 (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (2 insertions + 1 comment rewrite):
│                             #   + External LED driver state block (static s_led)
│                             #   + loop LED stage: tick -> digitalWrite(PIN_LED)
│                             #   + updated trailing integration comment
└── (no new files — no test dir by design; see item contract)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — ONE tick per pass: `s_led.tick(nowMs)` advances the engine's
//   lazy anchors; the WS2812 mirror (P1.M5.T3.S1) MUST reuse the `ledOn`
//   local, never re-tick. (Double-tick with the same nowMs is coincidentally
//   harmless — pure phase math — but the single-tick shape is T1.S1's frozen
//   caller contract.)

// CRITICAL — reuse the loop's EXISTING `nowMs` snapshot (taken once near the
//   top of loop()). Do NOT call millis() inside the LED stage — one clock
//   read per pass is the house rule (BOOTSEL/ADC stages already follow it).

// CRITICAL — do NOT touch setup(): the item contract's "pinMode(PIN_LED,
//   OUTPUT) in setup (LED off initially)" ALREADY LANDED with the P1.M1.T1.S2
//   skeleton (`pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW);`).
//   Rewriting it adds diff churn and risks the boot-dark invariant.

// GOTCHA — the LED stage must be the LAST loop stage: P1.M6.T2.S1's fill
//   engine inserts its routing ABOVE the stage so a same-pass setMode() is
//   rendered by the same pass's write (the engine's lazy anchor makes this
//   exact — setMode releases the anchor, the next tick re-stamps it).

// GOTCHA — HIGH/LOW spelling: `digitalWrite(PIN_LED, ledOn)` would compile
//   (bool→int, HIGH=1/LOW=0) but pin levels spelled out are the clearer
//   idiom and match the item's intent.

// GOTCHA — no logging in this stage: a per-pass log would spam USB CDC;
//   mode-change logs belong to the fill engine at setMode() time (P1.M6.T2.S1).

// GOTCHA — never `pio test -e pico` (no on-device transport): native tests
//   only; `pio run -e pico` is the compile gate. On-LED visual bench
//   validation is P1.M7.T2.S1's deliverable, NOT this task's.

// GOTCHA — the ARDUINO region is excluded from native builds (#ifdef): your
//   new code never runs under `pio test -e native`; that suite is a
//   regression gate only.
```

## Implementation Blueprint

### Data models and structure

No new types. One static instance + one loop-local boolean:

```cpp
// ARDUINO region, file scope (between ADC state block and setup()):
static LedPatternEngine s_led;   // pattern engine instance; RAM state only (PRD §3)

// loop(), final stage:
const bool ledOn = s_led.tick(nowMs);        // advance pattern, get level
digitalWrite(PIN_LED, ledOn ? HIGH : LOW);   // GP14 -> 330R -> LED anode -> GND
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "class LedPatternEngine" firmware/jug_filler/jug_filler.ino   # EXPECT: 1
  - RUN: ls test/test_led_pattern/test_led_pattern.cpp                          # EXPECT: exists
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "pinMode(PIN_LED, OUTPUT);" firmware/jug_filler/jug_filler.ino # EXPECT: 1 hit, inside setup()
  - RUN: grep -n "fills the loop below" firmware/jug_filler/jug_filler.ino      # EXPECT: 1 hit (your loop anchor)
  - RUN: grep -n "end ADC sampling service state" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 hit (your state-block anchor)
  - RUN: pio test -e native                                                     # EXPECT: exit 0, all 11 projects [PASSED]
  - A miss on the first two means P1.M5.T1.S1 has not finished landing — STOP.
    A moved seam means the tree regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — insert the "State-block
  listing" verbatim AFTER the `// ---- end ADC sampling service state ... ----`
  line and BEFORE `void setup() {` (exactly ONE blank line at each junction).

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — insert the "Loop-stage
  listing" verbatim AFTER the `// ---- end ADC sample / aggregate /
  commissioning log ----` line, REPLACING the old 3-line trailing comment
  (`// P1.M6.T2.S1 fills the loop below this line ... inserted above
  watchdog_update().`). The listing includes its own replacement trailing
  comment. PRESERVE everything else byte-for-byte — especially setup(), the
  UNGUARDED region, and all other loop stages.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**State-block listing — insert verbatim (after the ADC state block, before `void setup() {`):**

```cpp
// ---- External LED driver state (P1.M5.T2.S1) ----
// Hardware side of LED signaling (PRD §4): the external green LED on GP14 is
// AUTHORITATIVE — the onboard WS2812 (P1.M5.T3.S1) only mirrors it. Wiring
// (PRD §2.1): GP14 -> 330R -> LED anode, cathode -> GND, so HIGH = on; the
// pin sources ~8 mA at 3.3 V, ample within GPIO drive strength. setup() already
// did pinMode(PIN_LED, OUTPUT) + LOW (P1.M1.T1.S2 skeleton — LED dark at boot);
// the engine defaults to Off, so IDLE stays dark until the fill engine
// (P1.M6.T1.S2, routed by P1.M6.T2.S1) selects a mode. This driver is
// mode-agnostic: it only writes tick()'s level each pass — pattern semantics
// (Solid through CONFIRM, DonePulse forever until acknowledged per PRD §11,
// FaultBlink groups per §7) all live in the pure engine, native-tested by
// P1.M5.T1.S1. Shared instance: the WS2812 mirror reads mode()/ledOn from the
// SAME engine — one tick per pass, never two.
static LedPatternEngine s_led;  // pattern engine instance; RAM state only (PRD §3)
// ---- end External LED driver state (used by loop, shared with P1.M5.T3.S1) ----
```

**Loop-stage listing — insert verbatim (after `// ---- end ADC sample / aggregate / commissioning log ----`, replacing the old trailing comment):**

```cpp
  // ---- External LED drive (P1.M5.T2.S1) — the loop's LED stage ----
  // ONE tick of the pattern engine per pass, ONE pin write (PRD §10: nothing
  // here may block; the engine is a pure function of nowMs). Uses the loop's
  // single nowMs snapshot — never a second millis() read. ledOn is the shared
  // level: the WS2812 mirror (P1.M5.T3.S1) writes its color BESIDE this
  // digitalWrite using the SAME ledOn — never re-tick. The stage sits LAST so
  // the fill engine (P1.M6.T2.S1, routing lands ABOVE this stage) can setMode()
  // and the same pass's write already reflects it — the engine's lazy phase
  // anchor makes same-pass setMode->tick exact (P1.M5.T1.S1 design).
  const bool ledOn = s_led.tick(nowMs);          // advance pattern, get level
  digitalWrite(PIN_LED, ledOn ? HIGH : LOW);     // GP14 -> 330R -> LED -> GND
  // ---- end External LED drive ----
  // P1.M6.T2.S1 fills the loop below this line (fill engine routing + event
  // logs via logLine — INSERT ABOVE the External LED drive so mode selection
  // precedes s_led.tick in the same pass; ADC sampling landed in P1.M3.T1.S2,
  // LED drive in P1.M5.T2.S1). Nothing may ever be inserted above
  // watchdog_update().
```

### Implementation Patterns & Key Details

```cpp
# PATTERN: bannered file-scope static in the ARDUINO region (s_button /
#   s_adc precedent): banner cites PRD sections + task ID; end-banner names
#   the consumers. No prototype entry (class instances get none — same as
#   ButtonClassifier/ReadingAggregator instances).

# CRITICAL: exactly ONE s_led.tick(nowMs) per loop pass; ledOn is shared
#   downstream (T3.S1 mirror). The engine's tick() lazily stamps the phase
#   anchor on the first pass after any setMode — placement AFTER the future
#   fill-engine stage makes same-pass mode changes render immediately.

# CRITICAL: setup() is verify-only — the LED-off-at-boot invariant already
#   landed with the skeleton (pinMode + LOW), and the engine's default Off
#   keeps it dark until the fill engine speaks.

# PATTERN: pin constant BY NAME (PIN_LED from config.h line 25); HIGH/LOW
#   spelled out on the write.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 state block (ARDUINO
    region, after ADC state block) + the loop LED stage (final stage) + the
    rewritten trailing integration comment. UNGUARDED region, setup(), all
    other stages byte-stable.
BUILD:
  - None (platformio.ini unchanged; pico env already compiles the sketch).
CONFIG:
  - None. Consumes PIN_LED (GP14) from config.h BY NAME. §8 frozen.
TEST:
  - None by design (item contract: "Mock: none on device; engine patterns
    native-tested in P1.M5.T1.S1"; ARDUINO region is native-excluded).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
DOWNSTREAM CONTRACTS (frozen by this task):
  - s_led — the ONE shared engine instance (P1.M5.T3.S1 mirror reads
    s_led.mode() and the loop's ledOn; P1.M6.T2.S1 routes setMode/triggerAck
    into it via the fill engine).
  - ledOn — the per-pass level local to loop()'s LED stage; the mirror write
    lands directly beside the GP14 write, reusing it.
  - Insertion point for P1.M6.T2.S1 — ABOVE the External LED drive stage
    (mode selection precedes the tick); recorded in the trailing comment.
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - setMode/triggerAck call sites (WHO/WHEN)   -> P1.M6.T1.S2 + P1.M6.T2.S1
  - WS2812 color map / brightness cap          -> P1.M5.T3.S1
  - Visual bench observation of patterns       -> P1.M7.T2.S1
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the ARDUINO-region additions compile under
# arm-gcc/arduino-pico. A compile error almost certainly means a transcription
# deviation from the listings (or T1.S1's class not actually landed) — fix,
# don't redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; ALL 11 projects [PASSED] — test_seam, test_time_utils,
# test_log_utils, test_button, test_reading, test_baseline, test_trip,
# test_guards, test_backstop, test_deadline, test_led_pattern.
# NOTE: this task adds NO tests BY DESIGN (item contract: no on-device mock;
# engine patterns native-tested in P1.M5.T1.S1; the ARDUINO region is
# #ifdef-excluded from native builds). This run is a REGRESSION gate: it
# proves the .ino edit did not disturb the UNGUARDED region any test touches.
# ANY failure: your edit leaked outside the ARDUINO region — inspect
# `git diff firmware/jug_filler/jug_filler.ino` for unintended changes.
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifact:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
# (nothing else; no new untracked dirs — there is no test dir by design)

# (2) The diff is additive except the 3 old trailing-comment lines:
git diff firmware/jug_filler/jug_filler.ino | grep "^-[^-]"
# Expected: exactly the 3 removed lines of the OLD trailing comment
# ("// P1.M6.T2.S1 fills the loop below this line ...", "// logs via
# logLine; ...", "// inserted above watchdog_update().") — nothing else.

# (3) State block landed once, after the ADC state block, before setup():
grep -c "External LED driver state (P1.M5.T2.S1)" firmware/jug_filler/jug_filler.ino   # 1
grep -n -A1 "end ADC sampling service state" firmware/jug_filler/jug_filler.ino | head -3
#   Expected: banner end-line, blank, then "// ---- External LED driver state"

# (4) LED stage landed once, at the end of loop():
grep -c "External LED drive (P1.M5.T2.S1)" firmware/jug_filler/jug_filler.ino          # 1
grep -c "digitalWrite(PIN_LED, ledOn ? HIGH : LOW)" firmware/jug_filler/jug_filler.ino # 1
grep -c "s_led.tick(nowMs)" firmware/jug_filler/jug_filler.ino                          # 1

# (5) setup() verify-only contract held:
git diff firmware/jug_filler/jug_filler.ino | grep -E "^[-+].*(pinMode\(PIN_LED|digitalWrite\(PIN_LED, LOW)" 
# Expected: empty (no +/- churn on the setup() LED lines)

# (6) Region markers intact and UNGUARDED region untouched:
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # 2
git diff firmware/jug_filler/jug_filler.ino | sed -n '/---- UNGUARDED/,/ARDUINO REGION/p'   # empty
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Non-blocking audit — the stage adds exactly one call + one write, no
#     delay, no extra clock read, no log:
sed -n '/External LED drive (P1.M5.T2.S1)/,/end External LED drive/p' \
  firmware/jug_filler/jug_filler.ino | sed 's://.*$::' \
  | grep -nE "delay|millis|logLine" || echo "STAGE CLEAN"
# Expected: STAGE CLEAN

# (2) Single-tick contract — the whole file ticks s_led exactly once per
#     loop pass (grep count 1 above) and the trailing comment tells
#     downstream tasks to reuse ledOn (grep):
grep -c "never re-tick\|SAME ledOn" firmware/jug_filler/jug_filler.ino   # >= 2

# (3) Reasoning audit (no command — check against this PRP):
#     - Boot-dark invariant: setup() LOW + engine default Off -> LED off in
#       IDLE until the fill engine speaks. ✔ (verified: T1.S1 test_off_default)
#     - Authoritative external LED: this driver writes GP14 every pass;
#       the WS2812 mirror is a separate future consumer of the same level. ✔
#     - DONE-persists / CONFIRM-solid / FAULT-blink semantics all come from
#       the engine's native-tested pattern functions — none re-implemented
#       here. ✔

# (4) Bench observation of the actual LED is P1.M7.T2.S1's deliverable
#     (hardware_validation.md line 96) — out of scope here by design.
```

## Final Validation Checklist

### Technical Validation

- [ ] All 4 validation levels completed successfully
- [ ] `pio run -e pico` → `[SUCCESS]`
- [ ] `pio test -e native` → exit 0, all 11 projects `[PASSED]` (no new tests, by design)
- [ ] `git status --short` → only ` M firmware/jug_filler/jug_filler.ino`

### Feature Validation

- [ ] All success criteria from "What" section met (greps of Level 3 all green)
- [ ] State block + LED stage match the verbatim listings; only deletion = old trailing comment
- [ ] setup() LED lines byte-identical (verify-only contract respected)
- [ ] Integration points recorded for P1.M5.T3.S1 (`ledOn` reuse) and P1.M6.T2.S1 (insert-above rule)

### Code Quality Validation

- [ ] Follows existing conventions: bannered ARDUINO-region static blocks (`s_button`/`s_adc` precedent), config constants BY NAME (`PIN_LED`), shared `nowMs` snapshot
- [ ] No anti-patterns: no `delay`, no second `millis()`, no per-pass logging, no hardcoded pin numbers or cadences
- [ ] No scope creep: no `setMode`/`triggerAck` call, no WS2812 code, no config edits, no test files, no docs

### Documentation & Deployment

- [ ] Block comments cite PRD §2.1/§4/§10/§11 and name downstream consumers (self-documenting, house style)
- [ ] Trailing integration comment updated — the next two implementers know their insertion points
- [ ] No new environment variables or config tunables (§8 frozen)

## Anti-Patterns to Avoid

- ❌ Don't call `setMode`/`triggerAck` anywhere in this task — mode selection is the fill engine's job (P1.M6.T1.S2/T2.S1); a stray call would fake state changes
- ❌ Don't add a second `millis()` read or a `delay()` — one shared clock snapshot per pass; nothing blocks (PRD §10)
- ❌ Don't re-init the pin in loop() or touch setup() — pinMode+LOW already landed (P1.M1.T1.S2)
- ❌ Don't re-tick the engine for the mirror or add per-pass logging — one tick, one write, shared `ledOn`
- ❌ Don't add native tests or mocks for this stage (item contract) — the compile gate + regression suite are the validation
- ❌ Don't hardcode 14/GP14 or any cadence number — `PIN_LED` by name; cadences live in the engine's config constants
- ❌ Don't "fix" or restyle the landed `LedPatternEngine` block — it is a frozen upstream contract
