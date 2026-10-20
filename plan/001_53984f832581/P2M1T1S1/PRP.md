# PRP — P2.M1.T1.S1: Audit and complete log coverage for section 9 workflow

## Goal

**Feature Goal**: Close every logging gap so the PRD §9 commissioning workflow — specifically §9.3 (sensor calibration: dry/wet counts → `BASELINE_MIN/MAX`, `TRIP_DROP_COUNTS`) and §9.5 (attended first fill: fill durations vs recovery times → `BACKSTOP_TABLE_MIN`) — can be executed from the captured serial log stream alone, with no extra tooling. Pure audit-and-enrich task: **no behavior changes beyond log content**.

**Deliverable**: (1) enriched `FillLogEvent` payloads + a new pure, native-tested formatter `formatFillLogEvent()` in `firmware/jug_filler/jug_filler.ino`; (2) new Unity suite `test/test_logfmt/`; (3) README.md "Serial logging" table updated to the final normative line formats (Mode A documentation).

**Success Definition**: every §9 step's data need maps to a logged line (audit table below fully "covered"); `pio test -e native && python3 test/check_setup_first_line.py` green; `pio run -e pico` compiles; no decision logic, timing, pin, or state-machine change anywhere; existing bench-evidence greps (P1.M7.T2.S1) still match because all format changes are append-only.

## User Persona

**Target User**: The commissioning operator (human or agent) executing PRD §9 on the bench with a serial monitor; the downstream doc author (P2.M1.T2.S1, `docs/COMMISSIONING.md`) who references the final formats.

**Use Case**: During §9.3/§9.5 the operator watches the log, records numbers, and edits `config.h` constants. Every number needed must already be on a line — no side calculations, no re-runs.

**User Journey**: flash → watch `S1=/S2=` settle dry → start fill → read `BASE ok`/`FAULT baseline count=` → dip sensors, read wet values from `S1=/S2=` → run attended fill → read `FILL start … recmin=`, `DONE openms=` / `FAULT backstop budget= openms=` → adjust config.h constants → repeat.

