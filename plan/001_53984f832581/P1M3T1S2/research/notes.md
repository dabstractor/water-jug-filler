# Research Notes — P1.M3.T1.S2 (ADC hardware wiring: 12-bit + raw-count commissioning log)

## §1 Verified codebase state (this session, after P1.M3.T1.S1 landed)

All verified by direct read/execution on 2025-06 (current tree):

- `firmware/jug_filler/jug_filler.ino` (306+ lines):
  - UNGUARDED region contains `ReadingAggregator` + `medianOf` (S1 landed), end-banner
    at **line 263**: `// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========`
  - Frozen public surface confirmed in-file: `SENSOR_COUNT = 2`, `addSample(uint8_t, uint16_t, uint32_t)`,
    `readingDue(uint32_t) const`, `takeReading(uint32_t, uint16_t&, uint16_t&)`.
    Sensor 0 == PIN_SENSOR1 (fault code 1), 1 == PIN_SENSOR2 (fault code 2).
  - ARDUINO region starts at the marker (grep count of both region markers = **2**).
    `// ---- end BOOTSEL button service state (used by loop, kept by P1.M6.T2.S1) ----` = **line 307**.
  - `void setup()` = **line 309**; **line 315 already contains**
    `analogReadResolution(ADC_BITS); // MANDATORY 12-bit — every config count is 12-bit`
    (grep count `analogReadResolution` = 1). → setup() needs NO changes from this task; verify only.
  - `loop()` landmarks: `watchdog_update()` first statement (verified), `const uint32_t nowMs = millis();`
    single shared clock snapshot in the BOOTSEL stage, `// ---- end BOOTSEL poll ----` = **line 346**,
    placeholder comment `// P1.M6.T2.S1 fills the loop below this line (ADC, fill engine, LED, event`
    = **line 347**.
  - Only `delay(` occurrence in the file is inside the seam-contract COMMENT (line 22) — no live delay.
- `firmware/jug_filler/config.h`:
  - `ADC_BITS = 12` exists (§5 group, line ~47) with comment: "analogReadResolution(ADC_BITS) is
    MANDATORY in setup() — the arduino-pico default is 10-bit; every count below is a 12-bit count (0–4095)."
    at **lines 49–50**. It does NOT yet state the 3.3 V full-scale assumption → Mode A docs gap this
    task closes (comment-only edit).
  - `PIN_SENSOR1 = 26` (GP26/A0/ADC0), `PIN_SENSOR2 = 27` (GP27/A1/ADC1).
- Tests: `pio test -e native` → test_seam, test_time_utils, test_log_utils, test_button, **test_reading**
  all PASSED (run this session). `pio run -e pico` → `[SUCCESS]` in 0.9 s (incremental).
- platformio.ini: `[env:pico]` compile-only, `[env:native]` Unity. No changes needed.

## §2 Binding external facts (from plan/001_53984f832581/architecture/external_deps.md §3 — VERIFIED)

- arduino-pico `analogRead(pin)` **defaults to 10-bit (0–1023)**; returns 0–4095 only after
  `analogReadResolution(12)`. PRD §8 thresholds (BASELINE_MIN=2000, BASELINE_MAX=4050,
  TRIP_DROP_COUNTS=350) are 12-bit counts — without the call every dry baseline reads ~750–980
  (10-bit domain) and every fill baseline-faults.
- `analogReadResolution(int bits)` exists; 3.3 V nominally reads 4095 (ADC reference = AVDD = 3.3 V rail).
- Pin map: A0=GP26/ADC0, A1=GP27/ADC1, A2=GP28, A3=GP29 — GP26–GP29 are the only ADC-capable GPIOs.
- Source: https://arduino-pico.readthedocs.io/en/latest/analog.html
- `analogRead(pin)` takes the GPIO number (26/27) — A0/A1 aliases exist but config.h stores GPIO numbers.
- No `pinMode` needed for ADC pins — analogRead configures the pad itself.
- analogRead is a blocking conversion (~µs at the default ADC clock) — two reads per pace tick
  cannot threaten the 8 s watchdog budget.

