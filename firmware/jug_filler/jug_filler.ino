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
uint32_t backstopMinutes(uint32_t recoveryMin);  // concave backstop table lookup, floored (P1.M4.T1.S1)
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

// ===================== Trip detection, per sensor (P1.M3.T3.S1) =====================
// Sensor chain stage 3 (PRD §7.3), pure and TIMELESS: an averaged reading at
// or below (baseline − TRIP_DROP_COUNTS = 350 counts) for DEBOUNCE_COUNT = 8
// CONSECUTIVE readings means that sensor is wet = tripped. One instance per
// sensor (0 == PIN_SENSOR1 == fault code 1, 1 == PIN_SENSOR2 == fault code 2);
// instances share no state, so sensor independence is structural.
//
// Debounce logic: readings at or below the threshold advance a streak; any
// reading STRICTLY above it resets the streak to zero (PRD §7.3 "at or below
// (baseline − TRIP_DROP_COUNTS) for DEBOUNCE_COUNT consecutive readings" —
// the boundary baseline−350 itself QUALIFIES, inclusive; baseline−349 resets).
// When the streak reaches DEBOUNCE_COUNT the trip LATCHES: tripped() stays
// true for the remainder of the fill regardless of later readings — the
// valve-close decision (PRD §7.4) must never chatter. The latch is cleared
// only by begin() (next fill).
//
// This class owns ONLY the count-domain rule. Everything time-domain is the
// guards' business (P1.M3.T3.S2: TRIP_MIN_ELAPSED_S / EARLY_WET_WINDOW_S)
// and everything action-domain is the fill engine's (P1.M6.T1.S2: close
// valve, CONFIRM window).
//
// Caller contract (P1.M6.T1.S2, fed once per emitted averaged reading):
//   at fill start, after BaselineCapture (P1.M3.T2.S1) reports verdict Ok:
//     trip1.begin(baseline.baseline(0));   // VALIDATED dry baseline
//     trip2.begin(baseline.baseline(1));
//   per emitted pair:  trip1.feed(a1); trip2.feed(a2);
//   poll:              if (trip1.tripped() || trip2.tripped()) { /* engine */ }
// begin() with an UNVALIDATED baseline is a caller bug: a baseline below
// TRIP_DROP_COUNTS would wrap the threshold to a huge value (unsigned
// arithmetic is mod-2^16) and the detector would simply never trip — the
// fail-safe direction (the fill backstop, PRD §7.7, catches it as fault 3
// "timer, no trip"). No clamping on purpose: a clamp to 0 would make every
// reading qualify = trip-on-anything = unsafe.
class TripDetector {
public:
    TripDetector() : threshold_(0), streak_(0), tripped_(false) {}

    // Arm for a new fill with the sensor's VALIDATED dry baseline (counts,
    // BASELINE_MIN..BASELINE_MAX — BaselineCapture::verdict(s) == Ok).
    // Computes the wet threshold baseline − TRIP_DROP_COUNTS once, resets
    // the streak and clears the latch.
    void begin(uint16_t baseline) {
        threshold_ = (uint16_t)(baseline - TRIP_DROP_COUNTS);
        streak_ = 0;
        tripped_ = false;
    }

    // One AVERAGED reading from the ReadingAggregator stream (P1.M3.T1.S1).
    // At/below threshold advances the streak; strictly above resets it; the
    // DEBOUNCE_COUNT-th consecutive qualifying reading latches the trip.
    // No-op once latched (readings after the trip cannot change anything).
    void feed(uint16_t avg) {
        if (tripped_) return;  // latched for the remainder of the fill
        if (avg <= threshold_) {
            if (++streak_ >= DEBOUNCE_COUNT) tripped_ = true;
        } else {
            streak_ = 0;  // strictly above the threshold — streak broken
        }
    }

    // Latched trip flag — the ONLY output. Consumed by the fill engine
    // (P1.M6.T1.S2: close valve -> CONFIRM) and the guards (P1.M3.T3.S2).
    bool tripped() const { return tripped_; }

private:
    uint16_t threshold_;  // baseline − TRIP_DROP_COUNTS, set by begin()
    uint8_t  streak_;     // consecutive qualifying readings (0..DEBOUNCE_COUNT)
    bool     tripped_;    // latched once the streak reached DEBOUNCE_COUNT
};
// ========= end trip detector (consumed by P1.M3.T3.S2, P1.M6.T1.S2) =========

// ===================== False-trip guards (P1.M3.T3.S2) =====================
// Sensor chain stage 5 (PRD §7.6), pure: classify trips and wet readings that
// are TOO EARLY to be real water arrival as false trips (electrical short,
// wet tips, jug already full) so the fill engine faults the offending sensor
// instead of trusting it. Two windows, both measured as fill time = time since
// valveOpenedAtMs — supplied BY THE ENGINE as a parameter each call; this
// class does NOT track when the valve opened (P1.M6.T1.S2 owns that):
//   - Early-wet:  an averaged reading at/below baseline − TRIP_DROP_COUNTS
//     (the SAME inclusive wet predicate as the trip rule, PRD §7.3 /
//     P1.M3.T3.S1) while fill time is inside the first EARLY_WET_WINDOW_S
//     (5 s) -> the sensor reads wet before water can possibly have reached
//     it -> short.
//   - Early-trip: a trip ACCEPTED by the TripDetector (tripped() latched,
//     P1.M3.T3.S1) while fill time is before TRIP_MIN_ELAPSED_S (60 s) ->
//     false trip.
// Both windows close AT equality (strict '<'): a trip accepted at exactly
// 60.000 s is LEGAL (Ok) and a wet reading at exactly 5.000 s is legal.
//
// Verdict precedence when several conditions hold at once (deterministic; the
// fault OUTCOME is identical either way — the engine faults the named sensor
// and latches, PRD §7 fault table): early-wet wins over early-trip (a short is
// the more specific diagnosis, and wetness necessarily PRECEDED the trip
// because the 8-reading debounce takes >= 800 ms), and sensor 1 (index 0) is
// reported before sensor 2 (index 1).
//
// Caller contract (P1.M6.T1.S2 — once per loop pass while FILLING; loop
// cadence is sub-millisecond, so the 60 s boundary cannot be straddled):
//   at fill start, with the SAME validated baselines the TripDetectors got
//   (BaselineCapture verdict Ok, P1.M3.T2.S1):
//     guards.begin(baseline.baseline(0), baseline.baseline(1));
//   every loop pass in FILLING, after feeding the detectors:
//     const GuardVerdict v = guards.evaluate(nowMs, valveOpenedAtMs,
//                                            trip1.tripped(), trip2.tripped(),
//                                            avg1, avg2);
//     if (v.kind != GuardVerdict::Ok) { /* fault path: valve LOW first, latch */ }
// evaluate() is STATELESS (a pure function of its inputs, const): the engine
// owns verdict consumption and fault latching (P1.M6.T1.S3). Calling it
// outside FILLING is out of contract: if nowMs predates valveOpenedAtMs the
// elapsed wraps to a huge fill time, both windows read closed, verdict Ok —
// harmless. begin() with an UNVALIDATED baseline (< TRIP_DROP_COUNTS) wraps
// the wet threshold to a huge value -> every reading reads wet -> EarlyWet
// fault on the first evaluation: out-of-contract, but the fail-safe
// direction (fault = the valve stays closed / is driven LOW first). No
// clamping on purpose.
struct GuardVerdict {
    enum Kind : uint8_t { Ok = 0, EarlyTripFault = 1, EarlyWetFault = 2 };
    Kind    kind;    // Ok, or which rule fired
    uint8_t sensor;  // 0 == sensor 1 (PIN_SENSOR1, fault code 1),
                     // 1 == sensor 2 (PIN_SENSOR2, fault code 2); 0 when Ok
};
class FalseTripGuards {
public:
    FalseTripGuards() : wetThreshold1_(0), wetThreshold2_(0) {}

