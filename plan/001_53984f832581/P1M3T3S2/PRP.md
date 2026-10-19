# PRP — P1.M3.T3.S2: False-trip guards — TRIP_MIN_ELAPSED_S and EARLY_WET_WINDOW_S (pure)

## Goal

**Feature Goal**: Land the sensor chain's final pure-logic block in the UNGUARDED region of the sketch: a `FalseTripGuards` class + `GuardVerdict` struct implementing PRD §7.6 exactly — any trip ACCEPTED before `TRIP_MIN_ELAPSED_S` = 60 s of fill time, or any sensor reading WET during the first `EARLY_WET_WINDOW_S` = 5 s, is a false trip / short → the verdict names the offending sensor for the fault path. "Fill time" = time since `valveOpenedAtMs`, which the fill engine (P1.M6.T1.S2) supplies AS A PARAMETER each call — this class never tracks when the valve opened. "Wet" = averaged reading at/below baseline − `TRIP_DROP_COUNTS` (350) — the SAME inclusive predicate as the trip rule (PRD §7.3). `evaluate()` is a STATELESS pure function of its inputs; the engine owns verdict consumption and fault latching. Zero hardware calls, zero new config constants (both windows already live in config.h "Windows" group), zero ARDUINO-region changes.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: one bannered block (`struct GuardVerdict` + `class FalseTripGuards`) inserted into the UNGUARDED region, immediately AFTER the `// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========` banner (landed by P1.M3.T3.S1, verified present at line 458 of the current file) and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`. NO prototype-block changes (classes/structs get none), NO include changes, NO config.h changes, NO ARDUINO-region edits.
- `test/test_guards/test_guards.cpp` — NEW: 21 Unity native tests proving: trip at 59.9 s → EarlyTripFault, at exactly 60.0 s → Ok (window closes AT equality — strict `<`); wet reading at 4.9 s → EarlyWetFault, at exactly 5.0 s and at 5.1 s → Ok; both sensors clean → Ok; wet predicate inclusive at exactly baseline−350, dry at baseline−349; wrap-safe timestamps across the 2^32 ms rollover for BOTH windows; verdict sensor payload 0/1 ↔ fault codes 1/2; deterministic precedence (wet before trip, sensor 1 before sensor 2); legit trip after 60 s passes; per-fill re-arm via `begin()`; composition with a real `TripDetector` (8 wet feeds latch the detector, guards see both rules). All expected values hand-derived AND machine-verified this session against the REAL current `.ino` (`g++ -std=c++17 -Wall -Wextra -Werror`, **21 tests / 0 failures** — see research/notes.md §3).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_guards` project, plus regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline, test_trip).
- `pio run -e pico` → compile-only `[SUCCESS]` — the block compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_guards/` added by this task.
- The frozen public surface (`FalseTripGuards()` / `begin` / `evaluate` const, `GuardVerdict{kind, sensor}`) is ready for downstream consumption with zero rework: the fill engine's fault paths (P1.M6.T1.S2, P1.M6.T1.S3).

## User Persona

**Target User**: The implementing agents of the downstream chain — P1.M6.T1.S2 (fill engine: begins guards at fill start with the validated baselines, evaluates every loop pass while FILLING, routes non-Ok verdicts into the fault path), P1.M6.T1.S3 (fault latching: `verdict.sensor + 1` == fault blink code 1/2), P1.M6.T2.S1 (integration loop: supplies nowMs, valveOpenedAtMs, the detectors' `tripped()` flags, and the latest averaged pair).

**Use Case**: The valve has just opened and the fill timer is running. A splash-wet sensor tip, an electrical short, or a jug that was already full makes a sensor read wet or trip LONG before water could physically reach it. The guards classify that instant as a false trip: `EarlyWetFault(sensor)` while the reading is wet inside the first 5 s, or `EarlyTripFault(sensor)` while the detector's latch is set before 60 s. The engine sees the non-Ok verdict, drives the valve LOW first, and latches the sensor's fault (PRD §7.6 + fault table). Meanwhile a REAL fill — water arriving at 3–20+ minutes — passes both guards untouched.

**User Journey**: valve opens → every loop pass the engine evaluates the guards with current inputs → (short: wet reading at 2 s → EarlyWetFault → engine closes valve, latches fault 1/2, blinks) | (legit fill: guards stay Ok past 60 s; first trip at minute N is trusted → CONFIRM).

**Pain Points Addressed**: a shorted or pre-wet sensor must never be trusted as a trip source (it would close the valve instantly and fault the WRONG thing — or "complete" a fill that delivered nothing); the classification must be deterministic and wrap-safe (a 49.7-day uptime rollover mid-fill must not misclassify); the predicate for "wet" must be byte-identical to the trip rule's predicate so commissioning retunes (§9) move both together.

## Why

- PRD §7.6 (normative): "Any trip accepted before `TRIP_MIN_ELAPSED_S` (60s) of fill time, or any sensor reading wet during the first `EARLY_WET_WINDOW_S` (5s), is a false trip / short → fault that sensor." This block is the pure core of that rule.
- PRD §8 (frozen contract): `EARLY_WET_WINDOW_S = 5` and `TRIP_MIN_ELAPSED_S = 60` already live in config.h "Windows" group — consumed BY NAME, never hardcoded.
- Architecture (`firmware_architecture.md` §1): this task's exact named slot — `TripDetector + guard evaluation (P1.M3.T3.S1/S2)` directly after `BaselineCapture (P1.M3.T2.S1)` in the UNGUDED region. Seam rules: pure logic only, no hardware calls.
- Item contract (task breakdown): input = `TripDetector` from P1.M3.T3.S1, averaged readings, time utilities P1.M1.T2.S1; valve-open timestamp supplied by the fill engine as a parameter (do NOT track here); output = guard verdicts consumed by the fill engine fault paths (P1.M6.T1.S2, P1.M6.T1.S3); docs = none (internal).
- TDD-by-SOW: architecture §5 mandates native Unity proofs for every §7 rule this block underpins.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; downstream PRPs are written against it):

