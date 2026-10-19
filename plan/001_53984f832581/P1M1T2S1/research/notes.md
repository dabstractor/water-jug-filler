# Research Notes — P1.M1.T2.S1: Rollover-safe time utilities (pure)

## 1. What exists when implementation starts (S2 contract — treat as delivered)

P1.M1.T1.S2 is being implemented in parallel; its PRP is the binding contract:

- `firmware/jug_filler/jug_filler.ino` — skeleton sketch. UNGUARDED region holds exactly
  one probe function `uint32_t seamVersion() { return 1; }` whose prototype comment says
  *"seam probe — replaced when first real logic lands"*. **First real logic = THIS
  subtask** — we replace `seamVersion()` with the two time helpers.
- `firmware/jug_filler/config.h` — S1 deliverable, complete, read-only for us. All
  timing tunables already there (`BUTTON_DEBOUNCE_MS`, `READING_INTERVAL_MS`,
  `CONFIRM_WINDOW_S*1000`-scale windows, backstop minutes, LED pattern ms...).
  No new constants needed: `HALF_WHEEL_MS` below is a mathematical constant, not a
  tunable, so it lives in the sketch, not config.h.
- `platformio.ini` (repo root) — `[env:native]` (unity, `build_src_filter = -<*>`),
  `[env:pico]` compile-only, `default_envs = native`, `test_dir = test`.
- `test/test_seam.cpp` — Unity test that (a) asserts config constants, (b) calls
  `seamVersion()` asserting `== 1`. We must update (b) when we remove the probe.
  Pattern: `#include "../firmware/jug_filler/jug_filler.ino"`, explicit `int main()`
  with `UNITY_BEGIN()/RUN_TEST()/UNITY_END()` — **mandatory on PIO 6.2.0** (no
  auto-generated runner; link fails with "undefined reference to main" without it —
  verified live by the S2 research).
- Region-marker comment lines must stay byte-stable (P1.M7.T1.S1 structural tests
  anchor on them):
  `// ---- UNGUARDED REGION: pure logic (host-testable) ----`
  `// ---- #ifdef ARDUINO REGION: hardware glue ----`

Adding `test/test_time_utils.cpp` requires NO platformio.ini change: each `test_*.cpp`
is its own Unity project; `pio test -e native` runs them all sequentially.

## 2. The mathematics (why the pattern is correct)

- `millis()` on arduino-pico returns `uint32_t`; wraps 0xFFFFFFFF → 0x00000000 every
  2^32 ms = 4,294,967,296 ms ≈ **49.71 days**
  (source: https://arduino-pico.readthedocs.io/en/latest/millis.html — verified in
  plan/.../external_deps.md item 4).
