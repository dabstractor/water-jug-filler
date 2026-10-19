# PRP — P1.M3.T3.S1: TripDetector — baseline−350 for 8 consecutive readings (pure)

## Goal

**Feature Goal**: Land the sensor chain's stage-3 pure logic in the UNGUARDED region of the sketch: a `TripDetector` class — one instance per sensor — implementing PRD §7.3 exactly: an AVERAGED reading at or below (baseline − `TRIP_DROP_COUNTS` = 350 counts) for `DEBOUNCE_COUNT` = 8 CONSECUTIVE readings means that sensor tripped. The trip LATCHES (stays true for the remainder of the fill; readings after the trip are inert); the streak resets to zero on any reading STRICTLY above the threshold (the boundary baseline−350 itself qualifies — inclusive). The class is TIMELESS: `feed(uint16_t avg)` takes no clock — all time-domain logic belongs to the guards (P1.M3.T3.S2) and the engine (P1.M6.T1.S2). Zero hardware, zero new config constants (both tunables already live in config.h §5), zero ARDUINO-region changes.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: one bannered `TripDetector` class block inserted into the UNGUARDED region, immediately AFTER the `// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========` banner (landed by P1.M3.T2.S1) and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`. NO prototype-block changes (classes get none), NO include changes, NO config.h changes, NO ARDUINO-region edits.
- `test/test_trip/test_trip.cpp` — NEW: 14 Unity native tests proving: exactly-8 streak trips (7 do not); 7 qualifying then one strictly-above resets the streak (full 8 needed after the break); boundary exactly baseline−350 qualifies (inclusive); baseline−349 never advances; lowest legal baseline (2000 → threshold 1650); deep-wet (0 counts) still needs 8; alternating wet/dry never trips; latch survives dry readings and garbage after the trip; sensor1/sensor2 independence; different baselines → different thresholds; `begin()` re-arm with a new baseline; fresh-instance defaults. All expected values hand-derived AND machine-verified this session against the REAL current `.ino` (`g++ -std=c++17 -Wall -Wextra -Werror`, **14 tests / 0 failures** — see research/notes.md §3).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_trip` project, plus regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline).
- `pio run -e pico` → compile-only `[SUCCESS]` — the class compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_trip/` added by this task.
- The frozen public surface (`begin`, `feed`, `tripped`, default constructor) is ready for downstream consumption with zero rework: false-trip guards (P1.M3.T3.S2) and the fill engine's trip→valve-close→CONFIRM path (P1.M6.T1.S2).

## User Persona

**Target User**: The implementing agents of the downstream chain — P1.M3.T3.S2 (guards poll `tripped()` plus their own elapsed-time windows to classify false trips), P1.M6.T1.S2 (fill engine arms both detectors at fill start with validated baselines, feeds every emitted averaged pair, closes the valve on the first `tripped()` → CONFIRM), P1.M6.T2.S1 (integration loop forwards each aggregator pair into both detectors).

**Use Case**: The fill has started, baselines were captured and judged plausible (BaselineCapture verdict Ok, PRD §7.1), and the valve is open. Every 100 ms the ReadingAggregator emits one averaged pair; the engine feeds each sensor's value into its own TripDetector. As water rises past a sensor's tips, its readings drop; once 8 consecutive readings sit at or below baseline−350, that detector latches. The engine sees `tripped()` → closes the valve immediately → CONFIRM window (PRD §7.4–7.5). A single splash or noise dip cannot trip (streak resets on the next dry reading); a genuine trip cannot un-trip (latch).

**User Journey**: valve open → readings stream in → (noise blip: streak resets, nothing happens) → water reaches sensor → 8 wet readings → latch → engine closes valve → CONFIRM.

**Pain Points Addressed**: electrical noise and single-sample splashes must not shut the valve early (8-reading debounce); a genuinely wet sensor must trip deterministically at a fixed threshold (inclusive boundary, no hysteresis guessing); the trip decision must be chatter-free (latch) so the valve-close and CONFIRM sequencing is one-shot; each sensor must be judged independently (one instance per sensor, no shared state).

## Why

