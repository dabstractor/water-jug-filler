# Research Notes — P1.M3.T3.S2: False-trip guards (TRIP_MIN_ELAPSED_S / EARLY_WET_WINDOW_S)

## §1 Contract analysis & type mapping

Item contract (task breakdown):

> Pure guard functions: `GuardVerdict evaluate(uint32_t nowMs, uint32_t valveOpenedAtMs,
> TripState s1, TripState s2, Reading avg1, Reading avg2) -> {Ok, EarlyTripFault(sensor),
> EarlyWetFault(sensor)}`. 'Fill time' = time since valveOpenedAt. Native tests: trip at
> 59.9 s -> EarlyTripFault, at exactly 60.0 s -> Ok; wet reading at 4.9 s -> EarlyWetFault,
> at 5.1 s -> Ok; both sensors clean -> Ok; wrap-safe timestamps.

Mapping the contract's conceptual types onto the REAL codebase types (verified by reading
the landed code, not assumed):

| Contract term | Real codebase type | Evidence |
|---|---|---|
| `TripState s1/s2` | `bool tripped1/tripped2` | P1.M3.T3.S1 froze `TripDetector`'s public surface to EXACTLY `begin(uint16_t)`, `feed(uint16_t)`, `tripped() const` — `tripped()` is the detector's entire observable state (latch). Landed in `.ino` line 422 at research time. |
| `Reading avg1/avg2` | `uint16_t avg1/avg2` | `ReadingAggregator::takeReading(nowMs, uint16_t& outAvg1, uint16_t& outAvg2)` emits plain averaged counts; NO `Reading` struct exists anywhere in the codebase. |
| `valveOpenedAtMs` | parameter, never stored | Item: "the valve-open timestamp will be supplied by the fill engine (P1.M6.T1.S2) — accept it as a parameter, do not track it here." |
| baselines | `begin(uint16_t, uint16_t)` | The wet predicate needs baseline − TRIP_DROP_COUNTS per sensor. Follows the house per-fill arming pattern (`TripDetector::begin`, `BaselineCapture::begin`); the engine begins guards + detectors together with the SAME validated baselines. |
| `GuardVerdict` | `struct { Kind kind; uint8_t sensor; }` | Item writes `EarlyTripFault(sensor)` — the verdict carries WHICH sensor. `Kind` ∈ {Ok, EarlyTripFault, EarlyWetFault}; `sensor` 0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2 (matches the PRD §7 fault table mapping sensor→code). |

## §2 Binding design decisions

1. **Both windows close AT equality (strict `<`)** — pinned by the item: trip at exactly
   60.0 s → Ok means the early-trip predicate is `fillTimeMs < TRIP_MIN_ELAPSED_S*1000`,
   NOT `<=`. Symmetrically the wet window is `fillTimeMs < EARLY_WET_WINDOW_S*1000`
   (wet at exactly 5.000 s → Ok; 4.9 s → fault). Both windows use the same strict-<
   convention — self-consistent, and both boundary sides are test-pinned.
