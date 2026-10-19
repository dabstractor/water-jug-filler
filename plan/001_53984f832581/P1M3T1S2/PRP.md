# PRP — P1.M3.T1.S2: ADC hardware wiring — 12-bit resolution + raw-count commissioning log

## Goal

**Feature Goal**: Wire the physical ADC chain into `loop()` so the RP2040 samples both moisture sensors (GP26/A0 → sensor 1, GP27/A1 → sensor 2) in 12-bit counts (0–4095, 3.3 V full-scale), feeds them through the landed `ReadingAggregator` (P1.M3.T1.S1), and emits a 1 Hz raw-count commissioning log line `S1=<counts> S2=<counts>` on USB CDC for PRD §9.3 dry/wet observation. The 12-bit resolution call `analogReadResolution(ADC_BITS)` is ALREADY in `setup()` (line 315) — this task verifies it, never duplicates it. Device-only glue: zero changes to the UNGUARDED (pure) region, zero new native tests.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, ARDUINO region only: (a) a bannered ADC-service state block inserted after the BOOTSEL state block (after line 307, before `void setup()`); (b) a loop stage inserted between `// ---- end BOOTSEL poll ----` (line 346) and the P1.M6.T2.S1 placeholder comment, plus a one-line amendment of that placeholder's parenthetical (drop "ADC" — it has now landed). `setup()` byte-stable.
- `firmware/jug_filler/config.h` — MODIFIED, comment-only (Mode A docs): the ADC group comment gains the 3.3 V full-scale statement. Zero code-line changes.

**Success Definition**:
- `pio run -e pico` → `[SUCCESS]`: the ADC stage compiles under arm-gcc/arduino-pico.
- `pio test -e native` → exit 0, all five projects `[PASSED]` (test_seam, test_time_utils, test_log_utils, test_button, test_reading) — regression only; this task adds no tests (item contract: device-only, aggregator math proven in P1.M3.T1.S1).
- Structural gates: `analogReadResolution` count still exactly 1; region markers still 2; `analogRead(PIN_SENSOR1)` / `analogRead(PIN_SENSOR2)` each exactly 1; no `delay(` added; UNGUARDED region byte-stable (`git diff` confined to the ARDUINO region).
- `git diff firmware/jug_filler/config.h` shows comment lines only.
- On hardware (optional here; bench-owned by P1.M7.T2.S1): one `S1=xxxx S2=xxxx` line per second over USB CDC, dry sensors reading ~3000–3900.

## User Persona

**Target User**: (1) The commissioning operator following PRD §9.3 — sensors in open air, watching serial to record dry counts; tips submerged, recording wet counts. (2) Downstream implementers: P1.M6.T2.S1 (integration loop consumes the cached averaged pair) and P2.M1.T1.S1 (commissioning log audit greps the `S1=/S2=` format).

**Use Case**: During commissioning, the operator opens a serial monitor at 115200 baud and sees one raw-count line per second; the observed dry range sets `BASELINE_MIN/MAX`, the dry→wet delta sets `TRIP_DROP_COUNTS` (§9.3). In service, the same stage silently feeds the aggregator that drives baselines and trip detection.

**User Journey**: flash → board boots → banner → within ~1 s the first `S1=3120 S2=3098` line appears → operator dips a sensor tip → counts drop visibly over a few lines → operator records dry/wet values and updates §8 constants.

**Pain Points Addressed**: without `analogReadResolution(12)` every count is 10-bit and every fill baseline-faults (verified: arduino-pico defaults to 10-bit); without a periodic count log, §9.3 calibration is guesswork; without paced sampling, a 32-sample average would collapse onto the last millisecond of signal.

## Why