- PRD §7.3 (normative, the trip rule): "an averaged reading at or below (baseline − `TRIP_DROP_COUNTS`) for `DEBOUNCE_COUNT` consecutive readings = that sensor tripped." §7.4: "First sensor trips → close the valve immediately → CONFIRM." This class is the pure core of that rule.
- PRD §8 (frozen contract): `TRIP_DROP_COUNTS = 350`, `DEBOUNCE_COUNT = 8` already live in config.h §5 — consumed BY NAME, never hardcoded.
- Architecture (`firmware_architecture.md` §1): this task's exact named slot — `TripDetector + guard evaluation (P1.M3.T3.S1/S2)` directly after `BaselineCapture (P1.M3.T2.S1)` in the UNGUARDED region. Seam rules: pure logic only, no hardware calls.
- Item contract (task breakdown): input = baselines from P1.M3.T2.S1 (`BaselineCapture::baseline(sensor)`, frozen surface) + averaged readings from P1.M3.T1.S1 (`ReadingAggregator`, landed and frozen); output = per-sensor latched tripped flags consumed by the fill engine (P1.M6.T1.S2) and guards (P1.M3.T3.S2); docs = none (internal).
- TDD-by-SOW: architecture §5 mandates native Unity proofs for every §7 rule this class underpins.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; downstream PRPs are written against it):

1. **`TripDetector()`** — default constructor: threshold 0, streak 0, not tripped. Fail-safe note: feeding an UNBEGUN instance is out of contract (the engine always calls `begin()` first); threshold 0 means only 0-count readings would qualify.
2. **`void begin(uint16_t baseline)`** — arm for a new fill: computes `threshold_ = baseline − TRIP_DROP_COUNTS` once, resets the streak, clears the latch. MUST receive a VALIDATED dry baseline (`BaselineCapture::verdict(s) == Ok`, i.e. 2000..4050 — always > 350, so no unsigned wrap). An unvalidated baseline < 350 would wrap the threshold to a huge value → never trip → the backstop (PRD §7.7, fault 3) catches it — the fail-safe direction. NO clamping on purpose (a clamp to 0 would trip-on-anything = unsafe).
3. **`void feed(uint16_t avg)`** — one averaged reading from the aggregator stream. At/below threshold advances the streak; STRICTLY above resets it to zero; the `DEBOUNCE_COUNT`-th consecutive qualifying reading latches the trip. No-op once latched. Takes NO clock — this class is timeless (item contract signature).
4. **`bool tripped() const`** — the ONLY output: latched trip flag. Consumed by P1.M6.T1.S2 (valve close → CONFIRM) and P1.M3.T3.S2 (guards).

Sensor mapping is NOT in the class (structural independence): the engine owns instance↔sensor assignment (instance for sensor 0 == PIN_SENSOR1 == fault code 1; sensor 1 == PIN_SENSOR2 == fault code 2).

NOT in scope (owners): false-trip guards — `TRIP_MIN_ELAPSED_S` / `EARLY_WET_WINDOW_S` (P1.M3.T3.S2); arming/feeding from the engine + valve close + CONFIRM (P1.M6.T1.S2); loop wiring of aggregator → detectors (P1.M6.T2.S1); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `class TripDetector` present exactly once in the UNGUARDED region, after the baseline-capture end-banner, before the ARDUINO region marker; banner comments per the verbatim listing
- [ ] Public surface is EXACTLY the 3 members + default constructor — no extra public members (no `streak()`, no `threshold()`, no `reset()`; `begin()` is the only re-arm path)
- [ ] Threshold comparison is INCLUSIVE at exactly baseline−350 (`avg <= threshold_`); streak resets ONLY on strictly-above readings
- [ ] Trip latches at exactly the 8th consecutive qualifying reading; post-trip feeds of any value are inert; only `begin()` unlatches
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region
- [ ] `test/test_trip/test_trip.cpp` exists with explicit `int main()` Unity runner; all 14 listed cases present with the exact expected values
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` UNGUARDED region + new test dir only)
- [ ] config.h, platformio.ini, prototype block, include block, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete class listing and the complete 14-test file are provided verbatim below — both machine-compiled with `-Wall -Wextra -Werror` and executed green (**14 tests / 0 failures**) against the REAL current `.ino` on this host this session (merged-copy method, research/notes.md §3 — the only difference from the shipped test file is the include line, which points at the repo-relative `.ino` path per house convention); insertion anchors quote banner TEXT, not line numbers; the consumed upstream surface (`BaselineCapture::baseline(sensor)` return type/domain from its landed-PRP contract; `ReadingAggregator` emission shape read from the landed file) was verified, not assumed; validation commands mirror ones proven working in P1.M3.T1.S1/S2's sessions.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (UNGUARDED region only). Verified-current landmarks:
        (1) INSERT ANCHOR — the banner
        `// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========`
        (landed by P1.M3.T2.S1; at research time it did not exist yet — Task 0
        verifies it is present before you start); insert your block AFTER it
        (one blank line between), BEFORE
        `// ---- #ifdef ARDUINO REGION: hardware glue ----` (one blank line between).
        (2) `class ReadingAggregator` (~line 186) — the feed source: `takeReading`
        emits the averaged uint16_t pair your feed() accepts. Read-only.
  gotcha: if the baseline-capture banner is MISSING, P1.M3.T2.S1 has not landed —
        STOP and report; do not improvise an anchor.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of the constants you consume BY NAME: TRIP_DROP_COUNTS
        (350, uint16_t), DEBOUNCE_COUNT (8, uint16_t) — §5 "ADC / sensors" group.
        Do NOT add anything here — §8 is frozen.
  gotcha: never hardcode 350/8 — always the config names (§9 commissioning may
        retune sensor values).

