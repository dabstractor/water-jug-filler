# Research Notes — P1.M5.T1.S1: LedPatternEngine (pure)

## §1 Contract → codebase mapping (verified against the live tree)

| Item contract clause | Codebase fact (verified this session) |
| --- | --- |
| Time utilities input | `elapsedMs(uint32_t, uint32_t)` — UNGUARDED region of `firmware/jug_filler/jug_filler.ino` (proto line 32). Rollover-safe mod-2^32 subtraction. `deadlinePassed` NOT needed here (no absolute deadlines — patterns are phase functions). |
| Config LED constants | `config.h` §6 (lines 116–124): `DONE_PULSE_ON_MS=200`, `DONE_PULSE_OFF_MS=200`, `DONE_PULSE_COUNT=3`, `DONE_GAP_MS=1000`, `FAULT_BLINK_MS=250`, `FAULT_PAUSE_MS=2000`, `ACK_BLINK_MS=100`. All `constexpr`, `uint32_t`/`uint8_t`. §8 is FROZEN — zero new constants. |
| "Solid/Off/DonePulse/FaultBlink(n)" | PRD §4 table + §7 fault table: FILLING/CONFIRM solid, IDLE off, DONE 3x(200/200)+1000 repeat forever (§11 no timeout), FAULT n x(250/250)+2000 repeat, n = fault code 1..3. |
| Ack overlay | PRD §4 transient + §5.1: +5 long-press during FILLING → 3x(on 100/off 100) = exactly 600 ms, then resumes solid. One-shot; valve/fill timing untouched (not this class's business — it only masks LED output). |
| Consumers | P1.M5.T2.S1 (GP14 driver: `digitalWrite(PIN_LED, led.tick(nowMs))`), P1.M5.T3.S1 (WS2812 mirror: color from `mode()`, level from `tick()`), P1.M6.T1.S2 (fill engine: sole `setMode`/`triggerAck` caller). |
| Insertion anchor | ARDUINO marker `// ---- #ifdef ARDUINO REGION: hardware glue ----` at line **712** of the current 841-line `.ino`. P1.M4.T2.S1 **HAS LANDED** (commit 7c97ecb, `test_deadline` present): its end-banner is line 710, blank 711. Our block inserts between (marker-anchored, so order vs T2S1 was irrelevant — and it did land first). |
| Test conventions | `test/test_backstop/`, `test/test_deadline/` pattern: `#include <unity.h>`, relative `.ino` include, empty setUp/tearDown, hand-derived expectations with derivation comments, ITEM CASE tags, explicit `int main()` runner (PIO 6.2.0 — no auto runner). One .cpp per `test/` subdir; PIO auto-discovers; no platformio.ini change. |
| Prototype block | Classes get NO prototype (seam rule #3 is free-functions-only; ButtonClassifier/FillDeadline precedent). Prototype block stays byte-stable. |

## §2 Design decisions

1. **Phase-function model.** The active pattern is a pure function of
   `elapsedMs(nowMs, phaseStartMs_) % cycleLen` — no accumulated state, no
   delay(), wrap-safe by construction for any mode age < 2^32 ms. DONE
   repeats forever (§11 no timeout), FAULT latches (§7) — both fall out of
   the modulo naturally.
2. **Lazy phase anchor.** `setMode(Mode, n)` takes NO timestamp (item contract
   says `setMode(Mode)`): it releases the anchor; the NEXT `tick(nowMs)`
   stamps `phaseStartMs_ = nowMs`. A mid-pattern mode change therefore resets
   the phase cleanly at the next loop pass (sub-ms skew at 100 ms cadences).
   Same lazy-anchor for `triggerAck()`.
3. **Ack = pure 600 ms output mask.** The base pattern's phase keeps running
   under the overlay, so at overlay end the base resumes EXACTLY where it
   would have been (item: "then previous mode (Solid) resumes exactly").
   `setMode` does NOT cancel a running overlay (strictly time-bounded; the
   real trigger only fires during FILLING/Solid anyway).
4. **Boundary convention (house style).** A segment boundary instant belongs
   to the NEW segment: on-windows are `[k*P, k*P+ON)` — t=199 on, t=200 off;
   at t=600 the overlay is already over and the base shows. Matches
   `deadlinePassed`/`readingDue` inclusivity style.
5. **`ACK_BLINK_COUNT = 3`** is a class-local `static constexpr` task constant
   (PRD §4 "3x quickly"), NOT a §8 tunable — precedent:
   `BaselineCapture::READINGS_PER_SENSOR`. Only `ACK_BLINK_MS` is config.
6. **`setMode(FaultBlink, n)`.** C++ enums can't carry the `uint8_t groups`
   payload from the item's `FaultBlink(uint8_t groups)` notation, so groups
   ride as a defaulted second parameter (ignored by other modes). `n==0`
   clamps to 1: an empty group would render "off forever" — a latched fault
   must never look dead. No upper clamp (n is the 1..3 fault code).
7. **Accessors for the mirror.** `mode()` + `faultGroups()` (const) so
   P1.M5.T3.S1 can map color (Off→off, Solid→blue, DonePulse→green,
   FaultBlink→red per §4) without re-deriving state; `ackActive()` for
   diagnostics. Level always comes from `tick()`.
8. **Default mode = Off** — boots into IDLE semantics (PRD §4), and the
   loop's first ticks before any setMode are harmless.

## §3 Test-design notes (two expectation bugs found & fixed during verification)

- **Overlay end boundary:** first draft expected `tick(anchor+600)` = off
  ("last overlay ms"). WRONG under the house boundary convention: 600 is the
  first BASE instant (Solid → on). Last overlay ms is 599. Same convention as
  every other service boundary — pinned in tests now.
- **Rollover hand-arithmetic:** first draft used hand-computed post-wrap hex
  timestamps and forgot the wrap offset (post-wrap raw t differs from elapsed
  by the pre-wrap remainder). Fix: always write `anchor + k` in the test and
  let unsigned arithmetic wrap — reads as "k ms into the pattern" and cannot
  be miscomputed.
- Coverage (12 tests): Off default+explicit; Solid any-age; DonePulse exact
  on/off instants (t=0,199,200,399,400,599,600,799,800,999,1000,1199);
  1000 ms gap [1200,2200) + repetition at 2200/4400 + still pulsing ~1 h in;
  fault n=1/2/3 group shapes + 2000 ms pauses + cycle restarts; n=0 clamp;
  ack 3-blinks-in-600 ms table + boundary resume + one-shot flag; ack over a
  blinking base (phase undisturbed); mid-pattern mode change (×2); wrap-safe
  DonePulse + ack across the uint32 rollover.

## §4 Machine-verification record (this session, host)

- Method (house practice, cf. P1M4T2S1 §4): copied the REAL current `.ino`
  (841 lines, T2S1 landed) to /tmp, inserted the Task-1 block above the
  ARDUINO marker, compiled the Task-2 test with real Unity 2.6.1:
  `g++ -std=c++17 -Wall -Wextra -Werror -I<Unity>/src -Ifirmware/jug_filler
  test_led_pattern.cpp <Unity>/src/unity.c` → **12 tests / 0 failures**,
  zero warnings (`-Werror` clean).
- Purity grep (comments stripped) over the UNGUARDED region: `PURE OK` — no
  millis/delay/Serial/pinMode/digitalWrite/analogRead tokens in code lines.
- Config-by-name grep over the class code lines: NO ms literals
  (200/250/1000/2000/100/600 absent; 600 is computed via `ackSpanMs()`).
- Region markers: 2. Insertion produced exactly one blank line at each seam
  (T2S1 end-banner 710 / blank 711 / new block / blank / marker 712→moved).

## §5 Downstream API freeze (what P1.M5.T2.S1 / T3.S1 / P1.M6.T1.S2 build on)

```cpp
class LedPatternEngine {
public:
    enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2, FaultBlink = 3 };
    static constexpr uint8_t ACK_BLINK_COUNT = 3;
    void setMode(Mode mode, uint8_t faultGroups = 1);  // fill engine ONLY
    Mode mode() const;            // WS2812 color map input
    uint8_t faultGroups() const;  // active fault code 1..3
    void triggerAck();            // +5 long-press during FILLING
    bool ackActive() const;       // overlay still masking (post-tick view)
    bool tick(uint32_t nowMs);    // once per loop pass -> LED level
};
```
Drivers never call setMode/triggerAck; the fill engine never calls tick
directly (it routes through the drivers' loop stage). Level stream = tick();
color stream = mode().
