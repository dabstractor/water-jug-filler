# PRP — P1.M2.T1.S1: ButtonClassifier — 40ms debounce, 1000ms long threshold, events on release (pure)

## Goal

**Feature Goal**: Land the first CLASS in the sketch's UNGUARDED region: `ButtonClassifier`, a pure, hardware-free press/hold classifier implementing PRD §5.1's BOOT-button timing semantics — contact debounce of 40 ms (`BUTTON_DEBOUNCE_MS`), a completed press is Long iff the debounced hold lasted ≥ 1000 ms (`BUTTON_LONG_PRESS_MS`), and BOTH Short and Long events fire on RELEASE (never while held). The clock is injected; zero hardware calls; every semantic proven by native Unity tests.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: a new bannered section `ButtonClassifier (P1.M2.T1.S1)` inserted in the UNGUARDED region immediately above the `// ---- #ifdef ARDUINO REGION: hardware glue ----` marker. A class needs NO prototype-block entry (seam rule 3 covers free functions). NO include changes, NO config changes, NO ARDUINO-region changes.
- `test/test_button.cpp` — NEW Unity test file (13 tests) proving the full contract: sub-40ms spikes ignored, bounce settling, short/long release, exactly-1000ms → Long, 999ms → Short, silence while held, back-to-back presses, mid-hold glitch immunity, release-side bounce, wrap-around hold.

