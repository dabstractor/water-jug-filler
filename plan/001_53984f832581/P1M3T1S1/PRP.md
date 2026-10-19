# PRP — P1.M3.T1.S1: ReadingAggregator + median helper (pure)

## Goal

**Feature Goal**: Land the sensor chain's stage-1 pure logic in the UNGUARDED region of the sketch: a `ReadingAggregator` that turns a stream of raw 12-bit ADC counts (two sensors) into exactly ONE averaged reading pair per `READING_INTERVAL_MS` (100 ms) — each average taken over the last `ADC_SAMPLES_PER_READING` (32) samples per sensor, with the first sample pre-filling the window so readings are valid during warmup — plus the free function `uint16_t medianOf(uint16_t* values, size_t n)` that PRD §7.1 baselines are computed from (median of averaged readings). Zero hardware: the clock is injected; all behavior is native-provable. Zero new config constants.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: (a) one prototype line (`medianOf`) added to the explicit-prototype block; (b) a bannered `ReadingAggregator` class + `medianOf` free function inserted into the UNGUARDED region, between the end of the ButtonClassifier section and the `#ifdef ARDUINO` region marker. NO ARDUINO-region edits (P1.M2.T2.S1 is landing its blocks there in parallel), NO include changes, NO config.h changes.
- `test/test_reading/test_reading.cpp` — NEW: ~16 Unity native tests proving exact averaging (truncation pinned), cadence with injected clock including the 49.7-day wrap, no-catch-up-burst-after-stall, warmup pre-fill, out-of-domain sensor defense, and median odd/even/truncation/in-place/overflow/edge cases. All expected values are hand-derived AND machine-verified (g++, this repo, this session — see research/notes.md §3).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_reading` project, plus regressions (test_seam, test_time_utils, test_log_utils, test_button).
- `pio run -e pico` → compile-only `[SUCCESS]` — the class compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_reading/` added by this task (parallel tasks' files attributed to them, untouched).
- The frozen public surface (`addSample`, `readingDue`, `takeReading`, `medianOf`) is ready for downstream consumption with zero rework: BaselineCapture (P1.M3.T2.S1), TripDetector/guards (P1.M3.T3.S1/S2), fill engine (P1.M6.T1.S2).

## User Persona

**Target User**: The implementing agents of the downstream sensor chain — P1.M3.T1.S2 (ADC wiring feeds `addSample`), P1.M3.T2.S1 (BaselineCapture medians emitted pairs via `medianOf`), P1.M3.T3.S1/S2 (trip detectors consume the pair stream), P1.M6.T1.S2 (fill engine drives the due/take protocol each loop pass).

**Use Case**: Every loop pass, the wiring layer pushes raw 12-bit counts for both sensors into the aggregator. At a 10 Hz cadence the aggregator emits one `(avg1, avg2)` pair; that pair is the sole currency of every downstream rule: baseline plausibility (§7.1), trip detection (§7.3, 8 consecutive readings), early-wet/false-trip guards (§7.6).

**User Journey**: fill starts → wiring streams samples (burst or trickle — irrelevant to the aggregator) → every 100 ms a reading pair emits → baseline capture medians 2s of pairs → valve opens → trip detectors watch the same pair stream → first sensor drops ≥350 counts for 8 consecutive readings → valve closes.

**Pain Points Addressed**: raw ADC counts are noisy (32-sample average kills sample noise); a single wet splash during baseline settle must not poison the baseline (median, not mean); readings must exist BEFORE 32 samples collect (pre-fill — no special "warmup mode" branches downstream); reading cadence must stay honest after loop stalls (re-anchor, no catch-up bursts — so DEBOUNCE_COUNT always means ~800 ms of real time).

## Why

- PRD §7/§8: the whole fill lifecycle's correctness rests on "averaged readings" — §7.1 (baseline = median of averaged readings), §7.3 (trip = averaged reading ≤ baseline − 350 for 8 consecutive readings), §7.6 (early-wet window on readings). §8 fixes the shape: `ADC_SAMPLES_PER_READING = 32`, `READING_INTERVAL_MS = 100`.
- Architecture (firmware_architecture.md §1): this task's exact slot — "ReadingAggregator + medianOf (P1.M3.T1.S1)" in the UNGUARDED region; loop order `→ sample ADCs → aggregator → FillEngine.step`. Rules of the seam: pure logic takes `uint32_t nowMs` as a parameter, never calls `millis()`/`analogRead`/`Serial`.
- Item contract (task breakdown): input = time utilities from P1.M1.T2.S1 (landed: `elapsedMs`) + config constants from P1.M1.T1.S1 (landed: `ADC_SAMPLES_PER_READING`, `READING_INTERVAL_MS`); output = averaged-reading stream consumed by baseline, trip detectors, fill engine; docs = none (internal).
- TDD-by-SOW: architecture §5 mandates native Unity proofs for every §7 rule this class underpins.

## What

Pure logic only, in the UNGUARDED region:

1. **`class ReadingAggregator`** with public surface EXACTLY:
   - `void addSample(uint8_t sensor, uint16_t counts, uint32_t nowMs)` — appends one raw count to `sensor`'s boxcar window. Sensor domain {0, 1}: 0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2. Out-of-domain indices (≥ `SENSOR_COUNT`) are silently dropped. `nowMs` is accepted but intentionally unused (seam uniformity — sample timing carries no meaning; see Gotchas).
   - `bool readingDue(uint32_t nowMs) const` — true iff `elapsedMs(nowMs, lastReadingMs_) >= READING_INTERVAL_MS` (inclusive at exactly 100 ms; wrap-safe by construction).
   - `void takeReading(uint32_t nowMs, uint16_t& outAvg1, uint16_t& outAvg2)` — TOTAL function: writes the current window averages (truncating division by `ADC_SAMPLES_PER_READING`), re-anchors the cadence at `nowMs`. Intended protocol: `if (readingDue(nowMs)) takeReading(nowMs, a1, a2);` once per loop pass.
   - `static constexpr uint8_t SENSOR_COUNT = 2;`.
2. **Averaging semantics**: 32-slot ring per sensor + running `uint32_t` sum (`sum += new − evicted`, exact under mod-2^32 arithmetic). First sample per sensor PRE-FILLS the whole window (`sum = 32 × first`), so every reading is a valid 32-sample average from the first emission onward. A sensor never sampled averages 0 (consumers' BASELINE_MIN..MAX gate treats 0 as out-of-range).
3. **Cadence semantics**: one pair per 100 ms anchored at the last EMISSION instant; after a stall exactly ONE reading emits (no catch-up burst); anchor initializes to 0 so the first reading is due one full interval after construction.
4. **`uint16_t medianOf(uint16_t* values, size_t n)`** free function: sorts `values` ASCENDING IN PLACE (insertion sort), then `n==0 → 0`; odd `n → values[n/2]`; even `n → (values[n/2−1] + values[n/2]) / 2` truncated, summed in `uint32_t` (uint16_t sum overflows at 65535+65534). Prototype added to the explicit-prototype block.

NOT in scope (owners): ADC hardware reads + commissioning log (P1.M3.T1.S2); baseline settle/plausibility (P1.M3.T2.S1); trip/guard logic (P1.M3.T3.S1/S2); driving the protocol from `loop()` (P1.M6.T2.S1); any config.h addition (ZERO new constants — everything comes from §8); docs (item spec: none — internal).

### Success Criteria

- [ ] `class ReadingAggregator` present exactly once in the UNGUARDED region, after the ButtonClassifier end-banner, before the ARDUINO region marker; banner comments per the verbatim listing
- [ ] Public surface is EXACTLY `addSample` / `readingDue` (const) / `takeReading` / `SENSOR_COUNT` — no extra public members
- [ ] `medianOf` defined once in the UNGUARDED region with its prototype once in the prototype block; sorts in place; even-n mean via uint32 accumulator
- [ ] Zero occurrences of `analogRead`, `millis(`, `pinMode`, `digitalWrite`, `Serial` in non-comment lines of the UNGUARDED region
- [ ] `test/test_reading/test_reading.cpp` exists with explicit `int main()` Unity runner; all listed cases present with the exact expected values
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` + new test dir only)
- [ ] config.h, platformio.ini, all other test files, ARDUINO region byte-stable

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: complete verbatim listings for every edit (prototype line, class+function block, full test file) — all machine-compiled and executed against the REAL current `.ino` on this host with `-Wall -Wextra -Werror` (zero warnings, all asserts pass); exact insertion anchors quoted from the file read this session; frozen upstream contracts (`elapsedMs` semantics, seam rules, Unity runner convention) verified in-file; validation commands executed against the current tree this session. Parallel-work risk (P1.M2.T2.S1 editing the ARDUINO region concurrently) is neutralized by region disjointness + a Task 0 gate.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified-current landmarks: (1) prototype block starts
        after the three #includes with `uint32_t elapsedMs(...)` — your medianOf
        prototype goes after the `size_t formatBootBanner(...);` line; (2) UNGUARDED
        region ends with the banner line `// ========= end BOOT-button classifier
        (consumed by P1.M2.T2.S1, P1.M6.T1.S1) =========` — insert your block AFTER
        it, one blank line each side, BEFORE `// ---- #ifdef ARDUINO REGION:
        hardware glue ----`; (3) elapsedMs/ HALF_WHEEL_MS semantics live at the top
        of the region — compose elapsedMs, never subtract-and-compare stamps.
  gotcha: ButtonClassifier is ALREADY LANDED (verified by read this session). If
        P1.M2.T2.S1's ARDUINO-region blocks are also present, that's expected —
        they live below the region marker and are NOT yours to touch.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of every constant you consume: ADC_SAMPLES_PER_READING
        (32, uint16_t constexpr), READING_INTERVAL_MS (100, uint32_t constexpr).
        Do NOT add constants — §8 is complete; SENSOR_COUNT belongs to the class
        (structural, not a tunable).
  gotcha: never hardcode 32 or 100 — always the config names (P1.M9 commissioning
        may retune ADC values).

- file: test/test_button/test_button.cpp
  why: THE test pattern to follow verbatim-in-style: #include the .ino via relative
        path, empty setUp/tearDown, hand-derived expected values, explicit
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (REQUIRED on PIO 6.2.0 — no auto-generated Unity runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity project.

- file: plan/001_53984f832581/P1M3T1S1/research/notes.md
  why: this task's research: design-decision rationale (§2), the FULL
        machine-verified expected-value table (§3), external references (§4),
        downstream API freeze (§5).
  section: §3 (expected values) and §2 (binding design decisions)

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: §1 "The one architectural decision" (seam rules + component slot list)
  why: your block is the named slot "ReadingAggregator + medianOf (P1.M3.T1.S1)";
        loop-order line shows who feeds/calls you (sample ADCs → aggregator →
        FillEngine.step). Rules: pure logic takes nowMs as a parameter, never
        calls hardware; explicit prototypes for every free function.

- url: https://docs.arduino.cc/built-in-examples/analog/Smoothing/
  why: the canonical boxcar/running-sum moving-average pattern this class
        implements (readings array + running total + circular index); our pre-fill
        of the first sample is the standard extension that avoids a warmup mode.

- url: https://www.luisllamas.es/en/arduino-moving-average-filter/
  why: fast moving-average filter implementation notes (add-new-subtract-evicted
        running sum — why it's O(1) per sample and exact).

- url: https://www.megunolink.com/articles/coding/3-methods-filter-noisy-arduino-measurements
  why: average vs median trade-offs — why the PRD's baseline uses the MEDIAN of
        averaged readings (breakdown point 50%: a single wet splash during settle
        cannot move it) while sample noise is handled by the mean.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] unity, src-filter
├── firmware/jug_filler/
│   ├── jug_filler.ino        # seam + time helpers + banner + ButtonClassifier
│   │                         #   (+ P1.M2.T2.S1's ARDUINO blocks, landing in parallel)
│   │                         # ← EDIT (prototype block + UNGUARDED region only)
│   └── config.h              # every §8 tunable — READ-ONLY
├── test/
│   ├── test_seam/            # do not touch
│   ├── test_time_utils/      # do not touch
│   ├── test_log_utils/       # do not touch
│   └── test_button/          # do not touch (P1.M2.T1.S1's)
│   └── test_reading/         # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): medianOf prototype; ReadingAggregator
│                             #   class + medianOf free function in the UNGUARDED region
└── test/
    └── test_reading/
        └── test_reading.cpp  # NEW: native Unity proof of averaging, cadence (incl.
                              #   wrap + no-burst), warmup, OOR defense, median contract
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: NEVER subtract and compare raw timestamps ("now - last >= X" is
//   FORBIDDEN as hand-rolled code — ordering across the 49.7-day wrap is
//   meaningless). ALWAYS compose elapsedMs(now, since) >= CONST (house rule,
//   P1.M1.T2.S1). Mod-2^32 subtraction inside elapsedMs IS the safety.

// CRITICAL: the running-sum update sum += (uint32_t)new - evicted LOOKS wrong when
//   new < evicted (the difference wraps huge) — it is EXACT under mod-2^32
//   arithmetic because the true window sum always stays ≤ 32*65535 < 2^21. Do not
//   "fix" it with branches; add the explanatory comment from the listing.

// CRITICAL: medianOf's even-n mean MUST accumulate in uint32_t:
//   65535 + 65534 = 131069 overflows uint16_t (wraps to 533 → garbage median).
//   Pinned by test_median_max_pair_no_uint16_overflow.

// CRITICAL: medianOf SORTS IN PLACE — the caller's buffer is mutated. This is the
//   documented contract (BaselineCapture will pass its own scratch buffer). Say it
//   in the comment; test_median_sorts_in_place_ascending pins it.

// CRITICAL: addSample's nowMs is INTENTIONALLY unused ((void)nowMs) — samples may
//   burst or trickle; only reading cadence uses the clock. The parameter exists
//   for seam uniformity (every service API takes the injected clock). Do not
//   invent sample-time gating — the item's test list has none.

// CRITICAL: UNGUARDED-region comments must avoid gratuitous hardware tokens
//   ("clock"/"nowMs", never "millis()"/"analogRead") — the purity gate greps
//   tokens, and while the gate below strips // comments, keep new prose clean
//   anyway (the landed time-helpers comments mentioning millis() predate this
//   and are not yours to reword).

// GOTCHA: size_t at the prototype block — <stdio.h> is already included above it
//   and defines size_t (C std guarantee). NO new include; adding <stddef.h> would
//   churn the include block for nothing.

// GOTCHA (parallel work): P1.M2.T2.S1 is simultaneously adding ARDUINO-region
//   blocks (BOOTSEL poll). Regions are disjoint — but if you see merge noise in
//   the ARDUINO region, LEAVE IT ALONE; your diff must not touch anything below
//   the region marker.

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
//   `pio run -e pico` is compile-only validation.
```

## Implementation Blueprint

### Data models and structure

One class + one free function, both in the UNGUARDED region (complete verbatim listing in Task 2). State footprint per instance ≈ 150 B:

```cpp
uint16_t window_[2][ADC_SAMPLES_PER_READING];  // 128 B boxcar slots
uint32_t sum_[2];                              // running window sums (≤ 32*65535)
uint8_t  nextSlot_[2];                         // ring heads
bool     seeded_[2];                           // first-sample pre-fill done?
uint32_t lastReadingMs_;                       // cadence anchor (last emission)
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first)
  - RUN: grep -c "class ButtonClassifier" firmware/jug_filler/jug_filler.ino    # EXPECT: 1
  - RUN: grep -c "end BOOT-button classifier" firmware/jug_filler/jug_filler.ino # EXPECT: 1 (your insert anchor)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -c "uint32_t elapsedMs" firmware/jug_filler/jug_filler.ino        # EXPECT: >= 2 (proto + def)
  - Any expectation misses → STOP and report (seam damaged or parallel work moved
    anchors); do NOT improvise new anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — prototype block
  - EDIT: insert ONE line after `size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused);`:
      uint16_t medianOf(uint16_t* values, size_t n);   // median of n counts, sorts in place (P1.M3.T1.S1)
  - PRESERVE: every other prototype and the include block byte-for-byte.

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — UNGUARDED region block
  - EDIT: insert the "Task 2 listing" verbatim AFTER the line
      `// ========= end BOOT-button classifier (consumed by P1.M2.T2.S1, P1.M6.T1.S1) =========`
    and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with one blank
    line on each side. The listing was compiled with -Wall -Wextra -Werror and all
    its tests executed against the real current file — transcribe, don't redesign.

Task 3: CREATE test/test_reading/test_reading.cpp
  - IMPLEMENT: the "Task 3 listing" verbatim (16 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_reading matches test_button
    /test_time_utils convention. PIO auto-discovers the new project.

Task 4: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 2 listing — insert verbatim:**

```cpp
// ===================== Reading aggregator + median helper (P1.M3.T1.S1) =====================
// Sensor chain stage 1 (PRD §7/§8), pure: turns raw 12-bit counts from two
// sensors into ONE averaged reading pair per READING_INTERVAL_MS (100 ms),
// each average over the last ADC_SAMPLES_PER_READING (32) samples. Downstream:
// BaselineCapture (P1.M3.T2.S1) medians emitted pairs for the dry baseline;
// TripDetector + guards (P1.M3.T3.S1/S2) and the fill engine (P1.M6.T1.S2)
// consume the same stream. The clock is injected and wrap-safe via elapsedMs;
// no hardware is touched.
//
// Sensor index domain: 0 == PIN_SENSOR1 (fault code 1), 1 == PIN_SENSOR2
// (fault code 2); any other index is dropped. Samples may burst or trickle —
// the window is time-agnostic; only the reading CADENCE uses the clock.
//
// Warmup: the first sample a sensor ever sees PRE-FILLS its whole 32-slot
// window, so every emitted reading is a valid 32-sample average from the
// first reading onward (weight 31/32 on sample #1, decaying as real samples
// displace it). A sensor never sampled averages 0 — consumers gate on
// BASELINE_MIN..MAX and treat 0 as out-of-range.
//
// Cadence: one pair per READING_INTERVAL_MS anchored at the last EMISSION
// instant; after a >100 ms stall exactly ONE reading emits (no catch-up
// burst), so DEBOUNCE_COUNT consecutive readings always span ~800 ms of real
// time. Anchor starts at 0: first reading due one full interval after
// construction. Protocol (driven once per loop pass by P1.M6.T2.S1):
//   if (agg.readingDue(nowMs)) { agg.takeReading(nowMs, a1, a2); ... }
class ReadingAggregator {
public:
    static constexpr uint8_t SENSOR_COUNT = 2;  // 0 == PIN_SENSOR1, 1 == PIN_SENSOR2

    ReadingAggregator()
        : nextSlot_{0, 0}, seeded_{false, false}, sum_{0, 0}, lastReadingMs_(0) {}

    // One raw 12-bit count for `sensor`. `nowMs` is accepted for seam uniformity
    // (every service takes the injected clock) and deliberately unused: sample
    // arrival timing carries no meaning here, only reading cadence does.
    void addSample(uint8_t sensor, uint16_t counts, uint32_t nowMs) {
        (void)nowMs;
        if (sensor >= SENSOR_COUNT) return;  // out of domain: defensive no-op
        if (!seeded_[sensor]) {
            // First sample ever: pre-fill the whole window (see warmup note).
            for (uint16_t i = 0; i < ADC_SAMPLES_PER_READING; ++i)
                window_[sensor][i] = counts;
            sum_[sensor] = (uint32_t)counts * ADC_SAMPLES_PER_READING;
            seeded_[sensor] = true;
            nextSlot_[sensor] = 0;
            return;
        }
        // Running sum: add the new sample, drop the evicted one. Exact under
        // mod-2^32 arithmetic even when counts < evicted (the true window sum
        // never exceeds 32 * 65535 < 2^21, so the wraps cancel).
        sum_[sensor] += (uint32_t)counts - window_[sensor][nextSlot_[sensor]];
        window_[sensor][nextSlot_[sensor]] = counts;
        if (++nextSlot_[sensor] == ADC_SAMPLES_PER_READING) nextSlot_[sensor] = 0;
    }

    // True iff a reading emission is due at `nowMs`: >= READING_INTERVAL_MS
    // since the last emission (inclusive at exactly 100; wrap-safe by
    // construction — elapsedMs is mod-2^32 subtraction).
    bool readingDue(uint32_t nowMs) const {
        return elapsedMs(nowMs, lastReadingMs_) >= READING_INTERVAL_MS;
    }

    // Emit the current averaged pair (truncating division) and re-anchor the
    // cadence at `nowMs`. Total function — legal to call unconditionally; the
    // intended protocol pairs it with readingDue (see class comment).
    void takeReading(uint32_t nowMs, uint16_t& outAvg1, uint16_t& outAvg2) {
        outAvg1 = (uint16_t)(sum_[0] / ADC_SAMPLES_PER_READING);
        outAvg2 = (uint16_t)(sum_[1] / ADC_SAMPLES_PER_READING);
        lastReadingMs_ = nowMs;  // re-anchor at the emission instant — no bursts
    }

private:
    uint16_t window_[SENSOR_COUNT][ADC_SAMPLES_PER_READING];  // boxcar slots (128 B)
    uint8_t  nextSlot_[SENSOR_COUNT];                         // slot the next sample overwrites
    bool     seeded_[SENSOR_COUNT];                           // first sample seen (pre-filled)?
    uint32_t sum_[SENSOR_COUNT];                              // running window sums
    uint32_t lastReadingMs_;                                  // cadence anchor (last emission)
};

// Median of n counts — PRD §7.1: dry baselines are the MEDIAN of averaged
// readings (breakdown point 50%: one wet splash cannot move it; the mean
// would follow any outlier). Free function: BaselineCapture (P1.M3.T2.S1)
// owns the buffer. CONTRACT: sorts `values` ASCENDING IN PLACE (insertion
// sort; n is tens at most) — pass a scratch/mutable buffer.
//   n == 0 -> 0 (degenerate; callers pass >= 1)   odd n -> values[n/2]
//   even n -> (values[n/2-1] + values[n/2]) / 2, truncated — summed in
//   uint32_t: 65535 + 65534 = 131069 OVERFLOWS uint16_t (wraps to 533).
uint16_t medianOf(uint16_t* values, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 1; i < n; ++i) {  // insertion sort, ascending
        const uint16_t key = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }
    const size_t mid = n / 2;
    if (n % 2 == 1) return values[mid];
    return (uint16_t)(((uint32_t)values[mid - 1] + (uint32_t)values[mid]) / 2u);
}
// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========
```

**Task 3 listing — `test/test_reading/test_reading.cpp` (verbatim):**

```cpp
// test/test_reading/test_reading.cpp — native contract proof for
// ReadingAggregator + medianOf (P1.M3.T1.S1). Pins PRD §7/§8: one averaged
// pair per READING_INTERVAL_MS (100 ms), each the truncated mean of the last
// ADC_SAMPLES_PER_READING (32) samples (first sample pre-fills the window);
// cadence re-anchored at emission (no catch-up burst), wrap-safe; median
// odd/even/truncation/in-place/uint32-overflow.
// Expected values hand-derived AND machine-verified
// (plan/001_53984f832581/P1M3T1S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- averaging ----

// ITEM CASE: first sample pre-fills the whole window -> reading == sample.
void test_first_sample_prefills_window() {
    ReadingAggregator agg;
    agg.addSample(0, 3000, 0);
    agg.addSample(1, 2500, 0);
    uint16_t a1 = 999, a2 = 999;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(3000, a1);
    TEST_ASSERT_EQUAL_UINT16(2500, a2);
}

// ITEM CASE: exact averaging over a full window — 32 samples 1000..1031,
// sum 32496, 32496/32 = 1015.5 -> 1015 (truncation pinned).
void test_exact_average_full_window() {
    ReadingAggregator agg;
    for (uint16_t v = 1000; v <= 1031; ++v) agg.addSample(0, v, 0);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(1015, a1);
    TEST_ASSERT_EQUAL_UINT16(0, a2);   // sensor 1 never sampled -> 0
}

// Window slides: after 32x1000, one 1032 evicts a 1000 ->
// (32000 - 1000 + 1032)/32 = 1001.
void test_window_slides_oldest_evicted() {
    ReadingAggregator agg;
    for (int i = 0; i < 32; ++i) agg.addSample(0, 1000, 0);
    agg.addSample(0, 1032, 1);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(1001, a1);
}

// ITEM CASE: under-fill warmup — two samples: (64000 - 2000 + 3000)/32 = 2031.
void test_warmup_two_samples() {
    ReadingAggregator agg;
    agg.addSample(0, 2000, 0);
    agg.addSample(0, 3000, 1);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(2031, a1);
}

// Out-of-domain sensor indices are dropped; sensors are independent.
void test_out_of_domain_sensor_ignored() {
    ReadingAggregator agg;
    agg.addSample(0, 3000, 0);
    agg.addSample(2, 5000, 1);
    agg.addSample(255, 4095, 2);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(3000, a1);
    TEST_ASSERT_EQUAL_UINT16(0, a2);
}

// ---- cadence (injected clock; inclusive at exactly 100; wrap-safe) ----

// ITEM CASE: due at exactly 100 (inclusive); overdue stays due.
void test_due_exactly_at_interval_inclusive() {
    ReadingAggregator agg;
    TEST_ASSERT_FALSE(agg.readingDue(99));
    TEST_ASSERT_TRUE(agg.readingDue(100));
    TEST_ASSERT_TRUE(agg.readingDue(100000));
}

// Emission re-anchors the cadence at the emission instant.
void test_take_reading_reanchors_cadence() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    TEST_ASSERT_TRUE(agg.readingDue(100));
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(199));
    TEST_ASSERT_TRUE(agg.readingDue(200));
}

// A long stall emits exactly ONE reading (no catch-up burst) — DEBOUNCE_COUNT
// always means ~800 ms of real time.
void test_no_catchup_burst_after_stall() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_TRUE(agg.readingDue(5000));
    agg.takeReading(5000, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(5099));
    TEST_ASSERT_TRUE(agg.readingDue(5100));
}

// ITEM CASE: cadence across the 49.7-day wrap — anchor at 0xFFFFFF9C is
// exactly 100 ms below the wrap: not due at 0xFFFFFFFF (99 elapsed), due at
// 0x00000000 (exactly 100, wrapped).
void test_cadence_survives_49day_wrap() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    agg.addSample(0, 3000, 0);
    agg.takeReading(0xFFFFFF9CUL, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(0xFFFFFFFFUL));
    TEST_ASSERT_TRUE(agg.readingDue(0x00000000UL));
    agg.takeReading(0, a1, a2);
    TEST_ASSERT_TRUE(agg.readingDue(100));
}

// ---- medianOf ----

void test_median_odd_takes_middle() {
    uint16_t v[] = {3500, 2000, 3000};
    TEST_ASSERT_EQUAL_UINT16(3000, medianOf(v, 3));
}

void test_median_even_means_middle_pair() {
    uint16_t v[] = {10, 20, 30, 40};
    TEST_ASSERT_EQUAL_UINT16(25, medianOf(v, 4));
}

void test_median_even_truncates() {
    uint16_t v[] = {1, 2};
    TEST_ASSERT_EQUAL_UINT16(1, medianOf(v, 2));  // 1.5 -> 1
}

// ITEM CASE: single-outlier robustness — one wild 4095 cannot move the median.
void test_median_shrugs_off_single_outlier() {
    uint16_t v[] = {3000, 3010, 4095, 3005, 3008};
    TEST_ASSERT_EQUAL_UINT16(3008, medianOf(v, 5));
}

// CONTRACT: the buffer is sorted ascending in place (BaselineCapture passes
// scratch memory and relies on this being the only mutation).
void test_median_sorts_in_place_ascending() {
    uint16_t v[] = {3, 1, 2};
    medianOf(v, 3);
    TEST_ASSERT_EQUAL_UINT16(1, v[0]);
    TEST_ASSERT_EQUAL_UINT16(2, v[1]);
    TEST_ASSERT_EQUAL_UINT16(3, v[2]);
}

// Overflow trap: 65535 + 65534 = 131069 needs the uint32 accumulator
// (a uint16 sum wraps to 533 and yields 32766 — wrong).
void test_median_max_pair_no_uint16_overflow() {
    uint16_t v[] = {65535, 65534};
    TEST_ASSERT_EQUAL_UINT16(65534, medianOf(v, 2));
}

void test_median_edge_sizes() {
    uint16_t v[] = {7};
    TEST_ASSERT_EQUAL_UINT16(0, medianOf(v, 0));
    TEST_ASSERT_EQUAL_UINT16(7, medianOf(v, 1));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_prefills_window);
    RUN_TEST(test_exact_average_full_window);
    RUN_TEST(test_window_slides_oldest_evicted);
    RUN_TEST(test_warmup_two_samples);
    RUN_TEST(test_out_of_domain_sensor_ignored);
    RUN_TEST(test_due_exactly_at_interval_inclusive);
    RUN_TEST(test_take_reading_reanchors_cadence);
    RUN_TEST(test_no_catchup_burst_after_stall);
    RUN_TEST(test_cadence_survives_49day_wrap);
    RUN_TEST(test_median_odd_takes_middle);
    RUN_TEST(test_median_even_means_middle_pair);
    RUN_TEST(test_median_even_truncates);
    RUN_TEST(test_median_shrugs_off_single_outlier);
    RUN_TEST(test_median_sorts_in_place_ascending);
    RUN_TEST(test_median_max_pair_no_uint16_overflow);
    RUN_TEST(test_median_edge_sizes);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: pure-service class shape (follows ButtonClassifier exactly):
//   - all timing via injected nowMs, composed with elapsedMs (never raw stamps)
//   - total methods with documented protocol; const queries
//   - zero dynamic memory, fixed-size state, explicit constructor init-list

// PATTERN: reading protocol for the loop owner (P1.M6.T2.S1, documentation only):
//   if (agg.readingDue(nowMs)) {
//       uint16_t a1, a2;
//       agg.takeReading(nowMs, a1, a2);
//       // route the pair: baseline capture during settle, trip detectors while filling
//   }

// CRITICAL: division by ADC_SAMPLES_PER_READING truncates (1015.5 -> 1015) —
//   pinned by test_exact_average_full_window; do not add rounding.

// CRITICAL: readingDue is const and takes no side effects; calling it repeatedly
//   (e.g., every loop pass) is free and never advances the cadence — only
//   takeReading does.

// GOTCHA: constructing a fresh ReadingAggregator mid-run (e.g., per fill) is
//   legal: pre-fill makes readings valid immediately; the cadence restarts with
//   the first reading one interval after construction.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 prototype line, +1
    bannered block in the UNGUARDED region. ARDUINO region untouched (parallel
    P1.M2.T2.S1 owns it); no setup()/loop() changes — wiring comes in P1.M3.T1.S2
    / P1.M6.T2.S1.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes ADC_SAMPLES_PER_READING and
    READING_INTERVAL_MS from config.h §5 group. SENSOR_COUNT is a class-static
    (structural, not a §8 tunable).
TEST:
  - New project dir test/test_reading/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; commissioning log format belongs to P1.M3.T1.S2,
    README sync to P2.M1.T3.S1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - ADC pin reads + pacing + raw-count commissioning log  -> P1.M3.T1.S2
  - 2s settle, baseline buffer + plausibility gate        -> P1.M3.T2.S1
  - trip rule (baseline-350, 8 consecutive) + guards      -> P1.M3.T3.S1/S2
  - driving readingDue/takeReading from loop()            -> P1.M6.T2.S1
  - per-state routing of the pair stream                  -> P1.M6.T1.S2
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the class + medianOf compile under arm-gcc/arduino-pico
# inside the sketch build (they sit in the UNGUARDED region, included in both
# builds). Compile errors here mean a transcription deviation from the listing.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; projects test_seam, test_time_utils, test_log_utils,
# test_button AND the new test_reading all [PASSED] (16/16 tests).
# ANY failure: read the mismatch — the expected values are machine-verified
# against the real file, so a failure means the implementation (not the test)
# deviates. Never "fix" a test's expected value to make it pass.
```

### Level 3: Integration Testing (System Validation)

```bash
# Scope check — only this task's artifacts (plus parallel tasks' own files):
git status --short
# Expected (attributed to THIS task):
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_reading/
# (P1.M2.T2.S1's .ino changes share the M line — attribute by region, don't
#  touch its blocks.)

# Structural placement: block sits between the classifier end-banner and the
# ARDUINO marker; prototype present exactly once:
grep -n "end reading aggregator (consumed by P1.M3.T2.S1" firmware/jug_filler/jug_filler.ino   # 1 hit
grep -c "uint16_t medianOf(uint16_t\* values, size_t n);" firmware/jug_filler/jug_filler.ino    # 1 hit (prototype)
grep -c "class ReadingAggregator" firmware/jug_filler/jug_filler.ino                            # 1 hit
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region — strip // comments first (the landed
#     time-helpers comments legitimately mention the clock API by name; only
#     CODE must be hardware-free). Validated against the current tree:
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" \
  || echo "PURE OK (comments stripped)"

# (2) Region markers intact (note: -E -- required; a bare pattern starting
#     with dashes is parsed as an option and the grep fails):
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) No config/platformio churn:
git diff --stat firmware/jug_filler/config.h platformio.ini   # Expected: empty

# (4) Public surface freeze (no extra public members sneaked in):
sed -n '/class ReadingAggregator/,/^};/p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "^\s*(void|bool|uint16_t|static constexpr)"   # exactly the 4 contract members

# (5) Optional fast host re-verification of the exact listing semantics
#     (mirrors the research machine check — not a substitute for pio test):
#     skip; Level 2 already proves it through Unity.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]`
- [ ] "PURE OK (comments stripped)"; marker count 2; class exactly once; `medianOf` prototype exactly once
- [ ] `git status --short` shows only the `.ino` (UNGUARDED + prototype edits) and `test/test_reading/` from this task

### Feature Validation

- [ ] Averaging: pre-fill, exact window mean with pinned truncation (1015), slide (1001), warmup (2031), sensor independence, OOR-drop — all `[PASSED]`
- [ ] Cadence: inclusive at 100, emission re-anchor, no catch-up burst after stall, wrap at 0xFFFFFF9C→0x00000000 — all `[PASSED]`
- [ ] Median: odd, even, even-truncation, single-outlier robustness, in-place sort contract, uint32-overflow trap, n=0/n=1 edges — all `[PASSED]`
- [ ] Public surface exactly `addSample` / `readingDue` (const) / `takeReading` / `SENSOR_COUNT`; `medianOf` sorts in place

### Code Quality Validation

- [ ] Seam rules honored: UNGUARDED region additions are pure (injected clock only, composed via `elapsedMs`); no ARDUINO-region edits
- [ ] Zero new config constants; `ADC_SAMPLES_PER_READING` / `READING_INTERVAL_MS` referenced by name, never as 32/100 literals
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2)
- [ ] The `(void)nowMs` seam-uniformity rationale and the in-place-sort contract are documented in-code (they are — keep them)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't hand-roll `now - last >= 100` timestamp math — compose `elapsedMs()` (wrap safety is the entire point of P1.M1.T2.S1)
- ❌ Don't "fix" the running-sum `+=` when the new sample is smaller than the evicted one — mod-2^32 arithmetic is exact here; the comment explains why
- ❌ Don't accumulate the even-n median pair in `uint16_t` — 65535+65534 overflows; the `uint32_t` accumulator is a pinned test
- ❌ Don't add rounding to the average, catch-up readings after stalls, or sample-time gating in `addSample` — truncation, single-emission re-anchor, and time-agnostic windows are the frozen contract
- ❌ Don't add public members beyond the frozen four (e.g., `getAverage()`, `isSeeded()`, `sampleCount()`) — downstream PRPs are written against this exact surface
- ❌ Don't add `SENSOR_COUNT` or any sensor constant to config.h — §8 is complete; structural constants live with their class
- ❌ Don't copy values into `medianOf` (making it non-mutating) or switch to `std::sort`/`std::nth_element` — in-place insertion sort is the zero-dependency, deterministic contract
- ❌ Don't touch the ARDUINO region, the include block, other test files, or config.h — parallel/subsequent tasks own them
- ❌ Don't adjust a test's expected value to make it pass — expected values are machine-verified; failures mean implementation deviation
- ❌ Don't run `pio test -e pico` (no transport) or upload anything — this task is pure logic, native-proven

---

**Confidence Score: 10/10** — the complete listing (class + function + full 16-test suite) was compiled with `-Wall -Wextra -Werror` against the REAL current `.ino` on this machine and every expected value executed green before writing this PRP; insertion anchors quote the file as read this session; upstream contracts (elapsedMs semantics, Unity runner convention, marker/gate commands) were all verified by direct execution; parallel-work risk is confined to the disjoint ARDUINO region and gated by Task 0. Residual risk is limited to transcription fidelity, which the verbatim listings and Level-2 gate directly catch.
