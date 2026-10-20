# P2.M1.T1.S1 research notes — commissioning log audit

## Source of truth: live code read (not PRPs)
Read in full: `firmware/jug_filler/jug_filler.ino` (1673 lines), `README.md`,
`platformio.ini`, `test/test_log_utils/test_log_utils.cpp`, payload-assertion
grep across `test/test_fillengine`, `test/test_faultlatch`, `test/test_invariants`.
Parallel contract read: `plan/001_53984f832581/P1M7T2S1/PRP.md` (bench evidence
task; completes before this item; produces NO firmware changes).

## Current log surface (exact formats, from logFillEvent + loop)

| Producer | Line | Where |
|---|---|---|
| setup() | `JugFiller boot (reset cause: POWER_OR_RESET\|WATCHDOG)` | formatBootBanner (PURE, native-tested in test_log_utils) |
| loop() BTN stage | `BTN short\|long %lums` | loop, raw-edge stamped |
| loop() ADC stage | `S1=%u S2=%u` | 1 Hz, UNCONDITIONAL (all states incl. FILLING/FAULT) |
| logFillEvent | `FILL start backstop=%umin recovery=%lums` | a=tableMinutes, b=recoveryMsAtStart |
| logFillEvent | `BASE ok s1=%u s2=%u` | a/b = baselines |
| logFillEvent | `FAULT baseline sensor=%u code=%u` | NO observed counts ← GAP |
| logFillEvent | `VALVE open budget=%umin` | a=budgetMinutes at open |
| logFillEvent | `TRIP sensor=%u fillms=%lu` | a = valve-open ms |
| logFillEvent | `CONFIRM pass sensor=%u durms=%lu` | |
| logFillEvent | `DONE attemptms=%lu openms=%lu` | §9.5 duration datum ✓ |
| logFillEvent | `ABORT attemptms=%lu` | no openms ← minor gap |
| logFillEvent | `FAULT earlywet sensor=%u code=%u` | no fillms ← GAP |
| logFillEvent | `FAULT earlytrip sensor=%u code=%u` | no fillms ← GAP |
| logFillEvent | `FAULT backstop code=%u` | no budget/openms ← GAP (§9.5 core) |
| logFillEvent | `FAULT confirm sensor=%u code=%u` | no durms ← GAP |
| logFillEvent | `PLUS5 budget=%umin` | +5 stack visible ✓ |
| logFillEvent | `ACK done` / `VALVE close` | ✓ |
| wsMirrorInit | `RGB mirror init FAILED (no PIO SM)` | ✓ |

## Section 9 audit (step → data needed → verdict)

- §9.1 dry bench: banner ✓, IDLE proven by 1 Hz S1=/S2= ✓, BTN ✓, dry-fire
  VALVE open/close ✓. NO GAP.
- §9.2 power rails: USB necessarily unplugged ⇒ NO log possible by design;
  covered by P1.M7.T2.S1's multimeter .md record. Out of scope — document.
- §9.3 calibration: dry via S1=/S2= + `BASE ok` ✓; wet via S1=/S2= while
  dipped (line runs in ALL states) ✓. GAP: baseline FAULT carries no observed
  count → operator can't see how far out of range (matters when choosing
  BASELINE_MIN/MAX after a wet-start fault).
- §9.4 dry-runs: covered (P1.M7.T2.S1 benches them). Flow-1 restart needs
  recovery at press ✓ (FILL start recovery=).
- §9.5 attended first fill: fill duration ✓ (DONE openms, TRIP fillms);
  recovery ✓ (FILL start recovery=ms); backstop grant ✓ (backstop=, budget=);
  +5 ✓. GAPS: (a) table INPUT minutes not shown (truncation at :59.999 s
  invisible), (b) FAULT backstop carries no duration/budget — the single most
  relevant line when retuning BACKSTOP_TABLE_MIN upward, (c) false-trip /
  early-wet faults carry no fill-time context, (d) CONFIRM timeout no durms.
  Verdict language from item: "recovery minutes, table result, deadline,
  +5 extensions" → recmin (computed in formatter from b/60000), backstop=/budget=,
  PLUS5 stack. "End-of-fill summary (duration, end cause)" → add openms to
  ABORT; fault lines gain duration payloads.

## Key code facts for the implementer

- `logLine` (ARDUINO region, ~line 1300): vsnprintf into 128 B, one write +
  '\n', never gated on connection. ONLY logging API. Longest new line ≈ 60 B.
- `FillLogEvent { Kind kind; uint8_t sensor; uint32_t a; uint32_t b; }`,
  FILL_LOG_CAPACITY = 5 (worst step = 5: double-trip step). Payload changes
  add NO new events → capacity safe.
- `endFillFault(nowMs, why, sensor)` is the single fault exit; engine members
  needed for new payloads are all in scope there: `baseline_`, `deadline_`,
  `valveOpenedAtMs_`, `confirmStartMs_`. BaselineFault path: baseline_.done()
  is true ⇒ baseline_(sensor) valid. Guard faults fire only in valve-open
  sub-phase ⇒ elapsedMs(nowMs, valveOpenedAtMs_) valid. ConfirmTimeout:
  elapsedMs(nowMs, confirmStartMs_).
- Abort is logged in step() block 3, not endFillFault: capture
  `wasOpen = valveOpen_` BEFORE `valveOpen_ = false` for openms (0 = never
  opened — settle abort proof).
- logFillEvent today: ARDUINO-region switch, formats NORMATIVE per README.
  Pure-formatter precedent: `formatBootBanner` in UNGUARDED region + native
  tests (test_log_utils) + `logLine("%s", banner)` in setup(). Same shape for
  `formatFillLogEvent(char*, size_t, const FillLogEvent&)`.
- Existing payload assertions (test_fillengine 116–118, 135–137, 186, 253,
  369; test_faultlatch kind/sensor only): NONE assert `.a`/`.b` on
  BaselineFault / EarlyWet / EarlyTrip / BackstopExpired / ConfirmTimeout —
  payload repurpose on those kinds breaks no existing test (verify by run).
- FillStart `a` (=30) and ValveOpen `a` (=30) assertions exist and are kept
  unchanged. Plus5 a=35 kept. Trip a=60000 kept. Done/ConfirmPass unchanged.
- Fresh boot prints recovery=0ms; a real recovery of 0 ms is impossible via
  button (≥40 ms debounce) ⇒ README can document recovery=0ms ⇒ fresh boot,
  table bypassed (FRESH_BOOT_BACKSTOP_MIN).
- Tests: new suite dir `test/test_logfmt/` auto-discovered by `[env:native]`
  (test_filter absent ⇒ all suites run). Pattern: see test_log_utils
  (TEST_ASSERT_EQUAL_STRING exact formats, truncation-safety test).
- P1.M7.T2.S1 grep compatibility: bench greps are PREFIX anchored
  (`FILL start backstop=30min recovery=0ms`) ⇒ APPEND-ONLY field additions
  keep them matching. Never reorder/rename existing fields.
- Sibling consumers: P2.M1.T2.S1 (docs/COMMISSIONING.md) references the FINAL
  formats this task lands in README; P2.M1.T3.S1 syncs README afterwards —
  this task owns the "Serial logging" table update (Mode A).
- No new tunables (config.h untouched). 1 Hz literal precedent: `1000` in
  loop with task-contract comment.

## External research
Deliberately minimal: line formats are firmware-internal design; the only
external dependency semantics (vsnprintf/%lu on arm-none-eabi, CDC writes
non-blocking when unplugged) were established and landed in P1.M1.T2.S2
(code comments in .ino document both). No new library surface in this task.
