# Research notes — P1.M6.T1.S3: Fault latching and blink codes 1/2/3 (pure)

## Verified codebase state (read this session, post-S2-landing)

- `firmware/jug_filler/jug_filler.ino` — S2's FillEngine block HAS LANDED: lines 993–1281
  (`// ===== end fill lifecycle engine (consumed by P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) =====`
  at 1281; ARDUINO region marker at 1283). `test/test_fillengine/` exists (13th native project).
  git: `M firmware/jug_filler/jug_filler.ino`, `?? test/test_fillengine/` (S2's uncommitted work).
- Fault surfaces verified in the landed block:
  - `endFillFault()` (line ~1253): valve LOW FIRST → ValveClosed log → `state_ = Fault` →
    `faultCode_` (3 for BackstopExpired, else sensor+1) → `recordFillEnd(nowMs)`.
  - `faultCode_ = 0` reset only at fill start (line ~1120); no exit from Fault anywhere.
  - `out.faultCode = (state_ == Fault) ? faultCode_ : 0` (line ~1224) — LEVEL output.
  - `FillEngineOutputs` (lines 1070–1079): valveOpen, led, faultCode, plus5Applied,
    acknowledged, ackBlink, logCount, logs[FILL_LOG_CAPACITY=5].
  - Accessors: `state()`, `faultCode()`, `valveOpen()`, `valveOpenedAtMs()`, `deadline()`, `clock()`.

## Key APIs consumed by S3 (verified from source)

- `ButtonClassifier::Event` (line ~113): ONLY `None=0, ShortPress=1, LongPress=2` — the
  complete event vocabulary for the Fault-ignored matrix.
- `fillApplyButton` Fault row (line ~975): `break;` — both events ignored (§7 "BOOT is
  ignored while faulted" is structural).
- `fillLevelOutputs(FillState::Fault)` → `LedPatternEngine::Mode::FaultBlink` (group count
  NOT carried here — it rides on `out.faultCode`/`faultCode()`; S1 comment explicitly defers
  group count to P1.M6.T1.S3).
- `LedPatternEngine` (lines 748–864):
  - `void setMode(Mode mode, uint8_t faultGroups = 1)` — FaultBlink carries n = fault code;
    n==0 clamps to 1 (fail-safe: blink, never look dead). Releases the phase anchor.
  - `Mode mode() const`, `uint8_t faultGroups() const`.
  - `bool tick(uint32_t nowMs)` — lazily anchors phase at FIRST tick after setMode, returns
    LED level. FaultBlink cadence: `n × (on FAULT_BLINK_MS / off FAULT_BLINK_MS)`, then
    `FAULT_PAUSE_MS` off, repeat forever (mod elapsed-since-anchor — wrap-safe).
  - Constants (config.h): `FAULT_BLINK_MS=250`, `FAULT_PAUSE_MS=2000`.
- `RecoveryClock` (lines 637–658): fresh instance → `hasHistory()==false` (RAM only, PRD §5.3).
- `FillDeadline` (lines 660–710): `beginFill` sets `freshBoot_ = !clock.hasHistory()` →
  `tableMinutes_ = FRESH_BOOT_BACKSTOP_MIN (30)`; accessors `freshBoot()`, `tableMinutes()`,
  `recoveryMsAtStart()`. So "reset clears tank history → 30-min default" is provable by
  constructing a new FillEngine while wall time continues.
- config.h verified: `FRESH_BOOT_BACKSTOP_MIN=30`, `CONFIRM_WINDOW_S=12`, `BASELINE_MIN=2000`,
  `BASELINE_MAX=4050`, `READING_INTERVAL_MS=100`, `TRIP_DROP_COUNTS=350`, `DEBOUNCE_COUNT=8`,
  `TRIP_MIN_ELAPSED_S=60`, `EARLY_WET_WINDOW_S=5`.

## S2/S3 boundary analysis (non-duplication)

S2's `test_fillengine` (20 tests) already covers: fault ENTRY per cause with codes
(baseline s1/s2, confirm-timeout s2, early-wet s1, early-trip s1, backstop 3), one
BOOT-ignored assert inside the baseline test, and fuzz law E (Fault entry preserved).
S3's UNIQUE surface (what S2 does NOT test):
1. Complete §7 code matrix: confirm-miss → code 1 (s2-first variant), early-wet/early-trip
   on sensor 2 → code 2 (S2 tested sensor 1 only for guards).
2. EXHAUSTIVE button absorption in Fault: full Event vocabulary cycled across a long soak —
   state/code/led/valve/logs/edges all frozen; valve never opens while faulted.
3. Blink-code PRESENTATION: engine outputs → `setMode(FaultBlink, n)` → exact §7 cadence
   (250/250 × n, 2000 gap) for n = 1, 2, 3 — the pure-layer proof of the T2.S1 wiring.
4. §5.3/§11 reset semantics: fresh instance (simulating RESET/power cycle) after a fault
   WITH tank history → Idle, fault cleared, history WIPED → next fill = 30-min fresh-tank
   default (discriminating: ~1-min recovery would interpolate to 44 — 30 is only reachable
   fresh); never auto-starts.

## Design decisions

- S3 is primarily a VERIFICATION suite: `test/test_faultlatch/test_faultlatch.cpp` (14th
  native project, auto-discovered, no ini edit). The .ino is EXPECTED UNCHANGED — S2's
  engine already implements the full latching contract. Conditional clause: if Task-0
  verification finds a landed deviation from the S2 PRP contract, fix minimally INSIDE the
  S2 bannered block and note it; never restructure.
- Do NOT touch `test/test_fillengine/` (owned by S2, may still be finalizing).
- Test harness: copy of S2's Sim (tests #include the .ino, not each other — each suite is
  self-contained by house pattern). Same timing math: press at t=1000, emissions at t≡0
  (mod 100), baseline verdict at t=3400; DRY=3500, WET=2500 (< 3500−350=3150 → wet);
  TOO_LOW=1000, TOO_HIGH=4200. Fastest fault = baseline fault ≈ 2.4 s after press.
- 30-min backstop sims ≈ 72k passes run in well under a second natively (S2 PRP confirms).
- Fuzz: entry deterministic-fast (baseline fault, both sensors across seeds); the PROPERTY
  under test is absorption: 2000 passes of random events + hostile wet streams cannot
  leave Fault/open the valve/change the code/emit logs; fresh instance then Idles.
  Seeded LCG `s*1664525+1013904223` (house reproducibility contract).

## Risks / gotchas carried into the PRP

- `setMode` must be driven with BOTH args from engine outputs (`o.led, o.faultCode`);
  n==0 would clamp to 1 — assert the engine emits exactly 1..3.
- LedPatternEngine phase anchors at the FIRST `tick` after setMode — cadence instants are
  relative to that first tick's nowMs.
- Unity on PIO 6.2.0: explicit `main()` with RUN_TEST lines REQUIRED.
- Never `pio test -e pico`; `pio run -e pico` is the compile gate.
- Widening: `(uint32_t)X * 1000u` for second constants (only CONFIRM_WINDOW_S needed here).
