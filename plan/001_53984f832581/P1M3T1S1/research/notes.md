# Research Notes — P1.M3.T1.S1: ReadingAggregator + median helper (pure)

Session date: 2025-10-19. All codebase facts verified by direct reads this session.

## 1. Codebase contracts consumed (verified in-file)

- `firmware/jug_filler/jug_filler.ino` — the single seam file. UNGUARDED region
  currently holds: `elapsedMs`/`deadlinePassed` (P1.M1.T2.S1, landed),
  `formatBootBanner` (P1.M1.T2.S2), `class ButtonClassifier` (P1.M2.T1.S1 —
  ALREADY LANDED in the file, verified by read; plan_status says
  "Implementing" but the class text is present). ARDUINO region holds
  `logLine`, `setup`, `loop` (watchdog first line).
- Explicit-prototype block exists above the UNGUARDED region; house rule 3
  (native g++ does not run the Arduino .ino prototype generator). Classes need
  no prototype; free functions do. `size_t` is already visible at that point
  (`<stdio.h>` is included above the block; C std guarantees stdio.h defines
  `size_t`) — no new include needed for `medianOf(uint16_t*, size_t)`.
- `firmware/jug_filler/config.h` (READ-ONLY): `ADC_SAMPLES_PER_READING = 32`
  (uint16_t constexpr), `READING_INTERVAL_MS = 100` (uint32_t constexpr),
  `BASELINE_MIN/MAX`, `TRIP_DROP_COUNTS`, `DEBOUNCE_COUNT` all present.
  This task adds ZERO constants.
