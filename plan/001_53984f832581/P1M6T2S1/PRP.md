# PRP — P1.M6.T2.S1: setup()/loop() wiring — services → FillEngine → valve/LED/log, zero delay()

## Goal

**Feature Goal**: Wire the pure, natively-tested `FillEngine` (P1.M6.T1.S1–S3) into the real hardware loop: every `loop()` pass feeds it the BOOT event (BOOTSEL stage), the reading-due flag + latest averaged pair (ADC stage) and the shared `nowMs` clock, and routes its outputs to the three hardware surfaces — `digitalWrite(PIN_VALVE, …)` written FIRST on every fill-ending pass (the physical mirror of the engine's `logs[0]==ValveClosed` ordering law, PRD §7.9), `s_led.setMode(o.led, o.faultCode)` change-gated so blink patterns actually advance, and ONE `logLine` per engine log event. The result is the complete flashable Phase-1 deliverable: `setup()` already satisfies PRD §10/§3 line-for-line (verified — no change needed), and the loop contains **zero `delay()`**, nothing above `watchdog_update()`, and no `while(!Serial)`.

**Deliverable**: (1) A new bannered block in `firmware/jug_filler/jug_filler.ino`'s `#ifdef ARDUINO` region — `static FillEngine s_fill;` + setMode-shadow statics + a `logFillEvent()` helper (complete copy-paste-ready code below); (2) the loop-stage edit: reading-due capture in the ADC stage, the fill-routing stage inserted between the ADC stage and the External LED drive stage, and removal of the placeholder tail comment; (3) a `## Serial logging` section in `README.md` documenting every log-line format (Mode A — brief; full sweep is P2.M1.T3.S1's). No new files, no config.h change, no platformio.ini change.