**Success Definition**:
- `pio test -e native` → exit 0, EVERY project `[PASSED]` (test_seam, test_time_utils, test_log_utils once P1.M1.T2.S2 lands, test_button) — test_button: 13 tests green.
- `pio run -e pico` → compile-only `[SUCCESS]` (class compiles under the Arduino/ARM toolchain).
- Purity grep clean: the UNGUARDED region — now including the class — contains zero `millis|delay|Serial|pinMode|digitalWrite|analogRead` tokens, comments included.
- `git status --short`: exactly `.ino` modified + `test/test_button.cpp` added BY THIS TASK (P1.M1.T2.S2 may be landing in parallel in the same tree — attribute its files to it, don't touch them).
- Region markers byte-stable; S1 time helpers and S2's blocks (if present) untouched.

## User Persona

**Target User**: The implementing agents of P1.M2.T2.S1 (BOOTSEL hardware wiring) and P1.M6.T1.S1 (five-state fill engine) — the `ButtonClassifier::Event` stream is their input vocabulary. Secondary: the human on the bench pressing BOOT during P1.M7.T2.S1 dry-runs.

**Use Case**: Each loop pass, the hardware layer samples the BOOT button and calls `classifier.update(nowMs, pressedRaw)`. On the accepted RELEASE of a press the call returns `ShortPress` or `LongPress` (exactly one event, once); the fill engine then applies PRD §5.1's state-dependent action (IDLE+Short → start fill; FILLING+Short → abort; FILLING+Long → +5 min; DONE+any → acknowledge; CONFIRM/FAULT → ignored).

**User Journey**: plug in → press BOOT <1s → release → `ShortPress` returned → fill starts. Mid-fill, hold BOOT 2s → release → `LongPress` → +5 min backstop extension + LED ack. Contact bounce from the physical switch never produces a phantom event because sub-40ms raw excursions cannot change the accepted level.

**Pain Points Addressed**: phantom multi-presses from contact bounce (killed structurally by stable-sample tracking); events firing mid-hold (forbidden — actions on release only); poll-rate-dependent classification (killed by the theoretical acceptance-instant anchoring, see Blueprint); rollover bugs (killed by composing `elapsedMs`).

## Why

- PRD §5.1 (normative): "Contact debounce 40ms; a press is 'long' if held ≥ 1000ms (`BUTTON_LONG_PRESS_MS`). Both short and long actions fire on release." This subtask is exactly those three sentences, as pure code. §4's transient ack (+5min long-press during FILLING) and §5.2's normative flows all key off this event stream.
- Architecture (firmware_architecture.md seam diagram): `ButtonClassifier (P1.M2.T1.S1)` is a named resident of the UNGUARDED region, directly below the time helpers; `loop(): watchdog_update → sample BOOTSEL → button events` names it as the loop's second stage (wired by P1.M2.T2.S1/P1.M6.T2.S1, not here).
- external_deps.md item 1 (verified): BOOTSEL hardware reading is a `__Bootsel` object read as `bool pressed = BOOTSEL;` — that is P1.M2.T2.S1's job; this task receives the raw bool as an injected parameter, keeping the classifier host-testable.
- S1's time helpers exist precisely for this consumer: their PRP's cheat sheet names `elapsedMs(now, lastEdgeMs) >= BUTTON_DEBOUNCE_MS` (P1.M2) and `elapsedMs(now, pressStartMs) >= BUTTON_LONG_PRESS_MS` (P1.M2).

## What

One pure class + one enum, inserted in the UNGUARDED region; one new native test file. No config changes (`BUTTON_DEBOUNCE_MS 40`, `BUTTON_LONG_PRESS_MS 1000` already exist in config.h), no new includes (only `<stdint.h>` types via config.h), no Arduino-region changes, no logging (event logging belongs to P1.M2.T2.S1 via `logLine`).

**Normative decisions**:
- API is exactly `ButtonClassifier` with nested `enum Event : uint8_t { None, ShortPress, LongPress }` and `Event update(uint32_t nowMs, bool pressedRaw)`. NOTHING else public (no getters, no `isPressed()` — scope discipline; the fill engine needs only the event stream).
- Debounce = stable-sample tracking: a raw level must persist continuously ≥ `BUTTON_DEBOUNCE_MS` before it becomes the accepted level; any raw flip restarts the confirmation window. Sub-40ms spikes can NEVER change the accepted level.
- Hold is measured between THEORETICAL acceptance instants (`candidateSince + BUTTON_DEBOUNCE_MS` on both press and release sides) so measured hold = exact raw-stable press duration, independent of poll granularity. ≥ 1000 → Long (inclusive); < 1000 → Short.
- A mid-hold raw glitch < 40 ms neither splits nor restarts the press; a release-side bounce delays acceptance but cannot change classification.
- Events fire ONLY on accepted release; at most one event per call; `None` is the steady-state return, including the whole held period.
- The classifier is state-agnostic: PRD §5.1's per-device-state actions/ignores (CONFIRM/FAULT ignore everything) are the FILL ENGINE's job (P1.M6.T1.S1), not the classifier's.
- Caller contract: `nowMs` non-decreasing (millis() guarantees this in hardware); call every loop pass; same injected clock domain as all other services.

### Success Criteria

- [ ] `.ino` UNGUARDED region contains the class section between the time helpers' end banner and the ARDUINO marker; no prototype-block change; no include change
- [ ] Class is exactly: default constructor, one public `update`, private state (`level_`, `candidate_`, `candidateSince_`, `pressStart_`), nested `Event` enum with the three mandated names
- [ ] `test/test_button.cpp` exists, mirrors `test_seam.cpp`'s shape (include path, setUp/tearDown, explicit `main()`), and all 13 tests pass, covering every item-mandated case: sub-40ms spikes ignored; short release; long release; exactly-1000ms → Long; 999ms → Short; no event while still held; back-to-back presses
- [ ] `pio test -e native` exit 0 with every project `[PASSED]`; `pio run -e pico` compile-only `[SUCCESS]`
- [ ] Purity grep "PURE OK" over the whole unguarded region including the new class comments; region-marker count still 2
- [ ] `git status` scope: only the `.ino` modification + `test/test_button.cpp` added by this task; config.h, platformio.ini, README, test_seam.cpp, test_time_utils.cpp untouched

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP contains the complete verbatim class listing (with the hold-anchor math pre-derived) and the complete verbatim test file (every expected value hand-derived in research/notes.md §3). The insertion anchor (`// ---- #ifdef ARDUINO REGION: hardware glue ----`) is unique in the file and stable whether or not the parallel P1.M1.T2.S2 work has landed. Validation commands are the same ones proven live by the S1/S2 research on this exact machine.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit — current contents: seam header, prototypes, UNGUARDED region
        (time helpers with end-banner), ARDUINO marker, setup()/loop(). Your ONLY edit
        inserts the class section immediately ABOVE the ARDUINO marker line.
  gotcha: P1.M1.T2.S2 lands in parallel — a formatBootBanner block (unguarded) and
        logLine/watchdog glue (ARDUINO region) may appear. Do NOT touch, reorder, or
        duplicate them; your anchor (the ARDUINO marker) works in both states.

- file: firmware/jug_filler/config.h
  why: READ-ONLY. The two constants you consume: BUTTON_LONG_PRESS_MS (1000) and
        BUTTON_DEBOUNCE_MS (40), both constexpr uint32_t. NO new constants — 40/1000
        tunables already exist and must not be duplicated or shadowed in the class.

- file: plan/001_53984f832581/P1M1T2S1/PRP.md
  why: contract for elapsedMs/deadlinePassed (already in the file — verified) and the
        timing vocabulary your class composes; its consumer cheat sheet names the exact
        button patterns. Its test-convention gotchas (explicit main() on PIO 6.2.0, UL
        suffixes, one project per test_*.cpp) apply verbatim to test_button.cpp.

- file: plan/001_53984f832581/P1M1T2S2/PRP.md
  why: contract for the PARALLEL work — logLine() is how P1.M2.T2.S1 will log this
        class's events (not this task; no logging here). Its "Caller cheat sheet"
        shows `logLine("BTN event=%d t=%lu", (int)ev, nowMs);` — the downstream shape.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: seam diagram + "Rules of the seam"
  why: ButtonClassifier is a named UNGUARDED resident below the time helpers; rules:
        injected nowMs, no Arduino calls in pure region, explicit prototypes for free
        functions (a class needs none).
  gotcha: its diagram shows firmware/jug_filler.ino (pre-S2 path) — real path is
        firmware/jug_filler/jug_filler.ino.

- file: plan/001_53984f832581/architecture/external_deps.md
  section: "1. BOOTSEL button — VERIFIED with a BINDING PRD CORRECTION"
  why: the raw-level producer for P1.M2.T2.S1: `bool pressed = BOOTSEL;` (true ==
        pressed, no pinMode needed; the PRD's pinMode(BOOTSEL, INPUT_PULLUP) form is
        WRONG and would silently hit GPIO 0–1). This task only defines the bool's
        meaning: true == pressed.

- file: test/test_seam.cpp
  why: the test-file shape to copy (include path, setUp/tearDown, explicit main(),
        assertion styles). It proves unguarded-region classes/functions reach host tests.

- file: plan/001_53984f832581/P1M2T1S1/research/notes.md
  why: full research digest — algorithm choice, hold-anchor derivations for every
        test's expected value (incl. the wrap test arithmetic 0xFFFFFF00 → 0x2E8),
        purity-token scrub list, mandated-case → test mapping.

- url: https://www.ganssle.com/debouncing-pt2.htm
  why: canonical debounce engineering: vertical integration of a counter/timer per
        state change; "integrate over the debounce interval, then accept" — the
        stable-sample-tracking pattern implemented here. Also documents why <10ms
        excursions must be discarded entirely (they are noise, not state).
  critical: the classic INTEGRATOR (counter++/counter-- on every sample) can leak a
        press through if samples are sparse; the restart-window-on-flip pattern used
        here cannot — that's why it was chosen.

- url: https://eel.is/c++draft/expr#arith.conv
  why: unsigned arithmetic is defined modulo 2^32 — the anchor addition
        (candidateSince_ + BUTTON_DEBOUNCE_MS) and elapsedMs subtraction are
        wrap-safe by definition; never introduce signed casts.
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # contains ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] unity + src filter
├── firmware/
│   └── jug_filler/
│       ├── jug_filler.ino    # seam + S1 time helpers (+ S2 blocks if landed) ← EDIT
│       └── config.h          # all §8 tunables incl. BUTTON_* — READ-ONLY
├── test/
│   ├── test_seam.cpp         # 2 tests (do not touch)
│   ├── test_time_utils.cpp   # 16 tests (do not touch)
│   └── test_log_utils.cpp    # 7 tests (S2, if landed — do not touch)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified by S1/S2 research): pio 6.2.0 at ~/.local/bin/pio,
# host g++ 16.2.1, raspberrypi platform + Unity 2.6.1 under ~/.platformio.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── jug_filler/
│       └── jug_filler.ino    # MODIFIED (additive): ButtonClassifier section in the
│                             #   UNGUARDED region, above the ARDUINO marker
└── test/
    └── test_button.cpp       # NEW: 13 Unity tests = the classifier's full contract
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — purity grep matches COMMENTS: the unguarded region is checked with
//   grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead"
// so the class comments must NOT contain those words. Write "nowMs-domain",
// "injected clock", "confirmation window" — never the banned tokens (e.g. say
// "nowMs-domain timestamp", NOT "millis-domain timestamp").

// CRITICAL: PIO 6.2.0 — test_button.cpp needs an explicit
//   int main() { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); }
// or the link fails with "undefined reference to `main'". Each test_*.cpp is its
// OWN Unity project; `pio test -e native` runs them sequentially.

// CRITICAL: NO prototype-block entry for the class — seam rule 3 (explicit
// prototypes) exists because host g++ skips the .ino prototype generator; a class
// definition is order-independent and needs nothing above it. Adding a prototype
// for a class is a compile error anyway.

// CRITICAL: do NOT compare enum returns with TEST_ASSERT_EQUAL_UINT32 directly —
// Unity's EQUAL macros take ints; use TEST_ASSERT_TRUE(ev == ButtonClassifier::LongPress)
// (or Event::None etc.). Booleans → TEST_ASSERT_TRUE/FALSE, never EQUAL(1, b).

// CRITICAL: hold-anchor math must use the THEORETICAL acceptance instant on BOTH
// sides (candidateSince_ + BUTTON_DEBOUNCE_MS). Anchoring the press at the
// acceptance POLL time makes exactly-1000ms measure 999 → wrong Short. test_hold_
// exactly_1000ms_is_long exists specifically to catch that mistake.

// CRITICAL: PIO 6.2.0 + parallel S2: if `pio test -e native` reports test_log_utils
// missing, S2 hasn't landed yet — that's tolerable ONLY if the plan shows S2
// incomplete; never "fix" it by creating S2's files yourself. test_button must pass
// regardless of S2's state (no dependency between the two).

// Unity 2.6.1: UL-suffix big literals in wrap tests (0xFFFFFF00UL); keep every
// comparison unsigned on both toolchains. uint32_t loop counters in the pump
// helper avoid signed/unsigned warnings.

// Region-marker comment lines are STRUCTURAL ANCHORS for P1.M7.T1.S1 — byte-stable.
// The class section sits BETWEEN the S1 end banner and the ARDUINO marker; never
// reword either marker.
```

## Implementation Blueprint

### Data models and structure

One class, one nested enum, four words of private state. No free functions, no globals, no dynamic memory, no virtuals — trivially copyable, constructible at file scope or on the stack of loop().

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first)
  - RUN: grep -c "elapsedMs" firmware/jug_filler/jug_filler.ino      # EXPECT: >= 3
  - RUN: grep -c "BUTTON_DEBOUNCE_MS" firmware/jug_filler/config.h   # EXPECT: >= 1
  - RUN: grep -c "---- #ifdef ARDUINO REGION: hardware glue ----" \
        firmware/jug_filler/jug_filler.ino                           # EXPECT: 1
  - If any fails → prerequisites absent; STOP and re-check, do NOT recreate anything.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — insert the class
  - EDIT: insert the complete "Task 1 listing" verbatim immediately ABOVE the line
    `// ---- #ifdef ARDUINO REGION: hardware glue ----` (leave one blank line between
    the inserted section and the marker, matching the file's existing spacing).
  - PRESERVE: everything else byte-for-byte — header, includes, prototype block,
    time helpers, S2's blocks if present, entire #ifdef ARDUINO region.
  - NO prototype entry (class), NO new includes (std::uint32_t via config.h).

Task 2: CREATE test/test_button.cpp
  - IMPLEMENT: the complete "Task 2 listing" verbatim (expected values hand-derived
    in research/notes.md §3 — do not "fix" them if a test fails; the CLASS is wrong).
  - PATTERN: test_seam.cpp shape — same include line, setUp/tearDown, explicit
    main() (mandatory on PIO 6.2.0).
  - PLACEMENT: test/ (auto-becomes its own Unity project; no platformio.ini change).

Task 3: RUN native tests (PRIMARY GATE)
  - RUN: pio test -e native
  - EXPECT: exit 0; EVERY project [PASSED] and test_button (13 tests) among them.
    NEVER `pio test -e pico` (no transport there).

Task 4: RUN board compile-only check
  - RUN: pio run -e pico        # NEVER upload
  - EXPECT: [SUCCESS] — proves the class compiles under arm-g++/Arduino builder.

Task 5: RUN structural/safety checks (Level 4 below)
  - EXPECT: "PURE OK" (now includes the class region), markers == 2, git scope exact.
```

**Task 1 listing — the class section (verbatim; comment scrub of purity tokens is deliberate — keep it exactly):**

```cpp
// ===================== BOOT-button classifier (P1.M2.T1.S1) =====================
// Pure press/hold semantics for the BOOT button (PRD §5.1), hardware-agnostic:
// the caller injects the raw level and the time; this class returns the event.
//
//   BUTTON_DEBOUNCE_MS (40)     — a new raw level must persist this long before
//                                 it becomes the accepted level; anything shorter
//                                 is contact bounce and is discarded.
//   BUTTON_LONG_PRESS_MS (1000) — a completed press is Long iff the debounced
//                                 hold lasted >= this; otherwise Short.
//   BOTH events fire on RELEASE — never while held (PRD §5.1 "acts on RELEASE").
//
// Hold-time anchor: press and release are each deemed effective at the theoretical
// acceptance instant (rawEdge + BUTTON_DEBOUNCE_MS). The two debounce windows
// cancel, so measured hold = how long the raw level was continuously pressed —
// independent of how often update() happens to be called. A press raw-stable from
// C to R classifies exactly (R - C) >= 1000.
//
// State-agnostic by design: PRD §5.1's per-state actions/ignores (CONFIRM/FAULT
// ignore everything) belong to the fill engine (P1.M6.T1.S1), which consumes this
// event stream. Caller contract (P1.M2.T2.S1, P1.M6.T2.S1): call update() every
// loop pass with nowMs non-decreasing, same clock domain as all other services.
class ButtonClassifier {
public:
    // Exactly one value per update() call. None is the steady-state return —
    // including the entire time the button is held (no event while held).
    enum Event : uint8_t { None = 0, ShortPress = 1, LongPress = 2 };

    ButtonClassifier()
        : level_(false),          // accepted (debounced) level: not pressed
          candidate_(false),      // raw level being confirmed toward level_
          candidateSince_(0),     // nowMs-domain stamp when candidate_ appeared
          pressStart_(0) {}       // effective start of the current debounced press

    // Feed one raw sample at nowMs. Returns the event caused by this sample:
    // ShortPress or LongPress on the accepted RELEASE of a press, else None.
    // All timing composes elapsedMs() — rollover-safe by construction.
    Event update(uint32_t nowMs, bool pressedRaw) {
        // 1. Stable-sample tracking: any raw flip restarts the confirmation
        //    window, so bounce can never accumulate into an acceptance.
        if (pressedRaw != candidate_) {
            candidate_ = pressedRaw;
            candidateSince_ = nowMs;
        }
        // 2. Accept a pending level change once the candidate has held for the
        //    full debounce interval (>= is inclusive: exactly 40 is enough).
        if (candidate_ != level_ &&
            elapsedMs(nowMs, candidateSince_) >= BUTTON_DEBOUNCE_MS) {
            level_ = candidate_;
            if (level_) {
                // Effective press start = raw edge + debounce (see anchor note).
                // Presses NEVER emit — only releases do (PRD §5.1).
                pressStart_ = candidateSince_ + BUTTON_DEBOUNCE_MS;
                return Event::None;
            }
            // Accepted release: classify the completed hold, emit ONCE, then the
            // state is already reset for the next press (level_ false, candidate
            // tracking continues fresh). Hold = releaseEdge - pressEdge, both
            // anchored the same way, measured rollover-safely.
            const uint32_t holdMs =
                elapsedMs(candidateSince_ + BUTTON_DEBOUNCE_MS, pressStart_);
            return (holdMs >= BUTTON_LONG_PRESS_MS) ? Event::LongPress
                                                    : Event::ShortPress;
        }
        return Event::None;
    }

private:
    bool level_;               // accepted (debounced) level; true == pressed
    bool candidate_;           // latest raw level being confirmed toward level_
    uint32_t candidateSince_;  // stamp (nowMs domain) when candidate_ appeared
    uint32_t pressStart_;      // effective start of the current debounced press
};
// ========= end BOOT-button classifier (consumed by P1.M2.T2.S1, P1.M6.T1.S1) =========
```

**Task 2 listing — `test/test_button.cpp` (complete file):**

```cpp
// test/test_button.cpp — native contract proof for ButtonClassifier (P1.M2.T1.S1).
// Pins PRD §5.1 semantics: 40ms stable-sample debounce (sub-40ms spikes discarded),
// >=1000ms hold = Long, <1000ms = Short, BOTH fire on release, nothing while held.
// Timing model: press raw-stable from C to R (ms) classifies exactly (R - C).
// Expected values hand-derived (plan/.../P1M2T1S1/research/notes.md §3).
#include <unity.h>

#include "../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Pump helper: feed `pressed` at every ms from..until (inclusive); return the
// first non-None event, or None if none occurred. 1ms polls make the raw script
// exact at ms boundaries.
static ButtonClassifier::Event pump(ButtonClassifier& c, uint32_t from,
                                    uint32_t until, bool pressed) {
    for (uint32_t t = from; t <= until; ++t) {
        ButtonClassifier::Event ev = c.update(t, pressed);
        if (ev != ButtonClassifier::None) return ev;
    }
    return ButtonClassifier::None;
}

// Fresh classifier is silent on unpressed input.
void test_fresh_classifier_silent() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 500, false) == ButtonClassifier::None);
}

