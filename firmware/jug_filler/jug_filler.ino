// firmware/jug_filler/jug_filler.ino
//
// ============================== SEAM CONTRACT ==============================
// This single file is BOTH the Arduino sketch and the host-testable logic
// module (architecture: plan/.../firmware_architecture.md §1). Rules:
//
//   1. UNGUARDED region = pure logic ONLY. No Arduino calls whatsoever — no
//      pinMode/digitalWrite/analogRead/millis/Serial/delay, no libraries.
//      Pure C++ that takes `uint32_t nowMs` (and sensor/event data) as
//      parameters. Native tests compile and exercise it on the host.
//   2. #ifdef ARDUINO region = hardware glue ONLY: setup()/loop(), pin
//      writes, Serial logging, watchdog, LED/WS2812 drivers.
//   3. Every function gets an explicit prototype above its definition —
//      host g++ does not run the Arduino .ino prototype generator.
//   4. Tests live in test/*.cpp as:
//      #include "../firmware/jug_filler/jug_filler.ino"
//      and run via `pio test -e native` (which excludes this folder from
//      its own build via build_src_filter in platformio.ini).
//
// SAFETY (PRD §10): the FIRST executable line of setup() drives PIN_VALVE
// LOW. Never insert anything above it. The invariant suite (P1.M7.T1.S1)
// structurally asserts this and scans for delay() in the loop region.
//
// Every later pure-logic subtask lands in the UNGUARDED region below.
// ==========================================================================

#include "config.h"
#include <stdarg.h>   // va_list for the CDC log-line wrapper (host-safe)
#include <stdio.h>    // snprintf/vsnprintf — the only formatting engines (host-safe)

// ---- Prototypes (explicit; native g++ needs them, Arduino tolerates them) ----
uint32_t elapsedMs(uint32_t now, uint32_t since);      // rollover-safe elapsed ms (P1.M1.T2.S1)
bool deadlinePassed(uint32_t now, uint32_t deadline);  // rollover-safe deadline check (P1.M1.T2.S1)
size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused);  // pure banner text (P1.M1.T2.S2)
uint16_t medianOf(uint16_t* values, size_t n);   // median of n counts, sorts in place (P1.M3.T1.S1)
void logLine(const char* fmt, ...);  // bounded, newline-terminated CDC log line (P1.M1.T2.S2; ARDUINO region only)
void setup();             // hardware only (#ifdef ARDUINO)
void loop();              // hardware only (#ifdef ARDUINO)

// ---- UNGUARDED REGION: pure logic (host-testable) ----

// ===================== Rollover-safe time helpers (P1.M1.T2.S1) =====================
// millis() is uint32_t and wraps 0xFFFFFFFF -> 0x00000000 every 2^32 ms (~49.7 days).
// Unsigned C++ arithmetic is defined modulo 2^32 (ISO C++ [expr.arith.conv]) — so
// subtraction of timestamps NEVER overflows/UBs and yields the true elapsed span for
// any span < 2^32 ms. Direct timestamp comparison (now >= deadline) is FORBIDDEN —
// ordering across the wrap is meaningless. All timed logic goes through these two:
//
//   interval question:  elapsedMs(now, sinceMs)        >= SOME_CONFIG_MS
//   deadline question:  deadlinePassed(now, deadlineTs)  (absolute millis timestamp)
//
// Seam rule: `now` is ALWAYS injected — these functions never call millis().

// Milliseconds elapsed from timestamp `since` to timestamp `now` (both absolute
// millis()-domain uint32_t values). Wrap-safe for any true span < 2^32 ms.
uint32_t elapsedMs(uint32_t now, uint32_t since) {
    return now - since;  // mod-2^32 subtraction IS the rollover safety — keep it raw
}

// 2^31 ms ≈ 24.86 days: the maximum forward window deadlinePassed can certify.
// Mathematical constant (not a tunable) — every firmware deadline is ≤ 44 min.
constexpr uint32_t HALF_WHEEL_MS = 0x80000000u;