## §3 Design decisions (binding for this task)

1. **setup() is done.** `analogReadResolution(ADC_BITS)` already landed (line 315). This task VERIFIES
   it (Task 0 gate), never re-adds or moves it. PRD §10 ordering (valve LOW first) untouched.
2. **Sampling pacing = derived constant, not a tunable.** `ADC_SAMPLE_PACE_MS = READING_INTERVAL_MS /
   ADC_SAMPLES_PER_READING` = 100/32 = 3 ms (truncated) → exactly 32 samples span ~96 ms of each
   100 ms reading interval, so every emitted average is a true boxcar over (nearly) the whole interval,
   and a full fresh window is always ready before `readingDue()` fires (96 < 100 — no race). Defined
   in the ARDUINO region (hardware-glue concern, derived from §8 constants — NOT added to config.h,
   which is frozen per Mode A / §8 completeness). Rationale: sampling once per raw loop pass
   (~tens of kHz) would collapse the 32-sample window onto the last ~1 ms of signal — useless
   averaging; deterministic pacing makes trip timing reproducible.
3. **Commissioning log source = the latest EMITTED AVERAGED pair.** "Raw-count" names the UNIT
   (12-bit counts, not volts/%), per the item and external_deps: the values commissioning must
   observe are the ones §8 thresholds act on — averaged readings (§9.3 sets BASELINE_MIN/MAX around
   the observed dry range; baselines are medians OF averaged readings per §7.1). Format EXACTLY
   `S1=%u S2=%u` (e.g. `S1=3120 S2=3098`) — the literal format from the item contract, greppable by
   P2.M1.T1.S1's audit. 1 Hz via `elapsedMs(nowMs, s_lastAdcLogMs) >= 1000` — literal 1000 is the
   item contract's own 1 Hz (not a §8 tunable); comment it as such.
4. **Loop stage placement** = architecture loop order (firmware_architecture.md: watchdog → BOOTSEL →
   sample ADCs → aggregator → FillEngine): insert between `// ---- end BOOTSEL poll ----` and the
   P1.M6.T2.S1 placeholder comment, and amend the placeholder's parenthetical to drop "ADC" (now landed).
   Reuse the BOOTSEL stage's `nowMs` snapshot — one `millis()` per pass is the established pattern.
5. **State block placement** = BOOTSEL service pattern: bannered `static` block immediately after
   the BOOTSEL state block (after line 307), before `void setup()`; instance of the pure class,
   driven only from loop().
6. **Stage order within the block**: sample gate → emission gate (`readingDue`/`takeReading` caches
   the pair) → 1 Hz log gate. Freshest samples are in the window before any emission.
7. **No new native tests.** Item contract: "Mock: device-only; aggregator math native-tested in
   P1.M3.T1.S1". This task adds only `#ifdef ARDUINO` glue — invisible to the native build. Its
   gates are compile (pico), regression (native), and structural greps.
8. **Stage cost**: 2 analogRead + arithmetic per 3 ms; one logLine per 1000 ms. No watchdog risk;
   no delay(); logLine is bounded and never connection-gated (P1.M1.T2.S2 policy).

## §4 Validation evidence (commands run this session against the current tree)

- `grep -c analogReadResolution firmware/jug_filler/jug_filler.ino` → **1**
- region-marker grep → **2**; `grep -c "class ReadingAggregator"` → **1**
- loop-first-statement watchdog check (sed window between `void loop()` and `watchdog_update`) → hit
- `pio run -e pico` → `1 succeeded` (0.866 s)
- `pio test -e native` → all 5 projects PASSED
- config.h ADC comment lines 49–50 confirmed missing the 3.3 V statement (the only config.h edit is
  comment text; `git diff` must show no code-line changes there).
