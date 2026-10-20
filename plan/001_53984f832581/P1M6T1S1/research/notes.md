# Research Notes — P1.M6.T1.S1: Five-state transition core (§5.1 table + valve invariant, pure)

## 1. Codebase surfaces read (all verified directly this session)

### 1.1 Inputs this item consumes
- **`ButtonClassifier::Event`** (jug_filler.ino lines ~109–160, P1.M2.T1.S1): `enum Event : uint8_t { None = 0, ShortPress = 1, LongPress = 2 }`, returned by `update(nowMs, pressedRaw)`. `None` is the steady-state return (including the whole hold); Short/Long fire exactly ONCE per accepted release. Long/short classification (40 ms debounce, 1000 ms threshold) is entirely upstream — **the transition core needs NO time input**. The item contract lists "time utilities P1.M1.T2.S1" as input, but the table itself is time-free; time is consumed downstream by T1.S2 for logging/sequencing. Do not invent a nowMs parameter.
- **`LedPatternEngine::Mode`** (lines ~748+, P1.M5.T1.S1): `enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2, FaultBlink = 3 }`. The item contract's "`LedMode`" does NOT exist as a type — the canonical mapping is `LedPatternEngine::Mode`. FaultBlink's group count (fault code 1..3) is passed to `setMode(mode, n)` and is owned by the fault latcher (P1.M6.T1.S3) — no button event ever enters FAULT, so the table never needs the code.
- **`FillDeadline::extend5()`** (lines ~695, P1.M4.T2.S1): `void extend5() { budgetMs_ += PLUS5_MIN * 60000u; }` — stacks, active fill only, valid before/after `anchor()`. This core does NOT call it (pure, no members); it only emits `plus5Applied` for T1.S2 to wire.

### 1.2 House conventions confirmed
- Seam contract (lines 3–25): UNGUARDED region = pure logic only; every function gets an explicit prototype above its definition; tests `#include "../../firmware/jug_filler/jug_filler.ino"` and run via `pio test -e native`.
- Bannered block style: `// ===================== <name> (task id) =====================` … `// ===== end <name> (consumed by <next tasks>) =====`.
- File-scope types are established precedent (`struct GuardVerdict`, line ~505) alongside free functions (`elapsedMs`, `backstopMinutes`, `medianOf`). Stateful classes hold private members; this item is deliberately STATELESS (see §2).
- Enum style: `enum X : uint8_t` nested in classes, values numbered from 0 "stable for logging". File-scope generic name `State` would be collision-prone → `enum class FillState : uint8_t` chosen (contract's `State`/`LedMode` names are conceptual, like the contract's other pseudo-code).
- Test style (test_deadline/test_led_pattern): per-test `// ITEM CASE:` banner comments, `setUp`/`tearDown` empty, `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }` — **REQUIRED on PIO 6.2.0, no auto-generated runner**. Macros used: TEST_ASSERT_TRUE/FALSE, TEST_ASSERT_EQUAL_UINT32.
- No fuzz/seed pattern exists anywhere in `test/` (grep verified) — this item introduces the first seeded property test. A 3-line deterministic LCG (`s = s*1664525u + 1013904223u`, Numerical Recipes constants) avoids any `<random>` dependency and is bit-reproducible on any host g++.

### 1.3 Placement & parallel-execution (P1.M5.T3.S1 in flight)
- Insertion point: between `// ===== end LED pattern engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====` (~line 864) and `// ---- #ifdef ARDUINO REGION: hardware glue ----` (~line 866). TEXT anchors, never line numbers — T3.S1 is concurrently adding ~100 lines INSIDE the ARDUINO region (below our anchor) plus includes at the top of that region; zero overlap with the UNGUARDED insertion. Do not touch anything inside `#ifdef ARDUINO`.
- Prototypes: the top-of-file prototype list (lines 31–39) sits ABOVE all type definitions, so a prototype for `fillApplyButton(FillState&, ...)` cannot live there (types unknown at that point). Place the prototypes immediately after the `FillOutputs` struct inside the block — satisfies "explicit prototype above its definition" and compiles everywhere.
- config.h: **no changes** (§8 frozen). The core consumes NO config constants (long-press/debounce are classifier-side; PLUS5_MIN is deadline-side; LED cadences are engine-side). platformio.ini: **no changes** — a new `test/test_fillcore/` subdir is auto-discovered as a 12th native test project.

## 2. Key design decisions

### 2.1 Stateless transition function (caller owns the state)
The item contract says "OUTPUT: transition function consumed by the lifecycle orchestrator (P1.M6.T1.S2)". T1.S2 must drive lifecycle transitions (Idle→Filling authorization, →Confirm, →Done, →Fault) from sensor/timer events — if FillCore hid the state behind `onButton()` only, T1.S2 couldn't enter Confirm/Done/Fault without a leaky `setState()`. Chosen shape:

```cpp
enum class FillState : uint8_t { Idle = 0, Filling = 1, Confirm = 2, Done = 3, Fault = 4 };
struct FillOutputs { bool valveOpen; LedPatternEngine::Mode led; bool plus5Applied; bool acknowledged; };
FillOutputs fillLevelOutputs(FillState s);                                  // level half (reused by T1.S2)
FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev);  // the §5.1 table
```

