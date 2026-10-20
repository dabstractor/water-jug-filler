# PRP — P1.M6.T1.S3: Fault latching and blink codes 1/2/3 (pure)

## Goal

**Feature Goal**: Close PRD §7's fault contract completely inside `FillEngine`: every fault producer maps to blink code 1 (sensor 1), 2 (sensor 2) or 3 (timer, no trip) with the valve driven LOW as the FIRST action of the fault step; faults LATCH — no button event, sensor stream or timer tick ever leaves FAULT or opens the valve; the code is presented via `FillEngineOutputs.led == FaultBlink` + `faultCode == N` so `LedPatternEngine::setMode(o.led, o.faultCode)` renders PRD §7's exact cadence (N × on 250 ms / off 250 ms, then off 2000 ms, repeat); and RESET / power cycle — simulated by object re-construction — clears the fault AND the RAM tank history, landing in IDLE with no auto-start and a 30-min fresh-tank backstop on the next fill (PRD §5.3, §11).

**Deliverable**: ONE new native Unity test project — `test/test_faultlatch/test_faultlatch.cpp` (13 tests, copy-paste-ready below) — that proves the latching/blink/reset contract against the FillEngine block landed by P1.M6.T1.S2. **The `.ino` is expected to need NO change**: S2's engine already implements fault determination (`endFillFault`), structural latching (the §5.1 table ignores every event in Fault) and the `faultCode` level output; this task closes the behavioral contract by verifying it exhaustively and covering the three gaps S2's suite does not test (full button absorption, blink-cadence presentation, reset/history-wipe). A conditional remediation task exists ONLY if Task-0 verification finds the landed code deviates from the S2 PRP contract.

**Success Definition**: `pio test -e native` → exit 0, **14/14 projects `[PASSED]`** (13 existing incl. `test_fillengine` + new `test_faultlatch`); `pio run -e pico` → `[SUCCESS]`; deterministic re-run of `test_faultlatch` passes identically (seeded fuzz); `git status --short` → only `?? test/test_faultlatch/` (primary path: `firmware/` untouched, `test/test_fillengine/` untouched, config.h/platformio.ini untouched).

## User Persona

