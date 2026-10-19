# PRP — P1.M5.T1.S1: LedPatternEngine — Solid/Off/DonePulse/FaultBlink(n)/AckBlink overlay (pure)

## Goal

**Feature Goal**: Land the PRD §4/§7 LED-signaling contract as ONE pure, non-blocking class `LedPatternEngine` in the UNGUARDED region of the sketch: pattern vocabulary `Off` (IDLE), `Solid` (FILLING/CONFIRM), `DonePulse` (DONE — 3x on 200/off 200, then off 1000, repeating FOREVER until acknowledged, no timeout per §11), `FaultBlink(n)` (FAULT — n x on 250/off 250, then off 2000, repeat; n = fault code 1..3), plus the §4/§5.1 transient ack overlay (on a +5 long-press during FILLING: exactly 3 quick blinks at 100/100 = 600 ms total, then the base pattern resumes EXACTLY — a one-shot output mask that never disturbs the base phase, the valve, or fill timing). `setMode(Mode[, n])` and `triggerAck()` take NO timestamp (lazy phase anchor: the next `tick()` stamps it); `bool tick(uint32_t nowMs)` returns the LED level for that instant — zero `delay()`, zero hardware calls, injected clock, wrap-safe via the existing `elapsedMs` (P1.M1.T2.S1). All cadences derive from the §8 config constants BY NAME (`DONE_PULSE_ON_MS`, `DONE_PULSE_OFF_MS`, `DONE_PULSE_COUNT`, `DONE_GAP_MS`, `FAULT_BLINK_MS`, `FAULT_PAUSE_MS`, `ACK_BLINK_MS`) — ZERO new §8 constants (the ack count 3 is a class-local task constant, precedent `BaselineCapture::READINGS_PER_SENSOR`).

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: ONE bannered class block inserted at the END of the UNGUARDED region — directly ABOVE `// ---- #ifdef ARDUINO REGION: hardware glue ----` (line 712 of the current 841-line file; the line above the seam blank is P1.M4.T2.S1's LANDED end-banner `// ===== end recovery clock & fill deadline ... =====` at line 710 — commit 7c97ecb, verified in the tree). Exactly ONE blank line at each seam. NO prototype-block edit (classes get none — seam rule #3 is free-functions-only, ButtonClassifier/FillDeadline precedent).
- `test/test_led_pattern/test_led_pattern.cpp` — NEW: 12 Unity native tests proving: Off default+explicit; Solid at any age; DonePulse exact on/off instants at every boundary (t=0/199/200/399/400/599/600/799/800/999/1000/1199); the 1000 ms gap [1200,2200) and repetition (2200, 4400, still pulsing ~1 h in); fault groups n=1,2,3 exact shapes + 2000 ms pauses + cycle restarts; n=0 defensive clamp; ack overlay 3-blinks-in-600 ms table + boundary resume + one-shot flag; ack over a blinking base (phase undisturbed); mode change mid-pattern resets phase cleanly (twice); wrap-safe DonePulse + ack across the uint32 rollover. All expected values machine-verified this session against the REAL current `.ino` (block + test inserted into a copy, `g++ -std=c++17 -Wall -Wextra -Werror` + Unity 2.6.1, **12 tests / 0 failures** — research/notes.md §4).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_led_pattern` (11th project), plus all regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline, test_trip, test_guards, test_backstop, test_deadline).
- `pio run -e pico` → compile-only `[SUCCESS]` — the block compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2; NO ms literals in the class code lines.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_led_pattern/` added by this task.
- The frozen public surface (below) is ready for P1.M5.T2.S1 (GP14 driver: `digitalWrite(PIN_LED, led.tick(nowMs))`), P1.M5.T3.S1 (WS2812 mirror: color from `mode()`, level from `tick()`), and P1.M6.T1.S2 (fill engine: the ONLY `setMode`/`triggerAck` caller) with zero rework.

## User Persona

**Target User**: The implementing agents of the downstream consumers — P1.M5.T2.S1 (external green LED driver GP14), P1.M5.T3.S1 (config-gated WS2812 mirror GP16), P1.M6.T1.S2 (fill lifecycle engine that owns mode selection), P1.M6.T2.S1 (loop wiring).

**Use Case**: The fill engine enters FAULT with code 2 → calls `led.setMode(LedPatternEngine::FaultBlink, 2)`. Every loop pass the GP14 driver calls `digitalWrite(PIN_LED, led.tick(nowMs))` and the WS2812 mirror renders red at the same level — the user sees two blinks, a 2 s pause, repeat, until power cycle. During FILLING a +5 long-press fires → engine calls `led.triggerAck()` → the user sees 3 quick blinks (600 ms) and solid again, while the valve stays open and fill timing is untouched.

**User Journey**: IDLE (off) → short press → `setMode(Solid)` → filling (solid on; +5 long-press → `triggerAck()` → 3 quick blinks → solid) → CONFIRM (still Solid) → DONE → `setMode(DonePulse)` → triple-pulse + gap, repeating until the acknowledge press → `setMode(Off)`. Any fault path → `setMode(FaultBlink, code)`.

**Pain Points Addressed**: `delay()`-based blinking would stall the watchdog-feeding loop (PRD §10 — forbidden); per-driver re-implementations of the cadences would drift from §4; the ack transient would otherwise disturb the base pattern phase; non-wrap-safe phase math would glitch after ~49.7 days of uptime.

## Why

- PRD §4 (normative table): IDLE off; FILLING/CONFIRM external LED solid (WS2812 blue); DONE triple-pulse attention blink 3x(200/200) + off 1000, repeat until acknowledged; FAULT blink-code groups per §7. Transient ack: "+5min long-press during FILLING blinks the LED 3x quickly (on 100ms / off 100ms), then resumes solid."
- PRD §7 (normative): fault cadence N x (on 250/off 250), off 2000, repeat; N = fault code 1..3 (1 = sensor 1 bad, 2 = sensor 2 bad, 3 = timer no trip); faults LATCH (cleared only by RESET/power cycle — the pattern repeats forever).
- PRD §11: DONE has NO timeout — the attention pattern repeats until acknowledged; the engine's modulo-phase model gives "repeat forever" for free.
- PRD §8: every pattern cadence is a config.h constant — consumed BY NAME here, never re-hardcoded.
- PRD §3 / §10: non-blocking loop, watchdog fed every pass — the LED must be a per-pass `tick()`, zero `delay()`.
- Item contract: INPUT = time utilities (P1.M1.T2.S1) + config LED constants; OUTPUT = boolean ledOn stream + mode/ack API consumed by drivers (P1.M5.T2.S1, P1.M5.T3.S1), modes set only by the fill engine (P1.M6.T1.S2); DOCS = none (internal).
- Architecture (`firmware_architecture.md` line 22): `LedPatternEngine (P1.M5.T1.S1)` in the UNGUARDED region — pure logic, host-testable; line 31: LED drivers consume it in the loop's LED stage.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; P1.M5.T2.S1/P1.M5.T3.S1/P1.M6.T1.S2 PRPs will be written against it):

1. **`enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2, FaultBlink = 3 }`** — stable values (logging/mirroring).
2. **`void setMode(Mode mode, uint8_t faultGroups = 1)`** — select the pattern. `FaultBlink` carries its group count n = fault code 1..3 (PRD §7 table); every other mode ignores it. `n == 0` clamps to 1 (an empty group would render "off forever" — a latched fault must never look dead). Releases the phase anchor: the NEXT `tick()` stamps `phaseStartMs_` and the new pattern starts from its t=0 (LED on for every blink pattern) — a mode change mid-pattern resets the phase cleanly, with no timestamp parameter.
3. **`Mode mode() const` / `uint8_t faultGroups() const`** — WS2812 color-map inputs (Off→off, Solid→blue, DonePulse→green, FaultBlink→red, PRD §4) and fault-code diagnostics.
4. **`void triggerAck()`** — start (or restart) the one-shot +5 ack overlay: exactly `ACK_BLINK_COUNT` blinks at `ACK_BLINK_MS` cadence (600 ms total), then the base pattern shows again. NOT cancelled by `setMode` (strictly time-bounded).
5. **`bool ackActive() const`** — true while the overlay is still masking (reflects state as of the last `tick()`; the one-shot completes ON the first `tick()` at t ≥ 600).
6. **`bool tick(uint32_t nowMs)`** — advance to `nowMs`, return the LED level for this instant. One call per loop pass. Lazily anchors the pattern phase (and the ack window) at the first tick after `setMode`/`triggerAck` — the anchor is the loop's injected `nowMs`, never `millis()` here (seam rule).
7. **Phase model (the core design)**: the base pattern is a PURE FUNCTION `elapsedMs(nowMs, phaseStartMs_) % cycleLen` — wrap-safe by construction (true for any mode age < 2^32 ms ≈ 49.7 days); DONE repeats forever and FAULT latches via the modulo, no accumulated state. The ack overlay only MASKS the output for 600 ms — the base phase keeps running underneath, so the base "resumes exactly" (item contract wording) by construction.
8. **Boundary convention (house style, cf. `deadlinePassed`/`readingDue`)**: a segment boundary instant belongs to the NEW segment — on-windows are `[k*P, k*P+ON)`: t=199 on, t=200 off; at exactly t=600 the overlay is already over and the base shows.
9. **`static constexpr uint8_t ACK_BLINK_COUNT = 3`** — class-local task constant (PRD §4 "3x quickly"), NOT a §8 tunable; precedent `BaselineCapture::READINGS_PER_SENSOR`. Zero new config constants.

NOT in scope (owners): GP14 digitalWrite driver (P1.M5.T2.S1); WS2812 mirror/color map/brightness (P1.M5.T3.S1); WHO calls setMode/triggerAck and WHEN — the fill engine's state transitions (P1.M6.T1.S2); loop wiring (P1.M6.T2.S1); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `class LedPatternEngine` present exactly once in the UNGUARDED region (single bannered block directly above the ARDUINO marker, directly below T2S1's landed end-banner); NO prototype-block change
- [ ] Default-constructed engine is Off; `setMode(Off)` identical; Solid on at every instant (an hour in included)
- [ ] DonePulse boundaries exact: on [0,200), off [200,400), on [400,600), off [600,800), on [800,1000), off [1000,1200); gap off [1200,2200); on again at 2200 and 4400; still pulsing ~1 h in (no timeout)
- [ ] FaultBlink n=1/2/3: blinks on [0,250),[500,750),[1000,1250) as n grows; 2000 ms pause; cycle restarts at n*500+2000; `faultGroups()` echoes n; n=0 clamps to 1
- [ ] Ack overlay: exactly 3 blinks in 600 ms (on/off at every 100 ms boundary), `ackActive()` true through t=599, base (Solid) ALREADY on at exactly t=600, one-shot never refires; over a blinking base the base phase is undisturbed (DonePulse gap elapsed under the mask)
- [ ] Mode change mid-pattern resets the phase cleanly (from the DonePulse gap, FaultBlink(2) restarts at its t=0; a further Solid change lights immediately)
- [ ] Wrap-safe: DonePulse anchored 256 ms below the uint32 rollover keeps its exact shape across the wrap; the ack overlay's blinks and resume land post-wrap correctly
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region; NO ms literals (200/250/1000/2000/100/600) in the class code lines (600 computed via `ackSpanMs()`)
- [ ] `test/test_led_pattern/test_led_pattern.cpp` exists with explicit `int main()` Unity runner; all 12 listed tests pass
- [ ] `pio test -e native` exit 0, all 11 projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` one block + new test dir only)
- [ ] config.h, platformio.ini, prototype block, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete class listing and the complete 12-test file are provided verbatim below — both were inserted into a copy of the REAL current `.ino` (841 lines, P1.M4.T2.S1 already landed at commit 7c97ecb) and compiled with `g++ -std=c++17 -Wall -Wextra -Werror` + Unity 2.6.1 and executed green (**12 tests / 0 failures**) on this host this session (research/notes.md §4); the insertion anchor (`// ---- #ifdef ARDUINO REGION: hardware glue ----` at line 712, preceded by T2S1's landed end-banner at line 710 + one blank at 711) was verified in the current file; the consumed surface (`elapsedMs` and the seven §6 config constants) was read from the landed files; `pio test -e native` / `pio run -e pico` mirror commands proven working in every prior landed subtask's session (most recently P1.M4.T2.S1 — test_deadline is green in the current tree).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified-current landmarks:
        (1) BLOCK ANCHOR — `// ---- #ifdef ARDUINO REGION: hardware glue ----`
        (line 712). Insert your block DIRECTLY ABOVE it, with exactly ONE
        blank line at each seam. The line above the seam blank is P1.M4.T2.S1's
        LANDED end-banner `// ===== end recovery clock & fill deadline
        (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====` (line 710). Do NOT
        modify that block or any other landed block.
        (2) HELPER you reuse (do NOT reimplement): `elapsedMs(uint32_t now,
        uint32_t since)` (prototype line 32) — rollover-safe mod-2^32
        subtraction; the ONLY time math this class needs (no absolute
        deadlines → deadlinePassed is not used).
  gotcha: if T2S1's end-banner is MISSING, the tree regressed mid-plan —
        your block has NO hard dependency on it (insert above the ARDUINO
        marker either way), but STOP and report before improvising anchors.
        If the ARDUINO marker itself is missing, the seam moved — STOP.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of the seven §6 LED constants you consume BY NAME:
        DONE_PULSE_ON_MS=200, DONE_PULSE_OFF_MS=200 (uint32_t, lines ~118-
        119), DONE_PULSE_COUNT=3 (uint8_t), DONE_GAP_MS=1000,
        FAULT_BLINK_MS=250, FAULT_PAUSE_MS=2000, ACK_BLINK_MS=100. Do NOT
        add anything — §8 is frozen.
  gotcha: DONE_PULSE_COUNT is uint8_t and ACK counts multiply uint32_t
        cadences — widen with (uint32_t) BEFORE multiplying (the listing
        does). Never re-hardcode a cadence number.

