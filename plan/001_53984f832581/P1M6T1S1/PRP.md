# PRP — P1.M6.T1.S1: Five-state transition core (§5.1 table + valve invariant, pure)

## Goal

**Feature Goal**: Implement PRD §5.1's BOOT-button transition table as ONE pure, total, stateless function over the five fill states (`Idle, Filling, Confirm, Done, Fault`), plus the §10 valve invariant made structural: the commanded valve level is HIGH only in FILLING, and FILLING is entered ONLY by the human Idle+ShortPress cell — no other cell, event, or sequence ever commands the valve open.

**Deliverable**:
1. `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: one bannered block in the UNGUARDED region (between the LED-pattern-engine end banner and the `#ifdef ARDUINO` region marker) containing `enum class FillState`, `struct FillOutputs`, and two free functions (`fillLevelOutputs`, `fillApplyButton`) with explicit prototypes.
2. `test/test_fillcore/test_fillcore.cpp` — NEW: Unity native suite, 18 tests — one per §5.1 table cell (10), boot state, None no-op, +5 stacking, no-reentry-from-lifecycle-states property, §5.2 flows 1 & 2, and a seeded-LCG fuzz property proving the valve invariant. NO other files change (config.h §8 frozen; platformio.ini untouched — new test dir is auto-discovered as the 12th native project).

**Success Definition**: `pio test -e native` → exit 0, all 12 projects `[PASSED]` (11 existing green + new `test_fillcore`); `pio run -e pico` → `[SUCCESS]`; structure greps confirm the block sits in the UNGUARDED region exactly once; the ARDUINO region is untouched (P1.M5.T3.S1 is landing there in parallel — zero overlap); every table cell including the ignored ones is pinned by a dedicated test; the fuzz property passes deterministically (seeded).

## User Persona

**Target User**: The implementing agent of P1.M6.T1.S2 (lifecycle orchestrator) — this core is the button-half of its fill engine — and through it P1.M6.T2.S1 (hardware loop) and P1.M7.T1.S1 (invariant suite).

**Use Case**: Every classified BOOT press must be turned into a state transition + output commands per PRD §5.1, independent of hardware, so the table's exact semantics (including all six "ignored" cells) are provable on the host before any valve is wired.

**User Journey**: boot → caller state = `FillState::Idle`, valve level false → human short-press → `fillApplyButton` returns `{Filling, valveOpen=true, led=Solid}` → mid-fill short-press → abort `{Idle, valveOpen=false, led=Off}` → mid-fill long-press → `+5` edge flag, stays Filling → (lifecycle puts state in Done) → any press → acknowledge `{Idle, led=Off, ack=true}` → next short-press starts the next fill. Confirmed/Done/Fault presses that must be ignored are ignored — provably.

**Pain Points Addressed**: a hand-rolled `if(state==...)` chain in the orchestrator would entangle button semantics with sensor sequencing and be untestable cell-by-cell; the §10 invariant ("no path ever opens the valve automatically") needs a machine-checkable proof, not a code review; "long-press does nothing in IDLE" and "CONFIRM/FAULT ignore everything" are exactly the cells naive implementations forget.

## Why

- PRD §5.1 is the normative button table (both actions fire on release; short <1 s, long ≥1 s — classification is ALREADY done by `ButtonClassifier`, P1.M2.T1.S1). This item is the table's pure embodiment.
- PRD §10 safety invariants: "The valve opens only in FILLING; FILLING is entered only by a human BOOT press. No path — boot, watchdog reset, fault recovery, button — ever opens the valve automatically." The stateless core makes this structural: `valveOpen == (state == Filling)` and the only Filling-entering cell is Idle+ShortPress.
- PRD §5.2 flows 1/2 are normative and must be executable by composition (flow 2's lifecycle hop via T1.S2's state ownership — simulated in tests by direct assignment, which is exactly how T1.S2 will drive it).
- Splits cleanly out of P1.M6.T1.S2 (orchestrator) so sensor/timer sequencing never touches button semantics.

## What

Add ONE bannered block to the UNGUARDED region of `firmware/jug_filler/jug_filler.ino` and ONE new native test project. Exact code in the Implementation Blueprint — copy it verbatim.