**Target User**: The implementing agent of P1.M6.T2.S1 (hardware loop) and P1.M7.T1.S1 (safety-invariant suite); the human bench operator indirectly (fault blink codes are the device's only diagnostic channel — PRD §7 table).

**Use Case**: On the bench, a fill fails (wet tips at start, false trip, unconfirmed second sensor, or 30-min backstop with no trip). The device latches FAULT, blinks the green LED in groups of 1, 2 or 3 with a 2 s pause, ignores every BOOT press, and refuses to open the valve until the operator hits RESET — after which the next fill runs the 30-min fresh-tank default.

**User Journey**: fill faults → LED blinks N groups → operator presses BOOT (nothing happens; still blinking) → operator presses RESET → device lands IDLE, LED off, tank history wiped → operator short-presses BOOT → new fill with the 30-min default backstop.

**Pain Points Addressed**: a fault code that doesn't latch (or worse, restarts a fill into a wet jug), a blink group count that's off-by-one or clamped wrong, and a reset that "remembers" a recovery time it has no right to remember (no persistence, §11).

## Why

- PRD §7's fault table (codes 1/2/3), the fault cadence line, "**Faults latch**… BOOT is ignored while faulted", and "While faulted, the valve refuses to open" are the device's entire diagnostic + safety story after a failed fill. §10's non-negotiable valve invariant is only as strong as its weakest (fault) exit.
- PRD §5.3 + §11: RESET/power cycle is a FULL RAM wipe — fault cleared, tank history cleared, next fill = 30-min default, never auto-starts. In this firmware "reset" is literally object re-construction (no persistence layer exists by design); that equivalence must be proven, not assumed.
- This is the last pure-logic item before hardware integration (P1.M6.T2.S1 consumes `o.led`/`o.faultCode` → `setMode`); every behavior T2.S1 will wire must already be machine-proven here.

## What

Create `test/test_faultlatch/test_faultlatch.cpp` (14th native project — auto-discovered, no `platformio.ini` edit) containing the 13-test suite from the Implementation Blueprint, verbatim. The suite covers:

1. **§7 fault table, every producer of every code**: baseline out-of-range (sensor 1 → code 1, valve NEVER opened, also `logs[0] == ValveClosed` §7.9; sensor 2 → code 2), confirm-window miss (each direction: first-tripper's counterpart faulted — code 2 when sensor 1 tripped first, code 1 when sensor 2 tripped first), false-trip guards (early-wet AND early-trip, each sensor → that sensor's code), backstop expiry with no trip → code 3.
2. **Latching**: the complete `ButtonClassifier::Event` vocabulary `{None, ShortPress, LongPress}` cycled across a 3-minute faulted soak with hostile wet sensor streams — state, code, LED mode, valve level and log silence frozen on every pass; plus the seeded fuzz absorption property (8 sequences × 2000 random-event passes can never escape FAULT, open the valve, change the code, or emit one log event).
3. **Blink-code presentation**: the exact T2.S1 wiring (`setMode(o.led, o.faultCode)` once, then `tick`) reproduces the §7 cadence ms-exactly for codes 1, 2 and 3.
4. **§5.3/§11 reset**: a faulted engine WITH tank history is replaced by a fresh instance (wall clock continues) → IDLE, fault code 0, valve closed, LED off; 5 minutes with no press → still IDLE (never auto-starts); next fill → `freshBoot()` true, `tableMinutes() == 30 == FRESH_BOOT_BACKSTOP_MIN`, `recoveryMsAtStart() == 0`.

**NOT in scope** (owners): any ARDUINO-region change / pin writes (P1.M6.T2.S1), invariant-suite authoring beyond this suite (P1.M7.T1.S1), on-bench validation (P1.M7.T2.S1), config.h changes (§8 frozen), platformio.ini changes, modifying `test/test_fillengine/` (owned by P1.M6.T1.S2), docs (item contract: internal).

### Success Criteria

- [ ] `pio test -e native` → exit 0, 14/14 projects `[PASSED]`; `test_faultlatch` = 13 tests
- [ ] All §7 producers verified: baseline s1/s2, confirm-miss both directions, early-wet s1/s2, early-trip s1/s2 → codes 1/2; backstop → code 3; valve-closed-first (§7.9) asserted on fault entry
- [ ] Fault absorption: every event in the vocabulary ignored across a long soak AND seeded fuzz; valve never opens while faulted; zero log events from button input in Fault
- [ ] Blink cadence exact for codes 1/2/3: N × (250 on/250 off), 2000 ms gap, cycle repeats (sampled at mid-on / just-off / mid-gap / gap-end instants)
- [ ] Reset: fresh instance → Idle, no fault, history wiped → next fill 30 min (fresh-boot default), no auto-start after 5 idle minutes
- [ ] `pio run -e pico` → `[SUCCESS]`; deterministic re-run; primary-path diff = untracked `test/test_faultlatch/` only

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete copy-paste-ready test file is in the Blueprint; every consumed API (`FillEngine` outputs/accessors, `LedPatternEngine::setMode/tick/faultGroups`, `ButtonClassifier::Event`, `RecoveryClock`/`FillDeadline` accessors, config constants) was read from the live source this session; the S2 block was verified landed at .ino lines 993–1281 with all fault surfaces present; all validation commands are verified-working repo invocations.

### Documentation & References

```yaml
- file: firmware/jug_filler/jug_filler.ino
  why: THE file under test (and the ONLY file you may conditionally remediate).
        (1) S2's FillEngine block, lines ~993–1281: step() routing (readings fed
        ONLY in Filling/Confirm), endFillFault() (valve LOW first -> ValveClosed
        log -> state=Fault -> code 3 or sensor+1), faultCode_=0 reset only at
        fill start, out.faultCode level ((state==Fault) ? faultCode_ : 0).
        (2) S1 core, lines ~866–986: fillApplyButton's Fault row is `break;`
        (both events ignored) — latching is STRUCTURAL; fillLevelOutputs(Fault)
        -> Mode::FaultBlink (group count rides on faultCode, not the mode).
        (3) LedPatternEngine, lines ~748–864: setMode(Mode, faultGroups=1),
        faultGroups(), tick(nowMs) — phase anchors at the FIRST tick after
        setMode; FaultBlink = n*(250 on/250 off) then 2000 off, forever.
        (4) RecoveryClock/FillDeadline, lines ~637–710: fresh instance ->
        hasHistory()==false -> beginFill -> freshBoot + FRESH_BOOT_BACKSTOP_MIN.
  gotcha: do NOT touch the #ifdef ARDUINO region; do NOT restructure the S2
        block (remediation is surgical, and only if Task 0 fails).

- file: plan/001_53984f832581/P1M6T1S2/PRP.md
  why: BINDING CONTRACT for the engine under test. Its Integration Points
        section names THIS task: "consumes faultCode()/out.faultCode for
        FaultBlink groups 1..3 and hardens/verifies latching; this engine
        already determines the code at Fault entry and latches structurally
        (table ignores presses in Fault; reset = re-construction)".
  section: "Implementation Patterns & Key Details" (full engine source),
        "Integration Points" (FAULT LATCH hand-off).

- file: test/test_fillengine/test_fillengine.cpp
  why: the harness this suite MIRRORS (Sim struct, tripAt helper, LCG, timing
        math: press at t=1000, emissions at t==0 mod 100, baseline verdict at
        t=3400, DRY=3500/WET=2500). READ it; do NOT modify it (owned by S2).
  pattern: 25 ms Sim passes; readings(n); pressAndOpen(); tripAt(s, sensor, ms).

- file: test/test_deadline/test_deadline.cpp
  why: house Unity runner pattern — explicit main() with RUN_TEST lines
        (PlatformIO 6.2.0 generates NO auto runner).
  pattern: setUp/tearDown empty; UNITY_BEGIN/RUN_TEST.../UNITY_END.

- file: firmware/jug_filler/config.h
  why: the frozen §8 constants the suite consumes: FRESH_BOOT_BACKSTOP_MIN=30,
        CONFIRM_WINDOW_S=12, READING_INTERVAL_MS=100, BASELINE_MIN=2000,
        BASELINE_MAX=4050, FAULT_BLINK_MS=250, FAULT_PAUSE_MS=2000.
  gotcha: READ-ONLY; widen second-constants BEFORE x1000.

- file: plan/001_53984f832581/P1M6T1S3/research/notes.md
  why: this task's research: verified codebase state (post-S2-landing line
        evidence for every fault surface), S2/S3 non-duplication analysis,
        test-timing math, risk list.

- url: https://docs.throwtheswitch.org/Unity/Unity-Assertions-Reference.html
  why: assertion macros (all exercised by 13 green sibling suites).
  critical: TEST_ASSERT_EQUAL_UINT8/UINT32(expected, actual) argument order;
        enum-class equality via TEST_ASSERT_TRUE(a == b).
```

### Current Codebase tree (verified this session)

```bash
water-jug-filler/
├── platformio.ini            # [env:native] Unity host tests — 13 projects green
│                             #   after S2; [env:pico] compile-only. NO changes needed
├── firmware/jug_filler/
│   ├── jug_filler.ino        # ~1283+ lines: pure region … S1 core (866–986) …
│   │                         #   S2 FillEngine (993–1281) … #ifdef ARDUINO region (1283+)
│   └── config.h              # §8 frozen — READ-ONLY
├── test/                     # 13 Unity projects (… test_fillcore, test_fillengine)
└── plan/001_53984f832581/    # read-only plan material (this PRP's neighbors)
# Toolchain: PlatformIO Core 6.2.0 (`pio`); native = host g++ + Unity 2.6.1.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # EXPECTED UNCHANGED (conditional surgical remediation
│                             #   inside the S2 banner only, and only if Task 0 fails)
└── test/
    └── test_faultlatch/      # NEW — 14th native project (auto-discovered, no ini edit)
        └── test_faultlatch.cpp  # 13 tests: §7 fault table, latching/absorption,
                                 #   blink cadence 1/2/3, §5.3 reset/history wipe
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — scope discipline: this task's charter is "extend FillEngine" but
//   the S2 engine ALREADY implements the full latching contract. The expected
//   diff is test-only. Touch the .ino ONLY via the conditional Task 1R, and
//   only inside the S2 banner (993–1281). Never edit test_fillengine/.

// CRITICAL — setMode carries the code: the Fault group count does NOT ride on
//   Mode::FaultBlink (a plain enum); it rides on the SECOND setMode arg.
//   T2.S1 wiring = led.setMode(o.led, o.faultCode). n==0 clamps to 1 inside
//   LedPatternEngine (fail-safe) — so assert the engine emits exactly 1..3.

// CRITICAL — LedPatternEngine phase anchors at the FIRST tick after setMode:
//   cadence instants in the test are relative to that first tick's nowMs, not
//   to the setMode call's timestamp.

// CRITICAL — wet baselines are PLAUSIBLE: WET=2500 sits inside
//   BASELINE_MIN..MAX (2000..4050). A "hostile wet" stream during SETTLE can
//   produce a legal (if wrong) baseline and a 30-min run to backstop fault.
//   For FAST deterministic fault entry use out-of-range levels (TOO_LOW=1000
//   < 2000, TOO_HIGH=4200 > 4050) — verdict fires ~2.4 s after press.

// CRITICAL — the fuzz entry phase must be deterministic-fast: random button
//   events during a fill mostly ABORT/restart it, and random wet streams can
//   be baseline-plausible -> 30-min backstop. Entry = faultBaseline(seq%2);
//   the PROPERTY under fuzz is ABSORPTION after the latch, not entry variety
//   (entry variety is already covered by S2's fuzz + tests 1–9 here).

// GOTCHA — 30-min backstop sims are ~72k passes; two of them (tests 5 and 11)
//   run in well under a second natively. If the suite suddenly takes seconds,
//   a test loop is spinning — investigate, do not raise timeouts.

// GOTCHA — Unity on PIO 6.2.0 has NO auto runner: explicit main() with
//   RUN_TEST lines is REQUIRED.

// GOTCHA — widen seconds constants BEFORE multiplying:
//   (uint32_t)CONFIRM_WINDOW_S * 1000u.

// GOTCHA — never `pio test -e pico`. `pio run -e pico` is the compile gate;
//   on-hardware validation is P1.M7.T2.S1's deliverable.

// GOTCHA — tests must not include each other: each suite #includes the .ino
//   directly (platformio.ini excludes src_dir from native builds for exactly
//   this reason) — copy the Sim harness; do not share a header.
```

## Implementation Blueprint

### Data models and structure

No new models — the suite exercises the existing `FillEngine`/`FillEngineOutputs`/`FillLogEvent` (S2) and `LedPatternEngine` (P1.M5.T1.S1). Pure C++ / Unity; no ORM/Pydantic.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY the S2 contract landed (do not skip; expected: all pass)
  - RUN: pio test -e native
    → 13/13 projects [PASSED] (incl. test_fillengine). If test_fillengine is
    red/absent, STOP: S2 has not landed — report, do not work around it.
  - RUN: grep -n "end fill lifecycle engine\|#ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
    → the end banner exists BEFORE the ARDUINO region marker.
  - RUN: grep -c "class FillEngine" firmware/jug_filler/jug_filler.ino → 1
  - RUN: grep -n "endFillFault\|out.faultCode\|faultCode_ = 0\|uint8_t faultCode() const" firmware/jug_filler/jug_filler.ino
    → all four present inside the S2 block: endFillFault sets valveOpen_ = false
    BEFORE state_ = Fault and logs ValveClosed first; out.faultCode is
    (state_ == Fault) ? faultCode_ : 0; faultCode_ resets only at fill start.
  - RUN: grep -n -A3 "case FillState::Fault:" firmware/jug_filler/jug_filler.ino
    → the fillApplyButton Fault row is `break;` (events ignored — structural latch).
  - RUN: grep -n "void setMode(Mode mode, uint8_t faultGroups" firmware/jug_filler/jug_filler.ino
    → LedPatternEngine::setMode takes the group count.
  → If EVERY check passes: skip Task 1R (primary path). If any fails: S2
    deviated from its PRP — run Task 1R surgically, then continue.

Task 1R (CONDITIONAL — only if Task 0 found a deviation): REMEDIATE inside the
  S2 bannered block ONLY (firmware/jug_filler/jug_filler.ino, lines ~993–1281):
  - FIX the specific deviation against the S2 PRP's "Implementation Patterns &
    Key Details" (the authoritative engine source), keeping the banner text,
    the step() section order and every public signature byte-identical.
  - NEVER: restructure the block, change FillEngineOutputs/FillLogEvent field
    order, touch the ARDUINO region, or "improve" unrelated code.
  - RE-RUN: pio test -e native → test_fillengine must STILL pass 20/20.

Task 2: CREATE test/test_faultlatch/test_faultlatch.cpp
  - CONTENT: the COMPLETE file from "Implementation Patterns & Key Details".
  - FOLLOW pattern: test_fillengine.cpp (Sim harness, ITEM CASE banners,
    seeded LCG) + test_deadline.cpp (explicit RUN_TEST main).
  - NAMING: test_{subject}_{scenario}; helper faultBaseline/checkBlinkCadence.
  - PLACEMENT: test/test_faultlatch/ (auto-discovered as the 14th project).

Task 3: VALIDATE (Validation Loop) — 14/14 native, deterministic re-run,
  pico compile, RUN_TEST count, git status.
```

### Implementation Patterns & Key Details

**Complete test file — copy-paste-ready** (`test/test_faultlatch/test_faultlatch.cpp`):

```cpp
// test/test_faultlatch/test_faultlatch.cpp — native contract proof for the
// FAULT half of the fill engine (P1.M6.T1.S3): PRD §7's fault table closed
// end to end — every producer of codes 1/2/3 (baseline out of range, false-
// trip guards, confirm-window miss, backstop expiry), the LATCHING semantics
// (all button input ignored while faulted; the valve refuses to open; only
// re-construction = RESET/power cycle clears), the blink-code PRESENTATION
// handoff (FillEngineOutputs.led/.faultCode -> LedPatternEngine::setMode ->
// the §7 cadence n x 250/250 then 2000 off), and §5.3/§11's reset contract
// (fresh instance = RAM wipe: fault gone, tank history gone, next fill = the
// 30-min fresh-tank default, never auto-starts).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- deterministic loop simulator (mirrors test_fillengine's harness) ----
// 25 ms passes; reading emissions anchored at lastReadingMs=0 land on
// t == 0 (mod 100). A press at t=1000 => the baseline accepts pairs from
// t=3000 (inclusive 2000 ms settle), 5 pairs => the verdict fires at t=3400:
// valve opens (both plausible) or the baseline fault latches (out of range).
static constexpr uint16_t DRY     = 3500;  // inside BASELINE_MIN..MAX (2000..4050)
static constexpr uint16_t WET     = 2500;  // below DRY - TRIP_DROP_COUNTS (3150)
static constexpr uint16_t TOO_LOW  = 1000; // < BASELINE_MIN -> TooLow verdict
static constexpr uint16_t TOO_HIGH = 4200; // > BASELINE_MAX -> TooHigh verdict

struct Sim {
    FillEngine eng;
    FillEngineOutputs last{};
    uint32_t now = 1000;          // first press lands here
    uint32_t lastReadingMs = 0;   // emission cadence anchor
    uint16_t a1 = DRY, a2 = DRY;  // scripted sensor levels
    uint16_t e1 = DRY, e2 = DRY;  // last EMITTED pair (what the loop routes)

    const FillEngineOutputs& pass(ButtonClassifier::Event ev = ButtonClassifier::Event::None) {
        const bool due = elapsedMs(now, lastReadingMs) >= READING_INTERVAL_MS;
        if (due) {  // the loop re-anchors + snapshots at emission
            lastReadingMs = now;
            e1 = a1;
            e2 = a2;
        }
        last = eng.step(now, ev, due, e1, e2);
        now += 25;  // 4 passes per 100 ms reading
        return last;
    }
    const FillEngineOutputs& run(uint32_t ms) {
        const uint32_t end = now + ms;
        while (now < end) pass();
        return last;
    }
    void readings(int n) {
        for (int i = 0; i < n; ++i) {
            const uint32_t target = lastReadingMs + READING_INTERVAL_MS;
            while (now <= target) pass();
        }
    }
    uint32_t pressAndOpen() {
        pass(ButtonClassifier::Event::ShortPress);
        while (!eng.valveOpen()) pass();
        return eng.valveOpenedAtMs();
    }
};

// Deterministic 32-bit LCG (house reproducibility contract — the seed IS the
// reproducibility contract; never <random>).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Make `sensor` (0/1) trip with the 8th consecutive wet emission landing at
// EXACTLY fillTime == tripFillMs (a multiple of 100; t_open always is).
static void tripAt(Sim& s, int sensor, uint32_t tripFillMs) {
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_EQUAL_UINT32(0, tOpen % READING_INTERVAL_MS);  // alignment sanity
    const uint32_t tEnd = tOpen + tripFillMs;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();
    if (sensor == 0) s.a1 = WET; else s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);
}