- file: test/test_deadline/test_deadline.cpp
  why: THE most recent sibling test pattern (landed by P1.M4.T2.S1):
        relative `.ino` include, empty setUp/tearDown, hand-derived
        expected values with derivation comments, ITEM CASE tags on
        contract-pinned tests, explicit `int main() { UNITY_BEGIN();
        RUN_TEST(...); ... return UNITY_END(); }` (REQUIRED on PIO 6.2.0 —
        no auto-generated runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity
        project, auto-discovered — no platformio.ini change needed.

- file: plan/001_53984f832581/P1M5T1S1/research/notes.md
  why: this task's research: §1 contract→codebase mapping (verified line
        numbers), §2 design decisions (phase-function model, lazy anchor,
        ack-as-mask, boundary convention, ACK_BLINK_COUNT, groups clamp,
        mirror accessors), §3 the two test-expectation pitfalls found
        during verification (t=600 boundary; wrap-offset arithmetic),
        §4 machine-verification record, §5 downstream API freeze.
  section: §2 (design) and §3 (pitfalls)

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: line 22 ("LedPatternEngine (P1.M5.T1.S1)"), line 31 (LED
        drivers in the loop's LED stage), §1 seam rules (pure logic, no
        hardware calls; classes need no prototypes).
  why: justifies UNGUARDED-region placement, the tick()-per-loop-pass
        shape, and the driver/consumer split.

