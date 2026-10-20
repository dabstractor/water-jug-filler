# PRP — P1.M6.T1.S2: Fill lifecycle orchestration: baseline → valve → trip → CONFIRM → DONE (pure)

## Goal

**Feature Goal**: Implement PRD §7's complete fill lifecycle as ONE pure class `FillEngine` — composing the §5.1 transition core (P1.M6.T1.S1), BaselineCapture, TripDetectors, FalseTripGuards, RecoveryClock/FillDeadline and the LED mode mapping into the sequence: press → 2 s settle/baseline INSIDE Filling (valve closed) → plausibility gate → valve open + deadline anchored at the OPEN instant → first trip (valve closed FIRST, then CONFIRM) → second trip within 12 s → DONE with the recovery-clock reference set to the valve-CLOSE instant. Every alternate ending (abort, baseline/guard/confirm/backstop fault) ends the fill with the valve-close output as its FIRST action (§7.9 close-before-state-flip).

**Deliverable**:
1. `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: one bannered block in the UNGUARDED region (between the S1 end banner and the `#ifdef ARDUINO` region marker) containing `FILL_LOG_CAPACITY`, `struct FillLogEvent`, `struct FillEngineOutputs`, `class FillEngine`.
2. `test/test_fillengine/test_fillengine.cpp` — NEW: Unity native suite, 20 tests — happy paths (sensor-1-first and sensor-2-first, symmetric), same-step double trip, Flow 1 (both variants, fresh RECOMPUTED timer), Flow 2 (with the §7.8 valve-close-instant reference pinned in ms), confirm-window timeout → fault, early-wet/early-trip guard faults with the exact 60 s boundary, baseline faults (valve never opens), backstop expiry (code 3), +5 extension/stacking with ack edges, §7.9 ordering everywhere, and a seeded-LCG fuzz property. NO other files change (config.h §8 frozen; platformio.ini untouched — new test dir auto-discovered as the 13th native project).

**Success Definition**: `pio test -e native` → exit 0, all 13 projects `[PASSED]` (12 existing green + new `test_fillengine`); `pio run -e pico` → `[SUCCESS]`; structure greps confirm exactly one new block in the UNGUARDED region; the ARDUINO region is untouched; `git status --short` → only ` M firmware/jug_filler/jug_filler.ino` + untracked `test/test_fillengine/`.

## User Persona

**Target User**: The implementing agent of P1.M6.T2.S1 (hardware loop), and through it P1.M7.T1.S1 (invariant suite) and P1.M6.T1.S3 (fault latching/blink codes).

**Use Case**: The integration loop calls ONE method per pass — `FillEngineOutputs o = s_engine.step(nowMs, ev, readingDue, s_adcAvg1, s_adcAvg2);` — and routes `o.valveOpen` to the valve pin, `o.led`/`o.faultCode`/`o.ackBlink` to the LED engine, and `o.logs[]` to `logLine`. All lifecycle sequencing, timer wiring and fault determination live behind that call, natively testable.

**User Journey**: bench user short-presses BOOT → LED goes Solid immediately, valve stays closed 2 s while dry baselines are captured → valve opens, backstop running (anchored now) → first sensor sees water → valve closes that instant, LED stays Solid through CONFIRM → second sensor confirms → DONE: green attention blink, recovery clock referenced to the valve-close instant → next BOOT press acknowledges → a further press starts the next fill with a backstop recomputed from the measured recovery.

**Pain Points Addressed**: sequencing six pure services correctly (anchor instants, once-only computations, exactly-once reading routing) is exactly where hand-wired loops go wrong; §7.9 ordering and the §6/§7 instant choices need machine-checkable proofs before any hardware is wired.

## Why

- PRD §7 steps 1–9 with the architecture clarifications (plan/…/architecture/firmware_architecture.md §3): settle inside FILLING with valve closed; settle-abort honored and restarts the recovery clock (§6.1 "whatever ended it"); backstop computed once at press, deadline anchored at valve-open (§6 preamble bounds valve-open time); first trip closes the valve immediately; CONFIRM success → DONE with reference = valve-CLOSE instant (§7.8); every fill-ending path drives the valve LOW first (§7.9).
- PRD §5.2 flows 1/2 are normative and end-to-end executable at this layer with scripted sensor streams.
- PRD §10: valve HIGH only in FILLING's valve-open sub-phase; no path opens the valve automatically. This engine is the single place the valve-open decision exists (one `valveOpen_ = true` site).

## What

Add ONE bannered block to the UNGUARDED region of `firmware/jug_filler/jug_filler.ino` and ONE new native test project. The exact code is in the Implementation Blueprint — copy it verbatim.

**Public surface** (consumed by P1.M6.T2.S1, tested by this task, reused by P1.M6.T1.S3 / P1.M7.T1.S1):

```cpp
constexpr uint8_t FILL_LOG_CAPACITY = 5;   // worst single step = 5 events (double-trip step)
struct FillLogEvent { enum class Kind : uint8_t { ValveClosed=1, FillStart=2, BaselineOk=3,
    BaselineFault=4, ValveOpen=5, Trip=6, ConfirmPass=7, Done=8, Abort=9, EarlyWetFault=10,
    EarlyTripFault=11, BackstopExpired=12, ConfirmTimeout=13, Plus5=14, Ack=15 };
    Kind kind; uint8_t sensor; uint32_t a; uint32_t b; };
struct FillEngineOutputs {
    bool valveOpen;  LedPatternEngine::Mode led;  uint8_t faultCode;      // levels
    bool plus5Applied, acknowledged, ackBlink;                             // one-step edges
    uint8_t logCount; FillLogEvent logs[FILL_LOG_CAPACITY]; };             // ordered actions
class FillEngine {
  public:
    FillEngineOutputs step(uint32_t nowMs, ButtonClassifier::Event ev,
                           bool readingDue, uint16_t avg1, uint16_t avg2);
    FillState state() const;  uint8_t faultCode() const;  bool valveOpen() const;
    uint32_t valveOpenedAtMs() const;  const FillDeadline& deadline() const;
    const RecoveryClock& clock() const; };
```