// ITEM CASE: sub-40ms bounce spike ignored — a 35ms press never happened.
void test_spike_under_40ms_ignored() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 35, true) == ButtonClassifier::None);   // spike
    TEST_ASSERT_TRUE(pump(c, 36, 2000, false) == ButtonClassifier::None); // silence
}

// Bounce (0 true / 10 false / 15 true) settles into ONE press measured from the
// last raw edge: hold = 500 - 15 = 485ms → Short.
void test_bounce_settles_into_press() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 9, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 10, 14, false) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 15, 500, true) == ButtonClassifier::None); // held, no event
    TEST_ASSERT_TRUE(pump(c, 501, 700, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: short release — raw-stable 0..100 → hold 100ms → Short at ~140.
void test_short_press_fires_short_on_release() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 300, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: long release — raw-stable 0..1500 → hold 1500ms → Long.
void test_long_press_fires_long_on_release() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 1499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1500, 1800, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: hold exactly 1000ms → Long (>= inclusive). Also pins the anchor
// math: polling at acceptance gives pressStart = 0+40, hold = 1040-40 = 1000.
// A poll-time-anchored implementation would measure 999 → Short and FAIL here.
void test_hold_exactly_1000ms_is_long() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 999, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1000, 1200, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: release at 999ms → Short (strictly below the threshold).
void test_hold_999ms_is_short() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 998, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 999, 1200, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: no event while still held — 3s of continuous press, silence
// throughout (crossing the 1000ms threshold mid-hold emits nothing).
void test_no_event_while_still_held() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 3000, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 3001, 3400, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: back-to-back presses — two Shorts, second press works immediately
// after the first release (no lockout, no stuck state, no double event).
void test_back_to_back_short_presses() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 200, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 300, 399, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 400, 600, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 601, 800, false) == ButtonClassifier::None); // no extra
}