- url: https://docs.platformio.org/en/stable/plus/unit-testing.html
  why: PlatformIO native Unity conventions — test_dir auto-discovery, one
        project per test/ subdir, explicit runner requirement on PIO 6.2.0.
  critical: never `pio test -e pico` (no on-device transport) — native
        tests only.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # 841 lines: seam + time helpers (elapsedMs,
│   │                         #   deadlinePassed) + ButtonClassifier +
│   │                         #   ReadingAggregator/medianOf + BaselineCapture
│   │                         #   + TripDetector + FalseTripGuards
│   │                         #   + backstopMinutes (P1.M4.T1.S1)
│   │                         #   + RecoveryClock/FillDeadline (P1.M4.T2.S1 —
│   │                         #     LANDED, end-banner line 710, sits directly
│   │                         #     above YOUR insertion point)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service,
│   │                         #     ADC wiring, setup/loop
│   │                         # ← EDIT (UNGUARDED region +1 block; NO prototype)
│   └── config.h              # every §8 tunable — READ-ONLY
│                             #   (seven §6 LED constants by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/  test_baseline/  test_trip/  test_guards/       # untouched
│   ├── test_backstop/        # P1.M4.T1.S1's — untouched, must stay green
│   ├── test_deadline/        # P1.M4.T2.S1's — untouched, must stay green
│   └── test_led_pattern/     # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): +1 bannered block:
│                             #   LedPatternEngine — Off/Solid/DonePulse/
│                             #   FaultBlink(n) pattern functions + one-shot
│                             #   ack overlay; lazy phase anchor; wrap-safe
└── test/
    └── test_led_pattern/
        └── test_led_pattern.cpp  # NEW: native Unity proof — boundary
                                  #   instants, gap/pause repetition, fault
                                  #   groups 1/2/3, ack overlay + exact
                                  #   resume, phase reset, rollover wrap
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — setMode/triggerAck take NO timestamp BY DESIGN (item contract):
//   the phase anchor is RELEASED on setMode and stamped LAZILY by the next
//   tick(nowMs). Never add a nowMs parameter, and never stamp the anchor
//   inside setMode — there is no clock there (pure seam). Consequence: the
//   pattern's t=0 is the first loop pass after the mode change (sub-ms skew
//   at 100 ms cadences — invisible).

// CRITICAL — the ack overlay MASKS, it does not freeze: base output is
//   evaluated live every tick even while masked, so the base phase keeps
//   running and "resumes exactly" at t=600. Do not pause/restart the base
//   pattern on triggerAck — that would break "resumes exactly".

// CRITICAL — boundary instants belong to the NEW segment (house style,
//   cf. deadlinePassed inclusive-at-equality): on-windows are [k*P, k*P+ON).
//   t=200 → already OFF; t=2200 → already the next cycle's first pulse;
//   t=600 into the ack → base ALREADY showing. The first test draft got
//   t=600 wrong (expected off) — the machine-verified listing has it right.

