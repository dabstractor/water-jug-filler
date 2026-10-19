# Research notes — P1.M2.T1.S1 (ButtonClassifier, pure)

## 1. Verified codebase state (read directly, this session)

- `firmware/jug_filler/jug_filler.ino` currently contains: seam header, `#include "config.h"`,
  prototype block (elapsedMs/deadlinePassed + setup/loop), UNGUARDED region with the S1 time
  helpers (`elapsedMs`, `deadlinePassed`, `HALF_WHEEL_MS`, banner
  `// ============== end time helpers (consumed by P1.M2–P1.M6) ==============`),
  then `// ---- #ifdef ARDUINO REGION: hardware glue ----` marker, setup()/loop() skeleton.
- **P1.M1.T2.S1 outputs exist in the file** despite plan_status "Failed": helpers + prototypes
  present; `test/test_time_utils.cpp` (106 lines) and updated `test/test_seam.cpp` present.
- **P1.M1.T2.S2 is mid-implementation in parallel**: its PRP is the contract — it will add
  `<stdarg.h>/<stdio.h>` includes, prototypes for `formatBootBanner`/`logLine`,
  `formatBootBanner()` below the time helpers, watchdog/logLine glue inside `#ifdef ARDUINO`,
  banner + `watchdog_enable` in setup(), `watchdog_update()` first in loop().
  Its block may or may not be in the file when this task starts → this task's ONLY .ino edit
  anchors on the stable `#ifdef ARDUINO REGION` marker line (works in both worlds) and adds
  NO prototypes (a class needs none; seam rule 3 covers free functions only).
- `firmware/jug_filler/config.h` (READ-ONLY here) already defines:
  `constexpr uint32_t BUTTON_LONG_PRESS_MS = 1000;` and `constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;`
  → zero config changes needed.

## 2. Algorithm choice — integrator-free "stable-sample tracking" debounce

Chosen (matches the item's wording "debounce via stable-sample tracking over 40 ms"):
track `candidate_` (last raw level) + `candidateSince_` (when it first appeared).
Accept a level change only when the candidate has differed from the accepted level
continuously for >= `BUTTON_DEBOUNCE_MS`. Any raw flip restarts the confirmation window.

Properties (each becomes a test):
- Sub-40ms spikes never change the accepted level → a 35ms spike produces NO press ever.
- Mid-hold glitch < 40ms: candidate flips away and back; never accepted; press continues,
  hold measurement NOT restarted (glitch must not split or reset the press).
- Release-side bounce delays release acceptance but cannot change classification.

## 3. Hold-time anchor math (the subtle part)

`update()` is sampled, so acceptance happens at the first poll with elapsed >= 40 —
poll-granularity dependent. Fix: anchor BOTH press and release at the theoretical
acceptance instant `candidateSince_ + BUTTON_DEBOUNCE_MS` (mod-2^32 addition is safe):

- pressStart_ = candidateSince_ + 40 at press acceptance
- hold at release = elapsedMs(candidateSince_(release) + 40, pressStart_)
  = releaseCand − pressCand = exact raw-stable press duration, INDEPENDENT of poll rate.

Boundary derivations (raw stable press from C to R):
- C=0, R=1000 → hold = 1000 → **Long** (>= inclusive, PRD "held ≥ 1000ms")
- C=0, R=999  → hold = 999  → **Short**
- C=15 after bounce (raw flips at 0,10,15), R=500 → hold = 485 → Short
- C=0, R=1500 → 1500 → Long; C=0, R=100 → 100 → Short
- Wrap: C=0xFFFFFF00, R=C+1000=0x2E8 (744ms after wrap) → hold = 1000 → Long
  (0xFFFFFF00+40 = 0xFFFFFF28; 0x2E8+40 = 0x310; elapsedMs(0x310, 0xFFFFFF28) = 1000)

If an implementation anchored the press at the acceptance POLL time instead, test 6
(exactly 1000) would compute 999 → Short and FAIL — that test pins the anchor.

## 4. Events-on-release semantics (PRD §5.1)

- Exactly one event per update() call; None otherwise; NONE while held (mandated test).
- Classifier is state-agnostic: §5.1's per-state actions/ignores (CONFIRM/FAULT ignore)
  belong to the fill engine (P1.M6.T1.S1), NOT here. Classifier only classifies timing.
- Caller contract: poll every loop pass (P1.M6.T2.S1 loop order: watchdog → BOOTSEL sample
  → classifier), nowMs non-decreasing, same clock domain as all other services.

## 5. Test-platform facts (carried from S1/S2 research, verified on this machine)

- PIO 6.2.0 + Unity 2.6.1: every test_*.cpp needs explicit `int main()` with
  UNITY_BEGIN/RUN_TEST/UNITY_END; each file is its own project; include the .ino via
  `#include "../firmware/jug_filler/jug_filler.ino"`.
- UL-suffix big literals; TEST_ASSERT_TRUE/FALSE for booleans; enum comparisons via
  `==` inside TEST_ASSERT_TRUE (Unity's EQUAL macros need ints — cast or compare).
- After this task `pio test -e native` reports N projects (3 if S2 landed, else… actually
  S2 is contractually landing: expect FOUR — seam, time_utils, log_utils, button) —
  the binding expectation is: exit 0, EVERY project PASSED, test_button among them.
- Purity grep over the UNGUARDED region matches COMMENTS too — the class comments must
  avoid the tokens: millis, delay, Serial, pinMode, digitalWrite, analogRead
  (write "nowMs-domain", "confirmation window" — never "millis-domain", never "delay").
- `pio test -e pico` is FORBIDDEN (no transport); `pio run -e pico` is compile-only.

## 6. Mandated test-case → test mapping (item contract)

| Item-mandated case | Test in test_button.cpp |
|---|---|
| sub-40ms bounce spikes ignored | test_spike_under_40ms_ignored (+ bounce variant) |
| short release | test_short_press_fires_short_on_release |
| long release | test_long_press_fires_long_on_release |
| hold exactly 1000ms → Long | test_hold_exactly_1000ms_is_long |
| release at 999ms → Short | test_hold_999ms_is_short |
| no event while still held | test_no_event_while_still_held (0..3000ms) |
| back-to-back presses | test_back_to_back_short_presses |

Extras pinning the design: fresh-classifier silence, bounce-settles press, mid-hold glitch
no-split/no-reset, release-bounce classification, short-then-long sequence, wrap-around hold.
