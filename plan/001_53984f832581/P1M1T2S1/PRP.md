# PRP — P1.M1.T2.S1: Rollover-safe time utilities (pure)

## Goal

**Feature Goal**: Land the first real pure logic in the sketch's UNGUARDED region: two rollover-safe time helpers — `uint32_t elapsedMs(uint32_t now, uint32_t since)` and `bool deadlinePassed(uint32_t now, uint32_t deadline)` — that make every downstream timed behavior (button debounce, sensor cadence, fill backstop, LED patterns, confirm windows) immune to the `millis()` uint32_t wrap at ~49.7 days, with the wrap behavior proven by native Unity tests rather than assumed.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — `seamVersion()` probe REPLACED by the two helpers (+ their explicit prototypes); region markers byte-stable; nothing else changed.
- `test/test_time_utils.cpp` — NEW Unity test file (~18 tests) covering the item's mandated cases: wrap boundary 0xFFFFFFFF→0x00000000, equality (0 elapsed), 1 ms-after-deadline, large spans — plus a config-scale "30-min backstop anchored 256 ms before the wrap" test.
- `test/test_seam.cpp` — one test body updated (the `seamVersion()` assertion becomes an `elapsedMs`/`deadlinePassed` assertion).

**Success Definition**:
- `pio test -e native` → **PASSED**, exit 0, BOTH test projects (`test_seam`, `test_time_utils`) green.
- `pio run -e pico` → compile-only **[SUCCESS]**.
- Purity greps clean: zero `millis`/`delay`/`Serial`/Arduino symbols in the unguarded region; both helpers take the clock as a parameter and call nothing.
- Every timing constant consumer could need already exists in `config.h` (untouched); no new config, no API/serial/log surface change (PRD §10: all timing via these helpers; no `delay()` ever).

## User Persona

**Target User**: The implementing agents of P1.M2–P1.M6 (button, sensors, timer model, LED engine, state machine) — these two functions are their timing vocabulary. Secondary: the human running `pio test -e native` during development.

**Use Case**: Downstream pure components receive an injected `uint32_t nowMs` and ask one of two questions: *"how long since X?"* → `elapsedMs(now, since) >= SOME_MS_CONFIG`, or *"is absolute deadline D due?"* → `deadlinePassed(now, deadlineTs)`.

**User Journey**: consumer computes `deadline = valveOpenedMs + FRESH_BOOT_BACKSTOP_MIN * 60000UL` (mod-2^32 addition — safe), then each loop pass calls `deadlinePassed(now, deadline)`; the valve-closing decision is correct even if the 30-minute fill straddles the 49.7-day wrap, because the native tests pinned exactly that scenario.

**Pain Points Addressed**: the classic firmware bugs — direct timestamp comparison `now >= deadline` (breaks at wrap), signed-math overflow (UB), and `delay()`-based timing (banned by PRD §10) — are eliminated structurally: there is exactly one blessed way to do time in this codebase.

## Why

- PRD §10 (non-negotiable): "No blocking `delay()` in the fill loop; all timing via rollover-safe `millis()` arithmetic." These helpers ARE that sentence, as code.
- PRD §8: every timing tunable already lives in `config.h` (`BUTTON_DEBOUNCE_MS 40`, `BUTTON_LONG_PRESS_MS 1000`, `READING_INTERVAL_MS 100`, `BASELINE_SETTLE_MS 2000`, `CONFIRM_WINDOW_S 12`, `EARLY_WET_WINDOW_S 5`, `TRIP_MIN_ELAPSED_S 60`, `FRESH_BOOT_BACKSTOP_MIN 30`, `WATCHDOG_TIMEOUT_S 8`, LED pattern ms) — they need a rollover-safe consumer, which is this subtask's output and the downstream subtasks' input.
- Safety relevance: the fill backstop (valve auto-close, PRD §6) is a deadline check. If it were computed with wrap-unsafe arithmetic, a device left powered ~49.7 days could let the deadline comparison fail-to-fire → valve never auto-closes → overflow. This subtask is the safety foundation for that.
- Architecture (plan/001_53984f832581/architecture/firmware_architecture.md §1): the seam diagram names `elapsedMs / deadlinePassed (P1.M1.T2.S1)` as the first residents of the UNGUARDED region; external_deps.md item 4 pre-verified the pattern `(uint32_t)(now - since) >= interval` as standard on arduino-pico.