**Semantics decisions (binding):**
- **Step order** (fixes all anchor/routing races): ① button table via `fillApplyButton(state_, ev)` + edge side-effects (extend5, ack log) → ② fill start (Idle→Filling: `deadline_.beginFill` at press, `baseline_.begin`, log FillStart) → ③ abort (Filling→Idle: valve LOW first, `recordFillEnd(now)`, log Abort) → ④ route the reading stream EXACTLY ONCE (settle→baseline; valve-open or Confirm→both detectors) → ⑤ Filling work (settle: baseline-done gate → fault-or-open; valve-open: guards → trip → backstop, EVERY pass) → ⑥ Confirm work (also same step it was entered: second trip → Done; else 12 s timeout → fault) → ⑦ levels from the final state.
- **Valve level is the engine's sub-phase level**, NOT `fillApplyButton`'s `valveOpen` (which is true from press). Settle runs inside FILLING with the valve closed (arch §3.1). The ONLY `valveOpen_ = true` site is the baseline-Ok gate; every end path sets `valveOpen_ = false` BEFORE flipping `state_` (§7.9).
- **§7.9 ordering law (uniform)**: ANY step whose before-state is Filling or Confirm and whose after-state differs logs `ValveClosed` as `logs[0]` — including the baseline fault (valve never opened), settle abort, and Confirm exits (valve already closed). Driving an already-LOW pin LOW again is the point of the invariant.
- **Guards/backstop run ONLY while the valve is open**: entering Confirm proves the first trip passed fill time ≥ 60 s (else EarlyTripFault fired first — guards are evaluated before the trip check), so both guard windows are provably closed in Confirm; the backstop bounds valve-open time (§6 preamble) and the valve is already closed in Confirm.
- **Order within the valve-open sub-phase**: guards → trip → backstop. A false trip must fault, not CONFIRM; a real trip outranks the backstop.
- **Confirm window**: timeout fires when `elapsedMs(now, confirmStartMs_) >= (uint32_t)CONFIRM_WINDOW_S * 1000u` (inclusive at exactly 12.000 s, matching `deadlinePassed` house style); a second trip latched in the same pass wins (checked first).
- **Recovery references**: DONE → `recordFillEnd(valveClosedAtMs_)` (the first-trip instant, a PAST timestamp — elapsedMs handles it); abort and every fault → `recordFillEnd(nowMs)` (arch §3.3).
- **Fault codes**: baseline/guard/confirm-timeout → `sensor + 1` (sensor 1 wins ties — house precedence, cf. FalseTripGuards); backstop expiry → 3. `faultCode()`/`out.faultCode` are the hand-off to P1.M6.T1.S3 (blink groups, latch presentation). Fault latches structurally: the §5.1 table ignores all presses in Fault; only reset/power-cycle (object re-construction) clears it.
- **LED is Solid from the PRESS**, not from valve-open: state Filling includes settle (PRD §4 table + arch §3.1). The item text's "open valve … led Solid" clause is reconciled to the authoritative state table — do NOT "fix" this.
- **+5**: on `plus5Applied` the engine calls `deadline_.extend5()` itself (valid before or after anchor), sets `ackBlink` edge for `LedPatternEngine::triggerAck()`, logs Plus5. Stackable; FILLING-only (table).
- **Reading stream**: the loop owns the ReadingAggregator (P1.M3.T1.S2); the engine consumes the latest emitted pair. `readingDue` gates detector/baseline feeding; `avg1/avg2` are passed every call because the guards want the freshest levels even between emissions. Readings are ignored in Idle/Done/Fault. The reading emitted on the same pass the valve opens went to the baseline (correct — the valve was closed at emission); detectors feed from the next emission.

**NOT in scope** (owners): fault blink-code presentation/latch hardening tests (P1.M6.T1.S3 — consumes `faultCode`), any ARDUINO-region change / pin writes / Serial (P1.M6.T2.S1), invariant-suite authoring (P1.M7.T1.S1 — consumes this engine), config.h changes (§8 frozen), platformio.ini changes, docs (item contract: internal).

### Success Criteria

- [ ] One bannered block added after the S1 end banner, before the ARDUINO region marker; ARDUINO region byte-identical; diff purely additive
- [ ] Press → settle (valve closed, LED Solid) → baseline Ok → valve opens + deadline anchored at open instant; baseline implausible → Fault(sensor+1) with the valve NEVER having opened
- [ ] First trip (either sensor) → ValveClosed logged FIRST → Confirm; second trip within window → Done with recovery ref = valve-close instant; timeout → Fault(other sensor)
- [ ] §7.9 ordering: every Filling/Confirm exit step logs ValveClosed first — pinned per-scenario AND by the fuzz property
- [ ] Flow 1 both variants: restart recomputes the backstop from the abort instant (44 / 41 vs fresh-boot 30); Flow 2 complete with §7.8 ms-exact reference assert
- [ ] Guards: early-wet faults on first wet emission; early-trip at 59.8 s faults; trip at exactly 60.000 s accepted
- [ ] Backstop expiry → Fault 3; +5 extends (35 min) and stacks, with ackBlink/plus5Applied one-step edges
- [ ] `pio test -e native` → 13/13 PASSED; `pio run -e pico` → SUCCESS

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete copy-paste-ready engine AND test file are in the Blueprint; every consumed surface was read from the live file this session (S1 block verified landed at lines 866–986); the insertion anchor is quoted as exact text; all validation commands are verified-working repo invocations.

### Documentation & References

```yaml
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (UNGUARDED region only). (1) INSERTION ANCHOR — the
        banner `// ===== end fill-state transition core (consumed by
        P1.M6.T1.S2, P1.M6.T2.S1) =====` (verified at line 986); insert AFTER
        it, BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----` (line
        988). Use TEXT anchors — line numbers shift. (2) The S1 block above
        the anchor provides FillState/FillOutputs/fillApplyButton/
        fillLevelOutputs — read it. (3) Consumed services: BaselineCapture,
        TripDetector, FalseTripGuards/GuardVerdict, RecoveryClock,
        FillDeadline, ButtonClassifier::Event, LedPatternEngine::Mode — all
        defined ABOVE your insertion point. (4) Seam contract in the file
        header: pure logic only, explicit prototypes, injected clock.
  gotcha: do NOT touch the #ifdef ARDUINO region.

- file: plan/001_53984f832581/P1M6T1S1/PRP.md
  why: CONTRACT for the transition core you compose. Its "Integration
        Points" section anticipates THIS task: engine owns the state
        variable, calls fillApplyButton per press, maps plus5 → extend5 +
        triggerAck, gates the physical open behind baseline; "NEVER
        digitalWrite(PIN_VALVE, out.valveOpen) directly at press time".
  section: "What" + "Integration Points".

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  why: §3 behavioral decisions 1–7 are BINDING for FillEngine (settle inside
        Filling; anchor at valve-open; recovery reference instants;
        close-before-state-flip; +5 scope; DONE persists until ack).

- file: firmware/jug_filler/config.h
  why: the §8 constants consumed (CONFIRM_WINDOW_S etc.) — READ-ONLY,
        frozen. uint16_t second-constants MUST widen to uint32_t BEFORE ×1000.

- file: test/test_deadline/test_deadline.cpp
  why: test pattern — Unity include of the .ino, `// ITEM CASE:` banners,
        REQUIRED explicit runner `int main(){UNITY_BEGIN();RUN_TEST(...);
        return UNITY_END();}` (PIO 6.2.0 generates no runner).

- file: plan/001_53984f832581/P1M6T1S2/research/notes.md
  why: this task's research: verified codebase state, design decisions,
        exact test-timing math (baseline opens at t=3400 from a t=1000 press;
        emission grid t≡0 mod 100).

- url: https://docs.throwtheswitch.org/Unity/Unity-Assertions-Reference.html
  why: assertion macros (all already exercised by 12 green sibling suites).
  critical: TEST_ASSERT_EQUAL_UINT32(expected, actual) order; enum-class
        equality via TEST_ASSERT_TRUE(a == b).
```

### Current Codebase tree (verified this session)

```bash
water-jug-filler/
├── platformio.ini            # [env:native] Unity host tests (12 projects green);
│                             #   [env:pico] compile-only — NO changes needed
├── firmware/jug_filler/
│   ├── jug_filler.ino        # ~1164 lines: UNGUARDED pure logic … S1 core block
│   │                         #   (lines 866–986) … then #ifdef ARDUINO region (line 988)
│   │                         #   ← INSERT new block AFTER the S1 end banner
│   └── config.h              # §8 frozen — READ-ONLY
├── test/                     # 12 Unity projects (… test_fillcore)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain: PlatformIO Core 6.2.0 (`pio`); native = host g++ + Unity 2.6.1.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive, UNGUARDED only): +FILL_LOG_CAPACITY,
│                             #   +FillLogEvent, +FillEngineOutputs, +FillEngine (the §7 orchestrator)
└── test/
    └── test_fillengine/      # NEW — 13th native project (auto-discovered, no ini edit)
        └── test_fillengine.cpp  # 20 tests: deterministic Sim harness + seeded fuzz
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — double-feed race: the reading must be routed to baseline OR
//   detectors EXACTLY ONCE per step. On the trip pass the state moves
//   Filling→Confirm→(maybe)Done inside ONE step(); feeding must NOT live in
//   both the Filling and Confirm branches. The blueprint feeds in its own
//   section (④) before any phase work.

