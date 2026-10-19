// test/test_trip/test_trip.cpp — native contract proof for TripDetector
// (P1.M3.T3.S1). Pins PRD §7.3: an averaged reading at or below
// (baseline − TRIP_DROP_COUNTS = 350) for DEBOUNCE_COUNT = 8 consecutive
// readings = that sensor tripped; the trip LATCHES for the remainder of the
// fill; the streak resets on any reading STRICTLY above the threshold;
// sensors are independent (one instance per sensor).
// Worked example used throughout: baseline 3400 -> threshold 3050
// (3400 − 350; 3050 itself QUALIFIES, inclusive; 3051 resets).
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T3S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- streak length (DEBOUNCE_COUNT = 8) ----

// Seven consecutive qualifying readings are NOT enough (PRD §7.3 demands 8).
void test_seven_qualifying_readings_do_not_trip() {
    TripDetector t;
    t.begin(3400);                       // threshold 3050
    for (int i = 0; i < 7; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
}

// ITEM CASE: the 8th consecutive qualifying reading trips — exactly 8.
void test_exactly_eight_consecutive_trip() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) {
        t.feed(3050);
        TEST_ASSERT_FALSE(t.tripped());
    }
    t.feed(3050);                        // the 8th
    TEST_ASSERT_TRUE(t.tripped());
}

// ITEM CASE: 7 qualifying readings, then ONE strictly above the threshold,
// resets the streak to zero — 7 more are still not enough; the full 8 are
// needed after the break.
void test_seventh_then_above_resets_streak() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) t.feed(3050);
    t.feed(3051);                        // strictly above -> streak = 0
    for (int i = 0; i < 7; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
    t.feed(3050);                        // 8th since the break
    TEST_ASSERT_TRUE(t.tripped());
}

// ---- threshold boundary (inclusive at exactly baseline − 350) ----

// ITEM CASE: exactly baseline − 350 (3050 for baseline 3400) QUALIFIES —
// PRD §7.3 "at or below (baseline − TRIP_DROP_COUNTS)" is inclusive.
void test_exactly_baseline_minus_350_qualifies() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(3050);   // == threshold
    TEST_ASSERT_TRUE(t.tripped());
}

// ITEM CASE: baseline − 349 (3051) is strictly ABOVE the threshold — it
// never advances the streak (even 100 in a row do not trip).
void test_one_count_above_threshold_never_trips() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 100; ++i) t.feed(3051);
    TEST_ASSERT_FALSE(t.tripped());
}

// The lowest legal baseline (BASELINE_MIN = 2000, verdict Ok) gives threshold
// 1650; exactly 1650 still qualifies 8 times -> trip. (2000 > 350: no wrap.)
void test_min_valid_baseline_threshold() {
    TripDetector t;
    t.begin(2000);                       // threshold 2000 − 350 = 1650
    for (int i = 0; i < 8; ++i) t.feed(1650);
    TEST_ASSERT_TRUE(t.tripped());
}

// A far-below-threshold reading (deep wet, 0 counts) still needs all 8 —
// depth does not shortcut debounce.
void test_deep_wet_still_needs_eight() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 7; ++i) t.feed(0);
    TEST_ASSERT_FALSE(t.tripped());
    t.feed(0);
    TEST_ASSERT_TRUE(t.tripped());
}

// Alternating wet/dry never accumulates 8 consecutive.
void test_alternating_never_trips() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 40; ++i) {
        t.feed(3050);
        t.feed(3051);
    }
    TEST_ASSERT_FALSE(t.tripped());
}

// ---- latch ----

// ITEM CASE: no re-trip noise after the latch — dry readings after the trip
// cannot un-trip; the valve-close decision cannot chatter.
void test_latch_survives_dry_readings_after_trip() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
    for (int i = 0; i < 100; ++i) t.feed(3400);   // fully dry again
    TEST_ASSERT_TRUE(t.tripped());
}

// Post-trip feeds of ANY value are inert (0 and 4095 garbage included).
void test_latch_ignores_post_trip_garbage() {
    TripDetector t;
    t.begin(3400);
    for (int i = 0; i < 8; ++i) t.feed(2000);
    TEST_ASSERT_TRUE(t.tripped());
    t.feed(0);
    t.feed(4095);
    t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
}

// ---- per-sensor independence ----

// ITEM CASE: sensor 1 trips while sensor 2 stays dry — one instance per
// sensor, no shared state.
void test_sensors_independent_one_trips() {
    TripDetector t1, t2;
    t1.begin(3400);
    t2.begin(3400);
    for (int i = 0; i < 8; ++i) {
        t1.feed(3050);
        t2.feed(3400);
    }
    TEST_ASSERT_TRUE(t1.tripped());
    TEST_ASSERT_FALSE(t2.tripped());
}

// Different baselines give different thresholds: 3000 counts qualifies for
// baseline 3400 (threshold 3050) but not for baseline 2000 (threshold 1650).
void test_different_baselines_different_thresholds() {
    TripDetector t1, t2;
    t1.begin(3400);   // threshold 3050
    t2.begin(2000);   // threshold 1650
    for (int i = 0; i < 8; ++i) {
        t1.feed(3000);
        t2.feed(3000);
    }
    TEST_ASSERT_TRUE(t1.tripped());
    TEST_ASSERT_FALSE(t2.tripped());
}

// ---- re-arm / defaults ----

// begin() re-arms for the next fill: latch cleared, streak zero, and a NEW
// baseline installs a new threshold (lower baseline -> lower threshold ->
// the same readings may no longer qualify).
void test_begin_rearms_with_new_baseline() {
    TripDetector t;
    t.begin(3400);                      // threshold 3050
    for (int i = 0; i < 8; ++i) t.feed(3050);
    TEST_ASSERT_TRUE(t.tripped());
    t.begin(2400);                      // threshold 2050 — 3050 no longer qualifies
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 50; ++i) t.feed(3050);
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 8; ++i) t.feed(2050);
    TEST_ASSERT_TRUE(t.tripped());
}

// A fresh instance (never begun) reads not-tripped and dry readings keep it
// that way; the engine protocol always calls begin() first (class comment).
void test_fresh_instance_dry_never_trips() {
    TripDetector t;
    TEST_ASSERT_FALSE(t.tripped());
    for (int i = 0; i < 100; ++i) t.feed(4095);
    TEST_ASSERT_FALSE(t.tripped());
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_seven_qualifying_readings_do_not_trip);
    RUN_TEST(test_exactly_eight_consecutive_trip);
    RUN_TEST(test_seventh_then_above_resets_streak);
    RUN_TEST(test_exactly_baseline_minus_350_qualifies);
    RUN_TEST(test_one_count_above_threshold_never_trips);
    RUN_TEST(test_min_valid_baseline_threshold);
    RUN_TEST(test_deep_wet_still_needs_eight);
    RUN_TEST(test_alternating_never_trips);
    RUN_TEST(test_latch_survives_dry_readings_after_trip);
    RUN_TEST(test_latch_ignores_post_trip_garbage);
    RUN_TEST(test_sensors_independent_one_trips);
    RUN_TEST(test_different_baselines_different_thresholds);
    RUN_TEST(test_begin_rearms_with_new_baseline);
    RUN_TEST(test_fresh_instance_dry_never_trips);
    return UNITY_END();
}
