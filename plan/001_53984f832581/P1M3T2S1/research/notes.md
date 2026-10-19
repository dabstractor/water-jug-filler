# Research Notes — P1.M3.T2.S1: BaselineCapture (pure)

## §1 Contract extraction (binding inputs)

From the item contract + PRD §7.1/§8/§9.3 + architecture `firmware_architecture.md`:

- PRD §7.1: "settle `BASELINE_SETTLE_MS` (2s), read both sensors' dry baselines (median of
  averaged readings). If either is outside `BASELINE_MIN..MAX` → fault that sensor (also
  catches 'jug still full / tips wet' accidental starts). Refuse to open the valve."
- config.h (landed, verified this session): `BASELINE_SETTLE_MS = 2000` (uint32_t),
  `BASELINE_MIN = 2000`, `BASELINE_MAX = 4050` (both uint16_t). §8 is FROZEN — the item
  adds ZERO config constants; N=5 is a task-contract constant living in the class.
- Item LOGIC: `begin(uint32_t nowMs)`; Settle 2000 ms collecting NOTHING ("abort during
  settle is the FSM's business, P1.M6.T1.S2"); then N=5 averaged readings per sensor;
  finish with per-sensor verdict enum {Ok, TooLow, TooHigh} + baseline values.
- Item OUTPUT: verdicts + baselines consumed by TripDetector (P1.M3.T3.S1) and the fill
  engine baseline phase (P1.M6.T1.S2; fault-code path P1.M6.T1.S3). DOCS: none — internal.
- Arch §3.1 (binding): "Baseline settle runs INSIDE FILLING, valve closed. A short-press
  abort during the 2 s settle is honored" — i.e. the FSM owns abort; the class needs no
  cancel method.
- Arch §1 slot list: `BaselineCapture` sits in the UNGUARDED region directly after
  `ReadingAggregator + medianOf (P1.M3.T1.S1)`.

Upstream surface (read from the landed `.ino` this session, lines 165–263):
- `elapsedMs(now, since)` — mod-2^32 subtraction; `>= CONST` comparisons are inclusive at
  equality and wrap-safe. House rule: never hand-roll `now - last >= X`.
- `ReadingAggregator::SENSOR_COUNT == 2` (0 == PIN_SENSOR1 == fault code 1,
  1 == PIN_SENSOR2 == fault code 2); `takeReading(nowMs, outAvg1, outAvg2)` emits pairs.
- `uint16_t medianOf(uint16_t* values, size_t n)` — sorts ASCENDING IN PLACE; "free
  function: BaselineCapture (P1.M3.T2.S1) owns the buffer" (its comment says so verbatim).

## §2 Binding design decisions

1. **API freeze** (downstream PRPs P1.M3.T3.S1 / P1.M6.T1.S2 / P1.M6.T1.S3 are written
   against this surface; no extra public members):
   - `void begin(uint32_t nowMs)` — reset + settle anchor at the fill-start instant.
   - `void addReading(uint32_t nowMs, uint16_t avg1, uint16_t avg2)` — TOTAL function:
     pairs are DROPPED while `elapsedMs(nowMs, startMs_) < BASELINE_SETTLE_MS`; the first
     pair at/after the settle boundary flips Settle→Collect; the 5th accepted pair computes
     medians+verdicts once (Collect→Done); pairs after Done are ignored. The FSM forwards
     every emitted aggregator pair unconditionally — no caller-side phase tracking.
   - `bool done() const`; `uint16_t baseline(uint8_t sensor) const`;
     `Verdict verdict(uint8_t sensor) const`.
   - `enum Verdict : uint8_t { Ok = 0, TooLow = 1, TooHigh = 2 };`
   - `static constexpr uint8_t READINGS_PER_SENSOR = 5;` — task contract, NOT a §8 tunable
     (same precedent as `LOGLINE_MAX`, `ADC_SAMPLE_PACE_MS`). Odd on purpose: median of 5
     is the pure middle element (no even-pair truncation); 5 × 100 ms = 500 ms of capture.
2. **Settle boundary is INCLUSIVE at exactly 2000 ms** (house rule: `BUTTON_DEBOUNCE_MS`
   "exactly 40 is enough", `readingDue` "inclusive at exactly 100", `deadlinePassed`
   "inclusive at equality"). A pair at t=1999 is dropped; at t=2000 accepted. Pinned by
   tests on both sides.
3. **addReading does NOT re-check the 100 ms cadence** — the aggregator owns cadence
   (P1.M3.T1.S1 froze `readingDue`/`takeReading` for that). BaselineCapture's only
   time-dependence is the settle gate. Tests may feed pairs at any spacing ≥ settle.
4. **Fail-safe accessors before done()**: `baseline()==0`, `verdict()==TooLow`
   (constructor AND `begin()`). Invariant: `verdict(s)` ALWAYS equals the classification of
   `baseline(s)` under BASELINE_MIN..MAX (0 < 2000 → TooLow). A premature read can never
   look Ok — it looks faulted. Mirrors the aggregator convention "consumers treat 0 as
   out-of-range".
5. **No abort/cancel method** — item contract: "abort during settle is the FSM's business".
   The FSM stops calling; `begin()` fully resets for the next fill. Pinned by
   test_begin_resets_for_next_fill.
6. **medianOf sorts the capture buffers in place** — legal and free: raw reading order is
   never needed after the medians are taken; the class owns its buffers (medianOf's
   documented contract anticipates exactly this).
7. **classify() bounds are inclusive**: `< BASELINE_MIN → TooLow`, `> BASELINE_MAX →
   TooHigh`, else Ok. 2000 and 4050 pass; 1999 and 4051 fault. Pinned by 4 boundary tests.
8. **Verdict, not the valve**: "REFUSE to open the valve" belongs to the fill engine
   (P1.M6.T1.S2 reads verdicts; P1.M6.T1.S3 maps sensor index+1 → blink code 1/2). This
   class is pure and only reports.
9. **Out-of-domain sensor index** on accessors → `baseline()==0`, `verdict()==TooLow`
   (defensive, mirrors aggregator's out-of-domain no-op).
10. **RAM footprint**: 36 bytes/instance (measured). Buffers: 2×5 uint16 readings (20 B),
    2×uint16 baselines, 2×Verdict, phase/startMs/taken.

## §3 Machine verification (this session, against the REAL current `.ino`)

Method: scratch host build at /tmp/p1m3t2s1/verify.cpp —
`#include` the real `firmware/jug_filler/jug_filler.ino` (absolute path; config.h resolves
relative to the .ino), append the candidate class, assert all 17 planned cases, then:

```
g++ -std=c++17 -Wall -Wextra -Werror -o verify verify.cpp && ./verify
→ ALL 17 CASES PASS — sizeof(BaselineCapture) = 36 bytes
```

Zero warnings under -Wall -Wextra -Werror. Verified values (hand-derived AND executed):

| Case | Feeds | Expected |
|---|---|---|
| settle collects nothing | 19 pairs t=100..1900 | !done, baseline 0 |
| settle not shortcut @1999 | 5 pairs t=1999 | !done, baseline 0 |
| inclusive @2000 | 1@1999 + 5@2000..2400 (3500,2500) | done, 3500/2500, both Ok |
| done needs exactly 5 | 4 then 5th pair | !done → done |
| post-done pairs ignored | garbage 6th pair | baselines unchanged |
| boundary 2000 | 5×(2000,3200) | verdict(0) Ok, baseline 2000 |
| boundary 4050 | 5×(4050,3200) | verdict(0) Ok, baseline 4050 |
| 1999 faults | 5×(1999,3200) | TooLow, baseline 1999 |
| 4051 faults | 5×(4051,3200) | TooHigh, baseline 4051 |
| both bad | 5×(1999,4051) | TooLow + TooHigh |
| independent | 5×(3400,1999) | Ok + TooLow |
| outlier robustness | s0 {3400,3410,1000,3405,3408}, s1 {3300,3310,4095,3305,3308} | 3405 / 3308, both Ok |
| outlier at boundary | {1999,2000,2000,2000,2000} | median 2000 → Ok |
| unsampled sensor | 5×(3400,0) | Ok + TooLow (0 out-of-range) |
| fail-safe before done | begin(1000), nothing | baselines 0, both TooLow |
| begin() reuse | Ok capture → begin → (2000,4051) | reset then Ok + TooHigh |
| 49.7-day wrap | begin(0xFFFFF830); 5@0xFFFFFFFF dropped; 5@0..400 accepted | done, baseline 3500 |

Wrap arithmetic: `0xFFFFF830 + 0x7D0 (2000) == 0x1_0000_0000` — the settle completes
EXACTLY at the wrapped instant t=0; `elapsedMs(0xFFFFFFFF, 0xFFFFF830) = 0x1CF = 463 < 2000`
(dropped), `elapsedMs(0, 0xFFFFF830) = 2000` (accepted, mod-2^32).

Median spot-checks: sorted {1000,3400,3405,3408,3410} → mid idx2 = 3405;
sorted {1999,2000,2000,2000,2000} → mid idx2 = 2000.

## §4 External references

- Median vs mean robustness (why the PRD medians the baseline: breakdown point 50%, a
  single wet splash during capture cannot move it):
  https://www.megunolink.com/articles/coding/3-methods-filter-noisy-arduino-measurements
  (same reference underpinned P1.M3.T1.S1's medianOf).
- PlatformIO native Unity test conventions (test_dir auto-discovery, explicit runner on
  PIO 6.2.0): https://docs.platformio.org/en/stable/plus/unit-testing.html

No new library surface is consumed: the class is pure C++ over already-landed helpers
(elapsedMs, medianOf) and config constants — the whole point of the seam.

## §5 Downstream API freeze (what later PRPs may rely on)

```cpp
BaselineCapture bc;
bc.begin(fillStartMs);                    // P1.M6.T1.S2 at fill start (valve closed)
bc.addReading(nowMs, a1, a2);             // on every aggregator emission
if (bc.done()) {
    // P1.M6.T1.S2: if verdict(0)!=Ok -> fault code 1; verdict(1)!=Ok -> fault code 2;
    //              valve stays LOW (refuse). Else capture baselines, open valve.
    // P1.M3.T3.S1 TripDetector: armed with baseline(0)/baseline(1).
}
```

Sensor index → fault code mapping (frozen since P1.M3.T1.S1): 0 → code 1, 1 → code 2.

## §6 Parallel-work safety (P1.M3.T1.S2 in flight)

P1.M3.T1.S2 edits ONLY the ARDUINO region (its PRP's own gates confine its diff there:
"UNGUARDED region byte-stable"). This task edits ONLY the UNGUARDED region + adds
test/test_baseline/. Both tasks' anchors are region-local text markers, not line numbers:
insert AFTER `// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========`
(currently line 263) and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`
(currently line 265; blank line 264 between). If ARDUINO-region merge noise is visible,
leave it alone.
