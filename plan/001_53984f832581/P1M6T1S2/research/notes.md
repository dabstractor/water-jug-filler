# Research notes — P1.M6.T1.S2 (Fill lifecycle engine, pure)

## 1. Codebase state verified this session

- `firmware/jug_filler/jug_filler.ino` ~1164 lines. UNGUARDED region ends with the
  S1 block: `Fill-state transition core (P1.M6.T1.S1)` at lines 866–986; end banner
  `// ===== end fill-state transition core (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====`
  at line 986; `// ---- #ifdef ARDUINO REGION: hardware glue ----` at line 988.
  → S2's block inserts BETWEEN those two text anchors (line numbers shift; use text).
- S1 contract (landed, verified in file): `enum class FillState : uint8_t {Idle=0,
  Filling=1, Confirm=2, Done=3, Fault=4}`, `struct FillOutputs {valveOpen, led,
  plus5Applied, acknowledged}`, `fillLevelOutputs(FillState)`, `fillApplyButton(
  FillState&, Event)`. Stateless — caller owns the state variable.
- Consumed services (all read in full from the live file):
  - `BaselineCapture`: begin(nowMs) / addReading(nowMs,a1,a2) / done() /
    baseline(sensor) / verdict(sensor) → `Verdict{Ok,TooLow,TooHigh}`. Settle-drop
    internal; 5 accepted pairs post-settle; begin() fully resets.
  - `TripDetector` ×2: begin(validatedBaseline) / feed(avg) / tripped() (latched).
  - `FalseTripGuards`: begin(b1,b2) / evaluate(nowMs, valveOpenedAtMs, tripped1,
    tripped2, avg1, avg2) → `GuardVerdict{kind: Ok/EarlyTripFault/EarlyWetFault, sensor}`.
    STATELESS/const — engine owns consumption. Call every pass while valve open.
  - `RecoveryClock`: recordFillEnd(nowMs) / hasHistory() / recoveryMs(nowMs).
  - `FillDeadline`: beginFill(clock, nowMs) [compute ONCE at press] / anchor(
    valveOpenedAtMs) / expired(nowMs) / extend5() / tableMinutes() / budgetMinutes()
    / deadlineMs() / anchored() / freshBoot() / recoveryMsAtStart().
  - `LedPatternEngine::Mode {Off,Solid,DonePulse,FaultBlink}`; loop owns the
    instance (s_led) — engine only EMITS mode level + ackBlink edge.
- Test conventions: `test/` has 12 Unity projects (incl. test_fillcore); explicit
  `int main(){UNITY_BEGIN(); RUN_TEST(...); return UNITY_END();}` required
  (PIO 6.2.0). New dir auto-discovered; test_fillengine becomes #13.
- config.h (§8, frozen): CONFIRM_WINDOW_S=12, TRIP_MIN_ELAPSED_S=60,
  EARLY_WET_WINDOW_S=5, BASELINE_SETTLE_MS=2000, READING_INTERVAL_MS=100,
  DEBOUNCE_COUNT=8, TRIP_DROP_COUNTS=350, BASELINE_MIN/MAX=2000/4050,
  FRESH_BOOT_BACKSTOP_MIN=30, backstop knots {2,44},{10,41},{20,38},{35,34},{60,30}.
  uint16_t seconds constants MUST be widened to uint32_t BEFORE ×1000.

## 2. Architecture decisions binding this task (firmware_architecture.md §3)

1. Settle runs INSIDE Filling, valve closed; settle-abort honored, restarts
   recovery clock at abort instant.
2. Backstop computed at press; deadline anchored at valve-open (settle doesn't
   eat budget).
3. Recovery ref: valve-close instant for confirm-ended fills; attempt-end
   instant for aborts/faults; fresh boot → 30 min.
4. Uniform wet predicate (≤ baseline − 350).
5. Close-before-state-flip: valve-close output FIRST on every fill-ending path
   (P1.M7.T1.S1 asserts ordering).
6. +5 mutates only the active deadline; stacks.
7. DONE blink persists until acknowledged.