// CRITICAL — wrap tests must compute post-rollover instants as
//   `anchor + k` (unsigned arithmetic wraps for you). Hand-computed hex
//   timestamps drop the pre-wrap remainder (a raw post-wrap t of 0x100 is
//   elapsed 0x200 when anchored at 0xFFFFFF00) — the second test-draft bug.

// GOTCHA: classes get NO entry in the explicit-prototype block (seam rule
#   #3 covers FREE functions — elapsedMs/formatBootBanner/medianOf/
#   backstopMinutes precedent; ButtonClassifier & FillDeadline got none).

// GOTCHA: DONE/FAULT repeat FOREVER (§11 no timeout; faults latch §7) —
//   that is the modulo in baseOn, NOT a loop or a counter. Do not add a
//   "cycles" cap.

// GOTCHA: n==0 for FaultBlink clamps to 1 INSIDE setMode (an empty group
#   would read "off forever" — a latched fault must never look dead).

// GOTCHA: host-verification compile needs BOTH Unity files:
#   g++ ... -I<Unity>/src test.cpp <Unity>/src/unity.c   (unity.h alone links
#   to undefined UnityAssertEqualNumber).

# GOTCHA: never `pio test -e pico` (no on-device transport) — native tests
#   only; `pio run -e pico` is the compile gate.
```

## Implementation Blueprint

### Data models and structure

One pure class, no new types beyond the Mode enum, no dynamic memory:

```cpp
LedPatternEngine:
  Mode     mode_;          // Off/Solid/DonePulse/FaultBlink (fill engine sets)
  uint8_t  faultGroups_;   // n for FaultBlink (fault code 1..3, PRD §7)
  uint32_t phaseStartMs_;  // pattern phase anchor (stamped lazily by tick)
  bool     phaseAnchored_; // false after setMode — next tick re-stamps
  uint32_t ackStartMs_;    // ack overlay anchor (stamped lazily by tick)
  bool     ackAnchored_;   // false after triggerAck — next tick re-stamps
  bool     ackActive_;     // overlay masking the base pattern right now
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino  # EXPECT: line 712 (informational — insert above it wherever it is)
  - RUN: grep -c "end recovery clock & fill deadline" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (P1.M4.T2.S1 LANDED — expected neighbor; if 0, no hard dependency, insert above the marker anyway but REPORT)
  - RUN: grep -c "class LedPatternEngine" firmware/jug_filler/jug_filler.ino  # EXPECT: 0 (not yet landed)
  - RUN: grep -cE "DONE_PULSE_ON_MS|DONE_PULSE_OFF_MS|DONE_PULSE_COUNT|DONE_GAP_MS|FAULT_BLINK_MS|FAULT_PAUSE_MS|ACK_BLINK_MS" firmware/jug_filler/config.h  # EXPECT: 7
  - RUN: pio test -e native                                                  # EXPECT: exit 0, all 10 projects [PASSED]
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — ONE additive edit
  - Insert the "Task 1 listing" verbatim directly ABOVE
    `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with exactly ONE
    blank line above (after T2S1's `end recovery clock & fill deadline`
    banner) and ONE below (before the ARDUINO marker). Do NOT double-blank
    the seam. Do NOT touch the prototype block.
  - PRESERVE: everything else byte-for-byte — includes, prototype block, all
    other UNGUARDED sections (incl. backstopMinutes, RecoveryClock,
    FillDeadline), the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and its tests executed
    against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_led_pattern/test_led_pattern.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (12 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_led_pattern follows the
    test_backstop/test_deadline convention. PIO auto-discovers the new
    project — no build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — insert verbatim (directly above the ARDUINO marker):**

```cpp
// ===================== LED pattern engine (P1.M5.T1.S1) =====================
// The external-LED contract of PRD §4/§7 as ONE pure, non-blocking class:
// it turns "what state are we in" into a boolean LED level at each loop
// pass — zero delay(), zero hardware calls, injected clock (seam rules).
// The drivers (P1.M5.T2.S1 GP14, P1.M5.T3.S1 WS2812 mirror) just write
// tick()'s return; the fill engine (P1.M6.T1.S2) is the ONLY setMode
// caller (item contract: modes are state, not user input).
//
// Pattern vocabulary (all cadences are config.h §8 constants, by name):
//   Off        IDLE — always off.
//   Solid      FILLING / CONFIRM — always on (PRD §4 "solid on (continues)").
//   DonePulse  DONE — 3x (on 200 / off 200) then off 1000, repeat, FOREVER
//              until acknowledged (no timeout, PRD §11).
//   FaultBlink(n) FAULT — n x (on 250 / off 250) then off 2000, repeat; n is
//              the fault code 1..3 (PRD §7 table). Latched like the fault.
//   ack overlay one-shot transient (PRD §4/§5.1): on a +5 long-press during
//              FILLING, 3x (on 100 / off 100) = exactly 600 ms, then the
//              base pattern resumes EXACTLY where its phase is — the overlay
//              only masks the output, it never disturbs the base phase, the
//              valve, or the fill timing.
//
// Phase model: the active pattern is a PURE FUNCTION of elapsedMs(nowMs,
// phaseStartMs_) % cycleLength — wrap-safe by construction (P1.M1.T2.S1;
// true for any mode age < 2^32 ms ≈ 49.7 days). setMode() releases the
// anchor; the NEXT tick() re-stamps it, so a mode change mid-pattern resets
// the phase cleanly with no timestamp parameter (setMode runs inside the
// engine step, tick runs in the same loop pass — sub-ms skew, invisible at
// 100 ms cadences). Segment boundaries take effect AT the boundary instant
// (house style, cf. deadlinePassed/readingDue): on-windows are [k*P, k*P+ON).
//
// Caller contract (P1.M5.T2.S1 / P1.M5.T3.S1, driven once per loop pass by
// the LED stage; nowMs non-decreasing, same clock domain as all services):
//   digitalWrite(PIN_LED, led.tick(nowMs));        // external green LED
//   mirror: color from led.mode(), level from tick() // WS2812 (T3.S1)
// The fill engine (P1.M6.T1.S2): setMode(Off/Solid/DonePulse), FAULT ->
// setMode(FaultBlink, code); +5 long-press during FILLING -> triggerAck().
class LedPatternEngine {
public:
    // Pattern vocabulary (PRD §4). Values are stable for logging/mirroring.
    enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2, FaultBlink = 3 };

    // PRD §4 transient ack: "blinks the LED 3x quickly". The count is a task
    // contract, NOT a §8 tunable (only ACK_BLINK_MS is config) — precedent:
    // BaselineCapture::READINGS_PER_SENSOR.
    static constexpr uint8_t ACK_BLINK_COUNT = 3;

    LedPatternEngine()
        : mode_(Mode::Off), faultGroups_(1),
          phaseStartMs_(0), phaseAnchored_(false),
          ackStartMs_(0), ackAnchored_(false), ackActive_(false) {}

    // Select the pattern. FaultBlink carries its group count n = fault code
    // (1..3, PRD §7 table); every other mode ignores it. n == 0 clamps to 1
    // (an empty group would read as "off forever" — the fail-safe direction
    // for a latched fault is to blink, never to look dead). Releases the
    // phase anchor: the next tick() stamps phaseStartMs_ and the new pattern
    // starts from its t=0 (LED on for every blink pattern).
    void setMode(Mode mode, uint8_t faultGroups = 1) {
        mode_ = mode;
        faultGroups_ = (faultGroups == 0) ? 1 : faultGroups;
        phaseAnchored_ = false;
    }

    // Current pattern (WS2812 color map input, P1.M5.T3.S1: Off -> off,
    // Solid -> blue, DonePulse -> green, FaultBlink -> red, PRD §4).
    Mode mode() const { return mode_; }

    // Group count of the active FaultBlink (fault code 1..3).
    uint8_t faultGroups() const { return faultGroups_; }

    // Start (or restart) the one-shot +5 ack overlay. Strictly time-bounded:
    // exactly ACK_BLINK_COUNT blinks at ACK_BLINK_MS cadence, then the base
    // pattern shows again. NOT cancelled by setMode — a state change during
    // the 600 ms simply shows through afterwards.
    void triggerAck() {
        ackActive_ = true;
        ackAnchored_ = false;
    }

    // True while the ack overlay is still masking the base pattern.
    bool ackActive() const { return ackActive_; }

    // Advance to nowMs, return the LED level for this instant. One call per
    // loop pass. Lazily anchors the pattern phase (and the ack window) at
    // the first tick after setMode/triggerAck — the anchor is the loop's
    // nowMs snapshot, never millis() here (seam rule).
    bool tick(uint32_t nowMs) {
        if (!phaseAnchored_) {
            phaseStartMs_ = nowMs;
            phaseAnchored_ = true;
        }
        if (ackActive_) {
            if (!ackAnchored_) {
                ackStartMs_ = nowMs;
                ackAnchored_ = true;
            }
            const uint32_t t = elapsedMs(nowMs, ackStartMs_);
            if (t < ackSpanMs()) {
                return (t % (2u * ACK_BLINK_MS)) < ACK_BLINK_MS;
            }
            ackActive_ = false;  // one-shot complete — fall through to base
        }
        return baseOn(nowMs);
    }