// Fastest deterministic fault: out-of-range baseline on `sensor`, verdict at
// ~2.4 s after the press. (WET is baseline-PLAUSIBLE — never use it for fast
// fault entry; see the file-header gotcha in the PRP.)
static void faultBaseline(Sim& s, int sensor) {
    if (sensor == 0) s.a1 = TOO_LOW; else s.a2 = TOO_HIGH;
    s.pass(ButtonClassifier::Event::ShortPress);
    while (s.eng.state() == FillState::Filling) s.pass();
}

// ---- §7 fault table: every producer of every code -------------------------

// ITEM CASE: baseline out of range on sensor 1 -> code 1, valve NEVER opened
// (through the fault instant and 100 s of faulted life after — §7.1 "refuse
// to open"), ValveClosed logged FIRST (§7.9), FaultBlink + code 1 as levels.
void test_code1_baseline_s1_valve_never_opened() {
    Sim s;
    faultBaseline(s, 0);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(1, s.last.faultCode);   // level output
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9 first
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);                          // sensor 1
    bool everOpen = false;
    for (int i = 0; i < 4000; ++i) everOpen |= s.pass().valveOpen;  // 100 s faulted
    TEST_ASSERT_FALSE(everOpen);
}

// ITEM CASE: symmetric — baseline out of range on sensor 2 -> code 2.
void test_code2_baseline_s2() {
    Sim s;
    faultBaseline(s, 1);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(2, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);  // sensor 2
}