// CRITICAL — `FillEngineOutputs` cannot reference `FillEngine::LOG_CAPACITY`
//   (class not yet defined). Use the block-scope `constexpr uint8_t
//   FILL_LOG_CAPACITY = 5;` BEFORE both structs (precedent: HALF_WHEEL_MS).

// CRITICAL — capacity math: the same-step double trip emits ValveClosed,
//   Trip, ValveClosed, ConfirmPass, Done = 5 events. Capacity 4 would
//   silently DROP the Done event (defensive drop in log()).

// CRITICAL — widen seconds constants BEFORE multiplying:
//   (uint32_t)CONFIRM_WINDOW_S * 1000u — uint16_t*1000 truncates (guards'
//   header documents the same trap).

// CRITICAL — T2.S1 wiring (document, don't implement): call s_led.setMode()
//   ONLY when o.led (or faultCode) CHANGES — setMode re-anchors the pattern
//   phase every call; calling it every pass freezes every blink pattern.

// GOTCHA — recordFillEnd(valveClosedAtMs_) at DONE stores a PAST timestamp;
//   correct by design (elapsedMs is mod-2^32 subtraction). Not a bug.

// GOTCHA — trip detectors must be fed ONLY validated baselines
//   (TripDetector::begin contract); the engine arms them exactly at the
//   baseline-Ok gate — never with verdict != Ok values.

// GOTCHA — Unity on PIO 6.2.0 has NO auto runner: explicit main() with
//   RUN_TEST lines is REQUIRED.

// GOTCHA — fuzz uses the inline seeded LCG (s = s*1664525u + 1013904223u),
//   NOT <random>: bit-reproducible everywhere; the seed is the contract.

// GOTCHA — never `pio test -e pico`. `pio run -e pico` is the device-side
//   compile gate; on-hardware validation is P1.M7.T2.S1's deliverable.
```

## Implementation Blueprint

### Data models and structure

One constant, two POD structs, one class — complete code below. No ORM/Pydantic (embedded C++).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY preconditions (do not skip)
  - RUN: grep -n "end fill-state transition core" firmware/jug_filler/jug_filler.ino
    → anchor exists (S1 landed; verified at line 986 this session).
  - RUN: grep -c "class FillEngine" firmware/jug_filler/jug_filler.ino → 0.
  - RUN: grep -n "enum class FillState\|fillApplyButton\|class BaselineCapture\|class TripDetector\|class FalseTripGuards\|class FillDeadline\|class RecoveryClock" firmware/jug_filler/jug_filler.ino
    → all present ABOVE the anchor.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — insert the engine block
  - FIND: `// ===== end fill-state transition core (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====`
  - INSERT the COMPLETE block from "Implementation Patterns & Key Details"
    AFTER that banner, BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`.
  - NAMING: FillEngine class (CamelCase), step()/state()/faultCode()… (camelCase
    methods); banner `// ===================== Fill lifecycle engine (P1.M6.T1.S2) =====================`
    … `// ===== end fill lifecycle engine (consumed by P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) =====`.

Task 2: CREATE test/test_fillengine/test_fillengine.cpp
  - CONTENT: the COMPLETE file from "Implementation Patterns & Key Details".
  - FOLLOW pattern: test_deadline.cpp (ITEM CASE banners, setUp/tearDown empty,
    RUN_TEST main). PLACEMENT: test/test_fillengine/ (auto-discovered).

Task 3: VALIDATE (Validation Loop) — 13/13 native, pico compile, greps, git status.
```

### Implementation Patterns & Key Details

**Complete block — copy-paste-ready** (insert per Task 1):

```cpp
// ===================== Fill lifecycle engine (P1.M6.T1.S2) =====================
// PRD §7's fill lifecycle as ONE pure step function: composes the §5.1
// transition core (P1.M6.T1.S1) with BaselineCapture (P1.M3.T2.S1),
// TripDetectors + FalseTripGuards (P1.M3.T3.S1/S2), RecoveryClock +
// FillDeadline (P1.M4.T2.S1) and the LED mode map (P1.M5.T1.S1 via
// fillLevelOutputs) into the full sequence
//   IDLE --press--> settle/baseline (valve CLOSED — settle runs INSIDE
//   FILLING, arch §3.1) -> baseline Ok -> valve OPEN + deadline anchored
//   at the OPEN instant (§6 preamble bounds valve-open time) -> first
//   trip: valve closed FIRST, CONFIRM (§7.4/§7.9) -> second trip within
//   CONFIRM_WINDOW_S: DONE, recovery reference = the valve-CLOSE instant
//   (§7.8) — with every alternate ending (abort §5.2, baseline/guard/
//   confirm/backstop fault §7) driving the valve LOW as its FIRST action.
//
// SEAM: step() takes the injected clock, the classified button event and
// the averaged pair "when due" (the loop owns the ReadingAggregator,
// P1.M3.T1.S2 — the engine never samples). It RETURNS levels + edges + an
// ORDERED action log; it never touches pins, Serial or LedPatternEngine
// state. P1.M6.T2.S1 routes them: valve write; s_led.setMode(o.led,
// o.faultCode) ONLY ON CHANGE (setMode re-anchors the pattern phase —
// calling it every pass would freeze every blink); o.ackBlink ->
// triggerAck(); one logLine per log event.
//
// Sub-phase model: FILLING spans the WHOLE attempt, press to end (arch §3.1):
//   state==Filling && !valveOpen_ -> settle/baseline (valve closed)
//   state==Filling &&  valveOpen_ -> valve open: guards, trip, backstop —
//                                   evaluated EVERY pass, in that order
//   state==Confirm                -> valve closed: second trip vs the
//                                   CONFIRM_WINDOW_S timeout (inclusive at
//                                   exactly 12.000 s; a second trip latched
//                                   in the same pass wins)
// Guards and backstop run ONLY while the valve is open: entering CONFIRM
// proves the first trip passed fill time >= TRIP_MIN_ELAPSED_S (else
// EarlyTripFault fired first — guards are evaluated before the trip
// check), so both guard windows are provably closed throughout CONFIRM;
// and the backstop bounds VALVE-OPEN time, which is already over.
//
// Recovery references (arch §3.3, §6.1): DONE records the valve-CLOSE
// (first-trip) instant; abort and every fault record the attempt-end
// instant. Every fill computes its backstop ONCE at press
// (FillDeadline::beginFill) and anchors at OPEN (FillDeadline::anchor) —
// the 2 s settle never eats the budget, and a settle abort never anchored.
//
// §7.9 ordering law (uniform): ANY step that leaves Filling/Confirm logs
// ValveClosed as logs[0] — including the baseline fault (valve never
// opened), the settle abort, and Confirm exits (valve already closed):
// driving an already-LOW pin LOW again IS the invariant. The invariant
// suite (P1.M7.T1.S1) asserts this ordering.

// Worst single step = 5 events (the same-step double trip: ValveClosed,
// Trip, ValveClosed, ConfirmPass, Done). Block-scope so FillEngineOutputs
// can size its array (class not yet defined at that point).
constexpr uint8_t FILL_LOG_CAPACITY = 5;