    // Arm for a new fill with the sensors' VALIDATED dry baselines (counts,
    // BASELINE_MIN..BASELINE_MAX — BaselineCapture::verdict(s) == Ok).
    // Computes each wet threshold baseline − TRIP_DROP_COUNTS once, exactly
    // like TripDetector::begin does for the trip rule — the SAME predicate.
    void begin(uint16_t baseline1, uint16_t baseline2) {
        wetThreshold1_ = (uint16_t)(baseline1 - TRIP_DROP_COUNTS);
        wetThreshold2_ = (uint16_t)(baseline2 - TRIP_DROP_COUNTS);
    }

    // Classify one loop instant. nowMs and valveOpenedAtMs are timestamp-domain
    // uint32_t values injected by the engine; tripped1/2 are
    // TripDetector::tripped() (the detector's entire observable state);
    // avg1/2 are the latest averaged readings from the ReadingAggregator.
    // Wrap-safe fill time via elapsedMs() ONLY — direct timestamp comparison
    // is forbidden (time-helpers seam rule). Window constants are SECONDS in
    // config.h — widen to uint32_t BEFORE the x1000 or it truncates.
    GuardVerdict evaluate(uint32_t nowMs, uint32_t valveOpenedAtMs,
                          bool tripped1, bool tripped2,
                          uint16_t avg1, uint16_t avg2) const {
        const uint32_t fillTimeMs = elapsedMs(nowMs, valveOpenedAtMs);

        // Early-wet window (first EARLY_WET_WINDOW_S; closes AT equality).
        if (fillTimeMs < (uint32_t)EARLY_WET_WINDOW_S * 1000u) {
            if (avg1 <= wetThreshold1_) return { GuardVerdict::EarlyWetFault, 0 };
            if (avg2 <= wetThreshold2_) return { GuardVerdict::EarlyWetFault, 1 };
        }
        // Early-trip window (before TRIP_MIN_ELAPSED_S; closes AT equality).
        if (fillTimeMs < (uint32_t)TRIP_MIN_ELAPSED_S * 1000u) {
            if (tripped1) return { GuardVerdict::EarlyTripFault, 0 };
            if (tripped2) return { GuardVerdict::EarlyTripFault, 1 };
        }
        return { GuardVerdict::Ok, 0 };
    }

private:
    uint16_t wetThreshold1_;  // baseline1 − TRIP_DROP_COUNTS, set by begin()
    uint16_t wetThreshold2_;  // baseline2 − TRIP_DROP_COUNTS, set by begin()
};
// ========= end false-trip guards (consumed by P1.M6.T1.S2, P1.M6.T1.S3) =========

// ===================== Backstop table interpolation (P1.M4.T1.S1) =====================
// Timer model (PRD §6.2), pure: the CONCAVE backstop curve. Given minutes of
// recovery since the previous fill's valve closed (wall-clock recovery owned
// by P1.M4.T2.S1), return the maximum valve-open minutes granted to a fill
// started at that recovery — piecewise-LINEAR between the BACKSTOP_TABLE_MIN
// knots, CLAMPED at both ends:
//   recovery <= 2 min   -> 44   (steep: tank refills fast at low back-pressure)
//   recovery >= 60 min  -> 30   (flat: near-full tank, high pressure, slow fill)
// The concave shape is DELIBERATE (PRD §6.2): a straight line pads the
// mid-range in exactly the wrong direction. Interpolate between the given
// knots ONLY — no smoothing, no safety padding (§6 "never padded").
//
// ROUNDING (contract, explicit): the interpolated RESULT is FLOORED to whole
// minutes. Every segment falls (b1 < b0), and for integer b0:
//   floor(b0 - x) == b0 - ceil(x)
// so the DROP is computed with a CEILING division of strictly non-negative
// integers. A plain truncating '/' on the drop would round the result UP =
// a longer valve time = forbidden padding. Integer math only, no floats.
// Unsigned-safe by construction: (b0-b1), (recoveryMin-r0), (r1-r0) are all
// >= 0 inside a segment and the product stays <= 44*58 << 2^32.
//
// Caller contract (P1.M4.T2.S1 — computed ONCE at fill start, PRD §6.2):
//   budget = backstopMinutes(recoveryMinutesAtFillStart);
// Fresh boot / any reset has NO history: the caller uses FRESH_BOOT_BACKSTOP_MIN
// directly and never calls this with a fabricated recovery. A long-press +5
// (PLUS5_MIN) extends the ACTIVE fill's deadline only — never this table.
uint32_t backstopMinutes(uint32_t recoveryMin) {
    // Widen knot fields once (uint16_t -> uint32_t): every comparison and
    // subtraction below is plain unsigned 32-bit — no mixed-sign arithmetic.
    const uint32_t firstR = (uint32_t)BACKSTOP_TABLE_MIN[0].recoveryMin;
    const uint32_t firstB = (uint32_t)BACKSTOP_TABLE_MIN[0].backstopMin;
    const uint32_t lastR =
        (uint32_t)BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN - 1].recoveryMin;
    const uint32_t lastB =
        (uint32_t)BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN - 1].backstopMin;

    if (recoveryMin >= lastR) return lastB;    // clamp high:  >= 60 min -> 30
    if (recoveryMin <= firstR) return firstB;  // clamp low:   <= 2 min  -> 44

    // Here 2 < recoveryMin < 60: find the bracketing knot pair. The loop stops
    // at i == BACKSTOP_TABLE_LEN - 2 at the latest (recoveryMin < 60), so the
    // i + 1 access never runs out of bounds.
    uint8_t i = 0;
    while (recoveryMin >= (uint32_t)BACKSTOP_TABLE_MIN[i + 1].recoveryMin) ++i;
    const uint32_t r0 = BACKSTOP_TABLE_MIN[i].recoveryMin;      // left knot recovery
    const uint32_t b0 = BACKSTOP_TABLE_MIN[i].backstopMin;      // left knot backstop
    const uint32_t r1 = BACKSTOP_TABLE_MIN[i + 1].recoveryMin;  // right knot recovery
    const uint32_t b1 = BACKSTOP_TABLE_MIN[i + 1].backstopMin;  // right knot backstop

    // Ceiling division of the non-negative drop (see ROUNDING above):
    //   drop_exact = (b0 - b1) * (recoveryMin - r0) / (r1 - r0)
    const uint32_t drop =
        ((b0 - b1) * (recoveryMin - r0) + (r1 - r0) - 1) / (r1 - r0);
    return b0 - drop;  // == floor of the exact interpolated backstop, minutes
}
// ===== end backstop interpolation (consumed by P1.M4.T2.S1, P1.M6.T1.S2) =====

// ===================== Recovery clock & fill deadline (P1.M4.T2.S1) =====================
// Timer-model STATE (PRD §6): wall-clock recovery between fills, and the
// per-fill valve-open budget. RAM only — NO persistence (PRD §3): any reset
// re-constructs these objects, which clears history, so the next fill uses
// FRESH_BOOT_BACKSTOP_MIN (PRD §5.3 "tank history cleared", §6.1). That is
// the entire §5.3 story for this state — documented, not implemented.
//
// RecoveryClock (PRD §6.1): the reference point is the moment the previous
// fill's VALVE CLOSED — whatever ended it (done, abort or fault): the fill
// engine (P1.M6.T1.S2) calls recordFillEnd() at that instant for all three.
// Recovery for a new fill = wall-clock ms since that reference, wrap-safe
// via elapsedMs (P1.M1.T2.S1). Abort restarts the clock exactly like a
// normal fill end — same call, same meaning.
//
// FillDeadline (PRD §6 preamble + §6.2): the backstop bounds how long the
// VALVE may stay open, so the deadline is ANCHORED at the valve-open
// instant, NOT at the button press (the 2 s baseline settle precedes valve
// open). The budget MINUTES are computed ONCE at fill start (button press):
//     no history  -> FRESH_BOOT_BACKSTOP_MIN (assume full tank: fast fill)
//     has history -> backstopMinutes(recoveryMs / 60000)   (P1.M4.T1.S1)
// Sub-minute recovery truncates to whole minutes (house integer style; the
// table itself is minute-granular). A long-press +5 (PLUS5_MIN, PRD §5.1,
// FILLING only) extends the ACTIVE fill's deadline only — stacks,
// repeatable — and never touches the table or the recovery clock.