**Success Definition**: `pio run -e pico` → `[SUCCESS]` (the only gate that compiles the ARDUINO region); `pio test -e native` → exit 0, all projects `[PASSED]` (proves no pure-region code was disturbed); grep gates: zero `delay(`, zero `while (!Serial`, exactly ONE `setMode(` call site + ONE `s_fill.step(` call site in the loop, valve write present in both `setup()` line 1 and the fill stage; `git status --short` shows only `M firmware/jug_filler/jug_filler.ino` and `M README.md` (plus whatever prior uncommitted task work exists — do not touch or revert others').

## User Persona

**Target User**: The bench operator executing P1.M7.T2.S1 (commissioning dry-run, PRD §9) — this task's serial log is their entire observability channel; and the future maintainer reading the loop top-to-bottom.

**Use Case**: The operator flashes the firmware, opens a serial terminal at 115200, presses BOOT, dips sensor tips in a cup, and watches the full §5.2 flow-1/flow-2 lifecycle scroll by as log lines while the LED and valve track the engine — no bench guessing about what the device thinks it is doing.

**User Journey**: power-on → boot banner (`reset cause: …`) → 1 Hz `S1=/S2=` lines → `BTN short` on press → `FILL start backstop=…` → `BASE ok …` → `VALVE open budget=…` → `TRIP sensor=1 …` → `CONFIRM pass …` → `DONE …` → `ACK done` — or the matching `FAULT …` line with the LED blinking the code.

**Pain Points Addressed**: an engine that exists but never touches hardware; silent state transitions invisible over USB; a setMode-per-pass bug that freezes FaultBlink/DonePulse at t=0 (looks like "stuck LED on"); any `delay()` that would blow the §10 watchdog/no-blocking invariant.

## Why

- This task IS the Phase-1 deliverable's completion: PRD §3's "two files" firmware becomes flashable and meets §3–§10 end to end. Every prior task deliberately left this exact seam open (the loop's tail comment: "P1.M6.T2.S1 fills the loop below this line… INSERT ABOVE the External LED drive").
- PRD §3: USB CDC logging of state transitions, raw sensor counts, backstop computations — the engine's `FillLogEvent` stream is the state-transition/backstop half; this task is where it becomes bytes on the wire.
- PRD §10 non-negotiables land here physically: valve pin follows the engine level with ≤ one sub-millisecond pass of latency and is written LOW before the closing log line exists; zero blocking; watchdog fed first every pass (already landed, must be preserved).
- Consumers downstream: P1.M7.T1.S1 (invariant suite) reasons about this exact wiring; P1.M7.T2.S1 (bench) and P2.M1.T1.S1 (log audit) read these exact line formats.

## What

Add the fill-engine routing to the ARDUINO region exactly as specified in the Blueprint: a static `FillEngine` instance (+ change-gated setMode shadow + one-line-per-event `logFillEvent` helper), a `readingDueThisPass` capture in the existing ADC stage, and a new fourth loop stage (between ADC and LED drive) that steps the engine EVERY pass, writes the valve pin first, emits the event lines, routes `ackBlink`, and change-gates `setMode`. `setup()` is verified-complete, unchanged. README gains the serial-log format documentation. NOT in scope (owners): any pure-region/`FillEngine` code change (P1.M6.T1.*), config.h (frozen §8), platformio.ini, native test changes (suites are sibling-owned), on-device bench validation (P1.M7.T2.S1), full docs sweep (P2.M1.T3.S1).

### Success Criteria

- [ ] `pio run -e pico` → `[SUCCESS]`; `pio test -e native` → exit 0, all projects `[PASSED]`
- [ ] Zero `delay(` anywhere in the sketch (grep gate); nothing above `watchdog_update()` in `loop()`
- [ ] `setup()` line 1 still `digitalWrite(PIN_VALVE, LOW);` — byte-identical, untouched
- [ ] Exactly one `s_fill.step(nowMs, ev, readingDueThisPass, s_adcAvg1, s_adcAvg2)` call, executed EVERY pass
- [ ] Valve pin written before any log line of the same step; level follows `fill.valveOpen`
- [ ] `s_led.setMode` called ONLY on (led, faultCode) change; `triggerAck()` only on the `ackBlink` edge
- [ ] All 15 `FillLogEvent::Kind` values render as exactly one `logLine` each (switch is exhaustive, no default that swallows kinds)
- [ ] README `## Serial logging` section lists every line format incl. `S1=/S2=`, `BTN`, boot banner, and the 15 event lines

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: every consumed symbol (`FillEngine::step` signature, `FillEngineOutputs` levels/edges, `FillLogEvent` kinds+payloads, `LedPatternEngine::setMode/tick/triggerAck/mode`, `s_button`/`s_adc`/`s_adcAvg1/2`/`s_led` statics, `logLine`, `nowMs`/`ev` locals, `elapsedMs`) was read from the live source this session; the complete copy-paste-ready code for every edit is in the Blueprint; all validation commands are verified-working repo invocations used by the 13 sibling PRPs.

### Documentation & References

```yaml
- file: firmware/jug_filler/jug_filler.ino
  why: THE file being edited — but ONLY inside the #ifdef ARDUINO region
        (line ~1293 to end). Key landmarks (current numbers; re-grep in
        Task 0 — P1.M6.T1.S3 is test-only so drift is not expected):
        (1) logLine 1308–1325 (bounded, non-stalling — the ONLY logging API).
        (2) Service statics: s_button* 1328–1331, s_adc* 1351–1356,
        s_led ~1371, WS2812 mirror 1405–1501.
        (3) setup() 1503–1527 — COMPLETE: valve-LOW-first, pinModes,
        analogReadResolution(ADC_BITS), Serial.begin(SERIAL_BAUD) + banner,
        wsMirrorInit(), watchdog_enable LAST. DO NOT TOUCH.
        (4) loop() 1529–1588: watchdog_update() first; BOOTSEL stage (ev);
        ADC stage (s_adc.readingDue/takeReading + 1 Hz S1=/S2= log); LED
        drive stage LAST; tail placeholder comment names this task.
        (5) FillEngine class 1081–1288 (pure region, READ-ONLY for this
        task): step(nowMs, ev, readingDue, avg1, avg2) contract,
        FillEngineOutputs 1070–1079, FillLogEvent kinds 1044–1068.
        (6) LedPatternEngine 748–864: setMode is NOT internally
        change-gated — it unconditionally clears phaseAnchored_.
  gotcha: pure region (lines 1–1291) is FROZEN for this task — the native
        suites compile it and are sibling-owned.

- file: plan/001_53984f832581/P1M6T1S2/PRP.md
  why: BINDING CONTRACT for the engine being wired. Its Integration Points
        section names this task: setMode(o.led, o.faultCode) wiring,
        valve-write-immediately, close-before-state-flip preserved.
  section: "Implementation Patterns & Key Details", "Integration Points".

- file: plan/001_53984f832581/P1M6T1S3/PRP.md
  why: contract for the fault half (running in parallel; expected test-only).
        Task 0 gates on its outcome. Its Integration Points re-state the
        T2.S1 wiring law: setMode on CHANGE, valve level every faulted pass.
  gotcha: if its conditional Task-1R remediation touched the .ino, line
        numbers shift — re-grep, never assume.

- file: firmware/jug_filler/config.h
  why: every constant consumed (PIN_VALVE/PIN_LED/PIN_SENSOR1/2, SERIAL_BAUD,
        ADC_BITS, WATCHDOG_TIMEOUT_S…). READ-ONLY — §8 frozen.
  gotcha: none needed from it for new code — the loop uses only PIN_VALVE.

- file: test/test_fillengine/test_fillengine.cpp
  why: the Sim harness documents the loop's exact step contract (25 ms
        passes, readingDue cadence, e1/e2 = last emitted pair) — this task's
        loop() is the hardware twin of that harness's pass() function.
  gotcha: READ-only; do not modify.

- file: README.md
  why: gains the `## Serial logging` section (Mode A docs deliverable).
        Existing style: terse sections, fenced blocks, file-path references.

- file: spec/SPEC.md
  why: the full PRD (§3 logging, §4 LED contract, §7 lifecycle/faults,
        §10 invariants) — the normative source for every behavior wired here.

- url: https://arduino-pico.readthedocs.io/en/latest/stdio.html
  why: USB CDC behavior of Serial on arduino-pico — writes are never
        connection-gated/blocking (the landed logLine policy relies on it).
  critical: never add while(!Serial) — PRD §3 "always on" means logging
        must run with USB unplugged (in-service state, §2.2).
```

### Current Codebase tree (verified this session)

```bash
water-jug-filler/
├── platformio.ini                 # [env:native] 13 Unity projects green (14 once
│                                  #   P1.M6.T1.S3 lands); [env:pico] compile-only
├── firmware/jug_filler/
│   ├── jug_filler.ino             # 1589 lines: pure region 1–1291 (frozen),
│   │                              #   #ifdef ARDUINO region 1293–1589 (this task's
│   │                              #   edit surface; setup() 1503, loop() 1529)
│   └── config.h                   # §8 frozen — READ-ONLY
├── test/                          # 13 Unity projects (+ test_faultlatch landing)
├── README.md                      # 47 lines — gains a Serial logging section
└── spec/SPEC.md                   # the PRD
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino             # MODIFIED, ARDUINO region only:
│                                  #   + fill-engine routing statics + logFillEvent()
│                                  #   + loop stage 4 (engine routing) between the
│                                  #     ADC and LED-drive stages; placeholder
│                                  #     tail comment removed. setup() UNTOUCHED.
└── README.md                      # MODIFIED: + "## Serial logging" (Mode A brief)
# No new files. No test changes. No config/build changes.
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — setMode is NOT change-gated: LedPatternEngine::setMode()
//   unconditionally releases the phase anchor. Calling it every pass
//   restarts FaultBlink/DonePulse at t=0 forever — the LED reads as "stuck
//   on" and DONE/FAULT patterns never advance. The loop MUST shadow
//   (mode, faultCode) and call setMode ONLY on change (shadow init Off/0
//   matches FillEngine's Idle levels → no spurious boot setMode).

// CRITICAL — step() runs EVERY pass, not only on reading-due passes:
//   button events fire on their release pass, backstop/confirm deadlines
//   are evaluated every pass, and the false-trip guards read the freshest
//   emitted pair between emissions. readingDue only gates the reading
//   STREAM. (The work-item text's "on reading due, FillEngine.step" is
//   loose phrasing; the engine docstring is authoritative.)

// CRITICAL — valve write BEFORE the log loop: the engine guarantees
//   logs[0]==ValveClosed on every fill-ending step; the physical pin must
//   match that ordering (write LOW, then emit the lines). Writing the
//   level every pass (idempotent, sub-microsecond SIO write) satisfies
//   "immediately when it changes" with zero edge bookkeeping.

// CRITICAL — triggerAck() is also unconditional-restart: call it ONLY on
//   the ackBlink EDGE (FillEngineOutputs already shapes it as an edge —
//   true for exactly the +5-applied pass).

// GOTCHA — fill stage placement is fixed by the landed loop contract:
//   ABOVE the External LED drive stage (same-pass setMode→tick is exact —
//   the pattern engine lazily anchors at the first tick after setMode),
//   BELOW the ADC stage (consumes its readingDue + s_adcAvg1/2). Nothing
//   may ever be inserted above watchdog_update().

// GOTCHA — %lu with uint32_t payloads is the landed house precedent
//   (logLine("BTN %s %lums", ..., holdMs)); arm-none-eabi's uint32_t and
//   unsigned long share representation. Follow it for ms payloads — do not
//   "fix" half the file to casts and leave the rest inconsistent.

// GOTCHA — the fill stage adds ≤5 bounded logLines on the worst pass
//   (FILL_LOG_CAPACITY=5; e.g. DONE emits ValveClosed+ConfirmPass+Done).
//   logLine is non-stalling by design (P1.M1.T2.S2 policy) — the 8 s
//   watchdog budget is never in play (passes stay sub-millisecond).

// GOTCHA — FillLogEvent.sensor is 0-based (0 == sensor 1). Print
//   (e.sensor + 1) so log lines agree with the §7 fault codes 1/2.

// GOTCHA — never `pio test -e pico` (no on-device transport); `pio run
//   -e pico` is the compile gate. Bench flash is P1.M7.T2.S1's deliverable.

// GOTCHA — PlatformIO compiles the .ino as plain C++ (no auto-prototype
//   pass): logFillEvent() must be DEFINED before loop() uses it (the
//   Blueprint places it with the statics, well above setup()).
```

## Implementation Blueprint

### Data models and structure

No new models — this task routes existing ones: `FillEngine`/`FillEngineOutputs`/`FillLogEvent` (P1.M6.T1.S2/S3) and `LedPatternEngine` (P1.M5.T1.S1). Pure C++/Arduino; no ORM/Pydantic.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY prerequisites (do not skip)
  - RUN: pio test -e native
    → ALL projects [PASSED]; hard gate = test_fillengine green (P1.M6.T1.S2
    landed). test_faultlatch presence depends on parallel P1.M6.T1.S3 —
    either count is acceptable; a RED suite is not: STOP and report.
  - RUN: grep -n "P1.M6.T2.S1 fills the loop below this line" firmware/jug_filler/jug_filler.ino
    → the placeholder exists (this task replaces it). If absent, the loop
    already changed — re-read loop() and adapt, never blind-paste.
  - RUN: grep -n "void setup()\|void loop()" firmware/jug_filler/jug_filler.ino
    → note current line numbers; confirm setup() still ends with
    watchdog_enable(...) LAST and begins with digitalWrite(PIN_VALVE, LOW).

Task 1: ADD the fill-engine routing block (ARDUINO region)
  - EDIT firmware/jug_filler/jug_filler.ino: insert the COMPLETE block from
    "Implementation Patterns & Key Details" §A — placement: immediately
    AFTER the "end ADC sampling service state" banner, BEFORE the
    "External LED driver state" banner (keeps the file's service-block
    ordering: producer statics above, consumers below).
  - CONTENT: static FillEngine s_fill; setMode-shadow statics; logFillEvent().
  - NAMING: s_ prefix for statics (house style: s_button, s_adc, s_led);
    logFillEvent mirrors formatBootBanner/wsMirrorUpdate naming.

Task 2: EDIT loop() — reading-due capture + the routing stage
  - MODIFY the ADC stage's emission if (exact current text in §B) to set
    readingDueThisPass = true inside; declare the bool just above it.
  - INSERT the fill-routing stage (§C) between the "end ADC sample /
    aggregate / commissioning log" banner and the "External LED drive"
    stage comment. DELETE the tail placeholder comment (the one naming
    this task) — the stage it described now exists.
  - PRESERVE: watchdog_update() first statement; single nowMs snapshot;
    LED drive stage last; every other stage byte-identical.

Task 3: EDIT README.md — add "## Serial logging" section (§D, Mode A)
  - PLACEMENT: after "## Run native tests", before "## Safety note".

Task 4: VALIDATE (Validation Loop below) — all four levels.
```

### Implementation Patterns & Key Details

**§A — New static block + log-event helper** (insert between the ADC service state block and the External LED driver state block):

```cpp
// ---- Fill engine routing state (P1.M6.T2.S1) ----
// Hardware side of the fill lifecycle (PRD §7): one FillEngine::step() per
// loop pass, fed by the stages above it (BOOT event from the BOOTSEL stage,
// reading-due flag + latest emitted pair from the ADC stage, the loop's
// single nowMs snapshot) and driving the outputs below it: the valve pin,
// the LED pattern engine (and the WS2812 mirror via the shared s_led tick),
// and one logLine per engine log event (PRD §3). The engine itself is pure
// and native-tested (P1.M6.T1.S1–S3); this block only routes. Static init
// constructs the engine in Idle with the valve closed — "construct services
// in Idle" — before setup() runs (PRD §3 RAM-only state).
static FillEngine s_fill;
// setMode shadow: LedPatternEngine::setMode unconditionally RELEASES the
// pattern phase anchor — calling it every pass would restart FaultBlink and
// DonePulse at t=0 forever (LED reads "stuck on"). Call ONLY on a (mode,
// code) CHANGE. Shadow init matches FillEngine's Idle levels (Off, 0), so
// no spurious setMode fires on the first pass.
static LedPatternEngine::Mode s_prevLedMode   = LedPatternEngine::Mode::Off;
static uint8_t                s_prevFaultCode = 0;

// ONE logLine per engine event (PRD §3: state transitions + backstop
// computations; raw sensor counts are the 1 Hz S1=/S2= commissioning line
// above). Formats are NORMATIVE — documented in README.md "Serial logging".
// Sensor payloads are 0-based in the engine (0 == sensor 1); printed 1-based
// to match the §7 fault codes 1/2. %lu with uint32_t payloads is the landed
// house precedent (cf. the BTN line) — exact on arm-none-eabi.
static void logFillEvent(const FillLogEvent& e) {
    switch (e.kind) {
        case FillLogEvent::Kind::ValveClosed:
            logLine("VALVE close"); break;
        case FillLogEvent::Kind::FillStart:   // a: table min; b: recovery ms (0 = fresh boot)
            logLine("FILL start backstop=%umin recovery=%lums", e.a, e.b); break;
        case FillLogEvent::Kind::BaselineOk:  // a/b: dry baselines, counts
            logLine("BASE ok s1=%u s2=%u", e.a, e.b); break;
        case FillLogEvent::Kind::BaselineFault:
            logLine("FAULT baseline sensor=%u code=%u", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::ValveOpen:   // a: budget minutes at open (incl. stacked +5s)
            logLine("VALVE open budget=%umin", e.a); break;
        case FillLogEvent::Kind::Trip:        // a: valve-open ms at first trip
            logLine("TRIP sensor=%u fillms=%lu", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::ConfirmPass: // a: confirm duration ms
            logLine("CONFIRM pass sensor=%u durms=%lu", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::Done:        // a: press->close ms; b: valve-open ms
            logLine("DONE attemptms=%lu openms=%lu", e.a, e.b); break;
        case FillLogEvent::Kind::Abort:       // a: attempt duration ms
            logLine("ABORT attemptms=%lu", e.a); break;
        case FillLogEvent::Kind::EarlyWetFault:
            logLine("FAULT earlywet sensor=%u code=%u", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::EarlyTripFault:
            logLine("FAULT earlytrip sensor=%u code=%u", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::BackstopExpired:
            logLine("FAULT backstop code=%u", e.a); break;
        case FillLogEvent::Kind::ConfirmTimeout:
            logLine("FAULT confirm sensor=%u code=%u", (unsigned)(e.sensor + 1), e.a); break;
        case FillLogEvent::Kind::Plus5:       // a: new total budget minutes
            logLine("PLUS5 budget=%umin", e.a); break;
        case FillLogEvent::Kind::Ack:
            logLine("ACK done"); break;
    }
}
// ---- end Fill engine routing state (used by loop; feeding P1.M7.T2.S1) ----
```

**§B — ADC stage edit** (current text → new text; only the two marked lines change):

```cpp
  // CURRENT:
  if (s_adc.readingDue(nowMs)) {
    s_adc.takeReading(nowMs, s_adcAvg1, s_adcAvg2);  // emits + re-anchors cadence
  }

  // NEW (adds the handoff flag the fill engine consumes this same pass):
  bool readingDueThisPass = false;  // fresh averaged pair emitted THIS pass (engine stream gate)
  if (s_adc.readingDue(nowMs)) {
    s_adc.takeReading(nowMs, s_adcAvg1, s_adcAvg2);  // emits + re-anchors cadence
    readingDueThisPass = true;
  }
```

**§C — The fill-routing stage** (insert between the `---- end ADC sample / aggregate / commissioning log ----` banner and the `---- External LED drive (P1.M5.T2.S1) ----` comment; then delete the old tail placeholder comment that named this task):

```cpp
  // ---- Fill lifecycle routing (P1.M6.T2.S1) — the loop's fourth stage ----
  // ONE step per pass, EVERY pass (not only on reading-due): button events
  // fire on their release pass, the backstop and confirm deadlines are
  // evaluated every pass, and the false-trip guards read the freshest
  // emitted pair between emissions — readingDue gates only the reading
  // stream (FillEngine::step contract).
  const FillEngineOutputs fill =
      s_fill.step(nowMs, ev, readingDueThisPass, s_adcAvg1, s_adcAvg2);
  // Valve FIRST — the physical mirror of the engine's logs[0]==ValveClosed
  // ordering law (§7.9): the pin is LOW before the closing log line exists.
  // Level write every pass: idempotent SIO register write, sub-microsecond,
  // so the pin can never drift from the commanded level and every change
  // (open AND close) lands within one sub-millisecond pass (§10).
  digitalWrite(PIN_VALVE, fill.valveOpen ? HIGH : LOW);
  for (uint8_t i = 0; i < fill.logCount; ++i) logFillEvent(fill.logs[i]);
  if (fill.ackBlink) s_led.triggerAck();  // EDGE: exactly the +5-applied pass (PRD §4 transient)
  // CHANGE-gated setMode (see the routing-state block): setMode releases
  // the pattern phase anchor, so calling it every pass would freeze every
  // blink pattern at t=0. faultCode rides as the FaultBlink group count.
  if (fill.led != s_prevLedMode || fill.faultCode != s_prevFaultCode) {
    s_led.setMode(fill.led, fill.faultCode);
    s_prevLedMode   = fill.led;
    s_prevFaultCode = fill.faultCode;
  }
  // ---- end Fill lifecycle routing ----
```

**§D — README.md addition** (insert between "## Run native tests" and "## Safety note"):

````markdown
## Serial logging

USB CDC serial at 115200, always on (PRD §3) — logging runs with USB
unplugged; lines are dropped, never stalled. Newline-terminated lines:

| Line | Meaning |
| ---- | ------- |
| `JugFiller boot (reset cause: WATCHDOG)` / `(… POWER_OR_RESET)` | boot banner; WATCHDOG = the §10 reset path fired |
| `BTN short 1234ms` / `BTN long 1500ms` | BOOT press, classified on release (hold = raw edge-to-edge, diagnostic) |
| `S1=3500 S2=3498` | 1 Hz commissioning line — latest averaged pair, 12-bit counts (§9.3) |
| `FILL start backstop=30min recovery=0ms` | fill authorized: backstop table minutes + recovery since last fill end (0 = fresh boot) |
| `BASE ok s1=3502 s2=3499` | dry baselines accepted (counts) |
| `VALVE open budget=30min` | valve opened; deadline budget incl. any stacked +5s |
| `TRIP sensor=1 fillms=75000` | first sensor trip; valve-open time |
| `CONFIRM pass sensor=2 durms=3000` | second sensor confirmed within the window |
| `DONE attemptms=81000 openms=75000` | fill complete (press→close / valve-open→close) |
| `ACK done` | DONE acknowledged → IDLE |
| `PLUS5 budget=35min` | +5 min long-press extension (stacks) |
| `VALVE close` | valve closed — any fill end; always precedes the reason line |
| `ABORT attemptms=5000` | fill aborted by short press |
| `FAULT baseline sensor=1 code=1` | baseline out of range (§7.1) |
| `FAULT earlywet sensor=2 code=2` | wet reading inside the 5 s early window (§7.6) |
| `FAULT earlytrip sensor=1 code=1` | trip before 60 s elapsed (§7.6) |
| `FAULT confirm sensor=2 code=2` | other sensor never tripped in the 12 s window (§7.5) |
| `FAULT backstop code=3` | backstop expired, no trip (§7.7) |
| `RGB mirror init FAILED (no PIO SM)` | onboard mirror disabled; external LED unaffected |

Fault codes = LED blink groups (§7): 1 sensor 1, 2 sensor 2, 3 timer. Faults
latch until RESET. Full commissioning log workflow: `docs/COMMISSIONING.md`
(planned).
````

### Integration Points

```yaml
SETUP(): NONE — verified complete from prior tasks (valve-LOW first line,
  pinModes, analogReadResolution(ADC_BITS), Serial.begin(SERIAL_BAUD) +
  boot banner incl. watchdog_caused_reboot(), wsMirrorInit(),
  watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true) LAST).
  The FillEngine static is constructed pre-setup() by static init — Idle,
  valve closed, no auto-start (§10/§5.3).

LOOP ORDER (normative after this task): watchdog_update() → nowMs snapshot
  → BOOTSEL poll (ev) → ADC sample/emit (readingDueThisPass, s_adcAvg1/2)
  → 1 Hz commissioning log → FILL ROUTING (step → valve → logs → ack →
  setMode-on-change) → LED drive (s_led.tick → PIN_LED → wsMirrorUpdate).
  Nothing above watchdog_update(); LED drive stays last.

FUTURE (P1.M7.T1.S1 — invariant suite): greps THIS wiring as ground truth
  (valve write before logs; setMode change-gated; step every pass).
FUTURE (P1.M7.T2.S1 — bench): flashes this build; reads these exact log
  formats; verifies flows 1/2 + fault blink codes.
FUTURE (P2.M1.T1.S1/T3.S1): audits §9 log coverage against these lines;
  full README/docs sweep.

CONFIG: none — config.h frozen (§8). Consumed: PIN_VALVE only (new code).
BUILD: none — platformio.ini unchanged; Arduino IDE build unaffected
  (no new includes; logFillEvent defined before use — no auto-prototypes).
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico     # THE gate for the ARDUINO region → ========================= [SUCCESS]
pio test -e native  # regression guard for the pure region → all [PASSED]
# NO ruff/mypy (C++ project). NO `pio test -e pico`.
# If pico build fails: read the compiler output — usual suspects are a
# missing case in logFillEvent's switch (warning, not error) or a name typo
# (PlatformIO compiles the .ino as plain C++ — no auto-prototypes).
```

### Level 2: Structure & Wiring Validation (grep gates)

```bash
# Zero blocking — PRD §10 (both must return NOTHING):
grep -n "delay(" firmware/jug_filler/jug_filler.ino
grep -rn "while (!Serial\|while(!Serial" firmware/jug_filler/

# Single call sites, correct ordering:
grep -c "s_fill.step(" firmware/jug_filler/jug_filler.ino            # -> 1
grep -c "s_led.setMode(" firmware/jug_filler/jug_filler.ino          # -> 1 (the change-gated site)
grep -c "logFillEvent" firmware/jug_filler/jug_filler.ino            # -> 2 (definition + loop call)
grep -n "digitalWrite(PIN_VALVE" firmware/jug_filler/jug_filler.ino  # -> 2 (setup line 1, loop fill stage)

# Ordering proof (line numbers ascending): watchdog_update() < s_fill.step(
# < digitalWrite(PIN_VALVE < logFillEvent( < s_led.tick( :
sed -n '/^void loop()/,$p' firmware/jug_filler/jug_filler.ino | grep -n -m1 "watchdog_update"
sed -n '/^void loop()/,$p' firmware/jug_filler/jug_filler.ino | grep -n -m1 "s_fill.step"
sed -n '/^void loop()/,$p' firmware/jug_filler/jug_filler.ino | grep -n -m1 "digitalWrite(PIN_VALVE"
sed -n '/^void loop()/,$p' firmware/jug_filler/jug_filler.ino | grep -n -m1 "logFillEvent"
sed -n '/^void loop()/,$p' firmware/jug_filler/jug_filler.ino | grep -n -m1 "s_led.tick"
# Each -m1 hit's line number must be strictly greater than the previous.

# Placeholder removed; setup() untouched:
grep -c "P1.M6.T2.S1 fills the loop below this line" firmware/jug_filler/jug_filler.ino  # -> 0
git diff firmware/jug_filler/jug_filler.ino | grep -c "^-.*digitalWrite(PIN_VALVE, LOW)" # -> 0

# Exhaustive switch: 15 kinds, 15 cases, no default:
grep -c "case FillLogEvent::Kind::" firmware/jug_filler/jug_filler.ino  # -> 15

git status --short
#   Expected: " M firmware/jug_filler/jug_filler.ino" + " M README.md"
#   (+ prior tasks' uncommitted work, if any — leave it alone)
git diff firmware/jug_filler/config.h platformio.ini   # -> EMPTY (untouched)
git diff firmware/jug_filler/jug_filler.ino | grep "^-" | grep -v "P1.M6.T2.S1 fills\|INSERT ABOVE\|LED drive in P1.M5\|Nothing may ever be inserted\|event\n\|logs via logLine"
#   -> only the removed placeholder-comment lines (ADC if-block line changes show as -/+ pair)
```

### Level 3: Contract Validation (compile + review)

```bash
pio run -e pico && pio run -e pico   # deterministic double compile
# Manual review checklist (read the final loop() top to bottom):
#   1. watchdog_update() is the first statement.
#   2. setup() byte-identical (git diff shows no setup() hunks).
#   3. The fill stage sits between the ADC banner and the LED-drive comment.
#   4. digitalWrite(PIN_VALVE, …) precedes the logFillEvent loop.
#   5. setMode is inside the (fill.led != s_prevLedMode || fill.faultCode !=
#      s_prevFaultCode) guard; triggerAck inside if (fill.ackBlink).
#   6. Pure region (lines 1 to the #ifdef ARDUINO marker) has ZERO diff hunks.
```

### Level 4: On-Device Bench Validation — P1.M7.T2.S1's deliverable, NOT this task's

No upload target is configured (compile-only by design). The first bench session (P1.M7.T2.S1) will flash this build, watch the log lines scroll, and dry-run flows 1/2 + fault paths per PRD §9. Success Criteria end at Levels 1–3.

## Final Validation Checklist

### Technical Validation

- [ ] `pio run -e pico` → `[SUCCESS]` (twice, deterministic)
- [ ] `pio test -e native` → exit 0, all projects `[PASSED]` (pure region untouched)
- [ ] Level-2 greps all return expected counts/orderings; zero `delay(` / `while (!Serial`
- [ ] `git diff` confined to: ARDUINO region of `jug_filler.ino` + `README.md`; config.h/platformio.ini/setup()/pure region byte-identical

### Feature Validation

- [ ] One `s_fill.step(nowMs, ev, readingDueThisPass, s_adcAvg1, s_adcAvg2)` per pass, every pass
- [ ] Valve pin: written first on every fill-ending pass (before its log lines), level = `fill.valveOpen`; `setup()` line 1 still `digitalWrite(PIN_VALVE, LOW);`
- [ ] `setMode(fill.led, fill.faultCode)` fires only on (mode, code) change — blink patterns advance; `triggerAck()` only on the ackBlink edge
- [ ] All 15 event kinds render exactly one logLine each; sensor numbers 1-based; formats match README table (incl. `FILL start backstop=… recovery=…`, `S1=/S2=` 1 Hz, `BTN`, boot banner)
- [ ] PRD §3 logging coverage complete: state transitions (event stream) + raw counts (1 Hz line) + backstop computations (FillStart/ValveOpen/Plus5)
- [ ] Zero `delay()` in the fill loop — verified by grep + review (§10)

### Code Quality Validation

- [ ] House conventions: bannered blocks with task IDs, `s_` statics, comment style matches sibling stages, `%lu` ms precedent followed
- [ ] `logFillEvent` defined before `loop()` (no auto-prototypes); no new includes; no new tunables (PRD §8 frozen)
- [ ] No behavior duplicated from the engine (routing only — classification stays in the pure, tested layer)

### Documentation & Deployment

- [ ] README `## Serial logging` section complete (every line format) with the Mode-A pointer to the future full sweep
- [ ] No new env vars, dependencies, or config entries

---

## Anti-Patterns to Avoid

- ❌ Don't call `s_led.setMode()` every pass — it releases the phase anchor; FaultBlink/DonePulse freeze at t=0 and the LED looks stuck on. Change-gate it.
- ❌ Don't call `FillEngine::step()` only on reading-due passes — button events, backstop expiry, and confirm timeout must fire on their own pass; the engine docstring is the contract.
- ❌ Don't emit log lines before the valve write on a closing step — the pin must mirror `logs[0]==ValveClosed` (§7.9).
- ❌ Don't touch `setup()`, the pure region, config.h, platformio.ini, or sibling test suites — this task's entire edit surface is the ARDUINO region + README.
- ❌ Don't add edge-tracking for the valve pin — the level write is idempotent and cheaper than the bookkeeping.
- ❌ Don't add a `default:` to `logFillEvent`'s switch — an exhaustive 15-case switch makes a future Kind a compile-visible gap, not a silent drop.
- ❌ Don't add `while(!Serial)`, `delay()`, or any second `millis()` read — one snapshot per pass (seam rule), never gate on USB connection (§3/§2.2 in-service state).
- ❌ Don't print `e.sensor` raw (0-based) — logs must agree with the §7 fault codes; print `e.sensor + 1`.
- ❌ Don't `pio test -e pico`; don't reformat or "improve" neighboring stages while in there.
- ❌ Don't invent new log formats at implementation time — the §A table is normative (P2.M1.T1.S1 and the bench runbook consume it verbatim).

---

**Confidence Score: 9/10** — every consumed API was read from the live source this session (`step` signature and docstring, `FillEngineOutputs` levels/edges, all 15 `FillLogEvent` kinds with payload semantics, `setMode`'s unconditional anchor release, the loop's existing stages and their landed insertion contract, `logLine` policy, `setup()` completeness); the complete code for every edit is provided verbatim; validation commands are the repo's proven invocations. Residual risk: (a) P1.M6.T1.S3 lands concurrently — mitigated by Task 0's gate (hard requirement: `test_fillengine` green; placeholder-comment grep re-syncs line numbers) and by S3's expected test-only diff; (b) the `%lu`/uint32_t varargs pairing is pedantically implementation-defined off-ARM — it is the landed house precedent and exact on the only target this region compiles for.