## What

Replace the `seamVersion()` probe in the UNGUARDED region with two pure functions, and prove them with native tests. No config changes, no hardware-region changes, no serial output, no new files beyond the one test file.

**Semantics (normative)**:
- `elapsedMs(now, since)` — milliseconds from timestamp `since` to timestamp `now`, correct across the wrap for any true span `< 2^32 ms` (both are absolute `millis()`-domain timestamps).
- `deadlinePassed(now, deadline)` — **true iff `deadline` has been reached or passed** (inclusive at equality), rollover-safe; certifies forward windows `< 2^31 ms` (~24.86 days). All firmware deadlines are ≤ 44 min, so the domain is never stressed.
- The clock is ALWAYS an injected parameter — neither function calls `millis()`, `Serial`, or anything else. Zero dependencies beyond `<stdint.h>` (via `config.h`, already included by the sketch).

### Success Criteria

- [ ] `firmware/jug_filler/jug_filler.ino` unguarded region contains exactly the two helpers (+ `constexpr uint32_t HALF_WHEEL_MS = 0x80000000u;`); `seamVersion()` gone from file (prototype + definition)
- [ ] `test/test_time_utils.cpp` exists, follows `test/test_seam.cpp`'s shape (same include, setUp/tearDown, explicit `main()`), and all these tests pass:
  - equality → 0 elapsed (at 0, at 1,000,000, at 0xFFFFFFFF)
  - simple span (5000−1000 = 4000)
  - wrap boundary: `elapsedMs(0, 0xFFFFFFFF) == 1`
  - wrap small span: `elapsedMs(0x00000064, 0xFFFFFF00) == 356`
  - large spans: `elapsedMs(0xFFFFFFFF, 0) == 0xFFFFFFFF` and `elapsedMs(0x7FFFFFFF, 0) == 0x7FFFFFFF`
  - deadline inclusive at equality (`deadlinePassed(1000,1000) == true`), 1 ms after → true, 1 ms before → false
  - deadline across wrap: `deadlinePassed(0, 0xFFFFFFFF) == true`, `deadlinePassed(0xFFFFFFFE, 0xFFFFFFFF) == false`
  - config-scale backstop across wrap: deadline = `(0xFFFFFF00 + 1800000UL) = 0x1B6640`; at → true, −1 ms → false, +1 ms → true; `elapsedMs(0x1B6640, 0xFFFFFF00) == 1800000`
  - half-wheel boundary pinned: `deadlinePassed(0x7FFFFFFF, 0) == true`, `deadlinePassed(0x80000000, 0) == false`, `deadlinePassed(0xFFFFFFFF, 0) == false`
- [ ] `test/test_seam.cpp` compiles green after the probe removal (updated assertion in `test_unguarded_region_is_callable`)
- [ ] `pio test -e native` exit 0, all tests PASSED (both projects); `pio run -e pico` compile-only SUCCESS
- [ ] Purity: `sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p'` on the sketch greps ZERO hits for `millis|delay|Serial|pinMode|digitalWrite|analogRead`
- [ ] `firmware/jug_filler/config.h`, `platformio.ini`, `setup()`/`loop()` bodies, README, .gitignore: all untouched (`git status` shows only the 3 files above)
- [ ] No new helpers beyond the two specified (no `secondsElapsed`, no `now()` wrapper — scope discipline; consumers compose these two)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: this PRP contains the complete, final code for both functions and the entire test file (all expected values hand-verified — see research/notes.md table), the exact edit to `test_seam.cpp`, and verified validation commands. The only prerequisite state (the S2 skeleton) is fully specified by its own PRP, which is a binding contract for what exists at implementation start.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: plan/001_53984f832581/P1M1T1S2/PRP.md
  why: THE contract for the skeleton this task extends — read "Task 2 listing" (the exact
        .ino you will edit: prototype block, region markers, seamVersion probe) and
        "Task 4 listing" (test_seam.cpp you will update). Its gotchas (explicit main() on
        PIO 6.2.0, build_src_filter, byte-stable region markers) apply to you verbatim.
  pattern: unguarded probe -> your helpers; test file shape to copy for test_time_utils.cpp
  gotcha: S2 may still be mid-implementation; if firmware/jug_filler/jug_filler.ino or
        test/test_seam.cpp is absent, STOP — the seam is your input, wait/re-check.

