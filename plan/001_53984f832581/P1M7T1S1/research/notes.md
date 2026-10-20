# Research notes — P1.M7.T1.S1 (Safety invariant verification suite)

Session date: research against working tree at commit `c4d30cc` + uncommitted
`test/test_faultlatch/` (P1.M6.T1.S3) and the P1.M6.T2.S1 PRP as contract
(wiring task running in parallel, not yet in the tree — verified: placeholder
comment `P1.M6.T2.S1 fills the loop below this line` still present at .ino
line ~1582).

## 1. What exists and what this task adds

- `test/test_fillengine/test_fillengine.cpp` ALREADY contains a seeded-fuzz
  property test (`test_fuzz_lifecycle_invariants`, lines ~415–449): 64 seeds ×
  3000 passes, properties A–F (valve only in FILLING, close-first on exits,
  entry only via Idle+ShortPress, humanStart tracking, fault latch, log
  capacity). Its harness is fixed-Δt (25 ms) and binary DRY/WET levels.
- This task's suite is ADDITIVE, not duplicative:
  - dedicated project (`test/test_invariants/`) so invariants live together
    and guard all future refactors (the work item's OUTPUT contract);
  - variable per-pass Δt (5–55 ms) — off-grid pacing stresses deadline /
    reading-cadence arithmetic;
  - clock-wraparound seed family (now starts near 0xFFFFFFFF) — §10
    rollover-safety under the actual engine;
  - adversarial sensor levels beyond DRY/WET (0, 100, 1999, 2000, 4050, 4051,
    4095 → baseline faults, rail values);
  - explicit no-ShortPress storm (§5.3/§10: no human press → never fills);
  - fault-latch valve-refusal storm; fresh-instance (watchdog-reset proxy)
    tests incl. mid-fill reset → Idle/closed + freshBoot() tank history
    cleared (§5.3);
  - the STRUCTURAL check (Python) — nothing like it exists in-repo today.

## 2. Verified engine API (jug_filler.ino, pure region — READ-ONLY)

- `FillEngine::step(uint32_t nowMs, ButtonClassifier::Event ev, bool
  readingDue, uint16_t avg1, uint16_t avg2) -> FillEngineOutputs` (line ~1094).
- `FillEngineOutputs` (line ~1070): `valveOpen`, `led`, `faultCode`,
  `plus5Applied`, `acknowledged`, `ackBlink`, `logCount`,
  `logs[FILL_LOG_CAPACITY]`. Zero-init = valve closed, Off (§10 fail-safe).
- `FillLogEvent::Kind::ValveClosed = 1` — "ALWAYS first on its step (§7.9)".
- Public getters: `state()`, `valveOpen()`, `valveOpenedAtMs()`,
  `deadline()` → `const FillDeadline&` (line ~1244; `freshBoot()`,
  `tableMinutes()`, `anchored()` used by existing tests).
- `enum class FillState : uint8_t { Idle=0, Filling=1, Confirm=2, Done=3,
  Fault=4 }` (line ~911).
- `ButtonClassifier::Event::{None, ShortPress, LongPress}`.
- `constexpr uint8_t FILL_LOG_CAPACITY = 5` (namespace scope, visible to tests).
- `elapsedMs()` rollover-safe (P1.M1.T2.S1).
- config.h: `READING_INTERVAL_MS=100`, `BASELINE_MIN=2000`,
  `BASELINE_MAX=4050`, `TRIP_DROP_COUNTS=350` (implied by WET=2500 <
  3500−350). DRY=3500 in-range, WET=2500 always-trips (documented in the
  existing Sim harness header comment).

## 3. Structural facts of the .ino (basis for check_setup_first_line.py)

- `setup()` (line ~1503): first executable statement is literally
  `digitalWrite(PIN_VALVE, LOW);   // LINE 1 — PRD §10 …`.
- `loop()` (line ~1529): first executable statement is `watchdog_update();`.
- Forward declarations exist at lines ~38–39 (`void setup();` / `void
  loop();`) → a naive `find("void setup()")` hits the DECLARATION first.
  The script must match `void setup\s*\(\s*\)\s*\{` (definition) instead.
- Current `delay(` occurrences are ALL inside comments (lines ~21, ~715,
  ~1586 region) → comment stripping before scanning is MANDATORY, else
  false positives. No `delay(` in any string literal; no `while (!Serial`.
- `#ifdef ARDUINO` region: single, line ~1293 → `#endif // ARDUINO` ~1589.
  Whole-file scan (comments+strings stripped) is a strict superset of the
  required "ARDUINO loop region" check and simpler — chosen deliberately.
- P1.M6.T2S1 PRP contract (post-wiring state this task consumes): fill
  routing stage between ADC stage and LED drive; `s_fill.step(` exactly once;
  valve `digitalWrite` before `logFillEvent` loop; zero `delay(`; zero
  `while (!Serial`; `setup()` byte-identical.

## 4. Native test conventions (13 existing Unity projects)

- One directory per suite under `test/`, single `test_<name>.cpp`,
  `#include <unity.h>` then `#include "../../firmware/jug_filler/jug_filler.ino"`.
- `void setUp() {} / void tearDown() {}` stubs required.
- `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
  REQUIRED on PIO 6.2.0 (no auto-generated runner) — comment in
  test_fillengine.cpp.
- Deterministic LCG `rngStep` (Numerical Recipes constants,
  `s = s*1664525u + 1013904223u`) — "the seed IS the reproducibility
  contract" (house precedent, line ~75).
- platformio.ini `[env:native]`: test dirs auto-discovered; NO ini change
  needed for a new test dir; `build_src_filter = -<*>` (tests include the
  .ino directly). NEVER `pio test -e pico`.

## 5. External references (verified reachable this session)

- https://docs.platformio.org/en/latest/advanced/unit-testing/ → 200 OK
  (structure of test projects, `pio test`, Unity runner notes).
- https://docs.platformio.org/en/latest/advanced/unit-testing/#examples → 200 OK.
- arduino-pico readthedocs pages 404 on direct fetch from this host (bot
  filter) — NOT cited; not needed for this tests-only item.
- Property-based testing background (no dependency added — the suite uses
  the in-repo LCG pattern, deliberately no Hypothesis/gtest): seeded
  deterministic generation + invariant assertion is the standard
  "stateful property testing" shape (cf. Hypothesis's rule/stateful model —
  concept reference only).

## 6. Runtime budget estimate

Existing fuzz: 64×3000 = 192k steps, suite runs in seconds. Planned: 48×5000
(storms) + 16×4000 (no-press) + 8×4000 (wrap) + 2 small scripted tests ≈
~336k engine steps, each a pure in-memory state update — same order of
magnitude, still seconds on any CI host.

## 7. Out of scope (guarded elsewhere)

- Backstop-table hugging real fill times → P1.M4.T1.S1 table tests (per the
  work item's own note (e)).
- Physical watchdog-reset + power-rules bench proof → P1.M7.T2.S1.
- Loop wiring greps (valve-before-logs ordering, single setMode site) →
  P1.M6.T2.S1 Level-2 gate, re-checked on bench.