2. **Wet predicate = the trip rule's predicate, verbatim**: `avg <= baseline −
   TRIP_DROP_COUNTS` — inclusive at exactly baseline−350 (PRD §7.6 says "same predicate
   as the trip rule"). `FalseTripGuards::begin()` computes the thresholds exactly like
   `TripDetector::begin()` does. Test-pinned at the boundary (3050 wet / 3051 dry for
   baseline 3400).
3. **STATELESS `evaluate()`** (pure function of its inputs, `const` method): the item says
   "Pure guard functions". The engine (P1.M6.T1.S2) owns verdict consumption and fault
   latching (P1.M6.T1.S3) — the guard never latches. Consequence (accepted limitation,
   documented in-class): a trip latched at 59.5 s but FIRST evaluated at 60.5 s would read
   Ok. Cannot happen in the engine protocol: evaluate is called EVERY loop pass while
   FILLING (loop cadence ≪ 1 ms), so the 60 s boundary cannot be straddled undetected.
4. **Deterministic precedence when multiple conditions hold at once**: early-WET beats
   early-TRIP (a short is the more specific diagnosis; wetness necessarily preceded the
   trip — the 8-reading debounce takes ≥ 800 ms), and sensor 1 (index 0) is reported
   before sensor 2 (index 1). Fault OUTCOME is identical either way (engine faults the
   named sensor and latches); precedence only stabilizes diagnostics + tests.
5. **Units**: config.h windows are SECONDS (`uint16_t`); evaluate works in ms. The
   comparison MUST widen before multiplying: `(uint32_t)TRIP_MIN_ELAPSED_S * 1000u`.
   (`uint16_t` 60 000 would barely fit today, but `EARLY_WET_WINDOW_S`-style arithmetic
   in 16-bit is a silent-truncation trap; widen explicitly, pinned by gotcha note.)
6. **Wrap-safety**: fill time is computed via `elapsedMs(nowMs, valveOpenedAtMs)` BY NAME —
   the time-helpers seam rule (`.ino` lines 44–51: direct timestamp comparison is
   FORBIDDEN; mod-2^32 subtraction IS the rollover safety). Verified across the wrap:
   valve = 2^32−600, now = 59 300 → elapsed = 59 900 → EarlyTripFault despite wrap;
   valve = 2^32−4900, now = 0 → elapsed = 4 900 → EarlyWetFault.
7. **Un-begun / out-of-contract behavior**: fresh instance thresholds are 0 → only a
   0-count reading reads wet (deterministic, harmless; protocol always begins first).
   `begin()` with an UNVALIDATED baseline < 350 wraps the threshold to a huge value →
   everything reads wet → EarlyWet fault on first evaluation — fail-safe direction (fault
   = valve stays closed). No clamping on purpose (mirrors S1's no-clamp reasoning).
8. **Decoupling**: evaluate takes `bool tripped1/2`, NOT `const TripDetector&` — the
   guards compile and test independently of S1's class. The composition is pinned by ONE
   integration test (`test_composition_with_trip_detector_same_predicate`) that feeds a
   real TripDetector 8 wet readings and evaluates the guards with its latch + the wet avg.

## §3 Machine verification record (this session, host g++)

Method (house convention, same as P1.M3.T3.S1 §3): merged-copy verification.

1. `/tmp/p1m3t3s2/jug_filler.ino` = the REAL current repo `.ino` + the candidate guards
   block inserted verbatim after the `end trip detector` banner.
2. `/tmp/p1m3t3s2/test_guards.cpp` = the exact shipped test file, except the `#include`
   line points at the merged copy (shipped file uses the repo-relative
   `../../firmware/jug_filler/jug_filler.ino`).
3. Command + result:

```
g++ -std=c++17 -Wall -Wextra -Werror \
    -I.pio/libdeps/native/Unity/src \
    /tmp/p1m3t3s2/test_guards.cpp .pio/libdeps/native/Unity/src/unity.c -o /tmp/p1m3t3s2/run
→ COMPILE_OK (zero warnings under -Wall -Wextra -Werror)

/tmp/p1m3t3s2/run | tail -3
→ -----------------------
  21 Tests 0 Failures 0 Ignored
  OK
```

Expected-value derivations (all hand-derived, then executed):

