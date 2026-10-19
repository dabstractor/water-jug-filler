# PRP — P1.M3.T2.S1: BaselineCapture — 2s settle, median-of-averages, BASELINE_MIN..MAX check (pure)

## Goal

**Feature Goal**: Land the sensor chain's stage-2 pure logic in the UNGUARDED region of the sketch: a `BaselineCapture` class that, on `begin(nowMs)`, SETTLES for `BASELINE_SETTLE_MS` (2000 ms) collecting NOTHING (valve still closed — settle runs inside FILLING, arch §3.1), then collects exactly 5 averaged reading pairs per sensor from the `ReadingAggregator` stream (P1.M3.T1.S1), finishes with per-sensor dry baselines = the MEDIAN of those 5 readings (`medianOf` — one wet splash or wild spike cannot move it), and a plausibility verdict against `BASELINE_MIN..MAX` (2000..4050 counts, inclusive bounds). Zero hardware: the clock is injected; every behavior is native-provable. Zero new config constants; zero ARDUINO-region changes (P1.M3.T1.S2 is landing its ADC glue there in parallel — regions are disjoint).

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: one bannered `BaselineCapture` class block inserted into the UNGUARDED region, immediately AFTER the `// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========` banner (currently line 263) and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----` (currently line 265). NO prototype-block changes (classes get none — only free functions do), NO include changes, NO config.h changes, NO ARDUINO-region edits.
- `test/test_baseline/test_baseline.cpp` — NEW: 18 Unity native tests proving: settle collects nothing (not shortcut at 1999 ms; first pair accepted at exactly 2000, inclusive), done at exactly 5 pairs, post-done pairs ignored, boundary counts 2000/4050 inclusive pass, 1999/4051 fault, both-sensors-bad, sensor independence, median robust to one outlier per sensor, outlier-at-boundary, never-sampled sensor (0 counts) faults low, fail-safe accessors before done(), `begin()` reuse, settle across the 49.7-day wrap, out-of-domain accessor defense. All expected values hand-derived AND machine-verified this session against the REAL current `.ino` (`g++ -std=c++17 -Wall -Wextra -Werror`, all 17 assert groups green — see research/notes.md §3).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_baseline` project, plus regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading).
- `pio run -e pico` → compile-only `[SUCCESS]` — the class compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_baseline/` added by this task (the parallel task's own `.ino`/`config.h` changes are attributed to it — verify by REGION, not by file).
- The frozen public surface (`begin`, `addReading`, `done`, `baseline`, `verdict`, `Verdict`, `READINGS_PER_SENSOR`) is ready for downstream consumption with zero rework: TripDetector (P1.M3.T3.S1), fill engine baseline phase (P1.M6.T1.S2), fault-code path (P1.M6.T1.S3).

## User Persona

**Target User**: The implementing agents of the downstream chain — P1.M3.T3.S1 (TripDetector armed with `baseline(sensor)`), P1.M6.T1.S2 (fill engine drives `begin`/`addReading`/`done` and turns verdicts into valve-refusal + fault codes), P1.M6.T1.S3 (fault codes 1/2 = sensor index + 1), P1.M6.T2.S1 (integration loop forwards each emitted aggregator pair).

**Use Case**: Fill starts (BOOT short-press from IDLE). The engine calls `begin()` at the fill-start instant with the valve still closed. For 2 s nothing is collected (sensors stabilize after the press; the FSM may still abort during settle — that abort is its business). Then 5 averaged pairs (500 ms at the 100 ms cadence) are mediansed per sensor. The engine polls `done()`; on completion it checks `verdict(0)`/`verdict(1)`: any non-Ok → fault that sensor (blink code = index + 1) and REFUSE to open the valve (PRD §7.1 — this also catches "jug already full / tips wet" accidental starts, because a wet sensor reads far below the dry range). Both Ok → baselines are handed to TripDetector and the valve opens.

**User Journey**: short-press → 2 s of silence → 0.5 s of capture → either DONE→FILLING with valid baselines, or a latched fault blink (1 or 2) with the valve never having opened.

**Pain Points Addressed**: a sensor disturbed by the press or still wet from a previous fill must not poison the baseline (settle + median); a broken/disconnected sensor (rail, short, never-sampled → 0 counts) must fault BEFORE water is released, not after; an accidental start with the jug already full must be refused deterministically.

## Why

- PRD §7.1 (normative): "settle `BASELINE_SETTLE_MS` (2s), read both sensors' dry baselines (median of averaged readings). If either is outside `BASELINE_MIN..MAX` → fault that sensor (also catches 'jug still full / tips wet' accidental starts). Refuse to open the valve." §9.3: expected dry range ~3000–3900 counts — the 2000..4050 gate brackets it with margin.
- PRD §8 (frozen contract): `BASELINE_SETTLE_MS = 2000`, `BASELINE_MIN = 2000`, `BASELINE_MAX = 4050` already live in config.h — consumed BY NAME, never hardcoded.
- Architecture (`firmware_architecture.md` §1): this task's exact named slot — `BaselineCapture (P1.M3.T2.S1)` directly after `ReadingAggregator + medianOf` in the UNGUARDED region. Seam rules: pure logic takes `uint32_t nowMs` as a parameter, never calls `millis()`/`analogRead`/`Serial`. §3.1 (binding): settle runs INSIDE FILLING with the valve closed; a short-press abort during settle is the FSM's business — hence NO cancel method on this class.
- Item contract (task breakdown): input = averaged readings from P1.M3.T1.S1 (`ReadingAggregator` + `medianOf`, both landed and frozen) + time utilities from P1.M1.T2.S1 (`elapsedMs`, landed) + config constants; output = per-sensor verdicts + baselines consumed by P1.M3.T3.S1 and P1.M6.T1.S2/S3; docs = none (internal).
- TDD-by-SOW: architecture §5 mandates native Unity proofs for every §7 rule this class underpins.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; downstream PRPs are written against it):

1. **`enum Verdict : uint8_t { Ok = 0, TooLow = 1, TooHigh = 2 };`** — per-sensor plausibility.
2. **`static constexpr uint8_t READINGS_PER_SENSOR = 5;`** — task-contract constant, NOT a §8 tunable (§8 is frozen; same precedent as `LOGLINE_MAX`, `ADC_SAMPLE_PACE_MS`). Odd on purpose: median of 5 is the pure middle element (no even-pair truncation ambiguity); 5 × 100 ms = 500 ms of capture.
3. **`void begin(uint32_t nowMs)`** — starts a new capture: phase = Settle, settle anchor = `nowMs`, buffers/counters/verdicts reset to fail-safe defaults. Called by the fill engine at the fill-start instant.
4. **`void addReading(uint32_t nowMs, uint16_t avg1, uint16_t avg2)`** — TOTAL function, phase-aware: while Settle, the pair is DROPPED unless `elapsedMs(nowMs, startMs_) >= BASELINE_SETTLE_MS` (inclusive at exactly 2000 — house rule); the first accepted pair flips Settle→Collect; the 5th accepted pair computes both medians (`medianOf`), classifies both verdicts once, flips to Done; pairs after Done are ignored. Does NOT check the 100 ms reading cadence — the aggregator owns cadence.
5. **`bool done() const`** — true iff 5 pairs were collected post-settle. The FSM's only poll.
6. **`uint16_t baseline(uint8_t sensor) const`** — captured median (counts) for sensor 0/1. Returns 0 before `done()` and for out-of-domain indices (aggregator convention: consumers treat 0 as out-of-range).
7. **`Verdict verdict(uint8_t sensor) const`** — classification of the baseline: `< BASELINE_MIN → TooLow`, `> BASELINE_MAX → TooHigh`, else `Ok` (both bounds INCLUSIVE: 2000 and 4050 pass). Returns `TooLow` before `done()` and for out-of-domain indices — fail-safe invariant: **`verdict(s)` ALWAYS equals the classification of `baseline(s)`** (0 < 2000), so a premature read can never look Ok.
8. Sensor index domain (frozen since P1.M3.T1.S1): 0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2.

NOT in scope (owners): feeding readings from `loop()` (P1.M6.T2.S1/P1.M6.T1.S2); abort-during-settle semantics (P1.M6.T1.S2 — this class deliberately has NO cancel method); latching faults / blink codes / valve refusal (P1.M6.T1.S3 + engine — this class only REPORTS); trip detection (P1.M3.T3.S1); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `class BaselineCapture` present exactly once in the UNGUARDED region, after the reading-aggregator end-banner, before the ARDUINO region marker; banner comments per the verbatim listing
- [ ] Public surface is EXACTLY the 7 members above — no extra public members (no `phase()`, no `abort()`, no `progress()`)
- [ ] Settle is inclusive at exactly 2000 ms; nothing collected before it elapses; no cadence check inside `addReading`
- [ ] Baselines are medians of exactly 5 accepted pairs; verdict bounds inclusive at 2000/4050
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region
- [ ] `test/test_baseline/test_baseline.cpp` exists with explicit `int main()` Unity runner; all 18 listed cases present with the exact expected values
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` UNGUARDED region + new test dir only)
- [ ] config.h, platformio.ini, prototype block, include block, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete class listing and the complete 18-test file are provided verbatim below — both machine-compiled with `-Wall -Wextra -Werror` and executed green against the REAL current `.ino` on this host this session (see research/notes.md §3 for the command and the full expected-value table); insertion anchors quote the file as read this session (line numbers given, anchor BY TEXT so the parallel task's ARDUINO-region edits cannot invalidate them); the consumed upstream surface (`elapsedMs` semantics, `medianOf` in-place-sort contract, `ReadingAggregator::SENSOR_COUNT`) was read from the landed file, not assumed; validation commands mirror ones proven working in P1.M3.T1.S1's session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (UNGUARDED region only). Verified-current landmarks:
        (1) INSERT ANCHOR — line 263 `// ========= end reading aggregator
        (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========`;
        insert your block AFTER it (one blank line between), BEFORE line 265
        `// ---- #ifdef ARDUINO REGION: hardware glue ----` (blank line between).
        (2) `elapsedMs(now, since)` near the top of the region — compose it for
        the settle gate, never raw `now - start >= N`. (3) `medianOf` at the end
        of the aggregator section — sorts ASCENDING IN PLACE; its comment says
        "BaselineCapture (P1.M3.T2.S1) owns the buffer" — your class IS that owner.
        (4) `ReadingAggregator::SENSOR_COUNT` (== 2) — reuse it, do not redefine 2.
  gotcha: P1.M3.T1.S2 may have landed its ADC blocks in the ARDUINO region
        below line 265 (parallel work) — expected, NOT yours to touch. Anchor by
        the TEXT markers above, not by line numbers of anything below the region.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of the constants you consume BY NAME: BASELINE_SETTLE_MS
        (2000, uint32_t), BASELINE_MIN (2000, uint16_t), BASELINE_MAX (4050,
        uint16_t). Do NOT add READINGS_PER_SENSOR (or anything) here — §8 is
        frozen; the task-contract constant lives in the class.
  gotcha: never hardcode 2000/2000/4050 — always the config names (§9 commissioning
        may retune sensor values).

- file: test/test_reading/test_reading.cpp
  why: THE test pattern to follow verbatim-in-style: #include the .ino via relative
        path, empty setUp/tearDown, hand-derived expected values with derivation
        comments, ITEM CASE tags on contract-pinned tests, explicit
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (REQUIRED on PIO 6.2.0 — no auto-generated Unity runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity project,
        auto-discovered — no platformio.ini change needed.

- file: test/test_button/test_button.cpp
  why: enum assertion convention — this codebase asserts enum results as
        `TEST_ASSERT_TRUE(ev == ButtonClassifier::ShortPress)` (there is no enum-
        specific Unity macro in use). Do the same for Verdict.

- file: plan/001_53984f832581/P1M3T2S1/research/notes.md
  why: this task's research: §2 binding design decisions (fail-safe defaults, no
        abort method, N=5 placement, inclusivity), §3 the FULL machine-verified
        expected-value table + wrap arithmetic (0xFFFFF830 + 2000 == the wrap),
        §5 the downstream API freeze, §6 parallel-work safety.
  section: §3 (expected values) and §2 (design decisions)

- file: plan/001_53984f832581/P1M3T1S1/PRP.md
  why: CONTRACT for the upstream you consume. Frozen surface: takeReading emits
        the averaged pair; medianOf sorts in place, even-n via uint32 accumulator
        (irrelevant here — n=5 is odd, but do not "simplify" medianOf).
  gotcha: do NOT modify ReadingAggregator/medianOf in any way — they are frozen
        and their 16 tests must stay green untouched.

- file: plan/001_53984f832581/P1M3T1S2/PRP.md
  why: the PARALLEL task's contract — it edits ONLY the ARDUINO region (ADC state
        block + loop stage + one config.h comment) and its own gates enforce
        "UNGUARDED region byte-stable". Regions are disjoint by construction.
  gotcha: if you see its blocks/merge noise below the ARDUINO marker, LEAVE THEM
        ALONE; your diff must not touch anything below the region marker.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: §1 (seam rules + your named slot), §3.1 (binding: settle inside FILLING,
        valve closed; abort during settle is the FSM's business)
  why: justifies the no-cancel-method design and the begin/addReading/done protocol.

- url: https://www.megunolink.com/articles/coding/3-methods-filter-noisy-arduino-measurements
  why: median vs mean for sensor filtering — why the PRD medians the baseline
        (breakdown point 50%: a single wet splash during the 500 ms capture cannot
        move the median; the mean would follow it). Same reference that underpinned
        P1.M3.T1.S1's medianOf.
  critical: the median's robustness is the SAFETY argument for gate correctness —
        one outlier reading must not fault a good sensor nor pass a wet one.

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
│   ├── jug_filler.ino        # seam + time helpers + banner + ButtonClassifier
│   │                         #   + ReadingAggregator/medianOf (P1.M3.T1.S1, landed;
│   │                         #   aggregator end-banner at line 263 = YOUR ANCHOR)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service, setup/loop
│   │                         #   (+ P1.M3.T1.S2's ADC blocks, landing in parallel)
│   │                         # ← EDIT (UNGUARDED region only: +1 bannered class block)
│   └── config.h              # every §8 tunable — READ-ONLY (BASELINE_* consumed by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/         # P1.M3.T1.S1's — untouched, must stay green
│   └── test_baseline/        # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): BaselineCapture class in the
│                             #   UNGUARDED region — settle gate, 5-pair capture,
│                             #   median baselines, inclusive plausibility verdicts
└── test/
    └── test_baseline/
        └── test_baseline.cpp # NEW: native Unity proof — settle boundaries, capture
                              #   length, median robustness, gate boundaries, fail-safe
                              #   defaults, reuse, 49.7-day wrap, OOR defense
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: NEVER subtract and compare raw timestamps ("now - start >= 2000" is
//   FORBIDDEN as hand-rolled code — ordering across the 49.7-day wrap is
//   meaningless). ALWAYS compose elapsedMs(now, since) >= BASELINE_SETTLE_MS
//   (house rule, P1.M1.T2.S1). The settle is INCLUSIVE at exactly 2000 ms —
//   same convention as BUTTON_DEBOUNCE_MS ("exactly 40 is enough"), readingDue
//   ("inclusive at exactly 100"), deadlinePassed ("inclusive at equality").

// CRITICAL: medianOf SORTS ITS ARGUMENT ASCENDING IN PLACE. That is fine here —
//   the class passes its OWN capture buffers and never needs the raw order
//   after the medians are taken (medianOf's comment anticipated exactly this).
//   Do NOT copy to a temp buffer "to be safe" — that is dead RAM for nothing.

// CRITICAL: verdict(s) must ALWAYS equal the classification of baseline(s).
//   Before done() that means baseline()==0 AND verdict()==TooLow (0 < BASELINE_MIN)
//   — the fail-safe direction: a premature read can never look Ok, so an engine
//   bug that skips the done() gate refuses to open the valve instead of opening
//   it on garbage. Initialize BOTH in the constructor AND in begin().

// CRITICAL: NO abort/cancel method — the item contract says "abort during settle
//   is the FSM's business, P1.M6.T1.S2". The FSM simply stops calling; begin()
//   resets everything for the next fill. Adding one would duplicate engine state.

// CRITICAL: addReading must NOT check the 100 ms reading cadence — the aggregator
//   owns cadence (frozen in P1.M3.T1.S1); this class's only time-dependence is
//   the settle gate. Tests legitimately feed pairs at any spacing.

// CRITICAL: READINGS_PER_SENSOR = 5 is a TASK-CONTRACT constant inside the class,
//   NOT a config.h tunable — §8 is frozen (PRD lists no baseline-sample count).
//   Comment it that way in the listing (precedent: LOGLINE_MAX, ADC_SAMPLE_PACE_MS).

// GOTCHA: classes get NO entry in the explicit-prototype block (native g++ needs
//   prototypes for FREE functions only — ButtonClassifier and ReadingAggregator
//   set the precedent). Your edit adds zero prototype lines.

// GOTCHA: UNGUARDED-region comments must avoid hardware tokens ("injected
//   clock"/"nowMs", never "millis()"/"analogRead") — the purity gate greps tokens;
//   it strips // comments, but keep new prose clean anyway.

// GOTCHA: caller contract is nowMs NON-DECREASING (same clock domain as every
//   other service) — same contract ButtonClassifier documents. State it in the
//   class comment.

// GOTCHA: keep new comments free of "Ok" ambiguity — Verdict::Ok is the enum;
//   prose "in range"/"plausible" reads better under grep.

// GOTCHA (parallel work): P1.M3.T1.S2 edits ONLY the ARDUINO region + one config.h
//   comment. If its blocks are present, ignore them; if git shows the .ino modified
//   by both tasks, attribute hunks by region — yours live strictly between the
//   region markers' UNGUARDED side.

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
//   `pio run -e pico` is the compile gate. Never adjust a test's expected value
//   to make it pass — they are machine-verified; a failure means the
//   implementation deviates.
```

## Implementation Blueprint

### Data models and structure

One class in the UNGUARDED region (complete verbatim listing in Task 1). State footprint: **36 bytes per instance** (measured on host g++ this session):

```cpp
enum Phase : uint8_t { Settle = 0, Collect = 1, Done = 2 };   // one-way, private
Phase     phase_;        // Settle -> Collect -> Done
uint32_t  startMs_;      // begin() instant — settle anchor
uint8_t   taken_;        // accepted pairs so far (0..5)
uint16_t  readings_[2][5];   // per-sensor capture buffers (medianOf scratch, 20 B)
uint16_t  baseline_[2];      // medians, valid at Done
Verdict   verdict_[2];       // classifications, valid at Done (TooLow before)
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "class ReadingAggregator" firmware/jug_filler/jug_filler.ino    # EXPECT: 1
  - RUN: grep -c "end reading aggregator (consumed by P1.M3.T2.S1" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (your insert anchor)
  - RUN: grep -c "class BaselineCapture" firmware/jug_filler/jug_filler.ino      # EXPECT: 0 (not yet landed)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -c "uint16_t medianOf(uint16_t\* values, size_t n);" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (upstream intact)
  - RUN: pio test -e native                                                      # EXPECT: exit 0, all projects [PASSED]
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — UNGUARDED region block
  - EDIT: insert the "Task 1 listing" verbatim AFTER the line
      `// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========`
    and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with one blank
    line on each side (the blank line between the two markers currently separates
    them; keep exactly one blank line banner-to-banner on both sides of your block).
  - PRESERVE: everything else byte-for-byte — prototype block, includes, all other
    UNGUARDED sections, the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and all its tests executed
    against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_baseline/test_baseline.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (18 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_baseline matches test_reading
    /test_button convention. PIO auto-discovers the new project — no build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — insert verbatim:**

```cpp
// ===================== Baseline capture + plausibility gate (P1.M3.T2.S1) =====================
// Sensor chain stage 2 (PRD §7.1), pure: on fill start, SETTLE for
// BASELINE_SETTLE_MS (2000 ms) with the valve still closed (settle runs inside
// FILLING, valve LOW — arch §3.1), collecting NOTHING; then collect exactly
// READINGS_PER_SENSOR (5) averaged reading pairs from the ReadingAggregator
// stream (P1.M3.T1.S1) and finish with per-sensor dry baselines = the MEDIAN
// of those 5 readings (breakdown point 50%: one wet splash or one wild spike
// during capture cannot move the baseline) plus a plausibility verdict against
// BASELINE_MIN..MAX (2000..4050 counts, 12-bit) — this also catches "jug
// already full / tips wet" accidental starts (a wet sensor reads far below the
// dry range, PRD §9.3) and a never-sampled sensor (aggregator emits 0 —
// out-of-range by design, P1.M3.T1.S1).
//
// Verdicts are the fill gate (PRD §7.1): if either sensor is not Ok, the fill
// engine (P1.M6.T1.S2) faults that sensor (fault code = sensor index + 1,
// P1.M6.T1.S3) and REFUSES to open the valve. This class only REPORTS — it
// never touches hardware.
//
// Caller contract (P1.M6.T1.S2, driven once per loop pass by P1.M6.T2.S1;
// nowMs non-decreasing, same clock domain as all services):
//   baseline.begin(fillStartMs);                    // at fill start, valve closed
//   if (agg.readingDue(nowMs)) {
//       agg.takeReading(nowMs, a1, a2);
//       baseline.addReading(nowMs, a1, a2);         // total: dropped until settle elapses
//   }
//   if (baseline.done()) { /* route verdict()/baseline() once */ }
// addReading is time-gated ONLY by the settle: it does not check the reading
// cadence (the aggregator owns cadence) and is legal to call on every emitted
// pair. All timing composes elapsedMs (rollover-safe, P1.M1.T2.S1).
//
// Abort during settle is the FSM's business (P1.M6.T1.S2): there is NO cancel
// method by design — the FSM simply stops calling; begin() resets all state
// for the next fill. Accessors are meaningful once done(); before that
// baseline() is 0 and verdict() is TooLow — fail-safe: verdict(sensor) ALWAYS
// equals the classification of baseline(sensor) under BASELINE_MIN..MAX, and
// 0 < BASELINE_MIN, so a premature read can never look Ok.
class BaselineCapture {
public:
    // Per-sensor plausibility of the captured dry baseline (PRD §7.1).
    enum Verdict : uint8_t { Ok = 0, TooLow = 1, TooHigh = 2 };

    // Averaged readings medians per sensor — task contract (P1.M3.T2.S1),
    // NOT a §8 tunable. Odd on purpose: the median of 5 is the pure middle
    // element (no even-pair truncation); 5 x 100 ms = 500 ms of capture.
    static constexpr uint8_t READINGS_PER_SENSOR = 5;

    BaselineCapture()
        : phase_(Phase::Settle), startMs_(0), taken_(0),
          baseline_{0, 0}, verdict_{Verdict::TooLow, Verdict::TooLow} {}

    // Start a new capture at `nowMs` (the fill-start instant). Resets everything.
    void begin(uint32_t nowMs) {
        phase_ = Phase::Settle;
        startMs_ = nowMs;
        taken_ = 0;
        baseline_[0] = 0;
        baseline_[1] = 0;
        verdict_[0] = Verdict::TooLow;  // fail-safe defaults: baseline 0
        verdict_[1] = Verdict::TooLow;  // classifies below BASELINE_MIN
    }

    // Feed one emitted averaged pair. TOTAL: during Settle the pair is DROPPED
    // unless BASELINE_SETTLE_MS has elapsed since begin() (inclusive at exactly
    // 2000 — house rule); the first accepted pair flips to Collect. On the
    // READINGS_PER_SENSOR-th accepted pair the medians + verdicts are computed
    // once and the capture is Done — later pairs are ignored.
    void addReading(uint32_t nowMs, uint16_t avg1, uint16_t avg2) {
        if (phase_ == Phase::Done) return;  // stable after finish
        if (phase_ == Phase::Settle) {
            if (elapsedMs(nowMs, startMs_) < BASELINE_SETTLE_MS) return;
            phase_ = Phase::Collect;  // settle elapsed — start collecting HERE
        }
        readings_[0][taken_] = avg1;
        readings_[1][taken_] = avg2;
        if (++taken_ >= READINGS_PER_SENSOR) {
            // medianOf sorts each buffer ascending in place — fine: the raw
            // order is never needed again once the medians are taken.
            baseline_[0] = medianOf(readings_[0], READINGS_PER_SENSOR);
            baseline_[1] = medianOf(readings_[1], READINGS_PER_SENSOR);
            verdict_[0] = classify(baseline_[0]);
            verdict_[1] = classify(baseline_[1]);
            phase_ = Phase::Done;
        }
    }

    // True iff the capture finished (READINGS_PER_SENSOR pairs collected
    // post-settle). The FSM's only poll; verdicts/baselines gate on this.
    bool done() const { return phase_ == Phase::Done; }

    // Captured dry baseline (counts) for `sensor` (0 == PIN_SENSOR1 == fault
    // code 1, 1 == PIN_SENSOR2 == fault code 2). 0 until done() or for an
    // out-of-domain index — consumers treat 0 as out-of-range (aggregator
    // convention, P1.M3.T1.S1).
    uint16_t baseline(uint8_t sensor) const {
        return (sensor < ReadingAggregator::SENSOR_COUNT) ? baseline_[sensor] : 0;
    }

    // Plausibility verdict for `sensor`. TooLow until done() / for an
    // out-of-domain index — fail-safe: verdict(sensor) ALWAYS equals the
    // classification of baseline(sensor), and 0 < BASELINE_MIN.
    Verdict verdict(uint8_t sensor) const {
        return (sensor < ReadingAggregator::SENSOR_COUNT) ? verdict_[sensor]
                                                          : Verdict::TooLow;
    }

private:
    enum Phase : uint8_t { Settle = 0, Collect = 1, Done = 2 };

    // PRD §7.1 plausibility gate: bounds INCLUSIVE (2000 and 4050 are Ok).
    static Verdict classify(uint16_t v) {
        if (v < BASELINE_MIN) return Verdict::TooLow;
        if (v > BASELINE_MAX) return Verdict::TooHigh;
        return Verdict::Ok;
    }

    Phase     phase_;       // Settle -> Collect -> Done (one-way)
    uint32_t  startMs_;     // begin() instant — settle anchor
    uint8_t   taken_;       // accepted pairs so far (0..READINGS_PER_SENSOR)
    uint16_t  readings_[ReadingAggregator::SENSOR_COUNT][READINGS_PER_SENSOR];  // capture buffers / median scratch (20 B)
    uint16_t  baseline_[ReadingAggregator::SENSOR_COUNT];  // medians, valid at Done
    Verdict   verdict_[ReadingAggregator::SENSOR_COUNT];   // classifications, valid at Done
};
// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========
```

**Task 2 listing — `test/test_baseline/test_baseline.cpp` (verbatim):**

```cpp
// test/test_baseline/test_baseline.cpp — native contract proof for
// BaselineCapture (P1.M3.T2.S1). Pins PRD §7.1: 2 s settle collecting
// nothing (not shortcut at 1999 ms; first pair accepted at exactly 2000,
// inclusive), exactly 5 averaged pairs per sensor, baseline = median of 5
// (one outlier per sensor cannot move it), plausibility gate inclusive at
// BASELINE_MIN (2000) / BASELINE_MAX (4050) — 1999 -> TooLow, 4051 ->
// TooHigh — both-sensors-bad, sensor independence, fail-safe accessors
// before done(), begin() reuse, settle across the 49.7-day wrap.
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T2S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- settle (injected clock; inclusive at exactly 2000; wrap-safe) ----

// A dozen-plus pairs during the settle window are ALL dropped — nothing is
// collected before BASELINE_SETTLE_MS elapses (PRD §7.1).
void test_settle_collects_nothing() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 100; t <= 1900; t += 100) bc.addReading(t, 3400, 3300);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
}

// ITEM CASE: settle not shortcut at 1999 ms — five pairs at 1999 would
// complete a capture if any leaked in; none may.
void test_settle_not_shortcut_at_1999() {
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i) bc.addReading(1999, 1000, 1000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
}

// ITEM CASE: the settle completes at EXACTLY 2000 (inclusive, house rule) —
// the first pair at t=2000 is accepted; the t=1999 pair just before is not.
void test_first_pair_accepted_exactly_at_2000() {
    BaselineCapture bc;
    bc.begin(0);
    bc.addReading(1999, 1000, 1000);  // still settle: dropped
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(2500, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ---- capture length ----

// done() flips on exactly the 5th accepted pair.
void test_done_needs_exactly_five_pairs() {
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 4; ++i) bc.addReading(2000 + 100 * (uint32_t)i, 3500, 2500);
    TEST_ASSERT_FALSE(bc.done());
    bc.addReading(2400, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
}

// Pairs after Done are ignored — verdicts/baselines are stable.
void test_pairs_after_done_ignored() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    bc.addReading(2500, 0, 4095);  // garbage: must change nothing
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(2500, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// ---- plausibility gate (BASELINE_MIN..MAX, bounds inclusive) ----

// ITEM CASE: 2000 == BASELINE_MIN is IN range.
void test_min_boundary_2000_inclusive_ok() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 2000, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_EQUAL_UINT16(2000, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ITEM CASE: 4050 == BASELINE_MAX is IN range.
void test_max_boundary_4050_inclusive_ok() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 4050, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_EQUAL_UINT16(4050, bc.baseline(0));
}

// ITEM CASE: 1999 — one count below the floor — faults TooLow.
void test_below_min_1999_toolow() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 1999, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_EQUAL_UINT16(1999, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ITEM CASE: 4051 — one count above the ceiling — faults TooHigh.
void test_above_max_4051_toohigh() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 4051, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooHigh);
    TEST_ASSERT_EQUAL_UINT16(4051, bc.baseline(0));
}

// ITEM CASE: both sensors can be bad at once (one low, one high) — the FSM
// faults each independently (codes 1 and 2).
void test_both_sensors_faulted() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 1999, 4051);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooHigh);
}

// Sensors are judged independently.
void test_sensors_independent() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 1999);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
}

// ---- median-of-5 robustness ----

// ITEM CASE: one outlier per sensor (a wet-splash low on sensor 1, a wild
// spike high on sensor 2) cannot move the median of 5.
//   s0 sorted: 1000 3400 3405 3408 3410 -> median 3405
//   s1 sorted: 3300 3305 3308 3310 4095 -> median 3308
void test_median_robust_to_one_outlier() {
    const uint16_t s0[] = {3400, 3410, 1000, 3405, 3408};
    const uint16_t s1[] = {3300, 3310, 4095, 3305, 3308};
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i)
        bc.addReading(2000 + 100 * (uint32_t)i, s0[i], s1[i]);
    TEST_ASSERT_EQUAL_UINT16(3405, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(3308, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// A single low outlier cannot push a boundary-straddling capture out of
// range: median of {1999,2000,2000,2000,2000} is 2000 -> in range.
void test_one_outlier_cannot_fault_at_boundary() {
    const uint16_t v[] = {1999, 2000, 2000, 2000, 2000};
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i) bc.addReading(2000 + 100 * (uint32_t)i, v[i], 3400);
    TEST_ASSERT_EQUAL_UINT16(2000, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// A never-sampled sensor emits 0 counts (aggregator convention) -> TooLow,
// i.e. out-of-range by design — the other sensor is unaffected.
void test_never_sampled_sensor_zero_toolow() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 0);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
}

// ---- fail-safe defaults / reuse / defense ----

// Before done() the accessors are fail-safe: baseline 0 classifies TooLow —
// a premature read can never look Ok (verdict ALWAYS classifies baseline).
void test_accessors_fail_safe_before_done() {
    BaselineCapture bc;
    bc.begin(1000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
}

// begin() fully resets a completed capture for the next fill.
void test_begin_resets_for_next_fill() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 3300);
    TEST_ASSERT_TRUE(bc.done());
    bc.begin(5000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    for (uint32_t t = 7000; t <= 7400; t += 100) bc.addReading(t, 2000, 4051);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooHigh);
}

// Settle across the 49.7-day wrap: begin at 0xFFFFF830 — exactly +2000 is
// the wrapped instant 0. Five pairs at 0xFFFFFFFF (elapsed 463 ms) drop;
// pairs from t=0 (elapsed exactly 2000, wrapped) are accepted.
void test_settle_survives_49day_wrap() {
    BaselineCapture bc;
    bc.begin(0xFFFFF830UL);
    for (int i = 0; i < 5; ++i) bc.addReading(0xFFFFFFFFUL, 3400, 3300);
    TEST_ASSERT_FALSE(bc.done());
    for (uint32_t t = 0; t <= 400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// Out-of-domain sensor indices: defensive 0 / TooLow (aggregator convention).
void test_out_of_domain_sensor_accessors() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(2));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(255));
    TEST_ASSERT_TRUE(bc.verdict(2) == BaselineCapture::TooLow);
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_settle_collects_nothing);
    RUN_TEST(test_settle_not_shortcut_at_1999);
    RUN_TEST(test_first_pair_accepted_exactly_at_2000);
    RUN_TEST(test_done_needs_exactly_five_pairs);
    RUN_TEST(test_pairs_after_done_ignored);
    RUN_TEST(test_min_boundary_2000_inclusive_ok);
    RUN_TEST(test_max_boundary_4050_inclusive_ok);
    RUN_TEST(test_below_min_1999_toolow);
    RUN_TEST(test_above_max_4051_toohigh);
    RUN_TEST(test_both_sensors_faulted);
    RUN_TEST(test_sensors_independent);
    RUN_TEST(test_median_robust_to_one_outlier);
    RUN_TEST(test_one_outlier_cannot_fault_at_boundary);
    RUN_TEST(test_never_sampled_sensor_zero_toolow);
    RUN_TEST(test_accessors_fail_safe_before_done);
    RUN_TEST(test_begin_resets_for_next_fill);
    RUN_TEST(test_settle_survives_49day_wrap);
    RUN_TEST(test_out_of_domain_sensor_accessors);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: pure-service class shape (follows ButtonClassifier/ReadingAggregator):
//   - all timing via injected nowMs, composed with elapsedMs (never raw stamps)
//   - total methods with documented protocol; const queries; zero dynamic memory
//   - explicit constructor init-list in DECLARATION order (phase_, startMs_,
//     taken_, baseline_, verdict_) — native g++ -Wreorder is part of the gate set

// PATTERN: phase machine as plain nested enum with explicit values
//   (ButtonClassifier::Event precedent); one-way transitions only
//   (Settle -> Collect -> Done) — no way back except begin().

// CRITICAL: the settle gate must read elapsedMs(nowMs, startMs_) <
//   BASELINE_SETTLE_MS -> drop. "<" (not <=) makes exactly-2000 accepted —
//   pinned from BOTH sides by tests at 1999 and 2000.

// CRITICAL: verdict defaults (constructor AND begin) are TooLow, never Ok —
//   the fail-safe direction. An engine bug that reads verdicts before done()
//   refuses the fill instead of opening the valve on garbage.

// GOTCHA: constructing a fresh BaselineCapture mid-run is legal, but the
//   established protocol is ONE long-lived instance + begin() per fill
//   (P1.M6.T1.S2 owns that decision; both behave identically).

// GOTCHA: the FSM-side integration is DOCUMENTATION ONLY here — do not add
//   any ARDUINO-region code driving this class; P1.M6.T2.S1/P1.M6.T1.S2 own
//   the wiring.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 bannered class block in
    the UNGUARDED region (after the reading-aggregator end-banner). Prototype
    block, includes, ARDUINO region, setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes BASELINE_SETTLE_MS / BASELINE_MIN /
    BASELINE_MAX from config.h §5 group BY NAME. READINGS_PER_SENSOR is a
    class-static task-contract constant (not a §8 tunable).
TEST:
  - New project dir test/test_baseline/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - trip rule (baseline-350, 8 consecutive) + guards   -> P1.M3.T3.S1/S2
  - begin/addReading/done driving + valve refusal on
    bad verdicts + abort-during-settle semantics        -> P1.M6.T1.S2
  - fault latching + blink codes 1/2 mapping            -> P1.M6.T1.S3
  - loop wiring of aggregator -> BaselineCapture        -> P1.M6.T2.S1
  - ADC sampling feeding the aggregator                 -> P1.M3.T1.S2 (parallel)
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
# test_reading AND the new test_baseline all [PASSED] (18/18 tests in
# test_baseline). ANY failure: read the mismatch — the expected values are
# machine-verified against the real file, so a failure means the implementation
# (not the test) deviates. Never "fix" a test's expected value to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_baseline
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts (attribute .ino hunks by REGION;
#     the parallel task's ARDUINO-region/config.h changes are its own):
git status --short
# Expected (attributed to THIS task):
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_baseline/

# (2) This task's .ino diff lives strictly inside the UNGUARDED region:
git diff firmware/jug_filler/jug_filler.ino | grep "^@@"          # every hunk between
grep -nE -- "---- UNGUARDED REGION|---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   the UNGUARDED marker line no. and the ARDUINO marker line no.

# (3) Structural placement + counts:
grep -c "class BaselineCapture" firmware/jug_filler/jug_filler.ino            # 1
grep -c "end baseline capture (consumed by P1.M3.T3.S1" firmware/jug_filler/jug_filler.ino  # 1
grep -n "end reading aggregator (consumed by P1.M3.T2.S1" firmware/jug_filler/jug_filler.ino  # 1 hit, ABOVE your block

# (4) Public surface freeze (exactly the 7 contract members, no extras):
sed -n '/class BaselineCapture/,/^};/p' firmware/jug_filler/jug_filler.ino \
  | sed -n '/^public:/,/^private:/p' | grep -nE "^\s{4}(void|bool|uint16_t|Verdict|enum|static)"
# Expect: enum Verdict, READINGS_PER_SENSOR, begin, addReading, done,
#         baseline, verdict — and NOTHING else.

# (5) No churn where none belongs:
git diff --stat firmware/jug_filler/config.h platformio.ini   # empty for THIS task's diff
# (config.h may show the PARALLEL task's comment edit — not yours; do not
#  touch or revert it.)
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region — strip // comments first (landed comments
#     legitimately name clock concepts; only CODE must be hardware-free):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" \
  || echo "PURE OK (comments stripped)"

# (2) Region markers intact (note: -E -- required; a bare pattern starting
#     with dashes is parsed as an option and the grep fails):
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Constants by name, no magic numbers in the class body:
sed -n '/class BaselineCapture/,/end baseline capture/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "2000|4050" || echo "NO MAGIC BASELINE LITERALS"
# Expected: NO MAGIC BASELINE LITERALS (the only 5s are READINGS_PER_SENSOR=5
# and its comments; BASELINE_* appear by name only).

# (4) Safety-arithmetic audit (reasoning check, mirrors research/notes.md §3):
#     settle duration: BASELINE_SETTLE_MS (2000 ms) — pairs at t<2000 dropped,
#     t=2000 accepted (inclusive) -> total pre-valve time = 2000 + 5*100 = 2500 ms
#     before the engine may open the valve; capture uses medianOf (in-place sort,
#     uint32 even-pair accumulator — n=5 is odd so the middle element is exact).
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_baseline 18/18)
- [ ] "PURE OK (comments stripped)"; marker count 2; class exactly once; no magic baseline literals in the class body
- [ ] `git status --short`: `.ino` + `test/test_baseline/` from this task; this task's `.ino` hunks strictly inside the UNGUARDED region; config.h/platformio.ini untouched by this task

### Feature Validation

- [ ] Settle: collects nothing before 2000 ms; 1999 pinned dropped; exactly 2000 pinned accepted (inclusive); survives the 49.7-day wrap
- [ ] Capture: done at exactly 5 pairs; post-done pairs ignored; begin() fully resets
- [ ] Plausibility: 2000/4050 inclusive Ok; 1999 TooLow; 4051 TooHigh; both-sensors-bad; sensors independent; never-sampled (0) TooLow
- [ ] Median robustness: one outlier per sensor cannot move the baseline; one outlier cannot fault a boundary-straddling capture
- [ ] Fail-safe: accessors before done() read 0/TooLow (never Ok); out-of-domain indices defensive

### Code Quality Validation

- [ ] Seam rules honored: injected clock only, composed via `elapsedMs`; zero hardware tokens in non-comment UNGUARDED lines
- [ ] Public surface exactly the 7 frozen members; no cancel/abort method (FSM's business); no cadence check in `addReading`
- [ ] Zero new config constants; `BASELINE_SETTLE_MS`/`BASELINE_MIN`/`BASELINE_MAX` by name
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3); caller protocol quoted in the class comment
- [ ] The fail-safe-default invariant and the no-cancel rationale are documented in-code (they are — keep them)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't hand-roll `now - start >= 2000` timestamp math — compose `elapsedMs()` (wrap safety is the entire point of P1.M1.T2.S1)
- ❌ Don't make the settle exclusive (`<=`, or `<` with off-by-one) — exactly 2000 must ACCEPT; both boundary sides are pinned by tests
- ❌ Don't default verdicts to `Ok` — 0 counts classify TooLow; the fail-safe direction is the invariant `verdict(s) == classify(baseline(s))`
- ❌ Don't add a cancel/abort method, a `phase()` accessor, a progress counter, or any public member beyond the frozen 7 — downstream PRPs (P1.M3.T3.S1, P1.M6.T1.S2/S3) are written against this exact surface
- ❌ Don't add `READINGS_PER_SENSOR` (or any constant) to config.h — §8 is frozen; the task-contract constant lives in the class
- ❌ Don't check reading cadence inside `addReading` — the aggregator owns cadence; double-gating would drop legitimate pairs after loop stalls
- ❌ Don't copy readings to a temp buffer before `medianOf` — the in-place sort on the class's own buffers is the documented contract
- ❌ Don't touch `ReadingAggregator`, `medianOf`, the prototype block, the ARDUINO region, config.h, platformio.ini, or other test files — parallel/completed tasks own them
- ❌ Don't adjust a test's expected value to make it pass — expected values are machine-verified; failures mean implementation deviation
- ❌ Don't run `pio test -e pico` (no transport) or upload anything — this task is pure logic, native-proven; bench work is P1.M7.T2.S1

---

**Confidence Score: 10/10** — the complete listing (class + full 18-test suite semantics) was compiled with `-Wall -Wextra -Werror` against the REAL current `.ino` on this host and every expected value executed green before this PRP was written (research/notes.md §3 records the command and table); insertion anchors quote the file as read this session and are TEXT markers immune to the parallel task's ARDUINO-region edits; upstream contracts (`elapsedMs`, `medianOf`, `SENSOR_COUNT`) were read from the landed file, not assumed. Residual risk is limited to transcription fidelity, which the verbatim listings and the Level-2 gate directly catch.