// True iff the absolute timestamp `deadline` has been reached or passed at `now`
// (rollover-safe, inclusive at equality: a fill backstop sampled exactly at its
// deadline fires — the valve closes AT the deadline, never 1 ms late).
// now == deadline → true; deadline+1 → true; deadline-1 → false; correct even when
// the pair straddles the 49.7-day wrap.
bool deadlinePassed(uint32_t now, uint32_t deadline) {
    return elapsedMs(now, deadline) < HALF_WHEEL_MS;
}
// ============== end time helpers (consumed by P1.M2–P1.M6) ==============

// ===================== Boot banner builder (P1.M1.T2.S2) =====================
// Pure text builder so the boot banner's reset-cause wording is native-testable;
// the hardware read of the SDK bootcause flag is injected as a plain bool. Uses
// snprintf only — no hardware calls (seam rule). Labels are normative:
//   watchdog boot -> "JugFiller boot (reset cause: WATCHDOG)"          (38 chars)
//   any other     -> "JugFiller boot (reset cause: POWER_OR_RESET)"    (44 chars)
// snprintf contract: returns the would-be length (int); writes <= bufLen-1 chars
// plus a NUL; bufLen == 0 touches nothing.
size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused) {
    return (size_t)snprintf(buf, bufLen, "JugFiller boot (reset cause: %s)",
                            watchdogCaused ? "WATCHDOG" : "POWER_OR_RESET");
}

// ===================== BOOT-button classifier (P1.M2.T1.S1) =====================
// Pure press/hold semantics for the BOOT button (PRD §5.1), hardware-agnostic:
// the caller injects the raw level and the time; this class returns the event.
//
//   BUTTON_DEBOUNCE_MS (40)     — a new raw level must persist this long before
//                                 it becomes the accepted level; anything shorter
//                                 is contact bounce and is discarded.
//   BUTTON_LONG_PRESS_MS (1000) — a completed press is Long iff the debounced
//                                 hold lasted >= this; otherwise Short.
//   BOTH events fire on RELEASE — never while held (PRD §5.1 "acts on RELEASE").
//
// Hold-time anchor: press and release are each deemed effective at the theoretical
// acceptance instant (rawEdge + BUTTON_DEBOUNCE_MS). The two debounce windows
// cancel, so measured hold = how long the raw level was continuously pressed —
// independent of how often update() happens to be called. A press raw-stable from
// C to R classifies exactly (R - C) >= 1000.
//
// State-agnostic by design: PRD §5.1's per-state actions/ignores (CONFIRM/FAULT
// ignore everything) belong to the fill engine (P1.M6.T1.S1), which consumes this
// event stream. Caller contract (P1.M2.T2.S1, P1.M6.T2.S1): call update() every
// loop pass with nowMs non-decreasing, same clock domain as all other services.
class ButtonClassifier {
public:
    // Exactly one value per update() call. None is the steady-state return —
    // including the entire time the button is held (no event while held).
    enum Event : uint8_t { None = 0, ShortPress = 1, LongPress = 2 };

    ButtonClassifier()
        : level_(false),          // accepted (debounced) level: not pressed
          candidate_(false),      // raw level being confirmed toward level_
          candidateSince_(0),     // nowMs-domain stamp when candidate_ appeared
          pressStart_(0) {}       // effective start of the current debounced press