- PRD §7/§8/§9: the entire sensor chain is specified in 12-bit counts (`BASELINE_MIN=2000`, `BASELINE_MAX=4050`, `TRIP_DROP_COUNTS=350`); §9.3 calibration requires observing dry (~3000–3900) and wet counts; §2.1 fixes GP26/GP27 as the only sane ADC pins for the two sensors.
- Architecture (`firmware_architecture.md` §1 loop order): `watchdog_update → BOOTSEL → sample ADCs → aggregator → FillEngine.step` — this task lands the "sample ADCs → aggregator" stage in its exact slot, before the fill engine exists.
- Item contract: input = `ReadingAggregator` from P1.M3.T1.S1 (landed, verified this session) + skeleton/logLine from P1.M1.T1.S2/P1.M1.T2.S2 (landed); output = live averaged readings + raw-count log consumed by P1.M6.T2.S1 and P2.M1.T1.S1; docs = Mode A (config.h ADC comment must state the 12-bit requirement AND that counts assume 3.3 V full-scale — the 3.3 V half is currently missing).
- Binding research note (`architecture/external_deps.md` §3, VERIFIED): `analogRead` defaults to 10-bit; `analogReadResolution(int)` exists; 3.3 V nominally reads 4095; A0=GP26/ADC0, A1=GP27/ADC1.

## What

ARDUINO-region glue only:

1. **ADC service state block** (after the BOOTSEL state block, before `void setup()`), following the BOOTSEL block's pattern exactly:
   - `constexpr uint32_t ADC_SAMPLE_PACE_MS = READING_INTERVAL_MS / ADC_SAMPLES_PER_READING;` (= 3 ms truncated; DERIVED structural constant — NOT a config.h tunable; §8 is frozen).
   - `static ReadingAggregator s_adc;` — instance of the pure class; `static uint32_t s_adcSampleMs = 0;` (sample pacing anchor); `static uint32_t s_adcLogMs = 0;` (1 Hz log anchor); `static uint16_t s_adcAvg1 = 0, s_adcAvg2 = 0;` (cache of the latest emitted averaged pair — the downstream handoff point for P1.M6.T2.S1).
2. **Loop stage** (between `---- end BOOTSEL poll ----` and the P1.M6.T2.S1 placeholder), in this order:
   - **Sample gate**: `if (elapsedMs(nowMs, s_adcSampleMs) >= ADC_SAMPLE_PACE_MS)` → `s_adcSampleMs = nowMs;` then for both sensors: `s_adc.addSample(0, (uint16_t)analogRead(PIN_SENSOR1), nowMs);` and `s_adc.addSample(1, (uint16_t)analogRead(PIN_SENSOR2), nowMs);`. NO `pinMode` for ADC pins (analogRead configures the pad). Reuse the BOOTSEL stage's `nowMs` snapshot — never a second `millis()` call.
   - **Emission gate**: `if (s_adc.readingDue(nowMs)) s_adc.takeReading(nowMs, s_adcAvg1, s_adcAvg2);` — the aggregator's documented protocol, exactly as frozen in P1.M3.T1.S1.
   - **1 Hz commissioning log gate**: `if (elapsedMs(nowMs, s_adcLogMs) >= 1000)` → `s_adcLogMs = nowMs;` `logLine("S1=%u S2=%u", (unsigned)s_adcAvg1, (unsigned)s_adcAvg2);` — logs the LATEST EMITTED AVERAGED pair in raw 12-bit counts (the exact quantity §8 thresholds act on; see Known Gotchas).
3. **Placeholder comment amendment**: the loop's trailing comment currently reads `// P1.M6.T2.S1 fills the loop below this line (ADC, fill engine, LED, event` / `// logs via logLine). Nothing may ever be inserted above watchdog_update().` → reword the parenthetical to `(fill engine routing, LED, event logs via logLine; ADC sampling landed in` / `P1.M3.T1.S2)` — keep the `watchdog_update()` sentence byte-identical.
4. **config.h Mode A docs** (comment-only): extend the ADC group comment so it states BOTH the 12-bit requirement (already there) AND the 3.3 V full-scale assumption (missing).

NOT in scope (owners): aggregator math (P1.M3.T1.S1 — landed); baseline settle/plausibility (P1.M3.T2.S1); trip/guards (P1.M3.T3.S1/S2); routing readings into the fill engine / valve / LED (P1.M6.T2.S1); bench observation and calibration constants (P1.M7.T2.S1, §9.3); any new config constant; any UNGUARDED-region change; any new native test.

### Success Criteria