class RecoveryClock {
public:
    // Every fill end — done, abort or fault: whatever closed the valve.
    void recordFillEnd(uint32_t nowMs) {
        lastFillEndMs_ = nowMs;
        hasHistory_ = true;
    }

    // False until the first recordFillEnd() — i.e. on every fresh boot.
    bool hasHistory() const { return hasHistory_; }

    // Wall-clock ms since the previous fill's valve closed (wrap-safe).
    // Caller MUST check hasHistory() first: with no history the return is
    // meaningless — use FRESH_BOOT_BACKSTOP_MIN, never this value.
    uint32_t recoveryMs(uint32_t nowMs) const {
        return elapsedMs(nowMs, lastFillEndMs_);
    }

private:
    bool hasHistory_ = false;    // RAM only: cleared by any reset (PRD §5.3)
    uint32_t lastFillEndMs_ = 0;
};

class FillDeadline {
public:
    // Compute the budget ONCE at fill start (button press, PRD §6.2) and
    // snapshot the log inputs (recovery, table result — PRD §3 log line).
    void beginFill(const RecoveryClock& clock, uint32_t nowMs) {
        freshBoot_ = !clock.hasHistory();
        recoveryMsAtStart_ = freshBoot_ ? 0 : clock.recoveryMs(nowMs);
        tableMinutes_ = freshBoot_
                            ? FRESH_BOOT_BACKSTOP_MIN
                            : backstopMinutes(recoveryMsAtStart_ / 60000u);
        budgetMs_ = (uint32_t)tableMinutes_ * 60000u;
        anchored_ = false;  // valve not open yet — baseline settle comes first
    }

    // Fix the deadline origin at the VALVE-OPEN instant (PRD §6 preamble):
    // the backstop bounds valve-open time, not button-press time.
    void anchor(uint32_t valveOpenedAtMs) {
        anchorMs_ = valveOpenedAtMs;
        anchored_ = true;
    }

    // True once the valve-open budget — including every +5 extension — is
    // used up. False until anchored(): the settle window runs no deadline.
    // deadlineMs (anchor + budget) wraps naturally; deadlinePassed
    // (P1.M1.T2.S1) is the rollover-safe comparison.
    bool expired(uint32_t nowMs) const {
        return anchored_ && deadlinePassed(nowMs, anchorMs_ + budgetMs_);
    }

    // Long-press +5 (PRD §5.1): stacks, repeatable, ACTIVE fill only.
    // Valid before or after anchor() — the budget grows either way and the
    // deadline (anchor + budget) follows.
    void extend5() { budgetMs_ += (uint32_t)PLUS5_MIN * 60000u; }

    // ---- log inputs for the backstop computation line (PRD §3) ----
    bool freshBoot() const { return freshBoot_; }
    uint32_t recoveryMsAtStart() const { return recoveryMsAtStart_; }
    uint32_t tableMinutes() const { return tableMinutes_; }        // grant at fill start
    uint32_t budgetMinutes() const { return budgetMs_ / 60000u; }  // incl. +5 stacks
    uint32_t deadlineMs() const { return anchorMs_ + budgetMs_; }  // wraps naturally
    bool anchored() const { return anchored_; }

private:
    bool freshBoot_ = true;
    bool anchored_ = false;
    uint32_t recoveryMsAtStart_ = 0;
    uint32_t tableMinutes_ = 0;
    uint32_t budgetMs_ = 0;
    uint32_t anchorMs_ = 0;
};
// ===== end recovery clock & fill deadline (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====

// ===================== LED pattern engine (P1.M5.T1.S1) =====================
// The external-LED contract of PRD §4/§7 as ONE pure, non-blocking class:
// it turns "what state are we in" into a boolean LED level at each loop
// pass — zero delay(), zero hardware calls, injected clock (seam rules).
// The drivers (P1.M5.T2.S1 GP14, P1.M5.T3.S1 WS2812 mirror) just write
// tick()'s return; the fill engine (P1.M6.T1.S2) is the ONLY setMode
// caller (item contract: modes are state, not user input).
//
// Pattern vocabulary (all cadences are config.h §8 constants, by name):
//   Off        IDLE — always off.
//   Solid      FILLING / CONFIRM — always on (PRD §4 "solid on (continues)").
//   DonePulse  DONE — 3x (on 200 / off 200) then off 1000, repeat, FOREVER
//              until acknowledged (no timeout, PRD §11).
//   FaultBlink(n) FAULT — n x (on 250 / off 250) then off 2000, repeat; n is
//              the fault code 1..3 (PRD §7 table). Latched like the fault.
//   ack overlay one-shot transient (PRD §4/§5.1): on a +5 long-press during
//              FILLING, 3x (on 100 / off 100) = exactly 600 ms, then the
//              base pattern resumes EXACTLY where its phase is — the overlay
//              only masks the output, it never disturbs the base phase, the
//              valve, or the fill timing.
//
// Phase model: the active pattern is a PURE FUNCTION of elapsedMs(nowMs,
// phaseStartMs_) % cycleLength — wrap-safe by construction (P1.M1.T2.S1;
// true for any mode age < 2^32 ms ≈ 49.7 days). setMode() releases the
// anchor; the NEXT tick() re-stamps it, so a mode change mid-pattern resets
// the phase cleanly with no timestamp parameter (setMode runs inside the
// engine step, tick runs in the same loop pass — sub-ms skew, invisible at
// 100 ms cadences). Segment boundaries take effect AT the boundary instant
// (house style, cf. deadlinePassed/readingDue): on-windows are [k*P, k*P+ON).
//
// Caller contract (P1.M5.T2.S1 / P1.M5.T3.S1, driven once per loop pass by
// the LED stage; nowMs non-decreasing, same clock domain as all services):
//   digitalWrite(PIN_LED, led.tick(nowMs));        // external green LED
//   mirror: color from led.mode(), level from tick() // WS2812 (T3.S1)
// The fill engine (P1.M6.T1.S2): setMode(Off/Solid/DonePulse), FAULT ->
// setMode(FaultBlink, code); +5 long-press during FILLING -> triggerAck().
class LedPatternEngine {
public:
    // Pattern vocabulary (PRD §4). Values are stable for logging/mirroring.
    enum Mode : uint8_t { Off = 0, Solid = 1, DonePulse = 2, FaultBlink = 3 };

    // PRD §4 transient ack: "blinks the LED 3x quickly". The count is a task
    // contract, NOT a §8 tunable (only ACK_BLINK_MS is config) — precedent:
    // BaselineCapture::READINGS_PER_SENSOR.
    static constexpr uint8_t ACK_BLINK_COUNT = 3;

    LedPatternEngine()
        : mode_(Mode::Off), faultGroups_(1),
          phaseStartMs_(0), phaseAnchored_(false),
          ackStartMs_(0), ackAnchored_(false), ackActive_(false) {}