- file: firmware/jug_filler/config.h   (at implementation time; currently firmware/config.h pre-S2)
  why: confirms NO new constants are needed (all timing tunables exist); proves constants
        are constexpr/host-compilable. READ-ONLY — do not edit.
  pattern: uint32_t ms constants you will reference only in COMMENTS/tests, not in helpers

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: "1. The one architectural decision: pure-logic/hardware seam" — names these
        exact two functions as first UNGUARDED residents; rules: injected nowMs, explicit
        prototypes, no Arduino calls in pure region.
  gotcha: its diagram shows firmware/jug_filler.ino (pre-S2 layout) — superseded by
        firmware/jug_filler/jug_filler.ino; everything else binding.

- file: plan/001_53984f832581/architecture/external_deps.md
  section: "4. millis() rollover — VERIFIED (standard)" + "Risks & gotchas"
  why: pre-verified platform fact: millis() is uint32_t wrapping ~49.7 days; the
        (uint32_t)(now - since) >= interval pattern is standard on arduino-pico.

- file: plan/001_53984f832581/P1M1T2S1/research/notes.md
  why: the math behind every expected test value (mod-2^32 arithmetic, half-wheel limit),
        the consumer map (P1.M2..M6 usage), and Unity specifics. All expected values
        hand-computed there — don't re-derive.

- url: https://arduino-pico.readthedocs.io/en/latest/millis.html
  why: arduino-pico's millis() returns uint32_t and wraps ~49.7 days — the platform fact
        these helpers exist to absorb.
  critical: no 64-bit variant needed; the wrap is exactly 2^32 ms.

- url: https://eel.is/c++draft/expr#arith.conv
  why: ISO C++ draft [expr.arith.conv]: unsigned arithmetic is performed modulo 2^n —
        wrap-around subtraction is well-defined, never UB. THIS is why the idiom is safe.
  critical: signed overflow is UB; never introduce int32_t casts into the helpers.

- url: https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover
  why: canonical community Q&A; accepted answer codifies (uint32_t)(current - start) >= interval.
  critical: also shows the anti-patterns (direct >= comparison, cast-to-signed) we forbid.

- url: https://www.baldengineer.com/arduino-millis-plus-addition-does-not-add-up.html
  why: why "if (millis() >= target)" breaks across wrap while subtraction survives it —
        the exact reason deadlinePassed exists instead of a raw comparison.
  critical: computing deadlines BY addition is fine (mod 2^32); TESTING them by comparing
        now >= deadline is not. deadlinePassed is the blessed test.
```

### Current Codebase tree (expected at implementation start — S2 contract delivered)

```bash
water-jug-filler/
├── .gitignore                # has ".pio/" appended (S2)
├── README.md                 # S2 seed
├── platformio.ini            # S2: [env:pico] compile-only, [env:native] unity + src filter
├── firmware/
│   └── jug_filler/
│       ├── jug_filler.ino    # S2 skeleton: seam header, prototypes incl. seamVersion(),
│       │                     #   UNGUARDED region (seamVersion only), #ifdef ARDUINO region
│       └── config.h          # S1 deliverable (32 constexpr tunables) — READ-ONLY here
├── test/
│   └── test_seam.cpp         # S2: config assertions + seamVersion() call  ← update 1 test
└── plan/001_53984f832581/    # orchestrator + architecture + prior PRPs (read-only)
# Toolchain (verified): pio 6.2.0 at ~/.local/bin/pio, host g++ 16.2.1,
# raspberrypi platform + Unity 2.6.1 already installed under ~/.platformio.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── jug_filler/
│       └── jug_filler.ino    # MODIFIED: seamVersion() → elapsedMs + deadlinePassed
│                             #   (+HALF_WHEEL_MS constexpr, +2 prototypes). Markers stable.
├── test/
│   ├── test_seam.cpp         # MODIFIED: 1 test body (probe assertion → helper assertion)
│   └── test_time_utils.cpp   # NEW: ~18 Unity tests, complete rollover contract proof
└── (everything else untouched)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL (PIO 6.2.0, verified live in S2 research): EVERY test_*.cpp needs an explicit
//   int main() { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); }
// No auto-generated Unity runner — without main() the link fails with
// "undefined reference to `main'". Copy test_seam.cpp's main() shape exactly.

