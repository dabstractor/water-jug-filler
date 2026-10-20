# PRP — P1.M7.T1.S1: Property tests — no auto-open, valve LOW first, close-on-every-exit

## Goal

**Feature Goal**: A dedicated, CI-runnable **safety-invariant suite** that machine-proves PRD §10's non-negotiables against the real firmware artifacts, so every future refactor of the engine or the loop is guarded: (1) the valve NEVER opens except via `Idle + ShortPress` (a human BOOT press) — proven over seeded randomized event+sensor storms of thousands of steps each, including a no-press storm, a fault-latched storm, and a clock-wraparound storm; (2) every transition out of `Filling`/`Confirm` emits `ValveClosed` as `logs[0]` (close-before-anything, §7.9); (3) a fresh engine instance (the native proxy for watchdog reset / power cycle, §5.3/§10) boots `Idle` with the valve LOW and never auto-fills; (4) `setup()`'s first executable statement drives `PIN_VALVE` LOW and there is no `delay(` outside comments — enforced by a structural script, not trust.

**Deliverable**: Two new files, nothing else modified:
1. `test/test_invariants/test_invariants.cpp` — a new native Unity project (auto-discovered by `pio test -e native`) with six tests: fresh-boot safety, no-ShortPress-never-opens, randomized storms (48 seeds × 5000 passes), wraparound storms (8 seeds × 4000), fault-latch valve refusal, and mid-fill reset → Idle/closed/fresh-tank.
2. `test/check_setup_first_line.py` — a stdlib-only Python 3 structural checker (exit 0/1) asserting: **A1** `setup()` first executable statement is `digitalWrite(PIN_VALVE, LOW);`, **A2** `loop()` first executable statement is `watchdog_update();`, **A3** zero `delay(` tokens outside comments/string literals, **A4** zero `while (!Serial` gates.