// ITEM CASE: sensor 1 trips first, sensor 2 NEVER trips in the confirm
// window -> code 2 (§7.5 "fault that sensor" = the one that never tripped).
void test_code2_confirm_miss_s2() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);                 // legal first trip (>= 60 s)
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.run((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);  // widen BEFORE x1000
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);  // sensor 2 never tripped
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: symmetric direction — sensor 2 trips first, sensor 1 never
// trips -> code 1 (completes the confirm-miss row of the §7 table).
void test_code1_confirm_miss_s1() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 60u * 1000u);                 // sensor 2 first
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.run((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);  // sensor 1 never tripped
}

// ITEM CASE: wet reading inside EARLY_WET_WINDOW_S on sensor 1 -> guard
// faults sensor 1 -> code 1 (§7.6 false trip / short).
void test_code1_early_wet_s1() {
    Sim s;
    s.pressAndOpen();
    s.a1 = WET;                                // set AFTER open: baseline stays dry
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: symmetric — early wet on sensor 2 -> code 2.
void test_code2_early_wet_s2() {
    Sim s;
    s.pressAndOpen();
    s.a2 = WET;
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: trip accepted at 59.8 s (< TRIP_MIN_ELAPSED_S) on sensor 1 ->
// false trip -> code 1 (guards run BEFORE the trip check: fault, not CONFIRM).
void test_code1_early_trip_s1() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
}

// ITEM CASE: symmetric — early trip on sensor 2 -> code 2.
void test_code2_early_trip_s2() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: backstop expires with no trip -> code 3 ("timer, no trip"),
// ValveClosed first, valve level low, FaultBlink + 3 as levels. 30-min sim
// (~72k passes — sub-second natively).
void test_code3_backstop_expiry() {
    Sim s;
    s.pressAndOpen();
    s.run(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(3, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BackstopExpired);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- latching: FAULT absorbs ALL input (§7 "BOOT is ignored… valve
//      refuses to open") -----------------------------------------------------

// ITEM CASE: the COMPLETE ButtonClassifier event vocabulary, cycled across a
// 3-minute faulted soak with hostile wet sensor streams — state, code, LED
// mode, valve level, log silence and edge silence frozen on EVERY pass.
void test_fault_ignores_every_button_event_forever() {
    Sim s;
    faultBaseline(s, 0);
    const uint8_t code = s.eng.faultCode();
    static const ButtonClassifier::Event kEvents[] = {
        ButtonClassifier::Event::None,
        ButtonClassifier::Event::ShortPress,
        ButtonClassifier::Event::LongPress,
    };
    s.a1 = WET;  s.a2 = WET;  // hostile: wet sensors while faulted
    for (int i = 0; i < 7200; ++i) {          // 180 s at 25 ms
        const FillEngineOutputs& o = s.pass(kEvents[i % 3]);
        TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);   // latched
        TEST_ASSERT_EQUAL_UINT8(code, s.eng.faultCode());      // code kept
        TEST_ASSERT_EQUAL_UINT8(code, o.faultCode);            // level out
        TEST_ASSERT_FALSE(o.valveOpen);                        // refuses to open
        TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
        TEST_ASSERT_EQUAL_UINT8(0, o.logCount);                // fully silent
        TEST_ASSERT_FALSE(o.plus5Applied || o.acknowledged || o.ackBlink);
    }
}

// ---- blink-code presentation: the §7 cadence via the T2.S1 wiring ---------

// Drive a fresh LedPatternEngine EXACTLY as P1.M6.T2.S1 will (setMode(o.led,
// o.faultCode) ONCE, then tick every pass) and verify PRD §7's cadence
// ms-exactly: N x (on 250 / off 250), then off 2000, repeat forever. The
// pattern phase anchors at the FIRST tick after setMode (engine contract).
static void checkBlinkCadence(Sim& s) {
    const FillEngineOutputs& o = s.pass();     // steady faulted levels
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    const uint8_t n = o.faultCode;
    TEST_ASSERT_TRUE(n >= 1 && n <= 3);        // §7 table domain
    LedPatternEngine led;
    led.setMode(o.led, o.faultCode);           // the wiring under proof
    TEST_ASSERT_TRUE(led.mode() == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_EQUAL_UINT8(n, led.faultGroups());
    const uint32_t t0 = 5000;                  // first tick anchors the phase
    TEST_ASSERT_TRUE(led.tick(t0));            // anchor lands ON (blink 1)
    const uint32_t cycle = (uint32_t)n * 2u * FAULT_BLINK_MS + FAULT_PAUSE_MS;
    for (uint32_t cyc = 0; cyc < 2u; ++cyc) {
        const uint32_t base = t0 + cyc * cycle;
        for (uint8_t k = 0; k < n; ++k) {      // blink k of this cycle
            const uint32_t blinkAt = base + (uint32_t)k * 2u * FAULT_BLINK_MS;
            TEST_ASSERT_TRUE(led.tick(blinkAt + 200));                    // mid-on
            TEST_ASSERT_FALSE(led.tick(blinkAt + FAULT_BLINK_MS + 25));   // just off
            TEST_ASSERT_FALSE(led.tick(blinkAt + 2u * FAULT_BLINK_MS - 25)); // end off
        }
        const uint32_t pauseAt = base + (uint32_t)n * 2u * FAULT_BLINK_MS;
        TEST_ASSERT_FALSE(led.tick(pauseAt + 1000));                      // mid-gap
        TEST_ASSERT_FALSE(led.tick(pauseAt + FAULT_PAUSE_MS - 25));       // gap end
    }
    TEST_ASSERT_TRUE(led.tick(t0 + 2u * cycle));  // cycle 3 restarts: ON
}

// ITEM CASE: codes 1, 2 and 3 each render the §7 cadence through the engine
// outputs — code 3 via the (slow but native-cheap) backstop path.
void test_fault_blink_cadence_codes_1_2_3() {
    Sim sa;
    faultBaseline(sa, 0);
    TEST_ASSERT_EQUAL_UINT8(1, sa.eng.faultCode());
    checkBlinkCadence(sa);
    Sim sb;
    faultBaseline(sb, 1);
    TEST_ASSERT_EQUAL_UINT8(2, sb.eng.faultCode());
    checkBlinkCadence(sb);
    Sim sc;
    sc.pressAndOpen();
    sc.run(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(sc.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, sc.eng.faultCode());
    checkBlinkCadence(sc);
}

// ---- §5.3 / §11: RESET & power cycle == re-construction --------------------

// ITEM CASE: a faulted engine WITH tank history is replaced by a fresh
// instance (wall clock continues): IDLE, fault cleared, history WIPED — the
// next fill uses the 30-min fresh-tank default, NOT the recovery-interpolated
// backstop (~1-min recovery would give 44; 30 is reachable ONLY fresh). The
// fresh instance also NEVER auto-starts: 5 minutes of hostile idle, no press.
void test_reset_clears_fault_and_tank_history() {
    Sim s;
    s.pass(ButtonClassifier::Event::ShortPress);  // fill 1 starts (settle)
    s.pass(ButtonClassifier::Event::ShortPress);  // abort: records history
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory()); // RAM history exists
    s.run(60000);                                 // 1 min of recovery
    faultBaseline(s, 0);                          // fill 2 -> fault, code 1
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory());

    Sim t;                       // POWER CYCLE: fresh engine, clock continues
    t.now = s.now;
    const FillEngineOutputs& o0 = t.pass();
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);  // §5.3: lands IDLE
    TEST_ASSERT_FALSE(o0.valveOpen);                    // valve closed
    TEST_ASSERT_EQUAL_UINT8(0, o0.faultCode);           // fault cleared
    TEST_ASSERT_TRUE(o0.led == LedPatternEngine::Mode::Off);
    t.a1 = WET;  t.a2 = WET;                            // hostile idle streams
    t.run(5u * 60000u);                                 // 5 min, no press
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle); // never auto-starts
    TEST_ASSERT_FALSE(t.last.valveOpen);
    TEST_ASSERT_FALSE(t.eng.clock().hasHistory());      // §5.3: history wiped
    t.a1 = DRY;  t.a2 = DRY;
    t.pass(ButtonClassifier::Event::ShortPress);        // the next fill
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Filling);
    TEST_ASSERT_TRUE(t.eng.deadline().freshBoot());
    TEST_ASSERT_EQUAL_UINT32(FRESH_BOOT_BACKSTOP_MIN, t.eng.deadline().tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(30, t.eng.deadline().tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(0, t.eng.deadline().recoveryMsAtStart());
}

// ---- fuzz: the latch absorbs arbitrary input; reset always lands IDLE -----

// ITEM CASE: seeded fuzz — from a latched fault, NO random input sequence
// (button events + wet/dry sensor scripts, 2000 passes) can leave FAULT,
// open the valve, change the code, or emit a single log event; a fresh
// instance then starts IDLE with the valve closed. Complements
// test_fillengine's fuzz law E (fault entry preserved) with ABSORPTION.
// Entry is deterministic-fast (baseline fault; random entry scripts mostly
// abort/restart or run to a 30-min backstop — entry variety is tests 1–9).
void test_fuzz_fault_absorbs_all_input() {
    const uint32_t SEED_BASE = 0xFA17C0DEu;
    const int SEQUENCES = 8;
    const int SOAK = 2000;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;
        Sim s;
        faultBaseline(s, seq % 2);            // codes 1 and 2 across seeds
        const uint8_t code = s.eng.faultCode();
        TEST_ASSERT_TRUE(code == 1 || code == 2);
        for (int i = 0; i < SOAK; ++i) {
            const uint32_t r = rngStep(rng) % 1000u;
            ButtonClassifier::Event ev = ButtonClassifier::Event::None;
            if (r < 150u) ev = ButtonClassifier::Event::ShortPress;
            else if (r < 300u) ev = ButtonClassifier::Event::LongPress;
            s.a1 = (rngStep(rng) % 100u < 50u) ? WET : DRY;
            s.a2 = (rngStep(rng) % 100u < 50u) ? WET : DRY;
            const FillEngineOutputs o = s.pass(ev);
            TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);  // latched
            TEST_ASSERT_EQUAL_UINT8(code, o.faultCode);           // code frozen
            TEST_ASSERT_FALSE(o.valveOpen);                       // refuses
            TEST_ASSERT_EQUAL_UINT8(0, o.logCount);               // silent
        }
        Sim t;                                 // RESET: re-construction
        t.now = s.now;
        TEST_ASSERT_FALSE(t.pass().valveOpen);
        TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_code1_baseline_s1_valve_never_opened);
    RUN_TEST(test_code2_baseline_s2);
    RUN_TEST(test_code2_confirm_miss_s2);
    RUN_TEST(test_code1_confirm_miss_s1);
    RUN_TEST(test_code1_early_wet_s1);
    RUN_TEST(test_code2_early_wet_s2);
    RUN_TEST(test_code1_early_trip_s1);
    RUN_TEST(test_code2_early_trip_s2);
    RUN_TEST(test_code3_backstop_expiry);
    RUN_TEST(test_fault_ignores_every_button_event_forever);
    RUN_TEST(test_fault_blink_cadence_codes_1_2_3);
    RUN_TEST(test_reset_clears_fault_and_tank_history);
    RUN_TEST(test_fuzz_fault_absorbs_all_input);
    return UNITY_END();
}
```

**Test-math notes**: the harness timing is identical to `test_fillengine`'s (press at t=1000; emissions at t ≡ 0 mod 100; baseline verdict at t=3400, ~2.4 s after press — the fastest deterministic fault). `tripAt` internally asserts the emission-grid alignment (`tOpen % 100 == 0`) so exact-boundary trips fail loudly rather than silently testing the wrong edge. The blink-cadence check samples at mid-on (+200), just-off (+275), end-of-slot (+475), mid-gap (+1000) and gap-end (−25) instants — every boundary verified without off-by-one tolerance. The reset test's discriminating assert is `tableMinutes() == 30` with `freshBoot() == true`: after a 60-s recovery the interpolator would grant 44, so 30 is provably the fresh-tank default, not a coincidence.

### Integration Points

```yaml
LOOP (P1.M6.T2.S1 — the consumer; do NOT implement here):
  - fault presentation wiring proven by this suite: on (o.led, o.faultCode)
    CHANGE call "s_led.setMode(o.led, o.faultCode)" — FaultBlink carries the
    group count as the SECOND arg; levels are stable while faulted (asserted
    every pass by the absorption tests), so setMode fires exactly once.
  - valve: "digitalWrite(PIN_VALVE, o.valveOpen ? HIGH : LOW)" — false on
    every faulted pass (asserted here); structurally guaranteed because
    fillApplyButton's Fault row ignores all events.