// Mixed sequence: Short then Long in one classifier lifetime.
void test_short_then_long_sequence() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 300, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 500, 1999, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 2000, 2200, false) == ButtonClassifier::LongPress);
}

// Mid-hold glitch (5ms raw drop at t=500) neither splits nor resets the press:
// hold still measures 1500 → Long (not 1000, not two Shorts).
void test_midhold_glitch_does_not_split_or_reset() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 500, 504, false) == ButtonClassifier::None); // glitch
    TEST_ASSERT_TRUE(pump(c, 505, 1499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1500, 1700, false) == ButtonClassifier::LongPress);
}

// Release-side bounce (false 100 / true 110..114 / false 115) delays acceptance
// but classification uses the LAST raw edge: hold = 115ms → Short.
void test_release_bounce_still_classifies() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 109, false) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 110, 114, true) == ButtonClassifier::None);  // re-bounce
    TEST_ASSERT_TRUE(pump(c, 115, 400, false) == ButtonClassifier::ShortPress);
}

// Wrap safety: press anchored 0xFFFFFF00, held 1000ms → release raw edge wraps
// to 0x2E8. hold = (0x2E8+40) - (0xFFFFFF00+40) = 1000 mod 2^32 → Long.
void test_hold_across_clock_wrap_is_long() {
    ButtonClassifier c;
    const uint32_t pressEdge = 0xFFFFFF00UL;              // 256ms below the wrap
    const uint32_t releaseEdge = pressEdge + 1000UL;      // = 0x2E8 (wrapped)
    TEST_ASSERT_TRUE(pump(c, pressEdge, releaseEdge - 1UL, true) ==
                    ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, releaseEdge, releaseEdge + 300UL, false) ==
                    ButtonClassifier::LongPress);
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_classifier_silent);
    RUN_TEST(test_spike_under_40ms_ignored);
    RUN_TEST(test_bounce_settles_into_press);
    RUN_TEST(test_short_press_fires_short_on_release);
    RUN_TEST(test_long_press_fires_long_on_release);
    RUN_TEST(test_hold_exactly_1000ms_is_long);
    RUN_TEST(test_hold_999ms_is_short);
    RUN_TEST(test_no_event_while_still_held);
    RUN_TEST(test_back_to_back_short_presses);
    RUN_TEST(test_short_then_long_sequence);
    RUN_TEST(test_midhold_glitch_does_not_split_or_reset);
    RUN_TEST(test_release_bounce_still_classifies);
    RUN_TEST(test_hold_across_clock_wrap_is_long);
    return UNITY_END();
}
```

*(13 RUN_TEST lines / 13 test functions — count when copying; every function appears exactly once in main().)*

### Implementation Patterns & Key Details

```cpp
// The SAFETY is in what's absent:
//   - no hardware calls (raw level + clock injected — host-testable, seam rule)
//   - no event queue / no memory (exactly one event per call; the caller polls
//     every loop pass so no release can be missed: a full press-release cycle
//     needs >= ~2 x BUTTON_DEBOUNCE_MS of raw time to be accepted at all)
//   - no signed math anywhere (mod-2^32 unsigned arithmetic is THE rollover safety)
//   - no thresholds besides the two config constants (40/1000 live only in config.h)
//
// Boundary semantics (each pinned by a test):
//   raw-stable C..R  ->  hold = R - C  ->  >= 1000 ? Long : Short
//   debounce acceptance: elapsedMs(now, edge) >= 40  (exactly 40 accepts)
//   mid-hold glitch < 40ms: ignored (hold undisturbed)
//   release bounce: delays acceptance, classification uses the LAST raw edge
//
// CONSUMER CHEAT SHEET (downstream subtasks):
//   P1.M2.T2.S1 (wiring):  bool raw = BOOTSEL;                 // true == pressed
//                           ev = btn.update(nowMs, raw);
//                           if (ev != ButtonClassifier::None)
//                               logLine("BTN event=%d t=%lu", (int)ev, nowMs);
//   P1.M6.T1.S1 (engine):  on ShortPress/LongPress apply the §5.1 state table;
//                           CONFIRM/FAULT ignore; DONE: both ack.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes; UNGUARDED region grows by the
    class section above the ARDUINO marker. #ifdef ARDUINO region untouched — the
    classifier instance itself is instantiated by P1.M2.T2.S1/P1.M6.T2.S1 wiring.