// One ordered engine action. T2.S1 formats ONE logLine per event; the
// ORDER is normative (logs[0] == ValveClosed on any fill-ending step).
struct FillLogEvent {
    enum class Kind : uint8_t {
        ValveClosed     = 1,   // fill end, ANY cause — ALWAYS first on its step (§7.9)
        FillStart       = 2,   // a: backstop table minutes; b: recovery ms at press (0 = fresh boot)
        BaselineOk      = 3,   // a/b: dry baselines, counts
        BaselineFault   = 4,   // sensor: failing; a: fault code (sensor+1)
        ValveOpen       = 5,   // a: budget minutes at open (incl. pre-open +5s)
        Trip            = 6,   // sensor: first-trip sensor; a: fill time ms
        ConfirmPass     = 7,   // a: confirm duration ms
        Done            = 8,   // a: attempt ms (press->valve close); b: valve-open ms
        Abort           = 9,   // a: attempt duration ms
        EarlyWetFault   = 10,  // sensor: shorted/wet-too-early; a: fault code
        EarlyTripFault  = 11,  // sensor: false trip; a: fault code
        BackstopExpired = 12,  // a: fault code 3
        ConfirmTimeout  = 13,  // sensor: never tripped in window; a: fault code
        Plus5           = 14,  // a: new total budget minutes
        Ack             = 15,  // DONE acknowledged -> IDLE
    };
    Kind     kind;    // what happened
    uint8_t  sensor;  // 0 == sensor 1 (fault code 1), 1 == sensor 2 (fault code 2); 0 when n/a
    uint32_t a;       // payload 1 (ms / counts / minutes / code — per Kind)
    uint32_t b;       // payload 2
};

// One step()'s entire observable effect. Levels are safe to route every
// pass; edges are true for exactly the step that caused them.
struct FillEngineOutputs {
    bool valveOpen;              // LEVEL: commanded pin — HIGH only in FILLING's valve sub-phase
    LedPatternEngine::Mode led;  // LEVEL: PRD §4 pattern for the current state
    uint8_t faultCode;           // LEVEL: 0, or 1..3 while in FAULT (blink groups: P1.M6.T1.S3)
    bool plus5Applied;           // EDGE: +5 applied this step (extend5 already done internally)
    bool acknowledged;           // EDGE: DONE acknowledged this step
    bool ackBlink;               // EDGE: route to LedPatternEngine::triggerAck() this step
    uint8_t logCount;            // ordered actions; logs[0] is ValveClosed on any fill end
    FillLogEvent logs[FILL_LOG_CAPACITY];
};

class FillEngine {
public:
    FillEngine()
        : state_(FillState::Idle), valveOpen_(false),
          valveOpenedAtMs_(0), valveClosedAtMs_(0), confirmStartMs_(0),
          fillStartMs_(0), confirmFirst_(0), faultCode_(0), logCount_(0) {}

    // Advance the lifecycle one loop pass.
    //   nowMs      — injected clock, same domain as every service (seam rule)
    //   ev         — classified BOOT event (P1.M2.T1.S1); None on most passes
    //   readingDue — true iff the loop's aggregator emitted a fresh averaged
    //                pair THIS pass; gates baseline/detector feeding
    //   avg1/avg2  — the latest EMITTED pair (loop's s_adcAvg1/2): the
    //                guards want the freshest levels even between emissions
    FillEngineOutputs step(uint32_t nowMs, ButtonClassifier::Event ev,
                           bool readingDue, uint16_t avg1, uint16_t avg2) {
        FillEngineOutputs out{};  // zero-init: valve closed, Off, no edges (§10 fail-safe)
        logCount_ = 0;

        // ---- 1. Button table (§5.1, P1.M6.T1.S1) -------------------------
        const FillState before = state_;
        const FillOutputs btn = fillApplyButton(state_, ev);
        if (btn.plus5Applied) {  // FILLING long-press: +5 to THIS fill, stacks
            deadline_.extend5();
            out.plus5Applied = true;
            out.ackBlink = true;  // -> triggerAck() (PRD §4 transient)
            log(FillLogEvent::Kind::Plus5, 0, deadline_.budgetMinutes(), 0);
        }
        if (btn.acknowledged) {  // DONE press: light off via the Idle levels
            out.acknowledged = true;
            log(FillLogEvent::Kind::Ack, 0, 0, 0);
        }

        // ---- 2. Fill start: the ONLY entry into FILLING (Idle+ShortPress)
        if (before == FillState::Idle && state_ == FillState::Filling) {
            fillStartMs_ = nowMs;
            deadline_.beginFill(clock_, nowMs);  // backstop ONCE, at press (§6.2)
            baseline_.begin(nowMs);              // settle+collect; valve stays closed
            valveOpen_ = false;                  // settle runs inside FILLING (arch §3.1)
            faultCode_ = 0;
            log(FillLogEvent::Kind::FillStart, 0, deadline_.tableMinutes(),
                deadline_.recoveryMsAtStart());
        }

        // ---- 3. Abort: Filling+ShortPress -> Idle (§5.2 flow 1) -----------
        if (before == FillState::Filling && state_ == FillState::Idle) {
            valveOpen_ = false;  // §7.9: LOW FIRST — uniform, even if never opened
            log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
            clock_.recordFillEnd(nowMs);  // §6.1: recovery restarts at the abort instant
            log(FillLogEvent::Kind::Abort, 0, elapsedMs(nowMs, fillStartMs_), 0);
        }

        // ---- 4. Route the reading stream — EXACTLY ONCE per step ----------
        // (After the button blocks so a just-started fill owns the stream and
        // a just-aborted one drops it; before the phase work so the reading
        // that completes the baseline/trip is seen by blocks 5/6 same step.)
        if (readingDue) {
            if (state_ == FillState::Filling && !valveOpen_) {
                baseline_.addReading(nowMs, avg1, avg2);  // settle drops pre-2 s pairs
            } else if (state_ == FillState::Filling || state_ == FillState::Confirm) {
                trip1_.feed(avg1);  // a latched detector ignores feeds (P1.M3.T3.S1)
                trip2_.feed(avg2);
            }
        }

        // ---- 5. FILLING work ---------------------------------------------
        if (state_ == FillState::Filling && !valveOpen_) {
            // settle/baseline sub-phase: the §7.1 plausibility gate
            if (baseline_.done()) {
                const bool bad1 = baseline_.verdict(0) != BaselineCapture::Verdict::Ok;
                const bool bad2 = baseline_.verdict(1) != BaselineCapture::Verdict::Ok;
                if (bad1 || bad2) {
                    // Refuse to open the valve (§7.1). Sensor 1 wins ties
                    // (house precedence, cf. FalseTripGuards).
                    endFillFault(nowMs, FillLogEvent::Kind::BaselineFault, bad1 ? 0 : 1);
                } else {
                    trip1_.begin(baseline_.baseline(0));  // VALIDATED baselines only
                    trip2_.begin(baseline_.baseline(1));  // (TripDetector::begin contract)
                    guards_.begin(baseline_.baseline(0), baseline_.baseline(1));
                    valveOpen_ = true;         // the ONLY valve-open site in the firmware
                    valveOpenedAtMs_ = nowMs;
                    deadline_.anchor(nowMs);   // §6 preamble: bounds valve-open time
                    log(FillLogEvent::Kind::BaselineOk, 0,
                        baseline_.baseline(0), baseline_.baseline(1));
                    log(FillLogEvent::Kind::ValveOpen, 0, deadline_.budgetMinutes(), 0);
                }
            }
        } else if (state_ == FillState::Filling) {
            // valve-open sub-phase — EVERY pass, guards -> trip -> backstop:
            // a false trip must fault (not CONFIRM); a real trip outranks
            // the backstop.
            const GuardVerdict v = guards_.evaluate(nowMs, valveOpenedAtMs_,
                                                    trip1_.tripped(), trip2_.tripped(),
                                                    avg1, avg2);
            if (v.kind != GuardVerdict::Ok) {
                endFillFault(nowMs,
                             v.kind == GuardVerdict::EarlyWetFault
                                 ? FillLogEvent::Kind::EarlyWetFault
                                 : FillLogEvent::Kind::EarlyTripFault,
                             v.sensor);
            } else if (trip1_.tripped() || trip2_.tripped()) {
                // First trip (§7.4): valve closed FIRST, then CONFIRM.
                const uint8_t first = trip1_.tripped() ? 0 : 1;
                valveOpen_ = false;  // §7.9: LOW FIRST — before the state flip
                log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
                log(FillLogEvent::Kind::Trip, first,
                    elapsedMs(nowMs, valveOpenedAtMs_), 0);
                state_ = FillState::Confirm;
                confirmFirst_ = first;
                confirmStartMs_ = nowMs;
                valveClosedAtMs_ = nowMs;  // §7.8: DONE's recovery reference
            } else if (deadline_.expired(nowMs)) {
                endFillFault(nowMs, FillLogEvent::Kind::BackstopExpired, 0);  // code 3
            }
        }

        // ---- 6. CONFIRM work (also on the step it was entered — a -------
        // ---- same-step second trip completes the fill immediately) ------
        if (state_ == FillState::Confirm) {
            const uint8_t other = (uint8_t)(1u - confirmFirst_);
            const bool otherTripped = (other == 0) ? trip1_.tripped() : trip2_.tripped();
            if (otherTripped) {
                // §7.8: DONE; recovery reference = the valve-CLOSE instant —
                // a PAST timestamp; elapsedMs arithmetic handles it exactly.
                log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);  // uniform §7.9 exit ordering
                state_ = FillState::Done;
                clock_.recordFillEnd(valveClosedAtMs_);
                log(FillLogEvent::Kind::ConfirmPass, other,
                    elapsedMs(nowMs, confirmStartMs_), 0);
                log(FillLogEvent::Kind::Done, 0,
                    elapsedMs(valveClosedAtMs_, fillStartMs_),
                    elapsedMs(valveClosedAtMs_, valveOpenedAtMs_));
            } else if (elapsedMs(nowMs, confirmStartMs_) >=
                       (uint32_t)CONFIRM_WINDOW_S * 1000u) {  // widen BEFORE x1000;
                // inclusive at exactly 12.000 s (deadlinePassed house style)
                // §7.5: the other sensor never tripped — fault IT.
                endFillFault(nowMs, FillLogEvent::Kind::ConfirmTimeout, other);
            }
        }