    // Select the pattern. FaultBlink carries its group count n = fault code
    // (1..3, PRD §7 table); every other mode ignores it. n == 0 clamps to 1
    // (an empty group would read as "off forever" — the fail-safe direction
    // for a latched fault is to blink, never to look dead). Releases the
    // phase anchor: the next tick() stamps phaseStartMs_ and the new pattern
    // starts from its t=0 (LED on for every blink pattern).
    void setMode(Mode mode, uint8_t faultGroups = 1) {
        mode_ = mode;
        faultGroups_ = (faultGroups == 0) ? 1 : faultGroups;
        phaseAnchored_ = false;
    }

    // Current pattern (WS2812 color map input, P1.M5.T3.S1: Off -> off,
    // Solid -> blue, DonePulse -> green, FaultBlink -> red, PRD §4).
    Mode mode() const { return mode_; }

    // Group count of the active FaultBlink (fault code 1..3).
    uint8_t faultGroups() const { return faultGroups_; }

    // Start (or restart) the one-shot +5 ack overlay. Strictly time-bounded:
    // exactly ACK_BLINK_COUNT blinks at ACK_BLINK_MS cadence, then the base
    // pattern shows again. NOT cancelled by setMode — a state change during
    // the 600 ms simply shows through afterwards.
    void triggerAck() {
        ackActive_ = true;
        ackAnchored_ = false;
    }

    // True while the ack overlay is still masking the base pattern.
    bool ackActive() const { return ackActive_; }

    // Advance to nowMs, return the LED level for this instant. One call per
    // loop pass. Lazily anchors the pattern phase (and the ack window) at
    // the first tick after setMode/triggerAck — the anchor is the loop's
    // nowMs snapshot, never millis() here (seam rule).
    bool tick(uint32_t nowMs) {
        if (!phaseAnchored_) {
            phaseStartMs_ = nowMs;
            phaseAnchored_ = true;
        }
        if (ackActive_) {
            if (!ackAnchored_) {
                ackStartMs_ = nowMs;
                ackAnchored_ = true;
            }
            const uint32_t t = elapsedMs(nowMs, ackStartMs_);
            if (t < ackSpanMs()) {
                return (t % (2u * ACK_BLINK_MS)) < ACK_BLINK_MS;
            }
            ackActive_ = false;  // one-shot complete — fall through to base
        }
        return baseOn(nowMs);
    }

private:
    // Base pattern level at nowMs. Elapsed-since-anchor mod cycle: the
    // pattern repeats forever (DONE has no timeout, PRD §11; FAULT latches,
    // PRD §7). Unsigned % on a true elapsed span is exact and wrap-safe.
    bool baseOn(uint32_t nowMs) const {
        switch (mode_) {
            case Mode::Solid:
                return true;
            case Mode::DonePulse: {
                // cycle: 3x(200 on/200 off)=1200, then 1000 off -> 2200.
                const uint32_t period =
                    DONE_PULSE_ON_MS + DONE_PULSE_OFF_MS;
                const uint32_t groups =
                    (uint32_t)DONE_PULSE_COUNT * period;
                const uint32_t t =
                    elapsedMs(nowMs, phaseStartMs_) % (groups + DONE_GAP_MS);
                if (t >= groups) return false;  // the 1000 ms gap
                return (t % period) < DONE_PULSE_ON_MS;
            }
            case Mode::FaultBlink: {
                // cycle: n x(250 on/250 off)=n*500, then 2000 off.
                const uint32_t period = 2u * FAULT_BLINK_MS;
                const uint32_t groups = (uint32_t)faultGroups_ * period;
                const uint32_t t =
                    elapsedMs(nowMs, phaseStartMs_) % (groups + FAULT_PAUSE_MS);
                if (t >= groups) return false;  // the 2000 ms pause
                return (t % period) < FAULT_BLINK_MS;
            }
            case Mode::Off:
            default:
                return false;
        }
    }

    // Total ack overlay width: 3 blinks x (100 on + 100 off) = 600 ms.
    static constexpr uint32_t ackSpanMs() {
        return (uint32_t)ACK_BLINK_COUNT * 2u * ACK_BLINK_MS;
    }

    Mode     mode_;          // active pattern (set only by the fill engine)
    uint8_t  faultGroups_;   // n for FaultBlink (fault code 1..3, PRD §7)
    uint32_t phaseStartMs_;  // pattern phase anchor (stamped by tick)
    bool     phaseAnchored_; // false after setMode — next tick re-stamps
    uint32_t ackStartMs_;    // ack overlay anchor (stamped by tick)
    bool     ackAnchored_;   // false after triggerAck — next tick re-stamps
    bool     ackActive_;     // overlay masking the base pattern right now
};
// ===== end LED pattern engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====

// ===================== Fill-state transition core (P1.M6.T1.S1) =====================
// PRD §5.1's BOOT-button table as ONE pure, total, stateless function over
// the five fill states — plus the §10 valve invariant made structural.
// This core is the BUTTON half of the fill engine only: sensor/timer-driven
// transitions (baseline -> valve -> trip -> CONFIRM -> DONE, fault latching)
// belong to the lifecycle orchestrator (P1.M6.T1.S2 / P1.M6.T1.S3), which
// OWNS the state variable and calls fillApplyButton() on every classified
// press. Long/short classification (40 ms debounce, 1000 ms threshold) is
// entirely upstream in ButtonClassifier (P1.M2.T1.S1) — this table is
// time-free by construction.
//
// Model: STATELESS by design (contrast ButtonClassifier/LedPatternEngine).
// The caller's FillState variable IS the machine; the function is the pure
// table (State, Event) -> (State', Outputs). Every §5.1 cell is therefore
// directly seedable in a test, and P1.M6.T1.S2 drives its lifecycle
// transitions by the same caller-owned-state pattern without fighting a
// hidden member. Boot/reset (PRD §5.3, §10) is simply:
//     FillState st = FillState::Idle;   // the ONLY legal initial value
// — no path in this core ever re-enters Filling except the human
// short-press cell below (no auto-open from any source).
//
// Outputs are two LEVELS + two EDGES:
//   valveOpen  LEVEL == (result state == Filling). §10 made structural: the
//              commanded valve level is high ONLY in FILLING. The PHYSICAL
//              open instant is still gated by P1.M6.T1.S2's baseline stage
//              (PRD §7.1/§6: the deadline anchors at valve OPEN, ~2 s settle
//              after the press) — integrators must NOT wire the pin straight
//              to this flag at press time.
//   led        LEVEL == PRD §4 pattern for the result state (Idle Off,
//              Filling/Confirm Solid — §4 "solid on (continues)", Done
//              DonePulse, Fault FaultBlink). FaultBlink's group count n
//              (fault code, PRD §7) is carried by the fault latcher
//              (P1.M6.T1.S3): no button event ever enters FAULT.
//   plus5Applied  EDGE: FILLING long-press, one flag per release (PRD §5.1
//              "+5 min ... stacks; repeatable"). The orchestrator maps it to
//              FillDeadline::extend5() (P1.M4.T2.S1) + LedPatternEngine::
//              triggerAck() (PRD §4 ack transient) — this core calls neither.
//   acknowledged  EDGE: DONE short OR long press (PRD §5.1 "also
//              acknowledges"). Light-off IS the state change (led -> Off);
//              the flag exists for the log line.
// Event::None (the classifier's steady-state return) is a total-function
// no-op: same state, that state's levels, no edges — callers may route it
// every pass; both real actions still fire exactly once per classified
// release upstream (§5.1 "acts on RELEASE").

enum class FillState : uint8_t {
    Idle    = 0,   // powered, waiting for BOOT press (PRD §4)
    Filling = 1,   // fill authorized: valve level high, timer running
    Confirm = 2,   // first sensor tripped, valve closed, confirm window running
    Done    = 3,   // fill complete: attention pulse until acknowledged
    Fault   = 4,   // latched fault (blink codes, PRD §7)
};