- [ ] ADC state block present exactly once, after `---- end BOOTSEL button service state ----`, before `void setup()`, bannered per listing
- [ ] Loop stage present exactly once between `---- end BOOTSEL poll ----` and the amended P1.M6.T2.S1 comment; order = sample → emit → log; single shared `nowMs`
- [ ] `analogRead(PIN_SENSOR1)` and `analogRead(PIN_SENSOR2)` each appear exactly once in the file; zero `pinMode` for pins 26/27; zero second `analogReadResolution` call
- [ ] `logLine("S1=%u S2=%u", ...)` present exactly once; no raw `Serial.` calls added (logLine is the only logging API)
- [ ] config.h diff = the one comment extension, no code lines; `.ino` diff confined to the ARDUINO region (`#ifdef ARDUINO` … `#endif`)
- [ ] `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, five projects `[PASSED]`

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: exact current line-numbered anchors read this session; the full text of every insertion provided verbatim (state block, loop stage, comment amendments); the consumed API (`ReadingAggregator` surface, `elapsedMs`, `logLine`) quoted from the landed file; every validation command executed against the current tree this session with expected outputs recorded; the one surprising fact (resolution call already landed at line 315) verified by grep and gated in Task 0.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit (ARDUINO region only). Verified-current anchors: line 307
        `// ---- end BOOTSEL button service state (used by loop, kept by P1.M6.T2.S1) ----`
        (insert state block AFTER it); line 309 `void setup()` (DO NOT TOUCH — resolution
        call already at line 315); line 346 `// ---- end BOOTSEL poll ----` and line 347
        the P1.M6.T2.S1 placeholder (insert loop stage BETWEEN them); the BOOTSEL stage's
        `const uint32_t nowMs = millis();` is the shared clock snapshot to reuse.
  gotcha: ReadingAggregator + medianOf (lines ~165–263) and everything above the ARDUINO
        marker are OFF-LIMITS — P1.M3.T1.S1 owns them; byte-stable.

- file: firmware/jug_filler/config.h
  why: READ + one comment edit. ADC group lines 49–50 currently say the 10-bit default /
        12-bit counts (0–4095) — you add the 3.3 V full-scale sentence (Task 1 listing).
        Constants consumed BY NAME: PIN_SENSOR1(26), PIN_SENSOR2(27), READING_INTERVAL_MS(100),
        ADC_SAMPLES_PER_READING(32), ADC_BITS(12).
  gotcha: NEVER hardcode 32/100/26/27/12 — always the config names (§9 retunes ADC values).
        Do NOT add ADC_SAMPLE_PACE_MS here — it is derived (100/32), a glue-layer concern.

- file: plan/001_53984f832581/P1M3T1S1/PRP.md
  why: CONTRACT for the aggregator you drive. Frozen surface: addSample(sensor, counts,
        nowMs) with sensor 0==PIN_SENSOR1/1==PIN_SENSOR2, out-of-domain dropped; first
        sample pre-fills the window; readingDue(nowMs) const, inclusive at 100 ms;
        takeReading(nowMs, outAvg1, outAvg2) emits the truncated 32-sample mean and
        re-anchors the cadence (no catch-up bursts). Protocol (quoted from that PRP):
        `if (agg.readingDue(nowMs)) { agg.takeReading(nowMs, a1, a2); ... }`.
  gotcha: takeReading is the ONLY cadence-advancing call; calling readingDue every pass
        is free. Do not invent extra calls (getAverage/isSeeded do not exist — frozen).