        // ---- 7. Levels from the (possibly new) state ---------------------
        out.valveOpen = valveOpen_ && state_ == FillState::Filling;
        out.led = fillLevelOutputs(state_).led;
        out.faultCode = (state_ == FillState::Fault) ? faultCode_ : 0;
        out.logCount = logCount_;
        for (uint8_t i = 0; i < logCount_; ++i) out.logs[i] = logs_[i];
        return out;
    }

    // ---- observability (tests, P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) ----
    FillState state() const { return state_; }
    uint8_t faultCode() const { return faultCode_; }   // 0, or 1..3 (§7 table)
    bool valveOpen() const { return valveOpen_; }
    uint32_t valveOpenedAtMs() const { return valveOpenedAtMs_; }
    const FillDeadline& deadline() const { return deadline_; }
    const RecoveryClock& clock() const { return clock_; }

private:
    // Append one ordered action. Drop-proof by design (worst step = 5);
    // the guard is defensive only.
    void log(FillLogEvent::Kind kind, uint8_t sensor, uint32_t a, uint32_t b) {
        if (logCount_ >= FILL_LOG_CAPACITY) return;
        logs_[logCount_].kind = kind;
        logs_[logCount_].sensor = sensor;
        logs_[logCount_].a = a;
        logs_[logCount_].b = b;
        ++logCount_;
    }

    // Every fault exit in one place: valve LOW FIRST (§7.9 — uniform even
    // when the valve never opened or is already closed), latch the code,
    // recovery reference at the attempt-end instant (arch §3.3).
    void endFillFault(uint32_t nowMs, FillLogEvent::Kind why, uint8_t sensor) {
        valveOpen_ = false;  // §7.9: LOW FIRST — before the state flip
        log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
        state_ = FillState::Fault;
        faultCode_ = (why == FillLogEvent::Kind::BackstopExpired)
                         ? 3                        // "timer, no trip" (§7 table)
                         : (uint8_t)(sensor + 1);   // sensor fault code
        clock_.recordFillEnd(nowMs);
        log(why, sensor, faultCode_, 0);
    }

    FillState       state_;            // the §5.1 machine's state (S1 core drives it)
    bool            valveOpen_;        // valve sub-phase level: settle=false, open=true
    uint32_t        valveOpenedAtMs_;  // §6 anchor instant (guards' fill-time origin)
    uint32_t        valveClosedAtMs_;  // first-trip instant — DONE's recovery ref (§7.8)
    uint32_t        confirmStartMs_;   // CONFIRM window origin (== valve close)
    uint32_t        fillStartMs_;      // press-accept instant (attempt duration origin)
    uint8_t         confirmFirst_;     // sensor that tripped first (0/1)
    uint8_t         faultCode_;        // 0, or 1..3 latched at Fault entry
    BaselineCapture baseline_;
    TripDetector    trip1_;            // sensor 1 (index 0, fault code 1)
    TripDetector    trip2_;            // sensor 2 (index 1, fault code 2)
    FalseTripGuards guards_;
    RecoveryClock   clock_;
    FillDeadline    deadline_;
    FillLogEvent    logs_[FILL_LOG_CAPACITY];
    uint8_t         logCount_;
};
// ===== end fill lifecycle engine (consumed by P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) =====
```

**Complete test file — copy-paste-ready** (`test/test_fillengine/test_fillengine.cpp`):

```cpp
// test/test_fillengine/test_fillengine.cpp — native contract proof for the
// fill lifecycle engine (P1.M6.T1.S2): §7 steps 1–9 end to end with scripted
// sensor streams, §5.2 flows 1/2, the §6 anchor/reference instants, the
// §7.9 close-before-state-flip ordering (per scenario AND as a seeded fuzz
// property), and the §7 fault paths incl. the exact 60 s guard boundary.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- deterministic loop simulator -----------------------------------------
// 25 ms passes; reading emissions anchored at lastReadingMs=0 land on
// t == 0 (mod 100). A press at t=1000 => baseline accepts from t=3000
// (inclusive 2000 ms settle), 5 pairs => valve opens t=3400 (== 0 mod 100).
// ALL fills open on the emission grid, so exact-time assertions hold.
// DRY=3500 is inside BASELINE_MIN..MAX (2000..4050); WET=2500 is below the
// minimum legal wet threshold (2000-350), so a wet emission always feeds
// the detectors' streak and the early-wet guard.
static constexpr uint16_t DRY = 3500;
static constexpr uint16_t WET = 2500;

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
    // Advance until exactly one more reading emission has happened.
    void readings(int n) {
        for (int i = 0; i < n; ++i) {
            const uint32_t target = lastReadingMs + READING_INTERVAL_MS;
            while (now <= target) pass();
        }
    }
    // Press and run until the valve opens (dry script). Returns t_open.
    uint32_t pressAndOpen() {
        pass(ButtonClassifier::Event::ShortPress);
        while (!eng.valveOpen()) pass();
        return eng.valveOpenedAtMs();
    }
};