**Types** (contract's conceptual `enum State`/`LedMode` map to codebase-reality names — no `LedMode` type exists; `LedPatternEngine::Mode` is canonical):

```cpp
enum class FillState : uint8_t { Idle = 0, Filling = 1, Confirm = 2, Done = 3, Fault = 4 };
struct FillOutputs {
    bool valveOpen;               // LEVEL: commanded valve level == (result state == Filling)
    LedPatternEngine::Mode led;   // LEVEL: PRD §4 pattern for the result state
    bool plus5Applied;            // EDGE:  FILLING long-press (+5, stacks — one flag per press)
    bool acknowledged;            // EDGE:  DONE short OR long press
};
FillOutputs fillLevelOutputs(FillState s);                                  // level half
FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev);  // the §5.1 table
```

**Normative table — implement EXACTLY, every cell, including ignored ones:**

| State | Event | Next state | valveOpen | led | plus5Applied | acknowledged |
|---|---|---|---|---|---|---|
| Idle | ShortPress | **Filling** | **true** | Solid | false | false |
| Idle | LongPress | Idle (nothing) | false | Off | false | false |
| Filling | ShortPress | **Idle** (abort: valve closed) | false | Off | false | false |
| Filling | LongPress | Filling (stay) | **true** | Solid | **true** | false |
| Confirm | ShortPress | Confirm (ignored) | false | Solid | false | false |
| Confirm | LongPress | Confirm (ignored) | false | Solid | false | false |
| Done | ShortPress | **Idle** (acknowledge: light off) | false | Off | false | **true** |
| Done | LongPress | **Idle** (also acknowledges) | false | Off | false | **true** |
| Fault | ShortPress | Fault (ignored) | false | FaultBlink | false | false |
| Fault | LongPress | Fault (ignored) | false | FaultBlink | false | false |

Plus totality over `Event::None` (classifier's steady-state return): no-op in every state — state unchanged, level outputs for that state, no edge flags.

**Semantics decisions (binding):**
- **Stateless**: the caller (T1.S2) owns the `FillState` variable; the function is the pure table `(State, Event) → (State', Outputs)`. Boot/reset (§5.3) = `FillState st = FillState::Idle;` — the ONLY legal initial value; no constructor, no hidden members.
- **valveOpen is a LEVEL** `== (result state == Filling)` — not a "write the pin now" command. (T1.S2 gates the physical open instant behind the ~2 s baseline settle; see Integration Points.)
- **led is a LEVEL** per PRD §4: Idle→Off, Filling→Solid, Confirm→Solid ("solid on (continues)"), Done→DonePulse, Fault→FaultBlink (blink group count = fault code is P1.M6.T1.S3's, not the table's — no button event enters FAULT).
- **plus5Applied/acknowledged are EDGES** — true for exactly one call; +5 stacks because every long release re-fires (T1.S2 maps it to `FillDeadline::extend5()` + `LedPatternEngine::triggerAck()`).
- **No time input**: long/short classification is upstream (ButtonClassifier); the table is time-free. Do NOT add a `nowMs` parameter.

**NOT in scope** (owners — do NOT implement here): lifecycle transitions (Idle→baseline→valve→trip→CONFIRM→DONE = P1.M6.T1.S2), fault latching + blink codes 1/2/3 (P1.M6.T1.S3), any `FillDeadline`/`LedPatternEngine`/pin/log calls (the core only RETURNS outputs), any ARDUINO-region change (P1.M5.T3.S1's mirror is landing there in parallel), config.h changes (§8 frozen — the core consumes zero config constants), platformio.ini changes, docs (item contract: internal).

### Success Criteria

- [ ] UNGUARDED region gains exactly one bannered block, placed between the LED-pattern end banner and `// ---- #ifdef ARDUINO REGION: hardware glue ----`; ARDUINO region byte-identical; diff purely additive
- [ ] `fillApplyButton` reproduces all 10 table cells + None no-op exactly (each pinned by a test asserting ALL five outputs: next state, valveOpen, led, plus5Applied, acknowledged)
- [ ] Boot test: fresh `FillState::Idle` yields valveOpen=false, led=Off (§5.3/§10)
- [ ] +5 stacking test: repeated Filling+Long emits plus5Applied on EVERY call, stays Filling, valve stays true
- [ ] No-reentry property: from Confirm/Done/Fault, NO event (None/Short/Long) reaches Filling or valveOpen=true
- [ ] §5.2 flow 1 (press→fill→abort→press) and flow 2 (press→fill→[lifecycle hop]→Done→ack→next fill) execute as sequences
- [ ] Seeded fuzz (LCG, fixed seeds, ≥512 sequences) passes: valveOpen ⇒ state==Filling (A); Filling entered only via Idle+ShortPress (B); valveOpen ⇒ an Idle+ShortPress occurred earlier (C); plus5/ack edge sanity (D/E)
- [ ] `pio test -e native` → exit 0, 12/12 projects PASSED; `pio run -e pico` → [SUCCESS]
- [ ] `git status --short` → only ` M firmware/jug_filler/jug_filler.ino` + untracked `test/test_fillcore/`

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete copy-paste-ready implementation AND the complete test file are in the Blueprint below; every consumed surface (`ButtonClassifier::Event`, `LedPatternEngine::Mode`) was read from the live file this session; the insertion anchor is quoted as exact text; all validation commands are verified-working repo invocations.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (UNGUARDED region only).
        (1) INSERTION ANCHOR — the banner line `// ===== end LED pattern
        engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====`;
        insert your block AFTER it, BEFORE `// ---- #ifdef ARDUINO REGION:
        hardware glue ----`. Use TEXT anchors — P1.M5.T3.S1 is adding ~100
        lines inside the ARDUINO region in parallel; line numbers shift.
        (2) CONSUMED TYPE A — `class ButtonClassifier` (~line 109): nested
        `enum Event : uint8_t { None = 0, ShortPress = 1, LongPress = 2 }`.
        Read its header comment: events fire ONCE per accepted release.
        (3) CONSUMED TYPE B — `class LedPatternEngine` (~line 748): nested
        `enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2,
        FaultBlink = 3 }`; note `setMode(mode, faultGroups)` carries the
        fault code and `triggerAck()` exists — both are T1.S2's wiring,
        referenced only in comments here.
        (4) SEAM CONTRACT — file header (lines 3–25): pure logic only in
        UNGUARDED region; explicit prototypes; no Arduino calls.
  gotcha: do NOT touch anything inside `#ifdef ARDUINO ... #endif` —
        P1.M5.T3.S1 lands concurrently there; your diff must be UNGUARDED-
        region-only.

- file: firmware/jug_filler/jug_filler.ino (FillDeadline block, ~lines 612–710)
  why: read `FillDeadline::extend5()` — the +5 stacking primitive T1.S2 will
        call on your `plus5Applied` flag. Confirms the core itself must NOT
        hold or extend deadlines (scope boundary).
  pattern: comment style and banner discipline to imitate.

- file: test/test_deadline/test_deadline.cpp
  why: THE test pattern to follow — Unity include of the .ino, per-test
        `// ITEM CASE:` banners, and the REQUIRED explicit runner:
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (PIO 6.2.0 generates no Unity runner).
  pattern: setUp/tearDown empty; TEST_ASSERT_TRUE/FALSE for bools and enum
        equality; TEST_ASSERT_EQUAL_UINT32 for numbers.

- file: plan/001_53984f832581/P1M6T1S1/research/notes.md
  why: this task's research: §1 codebase surfaces + house conventions +
        parallel-execution analysis; §2 design decisions (stateless shape,
        level/edge semantics, property encoding); §2.5 the T1.S2
        consumption contract.

- file: plan/001_53984f832581/P1M5T1S1/PRP.md
  why: CONTRACT for the LED engine you reference (Mode enum values are
        stable-for-logging; triggerAck is the +5 transient). Treat as
        implemented-exactly.
  section: "What" (public surface).

- file: plan/001_53984f832581/P1M2T1S1/PRP.md
  why: CONTRACT for the classifier whose Event enum you consume (release-
        fired, once per press, None steady-state). Treat as
        implemented-exactly.
  section: "What".

- url: https://docs.throwtheswitch.org/Unity/Unity-Assertions-Reference.html
  why: Unity assertion macro reference (all macros used are already
        exercised by 11 green sibling suites).
  critical: do not invent macros; TEST_ASSERT_EQUAL on enum class values
        needs a cast — prefer TEST_ASSERT_TRUE(a == b) form.
```

### Current Codebase tree (verified this session; P1.M5.T3.S1 mid-landing in the ARDUINO region)

```bash
water-jug-filler/
├── platformio.ini            # [env:native] Unity host tests (11 projects, all green);
│                             #   [env:pico] compile-only — NO changes needed (new test dir auto-discovered)
├── firmware/jug_filler/
│   ├── jug_filler.ino        # ~1025 lines: UNGUARDED pure logic (time utils, ButtonClassifier,
│   │                         #   ReadingAggregator, BaselineCapture, TripDetector, FalseTripGuards,
│   │                         #   backstop, RecoveryClock/FillDeadline, LedPatternEngine) +
│   │                         #   ARDUINO region (logging, BOOTSEL, ADC, LED drivers, setup/loop)
│   │                         #   ← INSERT new block at the END of the UNGUARDED region
│   └── config.h              # §8 frozen — READ-ONLY (this item consumes ZERO config constants)
├── test/                     # 11 Unity projects (test_seam … test_led_pattern)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain: PlatformIO Core 6.2.0 at ~/.local/bin/pio; native = host g++ + Unity 2.6.1.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive, UNGUARDED region only): +FillState enum,
│                             #   +FillOutputs struct, +fillLevelOutputs, +fillApplyButton
│                             #   (the §5.1 table + structural valve invariant)
└── test/
    └── test_fillcore/        # NEW — 12th native project (auto-discovered, no ini edit)
        └── test_fillcore.cpp # 18 tests: 10 table cells + boot + None + stacking + no-reentry
                              #   + flows 1/2 + seeded-LCG fuzz property
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — the item contract's `enum State` and `LedMode` are CONCEPTUAL
//   names. No LedMode type exists; use LedPatternEngine::Mode. A file-scope
//   unscoped `State` is collision-prone in a single .ino — use
//   `enum class FillState : uint8_t` (values 0..4, stable for logging —
//   house precedent: ButtonClassifier::Event, LedPatternEngine::Mode).

// CRITICAL — PROTOTYPES: the top-of-file prototype list sits ABOVE all type
//   definitions, so fillApplyButton's prototype CANNOT go there (FillState/
//   FillOutputs unknown at that point). Put both prototypes immediately
//   after the FillOutputs struct inside your block — satisfies the seam
//   rule ("explicit prototype above its definition") and compiles everywhere.

// CRITICAL — Event::None must be a no-op, not an error: ButtonClassifier
//   returns None almost every pass. Total function over all three events.

// CRITICAL — zero-init FillOutputs as the fail-safe default (valveOpen
//   false): `FillOutputs o{};` then override per state. §10 direction.

// GOTCHA — valveOpen is a LEVEL (== state==Filling), so it is true on
//   Filling+Long too (+5 stays filling). That is CORRECT and the fuzz
//   property is written for it: valve open ⇒ in Filling ⇒ (inductively)
//   entered only via Idle+ShortPress. Do NOT make it edge-only.

// GOTCHA — exhaustive switch on enum class can still fall off the end for
//   out-of-range casts: keep the unconditional `return o;` AFTER the switch
//   (no return-inside-case-only pattern) to stay -Wreturn-type clean.

// GOTCHA — no time parameter: classification (40 ms debounce / 1000 ms
//   threshold) already happened in ButtonClassifier. The table is time-free.

// GOTCHA — Unity on PIO 6.2.0 has NO auto-generated runner: the explicit
//   int main() { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); } is
//   REQUIRED (see test_deadline.cpp tail).

// GOTCHA — the seeded fuzz uses a plain inline LCG (s = s*1664525u +
//   1013904223u), NOT <random>: bit-reproducible on any host g++, zero
//   dependencies, and the seed is the reproducibility contract.

// GOTCHA — never `pio test -e pico` (no on-device transport). `pio run -e
//   pico` is the device-side compile gate; on-hardware validation is
//   P1.M7.T2.S1's deliverable, not this task's.
```

## Implementation Blueprint

### Data models and structure

The complete data model IS the deliverable's core (see Implementation Tasks / Patterns). No ORM/Pydantic — this is embedded C++: one enum, one POD struct, two free functions.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY preconditions (do not skip)
  - RUN: grep -n "end LED pattern engine" firmware/jug_filler/jug_filler.ino
    → the anchor banner exists (insertion point confirmed).
  - RUN: grep -n "class ButtonClassifier\|enum Event" firmware/jug_filler/jug_filler.ino
    → Event { None, ShortPress, LongPress } present.
  - RUN: grep -n "enum Mode" firmware/jug_filler/jug_filler.ino → Off/Solid/DonePulse/FaultBlink present.
  - RUN: grep -c "fillApplyButton" firmware/jug_filler/jug_filler.ino → 0 (not already implemented).

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — insert the transition-core block
  - FIND: `// ===== end LED pattern engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====`
  - INSERT the COMPLETE block from "Implementation Patterns & Key Details"
    below AFTER that banner and BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`.
  - NAMING: FillState / FillOutputs (CamelCase types), fillLevelOutputs /
    fillApplyButton (camelCase free functions — house precedent: elapsedMs,
    backstopMinutes); banner `// ===================== Fill-state transition
    core (P1.M6.T1.S1) =====================` … `// ===== end fill-state
    transition core (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====`.
  - PLACEMENT: last block of the UNGUARDED region (all deps — ButtonClassifier
    line ~109, LedPatternEngine line ~748 — are defined above).

Task 2: CREATE test/test_fillcore/test_fillcore.cpp
  - CONTENT: the COMPLETE file from "Implementation Patterns & Key Details"
    below (18 tests + explicit Unity runner).
  - FOLLOW pattern: test_deadline.cpp (ITEM CASE banners, setUp/tearDown,
    RUN_TEST main). NAMING: test_{state}_{event}_{expectation}.
  - PLACEMENT: test/test_fillcore/ (new dir — PlatformIO auto-discovers it;
    platformio.ini UNCHANGED).

Task 3: VALIDATE (see Validation Loop) — pio test -e native (12/12), pio run
  -e pico, structure greps, git status. Fix and iterate until all green.
```

### Implementation Patterns & Key Details

**Complete block — copy-paste-ready** (insert per Task 1):

```cpp
// ===================== Fill-state transition core (P1.M6.T1.S1) =====================
// PRD §5.1's BOOT-button table as ONE pure, total, stateless function over
// the five fill states — plus the §10 valve invariant made structural.
// This core is the BUTTON half of the fill engine only: sensor/timer-driven
// transitions (baseline -> valve -> trip -> CONFIRM -> DONE, fault latching)
// belong to the lifecycle orchestrator (P1.M6.T1.S2 / P1.M6.T1.S3), which
// OWNS the state variable and calls fillApplyButton() on every classified
// press. Long/short classification (40 ms debounce, 1000 ms threshold) is
// entirely upstream in ButtonClassifier (P1.M2.T1.S1) — this table is
// time-free by construction.
//
// Model: STATELESS by design (contrast ButtonClassifier/LedPatternEngine).
// The caller's FillState variable IS the machine; the function is the pure
// table (State, Event) -> (State', Outputs). Every §5.1 cell is therefore
// directly seedable in a test, and P1.M6.T1.S2 drives its lifecycle
// transitions by the same caller-owned-state pattern without fighting a
// hidden member. Boot/reset (PRD §5.3, §10) is simply:
//     FillState st = FillState::Idle;   // the ONLY legal initial value
// — no path in this core ever re-enters Filling except the human
// short-press cell below (no auto-open from any source).
//
// Outputs are two LEVELS + two EDGES:
//   valveOpen  LEVEL == (result state == Filling). §10 made structural: the
//              commanded valve level is high ONLY in FILLING. The PHYSICAL
//              open instant is still gated by P1.M6.T1.S2's baseline stage
//              (PRD §7.1/§6: the deadline anchors at valve OPEN, ~2 s settle
//              after the press) — integrators must NOT wire the pin straight
//              to this flag at press time.
//   led        LEVEL == PRD §4 pattern for the result state (Idle Off,
//              Filling/Confirm Solid — §4 "solid on (continues)", Done
//              DonePulse, Fault FaultBlink). FaultBlink's group count n
//              (fault code, PRD §7) is carried by the fault latcher
//              (P1.M6.T1.S3): no button event ever enters FAULT.
//   plus5Applied  EDGE: FILLING long-press, one flag per release (PRD §5.1
//              "+5 min ... stacks; repeatable"). The orchestrator maps it to
//              FillDeadline::extend5() (P1.M4.T2.S1) + LedPatternEngine::
//              triggerAck() (PRD §4 ack transient) — this core calls neither.
//   acknowledged  EDGE: DONE short OR long press (PRD §5.1 "also
//              acknowledges"). Light-off IS the state change (led -> Off);
//              the flag exists for the log line.
// Event::None (the classifier's steady-state return) is a total-function
// no-op: same state, that state's levels, no edges — callers may route it
// every pass; both real actions still fire exactly once per classified
// release upstream (§5.1 "acts on RELEASE").

enum class FillState : uint8_t {
    Idle    = 0,   // powered, waiting for BOOT press (PRD §4)
    Filling = 1,   // fill authorized: valve level high, timer running
    Confirm = 2,   // first sensor tripped, valve closed, confirm window running
    Done    = 3,   // fill complete: attention pulse until acknowledged
    Fault   = 4,   // latched fault (blink codes, PRD §7)
};

struct FillOutputs {
    bool valveOpen;               // level: commanded valve state (FILLING only)
    LedPatternEngine::Mode led;   // level: PRD §4 pattern for the result state
    bool plus5Applied;            // edge: FILLING long-press (+5, stacks)
    bool acknowledged;            // edge: DONE press (short or long)
};

// Level half of the outputs as a pure function of a state — the single
// source of the state -> valve/LED mapping, reused by P1.M6.T1.S2 when its
// lifecycle transitions (CONFIRM/DONE/FAULT entry) need the same levels.
FillOutputs fillLevelOutputs(FillState s);

// The §5.1 table. Mutates `state` to the cell's result state and returns
// that result's outputs (edge flags set only by the acting cells).
FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev);

FillOutputs fillLevelOutputs(FillState s) {
    FillOutputs o{};  // zero-init: valveOpen=false is the fail-safe default (PRD §10)
    switch (s) {
        case FillState::Filling: o.valveOpen = true; o.led = LedPatternEngine::Mode::Solid; break;
        case FillState::Confirm: o.led = LedPatternEngine::Mode::Solid; break;      // §4: "solid on (continues)"
        case FillState::Done:    o.led = LedPatternEngine::Mode::DonePulse; break;
        case FillState::Fault:   o.led = LedPatternEngine::Mode::FaultBlink; break; // group count: P1.M6.T1.S3
        case FillState::Idle:    break;                                            // defaults: off, closed
    }
    return o;
}

FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev) {
    bool plus5 = false;
    bool ack = false;
    switch (state) {
        case FillState::Idle:
            // Short: START FILL -> FILLING — the ONLY entry into FILLING
            // anywhere in the button table (PRD §10: human BOOT press).
            // Long: nothing.
            if (ev == ButtonClassifier::Event::ShortPress) {
                state = FillState::Filling;
            }
            break;
        case FillState::Filling:
            // Short: ABORT — valve level drops with the state change.
            // Long: +5 to THIS fill's backstop; stays FILLING (valve level
            // unchanged); stacks because every long release re-fires.
            if (ev == ButtonClassifier::Event::ShortPress) {
                state = FillState::Idle;
            } else if (ev == ButtonClassifier::Event::LongPress) {
                plus5 = true;
            }
            break;
        case FillState::Confirm:
            break;  // ignored (both) — PRD §5.1
        case FillState::Done:
            // Short: ACKNOWLEDGE -> IDLE. Long: "also acknowledges".
            if (ev != ButtonClassifier::Event::None) {
                state = FillState::Idle;
                ack = true;
            }
            break;
        case FillState::Fault:
            break;  // ignored (both) — latched until reset (PRD §7)
    }
    FillOutputs out = fillLevelOutputs(state);
    out.plus5Applied = plus5;
    out.acknowledged = ack;
    return out;
}
// ===== end fill-state transition core (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====
```

**Complete test file — copy-paste-ready** (`test/test_fillcore/test_fillcore.cpp`):

```cpp
// test/test_fillcore/test_fillcore.cpp — native contract proof for the
// fill-state transition core (P1.M6.T1.S1). Pins PRD §5.1's BOOT-button
// table cell by cell (including every ignored cell), §5.2's normative flows
// (the lifecycle hop simulated by direct state assignment — the function is
// stateless and the caller owns the state, exactly how P1.M6.T1.S2 drives
// it), boot = Idle/valve closed (§5.3/§10), +5 stacking, and the §10 valve
// invariant as a seeded-fuzz property: no event sequence ever commands the
// valve open except via the human Idle+ShortPress cell.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- helpers ------------------------------------------------------------

// Seed a state, fire one event, capture the result state and ALL outputs.
static FillState runCell(FillState from, ButtonClassifier::Event ev, FillOutputs& out) {
    FillState st = from;
    out = fillApplyButton(st, ev);
    return st;
}

// Deterministic 32-bit LCG (Numerical Recipes constants) — seeded fuzz with
// zero dependencies; bit-reproducible on any host g++ (the seed IS the
// reproducibility contract).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// ---- boot / initial state (PRD §5.3, §10) --------------------------------

// ITEM CASE: boot initial state = Idle with valveOpen=false. The core is
// stateless; "boot" is the caller's initialization — the only legal initial
// value. A reset is re-initialization (RAM-only model, cf. RecoveryClock).
void test_boot_is_idle_valve_closed() {
    FillState st = FillState::Idle;                       // boot line (PRD §5.3)
    FillOutputs out = fillLevelOutputs(st);
    TEST_ASSERT_TRUE(out.valveOpen == false);             // §10: never auto-open
    TEST_ASSERT_TRUE(out.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(out.plus5Applied);
    TEST_ASSERT_FALSE(out.acknowledged);
    // And a None event on a fresh boot changes nothing:
    FillOutputs o2;
    TEST_ASSERT_TRUE(runCell(FillState::Idle, ButtonClassifier::Event::None, o2) == FillState::Idle);
    TEST_ASSERT_FALSE(o2.valveOpen);
}

// ---- §5.1 table, one test per cell (all five outputs pinned each) --------

// IDLE + Short: START FILL -> FILLING, valve level high, Solid, no edges.
void test_idle_short_starts_fill() {
    FillOutputs o;
    const FillState next = runCell(FillState::Idle, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen == true);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// IDLE + Long: nothing (PRD §5.1) — "abort long-press does nothing in Idle".
void test_idle_long_nothing() {
    FillOutputs o;
    const FillState next = runCell(FillState::Idle, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FILLING + Short: ABORT — valve closed -> IDLE.
void test_filling_short_aborts() {
    FillOutputs o;
    const FillState next = runCell(FillState::Filling, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FILLING + Long: +5 applied, STAYS Filling (valve level unchanged), LED ack
// is downstream (plus5Applied flag -> triggerAck in P1.M6.T1.S2).
void test_filling_long_plus5_stays_filling() {
    FillOutputs o;
    const FillState next = runCell(FillState::Filling, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen == true);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_TRUE(o.plus5Applied == true);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// CONFIRM + Short: ignored — state kept, SOLID level held (PRD §4
// "solid on (continues)"), valve closed, no edges.
void test_confirm_short_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Confirm, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Confirm);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// CONFIRM + Long: ignored.
void test_confirm_long_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Confirm, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Confirm);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// DONE + Short: ACKNOWLEDGE — light off -> IDLE (led level Off), ack edge.
void test_done_short_acknowledges() {
    FillOutputs o;
    const FillState next = runCell(FillState::Done, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_TRUE(o.acknowledged == true);
}

// DONE + Long: "also acknowledges" (PRD §5.1) — identical outcome to short.
void test_done_long_also_acknowledges() {
    FillOutputs o;
    const FillState next = runCell(FillState::Done, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_TRUE(o.acknowledged == true);
}

// FAULT + Short: ignored — latched, FaultBlink level held (group count is
// P1.M6.T1.S3's), valve closed, no edges.
void test_fault_short_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Fault, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Fault);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FAULT + Long: ignored.
void test_fault_long_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Fault, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Fault);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// ---- totality & stacking --------------------------------------------------

// Event::None is a no-op in EVERY state (classifier's steady-state return).
void test_none_noop_all_states() {
    const FillState states[5] = {FillState::Idle, FillState::Filling, FillState::Confirm,
                                 FillState::Done, FillState::Fault};
    for (int i = 0; i < 5; ++i) {
        FillOutputs o;
        const FillState next = runCell(states[i], ButtonClassifier::Event::None, o);
        TEST_ASSERT_TRUE(next == states[i]);
        TEST_ASSERT_TRUE(o.valveOpen == (states[i] == FillState::Filling));
        TEST_ASSERT_FALSE(o.plus5Applied);
        TEST_ASSERT_FALSE(o.acknowledged);
    }
}

// +5 stacks / is repeatable (PRD §5.1): every long release re-fires the
// flag; state, valve level, LED all stay FILLING throughout.
void test_plus5_stacks_repeatable() {
    FillState st = FillState::Idle;
    (void)fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    for (int i = 0; i < 3; ++i) {
        FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::LongPress);
        TEST_ASSERT_TRUE(o.plus5Applied == true);          // one edge per press
        TEST_ASSERT_TRUE(st == FillState::Filling);
        TEST_ASSERT_TRUE(o.valveOpen == true);
        TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    }
}

// §10 "no path": from CONFIRM/DONE/FAULT no button event (any of the three)
// ever reaches FILLING or commands the valve open.
void test_no_button_path_reenters_filling() {
    const FillState locked[3] = {FillState::Confirm, FillState::Done, FillState::Fault};
    const ButtonClassifier::Event events[3] = {ButtonClassifier::Event::None,
                                               ButtonClassifier::Event::ShortPress,
                                               ButtonClassifier::Event::LongPress};
    for (int s = 0; s < 3; ++s) {
        for (int e = 0; e < 3; ++e) {
            FillOutputs o;
            const FillState next = runCell(locked[s], events[e], o);
            TEST_ASSERT_TRUE(next != FillState::Filling);
            TEST_ASSERT_FALSE(o.valveOpen);
        }
    }
}

// ---- §5.2 normative flows --------------------------------------------------

// Flow 1 (abort): press -> fill; press -> stops; press -> fills again. (The
// "fresh, recomputed timer" is P1.M6.T1.S2's FillDeadline wiring — out of
// scope here; the STATE story is complete in the table.)
void test_flow1_abort_sequence() {
    FillState st = FillState::Idle;
    FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // abort mid-fill
    TEST_ASSERT_TRUE(st == FillState::Idle && !o.valveOpen);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // next fill
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
}

// Flow 2 (success): press -> fill; fill completes (lifecycle hop — simulated
// by direct state assignment, exactly how P1.M6.T1.S2 will drive it); press
// -> blinking stops (ack -> Idle); press -> next fill starts.
void test_flow2_success_sequence_with_lifecycle_hop() {
    FillState st = FillState::Idle;
    (void)fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(st == FillState::Filling);
    st = FillState::Done;  // P1.M6.T1.S2: second sensor confirmed -> DONE
    TEST_ASSERT_TRUE(fillLevelOutputs(st).led == LedPatternEngine::Mode::DonePulse);
    FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // acknowledge
    TEST_ASSERT_TRUE(st == FillState::Idle && o.acknowledged &&
                    o.led == LedPatternEngine::Mode::Off);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // next fill
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
}

// ---- §10 valve invariant as a seeded-fuzz property -------------------------

// Property: no event sequence ever yields valveOpen=true except via the
// Idle+ShortPress transition. Asserted at EVERY step of every sequence:
//   A) out.valveOpen == (state == Filling)          — valve high only in FILLING
//   B) Filling is ENTERED only by Idle+ShortPress   — the human-press cell
//   C) out.valveOpen => an Idle+ShortPress happened earlier in the sequence
//   D) plus5Applied => (prior Filling AND LongPress)  [edge sanity]
//   E) acknowledged => (prior Done AND event != None) [edge sanity]
void test_fuzz_valve_only_via_idle_short() {
    const uint32_t SEED_BASE = 0x600DF00Du;
    const int SEQUENCES = 512;
    const int MAX_EVENTS = 48;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;  // per-sequence seed
        FillState st = FillState::Idle;                          // boot (PRD §5.3)
        bool humanStartOccurred = false;
        const int n = 1 + (int)(rngStep(rng) % (uint32_t)MAX_EVENTS);
        for (int i = 0; i < n; ++i) {
            const uint32_t r = rngStep(rng) % 3u;
            const ButtonClassifier::Event ev =
                (r == 0u) ? ButtonClassifier::Event::None
              : (r == 1u) ? ButtonClassifier::Event::ShortPress
                          : ButtonClassifier::Event::LongPress;
            const FillState before = st;
            const FillOutputs o = fillApplyButton(st, ev);
            TEST_ASSERT_TRUE(o.valveOpen == (st == FillState::Filling));      // A
            if (before == FillState::Idle && ev == ButtonClassifier::Event::ShortPress) {
                humanStartOccurred = true;
                TEST_ASSERT_TRUE(st == FillState::Filling);                   // B (entry)
            } else {
                TEST_ASSERT_TRUE(st != FillState::Filling || before == FillState::Filling);  // B (no other entry)
            }
            if (o.valveOpen) TEST_ASSERT_TRUE(humanStartOccurred);            // C
            if (o.plus5Applied) {
                TEST_ASSERT_TRUE(before == FillState::Filling &&
                                 ev == ButtonClassifier::Event::LongPress);   // D
            }
            if (o.acknowledged) {
                TEST_ASSERT_TRUE(before == FillState::Done &&
                                 ev != ButtonClassifier::Event::None);        // E
            }
        }
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_boot_is_idle_valve_closed);
    RUN_TEST(test_idle_short_starts_fill);
    RUN_TEST(test_idle_long_nothing);
    RUN_TEST(test_filling_short_aborts);
    RUN_TEST(test_filling_long_plus5_stays_filling);
    RUN_TEST(test_confirm_short_ignored);
    RUN_TEST(test_confirm_long_ignored);
    RUN_TEST(test_done_short_acknowledges);
    RUN_TEST(test_done_long_also_acknowledges);
    RUN_TEST(test_fault_short_ignored);
    RUN_TEST(test_fault_long_ignored);
    RUN_TEST(test_none_noop_all_states);
    RUN_TEST(test_plus5_stacks_repeatable);
    RUN_TEST(test_no_button_path_reenters_filling);
    RUN_TEST(test_flow1_abort_sequence);
    RUN_TEST(test_flow2_success_sequence_with_lifecycle_hop);
    RUN_TEST(test_fuzz_valve_only_via_idle_short);
    return UNITY_END();
}
```

### Integration Points

```yaml
STATE (P1.M6.T1.S2 — the orchestrator, NEXT task):
  - owns: "FillState st = FillState::Idle;" (boot/reset line, PRD §5.3)
  - calls: "fillApplyButton(st, ev)" on every classified press (routing
    Event::None is optional — no-op either way)
  - maps: plus5Applied -> s_deadline.extend5() + s_led.triggerAck() + log;
    acknowledged -> log
  - lifecycle: sets st directly on sensor/timer transitions; derives levels
    via fillLevelOutputs(new state); FAULT blink code from P1.M6.T1.S3 rides
    setMode(FaultBlink, n)
  - CRITICAL WIRING: valveOpen is the state-implied LEVEL, not a "write the
    pin NOW" command — the physical open instant is gated by the ~2 s
    baseline settle (PRD §7.1/§6; deadline anchors at valve OPEN). Pin HIGH
    must require state==Filling && baseline settled && !deadline.expired().
    NEVER digitalWrite(PIN_VALVE, out.valveOpen) directly at press time.

CONFIG: none — config.h is FROZEN (§8). This core consumes ZERO config
  constants (debounce/long-press are classifier-side; PLUS5_MIN is
  deadline-side; LED cadences are engine-side).

BUILD: none — platformio.ini UNCHANGED; test/test_fillcore/ is
  auto-discovered as the 12th native project.

LOGGING: none — pure region; transition logging belongs to P1.M6.T1.S2.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Host compile + full suite (the new project appears automatically):
pio test -e native
# Expected: exit 0, 12 projects all [PASSED]:
#   test_seam test_time_utils test_log_utils test_button test_reading
#   test_baseline test_trip test_guards test_backstop test_deadline
#   test_led_pattern test_fillcore -> [PASSED]

# Device-side compile gate (unguarded region builds for the board too):
pio run -e pico
# Expected: ========================= [SUCCESS]

# NO ruff/mypy (C++ project). NO pio test -e pico (no on-device transport).
```

### Level 2: Structure & Diff Validation

```bash
# Block present exactly once, correctly placed (last block of UNGUARDED region):
grep -c "Fill-state transition core (P1.M6.T1.S1)" firmware/jug_filler/jug_filler.ino   # -> 1
grep -n "end LED pattern engine\|Fill-state transition core\|#ifdef ARDUINO" firmware/jug_filler/jug_filler.ino
#   -> order MUST be: end LED pattern engine ... THEN Fill-state transition core ...
#      THEN the #ifdef ARDUINO region marker

# Core symbols defined exactly once, prototypes present:
grep -c "enum class FillState" firmware/jug_filler/jug_filler.ino       # -> 1
grep -c "struct FillOutputs" firmware/jug_filler/jug_filler.ino         # -> 1
grep -c "FillOutputs fillApplyButton" firmware/jug_filler/jug_filler.ino # -> 2 (prototype + definition)

# Purity: no Arduino calls / no engine-deadline mutation inside the new block
# (visually verify the block contains no digitalWrite/FillDeadline/triggerAck CALLS —
#  comments may mention them).

# Scope discipline:
git status --short   # -> " M firmware/jug_filler/jug_filler.ino" + "?? test/test_fillcore/"
git diff --stat      # -> insertions only, zero deletions
git diff firmware/jug_filler/config.h platformio.ini   # -> EMPTY (untouched)
```

### Level 3: Contract Validation (table exactness)

```bash
# Every §5.1 cell is pinned — 10 dedicated cell tests + boot + None + stack +
# no-reentry + flows + fuzz = 18 RUN_TEST lines:
grep -c "RUN_TEST" test/test_fillcore/test_fillcore.cpp    # -> 18

# Seeded fuzz present (reproducibility contract):
grep -n "SEED_BASE\|1664525u" test/test_fillcore/test_fillcore.cpp   # -> seeded LCG in use

# Re-run to confirm determinism (identical pass, no flakiness):
pio test -e native -f test_fillcore && pio test -e native -f test_fillcore
# Expected: [PASSED] both times (seeded — bit-identical behavior)
```

### Level 4: On-Device Bench Validation — P1.M7.T2.S1's deliverable, NOT this task's

No hardware run is required from this task (pure logic; no upload target configured). When P1.M7.T2.S1 executes its bench dry-run it observes, through T1.S2/T2.S1 wiring: press→fill starts, press→aborts, long-press→+5 with LED ack, done-pulse stops on press, CONFIRM/FAULT presses inert. This PRP's Success Criteria end at Levels 1–3.

## Final Validation Checklist

### Technical Validation

- [ ] `pio test -e native` → exit 0, 12/12 projects `[PASSED]` (incl. new `test_fillcore`, 18 tests)
- [ ] Deterministic re-run of `test_fillcore` passes identically (seeded fuzz)
- [ ] `pio run -e pico` → `[SUCCESS]`
- [ ] All Level-2 structure greps return expected counts/order; diff purely additive

### Feature Validation

- [ ] All 10 §5.1 cells pinned by dedicated tests asserting ALL five outputs each
- [ ] Ignored cells truly ignored (state kept, correct level outputs held, no edges)
- [ ] Boot = Idle/valve-closed test present and passing (§5.3/§10)
- [ ] +5 stacking: flag re-fires per long release, stays Filling
- [ ] No-reentry property: Confirm/Done/Fault × all events never reach Filling/valveOpen
- [ ] §5.2 flows 1 and 2 execute as state sequences (flow 2 via simulated lifecycle hop)
- [ ] Fuzz property A–E passes at every step of 512 seeded sequences from boot

### Code Quality Validation

- [ ] House conventions: bannered block, camelCase free functions, `enum class … : uint8_t` with stable values, explicit prototypes above definitions, dense WHY-comments matching sibling blocks
- [ ] Placement: last block of the UNGUARDED region; ARDUINO region byte-identical (P1.M5.T3.S1 lands there in parallel)
- [ ] Anti-patterns avoided (see below) — especially: no time parameter, no pin/engine/deadline calls, None is a no-op, valveOpen stays a LEVEL
- [ ] No new files beyond `test/test_fillcore/`; config.h and platformio.ini untouched

### Documentation & Deployment

- [ ] Block comments explain the model (stateless, levels vs edges, §10 rationale, T1.S2 wiring contract) — self-documenting; item contract: DOCS none (internal)
- [ ] No logging from the pure region (T1.S2 owns transition logs)
- [ ] No new environment variables, no dependencies

---

## Anti-Patterns to Avoid

- ❌ Don't make the core stateful (hidden `state_` member) — T1.S2 must own the state to drive lifecycle transitions; the contract says "transition function".
- ❌ Don't add a `nowMs` parameter — long/short classification already happened in ButtonClassifier; the table is time-free.
- ❌ Don't treat `Event::None` as an error or assert — it's the classifier's steady-state return; total-function no-op.
- ❌ Don't make `valveOpen` edge-only — it's the state-implied LEVEL (true on FILLING+Long too); the fuzz property is written for level semantics.
- ❌ Don't skip the ignored cells (CONFIRM/FAULT both columns, IDLE long) — the contract demands the table EXACTLY, cell by cell.
- ❌ Don't call FillDeadline/LedPatternEngine/digitalWrite/logLine from the core — pure region; the core only RETURNS outputs.
- ❌ Don't name the enum `State` unscoped at file scope — collision-prone; use `enum class FillState : uint8_t`. Likewise `LedMode` doesn't exist — use `LedPatternEngine::Mode`.
- ❌ Don't put the prototype in the top-of-file prototype list — the types aren't defined up there; prototypes go inside the block after `FillOutputs`.
- ❌ Don't use `<random>` for the fuzz — the inline seeded LCG is the reproducibility contract (bit-identical on any host g++).
- ❌ Don't forget the explicit Unity `main()` runner — PIO 6.2.0 generates none.
- ❌ Don't touch the ARDUINO region, config.h, or platformio.ini.

---

**Confidence Score: 9/10** — the complete implementation and test file are provided verbatim against types read from the live codebase this session (ButtonClassifier::Event, LedPatternEngine::Mode); all validation commands are verified-working repo invocations. Residual risk: P1.M5.T3.S1's parallel landing shifts line numbers (mitigated by TEXT anchors + Task 0 precondition checks) and the T1.S2 boundary interpretation (mitigated by the explicitly recorded consumption contract, which this PRP pins as the interface).
