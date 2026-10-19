# Firmware Architecture — decisions binding all implementation subtasks

Derived from the PRD (spec/SPEC.md), system_context.md (greenfield),
external_deps.md (verified arduino-pico APIs) and hardware_validation.md.

## 1. The one architectural decision: pure-logic/hardware seam

The PRD mandates **two firmware files** — a sketch + `config.h` (§3) — while the
project SOW mandates TDD. Both are satisfied by a **seam inside the sketch**, not
by extra firmware modules:

```
firmware/jug_filler.ino
├── #include "config.h"
├── ── UNGUARDED REGION: pure logic, zero Arduino calls ──────────────
│   elapsedMs / deadlinePassed        (P1.M1.T2.S1)
│   ButtonClassifier                  (P1.M2.T1.S1)
│   ReadingAggregator + medianOf      (P1.M3.T1.S1)
│   BaselineCapture                   (P1.M3.T2.S1)
│   TripDetector + guard evaluation   (P1.M3.T3.S1/S2)
│   backstopMinutes / RecoveryClock / FillDeadline (P1.M4)
│   LedPatternEngine                  (P1.M5.T1.S1)
│   FillEngine (states + lifecycle + faults)       (P1.M6.T1)
├── ── #ifdef ARDUINO region: hardware glue ──────────────────────────
│   setup(): valve LOW first line → pinModes → analogReadResolution(12)
│            → Serial.begin + banner/watchdog_caused_reboot
│            → watchdog_enable(8000, true)
│   loop():  watchdog_update → sample BOOTSEL → button events
│            → sample ADCs → aggregator → FillEngine.step
│            → write valve pin on change (close-first ordering)
│            → LED drivers (GP14 + WS2812 mirror) → logLine events
└── config.h: every §8 tunable, constexpr, grouped, unit-commented

test/test_*.cpp  →  #include "../firmware/jug_filler.ino"  → pio test -e native
```

**Rules of the seam:**
- Pure logic takes `uint32_t nowMs` (and any sensor/event data) as parameters —
  never calls `millis()`, `analogRead`, `digitalRead`, `Serial`, or any library.
- Hardware code exists only in the `#ifdef ARDUINO` region (setup/loop/drivers).
- The `.ino` declares explicit prototypes for every function (native g++ does not
  run the Arduino preprocessor).
- Tests drive scripted clocks and sensor streams; every §4/§5/§7 rule is provable
  on the host. Hardware-adjacent subtasks (wiring, drivers, bench) verify on-device.

## 2. Verified platform facts that shape the code (see external_deps.md)
1. `analogReadResolution(12)` in setup() is **mandatory** (default is 10-bit; all
   §8 thresholds are 12-bit counts).
2. `BOOTSEL` is a bool-like object (`true` = pressed); the PRD §2.1 pseudo-pin
   pattern must NOT be implemented.
3. Watchdog: `watchdog_enable(8000, true)` / `watchdog_update()` first thing each
   loop pass; ceiling 8388 ms.
4. No `while(!Serial)`, no blocking `delay()` anywhere in `loop()`; log lines are
   small and on-change.
5. WS2812 mirror brightness enforced by scaling written RGB to ≤ 60/255.

## 3. Behavioral interpretation decisions (documented; follow in FillEngine)
Where the PRD left micro-semantics open, these decisions are binding:

1. **Baseline settle runs INSIDE FILLING, valve closed.** FILLING = the whole fill
   attempt from press-accept to end. A short-press abort during the 2 s settle is
   honored (§5.1 FILLING row applies), and like any fill end it restarts the
   recovery clock at the abort instant (§6.1 "whatever ended it").
2. **Backstop computed at fill start; deadline anchored at valve-open.** §6
   preamble bounds *valve-open* duration, so `FillDeadline` anchors at the
   valve-open instant; the 2 s settle does not eat into the backstop budget.
3. **Recovery-clock reference = valve-close instant** for normal/confirm-ended
   fills; for aborts/faults = the instant the fill attempt ended (valve close, or
   abort instant if the valve never opened). Fresh boot/reset → no history →
   30-min default (§6.1, §5.3) — trivially true because state is RAM-only.
4. **"Wet" predicate is uniform**: reading ≤ baseline − TRIP_DROP_COUNTS, used by
   the trip rule, the early-wet window, and guards.
5. **Close-before-state-flip ordering**: every path that ends a fill emits the
   valve-close output as its FIRST action (§7.9); the invariant suite (P1.M7.T1.S1)
   asserts this ordering.
6. **+5 extension** mutates only the active fill's deadline, stacks, repeatable;
   it never touches tank history or the next fill's computed backstop (§5.1/§6.2).
7. **DONE blink persists until acknowledged** — no timeout (§11); only a BOOT
   press (short or long) or reset clears it.

## 4. Safety architecture (defense in depth, §10)
- Electrical: NC valve (power loss = closed) → module gate pull-down (floating pin
  = closed) → firmware `valve LOW` first line of setup() (boot = closed) →
  valve HIGH only inside FILLING, reachable only via IDLE+ShortPress (human) →
  hardware watchdog (hang → reset → closed/IDLE) → backstop timer (sensor failure
  → bounded fill) → both-sensors-must-confirm (single-sensor lie → fault).
- Every layer is independently testable except the electrical ones, which are
  qualified at commissioning (§9, hardware_validation.md).

## 5. Testing strategy
- Native Unity via PlatformIO (`pio test -e native`): unit tests per pure
  component + property/fuzz suite over the FillEngine (seeded sequences) +
  structural check that setup()'s first executable line drives the valve LOW and
  that `delay(` never appears in the loop region.
- On-device verification is confined to: BOOTSEL reads (P1.M2.T2.S1), ADC counts
  (P1.M3.T1.S2), LED patterns (P1.M5), integration behavior (P1.M6.T2.S1 bench
  smoke), flows/faults/invariants (P1.M7.T2.S1 bench, USB-only power).
- TDD implied workflow per SOW: failing test → implement → pass; no separate
  "write tests" subtasks exist by design.

## 6. Documentation modes (SOW §5)
- Mode A riders: config.h group comments (P1.M3.T1.S2), README build section
  (P1.M1.T1.S2), README logging formats (P1.M6.T2.S1, P2.M1.T1.S1),
  docs/COMMISSIONING.md (P2.M1.T2.S1), docs/bench/ evidence (P1.M7.T2.S1).
- Mode B final: P2.M1.T3.S1 "Sync changeset-level documentation" depends on every
  implementing subtask and points at spec/SPEC.md as the source of truth.