Boot/reset (§5.3) = `FillState st = FillState::Idle;` — the only legal initial value; a reset is re-construction of caller state (RAM-only model, same story as RecoveryClock). Also makes every table cell directly seedable in tests.

### 2.2 Outputs = two LEVELS + two EDGES
- `valveOpen` (LEVEL) == `(result state == Filling)`. This makes §10's "valve only ever HIGH while in FILLING" STRUCTURAL: pure function of the result state. It also holds `true` on FILLING+Long (+5, stays Filling) — correct as a level; the fuzz property encodes "valve open ⇒ in Filling ⇒ (by induction from boot) entered only via Idle+ShortPress".
- `led` (LEVEL) == PRD §4 pattern for the result state: Idle→Off, Filling→Solid, Confirm→Solid ("solid on (continues)"), Done→DonePulse, Fault→FaultBlink. A level field is FORCED: an ignored cell in CONFIRM must still report Solid (an "edge/changed-only" led would leave the orchestrator with nothing to hold).
- `plus5Applied` / `acknowledged` (EDGE, one-shot per event): T1.S2 wires plus5 → `FillDeadline::extend5()` + `LedPatternEngine::triggerAck()` (+5 ack blink, §4) and ack → log line; light-off on acknowledge is the state change itself (led→Off).
- `Event::None` is a total-function no-op (same state, level outputs, no edges) — callers may route every pass; real actions still fire once per classified release upstream.

### 2.3 §5.1 normative table (every cell, implemented exactly)

| State | Event | Next | valveOpen | led | plus5 | ack |
|---|---|---|---|---|---|---|
| Idle | Short | **Filling** | **true** | Solid | – | – |
| Idle | Long | Idle (nothing) | false | Off | – | – |
| Filling | Short | **Idle** (abort) | false | Off | – | – |
| Filling | Long | Filling (stay) | true | Solid | **set** | – |
| Confirm | Short | Confirm (ignored) | false | Solid | – | – |
| Confirm | Long | Confirm (ignored) | false | Solid | – | – |
| Done | Short | **Idle** | false | Off | – | **set** |
| Done | Long | **Idle** (also acks) | false | Off | – | **set** |
| Fault | Short | Fault (ignored) | false | FaultBlink | – | – |
| Fault | Long | Fault (ignored) | false | FaultBlink | – | – |

### 2.4 Valve invariant → property test encoding (seeded fuzz)
From boot `FillState::Idle`, fuzz event sequences (None/Short/Long), asserting at EVERY step:
- A: `out.valveOpen == (state == FillState::Filling)` (§10 valve-high-only-in-FILLING).
- B: the result is Filling only if (prior Idle AND event ShortPress) OR prior Filling — i.e. the ONLY entry into Filling anywhere in the table is the human short-press cell (§10 "no path ever opens the valve automatically").
- C: `out.valveOpen ⇒ some Idle+ShortPress occurred earlier in the sequence` (tracked boolean).
- D/E (cheap extras): `plus5Applied ⇒ (prior Filling AND event LongPress)`; `acknowledged ⇒ (prior Done AND event != None)`.
"Abort long-press does nothing in Idle" = the Idle+Long cell test, reinforced by the fuzz.

### 2.5 Consumption contract for P1.M6.T1.S2 (recorded so T1.S2's PRP can rely on it)
- Owns `FillState st = FillState::Idle;` and calls `fillApplyButton(st, ev)` on classified presses (None routing optional — no-op either way).
- On `plus5Applied`: `s_deadline.extend5(); s_led.triggerAck();` + log. On `acknowledged`: log.
- **CRITICAL wiring note**: `valveOpen` is the state-implied LEVEL, not a "write the pin NOW" command — the physical valve-open instant is gated by T1.S2's baseline stage (PRD §7.1/§6: deadline anchors at valve OPEN, ~2 s settle AFTER the press). Pin HIGH must additionally require baseline-settled && !deadline.expired(). Never wire `digitalWrite(PIN_VALVE, out.valveOpen)` directly at press time.
- Lifecycle transitions: T1.S2 sets state directly and derives levels via `fillLevelOutputs(new state)` (single source of the state→valve/LED mapping); FAULT's blink code comes from P1.M6.T1.S3 and rides `setMode(FaultBlink, n)`.

## 3. Validation commands (verified environment)
- `pio` at `~/.local/bin/pio`, PlatformIO Core 6.2.0; native env = plain host g++ + Unity 2.6.1 (auto-fetched).
- `pio test -e native` currently: 11 projects all PASSED (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline, test_trip, test_guards, test_backstop, test_deadline, test_led_pattern) → 12 with test_fillcore.
- `pio run -e pico` → [SUCCESS] (unguarded region compiles for the device too — the whole .ino builds in both envs).

## 4. External research
None required beyond in-repo patterns: the deliverable is ~60 lines of pure C++ over verified in-repo types; no libraries, no config changes, no hardware. Unity macro/reference: https://docs.throwtheswitch.org/Unity/ (assertion reference) — macros used are already exercised by 11 green sibling suites.