// CRITICAL: each test_*.cpp is its OWN Unity test project; both include the same .ino
// with NO duplicate-symbol clash (build_src_filter = -<*> keeps src_dir out of native).
// `pio test -e native` runs test_seam then test_time_utils sequentially — expect TWO
// "PASSED" project summaries.

// CRITICAL: never compare uint32_t timestamps directly (now >= deadline) — ordering is
// meaningless across the wrap. Never cast to int32_t/long — signed overflow is UB.
// Unsigned +/− IS the safety: ISO C++ defines it mod 2^32.

// Unity 2.6.1: TEST_ASSERT_EQUAL_UINT32(expected, actual) — order matters for failure
// messages; TEST_ASSERT_TRUE/TEST_ASSERT_FALSE for booleans (NEVER
// TEST_ASSERT_EQUAL(1, bool_val)). Suffix big literals with UL: 0xFFFFFFFFUL, 1800000UL,
// 60000UL — keeps every comparison unsigned on both toolchains.

// Region-marker comment lines are STRUCTURAL ANCHORS for P1.M7.T1.S1 — do not reword:
//   "// ---- UNGUARDED REGION: pure logic (host-testable) ----"
//   "// ---- #ifdef ARDUINO REGION: hardware glue ----"

// The sketch needs NO #include <Arduino.h> (IDE/builder prepend it) and the unguarded
// region must never reference any Arduino symbol — host g++ would reject it anyway
// (that's the seam working as designed).

// config.h is READ-ONLY for this subtask. HALF_WHEEL_MS (2^31 ms) is a mathematical
// constant, not a tunable — it lives in the sketch next to its only consumer.
```

## Implementation Blueprint

### Data models and structure

None — two free functions and one local constexpr. (config.h's `BackstopTableEntry` is unrelated here.)

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: MODIFY firmware/jug_filler/jug_filler.ino — replace the probe with the helpers
  - PRECONDITION: file exists with "uint32_t seamVersion();" prototype and
    "uint32_t seamVersion() { return 1; }" definition (S2 contract). If absent → S2 not
    landed; STOP and re-check rather than recreating the skeleton.
  - EDIT 1 — prototype block: replace the seamVersion line with:
      uint32_t elapsedMs(uint32_t now, uint32_t since);      // rollover-safe elapsed ms (P1.M1.T2.S1)
      bool deadlinePassed(uint32_t now, uint32_t deadline);  // rollover-safe deadline check (P1.M1.T2.S1)
  - EDIT 2 — unguarded region: replace the probe comment + definition with the complete
    "Task 1 listing" below (copy verbatim — comments included; they are the consumer doc).
  - PRESERVE: everything else byte-for-byte — header, #include "config.h", region markers,
    entire #ifdef ARDUINO region. setup()'s valve-LOW line 1 untouched (PRD §10).

Task 2: MODIFY test/test_seam.cpp — one test body
  - EDIT: in test_unguarded_region_is_callable() replace
      TEST_ASSERT_EQUAL_UINT32(1, seamVersion());
    with:
      // Pure region reachable from host — time helpers replaced the seamVersion() probe
      // when the first real logic landed (P1.M1.T2.S1).
      TEST_ASSERT_EQUAL_UINT32(0, elapsedMs(1000, 1000));
      TEST_ASSERT_TRUE(deadlinePassed(1000, 1000));
  - PRESERVE: every other test, the include, setUp/tearDown, main() (same RUN_TESTs).

Task 3: CREATE test/test_time_utils.cpp — the full contract proof
  - IMPLEMENT: the complete "Task 3 listing" below (copy verbatim; expected values are
    hand-verified — see research/notes.md — do not "fix" them if a test fails; the
    FUNCTION is wrong, not the table).
  - NAMING: test/test_time_utils.cpp; test functions test_elapsed_* / test_deadline_*
  - PATTERN: test_seam.cpp shape — same relative include, explicit main() (mandatory).
  - PLACEMENT: test/  (becomes its own Unity project automatically; no ini change).

Task 4: RUN native tests (PRIMARY GATE)
  - RUN: pio test -e native
  - EXPECT: BOTH projects PASSED — test_seam (2 tests) and test_time_utils (18 tests),
    exit 0. NEVER `pio test -e pico` (no transport there).

Task 5: RUN board compile-only check
  - RUN: pio run -e pico        # NEVER upload
  - EXPECT: [SUCCESS] (toolchain already installed; no network needed)

Task 6: RUN purity + scope checks
  - RUN the three Level 4 commands below; expect "PURE OK", "MARKERS OK", clean git status.
```