- ISO C++ guarantees unsigned integer +/− is performed **modulo 2^N** — wrap-around is
  well-defined behavior, never UB (C++17 [expr.arith.conv]/6; current draft:
  https://eel.is/c++draft/expr#arith.conv — "usual arithmetic conversions ... unsigned
  arithmetic is performed modulo 2^n"). This is the entire reason the idiom is safe.
- `elapsedMs(now, since) = now - since` (uint32_t): for any true elapsed span
  `0 ≤ span < 2^32`, the modular difference returns `span` exactly, regardless of where
  the pair sits relative to the wrap. Examples verified by hand:
  - `0 - 0xFFFFFFFF ≡ 1 (mod 2^32)` → 1 ms elapsed across the wrap.
  - `0x1B6640 - 0xFFFFFF00 ≡ 1,800,000` (a 30-min backstop anchored 256 ms before wrap).
- `deadlinePassed(now, deadline)`: "reached or passed" test =
  `elapsedMs(now, deadline) < 0x80000000u` (the half-wheel trick).
  - True ⟺ `now` is in `[deadline, deadline + 0x7FFFFFFF]` (mod 2^32).
  - **Inclusive at equality** — chosen deliberately: a fill backstop sampled exactly at
    its deadline must fire (valve closes AT the deadline, not 1 ms later; PRD §10
    conservative safety bias).
  - Domain limit: forward windows < 2^31 ms ≈ 24.86 days ("half wheel"). Beyond that
    the answer is unobservable/ambiguous. Every firmware timing is ≤ 44 min (max
    backstop), so the domain is never stressed — but tests must PIN the boundary.
- NEVER compare timestamps directly (`now >= deadline`) — ordering across the wrap is
  meaningless. Never use signed math/casts (`int32_t`) for the comparison — signed
  overflow IS UB and the cast-based idiom buys nothing the unsigned one lacks.

## 3. Consumer map (who calls what, downstream — informs semantics, do NOT implement)

- P1.M2 ButtonClassifier: debounce `elapsedMs(now, lastEdgeMs) >= BUTTON_DEBOUNCE_MS`;
  long-press threshold `>= BUTTON_LONG_PRESS_MS`.
- P1.M3 sensor chain: cadence `>= READING_INTERVAL_MS`; baseline settle
  `>= BASELINE_SETTLE_MS`; false-trip guards TRIP_MIN_ELAPSED_S / EARLY_WET_WINDOW_S.
- P1.M4 FillDeadline: `deadlinePassed(now, fillDeadlineMs)` closes the valve;
  deadline computed as `valveOpenedMs + minutes * 60000UL` (mod-2^32 arithmetic makes
  deadline ADDITION safe too); +5 stacking re-anchors the deadline the same way.
- P1.M5 LedPatternEngine: on/off phase timing via `elapsedMs(now, phaseStartMs)`.
- P1.M6 FillEngine: CONFIRM_WINDOW_S between sensor trips via `elapsedMs`.
- Rule of thumb taught to consumers: interval question → `elapsedMs(...) >= interval`;
  absolute-due-time question → `deadlinePassed(now, deadlineTs)`.

## 4. External references (verified via web search 2025, PIO 6.2.0 facts via S2 research)

- https://arduino-pico.readthedocs.io/en/latest/millis.html — arduino-pico millis():
  uint32_t, wraps ~49.7 days (already cited in external_deps.md item 4).
- https://eel.is/c++draft/expr#arith.conv — ISO C++ draft: unsigned arithmetic is
  modular (the standardese that makes the idiom UB-free).
- https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover —
  canonical community Q&A; accepted answer: `(uint32_t)(current - start) >= interval`.
- https://bigdanzblog.wordpress.com/2016/05/15/writing-code-to-handle-arduinos-millis-roll-over —
  worked examples of the unsigned-subtraction property at the wrap.
- https://www.baldengineer.com/arduino-millis-plus-addition-does-not-add-up.html —
  the anti-pattern we forbid: comparing `millis()` against a precomputed SUM target
  (`if (millis() >= target)`) breaks across wrap; subtraction survives it.
  (Relevant nuance for us: computing a deadline via addition is fine; testing it via
  `now >= deadline` is not — that's exactly why `deadlinePassed` exists.)

## 5. Test design (maps 1:1 to the item's required cases)

Required by the item: wrap boundary 0xFFFFFFFF→0x00000000, equality (0 elapsed),
1ms-after-deadline, large spans. All hand-verified expected values:

| test | call | expected |
|---|---|---|
| equality at 0 | elapsedMs(0,0) | 0 |
| equality mid | elapsedMs(1000000,1000000) | 0 |
| equality at max | elapsedMs(0xFFFFFFFF,0xFFFFFFFF) | 0 |
| simple span | elapsedMs(5000,1000) | 4000 |
| wrap 1ms | elapsedMs(0,0xFFFFFFFF) | 1 |
| wrap small span | elapsedMs(0x00000064,0xFFFFFF00) | 356 |
| max span | elapsedMs(0xFFFFFFFF,0) | 0xFFFFFFFF |
| half-wheel span | elapsedMs(0x7FFFFFFF,0) | 0x7FFFFFFF |
| deadline at | deadlinePassed(1000,1000) | true (inclusive) |
| deadline +1ms | deadlinePassed(1001,1000) | true |
| deadline −1ms | deadlinePassed(999,1000) | false |
| wrap +1ms | deadlinePassed(0,0xFFFFFFFF) | true |
| wrap −1ms | deadlinePassed(0xFFFFFFFE,0xFFFFFFFF) | false |
| backstop across wrap | elapsedMs(0x1B6640,0xFFFFFF00) | 1800000 (30 min) |
| backstop deadline at/−1/+1 | deadlinePassed(0x1B6640 / −1 / +1, 0x1B6640) | true/false/true |
| half-wheel fwd | deadlinePassed(0x7FFFFFFF,0) | true |
| exactly half-wheel | deadlinePassed(0x80000000,0) | false (boundary pinned) |
| 1ms before 0 | deadlinePassed(0xFFFFFFFF,0) | false |

Sanity for "wrap small span": 0x64=100; since 0xFFFFFF00 is 256 below 2^32 →
elapsed = 100+256 = 356. For the backstop: deadline = (0xFFFFFF00 + 1,800,000) mod 2^32
= 0x1B6640.

Unity specifics (PIO 6.2.0 + Unity 2.6.1): `TEST_ASSERT_EQUAL_UINT32(expected, actual)`,
`TEST_ASSERT_TRUE/FALSE(x)`; suffix big literals with `UL` (e.g. `0xFFFFFFFFUL`) so the
comparison stays unsigned. Each test file needs setUp/tearDown + explicit main().