## 3. Design decisions for FillEngine (this PRP)

- ONE class `FillEngine`, one entry point `step(nowMs, ev, readingDue, avg1, avg2)`
  → `FillEngineOutputs{levels: valveOpen/led/faultCode; edges: plus5Applied/
  acknowledged/ackBlink; ordered FillLogEvent logs[5]}`. Loop owns aggregator;
  engine consumes emitted pairs "when due".
- Engine OWNS `FillState state_` and calls fillApplyButton(state_, ev) internally —
  exactly the caller-owned-state pattern S1's PRP anticipated. Engine's valveOpen
  output is its own sub-phase level (settle: closed), NOT btn.valveOpen (S1 PRP
  explicitly warns never to wire that at press time).
- Sub-phases: Filling&&!valveOpen_ = settle; Filling&&valveOpen_ = valve-open
  (guards→trip→backstop order, every pass); Confirm = second-trip vs 12 s timeout
  (inclusive at equality; trip checked first). Guards/backstop provably moot in
  Confirm (first trip passed ≥60 s ⇒ both windows closed; valve already closed).
- §7.9 ordering law: ANY step leaving Filling/Confirm logs ValveClosed FIRST —
  uniform, including baseline fault (never opened), settle abort, Confirm exits
  (already closed), same-step double trip (Filling→Confirm→Done in one step,
  worst-case log = 5 events → FILL_LOG_CAPACITY 5).
- Recovery refs: Done → recordFillEnd(valveClosedAtMs_) (first-trip instant,
  §7.8); abort/fault → recordFillEnd(nowMs).
- LED Solid from PRESS (state Filling includes settle — PRD §4 table + arch §3.1;
  the item text's "open valve … led Solid" clause reconciled to the authoritative
  state table). Documented in PRP so the implementer doesn't "fix" it.
- Reading stream routed exactly ONCE per step, AFTER button processing, BEFORE
  phase work (avoids double-feed on the trip pass; abort pass drops the reading).
- Fault code: sensor+1 for baseline/guard/confirm-timeout faults (sensor 1 wins
  ties — house precedence), 3 for backstop. faultCode accessor + output level =
  the P1.M6.T1.S3 hand-off (blink groups/latching presentation).
- T2.S1 wiring contract: setMode ONLY on mode CHANGE (setMode re-anchors the
  pattern phase every call — calling per pass would freeze patterns);
  setMode(FaultBlink, o.faultCode); o.ackBlink → triggerAck(); one logLine per log event.

## 4. Test-timing math (deterministic sim)

Sim: passes of 25 ms; reading cadence anchored at lastReadingMs=0 ⇒ emissions at
t ≡ 0 (mod 100); press at t=1000 ⇒ baseline accepted from t=3000 (inclusive 2000 ms
settle), 5 pairs → valve opens t=3400 (≡0 mod 100 — alignment provable for all
fills since emissions define the grid). Exact-time tests: trip with 8 wet
emissions ending at fillTime 59 800 (<60 s ⇒ EarlyTripFault) vs 60 000 (exactly
legal ⇒ Confirm) pins the strict-'<' guard boundary. DRY=3500 (Ok verdict),
WET=2500 (below min threshold 3150). Sim models aggregator semantics: scripted
levels snapshot into e1/e2 only at emission. Backstop sims (30/35 min ≈ 72–84 k
25 ms passes) are trivially fast natively.
Flow-1 recomputation: settle-abort at t=1500, restart t=61500 ⇒ recovery 1 min ⇒
44 (vs fresh-boot 30); midfill abort +10 min restart ⇒ knot 10 ⇒ 41.
Flow-2 §7.8 pin: restart recovery measured in ms (table too coarse to distinguish
20 min vs 19.97 min) — assert deadline().recoveryMsAtStart() == press−tClose.

## 5. External research

None needed — no new libraries: pure C++ + Unity 2.6.1, both established in-repo
with verified patterns (test_deadline.cpp runner; assertion macros already
exercised by 12 green suites). Unity reference:
https://docs.throwtheswitch.org/Unity/Unity-Assertions-Reference.html