- file: plan/001_53984f832581/architecture/external_deps.md
  section: §3 "ADC — VERIFIED, second binding correction"
  why: the binding research note: analogRead defaults 10-bit; analogReadResolution(int)
        exists; 3.3 V nominally reads 4095; A0=GP26/ADC0, A1=GP27/ADC1 (GP26–GP29 only
        ADC-capable pins); without the call dry baselines read ~750–980 → every fill
        baseline-faults.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: §1 "The one architectural decision" (loop-order diagram + invariants)
  why: your stage's slot: `watchdog_update → sample BOOTSEL → button events →
        sample ADCs → aggregator → FillEngine.step`. Invariants: no blocking delay()
        in loop(); log lines small and on-change-or-periodic; hardware only in #ifdef ARDUINO.

- file: test/test_reading/test_reading.cpp
  why: proof that the aggregator you feed behaves as assumed (16 native tests, green).
        You add NO test — the item contract says device-only; if you feel an urge to add
        one, the logic belongs in the pure region (already landed) — resist.

- url: https://arduino-pico.readthedocs.io/en/latest/analog.html
  why: official arduino-pico analog docs (source of the binding note): analogReadResolution
        semantics, 10-bit default, A0–A3 = GP26–GP29, ~4095 at full-scale AVDD (3.3 V).
  critical: resolution is GLOBAL and persistent — one call in setup() covers every later
        analogRead; there is no per-call resolution argument.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # seam + time helpers + banner + ButtonClassifier
│   │                         #   + ReadingAggregator/medianOf (P1.M3.T1.S1, landed)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service, setup/loop
│   │                         # ← EDIT (ARDUINO region only: state block + loop stage)
│   └── config.h              # every §8 tunable — one comment edit (Mode A)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   └── test_reading/         # P1.M3.T1.S1's — untouched, must stay green
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, Unity 2.6.1.
# Baseline this session: pio run -e pico [SUCCESS]; pio test -e native → 5/5 PASSED.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   ├── jug_filler.ino        # MODIFIED: ADC service state block + loop sampling stage
│   │                         #   (sample → aggregate → emit → 1 Hz S1=/S2= log); amended
│   │                         #   P1.M6.T2.S1 placeholder comment
│   └── config.h              # MODIFIED (comment-only): ADC group notes 3.3 V full-scale
└── test/                     # UNCHANGED — no new tests (device-only glue)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: analogReadResolution(ADC_BITS) is ALREADY in setup() (line 315, landed
//   earlier). Do NOT add a second call, move it, or "improve" its comment — Task 0
//   verifies count == 1. The 10-bit-default failure mode is already neutralized; your
//   job is the loop wiring + log.

// CRITICAL: "raw-count" log values = the latest EMITTED AVERAGED pair (s_adcAvg1/2),
//   NOT an extra one-off analogRead. Rationale (binding): "raw" names the unit (12-bit
//   counts, not volts); §9.3 calibrates BASELINE_MIN/MAX/TRIP_DROP_COUNTS against the
//   same quantity the firmware compares (averaged readings — baselines are medians OF
//   them, §7.1). Logging a fresh single-sample read would show a different (noisier)
//   distribution than the rules act on.

// CRITICAL: pace the sampling (3 ms gate), do NOT sample every loop pass. Loop() runs
//   at ~tens of kHz; unpaced, the 32-sample boxcar collapses onto the last ~1 ms of
//   signal — averaging becomes useless and trip timing unreproducible. 100/32 = 3
//   (truncated) → 32 samples span ~96 ms < the 100 ms cadence, so a full fresh window
//   is always ready before readingDue fires — no race, by construction.

// CRITICAL: reuse the BOOTSEL stage's `const uint32_t nowMs = millis();` — ONE clock
//   snapshot per pass is the established loop pattern. A second millis() call in the
//   same pass invites inconsistent gating decisions across stages.

// CRITICAL: UNGUARDED region must stay byte-stable — this task is #ifdef ARDUINO glue
//   only. `git diff` showing any hunk above the `---- #ifdef ARDUINO REGION` marker
//   (or in test/) means you have strayed; revert it.

// GOTCHA: NO pinMode for GP26/GP27 — arduino-pico's analogRead configures the ADC pad
//   itself; a pinMode(A0, INPUT) style call is at best redundant, at worst wrong.

// GOTCHA: analogRead returns int; cast to uint16_t for addSample. At ADC_BITS=12 the
//   range is 0..4095 — no clamp needed (resolution caps it).

// GOTCHA: never gate the log on Serial connection state and never call Serial.write
//   directly — logLine (P1.M1.T2.S2) is the ONLY logging API; it is bounded,
//   newline-terminated, and unplug-safe by design.