    // Feed one raw sample at nowMs. Returns the event caused by this sample:
    // ShortPress or LongPress on the accepted RELEASE of a press, else None.
    // All timing composes elapsedMs() — rollover-safe by construction.
    Event update(uint32_t nowMs, bool pressedRaw) {
        // 1. Stable-sample tracking: any raw flip restarts the confirmation
        //    window, so bounce can never accumulate into an acceptance.
        if (pressedRaw != candidate_) {
            candidate_ = pressedRaw;
            candidateSince_ = nowMs;
        }
        // 2. Accept a pending level change once the candidate has held for the
        //    full debounce interval (>= is inclusive: exactly 40 is enough).
        if (candidate_ != level_ &&
            elapsedMs(nowMs, candidateSince_) >= BUTTON_DEBOUNCE_MS) {
            level_ = candidate_;
            if (level_) {
                // Effective press start = raw edge + debounce (see anchor note).
                // Presses NEVER emit — only releases do (PRD §5.1).
                pressStart_ = candidateSince_ + BUTTON_DEBOUNCE_MS;
                return Event::None;
            }
            // Accepted release: classify the completed hold, emit ONCE, then the
            // state is already reset for the next press (level_ false, candidate
            // tracking continues fresh). Hold = releaseEdge - pressEdge, both
            // anchored the same way, measured rollover-safely.
            const uint32_t holdMs =
                elapsedMs(candidateSince_ + BUTTON_DEBOUNCE_MS, pressStart_);
            return (holdMs >= BUTTON_LONG_PRESS_MS) ? Event::LongPress
                                                    : Event::ShortPress;
        }
        return Event::None;
    }

private:
    bool level_;               // accepted (debounced) level; true == pressed
    bool candidate_;           // latest raw level being confirmed toward level_
    uint32_t candidateSince_;  // stamp (nowMs domain) when candidate_ appeared
    uint32_t pressStart_;      // effective start of the current debounced press
};
// ========= end BOOT-button classifier (consumed by P1.M2.T2.S1, P1.M6.T1.S1) =========

// ===================== Reading aggregator + median helper (P1.M3.T1.S1) =====================
// Sensor chain stage 1 (PRD §7/§8), pure: turns raw 12-bit counts from two
// sensors into ONE averaged reading pair per READING_INTERVAL_MS (100 ms),
// each average over the last ADC_SAMPLES_PER_READING (32) samples. Downstream:
// BaselineCapture (P1.M3.T2.S1) medians emitted pairs for the dry baseline;
// TripDetector + guards (P1.M3.T3.S1/S2) and the fill engine (P1.M6.T1.S2)
// consume the same stream. The clock is injected and wrap-safe via elapsedMs;
// no hardware is touched.
//
// Sensor index domain: 0 == PIN_SENSOR1 (fault code 1), 1 == PIN_SENSOR2
// (fault code 2); any other index is dropped. Samples may burst or trickle —
// the window is time-agnostic; only the reading CADENCE uses the clock.
//
// Warmup: the first sample a sensor ever sees PRE-FILLS its whole 32-slot
// window, so every emitted reading is a valid 32-sample average from the
// first reading onward (weight 31/32 on sample #1, decaying as real samples
// displace it). A sensor never sampled averages 0 — consumers gate on
// BASELINE_MIN..MAX and treat 0 as out-of-range.
//
// Cadence: one pair per READING_INTERVAL_MS anchored at the last EMISSION
// instant; after a >100 ms stall exactly ONE reading emits (no catch-up
// burst), so DEBOUNCE_COUNT consecutive readings always span ~800 ms of real
// time. Anchor starts at 0: first reading due one full interval after
// construction. Protocol (driven once per loop pass by P1.M6.T2.S1):
//   if (agg.readingDue(nowMs)) { agg.takeReading(nowMs, a1, a2); ... }
class ReadingAggregator {
public:
    static constexpr uint8_t SENSOR_COUNT = 2;  // 0 == PIN_SENSOR1, 1 == PIN_SENSOR2

    ReadingAggregator()
        : nextSlot_{0, 0}, seeded_{false, false}, sum_{0, 0}, lastReadingMs_(0) {}