// Deterministic 32-bit LCG (Numerical Recipes constants) — seeded fuzz,
// bit-reproducible on any host g++ (the seed IS the reproducibility contract).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Make `sensor` (0/1) trip with the 8th consecutive wet emission landing at
// EXACTLY fillTime == tripFillMs (a multiple of 100; t_open always is).
static void tripAt(Sim& s, int sensor, uint32_t tripFillMs) {
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_EQUAL_UINT32(0, tOpen % READING_INTERVAL_MS);  // alignment sanity
    const uint32_t tEnd = tOpen + tripFillMs;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();  // dry up to streak start
    if (sensor == 0) s.a1 = WET; else s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);  // 8 wet emissions, last AT tEnd
}

// ---- boot / fill start / baseline (PRD §7.1–§7.2) --------------------------

// ITEM CASE: fresh engine is IDLE, everything off, no events.
void test_boot_idle_all_off() {
    Sim s;
    const FillEngineOutputs& o = s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_EQUAL_UINT8(0, o.faultCode);
    TEST_ASSERT_EQUAL_UINT8(0, o.logCount);
}

// ITEM CASE: press starts the fill — backstop computed ONCE at press (fresh
// boot -> 30), LED Solid FROM THE PRESS (FILLING includes settle, arch
// §3.1/§4), but the valve stays CLOSED through the settle.
void test_press_starts_fill_backstop30_led_solid_valve_still_closed() {
    Sim s;
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_FALSE(o.valveOpen);                       // §7.1: settle first
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_TRUE(s.eng.deadline().freshBoot());
    TEST_ASSERT_EQUAL_UINT32(30, s.eng.deadline().tableMinutes());
    TEST_ASSERT_FALSE(s.eng.deadline().anchored());        // anchors at OPEN
    TEST_ASSERT_EQUAL_UINT8(1, o.logCount);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::FillStart);
    TEST_ASSERT_EQUAL_UINT32(30, o.logs[0].a);
    TEST_ASSERT_EQUAL_UINT32(0, o.logs[0].b);              // b: recovery ms (fresh boot)
}

// ITEM CASE: plausible baselines -> valve opens ~2.5 s after press and the
// deadline is ANCHORED AT THE OPEN INSTANT (settle does not eat the budget).
void test_baseline_ok_opens_valve_and_anchors_deadline() {
    Sim s;
    const uint32_t tPress = s.now;
    s.pass(ButtonClassifier::Event::ShortPress);
    while (!s.eng.valveOpen()) s.pass();
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_TRUE(elapsedMs(tOpen, tPress) >= BASELINE_SETTLE_MS);
    TEST_ASSERT_TRUE(s.last.valveOpen);
    TEST_ASSERT_TRUE(s.eng.deadline().anchored());
    TEST_ASSERT_EQUAL_UINT32(tOpen + 30u * 60000u, s.eng.deadline().deadlineMs());
    TEST_ASSERT_EQUAL_UINT8(2, s.last.logCount);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::BaselineOk);
    TEST_ASSERT_EQUAL_UINT32(DRY, s.last.logs[0].a);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ValveOpen);
    TEST_ASSERT_EQUAL_UINT32(30, s.last.logs[1].a);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::Solid);
}

// ITEM CASE: implausible baseline (§7.1) -> fault THAT sensor, valve NEVER
// opened, ValveClosed still logged FIRST (uniform §7.9), fault latches.
void test_baseline_fault_s1_refuses_valve() {
    Sim s;
    s.a1 = 1000;  // below BASELINE_MIN -> TooLow
    s.pass(ButtonClassifier::Event::ShortPress);
    bool everOpen = false;
    while (s.eng.state() == FillState::Filling) everOpen |= s.pass().valveOpen;
    TEST_ASSERT_FALSE(everOpen);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(1, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    s.pass(ButtonClassifier::Event::ShortPress);  // BOOT ignored while faulted (§7)
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
}

// ITEM CASE: symmetric — sensor 2 implausible -> fault code 2.
void test_baseline_fault_s2_refuses_valve() {
    Sim s;
    s.a2 = 4200;  // above BASELINE_MAX -> TooHigh
    s.pass(ButtonClassifier::Event::ShortPress);
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ---- happy paths (§7.3–§7.8) ----------------------------------------------

// ITEM CASE: full happy path, sensor 1 trips first at exactly 60.000 s (the
// guard boundary is strict '<': legal at equality), sensor 2 confirms 2 s
// later -> DONE with the recovery reference at the valve-CLOSE instant.
void test_happy_path_s1_first() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    TEST_ASSERT_FALSE(s.last.valveOpen);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9 first
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::Trip);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
    TEST_ASSERT_EQUAL_UINT32(60000, s.last.logs[1].a);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::Solid);  // CONFIRM continues
    s.a1 = DRY;
    tripAt(s, 1, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // exit ordering
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmPass);
    TEST_ASSERT_TRUE(s.last.logs[2].kind == FillLogEvent::Kind::Done);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::DonePulse);
    // §7.8: recovery reference == first-trip instant (tOpen+60000), ms-exact.
    // Identity: now - recoveryMs(now) == reference instant, for ANY now.
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory());
    TEST_ASSERT_EQUAL_UINT32(tOpen + 60000u, s.now - s.eng.clock().recoveryMs(s.now));
}

// ITEM CASE: symmetric — sensor 2 trips first, sensor 1 confirms. Same DONE.
void test_happy_path_s2_first() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 60u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
    s.a2 = DRY;
    tripAt(s, 0, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmPass);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
}

// ITEM CASE: both sensors trip on the SAME emission -> Filling->Confirm->Done
// in ONE step; worst-case log = 5 events, ValveClosed still first.
void test_double_trip_same_step_done() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    const uint32_t tEnd = tOpen + 60000u;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();
    s.a1 = WET;
    s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_EQUAL_UINT8(5, s.last.logCount);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[4].kind == FillLogEvent::Kind::Done);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- §5.2 flow 1: abort -> restart with fresh RECOMPUTED timer -------------

// ITEM CASE: abort DURING SETTLE (valve never opened) is honored, restarts
// the recovery clock at the abort instant, and the restart's backstop is
// RECOMPUTED (recovery 1 min -> 44, vs fresh-boot 30).
void test_flow1_abort_in_settle_restarts_recovery() {
    Sim s;
    s.pass(ButtonClassifier::Event::ShortPress);
    s.run(500);  // still settling (valve opens ~2.5 s after press)
    TEST_ASSERT_FALSE(s.eng.valveOpen());
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);  // abort
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9, uniform
    TEST_ASSERT_TRUE(o.logs[1].kind == FillLogEvent::Kind::Abort);
    s.run(60000);  // 60 s of recovery since the abort
    const FillEngineOutputs& o2 = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(44, s.eng.deadline().tableMinutes());  // recomputed!
    TEST_ASSERT_EQUAL_UINT32(44, o2.logs[0].a);
}

// ITEM CASE: abort MID-FILL (valve open) -> restart 10 min after the abort
// recomputes from the ABORT instant: knot (10 min) -> 41.
void test_flow1_abort_midfill_recomputes() {
    Sim s;
    s.pressAndOpen();
    s.run(10000);
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    s.run(10u * 60000u);
    s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(41, s.eng.deadline().tableMinutes());
}