// GOTCHA: the 1000 in the 1 Hz gate is the item contract's own 1 Hz, not a §8 tunable —
//   comment it that way; do NOT add a constant to config.h (§8 frozen, Mode A).

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
//   `pio run -e pico` is the compile gate. Do not upload in this task (bench work is
//   P1.M7.T2.S1).
```

## Implementation Blueprint

### Data models and structure

No new types. State added to the ARDUINO region (hardware-glue layer), total ~16 bytes:

```cpp
constexpr uint32_t ADC_SAMPLE_PACE_MS = READING_INTERVAL_MS / ADC_SAMPLES_PER_READING;  // 3 ms
static ReadingAggregator s_adc;        // pure class from P1.M3.T1.S1, driven from loop()
static uint32_t s_adcSampleMs = 0;     // sample-pacing anchor (last sample-pair instant)
static uint32_t s_adcLogMs    = 0;     // 1 Hz commissioning-log anchor
static uint16_t s_adcAvg1 = 0;         // latest emitted averaged pair — the handoff
static uint16_t s_adcAvg2 = 0;         //   point P1.M6.T2.S1 reads to route downstream
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "analogReadResolution" firmware/jug_filler/jug_filler.ino      # EXPECT: 1
  - RUN: grep -c "class ReadingAggregator" firmware/jug_filler/jug_filler.ino   # EXPECT: 1
  - RUN: grep -c "end reading aggregator" firmware/jug_filler/jug_filler.ino    # EXPECT: 1
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "end BOOTSEL button service state\|end BOOTSEL poll\|P1.M6.T2.S1 fills the loop" firmware/jug_filler/jug_filler.ino  # EXPECT: 3 distinct hits (your anchors)
  - RUN: pio test -e native                                                      # EXPECT: 5/5 PASSED
  - A miss means the seam moved or P1.M3.T1.S1 regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/config.h — Mode A comment (comment-only)
  - EDIT: replace the two comment lines
      // ADC_BITS: analogReadResolution(ADC_BITS) is MANDATORY in setup() — the
      //   arduino-pico default is 10-bit; every count below is a 12-bit count (0–4095).
    with the three lines from the Task 1 listing below. NO code lines may change.

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — ADC service state block
  - EDIT: insert the "Task 2 listing" verbatim AFTER line 307
      `// ---- end BOOTSEL button service state (used by loop, kept by P1.M6.T2.S1) ----`
    and BEFORE `void setup() {`, with one blank line each side.

Task 3: MODIFY firmware/jug_filler/jug_filler.ino — loop sampling stage
  - EDIT: insert the "Task 3 listing" verbatim AFTER `  // ---- end BOOTSEL poll ----`
    and BEFORE the P1.M6.T2.S1 placeholder comment; then amend the placeholder's first
    line exactly as shown in the listing (drop "ADC" from the future-work list; keep the
    watchdog sentence byte-identical).

Task 4: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — config.h ADC comment (replace the two lines, add the third):**

```cpp
// ADC_BITS: analogReadResolution(ADC_BITS) is MANDATORY in setup() — the
//   arduino-pico default is 10-bit; every count below is a 12-bit count (0–4095)
//   assuming the 3.3 V rail as full-scale (AVDD reference; 3.3 V ≈ 4095).
```

**Task 2 listing — insert verbatim (state block):**

```cpp
// ---- ADC sampling service state (P1.M3.T1.S2) ----
// Hardware side of the sensor chain (PRD §2.1/§7/§8): pace out 32 raw 12-bit
// samples per sensor across each READING_INTERVAL_MS (100 ms) window and feed
// them to the pure ReadingAggregator (P1.M3.T1.S1). Pacing is DERIVED, not a
// tunable: READING_INTERVAL_MS / ADC_SAMPLES_PER_READING = 100/32 = 3 ms
// (truncated) — 32 samples span ~96 ms < 100 ms, so a full fresh window is
// always ready before readingDue() fires, and the average is a true boxcar over
// (nearly) the whole interval instead of the last millisecond of signal.
// Resolution: setup() calls analogReadResolution(ADC_BITS) once (arduino-pico
// defaults to 10-bit — without it every §8 count threshold is ~4x off; see
// external_deps §3). No pinMode for GP26/GP27 — analogRead configures the pad.
constexpr uint32_t ADC_SAMPLE_PACE_MS = READING_INTERVAL_MS / ADC_SAMPLES_PER_READING;  // 3 ms, derived — NOT a §8 tunable
static ReadingAggregator s_adc;     // aggregator instance; RAM state only
static uint32_t s_adcSampleMs = 0;  // anchor: last sample-pair instant
static uint32_t s_adcLogMs    = 0;  // anchor: last commissioning log line
static uint16_t s_adcAvg1 = 0;      // latest emitted averaged pair (counts) — the
static uint16_t s_adcAvg2 = 0;      //   handoff P1.M6.T2.S1 routes into FillEngine
// ---- end ADC sampling service state (used by loop, kept by P1.M6.T2.S1) ----
```