FUTURE (P1.M7.T1.S1 — invariant suite): consumes this suite's laws as
  templates: valve-closed-first on every fault exit (logs[0] == ValveClosed),
  no-escape-from-Fault, reset-lands-Idle-no-history.

CONFIG: none — config.h FROZEN (§8). Consumed constants:
  FRESH_BOOT_BACKSTOP_MIN, CONFIRM_WINDOW_S (widen BEFORE x1000),
  FAULT_BLINK_MS, FAULT_PAUSE_MS, READING_INTERVAL_MS, BASELINE_MIN/MAX,
  TRIP_DROP_COUNTS (via the harness's DRY/WET math).

BUILD: none — platformio.ini UNCHANGED; test/test_faultlatch/ auto-discovered
  as the 14th native project.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio test -e native        # exit 0, 14 projects all [PASSED] (13 existing + test_faultlatch)
pio run -e pico           # ========================= [SUCCESS]
# NO ruff/mypy (C++ project). NO `pio test -e pico` (no on-device transport).
# Suite runtime is dominated by two 30-min sims (~145k passes total) — well
# under a second natively. If it suddenly takes seconds, a test loop is
# spinning; investigate, do not raise timeouts.
```

### Level 2: Structure & Diff Validation

```bash
grep -c "RUN_TEST" test/test_faultlatch/test_faultlatch.cpp   # -> 13
grep -n "SEED_BASE\|1664525u" test/test_faultlatch/test_faultlatch.cpp  # -> seeded LCG present
git status --short
#   PRIMARY PATH: only "?? test/test_faultlatch/"
#     (S2's own uncommitted " M firmware/...jug_filler.ino" + "?? test/test_fillengine/"
#      may also show — they are S2's, not yours; do not touch or revert them.)
#   CONDITIONAL PATH: additionally " M firmware/jug_filler/jug_filler.ino" (Task 1R)
git diff firmware/jug_filler/config.h platformio.ini   # -> EMPTY (untouched)
```

### Level 3: Contract Validation

```bash
pio test -e native -f test_faultlatch && pio test -e native -f test_faultlatch
#   -> [PASSED] twice, identically (seeded — bit-reproducible)
pio test -e native -f test_fillengine
#   -> still 20/20 (you did not disturb S2's suite or the engine)
# Purity: the new test file contains NO digitalWrite/Serial/millis/delay
# production calls (Sim injects time; the .ino is included, not executed on HW).
```

### Level 4: On-Device Bench Validation — P1.M7.T2.S1's deliverable, NOT this task's

Pure logic; no upload target configured. The blink codes will be eyeballed on hardware (fault a unit, count LED groups, press BOOT to confirm it's ignored, RESET to confirm recovery) in P1.M7.T2.S1. Success Criteria end at Levels 1–3.

## Final Validation Checklist

### Technical Validation

- [ ] `pio test -e native` → exit 0, 14/14 projects `[PASSED]` (incl. `test_faultlatch`, 13 tests)
- [ ] Deterministic re-run of `test_faultlatch` passes identically (seeded fuzz)
- [ ] `test_fillengine` still 20/20; `pio run -e pico` → `[SUCCESS]`
- [ ] Level-2 greps return expected counts; config.h/platformio.ini/ARDUINO region untouched; `test/test_fillengine/` untouched

### Feature Validation

- [ ] §7 table complete: baseline s1/s2 → 1/2 (valve never opened, ValveClosed first); confirm-miss both directions → counterpart's code; early-wet s1/s2 and early-trip s1/s2 → that sensor's code; backstop → 3
- [ ] Latching: full event vocabulary ignored across the 3-min soak and the seeded fuzz; faultCode/led/valve frozen; zero logs from button input in Fault
- [ ] Blink presentation: codes 1/2/3 through `setMode(o.led, o.faultCode)` render N × (250 on/250 off) + 2000 off, cycle repeating (mid-on/just-off/mid-gap/gap-end/restart instants)
- [ ] §5.3 reset: fresh instance → Idle, fault 0, history wiped; 5 hostile idle minutes → no auto-start; next fill → freshBoot, 30 min, recovery 0
- [ ] Item contract fully closed: fault entry closes valve FIRST; state Fault with code; led FaultBlink(code); BOOT ignored; valve refuses; reset clears fault AND history

### Code Quality Validation

- [ ] House conventions: ITEM CASE banners, seeded inline LCG (no `<random>`), `elapsedMs`-based harness, explicit Unity `main()` runner
- [ ] Suite is self-contained (own Sim copy; includes the .ino directly; does not include sibling tests)
- [ ] No production-code changes under the primary path; any Task-1R change is surgical, inside the S2 banner, and keeps `test_fillengine` green

### Documentation & Deployment

- [ ] File header documents the four contract areas (fault table, latching, presentation, reset) and the wet-baseline gotcha
- [ ] No new env vars, dependencies, or config; item contract says no docs — internal

---

## Anti-Patterns to Avoid

- ❌ Don't rewrite or "harden" the FillEngine block preemptively — the S2 contract already implements latching; this task VERIFIES it. Touch the .ino only via conditional Task 1R after a failed Task-0 check.
- ❌ Don't edit `test/test_fillengine/` — S2 owns it; your suite is a separate project with its own harness copy (tests include the .ino, never each other).
- ❌ Don't use WET streams for fast fault entry — 2500 is baseline-plausible (2000..4050); a wet settle can open the valve and run 30 min to backstop. Fast entry = out-of-range levels (1000 / 4200).
- ❌ Don't fuzz the fault-ENTRY phase with random buttons — aborts dominate and wet baselines go plausible; entry is deterministic here, absorption is the property (entry variety is tests 1–9 + S2's fuzz).
- ❌ Don't drive `setMode(FaultBlink)` without the group count — the code rides on the second argument; n==0 silently clamps to 1 and a wrong-code test would still "blink".
- ❌ Don't sample LED cadence at boundary instants only on one side — the suite checks mid-on, just-off, slot-end, mid-gap, gap-end AND next-cycle restart.
- ❌ Don't assert reset semantics via `tableMinutes()` alone without `freshBoot()` — 30 could otherwise coincide with an interpolation knot; the pair is the proof.
- ❌ Don't compare timestamps directly or multiply narrow constants — `elapsedMs` and `(uint32_t)X * 1000u`.
- ❌ Don't use `<random>` for the fuzz — the inline seeded LCG is the reproducibility contract.
- ❌ Don't forget the explicit Unity `main()` runner (PIO 6.2.0 generates none); don't `pio test -e pico`.

---

**Confidence Score: 9/10** — the complete test file is provided verbatim against APIs read from the live source this session (S2's engine verified landed at .ino lines 993–1281 with `endFillFault`/`out.faultCode`/accessors all present; `fillApplyButton`'s Fault row verified `break;`; `LedPatternEngine::setMode(Mode, faultGroups)`/`tick`/`faultGroups` verified; `RecoveryClock`/`FillDeadline` fresh-boot accessors verified; all timing constants read from config.h). Residual risk: (a) S2's final validation may still adjust its block — mitigated by Task 0's gate (stop if `test_fillengine` isn't green) and Task 1R's surgical remediation path keyed to the S2 PRP's authoritative source; (b) the exact-boundary trip tests inherit `tripAt`'s internal alignment assert, which fails loudly rather than silently testing the wrong edge.