struct FillOutputs {
    bool valveOpen;               // level: commanded valve state (FILLING only)
    LedPatternEngine::Mode led;   // level: PRD §4 pattern for the result state
    bool plus5Applied;            // edge: FILLING long-press (+5, stacks)
    bool acknowledged;            // edge: DONE press (short or long)
};

// Level half of the outputs as a pure function of a state — the single
// source of the state -> valve/LED mapping, reused by P1.M6.T1.S2 when its
// lifecycle transitions (CONFIRM/DONE/FAULT entry) need the same levels.
FillOutputs fillLevelOutputs(FillState s);

// The §5.1 table. Mutates `state` to the cell's result state and returns
// that result's outputs (edge flags set only by the acting cells).
FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev);

FillOutputs fillLevelOutputs(FillState s) {
    FillOutputs o{};  // zero-init: valveOpen=false is the fail-safe default (PRD §10)
    switch (s) {
        case FillState::Filling: o.valveOpen = true; o.led = LedPatternEngine::Mode::Solid; break;
        case FillState::Confirm: o.led = LedPatternEngine::Mode::Solid; break;      // §4: "solid on (continues)"
        case FillState::Done:    o.led = LedPatternEngine::Mode::DonePulse; break;
        case FillState::Fault:   o.led = LedPatternEngine::Mode::FaultBlink; break; // group count: P1.M6.T1.S3
        case FillState::Idle:    break;                                            // defaults: off, closed
    }
    return o;
}

FillOutputs fillApplyButton(FillState& state, ButtonClassifier::Event ev) {
    bool plus5 = false;
    bool ack = false;
    switch (state) {
        case FillState::Idle:
            // Short: START FILL -> FILLING — the ONLY entry into FILLING
            // anywhere in the button table (PRD §10: human BOOT press).
            // Long: nothing.
            if (ev == ButtonClassifier::Event::ShortPress) {
                state = FillState::Filling;
            }
            break;
        case FillState::Filling:
            // Short: ABORT — valve level drops with the state change.
            // Long: +5 to THIS fill's backstop; stays FILLING (valve level
            // unchanged); stacks because every long release re-fires.
            if (ev == ButtonClassifier::Event::ShortPress) {
                state = FillState::Idle;
            } else if (ev == ButtonClassifier::Event::LongPress) {
                plus5 = true;
            }
            break;
        case FillState::Confirm:
            break;  // ignored (both) — PRD §5.1
        case FillState::Done:
            // Short: ACKNOWLEDGE -> IDLE. Long: "also acknowledges".
            if (ev != ButtonClassifier::Event::None) {
                state = FillState::Idle;
                ack = true;
            }
            break;
        case FillState::Fault:
            break;  // ignored (both) — latched until reset (PRD §7)
    }
    FillOutputs out = fillLevelOutputs(state);
    out.plus5Applied = plus5;
    out.acknowledged = ack;
    return out;
}
// ===== end fill-state transition core (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====

// ===================== Fill lifecycle engine (P1.M6.T1.S2) =====================
// PRD §7's fill lifecycle as ONE pure step function: composes the §5.1
// transition core (P1.M6.T1.S1) with BaselineCapture (P1.M3.T2.S1),
// TripDetectors + FalseTripGuards (P1.M3.T3.S1/S2), RecoveryClock +
// FillDeadline (P1.M4.T2.S1) and the LED mode map (P1.M5.T1.S1 via
// fillLevelOutputs) into the full sequence
//   IDLE --press--> settle/baseline (valve CLOSED — settle runs INSIDE
//   FILLING, arch §3.1) -> baseline Ok -> valve OPEN + deadline anchored
//   at the OPEN instant (§6 preamble bounds valve-open time) -> first
//   trip: valve closed FIRST, CONFIRM (§7.4/§7.9) -> second trip within
//   CONFIRM_WINDOW_S: DONE, recovery reference = the valve-CLOSE instant
//   (§7.8) — with every alternate ending (abort §5.2, baseline/guard/
//   confirm/backstop fault §7) driving the valve LOW as its FIRST action.
//
// SEAM: step() takes the injected clock, the classified button event and
// the averaged pair "when due" (the loop owns the ReadingAggregator,
// P1.M3.T1.S2 — the engine never samples). It RETURNS levels + edges + an
// ORDERED action log; it never touches pins, Serial or LedPatternEngine
// state. P1.M6.T2.S1 routes them: valve write; s_led.setMode(o.led,
// o.faultCode) ONLY ON CHANGE (setMode re-anchors the pattern phase —
// calling it every pass would freeze every blink); o.ackBlink ->
// triggerAck(); one logLine per log event.
//
// Sub-phase model: FILLING spans the WHOLE attempt, press to end (arch §3.1):
//   state==Filling && !valveOpen_ -> settle/baseline (valve closed)
//   state==Filling &&  valveOpen_ -> valve open: guards, trip, backstop —
//                                   evaluated EVERY pass, in that order
//   state==Confirm                -> valve closed: second trip vs the
//                                   CONFIRM_WINDOW_S timeout (inclusive at
//                                   exactly 12.000 s; a second trip latched
//                                   in the same pass wins)
// Guards and backstop run ONLY while the valve is open: entering CONFIRM
// proves the first trip passed fill time >= TRIP_MIN_ELAPSED_S (else
// EarlyTripFault fired first — guards are evaluated before the trip
// check), so both guard windows are provably closed throughout CONFIRM;
// and the backstop bounds VALVE-OPEN time, which is already over.
//
// Recovery references (arch §3.3, §6.1): DONE records the valve-CLOSE
// (first-trip) instant; abort and every fault record the attempt-end
// instant. Every fill computes its backstop ONCE at press
// (FillDeadline::beginFill) and anchors at OPEN (FillDeadline::anchor) —
// the 2 s settle never eats the budget, and a settle abort never anchored.
//
// §7.9 ordering law (uniform): ANY step that leaves Filling/Confirm logs
// ValveClosed as logs[0] — including the baseline fault (valve never
// opened), the settle abort, and Confirm exits (valve already closed):
// driving an already-LOW pin LOW again IS the invariant. The invariant
// suite (P1.M7.T1.S1) asserts this ordering.

// Worst single step = 5 events (the same-step double trip: ValveClosed,
// Trip, ValveClosed, ConfirmPass, Done). Block-scope so FillEngineOutputs
// can size its array (class not yet defined at that point).
constexpr uint8_t FILL_LOG_CAPACITY = 5;

// One ordered engine action. T2.S1 formats ONE logLine per event; the
// ORDER is normative (logs[0] == ValveClosed on any fill-ending step).
struct FillLogEvent {
    enum class Kind : uint8_t {
        ValveClosed     = 1,   // fill end, ANY cause — ALWAYS first on its step (§7.9)
        FillStart       = 2,   // a: backstop table minutes; b: recovery ms at press (0 = fresh boot)
        BaselineOk      = 3,   // a/b: dry baselines, counts
        BaselineFault   = 4,   // sensor: failing; a: fault code (sensor+1)
        ValveOpen       = 5,   // a: budget minutes at open (incl. pre-open +5s)
        Trip            = 6,   // sensor: first-trip sensor; a: fill time ms
        ConfirmPass     = 7,   // a: confirm duration ms
        Done            = 8,   // a: attempt ms (press->valve close); b: valve-open ms
        Abort           = 9,   // a: attempt duration ms
        EarlyWetFault   = 10,  // sensor: shorted/wet-too-early; a: fault code
        EarlyTripFault  = 11,  // sensor: false trip; a: fault code
        BackstopExpired = 12,  // a: fault code 3
        ConfirmTimeout  = 13,  // sensor: never tripped in window; a: fault code
        Plus5           = 14,  // a: new total budget minutes
        Ack             = 15,  // DONE acknowledged -> IDLE
    };
    Kind     kind;    // what happened
    uint8_t  sensor;  // 0 == sensor 1 (fault code 1), 1 == sensor 2 (fault code 2); 0 when n/a
    uint32_t a;       // payload 1 (ms / counts / minutes / code — per Kind)
    uint32_t b;       // payload 2
};