**Task 3 listing — insert verbatim (loop stage + amended placeholder):**

```cpp
  // ---- ADC sample / aggregate / commissioning log (P1.M3.T1.S2) — third stage ----
  // Order inside the stage matters: SAMPLE first (freshest samples are in the
  // window), then EMIT the reading pair, then LOG. All timing composes elapsedMs
  // against the single nowMs snapshot taken above (one clock read per pass).
  if (elapsedMs(nowMs, s_adcSampleMs) >= ADC_SAMPLE_PACE_MS) {
    s_adcSampleMs = nowMs;  // re-anchor at the sample instant — even pacing
    s_adc.addSample(0, (uint16_t)analogRead(PIN_SENSOR1), nowMs);  // 0 == S1 / fault 1
    s_adc.addSample(1, (uint16_t)analogRead(PIN_SENSOR2), nowMs);  // 1 == S2 / fault 2
  }
  if (s_adc.readingDue(nowMs)) {
    s_adc.takeReading(nowMs, s_adcAvg1, s_adcAvg2);  // emits + re-anchors cadence
  }
  // Commissioning log (PRD §9.3): one line per second of the LATEST EMITTED
  // averaged pair, in raw 12-bit counts (3.3 V full-scale) — the same quantity
  // every §8 threshold compares against, so observed dry/wet ranges transfer
  // directly to BASELINE_MIN/MAX and TRIP_DROP_COUNTS. 1000 ms literal = this
  // task's 1 Hz contract, not a §8 tunable. Consumed by the §9 commissioning
  // audit (P2.M1.T1.S1); greppable as ^S1=.
  if (elapsedMs(nowMs, s_adcLogMs) >= 1000) {
    s_adcLogMs = nowMs;
    logLine("S1=%u S2=%u", (unsigned)s_adcAvg1, (unsigned)s_adcAvg2);
  }
  // ---- end ADC sample / aggregate / commissioning log ----
```

Immediately after this stage, AMEND the existing placeholder comment from:

```cpp
  // P1.M6.T2.S1 fills the loop below this line (ADC, fill engine, LED, event
  // logs via logLine). Nothing may ever be inserted above watchdog_update().
```

to (watchdog sentence byte-identical):

```cpp
  // P1.M6.T2.S1 fills the loop below this line (fill engine routing, LED, event
  // logs via logLine; ADC sampling landed in P1.M3.T1.S2). Nothing may ever be
  // inserted above watchdog_update().
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: hardware-service state block (mirrors the BOOTSEL service exactly):
//   bannered static block above setup(), driven from a bannered loop stage,
//   banner names task ID + downstream consumers, end-banner says who keeps it.

// PATTERN: gate-and-reanchor timing (house rule — never subtract-and-compare):
//   if (elapsedMs(nowMs, anchor) >= INTERVAL_MS) { anchor = nowMs; /* act */ }
//   Wrap-safe by construction; inclusive at exactly INTERVAL_MS.

// CRITICAL: sensor index mapping is positional and frozen (P1.M3.T1.S1):
//   addSample(0, ...) == PIN_SENSOR1 == fault code 1; addSample(1, ...) ==
//   PIN_SENSOR2 == fault code 2. Swapping them misattributes every downstream
//   fault code — the comments in the listing pin the mapping at the call site.

// CRITICAL: takeReading is called ONLY under readingDue() — it advances the
//   cadence; unconditional calling would emit at loop rate (contract violation).

// GOTCHA: the stage must not grow: two analogRead calls per 3 ms and one logLine
//   per 1000 ms is the whole budget. Nothing else belongs here until P1.M6.T2.S1
//   routes s_adcAvg1/2 into the fill engine.
```

### Integration Points