**Task 1 listing — the unguarded region content (verbatim):**

```cpp
// ---- UNGUARDED REGION: pure logic (host-testable) ----

// ===================== Rollover-safe time helpers (P1.M1.T2.S1) =====================
// millis() is uint32_t and wraps 0xFFFFFFFF -> 0x00000000 every 2^32 ms (~49.7 days).
// Unsigned C++ arithmetic is defined modulo 2^32 (ISO C++ [expr.arith.conv]) — so
// subtraction of timestamps NEVER overflows/UBs and yields the true elapsed span for
// any span < 2^32 ms. Direct timestamp comparison (now >= deadline) is FORBIDDEN —
// ordering across the wrap is meaningless. All timed logic goes through these two:
//
//   interval question:  elapsedMs(now, sinceMs)        >= SOME_CONFIG_MS
//   deadline question:  deadlinePassed(now, deadlineTs)  (absolute millis timestamp)
//
// Seam rule: `now` is ALWAYS injected — these functions never call millis().

// Milliseconds elapsed from timestamp `since` to timestamp `now` (both absolute
// millis()-domain uint32_t values). Wrap-safe for any true span < 2^32 ms.
uint32_t elapsedMs(uint32_t now, uint32_t since) {
    return now - since;  // mod-2^32 subtraction IS the rollover safety — keep it raw
}

// 2^31 ms ≈ 24.86 days: the maximum forward window deadlinePassed can certify.
// Mathematical constant (not a tunable) — every firmware deadline is ≤ 44 min.
constexpr uint32_t HALF_WHEEL_MS = 0x80000000u;

// True iff the absolute timestamp `deadline` has been reached or passed at `now`
// (rollover-safe, inclusive at equality: a fill backstop sampled exactly at its
// deadline fires — the valve closes AT the deadline, never 1 ms late).
// now == deadline → true; deadline+1 → true; deadline-1 → false; correct even when
// the pair straddles the 49.7-day wrap.
bool deadlinePassed(uint32_t now, uint32_t deadline) {
    return elapsedMs(now, deadline) < HALF_WHEEL_MS;
}
// ============== end time helpers (consumed by P1.M2–P1.M6) ==============
```

**Task 3 listing — `test/test_time_utils.cpp` (complete file):**

