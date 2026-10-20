# Research notes — P1.M6.T2.S1 (setup()/loop() wiring)

Session date: verified against live source this session (post-S2 .ino, 1589 lines).

## 1. What already exists in the ARDUINO region (read: lines 1293–1589)

The ARDUINO region is NOT a stub — four prior tasks landed hardware glue there:

- **logLine** (1308–1325): bounded vsnprintf → ≤128 B → one `Serial.write` + `'\n'`.
  Policy (P1.M1.T2.S2): never gated on connection state; can never stall the loop.
- **BOOTSEL service** (1328–1331 + loop stage): `s_button.update(nowMs, pressedRaw)`
  → `ev` (None/ShortPress/LongPress), `BTN short|long %lums` log on event.
- **ADC service** (1351–1356 + loop stage): 3 ms paced `analogRead` pairs →
  `s_adc.addSample(0/1, ...)`, then `if (s_adc.readingDue(nowMs)) s_adc.takeReading(
  nowMs, s_adcAvg1, s_adcAvg2);` — the emitted pair lives in `s_adcAvg1/2`, plus a
  1 Hz commissioning line `S1=%u S2=%u`.
- **LED stage** (s_led + wsMirrorUpdate): `tick(nowMs)` → `digitalWrite(PIN_LED,
  ledOn)` + `wsMirrorUpdate(s_led.mode(), ledOn)` — sits LAST by design.
- **setup()** (1503–1527): COMPLETE per PRD §10/§3 — valve LOW first line, pinModes,
  `analogReadResolution(ADC_BITS)`, `Serial.begin(SERIAL_BAUD)` + boot banner
  (incl. `watchdog_caused_reboot()`), `wsMirrorInit()`, `watchdog_enable(8000, true)`
  LAST. **This task adds nothing to setup()** — FillEngine is a static (default ctor
  lands Idle, valve closed); "construct services in Idle" is already satisfied.
- **loop() tail placeholder** (1580–1588): comment states the contract — fill-engine
  routing goes ABOVE the External LED drive stage; nothing above `watchdog_update()`.

## 2. FillEngine consumption API (read: lines 1044–1288)

- `FillEngineOutputs step(uint32_t nowMs, ButtonClassifier::Event ev, bool readingDue,
  uint16_t avg1, uint16_t avg2)` — **EVERY pass** (button events fire on release
  passes; backstop/confirm deadlines checked every pass; guards read freshest pair
  between emissions). The item text's "on reading due, FillEngine.step" is loose
  phrasing — the engine's own docstring is authoritative.
- Levels (safe every pass): `valveOpen`, `led` (LedPatternEngine::Mode), `faultCode`.
- Edges (true one pass): `plus5Applied`, `acknowledged` (both already log-carried),
  `ackBlink` → route to `s_led.triggerAck()`.
- `logs[0..logCount)`: FillLogEvent{kind, sensor(0/1), a, b} — 15 kinds, payload
  meaning documented per-kind in the struct comment (1044–1068). Worst step = 5
  events (FILL_LOG_CAPACITY). **Every fill-ending step has logs[0]==ValveClosed** —
  the physical write must precede the log lines (§7.9 mirror).

## 3. CRITICAL gotcha — setMode is NOT change-gated (read: 766–772)

`LedPatternEngine::setMode()` unconditionally sets `phaseAnchored_ = false` —
calling it every loop pass re-anchors the pattern at every tick: FaultBlink would
sit stuck ON (t=0 is "on" for every blink pattern), DonePulse would freeze at its
first pulse. **The loop MUST shadow (mode, faultCode) and call setMode only on
change.** Shadow init `Mode::Off, 0` matches FillEngine's Idle levels → no spurious
setMode at boot. `triggerAck()` is also unconditional-restart → call only on the
`ackBlink` EDGE (which is already edge-shaped in FillEngineOutputs).

## 4. readingDue handoff shape

ADC stage's `if (s_adc.readingDue(nowMs)) { takeReading(...); }` must gain a
`bool readingDueThisPass` capture (set true inside the if) — that bool is the
engine's third arg. Minimal, surgical edit to the existing stage.

## 5. Valve write policy

`digitalWrite(PIN_VALVE, fill.valveOpen ? HIGH : LOW)` **every pass**, placed
immediately after step() and BEFORE the log loop: idempotent SIO register write
(sub-microsecond, arduino-pico digitalWrite = direct GPIO write), satisfies
"IMMEDIATELY when output changes" with zero edge bookkeeping, and cannot drift.
Physical-first ordering mirrors the engine's logs[0]==ValveClosed law.

## 6. %lu precedent

Existing house code passes uint32_t to `%lu` (`logLine("BTN %s %lums", ..., holdMs)`)
— on arm-none-eabi, uint32_t == unsigned int == 32-bit, same representation as
unsigned long in varargs. Follow the precedent (ms payloads → `%lu`); on RP2040 it
is exact. Note the ARDUINO region never compiles under host g++, so no host
format-pedantry applies.

## 7. Log-format design (this task's normative deliverable)

PRD §3 requires logs of state transitions + raw sensor counts + backstop
computations. Counts exist (`S1=/S2=` 1 Hz). This task adds one logLine per
FillLogEvent, uppercase-tag + key=value style matching `BTN`/`S1=` precedent:
VALVE close / VALVE open budget / FILL start backstop+recovery / BASE ok s1 s2 /
TRIP sensor fillms / CONFIRM pass sensor durms / DONE attemptms openms / ABORT
attemptms / FAULT {baseline|earlywet|earlytrip|confirm|backstop} sensor+code /
PLUS5 budget / ACK done. Sensor payloads print 1-based (e.sensor+1) to match §7
fault codes 1/2. Documented Mode-A-brief in README (full sweep = P2.M1.T3.S1).

## 8. Zero-delay / no-block verification

- `grep -n "delay(" jug_filler.ino` → must return nothing.
- `grep -n "while (!Serial\|while(!Serial"` → nothing (already true).
- Loop pass cost: ≤1 analogRead pair (~µs), one step (pure, sub-µs), ≤5 bounded
  logLines (CDC writes, non-stalling by P1.M1.T2.S2 policy), 2 digitalWrite +
  wsMirrorUpdate (change-gated) — far inside the 8 s watchdog budget (§10).

## 9. Validation surface

- `pio test -e native` — ARDUINO region is `#ifdef`-excluded from native builds;
  suite green = guard against accidental pure-region edits (expect 14 projects
  once P1.M6.T1.S3's test_faultlatch lands; hard gate = test_fillengine green).
- `pio run -e pico` — the ONLY gate that compiles the ARDUINO region (RP2040
  toolchain, .ino.cpp build already proven in .pio/build/pico).
- No upload in this task (bench = P1.M7.T2.S1). No new includes/libs: everything
  consumed (FillEngine, logLine, s_led, s_adc, s_button, digitalWrite/millis) is
  already in the TU or Arduino core.

## 10. Placement summary (edit map, current line numbers)

1. NEW static block `s_fill` + `s_prevLedMode`/`s_prevFaultCode` + `logFillEvent()`
   helper — after the ADC service state block (~1356), before the External LED
   driver state block (~1358). Helper needs logLine (1308) — defined above. ✓
2. loop(): `bool readingDueThisPass = false;` + capture inside the ADC `if`
   (~1566–1569).
3. loop(): insert fill-routing stage between the ADC 1 Hz log block and the
   External LED drive block; delete the placeholder tail comment (1580–1588).
4. README.md: add `## Serial logging` section (Mode A brief).