// One step()'s entire observable effect. Levels are safe to route every
// pass; edges are true for exactly the step that caused them.
struct FillEngineOutputs {
    bool valveOpen;              // LEVEL: commanded pin — HIGH only in FILLING's valve sub-phase
    LedPatternEngine::Mode led;  // LEVEL: PRD §4 pattern for the current state
    uint8_t faultCode;           // LEVEL: 0, or 1..3 while in FAULT (blink groups: P1.M6.T1.S3)
    bool plus5Applied;           // EDGE: +5 applied this step (extend5 already done internally)
    bool acknowledged;           // EDGE: DONE acknowledged this step
    bool ackBlink;               // EDGE: route to LedPatternEngine::triggerAck() this step
    uint8_t logCount;            // ordered actions; logs[0] is ValveClosed on any fill end
    FillLogEvent logs[FILL_LOG_CAPACITY];
};

class FillEngine {
public:
    FillEngine()
        : state_(FillState::Idle), valveOpen_(false),
          valveOpenedAtMs_(0), valveClosedAtMs_(0), confirmStartMs_(0),
          fillStartMs_(0), confirmFirst_(0), faultCode_(0), logCount_(0) {}

    // Advance the lifecycle one loop pass.
    //   nowMs      — injected clock, same domain as every service (seam rule)
    //   ev         — classified BOOT event (P1.M2.T1.S1); None on most passes
    //   readingDue — true iff the loop's aggregator emitted a fresh averaged
    //                pair THIS pass; gates baseline/detector feeding
    //   avg1/avg2  — the latest EMITTED pair (loop's s_adcAvg1/2): the
    //                guards want the freshest levels even between emissions
    FillEngineOutputs step(uint32_t nowMs, ButtonClassifier::Event ev,
                           bool readingDue, uint16_t avg1, uint16_t avg2) {
        FillEngineOutputs out{};  // zero-init: valve closed, Off, no edges (§10 fail-safe)
        logCount_ = 0;

        // ---- 1. Button table (§5.1, P1.M6.T1.S1) -------------------------
        const FillState before = state_;
        bool plus5Pending = false;  // Plus5 log deferred to post-phase (§7.9)
        const FillOutputs btn = fillApplyButton(state_, ev);
        if (btn.plus5Applied) {  // FILLING long-press: +5 to THIS fill, stacks
            deadline_.extend5();
            out.plus5Applied = true;
            out.ackBlink = true;  // -> triggerAck() (PRD §4 transient)
            plus5Pending = true;  // logged AFTER phase work — see block 6½
        }
        if (btn.acknowledged) {  // DONE press: light off via the Idle levels
            out.acknowledged = true;
            log(FillLogEvent::Kind::Ack, 0, 0, 0);
        }

        // ---- 2. Fill start: the ONLY entry into FILLING (Idle+ShortPress)
        if (before == FillState::Idle && state_ == FillState::Filling) {
            fillStartMs_ = nowMs;
            deadline_.beginFill(clock_, nowMs);  // backstop ONCE, at press (§6.2)
            baseline_.begin(nowMs);              // settle+collect; valve stays closed
            valveOpen_ = false;                  // settle runs inside FILLING (arch §3.1)
            faultCode_ = 0;
            log(FillLogEvent::Kind::FillStart, 0, deadline_.tableMinutes(),
                deadline_.recoveryMsAtStart());
        }

        // ---- 3. Abort: Filling+ShortPress -> Idle (§5.2 flow 1) -----------
        if (before == FillState::Filling && state_ == FillState::Idle) {
            valveOpen_ = false;  // §7.9: LOW FIRST — uniform, even if never opened
            log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
            clock_.recordFillEnd(nowMs);  // §6.1: recovery restarts at the abort instant
            log(FillLogEvent::Kind::Abort, 0, elapsedMs(nowMs, fillStartMs_), 0);
        }

        // ---- 4. Route the reading stream — EXACTLY ONCE per step ----------
        // (After the button blocks so a just-started fill owns the stream and
        // a just-aborted one drops it; before the phase work so the reading
        // that completes the baseline/trip is seen by blocks 5/6 same step.)
        if (readingDue) {
            if (state_ == FillState::Filling && !valveOpen_) {
                baseline_.addReading(nowMs, avg1, avg2);  // settle drops pre-2 s pairs
            } else if (state_ == FillState::Filling || state_ == FillState::Confirm) {
                trip1_.feed(avg1);  // a latched detector ignores feeds (P1.M3.T3.S1)
                trip2_.feed(avg2);
            }
        }

        // ---- 5. FILLING work ---------------------------------------------
        if (state_ == FillState::Filling && !valveOpen_) {
            // settle/baseline sub-phase: the §7.1 plausibility gate
            if (baseline_.done()) {
                const bool bad1 = baseline_.verdict(0) != BaselineCapture::Verdict::Ok;
                const bool bad2 = baseline_.verdict(1) != BaselineCapture::Verdict::Ok;
                if (bad1 || bad2) {
                    // Refuse to open the valve (§7.1). Sensor 1 wins ties
                    // (house precedence, cf. FalseTripGuards).
                    endFillFault(nowMs, FillLogEvent::Kind::BaselineFault, bad1 ? 0 : 1);
                } else {
                    trip1_.begin(baseline_.baseline(0));  // VALIDATED baselines only
                    trip2_.begin(baseline_.baseline(1));  // (TripDetector::begin contract)
                    guards_.begin(baseline_.baseline(0), baseline_.baseline(1));
                    valveOpen_ = true;         // the ONLY valve-open site in the firmware
                    valveOpenedAtMs_ = nowMs;
                    deadline_.anchor(nowMs);   // §6 preamble: bounds valve-open time
                    log(FillLogEvent::Kind::BaselineOk, 0,
                        baseline_.baseline(0), baseline_.baseline(1));
                    log(FillLogEvent::Kind::ValveOpen, 0, deadline_.budgetMinutes(), 0);
                }
            }
        } else if (state_ == FillState::Filling) {
            // valve-open sub-phase — EVERY pass, guards -> trip -> backstop:
            // a false trip must fault (not CONFIRM); a real trip outranks
            // the backstop.
            const GuardVerdict v = guards_.evaluate(nowMs, valveOpenedAtMs_,
                                                    trip1_.tripped(), trip2_.tripped(),
                                                    avg1, avg2);
            if (v.kind != GuardVerdict::Ok) {
                endFillFault(nowMs,
                             v.kind == GuardVerdict::EarlyWetFault
                                 ? FillLogEvent::Kind::EarlyWetFault
                                 : FillLogEvent::Kind::EarlyTripFault,
                             v.sensor);
            } else if (trip1_.tripped() || trip2_.tripped()) {
                // First trip (§7.4): valve closed FIRST, then CONFIRM.
                const uint8_t first = trip1_.tripped() ? 0 : 1;
                valveOpen_ = false;  // §7.9: LOW FIRST — before the state flip
                log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
                log(FillLogEvent::Kind::Trip, first,
                    elapsedMs(nowMs, valveOpenedAtMs_), 0);
                state_ = FillState::Confirm;
                confirmFirst_ = first;
                confirmStartMs_ = nowMs;
                valveClosedAtMs_ = nowMs;  // §7.8: DONE's recovery reference
            } else if (deadline_.expired(nowMs)) {
                endFillFault(nowMs, FillLogEvent::Kind::BackstopExpired, 0);  // code 3
            }
        }

        // ---- 6. CONFIRM work (also on the step it was entered — a -------
        // ---- same-step second trip completes the fill immediately) ------
        if (state_ == FillState::Confirm) {
            const uint8_t other = (uint8_t)(1u - confirmFirst_);
            const bool otherTripped = (other == 0) ? trip1_.tripped() : trip2_.tripped();
            if (otherTripped) {
                // §7.8: DONE; recovery reference = the valve-CLOSE instant —
                // a PAST timestamp; elapsedMs arithmetic handles it exactly.
                log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);  // uniform §7.9 exit ordering
                state_ = FillState::Done;
                clock_.recordFillEnd(valveClosedAtMs_);
                log(FillLogEvent::Kind::ConfirmPass, other,
                    elapsedMs(nowMs, confirmStartMs_), 0);
                log(FillLogEvent::Kind::Done, 0,
                    elapsedMs(valveClosedAtMs_, fillStartMs_),
                    elapsedMs(valveClosedAtMs_, valveOpenedAtMs_));
            } else if (elapsedMs(nowMs, confirmStartMs_) >=
                       (uint32_t)CONFIRM_WINDOW_S * 1000u) {  // widen BEFORE x1000;
                // inclusive at exactly 12.000 s (deadlinePassed house style)
                // §7.5: the other sensor never tripped — fault IT.
                endFillFault(nowMs, FillLogEvent::Kind::ConfirmTimeout, other);
            }
        }