private:
    // Base pattern level at nowMs. Elapsed-since-anchor mod cycle: the
    // pattern repeats forever (DONE has no timeout, PRD §11; FAULT latches,
    // PRD §7). Unsigned % on a true elapsed span is exact and wrap-safe.
    bool baseOn(uint32_t nowMs) const {
        switch (mode_) {
            case Mode::Solid:
                return true;
            case Mode::DonePulse: {
                // cycle: 3x(200 on/200 off)=1200, then 1000 off -> 2200.
                const uint32_t period =
                    DONE_PULSE_ON_MS + DONE_PULSE_OFF_MS;
                const uint32_t groups =
                    (uint32_t)DONE_PULSE_COUNT * period;
                const uint32_t t =
                    elapsedMs(nowMs, phaseStartMs_) % (groups + DONE_GAP_MS);
                if (t >= groups) return false;  // the 1000 ms gap
                return (t % period) < DONE_PULSE_ON_MS;
            }
            case Mode::FaultBlink: {
                // cycle: n x(250 on/250 off)=n*500, then 2000 off.
                const uint32_t period = 2u * FAULT_BLINK_MS;
                const uint32_t groups = (uint32_t)faultGroups_ * period;
                const uint32_t t =
                    elapsedMs(nowMs, phaseStartMs_) % (groups + FAULT_PAUSE_MS);
                if (t >= groups) return false;  // the 2000 ms pause
                return (t % period) < FAULT_BLINK_MS;
            }
            case Mode::Off:
            default:
                return false;
        }
    }

    // Total ack overlay width: 3 blinks x (100 on + 100 off) = 600 ms.
    static constexpr uint32_t ackSpanMs() {
        return (uint32_t)ACK_BLINK_COUNT * 2u * ACK_BLINK_MS;
    }

    Mode     mode_;          // active pattern (set only by the fill engine)
    uint8_t  faultGroups_;   // n for FaultBlink (fault code 1..3, PRD §7)
    uint32_t phaseStartMs_;  // pattern phase anchor (stamped by tick)
    bool     phaseAnchored_; // false after setMode — next tick re-stamps
    uint32_t ackStartMs_;    // ack overlay anchor (stamped by tick)
    bool     ackAnchored_;   // false after triggerAck — next tick re-stamps
    bool     ackActive_;     // overlay masking the base pattern right now
};
// ===== end LED pattern engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====
```

**Task 2 listing — `test/test_led_pattern/test_led_pattern.cpp` (verbatim):**

```cpp
// test/test_led_pattern/test_led_pattern.cpp — native contract proof for
// LedPatternEngine (P1.M5.T1.S1). Pins PRD §4/§7/§11: Solid always on; Off
// always off; DonePulse = 3x(200 on/200 off) + 1000 off repeating forever
// (no timeout); FaultBlink(n) = n x(250 on/250 off) + 2000 off repeating,
// n = 1..3; ack overlay = exactly 3 blinks at 100/100 (600 ms total) then
// the base pattern resumes EXACTLY; mode change mid-pattern resets the
// phase cleanly; all arithmetic wrap-safe across the uint32 rollover.
// Boundary convention (house style): a segment boundary instant belongs to
// the NEW segment — on-windows are [k*P, k*P+ON): t=199 on, t=200 off.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ITEM CASE: IDLE — default-constructed engine is Off: off at any instant,
// forever. Explicit setMode(Off) is identical.
void test_off_default_and_explicit() {
    LedPatternEngine led;                      // boots Off (IDLE, PRD §4)
    TEST_ASSERT_EQUAL_INT(false, led.tick(5000u));   // anchors phase
    TEST_ASSERT_EQUAL_INT(false, led.tick(5001u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(999999u));
    TEST_ASSERT_EQUAL_INT(LedPatternEngine::Off, led.mode());

    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(10000u));   // anchors, Solid on
    led.setMode(LedPatternEngine::Off);
    TEST_ASSERT_EQUAL_INT(false, led.tick(10001u));  // re-anchors, off
    TEST_ASSERT_EQUAL_INT(false, led.tick(1049999u));
}