    // One raw 12-bit count for `sensor`. `nowMs` is accepted for seam uniformity
    // (every service takes the injected clock) and deliberately unused: sample
    // arrival timing carries no meaning here, only reading cadence does.
    void addSample(uint8_t sensor, uint16_t counts, uint32_t nowMs) {
        (void)nowMs;
        if (sensor >= SENSOR_COUNT) return;  // out of domain: defensive no-op
        if (!seeded_[sensor]) {
            // First sample ever: pre-fill the whole window (see warmup note).
            for (uint16_t i = 0; i < ADC_SAMPLES_PER_READING; ++i)
                window_[sensor][i] = counts;
            sum_[sensor] = (uint32_t)counts * ADC_SAMPLES_PER_READING;
            seeded_[sensor] = true;
            nextSlot_[sensor] = 0;
            return;
        }
        // Running sum: add the new sample, drop the evicted one. Exact under
        // mod-2^32 arithmetic even when counts < evicted (the true window sum
        // never exceeds 32 * 65535 < 2^21, so the wraps cancel).
        sum_[sensor] += (uint32_t)counts - window_[sensor][nextSlot_[sensor]];
        window_[sensor][nextSlot_[sensor]] = counts;
        if (++nextSlot_[sensor] == ADC_SAMPLES_PER_READING) nextSlot_[sensor] = 0;
    }

    // True iff a reading emission is due at `nowMs`: >= READING_INTERVAL_MS
    // since the last emission (inclusive at exactly 100; wrap-safe by
    // construction — elapsedMs is mod-2^32 subtraction).
    bool readingDue(uint32_t nowMs) const {
        return elapsedMs(nowMs, lastReadingMs_) >= READING_INTERVAL_MS;
    }

    // Emit the current averaged pair (truncating division) and re-anchor the
    // cadence at `nowMs`. Total function — legal to call unconditionally; the
    // intended protocol pairs it with readingDue (see class comment).
    void takeReading(uint32_t nowMs, uint16_t& outAvg1, uint16_t& outAvg2) {
        outAvg1 = (uint16_t)(sum_[0] / ADC_SAMPLES_PER_READING);
        outAvg2 = (uint16_t)(sum_[1] / ADC_SAMPLES_PER_READING);
        lastReadingMs_ = nowMs;  // re-anchor at the emission instant — no bursts
    }

private:
    uint16_t window_[SENSOR_COUNT][ADC_SAMPLES_PER_READING];  // boxcar slots (128 B)
    uint8_t  nextSlot_[SENSOR_COUNT];                         // slot the next sample overwrites
    bool     seeded_[SENSOR_COUNT];                           // first sample seen (pre-filled)?
    uint32_t sum_[SENSOR_COUNT];                              // running window sums
    uint32_t lastReadingMs_;                                  // cadence anchor (last emission)
};

// Median of n counts — PRD §7.1: dry baselines are the MEDIAN of averaged
// readings (breakdown point 50%: one wet splash cannot move it; the mean
// would follow any outlier). Free function: BaselineCapture (P1.M3.T2.S1)
// owns the buffer. CONTRACT: sorts `values` ASCENDING IN PLACE (insertion
// sort; n is tens at most) — pass a scratch/mutable buffer.
//   n == 0 -> 0 (degenerate; callers pass >= 1)   odd n -> values[n/2]
//   even n -> (values[n/2-1] + values[n/2]) / 2, truncated — summed in
//   uint32_t: 65535 + 65534 = 131069 OVERFLOWS uint16_t (wraps to 533).
uint16_t medianOf(uint16_t* values, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 1; i < n; ++i) {  // insertion sort, ascending
        const uint16_t key = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }
    const size_t mid = n / 2;
    if (n % 2 == 1) return values[mid];
    return (uint16_t)(((uint32_t)values[mid - 1] + (uint32_t)values[mid]) / 2u);
}
// ========= end reading aggregator (consumed by P1.M3.T2.S1, P1.M3.T3.S1/S2, P1.M6.T1.S2) =========