        // ---- 6½. Deferred Plus5 log (§7.9 ordering law) ------------------
        // The log is a PRIORITY-ordered action stream (logs[0] == ValveClosed
        // on ANY fill-ending step — the loop must write the valve first), so
        // the Plus5 entry is appended after the phase work: a same-step
        // fault/trip keeps ValveClosed at logs[0]. extend5() itself already
        // ran in block 1 (valid before or after the deadline anchor).
        if (plus5Pending) log(FillLogEvent::Kind::Plus5, 0, deadline_.budgetMinutes(), 0);

        // ---- 7. Levels from the (possibly new) state ---------------------
        out.valveOpen = valveOpen_ && state_ == FillState::Filling;
        out.led = fillLevelOutputs(state_).led;
        out.faultCode = (state_ == FillState::Fault) ? faultCode_ : 0;
        out.logCount = logCount_;
        for (uint8_t i = 0; i < logCount_; ++i) out.logs[i] = logs_[i];
        return out;
    }

    // ---- observability (tests, P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) ----
    FillState state() const { return state_; }
    uint8_t faultCode() const { return faultCode_; }   // 0, or 1..3 (§7 table)
    bool valveOpen() const { return valveOpen_; }
    uint32_t valveOpenedAtMs() const { return valveOpenedAtMs_; }
    const FillDeadline& deadline() const { return deadline_; }
    const RecoveryClock& clock() const { return clock_; }

private:
    // Append one ordered action. Drop-proof by design (worst step = 5);
    // the guard is defensive only.
    void log(FillLogEvent::Kind kind, uint8_t sensor, uint32_t a, uint32_t b) {
        if (logCount_ >= FILL_LOG_CAPACITY) return;
        logs_[logCount_].kind = kind;
        logs_[logCount_].sensor = sensor;
        logs_[logCount_].a = a;
        logs_[logCount_].b = b;
        ++logCount_;
    }

    // Every fault exit in one place: valve LOW FIRST (§7.9 — uniform even
    // when the valve never opened or is already closed), latch the code,
    // recovery reference at the attempt-end instant (arch §3.3).
    void endFillFault(uint32_t nowMs, FillLogEvent::Kind why, uint8_t sensor) {
        valveOpen_ = false;  // §7.9: LOW FIRST — before the state flip
        log(FillLogEvent::Kind::ValveClosed, 0, 0, 0);
        state_ = FillState::Fault;
        faultCode_ = (why == FillLogEvent::Kind::BackstopExpired)
                         ? 3                        // "timer, no trip" (§7 table)
                         : (uint8_t)(sensor + 1);   // sensor fault code
        clock_.recordFillEnd(nowMs);
        log(why, sensor, faultCode_, 0);
    }

    FillState       state_;            // the §5.1 machine's state (S1 core drives it)
    bool            valveOpen_;        // valve sub-phase level: settle=false, open=true
    uint32_t        valveOpenedAtMs_;  // §6 anchor instant (guards' fill-time origin)
    uint32_t        valveClosedAtMs_;  // first-trip instant — DONE's recovery ref (§7.8)
    uint32_t        confirmStartMs_;   // CONFIRM window origin (== valve close)
    uint32_t        fillStartMs_;      // press-accept instant (attempt duration origin)
    uint8_t         confirmFirst_;     // sensor that tripped first (0/1)
    uint8_t         faultCode_;        // 0, or 1..3 latched at Fault entry
    BaselineCapture baseline_;
    TripDetector    trip1_;            // sensor 1 (index 0, fault code 1)
    TripDetector    trip2_;            // sensor 2 (index 1, fault code 2)
    FalseTripGuards guards_;
    RecoveryClock   clock_;
    FillDeadline    deadline_;
    FillLogEvent    logs_[FILL_LOG_CAPACITY];
    uint8_t         logCount_;
};
// ===== end fill lifecycle engine (consumed by P1.M6.T2.S1, P1.M6.T1.S3, P1.M7.T1.S1) =====

// ---- #ifdef ARDUINO REGION: hardware glue ----
#ifdef ARDUINO

#include <hardware/watchdog.h>   // pico-sdk watchdog, shipped inside arduino-pico (verified)
#include <hardware/pio.h>       // pico-sdk PIO — WS2812 driver engine (P1.M5.T3.S1)
#include <hardware/clocks.h>    // clock_get_hz(clk_sys) for the PIO clkdiv
#include "PIOProgram.h"         // arduino-pico core helper: claim SM + load program

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

// ---- External LED driver state (P1.M5.T2.S1) ----
// Hardware side of LED signaling (PRD §4): the external green LED on GP14 is
// AUTHORITATIVE — the onboard WS2812 (P1.M5.T3.S1) only mirrors it. Wiring
// (PRD §2.1): GP14 -> 330R -> LED anode, cathode -> GND, so HIGH = on; the
// pin sources ~8 mA at 3.3 V, ample within GPIO drive strength. setup() already
// did pinMode(PIN_LED, OUTPUT) + LOW (P1.M1.T1.S2 skeleton — LED dark at boot);
// the engine defaults to Off, so IDLE stays dark until the fill engine
// (P1.M6.T1.S2, routed by P1.M6.T2.S1) selects a mode. This driver is
// mode-agnostic: it only writes tick()'s level each pass — pattern semantics
// (Solid through CONFIRM, DonePulse forever until acknowledged per PRD §11,
// FaultBlink groups per §7) all live in the pure engine, native-tested by
// P1.M5.T1.S1. Shared instance: the WS2812 mirror reads mode()/ledOn from the
// SAME engine — one tick per pass, never two.
static LedPatternEngine s_led;  // pattern engine instance; RAM state only (PRD §3)
// ---- end External LED driver state (used by loop, shared with P1.M5.T3.S1) ----