// ITEM CASE: FILLING/CONFIRM — Solid is on at every instant, any age.
void test_solid_always_on() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor
    TEST_ASSERT_EQUAL_INT(true, led.tick(1u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3600u * 1000u));  // an hour in
}

// ITEM CASE: DONE pulse — exact on/off instants of the first group and the
// boundaries between pulses: on [0,200), off [200,400), on [400,600), off
// [600,800), on [800,1000), off [1000,1200) (PRD §4 3x on200/off200).
void test_done_pulse_boundaries() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    static const uint32_t t[] = {0, 199, 200, 399, 400, 599, 600,
                                 799, 800, 999, 1000, 1199};
    static const bool on[] = {true, true, false, false, true, true, false,
                              false, true, true, false, false};
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(on[i], led.tick(t[i]));
    }
}

// ITEM CASE: DONE gap + repetition — off for the WHOLE 1000 ms gap
// [1200,2200), on again at exactly 2200 (second cycle), same shape, and
// still cycling an hour later (no timeout, PRD §11).
void test_done_pulse_gap_and_repeat() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor
    TEST_ASSERT_EQUAL_INT(false, led.tick(1200u));      // gap starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(2199u));      // last gap ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(2200u));       // cycle 2 starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2400u));      // same shape: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(2600u));       // pulse 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(4399u));      // gap of cycle 2
    TEST_ASSERT_EQUAL_INT(true, led.tick(4400u));       // cycle 3
    // Cycle N start = N*2200. 1636 cycles ~ one hour — still pulsing:
    TEST_ASSERT_EQUAL_INT(true, led.tick(1636u * 2200u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1636u * 2200u + 1200u));
}

// ITEM CASE: FAULT code 1 — one group: on [0,250), off [250,500), pause
// [500,2500), repeat from 2500 (PRD §7 n=1).
void test_fault_one_group() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 1);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor, group starts
    TEST_ASSERT_EQUAL_INT(true, led.tick(249u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(250u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(499u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(500u));       // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2499u));      // last pause ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(2500u));       // cycle 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(2750u));
}

// ITEM CASE: FAULT code 2 — two groups of blinks then the 2000 ms pause:
// on [0,250), off [250,500), on [500,750), off [750,1000), pause
// [1000,3000), repeat from 3000.
void test_fault_two_groups() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 2);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(250u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(500u));        // blink 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(750u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1000u));      // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2999u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3000u));       // cycle 2, blink 1
    TEST_ASSERT_EQUAL_INT(true, led.tick(3500u));       // cycle 2, blink 2
}

// ITEM CASE: FAULT code 3 — three blinks then pause: on [0,250), [500,750),
// [1000,1250); pause [1500,3500); repeat from 3500.
void test_fault_three_groups() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 3);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(260u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(510u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(760u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(1010u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1260u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));      // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(3499u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3500u));       // cycle 2
    TEST_ASSERT_EQUAL_INT(LedPatternEngine::FaultBlink, led.mode());
    TEST_ASSERT_EQUAL_UINT8(3, led.faultGroups());
}

// Defensive: setMode(FaultBlink, 0) clamps to one group (an empty group
// would read "off forever" — a latched fault must never look dead).
void test_fault_zero_groups_clamps_to_one() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 0);
    TEST_ASSERT_EQUAL_UINT8(1, led.faultGroups());
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(260u));       // 1-group shape
    TEST_ASSERT_EQUAL_INT(false, led.tick(2000u));      // inside the pause
    TEST_ASSERT_EQUAL_INT(true, led.tick(2500u));       // cycle 2 starts
}

// ITEM CASE: +5 ack overlay — exactly 3 blinks at 100/100 (600 ms total)
// masking Solid, then Solid resumes EXACTLY at t=600 and stays on. The
// overlay is one-shot: ackActive() drops when it completes.
void test_ack_overlay_then_solid_resumes_exactly() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(10000u));      // anchor Solid
    led.triggerAck();
    TEST_ASSERT_TRUE(led.ackActive());
    // First tick after triggerAck anchors the overlay at 20000:
    static const uint32_t t[] = {20000, 20099, 20100, 20199, 20200, 20299,
                                 20300, 20399, 20400, 20499, 20500, 20599};
    static const bool on[] = {true, true, false, false, true, true,
                              false, false, true, true, false, false};
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(on[i], led.tick(t[i]));
    }
    TEST_ASSERT_EQUAL_INT(false, led.tick(20599u));     // LAST overlay ms: off
    TEST_ASSERT_TRUE(led.ackActive());                 // still armed at 599
    TEST_ASSERT_EQUAL_INT(true, led.tick(20600u));     // boundary -> Solid ALREADY on
    TEST_ASSERT_FALSE(led.ackActive());                // one-shot complete
    TEST_ASSERT_EQUAL_INT(true, led.tick(20601u));      // ...and stays on
    TEST_ASSERT_EQUAL_INT(true, led.tick(50000u));
}