```yaml
FIRMWARE:
  - firmware/jug_filler/jug_filler.ino: +1 state block (after BOOTSEL state block),
    +1 loop stage (between BOOTSEL poll and the P1.M6.T2.S1 placeholder), 1 comment
    line amended. setup()/loop() skeleton ordering untouched: watchdog_update()
    stays the first loop statement; valve-LOW stays setup() line 1.
BUILD:
  - None (platformio.ini compiles the sketch for pico as-is).
CONFIG:
  - Comment-only edit in config.h ADC group (Mode A). ZERO new constants anywhere:
    pacing is derived in the ARDUINO region; the 1 Hz literal is this task's own
    contract, commented as such.
TEST:
  - None. Device-only glue; aggregator math is native-proven (test_reading, 16 tests).
DOCS:
  - Mode A satisfied by the config.h comment (12-bit requirement + 3.3 V full-scale).
    No other docs (README sync is P2.M1.T3.S1; commissioning runbook is P2.M1.T2.S1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - baseline settle + BASELINE_MIN..MAX gate          -> P1.M3.T2.S1
  - trip rule + false-trip guards                      -> P1.M3.T3.S1/S2
  - routing s_adcAvg1/2 into FillEngine, valve, LED    -> P1.M6.T2.S1
  - bench dry/wet observation, §9.3 calibration        -> P1.M7.T2.S1 (uses this log)
  - log-coverage audit of the §9 workflow              -> P2.M1.T1.S1 (greps S1=/S2=)
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the ADC stage compiles under arm-gcc/arduino-pico
# (analogRead/analogReadResolution resolve against the arduino-pico core).
# Errors here mean a transcription deviation from the listings — fix, don't redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; test_seam, test_time_utils, test_log_utils, test_button AND
# test_reading all [PASSED]. This task adds NO tests (device-only glue is invisible
# to the native build — that is the point of the seam). Any failure = regression:
# your diff touched something outside the ARDUINO region. Find it with:
git diff --stat
# Expected: firmware/jug_filler/jug_filler.ino and firmware/jug_filler/config.h only.
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Structural wiring — counts verified against this task's contract:
grep -c "analogReadResolution" firmware/jug_filler/jug_filler.ino          # 1 (setup's, untouched)
grep -c "analogRead(PIN_SENSOR1)" firmware/jug_filler/jug_filler.ino      # 1
grep -c "analogRead(PIN_SENSOR2)" firmware/jug_filler/jug_filler.ino      # 1
grep -c "logLine(\"S1=%u S2=%u\"" firmware/jug_filler/jug_filler.ino      # 1
grep -c "ADC_SAMPLE_PACE_MS" firmware/jug_filler/jug_filler.ino           # 2 (def + use)

# (2) No pinMode pollution of ADC pins, no second clock snapshot in the stage:
grep -n "pinMode(2[6789]" firmware/jug_filler/jug_filler.ino              # no hits
sed -n '/---- ADC sample/,/---- end ADC sample/p' firmware/jug_filler/jug_filler.ino \
  | grep -c "millis()"                                                    # 0 (reuses nowMs)

# (3) Region hygiene — the diff must live entirely inside #ifdef ARDUINO:
grep -nE -- "---- UNGUARDED REGION|---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
git diff firmware/jug_filler/jug_filler.ino | grep "^@@"                  # all hunks below the
                                                                          # ARDUINO marker line no.
# (4) config.h diff is comment-only:
git diff firmware/jug_filler/config.h | grep "^[+-]" | grep -v "^[+-][+-]" \
  | grep -vE "^[+-]\s*//"                                                 # no hits (all changed lines are //)

# (5) No blocking delay introduced in the loop region:
sed -n '/^void loop()/,/^}/p' firmware/jug_filler/jug_filler.ino | grep -c "delay("   # 0
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Device-only behavior (OPTIONAL here; formal bench is P1.M7.T2.S1). If a board
# is attached and the operator chooses to smoke-test:
pio run -e pico -t upload   # then open the USB CDC monitor at 115200
# Expected on the monitor: boot banner, then one line/second like `S1=3120 S2=3098`.
# Dry sensors (open air): ~3000–3900 (§9.3). Finger on sensor tip: visible drop
# over a few lines. If values sit in 0–1023, the resolution call was bypassed
# (impossible per gates — this check exists to catch toolchain surprises).

# Without hardware, Level 4 is satisfied by the structural gates in Level 3 plus
# the arithmetic audit: 100/32 = 3 ms pace -> 32 samples/96 ms < 100 ms cadence
# (no emission race); log line worst case "S1=4095 S2=4095" = 17 chars, far
# under LOGLINE_MAX (128) — truncation impossible.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–3 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, five projects `[PASSED]`
- [ ] Structural counts from Level 3 exactly as specified (resolution 1, each analogRead 1, logLine format 1, pace const 2, pinMode ADC 0, stage millis() 0, loop delay( 0)
- [ ] `git status --short`: only `firmware/jug_filler/jug_filler.ino` and `firmware/jug_filler/config.h` modified; no test/, platformio.ini, or UNGUARDED-region changes

### Feature Validation

- [ ] Loop stage order = sample → emit → log, all gates via `elapsedMs` against the single `nowMs`
- [ ] Sensor mapping pinned at call site: `addSample(0, …analogRead(PIN_SENSOR1)…)` / `addSample(1, …analogRead(PIN_SENSOR2)…)`
- [ ] 1 Hz line format exactly `S1=%u S2=%u` via logLine; values = latest emitted averaged pair
- [ ] setup() byte-stable (resolution call already landed — verified, not duplicated)
- [ ] config.h ADC comment now states BOTH the 12-bit requirement AND 3.3 V full-scale
- [ ] P1.M6.T2.S1 placeholder amended (ADC dropped from its future-work list; watchdog sentence intact)

### Code Quality Validation

- [ ] Follows the BOOTSEL service pattern (bannered static state block + bannered loop stage naming task ID and consumers)
- [ ] All constants by config name (PIN_SENSOR1/2, READING_INTERVAL_MS, ADC_SAMPLES_PER_READING); no magic 26/27/100/32
- [ ] logLine is the only logging API used; no connection-gated I/O; no new public members added to ReadingAggregator
- [ ] Zero new native tests (item contract), zero UNGUARDED-region edits, zero new config constants

### Documentation & Deployment

- [ ] State-block and stage comments explain pacing derivation (96 < 100 no-race), the averaged-pair log rationale, and the sensor-index/fault-code mapping
- [ ] Mode A docs complete (config.h comment); no other doc files touched (owners: P2.M1.T2.S1, P2.M1.T3.S1)
- [ ] No new environment variables, no platformio.ini changes

---

## Anti-Patterns to Avoid

- ❌ Don't add a second `analogReadResolution` call or move the existing one — it landed at file line 315 (inside setup()); duplicate calls are noise and Task 0 pins count == 1
- ❌ Don't sample every loop pass "because it's more data" — unpaced sampling collapses the 32-sample window onto ~1 ms of signal; the 3 ms pace IS the feature
- ❌ Don't log a fresh one-off `analogRead` for the commissioning line — log the emitted averaged pair; §9.3 calibrates against exactly what the thresholds compare
- ❌ Don't hand-roll `now - last >= N` timestamp math — compose `elapsedMs()` (wrap safety is the entire point of P1.M1.T2.S1)
- ❌ Don't add `ADC_SAMPLE_PACE_MS` (or any constant) to config.h — §8 is frozen; the pace is derived in the glue layer
- ❌ Don't add `pinMode(PIN_SENSOR1, …)` — arduino-pico's analogRead configures the ADC pad
- ❌ Don't call `millis()` again inside the stage or condition logging on `Serial` — reuse `nowMs`; use `logLine` only
- ❌ Don't route `s_adcAvg1/2` into any state machine, valve logic, or baseline check — that is P1.M6.T2.S1 / P1.M3.T2.S1 territory; this task ends at the cache + log
- ❌ Don't touch the UNGUARDED region, test/, platformio.ini, or the BOOTSEL blocks — parallel/completed tasks own them
- ❌ Don't add native tests for glue that the native build cannot see — the seam exists precisely so hardware wiring needs none

---

**Confidence Score: 9/10** — every anchor and gate was verified by direct execution against the current tree this session (including the surprise that S1 already landed and the resolution call already exists — both turned into Task 0 gates); all insertions are provided verbatim in the repo's established banner-comment style; the consumed aggregator surface was read from the landed file, not assumed. Residual risk: on-hardware behavior (actual counts) is inherently untestable in CI — mitigated by the structural gates, the arithmetic audit (96 < 100 ms), and the optional Level 4 smoke test; formal bench validation is deliberately deferred to P1.M7.T2.S1 per the plan.