| Test | fillTime (ms) | inputs | expected |
|---|---|---|---|
| trip 59.9 s | 59 900 | tripped1, dry | EarlyTripFault/0 |
| trip exactly 60.0 s | 60 000 | tripped1+2, dry | Ok (strict <) |
| trip 60.1 s | 60 100 | tripped1, avg1=0 | Ok (window closed) |
| trip 59.9 s sensor2 | 59 900 | tripped2 | EarlyTripFault/1 |
| both tripped | 59 900 | tripped1+2 | EarlyTripFault/0 (sensor-0 first) |
| wet 4.9 s deep | 4 900 | avg1=0 | EarlyWetFault/0 |
| wet exactly 5.0 s | 5 000 | avg=3050/3050 | Ok (strict <) |
| wet 5.1 s | 5 100 | avg=0/0 | Ok |
| wet 4.9 s sensor2 | 4 900 | avg2=3050 | EarlyWetFault/1 |
| wet boundary inclusive | 4 900 | avg1=3050 (=3400−350) | EarlyWetFault/0 |
| one count above | 4 900 | avg=3051/3051 | Ok |
| deep wet mid-fill | 30 000 | avg=0/0, no trips | Ok (debounce's business) |
| legit trip | 120 000 | tripped1, avg1=3050 | Ok |
| clean 59 999 ms | 59 999 | dry, no trips | Ok |
| trip at t=0 | 0 | tripped1 | EarlyTripFault/0 |
| wrap trip | 59 900 (across 2^32) | valve=2^32−600, now=59 300 | EarlyTripFault/0 |
| wrap wet | 4 900 (across 2^32) | valve=2^32−4900, now=0 | EarlyWetFault/0 |
| precedence wet>trip | 3 000 | tripped1 AND avg1=3050 | EarlyWetFault/0 |
| fresh instance | 10 000 | never begun, dry 4095 | Ok |
| re-arm 3400→2400 | 4 900 | avg 3050 then thresholds 2050 | fault → Ok → EarlyWet/1 |
| composition w/ TripDetector | 100→800, then 5 800 | 8×feed(3050), evaluate with t1.tripped() | EarlyWet/0 then EarlyTrip/0 |

**21 tests / 0 failures.** The class is 8 bytes per instance (`uint16_t` ×2), zero
dynamic memory, zero new config constants (consumes `EARLY_WET_WINDOW_S`,
`TRIP_MIN_ELAPSED_S`, `TRIP_DROP_COUNTS` by name from config.h §"Windows"/§"ADC / sensors").

## §4 Downstream API freeze (what P1.M6.T1.S2 / S3 build against)

```cpp
struct GuardVerdict {
    enum Kind : uint8_t { Ok = 0, EarlyTripFault = 1, EarlyWetFault = 2 };
    Kind    kind;
    uint8_t sensor;   // 0 == sensor1/fault code 1, 1 == sensor2/fault code 2; 0 when Ok
};
class FalseTripGuards {
public:
    FalseTripGuards();
    void begin(uint16_t baseline1, uint16_t baseline2);   // per fill, validated baselines
    GuardVerdict evaluate(uint32_t nowMs, uint32_t valveOpenedAtMs,
                          bool tripped1, bool tripped2,
                          uint16_t avg1, uint16_t avg2) const;
};
```

Fault-code mapping for P1.M6.T1.S3: `sensor + 1` == PRD §7 fault blink group (1/2).

## §5 Sequencing & anchors (parallel-execution note)

- P1.M3.T3.S1 (TripDetector) landed DURING this research session (it was mid-implementation
  when research started; re-verified after landing). Verified current landmarks:
  - `class TripDetector` — .ino line 422, exactly once
  - `// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========` — line 458 ← INSERT ANCHOR for this task
  - `// ---- #ifdef ARDUINO REGION: hardware glue ----` — line 460
- The guards block takes `bool` trip flags, so it compiles independently of S1's class —
  but the PRP still mandates S1-landed as a precondition (deterministic file order:
  baseline → TripDetector → guards → ARDUINO region).
- Symbol cleanliness verified: `FalseTripGuards` / `GuardVerdict` / `test_guards` absent
  from the entire repo before this task.

## §6 External references

- PlatformIO native Unity testing (test-dir auto-discovery, one project per `test/`
  subdir, explicit runner requirement on PIO 6.2.0):
  https://docs.platformio.org/en/stable/plus/unit-testing.html
- Arduino `millis()` reference — documents the ~49.7-day (2^32 ms) rollover that makes
  `elapsedMs()`'s mod-2^32 subtraction the correct wrap-safe idiom:
  https://docs.arduino.cc/language-reference/en/functions/time/millis/
