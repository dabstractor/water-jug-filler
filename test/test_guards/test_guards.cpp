// test/test_guards/test_guards.cpp — native contract proof for FalseTripGuards
// (P1.M3.T3.S2). Pins PRD §7.6: any trip accepted before TRIP_MIN_ELAPSED_S
// (60 s) of fill time, or any sensor reading wet during the first
// EARLY_WET_WINDOW_S (5 s), is a false trip / short -> fault that sensor.
// 'Wet' = averaged reading at/below baseline − TRIP_DROP_COUNTS — the SAME
// inclusive predicate as the trip rule (PRD §7.3, P1.M3.T3.S1).
// Window semantics (item-contract pins): trip at 59.9 s -> EarlyTripFault, at
// exactly 60.0 s -> Ok; wet at 4.9 s -> EarlyWetFault, at 5.1 s -> Ok. Both
// windows close AT equality (strict '<') — the exactly-5.000 s case mirrors
// exactly-60.000 s. Fill time = time since valveOpenedAtMs, wrap-safe.
// Worked example: baselines 3400/3400 -> wet thresholds 3050 (3050 itself
// reads wet, inclusive; 3051 reads dry).
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T3S2/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Assert a whole verdict at once (kind + sensor payload).
static void assertVerdict(GuardVerdict::Kind kind, uint8_t sensor, GuardVerdict v) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)kind, (uint8_t)v.kind);
    TEST_ASSERT_EQUAL_UINT8(sensor, v.sensor);
}

// ---- early-trip window (TRIP_MIN_ELAPSED_S = 60 s, closes AT equality) ----

// ITEM CASE: a trip accepted at 59.9 s of fill time is a false trip ->
// EarlyTripFault on sensor 1 (index 0 == fault code 1). Wet window is long
// closed at 59.9 s, so only the trip rule fires.
void test_trip_at_59_9s_is_early_trip_fault_sensor1() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(59900u, 0u, true, false, 3400, 3400));
}

// ITEM CASE: a trip accepted at EXACTLY 60.000 s is legal — the window closes
// at equality (strict '<'). Even BOTH sensors latched is Ok at 60.000 s.
void test_trip_at_exactly_60_0s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(60000u, 0u, true, true, 3400, 3400));
}

// 60.1 s with a wet averaged reading: past the trip window AND past the wet
// window — mid-fill wetness is the debounce's business, not the guards'.
void test_trip_at_60_1s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(60100u, 0u, true, false, 0, 3400));
}

// Sensor 2's early trip names sensor 2 (index 1 == fault code 2).
void test_trip_59_9s_on_sensor2_faults_sensor2() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 1,
                  g.evaluate(59900u, 0u, false, true, 3400, 3400));
}

// Deterministic precedence: both sensors early-tripped -> sensor 1 reported.
void test_both_tripped_early_reports_sensor1_first() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(59900u, 0u, true, true, 3400, 3400));
}

// ---- early-wet window (EARLY_WET_WINDOW_S = 5 s, closes AT equality) ----

// ITEM CASE: a wet reading at 4.9 s (deep wet, 0 counts) faults sensor 1
// before the debounce could even latch a trip.
void test_wet_at_4_9s_is_early_wet_fault_sensor1() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 0, 3400));
}

// A wet reading at EXACTLY 5.000 s is legal — the window closes at equality
// (strict '<'), mirroring exactly-60.000 s for the trip rule.
void test_wet_at_exactly_5_0s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(5000u, 0u, false, false, 3050, 3050));
}

// ITEM CASE: wet at 5.1 s (even 0 counts, both sensors) is Ok — past the
// window, no trips.
void test_wet_at_5_1s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(5100u, 0u, false, false, 0, 0));
}

// Sensor 2's wet reading names sensor 2 (index 1 == fault code 2).
void test_wet_at_4_9s_on_sensor2_faults_sensor2() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 1,
                  g.evaluate(4900u, 0u, false, false, 3400, 3050));
}

// ---- wet predicate boundary (same inclusive predicate as the trip rule) ----

// ITEM CASE: exactly baseline − 350 (3050 for baseline 3400) reads WET —
// PRD §7.6 "'wet' ... same predicate as the trip rule" and PRD §7.3 is
// inclusive ("at or below").
void test_wet_boundary_inclusive_at_baseline_minus_350() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
}

// One count above the threshold (3051) reads dry inside the window.
void test_one_count_above_threshold_is_dry() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(4900u, 0u, false, false, 3051, 3051));
}

// ---- guards stay silent on legitimate fills ----

// Deep wet mid-fill (30 s, no trip yet): the debounce is still accumulating —
// the guards only police the FIRST 5 s of wetness and trips before 60 s.
void test_deep_wet_after_window_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(30000u, 0u, false, false, 0, 0));
}

// A trip at 120 s with the reading still wet is a LEGIT trip: guards pass it
// (the engine proceeds to CONFIRM, PRD §7.4-7.5).
void test_legit_trip_after_60s_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(120000u, 0u, true, false, 3050, 3400));
}

// ITEM CASE: both sensors clean (dry readings, no trips) at 59 999 ms -> Ok.
void test_clean_sensors_59_999ms_is_ok() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(59999u, 0u, false, false, 3400, 3400));
}

// A trip at the valve-open instant itself (elapsed 0) is maximally false.
void test_trip_at_valve_open_instant_is_early_trip() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(0u, 0u, true, false, 3400, 3400));
}