**Success Definition**: `pio test -e native` → exit 0, ALL projects `[PASSED]` including the new `test_invariants`; `python3 test/check_setup_first_line.py` → exit 0 printing `STRUCTURAL SAFETY CHECK: PASS`; the combined CI command `pio test -e native && python3 test/check_setup_first_line.py` is green; `git status --short` shows ONLY the two new files (plus prior tasks' uncommitted work, untouched). Negative-control sanity: temporarily swapping `Low`↔`High` semantics or inserting a `delay(5);` makes the corresponding check FAIL (documented procedure below — run it once, then revert).

## User Persona

**Target User**: The future maintainer (human or AI agent) refactoring `FillEngine`, the loop stages, or any timing service — and the CI pipeline that must catch a §10 violation before it reaches a bench with water flowing.

**Use Case**: Someone "harmlessly" adds a `delay(250)` for LED blink pacing in the loop, or makes fault recovery re-enter `Filling` automatically after a watchdog reset. The next CI run must fail loudly, naming the violated invariant — without anyone remembering that §10 exists.

**User Journey**: maintainer edits firmware → runs `pio test -e native && python3 test/check_setup_first_line.py` → either green (invariants held) or a Unity assertion / `STRUCTURAL SAFETY CHECK: FAIL` line pinpointing the property or structural rule that broke.

**Pain Points Addressed**: §10 invariants currently rest on (a) one fuzz test buried inside the scenario suite (`test_fuzz_lifecycle_invariants` in `test_fillengine.cpp` — scenario-suite-owned, fixed-Δt, binary levels) and (b) human code review of the .ino. Nothing structural guards `setup()` line 1 or the no-`delay()` rule; nothing exercises clock wraparound or the reset-lands-safe property.

## Why

- PRD §10 is explicitly "non-negotiable": valve LOW first in `setup()`, valve opens only in FILLING via human press (never on boot/watchdog/fault/button auto-paths), watchdog hang → closed → IDLE, no blocking `delay()`, rollover-safe `millis()` arithmetic. This task is the machine enforcement of that section — the plan's own P1.M7.T1 name is "Safety invariant verification suite".
- Every prior task deliberately built toward this: pure engine + native seam (P1.M1–M6) so invariants are testable off-device; the .ino's own header comment (lines ~21–22) says "The invariant suite (P1.M7.T1.S1) structurally asserts this and scans for delay() in the loop region" — this task fulfills that promise.
- Downstream: P1.M7.T2.S1 (bench dry-run) consumes a firmware whose safety properties are already machine-proven, so bench time is spent on physical validation (real watchdog resets, real flows), not re-litigating logic. P2 refactors inherit the guard.

## What

Create the two files exactly as specified in the Blueprint (complete code provided — it was written against the live APIs read from source this session). The native suite drives the REAL `FillEngine` (included from the .ino, same as all 14 sibling suites) through adversarial seeded sequences; the Python script statically checks the REAL sketch file. **Not in scope** (owners): any change to `firmware/jug_filler/jug_filler.ino`, `config.h` (§8 frozen), `platformio.ini`, sibling test suites (scenario coverage lives in `test_fillengine`/`test_faultlatch`); backstop-table accuracy (P1.M4.T1.S1 table tests); on-bench watchdog/power validation (P1.M7.T2.S1); docs (work item says "DOCS: none — tests only").

### Success Criteria

- [ ] `pio test -e native` → exit 0, all projects `[PASSED]`, `test_invariants` among them
- [ ] `python3 test/check_setup_first_line.py` → exit 0, `PASS` line
- [ ] Storm properties all hold at EVERY step of EVERY sequence: valve HIGH only in `Filling`; `Filling` entered only from `Idle` on a `ShortPress` pass; every `Filling`/`Confirm` exit step has `logs[0] == ValveClosed` and `valveOpen == false`; no-press sequences never enter `Filling`/never open; fault latches with valve refusing; outputs agree with `eng.valveOpen()`
- [ ] Fresh-instance tests: new engine → `Idle`, valve LOW, zero logs, `deadline().freshBoot()`; mid-fill reset proxy → `Idle`+closed under a wet adversarial stream, then a NEW press starts a fresh fill
- [ ] Structural: A1 `setup()` first executable = `digitalWrite(PIN_VALVE, LOW);`; A2 `loop()` first executable = `watchdog_update();`; A3 zero `delay(` outside comments/strings; A4 zero `while (!Serial`
- [ ] Negative controls performed once and reverted (both files catch planted violations)
- [ ] `git status --short`: only the two new files added; zero modifications to existing files

## All Needed Context

### Context Completeness Check

✅ Passes the "No Prior Knowledge" test: every consumed symbol was read from live source this session — `FillEngine::step` signature and zero-init contract, `FillEngineOutputs` fields, `FillLogEvent::Kind::ValveClosed`, `FillState` members (`Idle=0, Filling=1, Confirm=2, Done=3, Fault=4`, .ino line ~911), public getters `state()/valveOpen()/valveOpenedAtMs()/deadline()` (line ~1244), `ButtonClassifier::Event::{None,ShortPress,LongPress}`, `FILL_LOG_CAPACITY=5`, `elapsedMs`, `READING_INTERVAL_MS/BASELINE_MIN/BASELINE_MAX` from config.h; the complete copy-paste-ready source for BOTH new files is in the Blueprint; the structural facts (setup line 1, watchdog first, comment-only `delay(` occurrences, forward-declaration trap at .ino lines ~38–39) were verified by reading the current .ino. Validation commands are the repo's proven invocations.

### Documentation & References

```yaml
- url: https://docs.platformio.org/en/latest/advanced/unit-testing/
  why: how PIO discovers test/*/ directories as projects and runs Unity —
        confirms the new test_invariants dir needs NO platformio.ini change.
  critical: PIO 6.2.0 requires an explicit main() with UNITY_BEGIN/RUN_TEST
        (no auto-generated runner) — every sibling suite carries this comment.

- url: https://docs.platformio.org/en/latest/advanced/unit-testing/#examples
  why: canonical test-project layout (one dir, one runner cpp) this task follows.

- file: test/test_fillengine/test_fillengine.cpp
  why: THE pattern source — Sim loop-simulator harness, deterministic LCG
        (rngStep, line ~75: "the seed IS the reproducibility contract"),
        DRY=3500/WET=2500 rationale, Unity main() footer, and the existing
        fuzz properties A–F this suite EXTENDS (not duplicates — see Gotchas).
  pattern: #include <unity.h> + #include "../../firmware/jug_filler/jug_filler.ino";
            setUp/tearDown stubs; TEST_ASSERT_* naming; RUN_TEST table in main().
  gotcha: READ-only — that suite is scenario-owned; do not edit or delete its
            fuzz test (overlap is intentional defense-in-depth).

- file: firmware/jug_filler/jug_filler.ino
  why: the artifact under test. Pure region (lines 1–~1291): FillEngine
        (class ~1081), FillEngineOutputs (~1070), FillLogEvent (~1044),
        FillState (~911), FILL_LOG_CAPACITY, elapsedMs. ARDUINO region
        (~1293–1589): setup() (~1503, first stmt `digitalWrite(PIN_VALVE,
        LOW);`), loop() (~1529, first stmt `watchdog_update();`), and — per
        the P1.M6.T2.S1 contract — the fill-routing stage between the ADC
        stage and the LED-drive stage.
  pattern: read it in Task 0 to re-confirm line landmarks after the wiring
            task lands (its PRP warns line numbers shift).
  gotcha: READ-ONLY for this task. Forward declarations `void setup();` /
            `void loop();` at ~38–39 — the Python script must regex the
            DEFINITION (`void setup\s*\(\s*\)\s*\{`), never a bare find().

- file: firmware/jug_filler/config.h
  why: READING_INTERVAL_MS=100, BASELINE_MIN=2000, BASELINE_MAX=4050 — the
        constants the harness's DRY/WET/wild levels are chosen against.
  gotcha: §8 frozen — no edits.

- file: plan/001_53984f832581/P1M6T2S1/PRP.md
  why: BINDING CONTRACT for the loop wiring this suite's structural check
        polices: zero delay(, zero while(!Serial, watchdog_update() first,
        setup() line 1 untouched, valve write before log lines.
  gotcha: that task runs in PARALLEL — Task 0 below gates on its landing.

- file: test/test_faultlatch/test_faultlatch.cpp
  why: newest sibling suite (P1.M6.T1.S3) — confirms the faultCode 1..3
        level contract in FAULT that test_fault_latch_valve_refuses asserts.
  gotcha: fault-code SEMANTICS are its ownership; this suite only asserts
            the §10 valve/latch consequences.

- file: spec/SPEC.md
  why: the PRD — §10 (invariants), §7 (lifecycle + §7.9 close-first law),
        §5.3 (reset lands IDLE/closed, fresh 30-min tank), §3 (never
        while(!Serial).
```

### Current Codebase tree (verified this session)

```bash
water-jug-filler/
├── platformio.ini              # [env:native] 14 Unity projects green; test dirs
│                               #   auto-discovered; [env:pico] compile-only
├── firmware/jug_filler/
│   ├── jug_filler.ino          # ~1589 lines: pure region 1–~1291 (READ-ONLY),
│   │                           #   #ifdef ARDUINO ~1293–1589; setup() ~1503;
│   │                           #   loop() ~1529 (+ P1.M6.T2.S1 fill stage, parallel)
│   └── config.h                # §8 frozen — READ-ONLY
├── test/                       # 14 Unity projects (…, test_fillengine,
│                               #   test_faultlatch, test_time_utils, …)
├── README.md
└── spec/SPEC.md                # the PRD
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── test/
│   ├── test_invariants/
│   │   └── test_invariants.cpp   # NEW — seeded property storms over the real
│   │                             #   FillEngine: no-auto-open, close-first on
│   │                             #   every exit, fresh-boot/reset safety, fault
│   │                             #   latch valve refusal, wraparound safety
│   └── check_setup_first_line.py # NEW — structural §10 check on the sketch:
│                                 #   setup line 1 = valve LOW, loop line 1 =
│                                 #   watchdog_update(), no delay( outside
│                                 #   comments/strings, no while(!Serial
# NOTHING else changes (no platformio.ini, no README, no firmware edits).
```

### Known Gotchas of our codebase & Library Quirks

```python
# CRITICAL — forward declarations: the .ino declares `void setup();` /
#   `void loop();` near the top (~38–39). A bare text find("void setup()")
#   anchors on the DECLARATION and then matches the WRONG brace block.
#   The script MUST use re.search(r"void\s+setup\s*\(\s*\)\s*\{", code).

# CRITICAL — delay( appears in COMMENTS today (e.g. "zero delay(), zero
#   hardware calls"). The script MUST strip // and /* */ comments (and
#   string/char literals — a format string could contain "delay(" text)
#   BEFORE scanning, replacing stripped spans with spaces while PRESERVING
#   newlines so line numbers stay reportable.

# CRITICAL — deterministic seeding is the contract: reuse the house LCG
#   rngStep (s = s*1664525u + 1013904223u). Never rand()/time(). A failure
#   must reproduce bit-for-bit from the printed seed constants.

# CRITICAL — Unity main() is REQUIRED on PIO 6.2.0 (no auto runner):
#   int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }

# CRITICAL — never `pio test -e pico` (no on-device transport). Native only.

# GOTCHA — feeding button events EVERY pass (even consecutive presses) is a
#   deliberate ADVERSARIAL SUPERSET of the legal classifier stream (real
#   events fire once per release); the engine must uphold the invariants
#   anyway. Document this in the test header, as done below.

# GOTCHA — variable Δt (5–55 ms) means fills do NOT open on the reading
#   grid; exact-time scripting helpers (tripAt) from test_fillengine do NOT
#   transfer. The property suite only asserts per-step invariants — that is
#   its strength (order-of-event independence).

# GOTCHA — wild levels (0/1999/4051/4095) during baseline cause
#   BaselineFault → latched FAULT; storms then sit in Fault (still valid
#   invariant coverage). Hence the 75/17/8-ish DRY/WET/wild bias keeps
#   Filling coverage alive. This is intended, not a bug.

# GOTTCHA — WET=2500 is below the minimum legal wet threshold (2000−350),
#   DRY=3500 inside BASELINE_MIN..MAX — rationales documented in
#   test_fillengine.cpp's harness header; reuse the same values so both
#   suites speak one language.

# GOTCHA — a wraparound-storm FAILURE likely indicates a REAL engine bug in
#   rollover arithmetic (elapsedMs/deadline), not a test bug. Investigate and
#   report; never weaken the assertion to pass.
```

## Implementation Blueprint

### Data models and structure

No production data models change. The suite defines two local test-side types (in `test_invariants.cpp`): `PropSim` (a variable-Δt, configurable-start-time adaptation of the scenario suite's `Sim`) and free functions `rngStep`, `randomLevel`, `randomEvent`, `runStorm` (the shared invariant oracle). The Python script is a single stdlib module. Pure C++17 host code + Python 3; no ORM/Pydantic/dependencies.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY prerequisites (do not skip — sibling task is parallel)
  - RUN: pio test -e native
    → ALL projects [PASSED] (incl. test_fillengine AND test_faultlatch).
    A RED suite = STOP and report; do not build on a broken baseline.
  - RUN: grep -c "P1.M6.T2.S1 fills the loop below this line" firmware/jug_filler/jug_filler.ino
    → 0 (the wiring landed). If 1, P1.M6.T2.S1 has NOT landed yet: STOP and
    report — this task's INPUT is "FillEngine + integration firmware from
    P1.M6.T2.S1" (work-item contract).
  - RUN: grep -c "s_fill.step(" firmware/jug_filler/jug_filler.ino   # -> 1
  - RUN: grep -n "void setup()\|void loop()" firmware/jug_filler/jug_filler.ino
    → note fresh line numbers; visually re-confirm setup()'s first executable
    statement is still digitalWrite(PIN_VALVE, LOW); and loop()'s is
    watchdog_update(); (P1.M6.T2.S1's contract guarantees both).

Task 1: CREATE test/test_invariants/test_invariants.cpp
  - IMPLEMENT: the six tests in "Implementation Patterns & Key Details" §P
    (complete file, copy-paste-ready).
  - FOLLOW pattern: test/test_fillengine/test_fillengine.cpp (include pair,
    setUp/tearDown, rngStep LCG, Unity main footer).
  - NAMING: test_{property}_{expected} functions; file/dir name
    test_invariants (matches sibling suite naming test_{concern}).
  - PLACEMENT: test/test_invariants/test_invariants.cpp (new dir — PIO
    auto-discovers; NO platformio.ini edit).

Task 2: CREATE test/check_setup_first_line.py
  - IMPLEMENT: the structural checker in §S (complete file, copy-paste-ready;
    stdlib only — re, sys, pathlib).
  - NAMING: exact name from the work-item contract.
  - PLACEMENT: test/check_setup_first_line.py (directly under test/, NOT a
    new PIO project dir — no .cpp ⇒ PIO ignores it; that is desired).
  - CHMOD +x and shebang #!/usr/bin/env python3 (nice-to-have; the CI
    command uses python3 explicitly).

Task 3: VALIDATE — run the full Validation Loop below, including the
  one-time negative controls (§N), then revert the planted violations and
  re-run to green.
```

### Implementation Patterns & Key Details

**§P — `test/test_invariants/test_invariants.cpp`** (complete file):

```cpp
// test/test_invariants/test_invariants.cpp — the §10 safety-invariant suite
// (P1.M7.T1.S1). Machine-proves, over seeded randomized event+sensor storms
// and fresh-instance (reset-proxy) scenarios against the REAL FillEngine:
//   P1  valveOpen HIGH only in FILLING (never Idle/Confirm/Done/Fault)
//   P2  §7.9: every Filling/Confirm exit step logs ValveClosed FIRST and
//       drives the valve LOW on that same step
//   P3  FILLING is entered ONLY via Idle+ShortPress (human BOOT press)
//   P4  valve HIGH anywhere implies a human start happened earlier
//   P5  FAULT latches and the valve refuses to open while faulted (§7)
//   P6  log capacity respected; output level == engine level
// plus: fresh engine = Idle/valve-LOW/no logs/freshBoot (§5.3 reset proxy,
// watchdog included), no-ShortPress storms NEVER fill, clock-wraparound
// safety (rollover-safe arithmetic, §10), and mid-fill reset → Idle+closed
// until a NEW press. Adversarial event streams (events possibly every pass)
// are a deliberate SUPERSET of the legal classifier output — the invariants
// must hold regardless. Backstop hugging (§10e) is P1.M4.T1.S1's table
// tests; bench watchdog/power proof is P1.M7.T2.S1's.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Same calibration language as test_fillengine.cpp: DRY=3500 is inside
// BASELINE_MIN..MAX (2000..4050); WET=2500 is below the minimum legal wet
// threshold (2000-350), so a wet emission always feeds detector streaks and
// the early-wet guard. Wild values exercise baseline faults and the rails.
static constexpr uint16_t DRY = 3500;
static constexpr uint16_t WET = 2500;

// Deterministic 32-bit LCG (Numerical Recipes constants) — house precedent
// (test_fillengine.cpp): bit-reproducible on any host g++; the seed IS the
// reproducibility contract.
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Adversarial sensor level: mostly dry, often wet, occasionally wild
// (baseline-breaking / rail values — forces BaselineFault paths too).
static uint16_t randomLevel(uint32_t& rng) {
    const uint32_t r = rngStep(rng) % 100u;
    if (r < 75u) return DRY;
    if (r < 92u) return WET;
    static const uint16_t wild[] = {0, 100, 1999, 2000, 4050, 4051, 4095};
    return wild[rngStep(rng) % 7u];
}

static ButtonClassifier::Event randomEvent(uint32_t& rng, bool allowShortPress) {
    const uint32_t r = rngStep(rng) % 1000u;
    if (allowShortPress && r < 55u) return ButtonClassifier::Event::ShortPress;
    if (r < 110u) return ButtonClassifier::Event::LongPress;
    return ButtonClassifier::Event::None;
}

// Property-harness: the scenario Sim adapted for property work —
// configurable start time (wraparound families) and a RANDOM per-pass
// stride (5..55 ms), so fills do NOT open on the reading grid and the
// invariants are proven order-of-event, off-grid. readingDue follows the
// loop contract: re-anchor + snapshot the emitted pair at emission.
struct PropSim {
    FillEngine eng;
    FillEngineOutputs last{};
    uint32_t now;
    uint32_t lastReadingMs;
    uint16_t a1 = DRY, a2 = DRY;  // scripted sensor levels
    uint16_t e1 = DRY, e2 = DRY;  // last EMITTED pair (what the loop routes)
    explicit PropSim(uint32_t start = 1000) : now(start), lastReadingMs(start) {}
    const FillEngineOutputs& pass(ButtonClassifier::Event ev, uint32_t dt) {
        const bool due = elapsedMs(now, lastReadingMs) >= READING_INTERVAL_MS;
        if (due) { lastReadingMs = now; e1 = a1; e2 = a2; }
        last = eng.step(now, ev, due, e1, e2);
        now += dt;
        return last;
    }
};

// The shared oracle: run one seeded storm and assert P1..P6 at EVERY step.
static void runStorm(uint32_t seed, int passes, uint32_t startNow,
                     bool allowShortPress) {
    uint32_t rng = seed;
    PropSim s(startNow);
    bool humanStart = false;
    for (int i = 0; i < passes; ++i) {
        const ButtonClassifier::Event ev = randomEvent(rng, allowShortPress);
        s.a1 = randomLevel(rng);
        s.a2 = randomLevel(rng);
        const uint32_t dt = 5u + rngStep(rng) % 51u;  // 5..55 ms stride
        const FillState before = s.eng.state();
        const FillEngineOutputs o = s.pass(ev, dt);
        const FillState after = s.eng.state();
        TEST_ASSERT_TRUE_MESSAGE(!o.valveOpen || after == FillState::Filling,
                                 "P1 valve HIGH outside FILLING");
        if (before != after &&
            (before == FillState::Filling || before == FillState::Confirm)) {
            TEST_ASSERT_TRUE_MESSAGE(o.logCount >= 1, "P2 exit with empty log");
            TEST_ASSERT_TRUE_MESSAGE(
                o.logs[0].kind == FillLogEvent::Kind::ValveClosed,
                "P2 exit step logs[0] != ValveClosed (§7.9)");
            TEST_ASSERT_FALSE_MESSAGE(o.valveOpen, "P2 valve HIGH on exit step");
        }
        if (after == FillState::Filling && before != FillState::Filling) {
            TEST_ASSERT_TRUE_MESSAGE(before == FillState::Idle &&
                                     ev == ButtonClassifier::Event::ShortPress,
                "P3 FILLING entered without Idle+ShortPress (auto-open)");
            humanStart = true;
        }
        if (o.valveOpen) TEST_ASSERT_TRUE_MESSAGE(humanStart, "P4 valve without human start");
        if (before == FillState::Fault) {
            TEST_ASSERT_TRUE_MESSAGE(after == FillState::Fault, "P5 FAULT unlatched");
            TEST_ASSERT_FALSE_MESSAGE(o.valveOpen, "P5 valve open while faulted");
        }
        TEST_ASSERT_TRUE_MESSAGE(o.logCount <= FILL_LOG_CAPACITY, "P6 log overflow");
        TEST_ASSERT_EQUAL_MESSAGE(s.eng.valveOpen(), o.valveOpen,
                                  "P6 output/engine valve level disagree");
    }
}

// ---- fresh instance = safe state (watchdog reset / power cycle, §5.3/§10)

void test_fresh_engine_boots_idle_valve_low() {
    PropSim s;
    const FillEngineOutputs o = s.pass(ButtonClassifier::Event::None, 25);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_FALSE(s.eng.valveOpen());
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_EQUAL_UINT8(0, o.faultCode);
    TEST_ASSERT_EQUAL_UINT8(0, o.logCount);
    TEST_ASSERT_TRUE(s.eng.deadline().freshBoot());  // §5.3: reset clears tank history
}

// §10/§5.3: with NO human BOOT press there is NO fill — whatever the
// sensors do. Long presses and wet storms never open the valve.
void test_no_shortpress_never_opens_valve() {
    for (int seq = 0; seq < 16; ++seq)
        runStorm(0xC0FFEEu + (uint32_t)seq * 40503u, 4000, 1000, false);
}

// The core storms: full adversarial event+sensor mix, thousands of steps.
void test_random_storms_invariants() {
    for (int seq = 0; seq < 48; ++seq)
        runStorm(0xF11EC0DEu + (uint32_t)seq * 2654435761u, 5000, 1000, true);
}

// Same properties with the clock starting near UINT32_MAX — the sequence
// crosses the millis() wrap: rollover-safe arithmetic must keep every
// invariant (§10). A failure here is a REAL engine bug — report it.
void test_wraparound_storms_invariants() {
    for (int seq = 0; seq < 8; ++seq)
        runStorm(0x0BADC0DEu + (uint32_t)seq * 2246822519u, 4000,
                 0xFFFFFF00u - (uint32_t)seq * 1000u, true);
}

// §7: faults latch; BOOT is ignored while faulted; the valve refuses to
// open. Storm the latched state with press bursts — nothing may give.
void test_fault_latch_valve_refuses() {
    PropSim s;
    s.pass(ButtonClassifier::Event::ShortPress, 25);
    s.a1 = 100; s.a2 = 100;  // baseline out of range -> BaselineFault latches
    while (s.eng.state() != FillState::Fault) s.pass(ButtonClassifier::Event::None, 25);
    for (int i = 0; i < 2000; ++i) {
        const FillEngineOutputs o = s.pass(
            (i % 7 == 0) ? ButtonClassifier::Event::ShortPress
                         : ButtonClassifier::Event::None, 25);
        TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
        TEST_ASSERT_FALSE(o.valveOpen);
        TEST_ASSERT_TRUE(o.faultCode >= 1 && o.faultCode <= 3);
    }
}

// §10 hang->reset path, native proxy: a fill is running with the valve OPEN,
// the device "resets" (fresh engine, RAM state gone) mid-stream — the fresh
// engine is Idle+closed under a WET adversarial stream, tank history is
// cleared (freshBoot), and only a NEW human press starts the next fill.
void test_reset_midfill_lands_idle_closed() {
    PropSim s;
    s.pass(ButtonClassifier::Event::ShortPress, 25);
    for (int i = 0; i < 400 && s.eng.state() == FillState::Filling; ++i)
        s.pass(ButtonClassifier::Event::None, 25);  // dry: settle -> baseline -> open
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);  // pre: fill alive
    TEST_ASSERT_TRUE(s.eng.valveOpen());                   // pre: valve OPEN at the "hang"
    PropSim t(s.now);              // fresh instance = the reset
    t.a1 = WET; t.a2 = WET;        // adversarial: wet tips at reboot
    for (int i = 0; i < 200; ++i) {
        const FillEngineOutputs o = t.pass(ButtonClassifier::Event::None, 25);
        TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);
        TEST_ASSERT_FALSE(o.valveOpen);
    }
    TEST_ASSERT_TRUE(t.eng.deadline().freshBoot());  // §5.3 fresh 30-min tank
    const FillEngineOutputs o2 = t.pass(ButtonClassifier::Event::ShortPress, 25);
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Filling);  // press still works
    TEST_ASSERT_FALSE(o2.valveOpen);  // §7.1: settle first — still closed this step
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_engine_boots_idle_valve_low);
    RUN_TEST(test_no_shortpress_never_opens_valve);
    RUN_TEST(test_random_storms_invariants);
    RUN_TEST(test_wraparound_storms_invariants);
    RUN_TEST(test_fault_latch_valve_refuses);
    RUN_TEST(test_reset_midfill_lands_idle_closed);
    return UNITY_END();
}
```

**§S — `test/check_setup_first_line.py`** (complete file):

```python
#!/usr/bin/env python3
"""Structural safety-invariant check (P1.M7.T1.S1, PRD §10).

Checks firmware/jug_filler/jug_filler.ino:
  A1  setup(): first executable statement is digitalWrite(PIN_VALVE, LOW);
  A2  loop():  first executable statement is watchdog_update();   [§10: fed
      every pass, before anything else can hang]
  A3  no delay( token outside comments and string/char literals, in the
      WHOLE file (strict superset of the required ARDUINO loop region — the
      pure region is delay-free by the seam rules anyway);
  A4  no `while (!Serial` gate anywhere (PRD §3: logging never blocks on
      USB presence).

Comments and literals are blanked (newlines preserved) before scanning, so
mentions like "zero delay()" in prose cannot false-positive. Stdlib only.
Exit 0 = pass, 1 = violations (each printed with a line number where known).
"""