```cpp
// test/test_time_utils.cpp — rollover-safety contract for elapsedMs/deadlinePassed
// (P1.M1.T2.S1). These tests ARE the timing contract for every downstream consumer:
// button debounce (P1.M2), sensor cadence + settle + false-trip windows (P1.M3),
// fill backstop deadlines (P1.M4), LED pattern phases (P1.M5), confirm windows (P1.M6).
// Expected values hand-derived from mod-2^32 arithmetic (see research notes); the
// 49.7-day wrap is exercised directly via 0xFFFFFFFF/0x00000000 timestamps.
#include <unity.h>

#include "../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// --- elapsedMs: equality → 0 elapsed ---
void test_elapsed_equality_at_zero() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(0UL, 0UL));
}
void test_elapsed_equality_mid_range() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(1000000UL, 1000000UL));
}
void test_elapsed_equality_at_max() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(0xFFFFFFFFUL, 0xFFFFFFFFUL));
}

// --- elapsedMs: plain spans ---
void test_elapsed_simple_span() {
    TEST_ASSERT_EQUAL_UINT32(4000UL, elapsedMs(5000UL, 1000UL));
}

// --- elapsedMs: wrap boundary 0xFFFFFFFF → 0x00000000 ---
void test_elapsed_wrap_boundary_one_ms() {
    TEST_ASSERT_EQUAL_UINT32(1UL, elapsedMs(0UL, 0xFFFFFFFFUL));  // 1 ms across the wrap
}
void test_elapsed_wrap_small_span() {
    // since = 256 ms below the wrap; now = 100 ms after it → 100 + 256 = 356 ms
    TEST_ASSERT_EQUAL_UINT32(356UL, elapsedMs(0x00000064UL, 0xFFFFFF00UL));
}

// --- elapsedMs: large spans ---
void test_elapsed_max_span() {
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFUL, elapsedMs(0xFFFFFFFFUL, 0UL));
}
void test_elapsed_half_wheel_span() {
    TEST_ASSERT_EQUAL_UINT32(0x7FFFFFFFUL, elapsedMs(0x7FFFFFFFUL, 0UL));
}

// --- deadlinePassed: inclusive at equality, ±1 ms edges ---
void test_deadline_at_deadline_is_passed() {
    TEST_ASSERT_TRUE(deadlinePassed(1000UL, 1000UL));  // inclusive: backstop fires at T
}
void test_deadline_one_ms_after() {
    TEST_ASSERT_TRUE(deadlinePassed(1001UL, 1000UL));
}
void test_deadline_one_ms_before() {
    TEST_ASSERT_FALSE(deadlinePassed(999UL, 1000UL));
}

// --- deadlinePassed: across the wrap ---
void test_deadline_wrap_one_ms_after() {
    TEST_ASSERT_TRUE(deadlinePassed(0UL, 0xFFFFFFFFUL));      // due 1 ms before wrap
}
void test_deadline_wrap_one_ms_before() {
    TEST_ASSERT_FALSE(deadlinePassed(0xFFFFFFFEUL, 0xFFFFFFFFUL));
}

// --- config-scale scenario: 30-min backstop anchored 256 ms before the wrap ---
void test_backstop30min_across_wrap_elapsed() {
    const uint32_t valveOpened = 0xFFFFFF00UL;                       // 256 ms below wrap
    const uint32_t deadline = valveOpened + 30UL * 60000UL;          // = 0x1B6640 (wrapped)
    TEST_ASSERT_EQUAL_UINT32(0x1B6640UL, deadline);                  // arithmetic anchor
    TEST_ASSERT_EQUAL_UINT32(1800000UL, elapsedMs(deadline, valveOpened));
}
void test_backstop30min_across_wrap_deadline_edges() {
    const uint32_t deadline = 0x1B6640UL;
    TEST_ASSERT_FALSE(deadlinePassed(0x1B663FUL, deadline));  // 1 ms early: valve still open
    TEST_ASSERT_TRUE(deadlinePassed(0x1B6640UL, deadline));   // at deadline: fires
    TEST_ASSERT_TRUE(deadlinePassed(0x1B6641UL, deadline));   // 1 ms late: fires
}

// --- deadlinePassed: half-wheel domain boundary (2^31 ms ≈ 24.86 days) ---
void test_deadline_half_wheel_boundary() {
    TEST_ASSERT_TRUE(deadlinePassed(0x7FFFFFFFUL, 0UL));      // 1 ms inside: passed
    TEST_ASSERT_FALSE(deadlinePassed(0x80000000UL, 0UL));     // exactly half-wheel: not certifiable
    TEST_ASSERT_FALSE(deadlinePassed(0xFFFFFFFFUL, 0UL));     // 1 ms "before" 0 across wrap
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_elapsed_equality_at_zero);
    RUN_TEST(test_elapsed_equality_mid_range);
    RUN_TEST(test_elapsed_equality_at_max);
    RUN_TEST(test_elapsed_simple_span);
    RUN_TEST(test_elapsed_wrap_boundary_one_ms);
    RUN_TEST(test_elapsed_wrap_small_span);
    RUN_TEST(test_elapsed_max_span);
    RUN_TEST(test_elapsed_half_wheel_span);
    RUN_TEST(test_deadline_at_deadline_is_passed);
    RUN_TEST(test_deadline_one_ms_after);
    RUN_TEST(test_deadline_one_ms_before);
    RUN_TEST(test_deadline_wrap_one_ms_after);
    RUN_TEST(test_deadline_wrap_one_ms_before);
    RUN_TEST(test_backstop30min_across_wrap_elapsed);
    RUN_TEST(test_backstop30min_across_wrap_deadline_edges);
    RUN_TEST(test_deadline_half_wheel_boundary);
    return UNITY_END();
}
```