// ===================== Baseline capture + plausibility gate (P1.M3.T2.S1) =====================
// Sensor chain stage 2 (PRD §7.1), pure: on fill start, SETTLE for
// BASELINE_SETTLE_MS (2000 ms) with the valve still closed (settle runs inside
// FILLING, valve LOW — arch §3.1), collecting NOTHING; then collect exactly
// READINGS_PER_SENSOR (5) averaged reading pairs from the ReadingAggregator
// stream (P1.M3.T1.S1) and finish with per-sensor dry baselines = the MEDIAN
// of those 5 readings (breakdown point 50%: one wet splash or one wild spike
// during capture cannot move the baseline) plus a plausibility verdict against
// BASELINE_MIN..MAX (2000..4050 counts, 12-bit) — this also catches "jug
// already full / tips wet" accidental starts (a wet sensor reads far below the
// dry range, PRD §9.3) and a never-sampled sensor (aggregator emits 0 —
// out-of-range by design, P1.M3.T1.S1).
//
// Verdicts are the fill gate (PRD §7.1): if either sensor is not Ok, the fill
// engine (P1.M6.T1.S2) faults that sensor (fault code = sensor index + 1,
// P1.M6.T1.S3) and REFUSES to open the valve. This class only REPORTS — it
// never touches hardware.
//
// Caller contract (P1.M6.T1.S2, driven once per loop pass by P1.M6.T2.S1;
// nowMs non-decreasing, same clock domain as all services):
//   baseline.begin(fillStartMs);                    // at fill start, valve closed
//   if (agg.readingDue(nowMs)) {
//       agg.takeReading(nowMs, a1, a2);
//       baseline.addReading(nowMs, a1, a2);         // total: dropped until settle elapses
//   }
//   if (baseline.done()) { /* route verdict()/baseline() once */ }
// addReading is time-gated ONLY by the settle: it does not check the reading
// cadence (the aggregator owns cadence) and is legal to call on every emitted
// pair. All timing composes elapsedMs (rollover-safe, P1.M1.T2.S1).
//
// Abort during settle is the FSM's business (P1.M6.T1.S2): there is NO cancel
// method by design — the FSM simply stops calling; begin() resets all state
// for the next fill. Accessors are meaningful once done(); before that
// baseline() is 0 and verdict() is TooLow — fail-safe: verdict(sensor) ALWAYS
// equals the classification of baseline(sensor) under BASELINE_MIN..MAX, and
// 0 < BASELINE_MIN, so a premature read can never look Ok.
class BaselineCapture {
public:
    // Per-sensor plausibility of the captured dry baseline (PRD §7.1).
    enum Verdict : uint8_t { Ok = 0, TooLow = 1, TooHigh = 2 };

    // Averaged readings medians per sensor — task contract (P1.M3.T2.S1),
    // NOT a §8 tunable. Odd on purpose: the median of 5 is the pure middle
    // element (no even-pair truncation); 5 x 100 ms = 500 ms of capture.
    static constexpr uint8_t READINGS_PER_SENSOR = 5;

    BaselineCapture()
        : phase_(Phase::Settle), startMs_(0), taken_(0),
          baseline_{0, 0}, verdict_{Verdict::TooLow, Verdict::TooLow} {}

    // Start a new capture at `nowMs` (the fill-start instant). Resets everything.
    void begin(uint32_t nowMs) {
        phase_ = Phase::Settle;
        startMs_ = nowMs;
        taken_ = 0;
        baseline_[0] = 0;
        baseline_[1] = 0;
        verdict_[0] = Verdict::TooLow;  // fail-safe defaults: baseline 0
        verdict_[1] = Verdict::TooLow;  // classifies below BASELINE_MIN
    }

    // Feed one emitted averaged pair. TOTAL: during Settle the pair is DROPPED
    // unless BASELINE_SETTLE_MS has elapsed since begin() (inclusive at exactly
    // 2000 — house rule); the first accepted pair flips to Collect. On the
    // READINGS_PER_SENSOR-th accepted pair the medians + verdicts are computed
    // once and the capture is Done — later pairs are ignored.
    void addReading(uint32_t nowMs, uint16_t avg1, uint16_t avg2) {
        if (phase_ == Phase::Done) return;  // stable after finish
        if (phase_ == Phase::Settle) {
            if (elapsedMs(nowMs, startMs_) < BASELINE_SETTLE_MS) return;
            phase_ = Phase::Collect;  // settle elapsed — start collecting HERE
        }
        readings_[0][taken_] = avg1;
        readings_[1][taken_] = avg2;
        if (++taken_ >= READINGS_PER_SENSOR) {
            // medianOf sorts each buffer ascending in place — fine: the raw
            // order is never needed again once the medians are taken.
            baseline_[0] = medianOf(readings_[0], READINGS_PER_SENSOR);
            baseline_[1] = medianOf(readings_[1], READINGS_PER_SENSOR);
            verdict_[0] = classify(baseline_[0]);
            verdict_[1] = classify(baseline_[1]);
            phase_ = Phase::Done;
        }
    }