- Time semantics (P1.M1.T2.S1, binding): never compare timestamps directly;
  interval questions compose `elapsedMs(now, since) >= CONST`. Unsigned
  mod-2^32 subtraction is exact for spans < 2^32 ms → cadence is wrap-safe by
  construction. Inclusive-at-equality is the house convention
  (`>=`, exactly 100 ms is due — mirrors BUTTON_DEBOUNCE_MS "exactly 40 is
  enough").
- Test pattern (`test/test_button/test_button.cpp`, `test_time_utils`): one
  dir per unit under `test/`, single `.cpp`, `#include
  "../../firmware/jug_filler/jug_filler.ino"`, empty `setUp`/`tearDown`,
  hand-derived expected values, and an EXPLICIT `int main()` with
  `UNITY_BEGIN()/RUN_TEST(...)/UNITY_END()` — required on PIO 6.2.0 (no
  auto-generated Unity runner). `pio test -e native` auto-discovers new dirs.
- Platform: PlatformIO Core 6.2.0 on this machine; `pio run -e pico`
  compile-only; `pio test -e native` host tests. NEVER `pio test -e pico`.
- Architecture (firmware_architecture.md §1): this task's slot is exactly
  "ReadingAggregator + medianOf (P1.M3.T1.S1)" in the UNGUARDED region;
  loop order shows `→ sample ADCs → aggregator → FillEngine.step` — the
  aggregator is fed by the wiring task P1.M3.T1.S2 and its emission protocol
  is driven once per loop pass by the fill engine (P1.M6.T2.S1).

## 2. Design decisions (binding for this PRP)

1. **Sensor index domain {0,1}**: 0 == PIN_SENSOR1 == fault code 1;
   1 == PIN_SENSOR2 == fault code 2. Out-of-domain `sensor` values are
   silently dropped (defensive no-op, testable). `SENSOR_COUNT = 2` is a
   class-static constexpr, NOT a config.h addition (config.h is §8 tunables
   only — complete, owned by P1.M1.T1.S1).
2. **Boxcar with pre-fill**: 32-slot ring per sensor + running `uint32_t` sum.
   First sample ever seen pre-fills the entire window → every emitted reading
   is a valid 32-sample average from the first reading onward (warmup weight
   31/32 on sample #1, decaying as real samples displace it). Matches the
   item contract "pre-fill with first sample so readings are valid before 32
   collect" and the canonical Arduino Smoothing pattern (running-sum ring).
3. **Running-sum update trick**: `sum += new - evicted` in `uint32_t` — exact
   under mod-2^32 arithmetic even when `new < evicted` (true window sum stays
   ≤ 32×65535 < 2^21, wraps cancel). Max sum 2,097,120 — no overflow risk.
   Division `sum / ADC_SAMPLES_PER_READING` truncates toward zero; truncation
   is PINNED by tests (32496/32 = 1015.5 → 1015).
4. **`addSample`'s `nowMs` is intentionally unused** (`(void)nowMs`): sample
   arrival timing carries no meaning (samples may burst or trickle); only the
   READING cadence uses the clock. The parameter exists for seam uniformity —
   every service API takes the injected clock. Documented in-code so no
   reviewer puzzles over it. (The item's own test list tests no sample-time
   behavior, confirming this reading.)
5. **Cadence**: `readingDue(now) := elapsedMs(now, lastReadingMs_) >=
   READING_INTERVAL_MS`, const. `takeReading(now, a1, a2)` is TOTAL (emits +
   re-anchors at `now` unconditionally); intended protocol is the pair
   `if (readingDue(now)) takeReading(now, a1, a2);`. Re-anchoring at the
   EMISSION instant (not lastDue + interval) means a >100 ms stall emits
   exactly ONE reading, then a full interval of silence — no catch-up burst.
   This keeps DEBOUNCE_COUNT = 8 consecutive readings ≈ 800 ms of REAL time
   regardless of polling gaps (protects PRD §7.3 semantics). Anchor
   initializes to 0 → first reading due one full interval after construction
   (readings at 100, 200, … on the boot-time grid; phase is irrelevant, only
   cadence is normative).
6. **Un-seeded sensor emits 0**: if no sample ever arrived, sum is 0 → avg 0.
   Harmless: consumers gate on BASELINE_MIN..MAX (0 is out of range → fault
   path), and the wiring task samples both pins every loop pass so this
   cannot persist in practice.
7. **`medianOf` is a FREE function** (per item contract), sorts the caller's
   buffer ASCENDING IN PLACE (insertion sort — n is tens; O(n²) irrelevant;
   zero dependencies, deterministic). Contract loudly documented: caller
   passes scratch/mutable memory (BaselineCapture P1.M3.T2.S1 will own its
   reading buffer). Edge cases: n==0 → 0; n==1 → v[0]; odd → v[n/2];
   even → (v[n/2−1]+v[n/2])/2 truncated, accumulated in uint32_t —
   65535+65534 = 131069 OVERFLOWS uint16_t (wraps to 533 → garbage median);
   the uint32 accumulator is a pinned test trap.
8. **RAM budget**: 2×32×uint16 window (128 B) + 2×uint32 sums + heads/flags +
   anchor ≈ 150 B static per instance — trivial vs RP2040's 264 KB.
9. **Purity comment discipline**: the unguarded-region purity gate greps for
   hardware tokens; the naive `grep -E "millis|…"` also matches COMMENT
   mentions (the landed time-helpers comment block says "millis()" three
   times). Corrected gate: strip `//` comments first, then grep (validated
   this session → prints PURE OK on the current tree). New code/comments in
   the unguarded region still avoid gratuitous hardware tokens.

## 3. Hand-derived test expectations (all machine-verified 2025-10-19)

Compiled the exact PRP listing + all asserts against the real .ino with
`g++ -std=c++17 -Wall -Wextra -Werror` on this host → ALL PASS.

| Case | Input | Expected |
| --- | --- | --- |
| pre-fill | one sample 3000 (s0), 2500 (s1) | avg1=3000, avg2=2500 |
| full window | 32 samples 1000..1031 (sum 32496) | 32496/32=1015.5→**1015** (truncation pinned) |
| slide | 32×1000 then 1032 | (32000−1000+1032)/32=**1001** |
| warmup | 2000 then 3000 | (64000−2000+3000)/32=65000/32=**2031** |
| OOR sensor | addSample(2,…), addSample(255,…) | dropped, no effect, no crash |
| due inclusive | t=99 / t=100 / t=100000 | false / true / true |
| re-anchor | take at 100 | false at 199, true at 200 |
| no burst | take at 100, stall→5000 | one take; false at 5099, true at 5100 |
| wrap | anchor 0xFFFFFF9C (=−100) | false at 0xFFFFFFFF (99 elapsed), true at 0x00000000 (exactly 100 across wrap) |
| median odd | {3500,2000,3000} | 3000 |
| median even | {10,20,30,40} | (20+30)/2=25 |
| even truncation | {1,2} | 1 (1.5→1) |
| outlier | {3000,3010,4095,3005,3008} | 3008 (wild 4095 can't move it) |
| in-place | {3,1,2} → after call | {1,2,3} (contract pinned for BaselineCapture) |
| overflow trap | {65535,65534} | 65534 via uint32 sum (uint16 sum would wrap → 32766-wrong) |
| edge sizes | n=0 / n=1 | 0 / value |

## 4. External references

- Arduino official Smoothing tutorial (canonical boxcar/running-sum moving
  average with circular buffer — the exact pattern, with pre-seeding as our
  extension): https://docs.arduino.cc/built-in-examples/analog/Smoothing/
- Luis Llamas, "Moving Average Filter on Arduino" (fast running-sum ring
  implementation, pre-fill discussion):
  https://www.luisllamas.es/en/arduino-moving-average-filter/
- MegunoLink, "Three Methods to Filter Noisy Arduino Measurements" (average
  vs median trade-offs; why median for baselines resists outliers):
  https://www.megunolink.com/articles/coding/3-methods-filter-noisy-arduino-measurements
- Why median (not mean) for the PRD §7.1 baseline: a single wet splash or
  spike during settle cannot drag the median — median's breakdown point is
  50% vs 0% for the mean. The outlier test pins this property.

## 5. Downstream consumers (API freeze for this task's public surface)

- `addSample(uint8_t sensor, uint16_t counts, uint32_t nowMs)` — called by ADC
  wiring (P1.M3.T1.S2) each loop pass for both sensors.
- `readingDue(uint32_t nowMs) const` + `takeReading(uint32_t nowMs, uint16_t&,
  uint16_t&)` — driven once per loop pass by the fill engine (P1.M6.T1.S2);
  the emitted pair feeds BaselineCapture (P1.M3.T2.S1), TripDetector
  (P1.M3.T3.S1) and false-trip guards (P1.M3.T3.S2).
- `uint16_t medianOf(uint16_t* values, size_t n)` — called by BaselineCapture
  on its own scratch buffer (in-place sort contract).