*(16 RUN_TEST lines / 16 test functions — count them when copying; every function must appear exactly once in main().)*

### Implementation Patterns & Key Details

```cpp
// The entire implementation is two lines of arithmetic; the SAFETY is in what's absent:
//   - no millis() call (clock injected — testable on host with scripted clocks)
//   - no signed types (signed overflow is UB; unsigned wrap is defined mod 2^32)
//   - no state (pure functions; no globals; callable from any test TU)
// deadlinePassed is defined via elapsedMs (single source of truth) — keep that
// delegation; do not inline a second subtraction.
//
// CONSUMER CHEAT SHEET (for downstream subtasks reading this file later):
//   debounce:      elapsedMs(now, lastEdgeMs)   >= BUTTON_DEBOUNCE_MS      // P1.M2
//   long press:    elapsedMs(now, pressStartMs) >= BUTTON_LONG_PRESS_MS    // P1.M2
//   cadence:       elapsedMs(now, lastReadingMs) >= READING_INTERVAL_MS    // P1.M3
//   settle:        elapsedMs(now, bootMs)        >= BASELINE_SETTLE_MS     // P1.M3
//   backstop:      deadlinePassed(now, fillDeadlineMs)  // = valveOpen + min*60000UL   // P1.M4
//   LED phase:     elapsedMs(now, phaseStartMs)  >= DONE_PULSE_ON_MS       // P1.M5
// (Deadline ADDITION is wrap-safe mod 2^32; testing via now>=deadline is NOT.)
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes; UNGUARDED region grows by the
    helpers; #ifdef ARDUINO region untouched (watchdog/logging = P1.M1.T2.S2; loop
    wiring = P1.M6.T2.S1).
BUILD:
  - No platformio.ini change: [env:native] auto-discovers test/test_time_utils.cpp as
    its own project; [env:pico] compiles the .ino including the new region.
CONFIG:
  - None. config.h untouched (all tunables pre-exist; HALF_WHEEL_MS is math, not tuning).
DOCS:
  - None (item spec: "internal helpers, no user-facing/config/API surface change").
  README is NOT updated in this subtask.
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - watchdog_update/watchdog_enable + serial logging policy → P1.M1.T2.S2
  - any millis() call site (hardware glue injects now) → P1.M6.T2.S1 et al.
  - minute/ms conversion helpers for backstop math → P1.M4 (they compose these two)
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Native build IS the syntax check — host g++ compiles the sketch's unguarded region:
pio test -e native
# Expected: TWO projects run (test_seam, test_time_utils); every line [PASSED];
# "2 test projects ... PASSED"-style summary; exit 0.
# If link fails with "undefined reference to `main'" → a test file lacks main() (Gotcha).
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native -v   # verbose: lists each RUN_TEST as it executes
# Expected: all 16 test_time_utils tests + both test_seam tests PASSED.
# Failure triage order: (1) transcription slip in main() vs function list,
# (2) literal missing UL suffix, (3) helper body — the EXPECTED VALUES are pre-verified.
```

### Level 3: Integration Testing (System Validation)

```bash
pio run -e pico        # compile-only; NEVER upload in this subtask
# Expected: "[SUCCESS]" and artifact under .pio/build/pico/. Proves the helpers compile
# under arm-g++ / Arduino builder (prototype generation, config.h include) too.