// Ack over a BLINKING base: the base phase keeps running under the mask —
// DonePulse's gap elapsed during the 600 ms overlay, so at overlay end the
// base is exactly where it would have been (PRD §4 "resumes", generalized).
void test_ack_over_blinking_base_phase_undisturbed() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor at 0
    led.triggerAck();                                   // during pulse 1
    // Overlay anchors at the next tick = 100: 3 blinks until 700.
    TEST_ASSERT_EQUAL_INT(true, led.tick(100u));        // overlay blink 1
    TEST_ASSERT_EQUAL_INT(false, led.tick(200u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(300u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(400u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(600u));
    // At 700 the base shows again: base t=700 -> [600,800) off. Base phase
    // ran continuously under the mask (700 % 2200 = 700 -> off).
    TEST_ASSERT_EQUAL_INT(false, led.tick(700u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(800u));        // base pulse 3
    TEST_ASSERT_EQUAL_INT(false, led.tick(1200u));      // base gap
}

// ITEM CASE: mode change mid-pattern resets phase cleanly — from deep in
// the DonePulse gap, FaultBlink(2) restarts at its t=0 (LED on), and a
// further Solid change lights immediately.
void test_mode_change_mid_pattern_resets_phase() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));      // mid-gap
    led.setMode(LedPatternEngine::FaultBlink, 2);       // mid-pattern change
    TEST_ASSERT_EQUAL_INT(true, led.tick(1600u));       // new phase t=0: on
    TEST_ASSERT_EQUAL_INT(false, led.tick(1600u + 260u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(1600u + 500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1600u + 1000u));  // pause
    led.setMode(LedPatternEngine::Solid);               // again, mid-pause
    TEST_ASSERT_EQUAL_INT(true, led.tick(2000u));       // instantly on
}

// ITEM CASE: wrap-safe — phase anchored just below the uint32 rollover and
// ticked across it keeps the exact pattern; the ack overlay crosses the
// rollover too. elapsedMs is mod-2^32 subtraction (P1.M1.T2.S1).
void test_wrap_safe_across_rollover() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    const uint32_t anchor = 0xFFFFFF00u;               // 256 ms below the wrap
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor));          // anchors, t=0: on
    // "anchor + k" wraps naturally in unsigned arithmetic — every instant
    // below k>256 is POST-rollover, exercising elapsedMs across the wrap.
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 200u));  // t=200: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor + 512u));   // t=512: pulse 2 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 600u));  // t=600: off slot
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 1200u)); // t=1200: gap
    // Ack across the rollover: overlay anchored 256 ms below the wrap, all
    // three blinks and the resume land after it.
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor + 2000u));      // Solid anchor
    led.triggerAck();
    const uint32_t ackAnchor = anchor + 2100u;                  // still pre-wrap
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor));           // blink 1 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 100u));   // blink 1 off
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 200u));    // blink 2 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 300u));   // post-wrap: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 400u));    // blink 3 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 599u));   // last overlay ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 600u));    // Solid resumes
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 601u));    // ...and stays
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_off_default_and_explicit);
    RUN_TEST(test_solid_always_on);
    RUN_TEST(test_done_pulse_boundaries);
    RUN_TEST(test_done_pulse_gap_and_repeat);
    RUN_TEST(test_fault_one_group);
    RUN_TEST(test_fault_two_groups);
    RUN_TEST(test_fault_three_groups);
    RUN_TEST(test_fault_zero_groups_clamps_to_one);
    RUN_TEST(test_ack_overlay_then_solid_resumes_exactly);
    RUN_TEST(test_ack_over_blinking_base_phase_undisturbed);
    RUN_TEST(test_mode_change_mid_pattern_resets_phase);
    RUN_TEST(test_wrap_safe_across_rollover);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
# PATTERN: bannered pure-class block in the UNGUARDED region
#   (ButtonClassifier/BaselineCapture/FillDeadline precedent): banner
#   comment cites PRD sections + task ID; end-banner names downstream
#   consumers. NO prototype entry (seam rule #3 is free-functions-only).

# CRITICAL: the base pattern is a PURE FUNCTION of
#   elapsedMs(nowMs, phaseStartMs_) % cycle — no counters, no accumulated
#   state, no delay(). DONE-forever (§11) and latched FAULT (§7) fall out
#   of the modulo; wrap-safety falls out of elapsedMs.

# CRITICAL: lazy anchoring — setMode()/triggerAck() only RELEASE the
#   anchor (phaseAnchored_/ackAnchored_ = false); tick() stamps it. Never
#   add a nowMs parameter to the mutators; never stamp inside them.

# CRITICAL: the ack overlay MASKS for exactly ackSpanMs() = 600 ms; the
#   base keeps evaluating underneath every tick, so the resume is exact.

# PATTERN: consume config BY NAME — the seven §6 LED constants;
#   (uint32_t)DONE_PULSE_COUNT / (uint32_t)ACK_BLINK_COUNT widened before
#   multiplication. §9 commissioning may retune cadences; nothing here
#   may re-hardcode 200/250/1000/2000/100/600.

# GOTCHA: t == ackSpanMs() is ALREADY past the overlay (boundary belongs
#   to the new segment) — the tick at t=600 returns the base level and
#   clears ackActive_ on that same call.