- file: test/test_reading/test_reading.cpp
  why: THE test pattern to follow verbatim-in-style: relative `.ino` include,
        empty setUp/tearDown, hand-derived expected values with derivation
        comments, ITEM CASE tags on contract-pinned tests, explicit
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (REQUIRED on PIO 6.2.0 — no auto-generated Unity runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity project,
        auto-discovered — no platformio.ini change needed.

- file: plan/001_53984f832581/P1M3T3S1/research/notes.md
  why: this task's research: §2 binding design decisions (begin() necessity,
        timeless feed, inclusive threshold, latch, no-clamp fail-safe), §3 the
        machine-verification record + expected-value table, §5 downstream API
        freeze, §6 sequencing safety.
  section: §2 (design decisions) and §3 (verification)

- file: plan/001_53984f832581/P1M3T2S1/PRP.md
  why: CONTRACT for the upstream you consume. BaselineCapture's frozen surface:
        `begin(uint32_t)`, `addReading(...)`, `done()`, `baseline(uint8_t) -> uint16_t`
        (median counts, valid at done(), verdict Ok implies BASELINE_MIN..MAX),
        `verdict(uint8_t)`. Your begin() receives `baseline.baseline(s)` AFTER
        verdict Ok — the engine (P1.M6.T1.S2) is the gatekeeper of that order.
  gotcha: do NOT modify BaselineCapture — frozen; its tests must stay green.

- file: plan/001_53984f832581/P1M3T1S1/PRP.md
  why: CONTRACT for the feed source. ReadingAggregator emits one averaged pair
        per READING_INTERVAL_MS (100 ms) — your detector just counts; it does
        NOT check cadence (the aggregator owns cadence, frozen).
  gotcha: do NOT modify ReadingAggregator/medianOf — frozen, 16 tests green.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: §1 (seam rules + your named slot after BaselineCapture)
  why: justifies the UNGUARDED-region placement and the pure/no-hardware rule.

- url: https://ganssle.com/debouncing.pdf
  why: Ganssle, "A Guide to Debouncing" — the consecutive-sample counter
        (integrator) as the robust debounce form. The 8-reading streak is the
        same structure applied to an ADC threshold instead of a switch level:
        any single perturbation resets the count, so 8-in-a-row is strong
        evidence of a real state change.
  critical: the debounce is COUNT-domain only — never mix elapsed time into
        this class; time belongs to the guards (P1.M3.T3.S2).

- url: https://docs.platformio.org/en/stable/plus/unit-testing.html
  why: PlatformIO native Unity conventions — test_dir auto-discovery, one project
        per test/ subdir, explicit runner requirement on PIO 6.2.0.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # seam + time helpers + ButtonClassifier
│   │                         #   + ReadingAggregator/medianOf (landed)
│   │                         #   + BaselineCapture (P1.M3.T2.S1 — MUST be landed
│   │                         #     before you start; its end-banner = YOUR ANCHOR)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service, ADC
│   │                         #     wiring, setup/loop
│   │                         # ← EDIT (UNGUARDED region only: +1 bannered class block)
│   └── config.h              # every §8 tunable — READ-ONLY (TRIP_DROP_COUNTS /
│                             #   DEBOUNCE_COUNT consumed by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/         # P1.M3.T1.S1's — untouched, must stay green
│   ├── test_baseline/        # P1.M3.T2.S1's — untouched, must stay green
│   └── test_trip/            # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): TripDetector class in the
│                             #   UNGUARDED region — threshold from validated
│                             #   baseline, 8-consecutive streak, latched trip
└── test/
    └── test_trip/
        └── test_trip.cpp     # NEW: native Unity proof — streak length, boundary
                              #   inclusivity, reset semantics, latch, sensor
                              #   independence, re-arm, defaults
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: the threshold comparison MUST be `avg <= threshold_` — PRD §7.3 says
//   "at or below (baseline − TRIP_DROP_COUNTS)". Exactly baseline−350 QUALIFIES
//   (inclusive); baseline−349 is strictly above and RESETS the streak. Both
//   boundary sides are pinned by tests (3050 trips for baseline 3400; 3051 never).