// ---- §5.2 flow 2: done -> acknowledge -> next fill --------------------------

// ITEM CASE: complete flow 2. The third press's recovery is measured from
// the VALVE-CLOSE instant (first trip), pinned ms-exact — the table alone is
// too coarse (20 min vs 19.97 min both interpolate to 38).
void test_flow2_done_ack_next_fill() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);
    tripAt(s, 1, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::DonePulse);
    const uint32_t tClose = tOpen + 60000u;  // §7.8 reference instant
    const FillEngineOutputs& oAck = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(oAck.acknowledged);
    TEST_ASSERT_TRUE(oAck.led == LedPatternEngine::Mode::Off);
    s.run(20u * 60000u);
    const uint32_t tPress3 = s.now;
    s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(tPress3 - tClose, s.eng.deadline().recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(38, s.eng.deadline().tableMinutes());  // 20-min row
}

// ---- fault paths (§7.5–§7.7) ------------------------------------------------

// ITEM CASE: confirm window times out -> fault the OTHER sensor (never
// tripped). Timeout inclusive at 12.000 s.
void test_confirm_timeout_faults_other_sensor() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);  // S1 trips; S2 stays dry
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.run((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: wet reading inside EARLY_WET_WINDOW_S -> guard faults that
// sensor immediately (false trip / short).
void test_early_wet_fault() {
    Sim s;
    s.pressAndOpen();
    s.a1 = WET;
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: trip ACCEPTED before 60 s (8th wet emission at 59.8 s) is a
// false trip -> EarlyTripFault (strict '<' window).
void test_early_trip_fault() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
}

// ITEM CASE: a trip accepted AT EXACTLY 60.000 s is LEGAL -> Confirm.
void test_trip_exactly_60s_legal() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
}

// ITEM CASE: backstop expires with no trip -> fault code 3; the valve was
// open for exactly the budget (fresh boot: 30 min).
void test_backstop_expiry_fault3() {
    Sim s;
    s.pressAndOpen();
    s.run(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BackstopExpired);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- +5 extension (§5.1/§6) --------------------------------------------------

// ITEM CASE: one +5 extends the fresh-boot 30 min to 35 — the fill now
// survives past 30 min but faults at 35.
void test_plus5_extends_backstop() {
    Sim s;
    s.pressAndOpen();
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::LongPress);
    TEST_ASSERT_TRUE(o.plus5Applied);
    TEST_ASSERT_TRUE(o.ackBlink);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::Plus5);
    TEST_ASSERT_EQUAL_UINT32(35, o.logs[0].a);
    TEST_ASSERT_EQUAL_UINT32(35, s.eng.deadline().budgetMinutes());
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen);
    s.run(30u * 60000u);  // past the ORIGINAL deadline — still filling
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    s.run(5u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
}

// ITEM CASE: +5 stacks (every long release re-fires) and the edges are
// one-step pulses.
void test_plus5_stacks_edges_each_press() {
    Sim s;
    s.pressAndOpen();
    for (int i = 1; i <= 3; ++i) {
        const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::LongPress);
        TEST_ASSERT_TRUE(o.plus5Applied && o.ackBlink);
        TEST_ASSERT_EQUAL_UINT32(30u + 5u * (uint32_t)i, s.eng.deadline().budgetMinutes());
    }
    const FillEngineOutputs& o2 = s.pass();
    TEST_ASSERT_FALSE(o2.plus5Applied || o2.ackBlink);
}

// ---- stream scope + fuzz -----------------------------------------------------

// ITEM CASE: readings are ignored outside a fill — wet sensors in IDLE never
// fault or log.
void test_readings_ignored_outside_fill_states() {
    Sim s;
    s.a1 = WET;
    s.a2 = WET;
    s.run(20000);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logCount);
}

// ITEM CASE: seeded fuzz over random presses and wet/dry scripts — the
// structural laws hold at EVERY step of every sequence:
//   A) valve level high only in FILLING
//   B) §7.9: every Filling/Confirm exit logs ValveClosed FIRST
//   C) FILLING is entered only via Idle+ShortPress (human press)
//   D) valve high implies a human start occurred earlier
//   E) FAULT latches (no button exit)
//   F) log capacity respected
void test_fuzz_lifecycle_invariants() {
    const uint32_t SEED_BASE = 0xF11EC0DEu;
    const int SEQUENCES = 64;
    const int PASSES = 3000;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;
        Sim s;
        bool humanStart = false;
        for (int i = 0; i < PASSES; ++i) {
            const uint32_t r = rngStep(rng) % 1000u;
            ButtonClassifier::Event ev = ButtonClassifier::Event::None;
            if (r < 25u) ev = ButtonClassifier::Event::ShortPress;
            else if (r < 50u) ev = ButtonClassifier::Event::LongPress;
            s.a1 = (rngStep(rng) % 100u < 70u) ? DRY : WET;
            s.a2 = (rngStep(rng) % 100u < 70u) ? DRY : WET;
            const FillState before = s.eng.state();
            const FillEngineOutputs o = s.pass(ev);
            const FillState after = s.eng.state();
            TEST_ASSERT_TRUE(!o.valveOpen || after == FillState::Filling);              // A
            if (before != after &&
                (before == FillState::Filling || before == FillState::Confirm)) {       // B
                TEST_ASSERT_TRUE(o.logCount >= 1);
                TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);
            }
            if (after == FillState::Filling && before != FillState::Filling) {          // C
                TEST_ASSERT_TRUE(before == FillState::Idle &&
                                 ev == ButtonClassifier::Event::ShortPress);
                humanStart = true;
            }
            if (o.valveOpen) TEST_ASSERT_TRUE(humanStart);                              // D
            if (before == FillState::Fault) TEST_ASSERT_TRUE(after == FillState::Fault);// E
            TEST_ASSERT_TRUE(o.logCount <= FILL_LOG_CAPACITY);                          // F
        }
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_boot_idle_all_off);
    RUN_TEST(test_press_starts_fill_backstop30_led_solid_valve_still_closed);
    RUN_TEST(test_baseline_ok_opens_valve_and_anchors_deadline);
    RUN_TEST(test_baseline_fault_s1_refuses_valve);
    RUN_TEST(test_baseline_fault_s2_refuses_valve);
    RUN_TEST(test_happy_path_s1_first);
    RUN_TEST(test_happy_path_s2_first);
    RUN_TEST(test_double_trip_same_step_done);
    RUN_TEST(test_flow1_abort_in_settle_restarts_recovery);
    RUN_TEST(test_flow1_abort_midfill_recomputes);
    RUN_TEST(test_flow2_done_ack_next_fill);
    RUN_TEST(test_confirm_timeout_faults_other_sensor);
    RUN_TEST(test_early_wet_fault);
    RUN_TEST(test_early_trip_fault);
    RUN_TEST(test_trip_exactly_60s_legal);
    RUN_TEST(test_backstop_expiry_fault3);
    RUN_TEST(test_plus5_extends_backstop);
    RUN_TEST(test_plus5_stacks_edges_each_press);
    RUN_TEST(test_readings_ignored_outside_fill_states);
    RUN_TEST(test_fuzz_lifecycle_invariants);
    return UNITY_END();
}
```

**Test-math notes**: the recovery asserts use the identity `now − recoveryMs(now) == reference instant` (valid for any `now` after the fill ended), which pins §7.8/§6.1 references ms-exactly — the backstop table alone is too coarse (20 min vs 19.97 min both interpolate to 38). `tripAt` asserts the emission-grid alignment (`tOpen % 100 == 0`) internally, so an exact-boundary test that drifts fails loudly rather than silently testing the wrong edge.

### Integration Points

```yaml
LOOP (P1.M6.T2.S1 — the consumer; do NOT implement here):
  - owns: "static FillEngine s_engine;" + the aggregator/button globals that
    already exist (s_adc, s_adcAvg1/2, s_button).
  - per pass: "const FillEngineOutputs o = s_engine.step(nowMs, ev,
    readingDueFlag, s_adcAvg1, s_adcAvg2);" where readingDueFlag is true iff
    takeReading fired THIS pass.
  - valve: "digitalWrite(PIN_VALVE, o.valveOpen ? HIGH : LOW);" — the ONLY
    valve write; never wire fillApplyButton's valveOpen at press time.
  - LED: call s_led.setMode(o.led, o.faultCode) ONLY when o.led (or faultCode)
    CHANGES (static last-mode tracker); "if (o.ackBlink) s_led.triggerAck();"
  - logs: "for (uint8_t i = 0; i < o.logCount; ++i) logLine(...per Kind...);"