# Scope check — only the three intended files changed:
git status --short
# Expected: M firmware/jug_filler/jug_filler.ino, M test/test_seam.cpp, ?? test/test_time_utils.cpp
# and NOTHING else (no config.h, no platformio.ini, no README, no .gitignore).
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity: unguarded region contains zero Arduino/library symbols:
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" || echo "PURE OK"

# (2) Structural anchors intact for P1.M7.T1.S1:
grep -c "---- UNGUARDED REGION: pure logic (host-testable) ----\|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Probe fully removed (no stale references anywhere):
grep -rn "seamVersion" firmware/ test/ || echo "PROBE GONE OK"

# (4) Safety regression check (still from S2 — must keep passing):
awk '/^void setup\(\)/{f=1} f && /digitalWrite\(PIN_VALVE, LOW\)/{print "valve-LOW first: OK"; exit} f && /pinMode|Serial|analogRead/{print "VIOLATION: "NR; exit}' \
  firmware/jug_filler/jug_filler.ino
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio test -e native` exit 0 with both test projects `[PASSED]`
- [ ] `pio run -e pico` → `[SUCCESS]` (compile-only, no upload attempted)
- [ ] `git status --short` shows exactly: `.ino` modified, `test_seam.cpp` modified, `test_time_utils.cpp` new

### Feature Validation

- [ ] All "What" success-criteria boxes tickable from command output (16 mandated tests green)
- [ ] Wrap boundary, equality, ±1 ms deadline edges, large spans, config-scale 30-min backstop across wrap all proven
- [ ] Half-wheel boundary pinned (`0x7FFFFFFF` true / `0x80000000` false / `0xFFFFFFFF` false)
- [ ] Purity grep prints "PURE OK"; region-marker count = 2; probe grep prints "PROBE GONE OK"; valve-LOW-first check OK
- [ ] No user-facing/config/API surface change (per item spec: "DOCS: none")

### Code Quality Validation

- [ ] Helpers follow seam rules: injected clock, explicit prototypes, pure, no Arduino symbols
- [ ] Region markers and the rest of the sketch byte-stable (only the specified edits)
- [ ] No scope creep: exactly two helpers + one local constexpr; no extra utilities
- [ ] Test file mirrors test_seam.cpp conventions (include path, setUp/tearDown, explicit main(), UL-suffixed literals)

### Documentation & Deployment

- [ ] Function comments carry the consumer cheat sheet (interval vs deadline usage)
- [ ] No new environment variables; no README/config changes

---

## Anti-Patterns to Avoid

- ❌ Don't compare timestamps directly (`now >= deadline`, `now > since`) — ordering is meaningless across the wrap; that's the bug class these helpers exist to kill
- ❌ Don't cast to `int32_t`/`long` or use signed math — signed overflow is UB; unsigned wrap is defined mod 2^32 (that definition IS the safety)
- ❌ Don't call `millis()` (or anything) inside the helpers — clock is always injected; the seam dies otherwise
- ❌ Don't "simplify" the test expectations if one fails — the table is hand-verified (research/notes.md); the function is what's wrong
- ❌ Don't omit the explicit `main()` in `test_time_utils.cpp` — PIO 6.2.0 will fail to link (verified)
- ❌ Don't reword/move the region-marker comments — P1.M7.T1.S1 anchors on them byte-exact
- ❌ Don't add a `now()` convenience wrapper, seconds converters, or any third helper — P1.M4 composes these two; scope discipline
- ❌ Don't touch `config.h`, `platformio.ini`, `setup()`/`loop()`, README, or `.gitignore`
- ❌ Don't use `TEST_ASSERT_EQUAL` with booleans — use `TEST_ASSERT_TRUE`/`TEST_ASSERT_FALSE`
- ❌ Don't run `pio test -e pico` or upload anything — native is the only test transport; board env is compile-only

---

**Confidence Score: 10/10** — the deliverable is two one-line functions with complete verbatim listings, a complete verbatim test file with hand-verified expected values, and validation commands already proven working on this exact machine/toolchain by the S2 research (`pio test -e native` green on PIO 6.2.0 + g++ 16.2.1 with the same layout). The only external dependency is the S2 skeleton landing per its own PRP (stop-and-check precondition included).