// ---- rollover safety (item contract: wrap-safe timestamps) ----

// Valve opened 600 ms before the 2^32 ms rollover; the trip is accepted
// 59.9 s into the fill, i.e. 'now' has wrapped to 59300. elapsedMs()'s
// mod-2^32 subtraction still yields 59900 -> EarlyTripFault.
void test_wrap_safe_trip_window_across_rollover() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    const uint32_t valve = 4294967295u - 599u;   // 2^32 - 600
    const uint32_t now   = 59300u;               // (valve + 59900) mod 2^32
    assertVerdict(GuardVerdict::EarlyTripFault, 0,
                  g.evaluate(now, valve, true, false, 3400, 3400));
}

// Valve opened 4900 ms before the rollover; the wet reading is observed
// exactly at the wrap, when the clock restarts at 0 -> elapsed 4900 ->
// EarlyWetFault.
void test_wrap_safe_wet_window_across_rollover() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    const uint32_t valve = 4294967295u - 4899u;  // 2^32 - 4900
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(0u, valve, false, false, 3050, 3400));
}

// ---- precedence / defaults / re-arm ----

// At 3 s the sensor has BOTH latched a trip and still reads wet: report
// EarlyWet — the more specific short diagnosis (fault outcome identical).
void test_precedence_wet_reported_before_trip() {
    FalseTripGuards g;
    g.begin(3400, 3400);
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(3000u, 0u, true, false, 3050, 3400));
}

// Never begun (protocol always calls begin() first): thresholds are 0, so
// only a 0-count reading would read wet; dry readings stay Ok.
void test_fresh_instance_dry_readings_ok() {
    FalseTripGuards g;
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(10000u, 0u, false, false, 4095, 4095));
}

// begin() re-arms for the next fill with NEW baselines -> new thresholds
// (lower baseline -> lower threshold -> the same reading may no longer read
// wet).
void test_begin_rearm_changes_thresholds() {
    FalseTripGuards g;
    g.begin(3400, 3400);                     // thresholds 3050
    assertVerdict(GuardVerdict::EarlyWetFault, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
    g.begin(2400, 2400);                     // thresholds 2050
    assertVerdict(GuardVerdict::Ok, 0,
                  g.evaluate(4900u, 0u, false, false, 3050, 3400));
    assertVerdict(GuardVerdict::EarlyWetFault, 1,
                  g.evaluate(4900u, 0u, false, false, 3400, 2050));
}

// End-to-end composition with the real TripDetector (P1.M3.T3.S1): 8 wet
// averaged readings at 100 ms cadence latch the detector at 800 ms; the
// guards — armed with the same baseline — see the wet reading inside the
// 5 s window, and (queried past the window) the latched trip inside the
// 60 s window. Pins that both classes share ONE predicate and ONE protocol.
void test_composition_with_trip_detector_same_predicate() {
    FalseTripGuards g;
    TripDetector t1, t2;
    g.begin(3400, 3400);
    t1.begin(3400);
    t2.begin(3400);
    GuardVerdict v{GuardVerdict::Ok, 0};
    for (uint32_t t = 100; t <= 800; t += 100) {
        t1.feed(3050);   // sensor 1 goes wet...
        t2.feed(3400);   // ...sensor 2 stays dry
        v = g.evaluate(t, 0u, t1.tripped(), t2.tripped(), 3050, 3400);
    }
    assertVerdict(GuardVerdict::EarlyWetFault, 0, v);   // wet window, avg 3050
    assertVerdict(GuardVerdict::EarlyTripFault, 0,      // trip window, latch set
                  g.evaluate(5800u, 0u, t1.tripped(), t2.tripped(), 3050, 3400));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_trip_at_59_9s_is_early_trip_fault_sensor1);
    RUN_TEST(test_trip_at_exactly_60_0s_is_ok);
    RUN_TEST(test_trip_at_60_1s_is_ok);
    RUN_TEST(test_trip_59_9s_on_sensor2_faults_sensor2);
    RUN_TEST(test_both_tripped_early_reports_sensor1_first);
    RUN_TEST(test_wet_at_4_9s_is_early_wet_fault_sensor1);
    RUN_TEST(test_wet_at_exactly_5_0s_is_ok);
    RUN_TEST(test_wet_at_5_1s_is_ok);
    RUN_TEST(test_wet_at_4_9s_on_sensor2_faults_sensor2);
    RUN_TEST(test_wet_boundary_inclusive_at_baseline_minus_350);
    RUN_TEST(test_one_count_above_threshold_is_dry);
    RUN_TEST(test_deep_wet_after_window_is_ok);
    RUN_TEST(test_legit_trip_after_60s_is_ok);
    RUN_TEST(test_clean_sensors_59_999ms_is_ok);
    RUN_TEST(test_trip_at_valve_open_instant_is_early_trip);
    RUN_TEST(test_wrap_safe_trip_window_across_rollover);
    RUN_TEST(test_wrap_safe_wet_window_across_rollover);
    RUN_TEST(test_precedence_wet_reported_before_trip);
    RUN_TEST(test_fresh_instance_dry_readings_ok);
    RUN_TEST(test_begin_rearm_changes_thresholds);
    RUN_TEST(test_composition_with_trip_detector_same_predicate);
    return UNITY_END();
}