1. **`struct GuardVerdict`** — `{ Kind kind; uint8_t sensor; }` where `Kind : uint8_t` ∈ `{ Ok = 0, EarlyTripFault = 1, EarlyWetFault = 2 }`. `sensor` 0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2; 0 when kind == Ok. Plain aggregate (no user ctor) — tests assert `kind` and `sensor` fields.
2. **`FalseTripGuards()`** — default constructor: both wet thresholds 0. Fresh-instance semantics: only a 0-count reading reads wet; the engine protocol always calls `begin()` first.
3. **`void begin(uint16_t baseline1, uint16_t baseline2)`** — arm for a new fill: computes `wetThresholdN_ = baselineN − TRIP_DROP_COUNTS` once (byte-identical to `TripDetector::begin`'s threshold math — SAME wet predicate as the trip rule). MUST receive VALIDATED baselines (`BaselineCapture::verdict(s) == Ok`, i.e. 2000..4050 — always > 350, no unsigned wrap). An unvalidated baseline < 350 wraps the threshold to a huge value → every reading reads wet → EarlyWet fault on first evaluation — the fail-safe direction (fault = valve stays closed / driven LOW first). NO clamping on purpose.
4. **`GuardVerdict evaluate(uint32_t nowMs, uint32_t valveOpenedAtMs, bool tripped1, bool tripped2, uint16_t avg1, uint16_t avg2) const`** — the ONLY judgment call. STATELESS (pure function of its inputs; `const`). Fill time = `elapsedMs(nowMs, valveOpenedAtMs)` — rollover-safe BY NAME, never raw subtraction or timestamp comparison (time-helpers seam rule). Window semantics — BOTH windows close AT equality (strict `<`):
   - Early-wet: if `fillTimeMs < EARLY_WET_WINDOW_S*1000` and `avgN <= wetThresholdN_` → `EarlyWetFault` (sensor 0 checked before sensor 1).
   - Early-trip: if `fillTimeMs < TRIP_MIN_ELAPSED_S*1000` and `trippedN` → `EarlyTripFault` (sensor 0 before sensor 1).
   - Precedence when several hold at once: early-wet beats early-trip (a short is the more specific diagnosis; wetness necessarily preceded the trip since the debounce takes ≥ 800 ms), sensor 1 (index 0) before sensor 2. Fault outcome is identical either way — precedence stabilizes diagnostics and tests.
   - Units GOTCHA baked in: config windows are SECONDS; the comparison widens BEFORE multiplying: `(uint32_t)EARLY_WET_WINDOW_S * 1000u`.
   - Else `Ok`.
5. The booleans `tripped1/tripped2` are `TripDetector::tripped()` values (the detector's entire observable state — S1-frozen surface); `avg1/avg2` are the latest `ReadingAggregator` emissions. The class does NOT reference `TripDetector` or `ReadingAggregator` types — fully decoupled, takes plain values.

NOT in scope (owners): tracking the valve-open instant or any fill state (P1.M6.T1.S2 — supplied as parameter); consuming verdicts / latching faults / blink codes (P1.M6.T1.S2, P1.M6.T1.S3); loop wiring (P1.M6.T2.S1); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `struct GuardVerdict` + `class FalseTripGuards` present exactly once in the UNGUARDED region, after the trip-detector end-banner, before the ARDUINO region marker; banner comments per the verbatim listing
- [ ] Public surface is EXACTLY the members above — no `reset()`, no `threshold()` getters, no extra public members
- [ ] Both windows use strict `<` on `elapsedMs` fill time: trip at exactly 60.000 s → Ok; wet at exactly 5.000 s → Ok (and 59.9 s / 4.9 s → faults)
- [ ] Wet predicate inclusive at exactly baseline−350 (same as the trip rule); baseline−349 reads dry
- [ ] Verdict carries the sensor payload (0/1); wrap-safe across the 2^32 ms rollover for both windows
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region
- [ ] `test/test_guards/test_guards.cpp` exists with explicit `int main()` Unity runner; all 21 listed cases present with the exact expected values
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` UNGUARDED region + new test dir only)
- [ ] config.h, platformio.ini, prototype block, include block, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete block listing and the complete 21-test file are provided verbatim below — both machine-compiled with `-Wall -Wextra -Werror` and executed green (**21 tests / 0 failures**) against the REAL current `.ino` on this host this session (merged-copy method, research/notes.md §3 — the only difference from the shipped test file is the include line, which points at the repo-relative `.ino` path per house convention); the insertion anchor was verified present in the CURRENT file (P1.M3.T3.S1 landed during this research session — see notes §5); the consumed upstream surfaces (`TripDetector::tripped()` frozen by S1's PRP and verified in the landed file; `ReadingAggregator::takeReading` emission type `uint16_t&`; `elapsedMs(now, since)` semantics from the landed time helpers) were read from the landed code, not assumed; validation commands mirror ones proven working in P1.M3.T1–T3.S1's sessions.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (UNGUARDED region only). Verified-current landmarks:
        (1) INSERT ANCHOR — the banner
        `// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========`
        (landed by P1.M3.T3.S1; at line 458 of the current file); insert your block
        AFTER it (one blank line between), BEFORE
        `// ---- #ifdef ARDUINO REGION: hardware glue ----` (line 460; one blank
        line between).
        (2) `class TripDetector` (line 422) — READ ONLY. Your evaluate() takes its
        `tripped()` bools; do not modify or reference the type.
        (3) `elapsedMs(uint32_t now, uint32_t since)` (~line 56) — the ONLY legal
        fill-time computation; mod-2^32 subtraction IS the rollover safety.
  gotcha: if the end-trip banner is MISSING, P1.M3.T3.S1 has not landed — STOP and
        report; do not improvise an anchor.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of the constants you consume BY NAME: EARLY_WET_WINDOW_S
        (5) and TRIP_MIN_ELAPSED_S (60) in the "Windows" group (uint16_t, SECONDS),
        TRIP_DROP_COUNTS (350, uint16_t) in the "ADC / sensors" group. Do NOT add
        anything — §8 is frozen.
  gotcha: never hardcode 5/60/350 — always the config names (§9 commissioning may
        retune them); the seconds→ms conversion happens at the USE site with an
        explicit (uint32_t) widening.

- file: test/test_trip/test_trip.cpp
  why: THE sibling test pattern (landed by P1.M3.T3.S1, same shape you follow):
        relative `.ino` include, empty setUp/tearDown, hand-derived expected values
        with derivation comments, ITEM CASE tags on contract-pinned tests, explicit
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (REQUIRED on PIO 6.2.0 — no auto-generated Unity runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity project,
        auto-discovered — no platformio.ini change needed.

- file: plan/001_53984f832581/P1M3T3S2/research/notes.md
  why: this task's research: §1 contract→codebase type mapping (TripState→bool,
        Reading→uint16_t — no such types exist in the codebase), §2 binding design
        decisions (strict-< window closure, wet-before-trip precedence, stateless
        evaluate, units widening, no-clamp fail-safe), §3 the machine-verification
        record + full expected-value table, §4 the downstream API freeze.
  section: §2 (design decisions) and §3 (verification)

- file: plan/001_53984f832581/P1M3T3S1/PRP.md
  why: CONTRACT for the upstream you consume. TripDetector's frozen surface:
        `begin(uint16_t)`, `feed(uint16_t)`, `tripped() const`. Your evaluate()
        receives `trip1.tripped()` / `trip2.tripped()` as its bool parameters.
  gotcha: do NOT modify TripDetector — frozen; its 14 tests must stay green.

- file: plan/001_53984f832581/P1M3T2S1/PRP.md
  why: CONTRACT for the baselines your begin() receives (via the engine):
        `BaselineCapture::baseline(uint8_t) -> uint16_t`, valid at done(), verdict
        Ok implies BASELINE_MIN..BASELINE_MAX (2000..4050 — always > 350, so the
        threshold subtraction never wraps in-contract).
  gotcha: do NOT modify BaselineCapture — frozen; its tests must stay green.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: §1 (seam rules + your named slot: "TripDetector + guard evaluation
        (P1.M3.T3.S1/S2)")
  why: justifies the UNGUARDED-region placement and the pure/no-hardware rule.

- url: https://docs.platformio.org/en/stable/plus/unit-testing.html
  why: PlatformIO native Unity conventions — test_dir auto-discovery, one project
        per test/ subdir, explicit runner requirement on PIO 6.2.0.
  critical: never `pio test -e pico` (no on-device transport) — native tests only.

- url: https://docs.arduino.cc/language-reference/en/functions/time/millis/
  why: Arduino millis() reference — documents the ~49.7-day (2^32 ms) rollover.
        Your guard MUST classify correctly when valveOpenedAtMs sits near the wrap
        and nowMs has wrapped past 0 — elapsedMs()'s mod-2^32 subtraction handles
        it; both wrap cases are test-pinned.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # seam + time helpers + ButtonClassifier
│   │                         #   + ReadingAggregator/medianOf + BaselineCapture
│   │                         #   + TripDetector (P1.M3.T3.S1 — LANDED; its
│   │                         #     end-banner at ~line 458 = YOUR ANCHOR)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service, ADC
│   │                         #     wiring, setup/loop
│   │                         # ← EDIT (UNGUARDED region only: +1 bannered block)
│   └── config.h              # every §8 tunable — READ-ONLY (EARLY_WET_WINDOW_S /
│                             #   TRIP_MIN_ELAPSED_S / TRIP_DROP_COUNTS by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/         # P1.M3.T1.S1's — untouched, must stay green
│   ├── test_baseline/        # P1.M3.T2.S1's — untouched, must stay green
│   ├── test_trip/            # P1.M3.T3.S1's — untouched, must stay green
│   └── test_guards/          # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): GuardVerdict + FalseTripGuards
│                             #   in the UNGUARDED region — wet-window and
│                             #   min-elapsed checks on wrap-safe fill time
└── test/
    └── test_guards/
        └── test_guards.cpp   # NEW: native Unity proof — window boundaries (59.9/60.0,
                              #   4.9/5.0/5.1), inclusive wet predicate, sensor payload,
                              #   precedence, wrap safety, re-arm, TripDetector composition
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: BOTH windows close AT equality — use strict '<' on fill time. A trip
//   accepted at exactly 60.000 s is LEGAL (Ok) and a wet reading at exactly
//   5.000 s is legal; 59.9 s and 4.9 s fault. The item contract pins 59.9/60.0
//   and 4.9/5.1; the exactly-5.000 case is the symmetric mirror (test-pinned).

// CRITICAL: the config window constants are SECONDS (uint16_t); evaluate works in
//   milliseconds. Widen BEFORE multiplying or the comparison silently truncates:
//     fillTimeMs < (uint32_t)TRIP_MIN_ELAPSED_S * 1000u     // 60000 — correct
//     fillTimeMs < TRIP_MIN_ELAPSED_S * 1000                // 16-bit trap — never

// CRITICAL: fill time is computed ONLY via elapsedMs(nowMs, valveOpenedAtMs) — the
//   time-helpers seam rule forbids raw timestamp subtraction/comparison at use
//   sites. mod-2^32 subtraction IS the rollover safety; both wrap cases are
//   test-pinned (valve near 2^32, now wrapped to small values).

// CRITICAL: the wet predicate must be BYTE-IDENTICAL to the trip rule's:
//   `avgN <= wetThresholdN_` where begin() computed wetThresholdN_ = baselineN -
//   TRIP_DROP_COUNTS exactly like TripDetector::begin(). Inclusive at exactly
//   baseline−350; baseline−349 reads dry (both sides test-pinned, 3050/3051 for
//   baseline 3400).

// CRITICAL: evaluate() takes bool trip flags, NOT TripDetector references — the
//   guards stay decoupled from S1's class (compiles and tests independently).
//   Do not "improve" the coupling.

// CRITICAL: do NOT clamp the thresholds in begin(). Baselines are validated
//   (2000..4050 > 350 — no wrap possible in-contract). An out-of-contract
//   baseline < 350 wraps the threshold huge -> everything reads wet -> EarlyWet
//   fault on first evaluation = the fail-safe direction (fault = valve stays
//   closed / is driven LOW first).

// GOTCHA: structs/classes get NO entry in the explicit-prototype block (native
//   g++ needs prototypes for FREE functions only — every landed class sets the
//   precedent). Your edit adds zero prototype lines.

// GOTCHA: UNGUARDED-region comments must avoid hardware tokens ("millis()",
//   "analogRead") — the purity gate greps tokens; it strips // comments, but
//   keep new prose clean anyway (write "millis()-domain timestamp" — wait, that
//   contains "millis": the shipped listing's evaluate() comment says
//   "millis()-domain" and the gate strips comments before grepping, so it is
//   safe — but do NOT put such tokens in CODE lines).

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
//   `pio run -e pico` is the compile gate. Never adjust a test's expected value
//   to make it pass — they are machine-verified; a failure means the
//   implementation deviates.
```

## Implementation Blueprint

### Data models and structure

Two types in the UNGUARDED region (complete verbatim listing in Task 1). State footprint: **4 bytes per instance** (two `uint16_t` thresholds, padded):

```cpp
uint16_t wetThreshold1_;  // baseline1 − TRIP_DROP_COUNTS, set by begin()
uint16_t wetThreshold2_;  // baseline2 − TRIP_DROP_COUNTS, set by begin()
```

Plus the POD verdict returned by value: `GuardVerdict { Kind kind; uint8_t sensor; }` (2 bytes). No clock state — `valveOpenedAtMs` arrives per call by contract.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "class TripDetector" firmware/jug_filler/jug_filler.ino      # EXPECT: 1 (S1 landed)
  - RUN: grep -c "end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2)" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (your insert anchor)
  - RUN: grep -cE "FalseTripGuards|GuardVerdict" firmware/jug_filler/jug_filler.ino  # EXPECT: 0 (not yet landed)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "TRIP_MIN_ELAPSED_S\|EARLY_WET_WINDOW_S\|TRIP_DROP_COUNTS" firmware/jug_filler/config.h  # EXPECT: all present (uint16_t, 60 / 5 / 350)
  - RUN: pio test -e native                                                   # EXPECT: exit 0, all projects [PASSED] (incl. test_trip)
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — UNGUARDED region block
  - EDIT: insert the "Task 1 listing" verbatim AFTER the line
      `// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========`
    and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with one blank
    line on each side (keep exactly one blank line banner-to-banner on both sides
    of your block).
  - PRESERVE: everything else byte-for-byte — prototype block, includes, all other
    UNGUARDED sections (incl. TripDetector), the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and all its tests executed
    against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_guards/test_guards.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (21 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_guards matches test_reading
    /test_baseline/test_trip convention. PIO auto-discovers the new project — no
    build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — insert verbatim:**

```cpp
// ===================== False-trip guards (P1.M3.T3.S2) =====================
// Sensor chain stage 5 (PRD §7.6), pure: classify trips and wet readings that
// are TOO EARLY to be real water arrival as false trips (electrical short,
// wet tips, jug already full) so the fill engine faults the offending sensor
// instead of trusting it. Two windows, both measured as fill time = time since
// valveOpenedAtMs — supplied BY THE ENGINE as a parameter each call; this
// class does NOT track when the valve opened (P1.M6.T1.S2 owns that):
//   - Early-wet:  an averaged reading at/below baseline − TRIP_DROP_COUNTS
//     (the SAME inclusive wet predicate as the trip rule, PRD §7.3 /
//     P1.M3.T3.S1) while fill time is inside the first EARLY_WET_WINDOW_S
//     (5 s) -> the sensor reads wet before water can possibly have reached
//     it -> short.
//   - Early-trip: a trip ACCEPTED by the TripDetector (tripped() latched,
//     P1.M3.T3.S1) while fill time is before TRIP_MIN_ELAPSED_S (60 s) ->
//     false trip.
// Both windows close AT equality (strict '<'): a trip accepted at exactly
// 60.000 s is LEGAL (Ok) and a wet reading at exactly 5.000 s is legal.
//
// Verdict precedence when several conditions hold at once (deterministic; the
// fault OUTCOME is identical either way — the engine faults the named sensor
// and latches, PRD §7 fault table): early-wet wins over early-trip (a short is
// the more specific diagnosis, and wetness necessarily PRECEDED the trip
// because the 8-reading debounce takes >= 800 ms), and sensor 1 (index 0) is
// reported before sensor 2 (index 1).
//
// Caller contract (P1.M6.T1.S2 — once per loop pass while FILLING; loop
// cadence is sub-millisecond, so the 60 s boundary cannot be straddled):
//   at fill start, with the SAME validated baselines the TripDetectors got
//   (BaselineCapture verdict Ok, P1.M3.T2.S1):
//     guards.begin(baseline.baseline(0), baseline.baseline(1));
//   every loop pass in FILLING, after feeding the detectors:
//     const GuardVerdict v = guards.evaluate(nowMs, valveOpenedAtMs,
//                                            trip1.tripped(), trip2.tripped(),
//                                            avg1, avg2);
//     if (v.kind != GuardVerdict::Ok) { /* fault path: valve LOW first, latch */ }
// evaluate() is STATELESS (a pure function of its inputs, const): the engine
// owns verdict consumption and fault latching (P1.M6.T1.S3). Calling it
// outside FILLING is out of contract: if nowMs predates valveOpenedAtMs the
// elapsed wraps to a huge fill time, both windows read closed, verdict Ok —
// harmless. begin() with an UNVALIDATED baseline (< TRIP_DROP_COUNTS) wraps
// the wet threshold to a huge value -> every reading reads wet -> EarlyWet
// fault on the first evaluation: out-of-contract, but the fail-safe
// direction (fault = the valve stays closed / is driven LOW first). No
// clamping on purpose.
struct GuardVerdict {
    enum Kind : uint8_t { Ok = 0, EarlyTripFault = 1, EarlyWetFault = 2 };
    Kind    kind;    // Ok, or which rule fired
    uint8_t sensor;  // 0 == sensor 1 (PIN_SENSOR1, fault code 1),
                     // 1 == sensor 2 (PIN_SENSOR2, fault code 2); 0 when Ok
};
class FalseTripGuards {
public:
    FalseTripGuards() : wetThreshold1_(0), wetThreshold2_(0) {}

    // Arm for a new fill with the sensors' VALIDATED dry baselines (counts,
    // BASELINE_MIN..BASELINE_MAX — BaselineCapture::verdict(s) == Ok).
    // Computes each wet threshold baseline − TRIP_DROP_COUNTS once, exactly
    // like TripDetector::begin does for the trip rule — the SAME predicate.
    void begin(uint16_t baseline1, uint16_t baseline2) {
        wetThreshold1_ = (uint16_t)(baseline1 - TRIP_DROP_COUNTS);
        wetThreshold2_ = (uint16_t)(baseline2 - TRIP_DROP_COUNTS);
    }

    // Classify one loop instant. nowMs and valveOpenedAtMs are timestamp-domain
    // uint32_t values injected by the engine; tripped1/2 are
    // TripDetector::tripped() (the detector's entire observable state);
    // avg1/2 are the latest averaged readings from the ReadingAggregator.
    // Wrap-safe fill time via elapsedMs() ONLY — direct timestamp comparison
    // is forbidden (time-helpers seam rule). Window constants are SECONDS in
    // config.h — widen to uint32_t BEFORE the x1000 or it truncates.
    GuardVerdict evaluate(uint32_t nowMs, uint32_t valveOpenedAtMs,
                          bool tripped1, bool tripped2,
                          uint16_t avg1, uint16_t avg2) const {
        const uint32_t fillTimeMs = elapsedMs(nowMs, valveOpenedAtMs);

        // Early-wet window (first EARLY_WET_WINDOW_S; closes AT equality).
        if (fillTimeMs < (uint32_t)EARLY_WET_WINDOW_S * 1000u) {
            if (avg1 <= wetThreshold1_) return { GuardVerdict::EarlyWetFault, 0 };
            if (avg2 <= wetThreshold2_) return { GuardVerdict::EarlyWetFault, 1 };
        }
        // Early-trip window (before TRIP_MIN_ELAPSED_S; closes AT equality).
        if (fillTimeMs < (uint32_t)TRIP_MIN_ELAPSED_S * 1000u) {
            if (tripped1) return { GuardVerdict::EarlyTripFault, 0 };
            if (tripped2) return { GuardVerdict::EarlyTripFault, 1 };
        }
        return { GuardVerdict::Ok, 0 };
    }

private:
    uint16_t wetThreshold1_;  // baseline1 − TRIP_DROP_COUNTS, set by begin()
    uint16_t wetThreshold2_;  // baseline2 − TRIP_DROP_COUNTS, set by begin()
};
// ========= end false-trip guards (consumed by P1.M6.T1.S2, P1.M6.T1.S3) =========
```

**Task 2 listing — `test/test_guards/test_guards.cpp` (verbatim):**

```cpp
// test/test_guards/test_guards.cpp — native contract proof for FalseTripGuards
// (P1.M3.T3.S2). Pins PRD §7.6: any trip accepted before TRIP_MIN_ELAPSED_S
// (60 s) of fill time, or any sensor reading wet during the first
// EARLY_WET_WINDOW_S (5 s), is a false trip / short -> fault that sensor.
// 'Wet' = averaged reading at/below baseline − TRIP_DROP_COUNTS — the SAME
// inclusive predicate as the trip rule (PRD §7.3, P1.M3.T3.S1).
// Window semantics (item-contract pins): trip at 59.9 s -> EarlyTripFault, at
// exactly 60.0 s -> Ok; wet at 4.9 s -> EarlyWetFault, at 5.1 s -> Ok. Both
// windows close AT equality (strict '<') — the exactly-5.000 s case mirrors
// exactly-60.000 s. Fill time = time since valveOpenedAtMs, wrap-safe.
// Worked example: baselines 3400/3400 -> wet thresholds 3050 (3050 itself
// reads wet, inclusive; 3051 reads dry).
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T3S2/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Assert a whole verdict at once (kind + sensor payload).
static void assertVerdict(GuardVerdict::Kind kind, uint8_t sensor, GuardVerdict v) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)kind, (uint8_t)v.kind);
    TEST_ASSERT_EQUAL_UINT8(sensor, v.sensor);
}

// ---- early-trip window (TRIP_MIN_ELAPSED_S = 60 s, closes AT equality) ----

// ITEM CASE: a trip accepted at 59.9 s of fill time is a false trip ->
// EarlyTripFault on sensor 1 (index 0 == fault code 1). Wet window is long
// closed at 59.9 s, so only the trip rule fires.
void test_trip_at_59_9s_is_early_trip_fault_sensor1() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(59900u, 0u, true, false, 3400, 3400));
}

// ITEM CASE: a trip accepted at EXACTLY 60.000 s is legal — the window closes
// at equality (strict '<'). Even BOTH sensors latched is Ok at 60.000 s.
void test_trip_at_exactly_60_0s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(60000u, 0u, true, true, 3400, 3400));
}

// 60.1 s with a wet averaged reading: past the trip window AND past the wet
// window — mid-fill wetness is the debounce's business, not the guards'.
void test_trip_at_60_1s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(60100u, 0u, true, false, 0, 3400));
}

// Sensor 2's early trip names sensor 2 (index 1 == fault code 2).
void test_trip_59_9s_on_sensor2_faults_sensor2() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 1,
                  g.evaluate(59900u, 0u, false, true, 3400, 3400));
}

// Deterministic precedence: both sensors early-tripped -> sensor 1 reported.
void test_both_tripped_early_reports_sensor1_first() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(59900u, 0u, true, true, 3400, 3400));
}

// ---- early-wet window (EARLY_WET_WINDOW_S = 5 s, closes AT equality) ----

// ITEM CASE: a wet reading at 4.9 s (deep wet, 0 counts) faults sensor 1
// before the debounce could even latch a trip.
void test_wet_at_4_9s_is_early_wet_fault_sensor1() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 0, 3400));
}

// A wet reading at EXACTLY 5.000 s is legal — the window closes at equality
// (strict '<'), mirroring exactly-60.000 s for the trip rule.
void test_wet_at_exactly_5_0s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(5000u, 0u, false, false, 3050, 3050));
}

// ITEM CASE: wet at 5.1 s (even 0 counts, both sensors) is Ok — past the
// window, no trips.
void test_wet_at_5_1s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(5100u, 0u, false, false, 0, 0));
}

// Sensor 2's wet reading names sensor 2 (index 1 == fault code 2).
void test_wet_at_4_9s_on_sensor2_faults_sensor2() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 1,
                  g.evaluate(4900u, 0u, false, false, 3400, 3050));
}

// ---- wet predicate boundary (same inclusive predicate as the trip rule) ----

// ITEM CASE: exactly baseline − 350 (3050 for baseline 3400) reads WET —
// PRD §7.6 "'wet' ... same predicate as the trip rule" and PRD §7.3 is
// inclusive ("at or below").
void test_wet_boundary_inclusive_at_baseline_minus_350() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
}

// One count above the threshold (3051) reads dry inside the window.
void test_one_count_above_threshold_is_dry() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(4900u, 0u, false, false, 3051, 3051));
}

// ---- guards stay silent on legitimate fills ----

// Deep wet mid-fill (30 s, no trip yet): the debounce is still accumulating —
// the guards only police the FIRST 5 s of wetness and trips before 60 s.
void test_deep_wet_after_window_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(30000u, 0u, false, false, 0, 0));
}

// A trip at 120 s with the reading still wet is a LEGIT trip: guards pass it
// (the engine proceeds to CONFIRM, PRD §7.4-7.5).
void test_legit_trip_after_60s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(120000u, 0u, true, false, 3050, 3400));
}

// ITEM CASE: both sensors clean (dry readings, no trips) at 59 999 ms -> Ok.
void test_clean_sensors_59_999ms_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(59999u, 0u, false, false, 3400, 3400));
}

// A trip at the valve-open instant itself (elapsed 0) is maximally false.
void test_trip_at_valve_open_instant_is_early_trip() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(0u, 0u, true, false, 3400, 3400));
}

// ---- rollover safety (item contract: wrap-safe timestamps) ----

// Valve opened 600 ms before the 2^32 ms rollover; the trip is accepted
// 59.9 s into the fill, i.e. 'now' has wrapped to 59300. elapsedMs()'s
// mod-2^32 subtraction still yields 59900 -> EarlyTripFault.
void test_wrap_safe_trip_window_across_rollover() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    const uint32_t valve = 4294967295u - 599u;   // 2^32 - 600
    const uint32_t now   = 59300u;               // (valve + 59900) mod 2^32
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(now, valve, true, false, 3400, 3400));
}

// Valve opened 4900 ms before the rollover; the wet reading is observed
// exactly at the wrap, when the clock restarts at 0 -> elapsed 4900 ->
// EarlyWetFault.
void test_wrap_safe_wet_window_across_rollover() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    const uint32_t valve = 4294967295u - 4899u;  // 2^32 - 4900
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(0u, valve, false, false, 3050, 3400));
}

// ---- precedence / defaults / re-arm ----

// At 3 s the sensor has BOTH latched a trip and still reads wet: report
// EarlyWet — the more specific short diagnosis (fault outcome identical).
void test_precedence_wet_reported_before_trip() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(3000u, 0u, true, false, 3050, 3400));
}

// Never begun (protocol always calls begin() first): thresholds are 0, so
// only a 0-count reading would read wet; dry readings stay Ok.
void test_fresh_instance_dry_readings_ok() {
    FalseTripGuards g;
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(10000u, 0u, false, false, 4095, 4095));
}

// begin() re-arms for the next fill with NEW baselines -> new thresholds
// (lower baseline -> lower threshold -> the same reading may no longer read
// wet).
void test_begin_rearm_changes_thresholds() {
    FalseTripGuards g;
    g.begin(3400, 3400);                     // thresholds 3050
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
    g.begin(2400, 2400);                     // thresholds 2050
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
    assertVerdict(GuardVerdict::EarlyWetFault, 1,
                  g.evaluate(4900u, 0u, false, false, 3400, 2050));
}

// End-to-end composition with the real TripDetector (P1.M3.T3.S1): 8 wet
// averaged readings at 100 ms cadence latch the detector at 800 ms; the
// guards — armed with the same baseline — see the wet reading inside the
// 5 s window, and (queried past the window) the latched trip inside the
// 60 s window. Pins that both classes share ONE predicate and ONE protocol.
void test_composition_with_trip_detector_same_predicate() {
    FalseTripGuards g;
    TripDetector t1, t2;
    g.begin(3400, 3400);
    t1.begin(3400);
    t2.begin(3400);
    GuardVerdict v{GuardVerdict::Ok, 0};
    for (uint32_t t = 100; t <= 800; t += 100) {
        t1.feed(3050);   // sensor 1 goes wet...
        t2.feed(3400);   // ...sensor 2 stays dry
        v = g.evaluate(t, 0u, t1.tripped(), t2.tripped(), 3050, 3400);
    }
    assertVerdict(GuardVerdict::EarlyWetFault, 0, v);   // wet window, avg 3050
    assertVerdict(GuardVerdict::EarlyTripFault, 0,      // trip window, latch set
                  g.evaluate(5800u, 0u, t1.tripped(), t2.tripped(), 3050, 3400));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_trip_at_59_9s_is_early_trip_fault_sensor1);
    RUN_TEST(test_trip_at_exactly_60_0s_is_ok);
    RUN_TEST(test_trip_at_60_1s_is_ok);
    RUN_TEST(test_trip_59_9s_on_sensor2_faults_sensor2);
    RUN_TEST(test_both_tripped_early_reports_sensor1_first);
    RUN_TEST(test_wet_at_4_9s_is_early_wet_fault_sensor1);
    RUN_TEST(test_wet_at_exactly_5_0s_is_ok);
    RUN_TEST(test_wet_at_5_1s_is_ok);
    RUN_TEST(test_wet_at_4_9s_on_sensor2_faults_sensor2);
    RUN_TEST(test_wet_boundary_inclusive_at_baseline_minus_350);
    RUN_TEST(test_one_count_above_threshold_is_dry);
    RUN_TEST(test_deep_wet_after_window_is_ok);
    RUN_TEST(test_legit_trip_after_60s_is_ok);
    RUN_TEST(test_clean_sensors_59_999ms_is_ok);
    RUN_TEST(test_trip_at_valve_open_instant_is_early_trip);
    RUN_TEST(test_wrap_safe_trip_window_across_rollover);
    RUN_TEST(test_wrap_safe_wet_window_across_rollover);
    RUN_TEST(test_precedence_wet_reported_before_trip);
    RUN_TEST(test_fresh_instance_dry_readings_ok);
    RUN_TEST(test_begin_rearm_changes_thresholds);
    RUN_TEST(test_composition_with_trip_detector_same_predicate);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: pure-service class shape (follows ButtonClassifier/ReadingAggregator
#   /BaselineCapture/TripDetector): documented protocol; const query methods;
#   zero dynamic memory; explicit constructor init-list in DECLARATION order
#   (wetThreshold1_, wetThreshold2_) — native g++ -Wreorder is part of the gate set.

// PATTERN: begin() as the per-fill arming point (BaselineCapture/TripDetector
#   precedent): one long-lived instance, begin(b1, b2) at fill start, evaluate()
#   per loop pass. The engine begins guards and detectors together with the SAME
#   validated baselines — one arming point, one predicate.

// CRITICAL: `fillTimeMs < (uint32_t)CONST_S * 1000u` — strict '<' (windows close
#   AT equality) AND the (uint32_t) widening BEFORE the multiply (config constants
#   are uint16_t SECONDS; 16-bit arithmetic here is a silent-truncation trap).

// CRITICAL: fill time ONLY via elapsedMs(nowMs, valveOpenedAtMs) — the mod-2^32
#   subtraction is the rollover safety; raw `now - since` at the use site violates
#   the time-helpers seam rule even though it would compute the same value.

// CRITICAL: wet comparison `avgN <= wetThresholdN_` (<=, not <) — same inclusive
#   boundary as the trip rule (PRD §7.3/§7.6); flipping it breaks the pinned
#   3050-wet/3051-dry pair.

// PATTERN: aggregate-init returns `return { GuardVerdict::EarlyWetFault, 0 };` —
#   GuardVerdict is a plain aggregate (Kind first, then sensor); keep the field
#   order, tests assert both fields.

// GOTCHA: the engine-side integration is DOCUMENTATION ONLY here — do not add any
#   ARDUINO-region code calling this class; P1.M6.T1.S2/P1.M6.T2.S1 own the wiring.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 bannered block in the
    UNGUARDED region (after the trip-detector end-banner). Prototype block,
    includes, ARDUINO region, setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes EARLY_WET_WINDOW_S / TRIP_MIN_ELAPSED_S /
    TRIP_DROP_COUNTS from config.h BY NAME.
TEST:
  - New project dir test/test_guards/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
DOWNSTREAM API (frozen by this task — P1.M6.T1.S2/S3 build against it):
  - GuardVerdict { Kind kind; uint8_t sensor; } — fault blink code == sensor + 1.
  - guards.begin(baseline1, baseline2) at fill start (validated baselines).
  - guards.evaluate(nowMs, valveOpenedAtMs, trip1.tripped(), trip2.tripped(),
    avg1, avg2) every loop pass while FILLING.
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - valve-open timestamp tracking / fill state        -> P1.M6.T1.S2 (parameter)
  - verdict consumption, valve LOW first, fault latch -> P1.M6.T1.S2 + P1.M6.T1.S3
  - loop wiring of detectors + guards                 -> P1.M6.T2.S1
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the block compiles under arm-gcc/arduino-pico inside
# the sketch build (it sits in the UNGUARDED region, included in both builds).
# Compile errors mean a transcription deviation from the listing — fix, don't
# redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; test_seam, test_time_utils, test_log_utils, test_button,
# test_reading, test_baseline, test_trip AND the new test_guards all [PASSED]
# (21/21 tests in test_guards). ANY failure: read the mismatch — the expected
# values are machine-verified against the real file, so a failure means the
# implementation (not the test) deviates. Never "fix" a test's expected value
# to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_guards
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_guards/

# (2) This task's .ino diff lives strictly inside the UNGUARDED region:
git diff firmware/jug_filler/jug_filler.ino | grep "^@@"
grep -nE -- "---- UNGUARDED REGION|---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   every hunk must start after the UNGUARDED marker line no. and end before
#   the ARDUINO marker line no.

# (3) Structural placement + counts:
grep -c "class FalseTripGuards" firmware/jug_filler/jug_filler.ino            # 1
grep -c "struct GuardVerdict" firmware/jug_filler/jug_filler.ino              # 1
grep -c "end false-trip guards (consumed by P1.M6.T1.S2" firmware/jug_filler/jug_filler.ino  # 1
grep -n "end trip detector (consumed by P1.M3.T3.S2" firmware/jug_filler/jug_filler.ino      # 1 hit, ABOVE your block

# (4) Public surface freeze (exactly the contract members, no extras):
sed -n '/class FalseTripGuards/,/^};/p' firmware/jug_filler/jug_filler.ino \
  | sed -n '/^public:/,/^private:/p' | grep -nE "^\s{4}(void|GuardVerdict|FalseTripGuards)"
# Expect: FalseTripGuards(), begin, evaluate — and NOTHING else.

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

# (3) Constants by name, no magic window literals in the class body:
sed -n '/class FalseTripGuards/,/end false-trip guards/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "\b60000\b|\b5000\b|\b59900\b|\b4900\b|\b350\b" \
  || echo "NO MAGIC WINDOW LITERALS"
# Expected: NO MAGIC WINDOW LITERALS (EARLY_WET_WINDOW_S / TRIP_MIN_ELAPSED_S /
#   TRIP_DROP_COUNTS appear by name only; the x1000u unit conversion stays).

# (4) Safety-reasoning audit (mirrors research/notes.md §2):
#     - Both windows close AT equality: exactly 60.000 s / exactly 5.000 s are
#       LEGAL — item pins 59.9->fault, 60.0->Ok, 4.9->fault, 5.1->Ok.
#     - Wrap: fill time via elapsedMs() only; both wrap cases test-pinned.
#     - Fail-safe: unvalidated-baseline wrap -> everything reads wet ->
#       EarlyWet fault = valve stays closed; evaluate() outside FILLING -> Ok
#       (harmless — the engine only calls it while FILLING).
#     - 5 s wet window vs 800 ms debounce: a genuine short faults at the FIRST
#       wet evaluation (~100 ms), long before the detector could latch.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_guards 21/21)
- [ ] "PURE OK (comments stripped)"; marker count 2; block exactly once; no magic window literals in the class body
- [ ] `git status --short`: `.ino` + `test/test_guards/` only; this task's `.ino` hunks strictly inside the UNGUARDED region; config.h/platformio.ini untouched

### Feature Validation

- [ ] Early-trip: 59.9 s → EarlyTripFault(sensor); exactly 60.0 s → Ok; 60.1 s → Ok; sensor 2 named correctly; both tripped → sensor 1 first
- [ ] Early-wet: 4.9 s → EarlyWetFault(sensor); exactly 5.0 s and 5.1 s → Ok; deep wet mid-fill → Ok (debounce's business)
- [ ] Wet predicate: inclusive at exactly baseline−350; dry at baseline−349 (same predicate as the trip rule)
- [ ] Legit fills pass: trip at 120 s → Ok; clean sensors at 59 999 ms → Ok; trip at valve-open instant → fault
- [ ] Wrap-safe: both windows classify correctly across the 2^32 ms rollover
- [ ] Precedence deterministic: wet beats trip; sensor 1 before sensor 2
- [ ] Re-arm: begin() with new baselines installs new thresholds; fresh instance defaults safe
- [ ] Composition: real TripDetector fed 8 wet readings → guards report EarlyWet (in window) and EarlyTrip (past window, latch set)

### Code Quality Validation

- [ ] Seam rules honored: zero hardware tokens in non-comment UNGUARDED lines; fill time via `elapsedMs()` only; valve-open timestamp accepted as a parameter (never stored)
- [ ] Public surface exactly the frozen members; `evaluate()` is `const` and stateless
- [ ] Zero new config constants; `EARLY_WET_WINDOW_S`/`TRIP_MIN_ELAPSED_S`/`TRIP_DROP_COUNTS` by name; `(uint32_t)` widening before `* 1000u`
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M6.T1.S2, P1.M6.T1.S3); caller protocol quoted in the class comment
- [ ] The window-closure semantics, precedence, and fail-safe rationale are documented in-code (they are — keep them)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't use `<=` for either window, or `<` for the wet threshold — window closure is strict (60.000 s and 5.000 s are legal), the wet predicate is inclusive (baseline−350 reads wet). Both directions are PRD §7.6 semantics and are test-pinned
- ❌ Don't track the valve-open timestamp, add fill state, or latch verdicts inside the class — the item contract says the engine supplies `valveOpenedAtMs` per call and owns fault latching (P1.M6.T1.S2/S3); `evaluate()` is a pure function
- ❌ Don't store or accept `TripDetector&` / a `Reading` struct — those types don't apply (S1's detector exposes only `tripped()`; the aggregator emits plain `uint16_t`s). Take the bools and counts
- ❌ Don't compute fill time with raw `now - since` or timestamp comparison — `elapsedMs()` only (time-helpers seam rule); direct comparison is meaningless across the wrap
- ❌ Don't multiply the SECONDS constants in 16-bit — widen with `(uint32_t)... * 1000u` first or the comparison silently truncates
- ❌ Don't clamp the thresholds in begin() — the wrap-to-huge on an out-of-contract baseline makes everything read wet → immediate EarlyWet fault = the fail-safe direction
- ❌ Don't add any constant to config.h — §8 is frozen; all three tunables already exist
- ❌ Don't touch `TripDetector`, `BaselineCapture`, `ReadingAggregator`, the prototype block, the ARDUINO region, config.h, platformio.ini, or other test files — completed tasks own them
- ❌ Don't adjust a test's expected value to make it pass — expected values are machine-verified; failures mean implementation deviation