**Pain Points Addressed**: baseline fault with no observed count (operator can't tell how far out of range); backstop expiry with no duration/budget (can't retune `BACKSTOP_TABLE_MIN`); false-trip faults with no time context (can't distinguish guard windows); table input minutes hidden (truncation at :59.999 invisible).

## Why

- PRD §3 mandates logging of state transitions, raw sensor counts, and backstop computations; §9.3/§9.5 define calibration as *reading logged values* and setting constants. The log stream IS the calibration instrument.
- Most lines already rode in with their features (P1.M3.T1.S2 raw-count 1 Hz line, P1.M6.T2.S1 `logFillEvent` formats). This subtask is the planned closing audit — it finds and fills the remaining gaps without touching behavior.
- P1.M7.T2.S1 (completing in parallel, contract read) captures bench evidence with the current formats and files `DISCREPANCIES.md`; append-only enrichment keeps that evidence grep-compatible.

## What

Walk each §9 step against the actual log stream; add or adjust missing lines with sensor id + counts. Final normative formats (changes **bold**):

| Line (final format) | Change vs today |
| --- | --- |
| `JugFiller boot (reset cause: …)` | unchanged |
| `BTN short\|long <ms>ms` | unchanged |
| `S1=%u S2=%u` (1 Hz, all states) | unchanged — dry AND wet values for §9.3 |
| `FILL start backstop=%umin recovery=%lums` **`recmin=%u`** | append table-input minutes (computed `recovery/60000`; 0 on fresh boot) |
| `BASE ok s1=%u s2=%u` | unchanged |
| `FAULT baseline sensor=%u code=%u` **`count=%lu`** | append failing sensor's observed baseline |
| `VALVE open budget=%umin` | unchanged — the "deadline" budget datum |
| `TRIP sensor=%u fillms=%lu` | unchanged |
| `CONFIRM pass sensor=%u durms=%lu` | unchanged |
| `DONE attemptms=%lu openms=%lu` | unchanged — §9.5 duration datum |
| `ABORT attemptms=%lu` **`openms=%lu`** | append valve-open ms (0 ⇒ never opened) |
| `FAULT earlywet sensor=%u code=%u` **`fillms=%lu`** | append fill time at fault |
| `FAULT earlytrip sensor=%u code=%u` **`fillms=%lu`** | append fill time at fault |
| `FAULT backstop code=3` **`budget=%umin openms=%lums`** | code literal (always 3); append budget + open duration |
| `FAULT confirm sensor=%u code=%u` **`durms=%lu`** | append confirm-window elapsed |
| `PLUS5 budget=%umin` | unchanged — +5 stacks visible |
| `ACK done` / `VALVE close` / `RGB mirror init FAILED (no PIO SM)` | unchanged |

Structural change: `logFillEvent`'s format switch moves into a **pure** formatter `formatFillLogEvent(char* buf, size_t bufLen, const FillLogEvent& e)` in the UNGUARDED region (exact precedent: `formatBootBanner` + `test_log_utils`), so every final format is native-testable; the ARDUINO-region `logFillEvent` becomes format + `logLine("%s", buf)`.

### §9 audit table (the contract this task closes)

| §9 step | Data needed | Line(s) | Status after this task |
| --- | --- | --- | --- |
| 9.1 dry bench | boot cause, IDLE liveness, BOOT reads, dry-fire clicks | banner, `S1=/S2=`, `BTN …`, `VALVE open/close` | already covered — no change |
| 9.2 power rails | rail volts | — | **out of scope by design**: USB unplugged ⇒ CDC absent; covered by P1.M7.T2.S1's multimeter record. Document in README note |
| 9.3 dry baselines | per-sensor dry counts, verdict | `S1=/S2=`, `BASE ok s1= s2=` | covered; fault side now carries `count=` |
| 9.3 wet values | per-sensor wet counts | `S1=/S2=` while dipped (line runs in all states) | covered — no change |
| 9.4 dry-run flows | flow 1/2 traces | full existing sequence | covered (benched by P1.M7.T2.S1) |
| 9.5 durations | fill duration, end cause | `DONE openms`, `TRIP fillms`, `ABORT openms`, `FAULT … openms/durms` | covered by this task's additions |
| 9.5 recovery | recovery before fill, table input & result | `FILL start recovery=ms recmin= backstop=` | covered by this task's additions |

### Success Criteria

- [ ] §9 audit table rows all "covered" in the shipped firmware
- [ ] `FAULT baseline` prints the failing sensor's observed count
- [ ] `FAULT backstop` prints budget minutes + openms; other fault lines print their duration context
- [ ] `FILL start` prints `recmin=` (table input, truncated minutes)
- [ ] `formatFillLogEvent` is pure (UNGUARDED region, snprintf only) and native-tested for every Kind, including sensor 1/2 mapping and truncation safety
- [ ] README "Serial logging" table matches the code exactly (Mode A); notes `recovery=0ms` ⇒ fresh boot (table bypassed) and §9.2 is bench-multimeter territory
- [ ] Zero diffs outside `jug_filler.ino`, `test/test_logfmt/`, `README.md`

## All Needed Context

### Context Completeness Check

✅ Written from a full read of the live `.ino` (1673 lines): `logLine` policy, `FillLogEvent`/`FillEngine` internals, `endFillFault`, step() block structure. Payload assertions grepped across all test suites — none pin `.a`/`.b` on the fault Kinds being repurposed. Parallel-item contract (P1.M7.T2.S1) read: it lands no firmware changes and its greps are prefix-anchored, so append-only edits stay compatible.

### Documentation & References

```yaml
- file: firmware/jug_filler/jug_filler.ino
  why: THE artifact. UNGUARDED region = pure logic only; #ifdef ARDUINO region =
        hardware glue only (seam contract, file header).
  pattern: formatBootBanner (pure, snprintf-only) + setup()'s logLine("%s", banner)
        is the exact shape to replicate for formatFillLogEvent + logFillEvent.
  gotcha: every function needs an explicit prototype above its definition
        (host g++ runs no .ino prototype generator). logLine is ARDUINO-only —
        the pure formatter must NOT call it. LOGLINE_MAX = 128 bounds lines;
        longest new line ≈ 60 chars.

- file: firmware/jug_filler/jug_filler.ino  (FillLogEvent + endFillFault + step)
  why: payload wiring sites. FILL_LOG_CAPACITY=5 (worst step=5) — payload changes
        add NO new events, capacity stays safe.
  pattern: endFillFault(nowMs, why, sensor) is the single fault exit with access
        to baseline_, deadline_, valveOpenedAtMs_, confirmStartMs_.
  gotcha: BaselineFault path runs only after baseline_.done() ⇒ baseline_(sensor)
        valid. Guard faults fire only in the valve-open sub-phase ⇒
        elapsedMs(nowMs, valveOpenedAtMs_) valid. Abort is logged in step()
        block 3 — capture `wasOpen = valveOpen_` BEFORE `valveOpen_ = false`.

- file: test/test_log_utils/test_log_utils.cpp
  why: the pattern for the new suite: #include "../../firmware/jug_filler/jug_filler.ino",
        Unity, TEST_ASSERT_EQUAL_STRING exact formats, truncation-safety test,
        zero-length buffer test.

- file: test/test_fillengine/test_fillengine.cpp
  why: existing payload assertions to preserve: FillStart a=30 b=0 (fresh),
        ValveOpen a=30, Trip a=60000, restart a=44, Plus5 a=35.
  gotcha: NO existing test asserts a/b on BaselineFault/EarlyWet/EarlyTrip/
        BackstopExpired/ConfirmTimeout — repurposing those payloads breaks
        nothing, but ADD assertions for the new payloads there or in test_logfmt.

- file: README.md  (section "Serial logging")
  why: the normative log table this task finalizes (Mode A). Keep the existing
        rows' wording, extend formats append-only, document recovery=0ms ⇒
        fresh boot and the §9.2 note.

- file: plan/001_53984f832581/P1M7T2S1/PRP.md
  why: parallel-item CONTRACT — bench evidence + DISCREPANCIES.md produced with
        current formats; its greps are prefix-anchored (e.g. 'FILL start
        backstop=30min recovery=0ms'), so appended fields keep them matching.
        NEVER rename/reorder existing fields.

- file: plan/001_53984f832581/P2M1T1S1/research/notes.md
  why: full audit trail: current log surface table, §9 step-by-step verdicts,
        code facts (payload sites, test-assertion inventory), external-research
        rationale (none needed — internal formats; vsnprintf/%lu semantics
        already landed and documented in P1.M1.T2.S2 code comments).

- file: firmware/jug_filler/config.h
  why: read-only reference for README notes (BASELINE_MIN/MAX, BACKSTOP_TABLE_MIN
        knots, FRESH_BOOT_BACKSTOP_MIN). NO tunables are added or changed.
```

### Current Codebase tree (relevant excerpt)

```bash
water-jug-filler/
├── firmware/jug_filler/
│   ├── jug_filler.ino        # pure region ~lines 33–1290; ARDUINO region ~1293–1673
│   └── config.h              # §8 constants (frozen for this task)
├── test/                     # 16 Unity suites + check_setup_first_line.py
│   └── test_log_utils/       # formatBootBanner pattern to follow
├── platformio.ini            # [env:native] auto-discovers new test dirs
└── README.md                 # "Serial logging" normative table
```

### Desired Codebase tree with files to be added

```bash
firmware/jug_filler/jug_filler.ino   # MODIFIED: formatFillLogEvent (pure region),
                                     #   prototypes, payload enrichments, slim logFillEvent
test/test_logfmt/test_logfmt.cpp     # NEW: exact-format tests for every FillLogEvent Kind
README.md                            # MODIFIED: final "Serial logging" table + notes
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: seam rule — formatFillEvent must live in the UNGUARDED region and
//   use snprintf ONLY (no Serial, no millis). It formats; logLine transmits.
//   Pass the buffer, never logLine from pure code (logLine is ARDUINO-only).

// CRITICAL: append-only formats — P1.M7.T2.S1 bench greps are prefix matches
//   ('FILL start backstop=30min recovery=0ms' etc.). New fields go at END of
//   line; never rename/reorder existing fields.

// CRITICAL: recmin is COMPUTED in the formatter (e.b / 60000u) — no new payload
//   field, no engine change for FillStart. Fresh boot: recovery=0ms ⇒ recmin=0
//   (and backstop came from FRESH_BOOT_BACKSTOP_MIN, not the table — README it).

// GOTCHA: FAULT backstop — code is CONSTANT 3; print it as a literal
//   ("FAULT backstop code=3 budget=%umin openms=%lums") and repurpose
//   a=budgetMinutes, b=openms. Other fault Kinds keep a=faultCode.

// GOTCHA: %lu with uint32_t payloads is the landed house precedent, exact on
//   arm-none-eabi AND host g++ (both LP64/LLP64 treat unsigned long >= 32-bit;
//   payloads are uint32_t — keep casts explicit like logFillEvent does today).

// GOTCHA: FILL_LOG_CAPACITY=5 — you are NOT adding log events, only enriching
//   payloads of existing ones. If you ever need a 6th event on a step, STOP:
//   that's a behavior change (out of scope).

// GOTCHA: explicit prototypes above definitions (host g++). Add
//   size_t formatFillLogEvent(char* buf, size_t bufLen, const FillLogEvent& e);
//   next to the formatBootBanner prototype.

// GOTCHA: 1 Hz S1=/S2= line already runs in ALL states (loop is unconditional)
//   — wet-value logging for §9.3 needs NO new line; do not duplicate it.
```

## Implementation Blueprint

### Data models and structure

No new types. `FillLogEvent` payload semantics change per Kind (keep struct shape):

```cpp
// Final payload contract (document in the FillLogEvent comment block):
//   FillStart:       a = table minutes (fresh-boot default on fresh boot),
//                    b = recovery ms at press; formatter derives recmin = b/60000
//   BaselineFault:   a = fault code, b = FAILING sensor's observed baseline count
//   EarlyWet/EarlyTripFault: a = fault code, b = fill ms at fault (valve-open span)
//   BackstopExpired: a = budget minutes (incl. +5 stacks), b = open ms at expiry
//   ConfirmTimeout:  a = fault code, b = confirm-window elapsed ms
//   Abort:           a = attempt ms, b = open ms (0 ⇒ valve never opened)
//   All other Kinds: unchanged (BaselineOk a/b counts, ValveOpen a budget minutes,
//                    Trip a fillms, ConfirmPass a durms, Done a/b, Plus5 a budget)
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: MODIFY firmware/jug_filler/jug_filler.ino — pure formatter
  - ADD prototype next to formatBootBanner's:
      size_t formatFillLogEvent(char* buf, size_t bufLen, const FillLogEvent& e);
  - ADD in UNGUARDED region (after FillEngine, which defines FillLogEvent;
      BEFORE the #ifdef ARDUINO): formatFillLogEvent implementing the FINAL
      formats table above via one snprintf; return (size_t)snprintf result.
      Sensor payloads print 1-based (e.sensor + 1) — fault-code alignment.
  - KEEP: ValveClosed -> "VALVE close", Ack -> "ACK done", BaselineOk, ValveOpen,
      Trip, ConfirmPass, Done, Plus5, BTN/banner formats untouched.

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — engine payload enrichment
  - END FillFault (private): switch on `why` to fill a/b:
      BaselineFault   -> b = baseline_.baseline(sensor)          (done() is true here)
      EarlyWet/Early  -> b = elapsedMs(nowMs, valveOpenedAtMs_)   (valve-open sub-phase only)
      TripFault
      BackstopExpired -> a = deadline_.budgetMinutes(), b = elapsedMs(nowMs, valveOpenedAtMs_)
      ConfirmTimeout  -> b = elapsedMs(nowMs, confirmStartMs_)
    (keep a = faultCode for the sensor Kinds; keep faultCode_ latching untouched)
  - STEP() BLOCK 3 (abort): capture wasOpen = valveOpen_ BEFORE valveOpen_=false;
    log(Abort, 0, elapsedMs(nowMs, fillStartMs_),
        wasOpen ? elapsedMs(nowMs, valveOpenedAtMs_) : 0)
  - NO other engine edits — decision logic, ordering (logs[0]==ValveClosed),
    FILL_LOG_CAPACITY, state machine: all frozen.

Task 3: MODIFY firmware/jug_filler/jug_filler.ino — slim logFillEvent
  - REPLACE the format switch body with:
      char buf[96];  // longest final line ~60 chars; snprintf truncates safely
      formatFillLogEvent(buf, sizeof(buf), e);
      logLine("%s", buf);
  - KEEP logFillEvent in the ARDUINO region (it calls logLine).

Task 4: CREATE test/test_logfmt/test_logfmt.cpp
  - FOLLOW pattern: test/test_log_utils/test_log_utils.cpp (include the .ino,
      Unity, setUp/tearDown empty, exact-string asserts).
  - COVER every Kind: one exact TEST_ASSERT_EQUAL_STRING per final format,
      including: FillStart with recmin derivation (b=599999 -> recmin=9;
      b=0 -> recmin=0), BaselineFault count, BackstopExpired budget/openms with
      literal code=3, EarlyWet fillms, ConfirmTimeout durms, Abort openms=0 and
      nonzero, sensor mapping (sensor=0 prints 1, sensor=1 prints 2),
      truncation-safety (tiny buffer, NUL-terminated, full return length),
      zero-length buffer writes nothing.
  - RUN: pio test -e native   (new dir auto-discovered; no platformio.ini edit)

Task 5: MODIFY test/test_fillengine/test_fillengine.cpp (and test_faultlatch if natural)
  - ADD payload assertions for the enriched Kinds at existing fault-path tests
      (e.g. BackstopExpired b >= budget ms; BaselineFault b equals the forced
      out-of-range count; Abort b == 0 on settle abort, > 0 on open abort).
  - PRESERVE all existing assertions (they all still hold — verified by grep,
      see research/notes.md).

Task 6: MODIFY README.md — "Serial logging" section
  - UPDATE the table to the FINAL formats (append-only wording); add rows/notes:
      recmin = table input minutes (truncated); recovery=0ms => fresh boot,
      table bypassed (FRESH_BOOT_BACKSTOP_MIN); fault duration fields;
      "S1=/S2= runs in every state — wet values during dips (§9.3)";
      §9.2 power rails are not loggable (USB unplugged) — see bench records.
  - KEEP the trailing pointer to docs/COMMISSIONING.md (planned, P2.M1.T2.S1).

Task 7: VALIDATE (all levels below) + confirm zero diffs elsewhere
  - RUN: git diff --stat  → only jug_filler.ino, test/test_logfmt/, test_fillengine
      (test_faultlatch optional), README.md
```

### Implementation Patterns & Key Details

```cpp
// Pure formatter — exact shape (precedent: formatBootBanner):
size_t formatFillLogEvent(char* buf, size_t bufLen, const FillLogEvent& e) {
    switch (e.kind) {
        case FillLogEvent::Kind::FillStart:  // recmin DERIVED here — no new payload
            return (size_t)snprintf(buf, bufLen, "FILL start backstop=%umin recovery=%lums recmin=%u",
                                    e.a, e.b, (unsigned)(e.b / 60000u));
        // ... one case per Kind, exact strings from the "What" table ...
    }
    return 0;  // unreachable defensive return (all Kinds covered)
}

// endFillFault enrichment — payloads only, latch/ordering untouched:
void endFillFault(uint32_t nowMs, FillLogEvent::Kind why, uint8_t sensor) {
    valveOpen_ = false;                            // §7.9 LOW FIRST — unchanged
    log(FillLogEvent::Kind::ValveClosed, 0, 0, 0); // logs[0] law — unchanged
    state_ = FillState::Fault;
    faultCode_ = (why == FillLogEvent::Kind::BackstopExpired) ? 3 : (uint8_t)(sensor + 1);
    clock_.recordFillEnd(nowMs);
    // NEW: kind-specific duration/count payloads (see Task 2) — then:
    log(why, sensor, aPayload, bPayload);
}

// GOTCHA: do NOT route fault payloads through new event Kinds or extra events —
// enrich the EXISTING event. Capacity and §7.9 ordering are frozen contracts
// (test_invariants + test_fillengine pin them).
```

### Integration Points

```yaml
FIRMWARE: jug_filler.ino only; config.h UNTOUCHED (no new tunables — the 1 Hz
  literal and buf sizes follow existing task-contract precedent comments).
BUILD: platformio.ini UNTOUCHED ([env:native] auto-discovers test/test_logfmt).
DOCS: README.md "Serial logging" = final normative table (Mode A). Downstream:
  P2.M1.T2.S1 (docs/COMMISSIONING.md) and P2.M1.T3.S1 (doc sync) consume these
  formats verbatim — land them exactly as specified here.
INPUTS CONSUMED: P1.M6.T2.S1 logLine/logFillEvent structure; P1.M1.T2.S2
  unplug-safe logging policy (logLine — never "improve" it here).
COMPATIBILITY: P1.M7.T2.S1 bench evidence greps stay valid (append-only fields).
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico                 # board compile — [SUCCESS]
pio test -e native              # all suites incl. new test_logfmt
python3 test/check_setup_first_line.py   # structural safety gate (unchanged code paths)
# Expected: all green. test_logfmt failures = format/string drift — fix the
# formatter (or the test if the string was mistyped), never logLine.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native --verbose 2>&1 | grep -E 'test_logfmt|PASS|FAIL' | tail -20
# Expected: every Kind has an exact-format test; truncation + zero-len pass;
# test_fillengine/test_faultlatch all pass (existing + new payload asserts).
```

### Level 3: Integration Testing (log-stream walkthrough, no hardware needed)

```bash
# Structural audit against the .ino: every §9 datum has a producer.
grep -n 'recmin\|count=%lu\|fillms=%lu\|durms=%lu\|openms' firmware/jug_filler/jug_filler.ino
# Expected: FILL start (recmin), FAULT baseline (count), earlywet/earlytrip
# (fillms), FAULT backstop (budget/openms), FAULT confirm (durms), DONE/ABORT
# (openms) all present in formatFillLogEvent; TRIP (fillms) unchanged.

# README table vs code: every format string in formatFillLogEvent appears in
# README's table (spot-check the 7 changed rows).
grep -c 'recmin' README.md        # >= 1
grep -c 'FAULT backstop code=3 budget' README.md   # >= 1

# Native end-to-end proxy: test_fillengine already drives full lifecycles and
# asserts event payloads — the formatter tests turn those into exact lines.
# (On-bench confirmation of the final lines belongs to the §9 commissioning
# session itself, not this task.)
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Safety-invariant re-verification (payload edits must not disturb them):
pio test -e native --verbose 2>&1 | grep -E 'test_invariants' 
# Expected: PASS — valve-first ordering, no auto-open, close-on-every-exit.

# Diff-scope gate:
git diff --stat
# Expected EXACTLY: firmware/jug_filler/jug_filler.ino, README.md,
# test/test_logfmt/test_logfmt.cpp (new), test/test_fillengine/test_fillengine.cpp
# (+ test/test_faultlatch/test_faultlatch.cpp if assertions added there).
```

## Final Validation Checklist

### Technical Validation

- [ ] `pio run -e pico` SUCCESS; `pio test -e native` all pass; `check_setup_first_line.py` exit 0
- [ ] New suite covers every FillLogEvent Kind with exact-string asserts + truncation safety
- [ ] `git diff --stat` shows only the four allowed paths

### Feature Validation

- [ ] §9 audit table: every row covered; §9.3 wet values via unconditional `S1=/S2=`
- [ ] `FAULT baseline` prints observed count; `FAULT backstop` prints budget + openms; early faults print fillms; confirm timeout prints durms; `ABORT` prints openms
- [ ] `FILL start` prints `recmin=` derived from recovery ms
- [ ] All format changes append-only (P1.M7.T2.S1 greps still match)
- [ ] README "Serial logging" table == code formats, incl. fresh-boot and §9.2 notes

### Code Quality Validation

- [ ] `formatFillLogEvent` in UNGUARDED region, snprintf-only, explicit prototype
- [ ] No behavior change: state machine, timings, valve/LED handling, FILL_LOG_CAPACITY, logs[0]==ValveClosed ordering all untouched
- [ ] No new tunables; config.h and platformio.ini untouched
- [ ] Payload comments in FillLogEvent's doc block updated to the new contract

### Documentation & Deployment

- [ ] README documents `recovery=0ms` ⇒ fresh boot (table bypassed) and §9.2 non-loggable note
- [ ] Formats ready for P2.M1.T2.S1's `docs/COMMISSIONING.md` to reference verbatim

## Anti-Patterns to Avoid

- ❌ Don't touch `logLine`'s bounded/non-blocking policy (P1.M1.T2.S2 contract) — it is the ONLY logging API
- ❌ Don't rename/reorder existing line fields — bench evidence greps break
- ❌ Don't add log EVENTS (only enrich payloads) — FILL_LOG_CAPACITY and §7.9 ordering are frozen
- ❌ Don't "fix" engine behavior while in there — this is a logging-only audit
- ❌ Don't put formatting in the ARDUINO region and skip native tests — the whole point of `formatFillLogEvent` is exact-format tests
- ❌ Don't log absolute millis deadlines (meaningless to humans); budget minutes + spans only
- ❌ Don't catch-all or invent new fault Kinds — code=3 stays a literal for backstop

---

**Confidence Score: 9/10** — the audit was performed against the live 1673-line sketch (not PRPs), every gap has a named producer site (`endFillFault`, step block 3, formatter), existing test-assertion inventory was grepped (no conflicts), and the pure-formatter extraction follows the landed `formatBootBanner` precedent. Residual risk: minor wording drift between README table and formatter strings (mitigated by exact-string Unity tests + Level 3 grep checks) and the possibility the bench discrepancy register (P1.M7.T2.S1, parallel) surfaces a format change request — append-only discipline keeps merges trivial.