    // True iff the capture finished (READINGS_PER_SENSOR pairs collected
    // post-settle). The FSM's only poll; verdicts/baselines gate on this.
    bool done() const { return phase_ == Phase::Done; }

    // Captured dry baseline (counts) for `sensor` (0 == PIN_SENSOR1 == fault
    // code 1, 1 == PIN_SENSOR2 == fault code 2). 0 until done() or for an
    // out-of-domain index — consumers treat 0 as out-of-range (aggregator
    // convention, P1.M3.T1.S1).
    uint16_t baseline(uint8_t sensor) const {
        return (sensor < ReadingAggregator::SENSOR_COUNT) ? baseline_[sensor] : 0;
    }

    // Plausibility verdict for `sensor`. TooLow until done() / for an
    // out-of-domain index — fail-safe: verdict(sensor) ALWAYS equals the
    // classification of baseline(sensor), and 0 < BASELINE_MIN.
    Verdict verdict(uint8_t sensor) const {
        return (sensor < ReadingAggregator::SENSOR_COUNT) ? verdict_[sensor]
                                                          : Verdict::TooLow;
    }

private:
    enum Phase : uint8_t { Settle = 0, Collect = 1, Done = 2 };

    // PRD §7.1 plausibility gate: bounds INCLUSIVE (2000 and 4050 are Ok).
    static Verdict classify(uint16_t v) {
        if (v < BASELINE_MIN) return Verdict::TooLow;
        if (v > BASELINE_MAX) return Verdict::TooHigh;
        return Verdict::Ok;
    }

    Phase     phase_;       // Settle -> Collect -> Done (one-way)
    uint32_t  startMs_;     // begin() instant — settle anchor
    uint8_t   taken_;       // accepted pairs so far (0..READINGS_PER_SENSOR)
    uint16_t  readings_[ReadingAggregator::SENSOR_COUNT][READINGS_PER_SENSOR];  // capture buffers / median scratch (20 B)
    uint16_t  baseline_[ReadingAggregator::SENSOR_COUNT];  // medians, valid at Done
    Verdict   verdict_[ReadingAggregator::SENSOR_COUNT];   // classifications, valid at Done
};
// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========

// ---- #ifdef ARDUINO REGION: hardware glue ----
#ifdef ARDUINO

#include <hardware/watchdog.h>   // pico-sdk watchdog, shipped inside arduino-pico (verified)

// Bounded CDC log line (P1.M1.T2.S2) — the ONLY logging API; later subtasks
// (P1.M2.T2.S1, P1.M3.T1.S2, P1.M6.T2.S1) must use it, never raw CDC writes.
// Policy (PRD §2.2/§3/§10): one vsnprintf into a fixed buffer, one bounded write,
// one newline — NEVER gated on connection state (USB is unplugged in service), so
// logging can never stall the loop. Overlong lines truncate (drop the excess, keep
// the newline); a formatting error (n < 0) drops the line.
constexpr size_t LOGLINE_MAX = 128;  // bound, not a tunable — sole consumer is logLine

void logLine(const char* fmt, ...) {
    char line[LOGLINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0) return;  // formatting error: drop the line, keep the loop alive
    size_t len = ((size_t)n >= sizeof(line)) ? sizeof(line) - 1 : (size_t)n;
    Serial.write(reinterpret_cast<const uint8_t*>(line), len);
    Serial.write('\n');
}