// ---- WS2812 onboard mirror state (P1.M5.T3.S1) ----
// Hardware side of the PRD §4 mirror column: the onboard WS2812 on GP16
// (variant constant PIN_NEOPIXEL — firmware-only pin, PRD §2.1; powered from
// 3V3, hence the RGB_BRIGHTNESS <= 60/255 LDO cap, PRD §2.2) duplicates the
// AUTHORITATIVE external green LED with color coding: IDLE off (black),
// FILLING/CONFIRM blue solid, DONE green in the done-pulse pattern, FAULT red
// in the blink-code pattern. Color comes from s_led.mode(), level from the
// SAME ledOn the GP14 driver wrote (one engine tick per pass — frozen
// P1.M5.T1.S1 caller contract); the ack overlay shows through as a blue blink
// because the mode stays Solid while tick() masks it.
//
// Driver: NO external library (none is bundled with the installed core —
// verified; research notes §1). Instead this embeds the pico-sdk's canonical
// WS2812 PIO program (pico-sdk/src/rp2_common/pico_status_led/ws2812.pio,
// shipped inside arduino-pico), pre-assembled with the installed pioasm —
// the same pattern as the core's own tone2.pio.h — and loads it through the
// core's PIOProgram helper (the core's Tone/SerialPIO loader). Builds in
// Arduino IDE with just the core (PRD §3).
//
// Update discipline: the pixel is written ONLY when the packed GRB word
// changes (s_wsLastGrb). WS2812 latches on data-line idle > 50 us and holds
// color indefinitely, so unchanged colors are never re-sent; pattern-driven
// changes are >= ACK_BLINK_MS (100 ms) apart — 3 orders of magnitude of
// latch margin, no waits. pio_sm_put_blocking cannot stall the loop: the
// joined TX FIFO (8 deep) drains a 24-bit frame in ~30 us while pushes are
// >= 100 ms apart (PRD §10 non-blocking invariant).
constexpr uint32_t WS2812_BITRATE_HZ = 800000;  // WS2812 protocol bit rate — NOT a §8 tunable (task contract; precedent ADC_SAMPLE_PACE_MS)

// ---- embedded ws2812.pio.h — autogenerated by pioasm from pico-sdk's
// ws2812.pio (do not hand-edit; T1/T2/T3 chosen upstream for WS2812/WS2812B/
// SK6812 compatibility) ----
#define ws2812_wrap_target 0
#define ws2812_wrap 3
#define ws2812_pio_version 0

#define ws2812_T1 3
#define ws2812_T2 3
#define ws2812_T3 4

static const uint16_t ws2812_program_instructions[] = {
            //     .wrap_target
    0x6321, //  0: out    x, 1            side 0 [3]
    0x1223, //  1: jmp    !x, 3           side 1 [2]
    0x1200, //  2: jmp    0               side 1 [2]
    0xa242, //  3: nop                    side 0 [2]
            //     .wrap
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
    .pio_version = ws2812_pio_version,
};

static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812_wrap_target, offset + ws2812_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    float div = clock_get_hz(clk_sys) / (freq * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
// ---- end embedded ws2812.pio.h ----

static PIOProgram s_wsPgm(&ws2812_program);  // claims SM + loads program at prepare()
static PIO        s_wsPio = pio0;            // PIO block prepare() selected (unused until then)
static int        s_wsSm  = -1;              // state machine index (-1 until prepared)
static int        s_wsOff = -1;              // program load offset (-1 until prepared)
static bool       s_wsOk  = false;           // mirror live (init succeeded AND config enabled)
static uint32_t   s_wsLastGrb = 0xFFFFFFFFu; // last pushed GRB word (init value forces first write)

// Per-pass mirror update. color = mode() per PRD §4 mirror column; level =
// the SAME ledOn written to GP14 (includes the ack overlay: mode stays Solid
// so the blink shows blue — mirror fidelity over color purity). Brightness
// cap by SCALING each channel: channel * RGB_BRIGHTNESS / 255 (the map only
// writes 0 or 255 per channel, so scaled values are exactly 0 or 60). Word
// format GRB MSB-first 24-bit autopull: (g<<24)|(r<<16)|(b<<8) — official
// pico-sdk status_led.c "0xGGRRBB00". Writes only on change (see block
// comment above). Defined BEFORE wsMirrorInit (which calls it): PlatformIO
// compiles this sketch as plain C++ — no Arduino auto-prototype pass.
static void wsMirrorUpdate(LedPatternEngine::Mode mode, bool ledOn) {
    if (!USE_ONBOARD_RGB || !s_wsOk) return;
    uint8_t r = 0, g = 0, b = 0;
    if (ledOn) {
        switch (mode) {
            case LedPatternEngine::Mode::Solid:      b = 255; break;  // FILLING / CONFIRM
            case LedPatternEngine::Mode::DonePulse:  g = 255; break;  // DONE attention pulse
            case LedPatternEngine::Mode::FaultBlink: r = 255; break;  // FAULT blink codes
            case LedPatternEngine::Mode::Off:        break;           // IDLE (ledOn false anyway)
        }
    }
    const uint32_t grb = (((uint32_t)g * RGB_BRIGHTNESS / 255u) << 24)
                       | (((uint32_t)r * RGB_BRIGHTNESS / 255u) << 16)
                       | (((uint32_t)b * RGB_BRIGHTNESS / 255u) << 8);
    if (grb == s_wsLastGrb) return;  // update only when engine output changes
    s_wsLastGrb = grb;
    pio_sm_put_blocking(s_wsPio, (uint)s_wsSm, grb);
}

// setup-time init. Fully gated by USE_ONBOARD_RGB (constexpr bool — a plain
// if, folded away when false; it is not a macro and cannot be #if'd). On PIO
// allocation failure: one diagnostic line, mirror permanently off — the
// external LED (authoritative, PRD §4) is unaffected: fail-safe direction.
static void wsMirrorInit() {
    if (!USE_ONBOARD_RGB) return;
    if (!s_wsPgm.prepare(&s_wsPio, &s_wsSm, &s_wsOff, PIN_NEOPIXEL, 1)) {
        logLine("RGB mirror init FAILED (no PIO SM)");  // mirror off; external LED unaffected
        return;
    }
    ws2812_program_init(s_wsPio, (uint)s_wsSm, (uint)s_wsOff, PIN_NEOPIXEL,
                        (float)WS2812_BITRATE_HZ, false);  // RGB (not RGBW), 800 kHz
    s_wsOk = true;
    wsMirrorUpdate(LedPatternEngine::Mode::Off, false);  // deterministic boot-dark (PRD §4 IDLE)
}
// ---- end WS2812 onboard mirror state (used by setup/loop, kept for P1.M6.T2.S1/P1.M7.T2.S1) ----

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

  // Onboard WS2812 mirror (P1.M5.T3.S1): GP16, config-gated, brightness-
  // capped; pushes black once so boot is deterministically dark. Must
  // precede watchdog_enable (which stays LAST, arch boot order).
  wsMirrorInit();

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
  bool readingDueThisPass = false;  // fresh averaged pair emitted THIS pass (engine stream gate)
  if (s_adc.readingDue(nowMs)) {
    s_adc.takeReading(nowMs, s_adcAvg1, s_adcAvg2);  // emits + re-anchors cadence
    readingDueThisPass = true;
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
  // ---- External LED drive (P1.M5.T2.S1) — the loop's LED stage ----
  // ONE tick of the pattern engine per pass, ONE pin write (PRD §10: nothing
  // here may block; the engine is a pure function of nowMs). Uses the loop's
  // single nowMs snapshot — never a second millis() read. ledOn is the shared
  // level: the WS2812 mirror (P1.M5.T3.S1) writes its color BESIDE this
  // digitalWrite using the SAME ledOn — never re-tick. The stage sits LAST so
  // the fill engine (P1.M6.T2.S1, routing lands ABOVE this stage) can setMode()
  // and the same pass's write already reflects it — the engine's lazy phase
  // anchor makes same-pass setMode->tick exact (P1.M5.T1.S1 design).
  const bool ledOn = s_led.tick(nowMs);          // advance pattern, get level
  digitalWrite(PIN_LED, ledOn ? HIGH : LOW);     // GP14 -> 330R -> LED -> GND
  wsMirrorUpdate(s_led.mode(), ledOn);           // PRD §4 mirror: color from mode, level from same tick
  // ---- end External LED drive ----
}

#endif  // ARDUINO