# GOTCHA: setMode(FaultBlink, 0) clamps to 1 inside setMode — never let an
#   empty fault group render "off forever".
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 bannered block at the
    end of the UNGUARDED region (directly above the ARDUINO marker, directly
    below P1.M4.T2.S1's end-banner). Prototype block, includes, ARDUINO
    region, setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes DONE_PULSE_ON_MS, DONE_PULSE_OFF_MS,
    DONE_PULSE_COUNT, DONE_GAP_MS, FAULT_BLINK_MS, FAULT_PAUSE_MS,
    ACK_BLINK_MS from config.h BY NAME.
TEST:
  - New project dir test/test_led_pattern/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
DOWNSTREAM API (frozen by this task — P1.M5.T2.S1 / P1.M5.T3.S1 /
P1.M6.T1.S2 build on it):
  - LedPatternEngine::Mode { Off, Solid, DonePulse, FaultBlink }
  - setMode(Mode, faultGroups=1) — fill engine ONLY (item contract)
  - mode() / faultGroups() — WS2812 color map + diagnostics
  - triggerAck() — fill engine, +5 long-press during FILLING
  - ackActive() — overlay state (post-tick view)
  - tick(nowMs) -> bool ledOn — drivers, once per loop pass
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - GP14 digitalWrite / WS2812 color+brightness         -> P1.M5.T2.S1 / T3.S1
  - WHO sets modes WHEN (state transitions)             -> P1.M6.T1.S2
  - loop wiring of the LED stage                        -> P1.M6.T2.S1
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the block compiles under arm-gcc/arduino-pico inside
# the sketch build. Compile errors mean a transcription deviation from the
# listing — fix, don't redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; all 11 projects [PASSED] — test_seam, test_time_utils,
# test_log_utils, test_button, test_reading, test_baseline, test_trip,
# test_guards, test_backstop, test_deadline AND the new test_led_pattern
# (12/12 tests).
# ANY failure: read the mismatch — the expected values are machine-verified
# against the real file, so a failure means the implementation (not the test)
# deviates. Never "fix" a test's expected value to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_led_pattern
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_led_pattern/

# (2) Structural placement: your block sits between T2S1's end-banner and the
#     ARDUINO marker, one blank line at each junction:
grep -n -A2 "end recovery clock & fill deadline" firmware/jug_filler/jug_filler.ino
#   Expected: banner line, blank, then "// ===================== LED pattern engine"
grep -n -B2 -- "---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   Expected: blank, "// ===== end LED pattern engine ... =====", marker

# (3) Class present exactly once, no prototype added:
grep -c "class LedPatternEngine" firmware/jug_filler/jug_filler.ino   # 1

# (4) No churn where none belongs:
git diff --stat firmware/jug_filler/config.h platformio.ini   # empty
git diff firmware/jug_filler/jug_filler.ino | grep "^-[^-]" | head   # empty — purely additive
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region — strip // comments first (comments
#     legitimately name hardware concepts; only CODE must be hardware-free):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" \
  || echo "PURE OK (comments stripped)"

# (2) Region markers intact (note: -E -- required; a bare pattern starting
#     with dashes is parsed as an option and the grep fails):
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Constants by name — no cadence literals in the class CODE (200/250/
#     1000/2000/100/600 may appear only in TESTS, where they are pinned
#     contract values; the CLASS must use the §6 config names, with the
#     600 ms overlay width computed via ackSpanMs()):
sed -n '/class LedPatternEngine/,/end LED pattern engine/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "\b(200|250|1000|2000|100|600)\b" \
  || echo "NO MS LITERALS IN CLASS CODE"

# (4) Safety-reasoning audit (mirrors research/notes.md §2/§4):
#     - Non-blocking: the only "loop" is the modulo in baseOn — grep the
#       class for 'delay(' finds nothing (covered by (1)).
#     - Wrap safety: every time read routes through elapsedMs; no raw
#       timestamp comparisons anywhere in the class.
#     - Ack masking: baseOn() is evaluated on the SAME tick that retires
#       the overlay (t=600 boundary) — the resume is instant and exact.
#     - Latch semantics: no code path turns FaultBlink/DonePulse off except
#       an explicit setMode by the fill engine (owner of mode changes).
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_led_pattern 12/12; test_deadline, test_backstop and all siblings still green)
- [ ] "PURE OK (comments stripped)"; marker count 2; "NO MS LITERALS IN CLASS CODE"; purely additive diff
- [ ] `git status --short`: `.ino` + `test/test_led_pattern/` only; config.h/platformio.ini/prototype block/ARDUINO region untouched

### Feature Validation

- [ ] Off default + explicit; Solid at any age (1 h sample pinned)
- [ ] DonePulse boundary instants exact (t=0/199/200/399/400/599/600/799/800/999/1000/1199); whole 1000 ms gap off; repetition at 2200/4400; still pulsing ~1 h in (§11 no timeout)
- [ ] FaultBlink n=1/2/3 group shapes + 2000 ms pauses + cycle restarts at n*500+2000; `faultGroups()` echoes n; n=0 clamps to 1
- [ ] Ack overlay: 3 blinks in exactly 600 ms (every 100 ms boundary pinned); `ackActive()` true at t=599, false after the t=600 tick; Solid ALREADY on at t=600; over DonePulse the base phase is undisturbed
- [ ] Mode change mid-pattern resets phase cleanly (twice-chained in one test)
- [ ] Wrap-safe: DonePulse + ack anchored below the uint32 rollover behave exactly across the wrap

### Code Quality Validation

- [ ] Seam rules honored: zero hardware tokens in non-comment UNGUARDED lines; bannered class block, NO prototype entry; const-correct getters
- [ ] Zero new config constants; the seven §6 LED constants by name; `elapsedMs` reused, not reimplemented; counts widened to uint32_t before multiplication
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2); caller protocol documented in-code (tick-per-loop-pass, setMode-by-fill-engine-only, ack-as-mask)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't use `delay()` or any blocking wait for blink cadences — PRD §3/§10: the loop feeds the watchdog every pass; the LED is a per-pass `tick()` over a phase function
- ❌ Don't accumulate pattern state with counters/lastToggle timestamps — the pure `elapsed % cycle` function is wrap-safe, reset-proof, and makes DONE-forever/FAULT-latch free
- ❌ Don't stamp the phase anchor inside `setMode()`/`triggerAck()` — there is no clock there; release the anchor and let the next `tick()` stamp it (lazy anchor is the item contract's shape: `setMode(Mode)` has no timestamp)
- ❌ Don't freeze/restart the base pattern during the ack overlay — the overlay only MASKS; "resumes exactly" requires the base phase to keep running underneath
- ❌ Don't re-hardcode a cadence (200/250/1000/2000/100/600) — consume the §6 config names; §9 commissioning may retune them
- ❌ Don't add `ACK_BLINK_COUNT` (or anything else) to config.h — §8 is frozen; the count 3 is a class-local task constant (BaselineCapture precedent)
- ❌ Don't let drivers call `setMode`/`triggerAck`, and don't have the fill engine call `tick()` directly — mode/ack ownership is the fill engine's (P1.M6.T1.S2), the level stream is the drivers' (P1.M5.T2.S1/T3.S1)
- ❌ Don't "fix" a boundary expectation to make a test pass — t=200 off, t=2200 on, t=600-into-ack = base are the machine-verified house convention (boundary belongs to the new segment)
- ❌ Don't add a prototype for the class, a `begin()`-style hardware hook, pinMode/digitalWrite, or any ARDUINO-region wiring — pure object only; the drivers own the hardware
- ❌ Don't touch the RecoveryClock/FillDeadline block (or any sibling block), config.h, or any sibling test — additive-only edit at the one named anchor