// CRITICAL: the streak resets to zero ONLY on strictly-above readings — a
//   qualifying reading after a reset starts a NEW streak from 1; there is no
//   partial credit and no decay. 7 qualifying + 1 above + 7 qualifying = still
//   not tripped (pinned).

// CRITICAL: feed() MUST be a no-op once latched (`if (tripped_) return;` FIRST)
//   — "latched for the remainder of the fill" is the anti-chatter contract for
//   the valve-close decision. Only begin() unlatches.

// CRITICAL: feed() takes NO nowMs — the class is timeless by design (item
//   contract signature). Do not "improve" it with elapsed-time logic: the
//   time-domain rules (TRIP_MIN_ELAPSED_S, EARLY_WET_WINDOW_S) are P1.M3.T3.S2's.

// CRITICAL: do NOT clamp the threshold in begin(). Baseline is validated
//   (2000..4050 > 350 — no wrap possible in-contract). An out-of-contract
//   baseline < 350 wraps mod-2^16 to a huge threshold = never trips = the
//   backstop faults (fail-safe). A clamp to 0 would trip-on-anything = unsafe.

// GOTCHA: classes get NO entry in the explicit-prototype block (native g++
#   needs prototypes for FREE functions only — ButtonClassifier,
#   ReadingAggregator and BaselineCapture set the precedent). Your edit adds
#   zero prototype lines.

// GOTCHA: UNGUARDED-region comments must avoid hardware tokens ("millis()",
//   "analogRead") — the purity gate greps tokens; it strips // comments, but
//   keep new prose clean anyway.