BUILD:
  - No platformio.ini change: [env:native] auto-discovers test_button.cpp as its
    own Unity project; [env:pico] compiles the enlarged sketch.
CONFIG:
  - None. BUTTON_DEBOUNCE_MS and BUTTON_LONG_PRESS_MS already exist and are
    consumed as-is; the class adds zero constants.
DOCS:
  - None (item spec: "DOCS: none — internal"). README NOT updated here.
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - BOOTSEL hardware sampling / event logging   -> P1.M2.T2.S1
  - per-state interpretation (§5.1 table)       -> P1.M6.T1.S1 (fill engine)
  - loop() wiring (watchdog first, then button) -> P1.M6.T2.S1
  - bench validation of real bounce behavior    -> P1.M7.T2.S1
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Native build IS the syntax check — host g++ compiles the class (and rejects any
# stray hardware symbol there):
pio test -e native
# Expected: EVERY project [PASSED], exit 0, test_button among them (13 tests).
# "undefined reference to `main'" => test_button.cpp lacks its explicit main().
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native -v
# Expected: 13 test_button tests PASSED. Failure triage order:
#   (1) transcription slip in main() vs function list;
#   (2) hold-anchor mistake — if exactly-1000ms yields Short, the implementation
#       anchored the press at the acceptance poll time (see Gotchas);
#   (3) confirmation window not restarted on raw flip — if the spike test fails,
#       the candidate tracking is wrong.
# The EXPECTED VALUES are pre-derived; don't "fix" them, fix the class.
```

### Level 3: Integration Testing (System Validation)

```bash
pio run -e pico        # compile-only; NEVER upload in this subtask
# Expected: "[SUCCESS]" under .pio/build/pico/ — proves the class + enum compile
# under the arm-g++/Arduino builder with config.h's constants.

# Scope check — only this task's two files (S2's files, if present, belong to it):
git status --short
# Expected (attributed to THIS task):
#  M firmware/jug_filler/jug_filler.ino
# ?? test/test_button.cpp
# and nothing else beyond whatever P1.M1.T2.S2 legitimately produced in parallel.
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity: unguarded region (now including the class) free of hardware tokens,
#     comments included:
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" || echo "PURE OK"

# (2) Region markers still byte-stable (P1.M7.T1.S1 anchors):
grep -c "---- UNGUARDED REGION: pure logic (host-testable) ----\|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Class landed exactly once, with the mandated API surface:
grep -c "class ButtonClassifier" firmware/jug_filler/jug_filler.ino   # Expected: 1
grep -c "enum Event : uint8_t" firmware/jug_filler/jug_filler.ino    # Expected: 1
grep -c "Event update(uint32_t nowMs, bool pressedRaw)" \
  firmware/jug_filler/jug_filler.ino                                  # Expected: 1

# (4) No shadowed/duplicated button constants in the sketch (config.h is the
#     only home for tunables — the class must reference, not redefine):
grep -n "BUTTON_DEBOUNCE_MS\s*=\|BUTTON_LONG_PRESS_MS\s*=" \
  firmware/jug_filler/jug_filler.ino || echo "NO-SHADOW OK"

# (5) No prototype-block entry added for the class (classes need none):
grep -n "class ButtonClassifier" firmware/jug_filler/jug_filler.ino \
  | head -1   # Expected: exactly one hit, inside the UNGUARDED region (line number
              # after the time helpers, before the ARDUINO marker)
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio test -e native` exit 0, every project `[PASSED]`, test_button 13/13 green
- [ ] `pio run -e pico` → `[SUCCESS]` (compile-only; no upload attempted)
- [ ] `git status --short` shows only this task's `.ino` modification + `test/test_button.cpp`

### Feature Validation

- [ ] All seven item-mandated cases green (spike ignored, short release, long release, exactly-1000→Long, 999→Short, no event while held, back-to-back presses)
- [ ] Wrap-around hold test green (rollover-safe by composition with elapsedMs)
- [ ] Purity "PURE OK"; markers == 2; class exactly once; NO-SHADOW OK
- [ ] No user-facing/config/API/log surface change (item spec: "DOCS: none — internal")

### Code Quality Validation

- [ ] Seam rules honored: injected clock and raw level, zero hardware calls, no class prototype needed
- [ ] S1 time helpers, S2 blocks (if present), test_seam/test_time_utils/test_log_utils byte-stable
- [ ] No scope creep: exactly one class, one enum, one public method; no getters, no extra helpers, no logging
- [ ] Test file mirrors test_seam.cpp conventions (include path, setUp/tearDown, explicit main(), enum-== assertions)

### Documentation & Deployment

- [ ] Class comments document the anchor math, the events-on-release policy, and the caller contract (poll every pass, non-decreasing clock)
- [ ] Consumer cheat sheet present in comments for P1.M2.T2.S1 / P1.M6.T1.S1
- [ ] No new environment variables, no README/config changes

---

## Anti-Patterns to Avoid

- ❌ Don't emit any event while the button is held — PRD §5.1 fires BOTH actions on release only; the held period returns None on every call
- ❌ Don't use an increment/decrement integrator debounce — a flip-restart confirmation window is required (integrators can leak presses at sparse poll rates; the chosen pattern cannot)
- ❌ Don't anchor the press start at the acceptance POLL time — anchor at `edge + BUTTON_DEBOUNCE_MS` on both sides or poll granularity leaks into classification (the exactly-1000ms test catches this)
- ❌ Don't compare raw timestamps directly or use signed math — compose `elapsedMs()` only; unsigned mod-2^32 is the safety
- ❌ Don't add per-state logic (§5.1's CONFIRM/FAULT ignores) into the classifier — that's the fill engine's table (P1.M6.T1.S1)
- ❌ Don't add `isPressed()`, event queues, callbacks, or a second public method — the API is exactly `Event update(uint32_t, bool)`; scope discipline
- ❌ Don't write the purity-banned tokens (millis/delay/Serial/pinMode/digitalWrite/analogRead) anywhere in the unguarded region — including comments; the Level-4 grep matches comments
- ❌ Don't add BUTTON_* constants to the sketch or config.h — they exist; reference, never shadow
- ❌ Don't add a prototype-block entry for the class — only free functions need prototypes (seam rule 3); a class prototype is a compile error
- ❌ Don't omit the explicit `main()` in test_button.cpp — PIO 6.2.0 link failure (verified live)
- ❌ Don't "fix" failing test expectations — expected values are hand-derived (research/notes.md §3); the class is what's wrong
- ❌ Don't touch S1/S2 code, test_seam.cpp, test_time_utils.cpp, test_log_utils.cpp, config.h, platformio.ini, README, or .gitignore; don't run `pio test -e pico` or upload anything

---

**Confidence Score: 10/10** — the deliverable is one self-contained pure class with a complete verbatim listing (anchor math pre-derived and cross-checked), a complete verbatim 13-test file whose expected values are hand-computed, an insertion anchor that is unique and stable under the parallel S2 work, and validation commands already proven working on this exact machine/toolchain by the S1/S2 research (`pio test -e native` green, PIO 6.2.0 + host g++ 16.2.1, per-file Unity projects). No external dependencies beyond the verified-in-file S1 helpers and the two existing config constants.