// ---- BOOTSEL button service (P1.M2.T2.S1) ----
// Hardware side of the BOOT button: sample the __Bootsel bool object each loop
// pass, feed the pure classifier (P1.M2.T1.S1), log each event with its hold
// duration. NO pinMode for BOOTSEL — it is a global object, not a pin; the bool
// conversion (true == pressed) momentarily floats the QSPI flash-CS line with
// IRQs parked and the other core idled — designed for loop() use (external_deps
// §1). Runtime reads can never enter the bootloader (entry needs BOOTSEL held
// during RESET), matching PRD §2.1's intent.
static ButtonClassifier s_button;      // classifier instance; RAM state only (PRD §3)
static bool     s_btnPrevRaw = false;  // previous raw level (edge stamps below)
static uint32_t s_btnRiseMs  = 0;      // stamp of raw press edge    (false -> true)
static uint32_t s_btnFallMs  = 0;      // stamp of raw release edge  (true  -> false)
// Hold-duration policy: the classifier's contract exposes events only (no hold
// accessor, by design), so the wiring layer stamps the RAW edges itself. The
// printed duration is raw first-edge to last-edge — within contact-bounce
// (~tens of ms) of the classifier's theoretical hold. Diagnostic only: the
// classifier stays the sole authority for debounce and short/long classification.
// ---- end BOOTSEL button service state (used by loop, kept by P1.M6.T2.S1) ----

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

void setup() {
  digitalWrite(PIN_VALVE, LOW);   // LINE 1 — PRD §10: valve closed before all else
  pinMode(PIN_VALVE, OUTPUT);     // drive the pre-latched LOW (glitch-free)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);     // external LED off at boot
  Serial.begin(SERIAL_BAUD);      // USB CDC, always on — NEVER while(!Serial) (PRD §3)
  analogReadResolution(ADC_BITS); // MANDATORY 12-bit — every config count is 12-bit

  // Boot banner (P1.M1.T2.S2): reset cause from the SDK bootcause flag, read once
  // BEFORE the watchdog is re-enabled; ONE bounded CDC write via logLine.
  char banner[64];  // 44-char worst case fits with headroom
  formatBootBanner(banner, sizeof(banner), watchdog_caused_reboot());
  logLine("%s", banner);

  // Hardware watchdog LAST in setup() (arch boot order): 8000 ms < 8388 ms ceiling
  // (RP2040-E1); pause_on_debug=true so bench debugging never trips it. loop() feeds
  // it as its first statement every pass — a hang resets -> setup() line 1 closes
  // the valve -> IDLE, never auto-refill (PRD §10, §5.3).
  watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true);
}

void loop() {
  watchdog_update();   // FIRST STATEMENT — PRD §10: hang -> reset -> valve LOW -> IDLE
  // ---- BOOTSEL poll (P1.M2.T2.S1) — the loop's second stage (arch loop order) ----
  const uint32_t nowMs = millis();     // one clock snapshot per pass, shared downstream
  const bool pressedRaw = BOOTSEL;     // __Bootsel operator bool(): true == pressed
  if (pressedRaw != s_btnPrevRaw) {    // raw edge: stamp it (log-only bookkeeping)
    if (pressedRaw) s_btnRiseMs = nowMs;
    else            s_btnFallMs = nowMs;
    s_btnPrevRaw = pressedRaw;
  }
  const ButtonClassifier::Event ev = s_button.update(nowMs, pressedRaw);
  if (ev != ButtonClassifier::None) {  // fired exactly once, on accepted RELEASE
    const uint32_t holdMs = elapsedMs(s_btnFallMs, s_btnRiseMs);  // wrap-safe
    logLine("BTN %s %lums",
            ev == ButtonClassifier::LongPress ? "long" : "short", holdMs);
  }
  // ---- end BOOTSEL poll ----
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
  // P1.M6.T2.S1 fills the loop below this line (fill engine routing, LED, event
  // logs via logLine; ADC sampling landed in P1.M3.T1.S2). Nothing may ever be
  // inserted above watchdog_update().
}

#endif  // ARDUINO