// GOTCHA: keep the DEBOUNCE_COUNT comparison as `++streak_ >= DEBOUNCE_COUNT`
#   — >= (not ==) is deliberate: if DEBOUNCE_COUNT were ever retuned DOWN in
#   commissioning, a longer pre-existing streak must still trip immediately.

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
#   `pio run -e pico` is the compile gate. Never adjust a test's expected value
#   to make it pass — they are machine-verified; a failure means the
#   implementation deviates.
```

## Implementation Blueprint

### Data models and structure

One class in the UNGUARDED region (complete verbatim listing in Task 1). State footprint: **4 bytes per instance** (measured semantics: `uint16_t` + `uint8_t` + `bool`, padded):

```cpp
uint16_t threshold_;  // baseline − TRIP_DROP_COUNTS, set by begin()
uint8_t  streak_;     // consecutive qualifying readings (0..DEBOUNCE_COUNT)
bool     tripped_;    // latched once the streak reached DEBOUNCE_COUNT
```

No enum, no buffers, no clock — the simplest service class in the chain by design.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "class BaselineCapture" firmware/jug_filler/jug_filler.ino   # EXPECT: 1 (upstream landed)
  - RUN: grep -c "end baseline capture (consumed by P1.M3.T3.S1" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (your insert anchor)
  - RUN: grep -c "class TripDetector" firmware/jug_filler/jug_filler.ino       # EXPECT: 0 (not yet landed)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "TRIP_DROP_COUNTS\|DEBOUNCE_COUNT" firmware/jug_filler/config.h  # EXPECT: both present (uint16_t, 350 / 8)
  - RUN: pio test -e native                                                    # EXPECT: exit 0, all projects [PASSED]
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — UNGUARDED region block
  - EDIT: insert the "Task 1 listing" verbatim AFTER the line
      `// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========`
    and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with one blank
    line on each side (keep exactly one blank line banner-to-banner on both
    sides of your block).
  - PRESERVE: everything else byte-for-byte — prototype block, includes, all
    other UNGUARDED sections, the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and all its tests
    executed against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_trip/test_trip.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (14 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_trip matches test_reading
    /test_baseline convention. PIO auto-discovers the new project — no build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — insert verbatim:**

```cpp
// ===================== Trip detection, per sensor (P1.M3.T3.S1) =====================
// Sensor chain stage 3 (PRD §7.3), pure and TIMELESS: an averaged reading at
// or below (baseline − TRIP_DROP_COUNTS = 350 counts) for DEBOUNCE_COUNT = 8
// CONSECUTIVE readings means that sensor is wet = tripped. One instance per
// sensor (0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2);
// instances share no state, so sensor independence is structural.
//
// Debounce logic: readings at or below the threshold advance a streak; any
// reading STRICTLY above it resets the streak to zero (PRD §7.3 "at or below
// (baseline − TRIP_DROP_COUNTS) for DEBOUNCE_COUNT consecutive readings" —
// the boundary baseline−350 itself QUALIFIES, inclusive; baseline−349 resets).
// When the streak reaches DEBOUNCE_COUNT the trip LATCHES: tripped() stays
// true for the remainder of the fill regardless of later readings — the
// valve-close decision (PRD §7.4) must never chatter. The latch is cleared
// only by begin() (next fill).
//
// This class owns ONLY the count-domain rule. Everything time-domain is the
// guards' business (P1.M3.T3.S2: TRIP_MIN_ELAPSED_S / EARLY_WET_WINDOW_S)
// and everything action-domain is the fill engine's (P1.M6.T1.S2: close
// valve, CONFIRM window).
//
// Caller contract (P1.M6.T1.S2, fed once per emitted averaged reading):
//   at fill start, after BaselineCapture (P1.M3.T2.S1) reports verdict Ok:
//     trip1.begin(baseline.baseline(0));   // VALIDATED dry baseline
//     trip2.begin(baseline.baseline(1));
//   per emitted pair:  trip1.feed(a1); trip2.feed(a2);
//   poll:              if (trip1.tripped() || trip2.tripped()) { /* engine */ }
// begin() with an UNVALIDATED baseline is a caller bug: a baseline below
// TRIP_DROP_COUNTS would wrap the threshold to a huge value (unsigned
// arithmetic is mod-2^16) and the detector would simply never trip — the
// fail-safe direction (the fill backstop, PRD §7.7, catches it as fault 3
// "timer, no trip"). No clamping on purpose: a clamp to 0 would make every
// reading qualify = trip-on-anything = unsafe.
class TripDetector {
public:
    TripDetector() : threshold_(0), streak_(0), tripped_(false) {}

    // Arm for a new fill with the sensor's VALIDATED dry baseline (counts,
    // BASELINE_MIN..BASELINE_MAX — BaselineCapture::verdict(s) == Ok).
    // Computes the wet threshold baseline − TRIP_DROP_COUNTS once, resets
    // the streak and clears the latch.
    void begin(uint16_t baseline) {
        threshold_ = (uint16_t)(baseline - TRIP_DROP_COUNTS);
        streak_ = 0;
        tripped_ = false;
    }

    // One AVERAGED reading from the ReadingAggregator stream (P1.M3.T1.S1).
    // At/below threshold advances the streak; strictly above resets it; the
    // DEBOUNCE_COUNT-th consecutive qualifying reading latches the trip.
    // No-op once latched (readings after the trip cannot change anything).
    void feed(uint16_t avg) {
        if (tripped_) return;  // latched for the remainder of the fill
        if (avg <= threshold_) {
            if (++streak_ >= DEBOUNCE_COUNT) tripped_ = true;
        } else {
            streak_ = 0;  // strictly above the threshold — streak broken
        }
    }

    // Latched trip flag — the ONLY output. Consumed by the fill engine
    // (P1.M6.T1.S2: close valve -> CONFIRM) and the guards (P1.M3.T3.S2).
    bool tripped() const { return tripped_; }

private:
    uint16_t threshold_;  // baseline − TRIP_DROP_COUNTS, set by begin()
    uint8_t  streak_;     // consecutive qualifying readings (0..DEBOUNCE_COUNT)
    bool     tripped_;    // latched once the streak reached DEBOUNCE_COUNT
};
// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========
```

**Task 2 listing — `test/test_trip/test_trip.cpp` (verbatim):**

```cpp
// test/test_trip/test_trip.cpp — native contract proof for TripDetector
// (P1.M3.T3.S1). Pins PRD §7.3: an averaged reading at or below
// (baseline − TRIP_DROP_COUNTS = 350) for DEBOUNCE_COUNT = 8 consecutive
// readings = that sensor tripped; the trip LATCHES for the remainder of the
// fill; the streak resets on any reading STRICTLY above the threshold;
// sensors are independent (one instance per sensor).
// Worked example used throughout: baseline 3400 -> threshold 3050
// (3400 − 350; 3050 itself QUALIFIES, inclusive; 3051 resets).
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T3S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- streak length (DEBOUNCE_COUNT = 8) ----

// Seven consecutive qualifying readings are NOT enough (PRD §7.3 demands 8).
void test_seven_qualifying_readings_do_not_trip() {
    TripDetector t;
    t.begin(3400);                       // threshold 3050
    for (int i = 0; i < 7; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
}

// ITEM CASE: the 8th consecutive qualifying reading trips — exactly 8.
void test_exactly_eight_consecutive_trip() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) {
        t.feed(3050);
        TEST_ASSERT_FALSE(t.tripped());
    }
    t.feed(3050);                        // the 8th
    TEST_ASSERT_TRUE(t.tripped());
}

// ITEM CASE: 7 qualifying readings, then ONE strictly above the threshold,
// resets the streak to zero — 7 more are still not enough; the full 8 are
// needed after the break.
void test_seventh_then_above_resets_streak() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) t.feed(3050);
    t.feed(3051);                        // strictly above -> streak = 0
    for (int i = 0; i < 7; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
    t.feed(3050);                        // 8th since the break
    TEST_ASSERT_TRUE(t.tripped());
}

// ---- threshold boundary (inclusive at exactly baseline − 350) ----

// ITEM CASE: exactly baseline − 350 (3050 for baseline 3400) QUALIFIES —
// PRD §7.3 "at or below (baseline − TRIP_DROP_COUNTS)" is inclusive.
void test_exactly_baseline_minus_350_qualifies() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(3050);   // == threshold
    TEST_ASSERT_TRUE(t.tripped());
}

// ITEM CASE: baseline − 349 (3051) is strictly ABOVE the threshold — it
// never advances the streak (even 100 in a row do not trip).
void test_one_count_above_threshold_never_trips() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 100; ++i) t.feed(3051);
    TEST_ASSERT_FALSE(t.tripped());
}

// The lowest legal baseline (BASELINE_MIN = 2000, verdict Ok) gives threshold
// 1650; exactly 1650 still qualifies 8 times -> trip. (2000 > 350: no wrap.)
void test_min_valid_baseline_threshold() {
    TripDetector t;
    t.begin(2000);                       // threshold 2000 − 350 = 1650
    for (int i = 0; i < 8; ++i) t.feed(1650);
    TEST_ASSERT_TRUE(t.tripped());
}

// A far-below-threshold reading (deep wet, 0 counts) still needs all 8 —
// depth does not shortcut debounce.
void test_deep_wet_still_needs_eight() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) t.feed(0);
    TEST_ASSERT_FALSE(t.tripped());
    t.feed(0);
    TEST_ASSERT_TRUE(t.tripped());
}

// Alternating wet/dry never accumulates 8 consecutive.
void test_alternating_never_trips() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 40; ++i) {
        t.feed(3050);
        t.feed(3051);
    }
    TEST_ASSERT_FALSE(t.tripped());
}

// ---- latch ----

// ITEM CASE: no re-trip noise after the latch — dry readings after the trip
// cannot un-trip; the valve-close decision cannot chatter.
void test_latch_survives_dry_readings_after_trip() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
    for (int i = 0; i < 100; ++i) t.feed(3400);   // fully dry again
    TEST_ASSERT_TRUE(t.tripped());
}

// Post-trip feeds of ANY value are inert (0 and 4095 garbage included).
void test_latch_ignores_post_trip_garbage() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(2000);
    TEST_ASSERT_TRUE(t.tripped());
    t.feed(0);
    t.feed(4095);
    t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
}

// ---- per-sensor independence ----

// ITEM CASE: sensor 1 trips while sensor 2 stays dry — one instance per
// sensor, no shared state.
void test_sensors_independent_one_trips() {
    TripDetector t1, t2;
    t1.begin(3400);
    t2.begin(3400);
    for (int i = 0; i < 8; ++i) {
        t1.feed(3050);
        t2.feed(3400);
    }
    TEST_ASSERT_TRUE(t1.tripped());
    TEST_ASSERT_FALSE(t2.tripped());
}

// Different baselines give different thresholds: 3000 counts qualifies for
// baseline 3400 (threshold 3050) but not for baseline 2000 (threshold 1650).
void test_different_baselines_different_thresholds() {
    TripDetector t1, t2;
    t1.begin(3400);   // threshold 3050
    t2.begin(2000);   // threshold 1650
    for (int i = 0; i < 8; ++i) {
        t1.feed(3000);
        t2.feed(3000);
    }
    TEST_ASSERT_TRUE(t1.tripped());
    TEST_ASSERT_FALSE(t2.tripped());
}

// ---- re-arm / defaults ----

// begin() re-arms for the next fill: latch cleared, streak zero, and a NEW
// baseline installs a new threshold (lower baseline -> lower threshold ->
// the same readings may no longer qualify).
void test_begin_rearms_with_new_baseline() {
    TripDetector t;
    t.begin(3400);                      // threshold 3050
    for (int i = 0; i < 8; ++i) t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
    t.begin(2400);                      // threshold 2050 — 3050 no longer qualifies
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 50; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 8; ++i) t.feed(2050);
    TEST_ASSERT_TRUE(t.tripped());
}

// A fresh instance (never begun) reads not-tripped and dry readings keep it
// that way; the engine protocol always calls begin() first (class comment).
void test_fresh_instance_dry_never_trips() {
    TripDetector t;
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 100; ++i) t.feed(4095);
    TEST_ASSERT_FALSE(t.tripped());
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_seven_qualifying_readings_do_not_trip);
    RUN_TEST(test_exactly_eight_consecutive_trip);
    RUN_TEST(test_seventh_then_above_resets_streak);
    RUN_TEST(test_exactly_baseline_minus_350_qualifies);
    RUN_TEST(test_one_count_above_threshold_never_trips);
    RUN_TEST(test_min_valid_baseline_threshold);
    RUN_TEST(test_deep_wet_still_needs_eight);
    RUN_TEST(test_alternating_never_trips);
    RUN_TEST(test_latch_survives_dry_readings_after_trip);
    RUN_TEST(test_latch_ignores_post_trip_garbage);
    RUN_TEST(test_sensors_independent_one_trips);
    RUN_TEST(test_different_baselines_different_thresholds);
    RUN_TEST(test_begin_rearms_with_new_baseline);
    RUN_TEST(test_fresh_instance_dry_never_trips);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: pure-service class shape (follows ButtonClassifier/ReadingAggregator
#   /BaselineCapture): total methods with documented protocol; const queries;
#   zero dynamic memory; explicit constructor init-list in DECLARATION order
#   (threshold_, streak_, tripped_) — native g++ -Wreorder is part of the gate set.

// PATTERN: begin() as the per-fill arming point (BaselineCapture precedent):
#   one long-lived instance per sensor, begin(baseline) at fill start, feed()
#   per reading. Constructing a fresh instance mid-run is legal but not the
#   protocol (P1.M6.T1.S2 owns that decision; both behave identically).

// CRITICAL: latch check FIRST in feed() (`if (tripped_) return;`) — before the
#   threshold comparison. Post-trip feeds must not even touch streak_.

// CRITICAL: `++streak_ >= DEBOUNCE_COUNT` (>=, not ==) and `avg <= threshold_`
#   (<=, not <) — both directions are boundary-pinned by tests; flipping either
#   breaks PRD §7.3.

// GOTCHA: `baseline - TRIP_DROP_COUNTS` on two uint16_t operands promotes to
#   int, subtracts, casts back to uint16_t — defined, exact for every validated
#   baseline (2000..4050 − 350 = 1650..3700, no wrap in-contract). Keep the
#   explicit (uint16_t) cast.

// GOTCHA: the FSM-side integration is DOCUMENTATION ONLY here — do not add any
#   ARDUINO-region code driving this class; P1.M6.T2.S1/P1.M6.T1.S2 own the wiring.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 bannered class block in
    the UNGUARDED region (after the baseline-capture end-banner). Prototype
    block, includes, ARDUINO region, setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes TRIP_DROP_COUNTS / DEBOUNCE_COUNT from
    config.h §5 group BY NAME.
TEST:
  - New project dir test/test_trip/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - false-trip guards (TRIP_MIN_ELAPSED_S / EARLY_WET_WINDOW_S) -> P1.M3.T3.S2
  - arming/feeding protocol + valve close + CONFIRM window     -> P1.M6.T1.S2
  - loop wiring of aggregator -> TripDetector                  -> P1.M6.T2.S1
  - fault latching + blink codes 1/2/3                         -> P1.M6.T1.S3
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the class compiles under arm-gcc/arduino-pico inside
# the sketch build (it sits in the UNGUARDED region, included in both builds).
# Compile errors mean a transcription deviation from the listing — fix, don't
# redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; test_seam, test_time_utils, test_log_utils, test_button,
# test_reading, test_baseline AND the new test_trip all [PASSED] (14/14 tests
# in test_trip). ANY failure: read the mismatch — the expected values are
# machine-verified against the real file, so a failure means the implementation
# (not the test) deviates. Never "fix" a test's expected value to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_trip
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_trip/

# (2) This task's .ino diff lives strictly inside the UNGUARDED region:
git diff firmware/jug_filler/jug_filler.ino | grep "^@@"
grep -nE -- "---- UNGUARDED REGION|---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   every hunk must start after the UNGUARDED marker line no. and end before
#   the ARDUINO marker line no.

# (3) Structural placement + counts:
grep -c "class TripDetector" firmware/jug_filler/jug_filler.ino            # 1
grep -c "end trip detector (consumed by P1.M3.T3.S2" firmware/jug_filler/jug_filler.ino  # 1
grep -n "end baseline capture (consumed by P1.M3.T3.S1" firmware/jug_filler/jug_filler.ino  # 1 hit, ABOVE your block

# (4) Public surface freeze (exactly the 3 contract members, no extras):
sed -n '/class TripDetector/,/^};/p' firmware/jug_filler/jug_filler.ino \
  | sed -n '/^public:/,/^private:/p' | grep -nE "^\s{4}(void|bool|TripDetector)"
# Expect: TripDetector(), begin, feed, tripped — and NOTHING else.

# (5) No churn where none belongs:
git diff --stat firmware/jug_filler/config.h platformio.ini   # empty
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region — strip // comments first (landed comments
#     legitimately name hardware concepts; only CODE must be hardware-free):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" \
  || echo "PURE OK (comments stripped)"

# (2) Region markers intact (note: -E -- required; a bare pattern starting
#     with dashes is parsed as an option and the grep fails):
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Constants by name, no magic numbers in the class body:
sed -n '/class TripDetector/,/end trip detector/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "\b350\b|\b8\b" || echo "NO MAGIC TRIP LITERALS"
# Expected: NO MAGIC TRIP LITERALS (TRIP_DROP_COUNTS / DEBOUNCE_COUNT appear
# by name only; the comment mentions "= 350"/"= 8" but comments are stripped).

# (4) Safety-reasoning audit (mirrors research/notes.md §2):
#     - 8 consecutive x 100 ms cadence = 800 ms of sustained wetness before the
#       valve closes — a splash shorter than that cannot trip (streak resets).
#     - threshold floor: lowest legal baseline 2000 -> threshold 1650; highest
#       4050 -> 3700: both far from the 0/4095 rails, no wrap possible.
#     - fail-safe: unvalidated-baseline wrap -> never trip -> backstop fault 3;
#       latch -> valve-close is one-shot, chatter-free.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_trip 14/14)
- [ ] "PURE OK (comments stripped)"; marker count 2; class exactly once; no magic trip literals in the class body
- [ ] `git status --short`: `.ino` + `test/test_trip/` only; this task's `.ino` hunks strictly inside the UNGUARDED region; config.h/platformio.ini untouched

### Feature Validation

- [ ] Streak: 7 qualifying readings do not trip; the 8th does; 7 + one strictly-above resets (full 8 needed after the break)
- [ ] Boundary: exactly baseline−350 qualifies (inclusive); baseline−349 never advances; lowest legal baseline 2000 → threshold 1650 works
- [ ] Robustness: deep wet (0 counts) still needs 8; alternating wet/dry never trips
- [ ] Latch: survives dry readings and garbage after the trip; only begin() unlatches
- [ ] Independence: one instance per sensor — one trips, the other does not; different baselines give different thresholds
- [ ] Re-arm: begin() clears the latch, zeroes the streak, installs the new baseline's threshold; fresh instance defaults safe

### Code Quality Validation

- [ ] Seam rules honored: zero hardware tokens in non-comment UNGUARDED lines; feed() takes no clock (timeless by design)
- [ ] Public surface exactly the 3 frozen members + default constructor; no streak()/threshold()/reset() extras
- [ ] Zero new config constants; `TRIP_DROP_COUNTS`/`DEBOUNCE_COUNT` by name
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M3.T3.S2, P1.M6.T1.S2); caller protocol quoted in the class comment
- [ ] The latch/no-clamp fail-safe rationale is documented in-code (it is — keep it)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't use `<` instead of `<=` for the threshold (or `==` instead of `>=` for the streak) — both boundary directions are PRD §7.3 semantics and are test-pinned
- ❌ Don't add elapsed-time logic, a nowMs parameter, or cadence checks — the aggregator owns cadence; the guards (P1.M3.T3.S2) own time; this class owns counts only
- ❌ Don't clamp the threshold in begin() — the wrap-to-huge on an out-of-contract baseline is the fail-safe direction (never trip → backstop fault); a clamp would be trip-on-anything
- ❌ Don't unlatch on dry readings, add a `reset()` public method, or reorder feed() so the latch check isn't first — "latched for the remainder of the fill" is the anti-chatter contract
- ❌ Don't add a sensor index, shared table, or static state to the class — one instance per sensor is the independence guarantee
- ❌ Don't add any constant to config.h — §8 is frozen; both tunables already exist
- ❌ Don't touch `ReadingAggregator`, `medianOf`, `BaselineCapture`, the prototype block, the ARDUINO region, config.h, platformio.ini, or other test files — completed tasks own them
- ❌ Don't adjust a test's expected value to make it pass — expected values are machine-verified; failures mean implementation deviation