import re
import sys
from pathlib import Path

SKETCH = (Path(__file__).resolve().parent.parent
          / "firmware" / "jug_filler" / "jug_filler.ino")


def strip_comments_and_strings(text):
    """Blank out // and /* */ comments and "..." / '...' literals, keeping
    newlines (line numbers survive). Single left-to-right scanner: whichever
    token opens first owns its span, so '"' inside a char literal and "'"
    inside a string are handled correctly."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":                       # line comment
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":                     # block comment
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n
                                 and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
        elif c in ('"', "'"):                             # string/char literal
            quote = c
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                else:
                    out.append("\n" if text[i] == "\n" else " ")
                    i += 1
            if i < n:
                out.append(" ")
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_body(code, name):
    """Lines inside { ... } of the DEFINITION `void name() { ... }`.
    Matches the definition, never the forward declaration `void name();`
    (which ends in ';' and has no brace)."""
    m = re.search(r"void\s+" + name + r"\s*\(\s*\)\s*\{", code)
    if not m:
        return None
    brace = code.find("{", m.start())
    depth, i = 1, brace + 1
    while i < len(code) and depth > 0:
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
        i += 1
    return code[brace + 1:i - 1].splitlines()


def first_executable_line(body_lines):
    for ln in body_lines:
        stripped = ln.strip()
        if stripped:
            return stripped
    return ""


def main():
    failures = []
    code = strip_comments_and_strings(SKETCH.read_text(encoding="utf-8"))

    setup_body = function_body(code, "setup")
    loop_body = function_body(code, "loop")

    # A1 — valve LOW is setup()'s very first executable statement (PRD §10)
    if setup_body is None:
        failures.append("A1: setup() definition not found")
    else:
        first = first_executable_line(setup_body)
        if not re.fullmatch(r"digitalWrite\s*\(\s*PIN_VALVE\s*,\s*LOW\s*\)\s*;", first):
            failures.append(
                "A1: setup() first executable statement is not "
                f"digitalWrite(PIN_VALVE, LOW); — got: {first!r}")

    # A2 — watchdog fed before anything else in loop() (PRD §10)
    if loop_body is None:
        failures.append("A2: loop() definition not found")
    else:
        first = first_executable_line(loop_body)
        if not re.fullmatch(r"watchdog_update\s*\(\s*\)\s*;", first):
            failures.append(
                "A2: loop() first executable statement is not "
                f"watchdog_update(); — got: {first!r}")

    # A3 — zero delay( outside comments/strings, whole file (see docstring)
    for lineno, ln in enumerate(code.splitlines(), start=1):
        if "delay(" in ln:
            failures.append(f"A3: delay( at line {lineno}: {ln.strip()!r}")

    # A4 — zero while (!Serial gates (PRD §3)
    if re.search(r"while\s*\(\s*!\s*Serial", code):
        failures.append("A4: while (!Serial present — logging must not block on USB")

    if failures:
        print("STRUCTURAL SAFETY CHECK: FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("STRUCTURAL SAFETY CHECK: PASS "
          "(setup line 1 valve LOW; loop line 1 watchdog; "
          "no delay(; no !Serial gate)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

**§N — one-time negative controls** (perform, observe FAIL, revert, re-run to green — proves the suite actually bites):

```bash
# Control 1 (structural A1/A3): plant two violations, expect both caught
sed -i 's/digitalWrite(PIN_VALVE, LOW);   \/\/ LINE 1/digitalWrite(PIN_LED, LOW);   \/\/ LINE 1/' firmware/jug_filler/jug_filler.ino
# and add `delay(5);` as the 2nd statement of loop() (any line after watchdog_update();)
python3 test/check_setup_first_line.py   # MUST print FAIL naming A1 and A3 -> exit 1
git checkout -- firmware/jug_filler/jug_filler.ino   # REVERT immediately

# Control 2 (native P2/P3): in a THROWAWAY edit, make the engine auto-open —
# e.g. in FillEngine::step change the fill-start guard
#   `if (before == FillState::Idle && state_ == FillState::Filling)`
# to `if (before != FillState::Filling)` style behavior, or simply force
# `out.valveOpen = true;` after the zero-init. Run:
pio test -e native -- -f test_invariants   # MUST fail P1/P3/P4 assertions
git checkout -- firmware/jug_filler/jug_filler.ino   # REVERT immediately
pio test -e native                          # green again before finishing
```

### Integration Points

```yaml
BUILD: NONE — platformio.ini untouched; test/test_invariants/ is auto-
  discovered as a Unity project by [env:native]; test/check_setup_first_line.py
  has no .cpp and is invisible to PIO (intended — it runs as its own command).
FIRMWARE: NONE — read-only consumer of jug_filler.ino (pure region symbols)
  and of its ARDUINO-region structure (A1..A4).
CONFIG: NONE — config.h is §8-frozen; the suite only READS
  READING_INTERVAL_MS / BASELINE_MIN..MAX semantics via DRY/WET constants.
CI (the work item's OUTPUT contract): the runnable gate is
  `pio test -e native && python3 test/check_setup_first_line.py`
  — both must exit 0. (No CI YAML exists in-repo; the command pair IS the
  contract consumed by future automation and by P1.M7.T2.S1's pre-bench check.)
FUTURE (P1.M7.T2.S1): runs this pair BEFORE flashing the bench; physical
  watchdog-reset behavior it validates on-device is proxied natively by
  test_reset_midfill_lands_idle_closed + test_fresh_engine_boots_idle_valve_low.
FUTURE (P2 refactors): every engine/loop change must keep this suite green —
  that is the suite's entire reason to exist.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# After creating each file:
pio test -e native -- -f test_invariants    # new suite compiles + runs
python3 test/check_setup_first_line.py      # script runs, exit 0
python3 -m py_compile test/check_setup_first_line.py   # syntax OK

# Project-wide regression (all 15 suites):
pio test -e native                           # exit 0, ALL [PASSED]

# NO ruff/mypy (C++/stdlib-Python project). NO pio test -e pico.
# Compile errors in the new suite usually mean an API-name typo — re-check
# the spellings in the Gotchas/research notes against the live .ino.
```

### Level 2: Property & Structural Validation (this task's core)

```bash
# The CI contract:
pio test -e native && python3 test/check_setup_first_line.py    # both exit 0

# Confirm the new suite is actually among the run (PIO prints per-project):
pio test -e native 2>&1 | grep -E "test_invariants|PASSED|FAILED"

# Confirm determinism (seeded: two runs identical — second run green too):
pio test -e native -- -f test_invariants && pio test -e native -- -f test_invariants

# Negative controls (§N): each planted violation FAILS, then revert to green.
```

### Level 3: Cross-Artifact Consistency

```bash
# The structural facts the script asserts are true in the real file:
sed -n '/^void setup()/,/^}/p' firmware/jug_filler/jug_filler.ino | head -3
  # first executable line: digitalWrite(PIN_VALVE, LOW);
sed -n '/^void loop()/,/^}/p' firmware/jug_filler/jug_filler.ino | head -3
  # first executable line: watchdog_update();

# Wiring prerequisite intact (P1.M6.T2.S1 landed, unmodified by this task):
grep -c "s_fill.step(" firmware/jug_filler/jug_filler.ino          # -> 1
grep -c "P1.M6.T2.S1 fills the loop below this line" firmware/jug_filler/jug_filler.ino  # -> 0

# Clean hands — tests only, per the work item:
git status --short
  # ONLY: ?? test/test_invariants/  and  ?? test/check_setup_first_line.py
  # (plus prior tasks' uncommitted work — leave it alone)
git diff firmware/jug_filler/ platformio.ini README.md             # -> EMPTY
```

### Level 4: Domain-Specific Validation

```bash
# Runtime sanity: the whole native run stays in the seconds range
# (≈336k engine steps ≈ same order as the existing 192k-step fuzz):
time pio test -e native

# If a wraparound-storm or storm assertion fails: it is PROBABLY a real
# engine invariant breach, not a flaky test (everything is seeded and
# deterministic). Reproduce with the printed seed family, minimize the
# sequence, report — never weaken P1..P6 to pass.
```

## Final Validation Checklist

### Technical Validation

- [ ] `pio test -e native` → exit 0, ALL projects `[PASSED]` (15 incl. `test_invariants`)
- [ ] `python3 test/check_setup_first_line.py` → exit 0, `STRUCTURAL SAFETY CHECK: PASS`
- [ ] Combined CI command green: `pio test -e native && python3 test/check_setup_first_line.py`
- [ ] Negative controls executed once each: planted violations FAIL, reverted, green restored
- [ ] `git status --short`: only the two new untracked files; zero diffs to firmware/config/ini/README/tests

### Feature Validation

- [ ] All §10-derived properties asserted at EVERY step of every seeded sequence (P1–P6)
- [ ] Fresh-instance tests prove reset lands `Idle` + valve LOW + `freshBoot()`; a new press still starts a fill (§5.3/§10)
- [ ] No-ShortPress storms never enter `Filling` and never open the valve
- [ ] Wraparound storms cross the `millis()` seam with all invariants held
- [ ] Structural A1–A4 all enforced by script with comment/string stripping (no false positives from prose mentions of `delay(`)

### Code Quality Validation

- [ ] House conventions: include pair, `setUp/tearDown`, LCG `rngStep`, Unity `main()` footer — mirrors `test/test_fillengine/test_fillengine.cpp`
- [ ] Suite is additive: sibling suites untouched; overlap with the scenario fuzz is intentional defense-in-depth, not duplication
- [ ] No new dependencies (stdlib Python; no Hypothesis/gtest); no config/build changes
- [ ] Test header documents scope boundaries (backstop table = P1.M4.T1.S1; bench watchdog = P1.M7.T2.S1)

### Documentation & Deployment

- [ ] Per the work item: DOCS = none — tests only (no README changes)
- [ ] Reproducibility: seeds are compile-time constants; failure repro procedure noted in Level 4

---

## Anti-Patterns to Avoid

- ❌ Don't use `rand()`/`time()`/thread RNG — determinism IS the contract; reuse the seeded LCG pattern.
- ❌ Don't weaken or delete an assertion to make a storm pass — a failure here means the SAFETY contract is broken; report it.
- ❌ Don't grep for `delay(` on the raw file — prose mentions exist in comments today; strip comments/strings first (that is the entire point of A3).
- ❌ Don't anchor the script on `find("void setup()")` — the forward declarations at the top of the .ino will capture the wrong block; regex the braced definition.
- ❌ Don't edit `jug_filler.ino`, `config.h`, `platformio.ini`, sibling test suites, or README — this task's entire output is two new files.
- ❌ Don't run/start this task if the Task 0 gates fail (red suite, or P1.M6.T2.S1 wiring absent) — STOP and report instead of building on a moving base.
- ❌ Don't import exact-time scripting helpers (`tripAt`) from the scenario suite — variable-Δt storms deliberately break the reading grid; property assertions are per-step only.
- ❌ Don't `pio test -e pico` (no on-device transport) and don't add upload targets — bench flashing is P1.M7.T2.S1's deliverable.
- ❌ Don't leave planted negative-control edits in the tree — revert and re-run to green before finishing.

---

**Confidence Score: 9/10** — every consumed API (`step` signature, `FillEngineOutputs`/`FillLogEvent` field names, `FillState` members, getters `state()/valveOpen()/deadline()`, `ButtonClassifier::Event`, `FILL_LOG_CAPACITY`, `elapsedMs`, `READING_INTERVAL_MS`) was read from the live source this session; complete copy-paste-ready code for both deliverables is provided and matched to the 14-suite house conventions; the structural facts (setup line 1, watchdog-first, comment-only `delay(` hits, forward-declaration trap) were verified in the working tree; validation commands are the repo's proven invocations. Residual risks: (a) the parallel P1.M6.T2.S1 landing shifts .ino line numbers and adds comments — mitigated by Task 0's re-grep gate and by the script being line-number-agnostic; (b) a wraparound-storm failure would indicate a genuine engine-side rollover bug — that is the suite doing its job, with an explicit report-don't-weaken rule; (c) Unity's `TEST_ASSERT_EQUAL_MESSAGE` bool comparison is standard, but if a PIO/Unity version quirk arises the equal-message asserts may be swapped for `TEST_ASSERT_TRUE_MESSAGE(a == b, ...)` without changing semantics.