FAULT LATCH (P1.M6.T1.S3 — next task): consumes faultCode()/out.faultCode for
  FaultBlink groups 1..3 and hardens/verifies latching; this engine already
  determines the code at Fault entry and latches structurally (table ignores
  presses in Fault; reset = re-construction).

CONFIG: none — config.h FROZEN (§8). Consumed constants: CONFIRM_WINDOW_S (+
  the ones used by the composed services); widen seconds BEFORE x1000.

BUILD: none — platformio.ini UNCHANGED; test/test_fillengine/ auto-discovered
  as the 13th native project.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio test -e native        # exit 0, 13 projects all [PASSED] (12 existing + test_fillengine)
pio run -e pico           # ========================= [SUCCESS]
# NO ruff/mypy (C++ project). NO `pio test -e pico` (no on-device transport).
# The two long sims (30/35-min backstop runs ≈ 72–84 k passes) finish in
# well under a second natively — if the suite suddenly takes seconds, a test
# loop is spinning; investigate, do not raise timeouts.
```

### Level 2: Structure & Diff Validation

```bash
grep -c "Fill lifecycle engine (P1.M6.T1.S2)" firmware/jug_filler/jug_filler.ino   # -> 1
grep -n "end fill-state transition core\|Fill lifecycle engine\|#ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   -> order MUST be: end fill-state transition core ... THEN Fill lifecycle engine ... THEN the ARDUINO marker
grep -c "class FillEngine" firmware/jug_filler/jug_filler.ino                      # -> 1
grep -c "valveOpen_ = true" firmware/jug_filler/jug_filler.ino                     # -> 1 (the ONLY valve-open site)
git status --short   # -> " M firmware/jug_filler/jug_filler.ino" + "?? test/test_fillengine/"
git diff firmware/jug_filler/config.h platformio.ini   # -> EMPTY (untouched)
```

### Level 3: Contract Validation

```bash
grep -c "RUN_TEST" test/test_fillengine/test_fillengine.cpp    # -> 20
grep -n "SEED_BASE\|1664525u" test/test_fillengine/test_fillengine.cpp   # -> seeded LCG present
pio test -e native -f test_fillengine && pio test -e native -f test_fillengine
#   -> [PASSED] twice, identically (seeded — bit-reproducible)
# Purity: the new block contains NO digitalWrite/Serial/millis/delay calls.
```

### Level 4: On-Device Bench Validation — P1.M7.T2.S1's deliverable, NOT this task's

Pure logic; no upload target configured. Success Criteria end at Levels 1–3.

## Final Validation Checklist

### Technical Validation

- [ ] `pio test -e native` → exit 0, 13/13 projects `[PASSED]` (incl. `test_fillengine`, 20 tests)
- [ ] Deterministic re-run of `test_fillengine` passes identically (seeded fuzz)
- [ ] `pio run -e pico` → `[SUCCESS]`
- [ ] Level-2 greps return expected counts/order; diff purely additive; ARDUINO region byte-identical

### Feature Validation

- [ ] Happy paths both sensor orders; same-step double trip; confirm-timeout → other-sensor fault; early-wet/early-trip faults with the exact 60 s boundary both sides
- [ ] Baseline faults refuse the valve (never opened) and log ValveClosed first
- [ ] Flow 1 (settle abort + midfill abort) recomputes the backstop from the abort instant (44 / 41 vs 30)
- [ ] Flow 2 complete; §7.8 recovery reference pinned ms-exact to the valve-close instant
- [ ] Backstop expiry → code 3; +5 extends to 35 min and stacks; ackBlink/plus5Applied are one-step edges
- [ ] §7.9 ordering asserted per-scenario and by the fuzz property (law B)
- [ ] Fuzz laws A–F pass at every step of 64 seeded × 3000-pass sequences

### Code Quality Validation

- [ ] House conventions: bannered block, dense WHY-comments, camelCase methods, zero-init fail-safe outputs, elapsedMs/deadlinePassed for ALL timing (no direct timestamp comparison)
- [ ] Reading routed exactly once per step; no detector feeding outside Filling/Confirm; detectors armed only with validated baselines
- [ ] No new files beyond `test/test_fillengine/`; config.h, platformio.ini, ARDUINO region untouched

### Documentation & Deployment

- [ ] Block comments document the sub-phase model, ordering law, anchor/reference instants and the T2.S1 wiring contract (setMode-on-change!)
- [ ] No logging/pin/Serial calls from the pure region; no new env vars or dependencies

---

## Anti-Patterns to Avoid

- ❌ Don't wire the valve to `fillApplyButton`'s `valveOpen` — it is true from press; the engine's settle gate owns the physical open (S1 PRP explicitly warns this).
- ❌ Don't feed the reading in BOTH the Filling and Confirm branches — exactly-once routing (section ④) is what makes the same-step double trip correct instead of double-fed.
- ❌ Don't skip the ValveClosed event "because the valve is already closed" — the §7.9 law is uniform; P1.M7.T1.S1 asserts it unconditionally.
- ❌ Don't run guards or the backstop check in CONFIRM — provably moot (windows closed; valve already closed); document, don't code.
- ❌ Don't record the recovery reference at the DONE step's `nowMs` — §7.8 says the valve-CLOSE (first-trip) instant; the flow-2 test pins this in ms.
- ❌ Don't call `setMode` every pass in the T2.S1 wiring (documented contract) — it re-anchors the pattern phase and freezes blinking.
- ❌ Don't compare timestamps directly or multiply narrow seconds constants — `elapsedMs`/`deadlinePassed` and `(uint32_t)X * 1000u` everywhere.
- ❌ Don't use `<random>` for the fuzz — the inline seeded LCG is the reproducibility contract.
- ❌ Don't forget the explicit Unity `main()` runner (PIO 6.2.0 generates none).
- ❌ Don't touch the ARDUINO region, config.h, or platformio.ini.

---

**Confidence Score: 9/10** — the complete engine and test file are provided verbatim against surfaces read from the live codebase this session (S1's block verified landed at lines 866–986; all composed service APIs quoted from source); every timing constant in the test harness was derived from the actual config values and cross-checked against each service's boundary semantics. Residual risk: the exact-time trip tests depend on the documented emission-grid alignment (mitigated by the in-`tripAt` alignment assert, which fails loudly rather than silently testing the wrong boundary), and the P1.M6.T1.S3 boundary (fault presentation) is a hand-off, not an overlap — `faultCode()` is the entire interface.
