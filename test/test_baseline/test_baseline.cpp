// test/test_baseline/test_baseline.cpp — native contract proof for
// BaselineCapture (P1.M3.T2.S1). Pins PRD §7.1: 2 s settle collecting
// nothing (not shortcut at 1999 ms; first pair accepted at exactly 2000,
// inclusive), exactly 5 averaged pairs per sensor, baseline = median of 5
// (one outlier per sensor cannot move it), plausibility gate inclusive at
// BASELINE_MIN (2000) / BASELINE_MAX (4050) — 1999 -> TooLow, 4051 ->
// TooHigh — both-sensors-bad, sensor independence, fail-safe accessors
// before done(), begin() reuse, settle across the 49.7-day wrap.
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M3T2S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- settle (injected clock; inclusive at exactly 2000; wrap-safe) ----

// A dozen-plus pairs during the settle window are ALL dropped — nothing is
// collected before BASELINE_SETTLE_MS elapses (PRD §7.1).
void test_settle_collects_nothing() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 100; t <= 1900; t += 100) bc.addReading(t, 3400, 3300);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
}

// ITEM CASE: settle not shortcut at 1999 ms — five pairs at 1999 would
// complete a capture if any leaked in; none may.
void test_settle_not_shortcut_at_1999() {
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i) bc.addReading(1999, 1000, 1000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
}

// ITEM CASE: the settle completes at EXACTLY 2000 (inclusive, house rule) —
// the first pair at t=2000 is accepted; the t=1999 pair just before is not.
void test_first_pair_accepted_exactly_at_2000() {
    BaselineCapture bc;
    bc.begin(0);
    bc.addReading(1999, 1000, 1000);  // still settle: dropped
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(2500, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ---- capture length ----

// done() flips on exactly the 5th accepted pair.
void test_done_needs_exactly_five_pairs() {
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 4; ++i) bc.addReading(2000 + 100 * (uint32_t)i, 3500, 2500);
    TEST_ASSERT_FALSE(bc.done());
    bc.addReading(2400, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
}

// Pairs after Done are ignored — verdicts/baselines are stable.
void test_pairs_after_done_ignored() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    bc.addReading(2500, 0, 4095);  // garbage: must change nothing
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(2500, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// ---- plausibility gate (BASELINE_MIN..MAX, bounds inclusive) ----

// ITEM CASE: 2000 == BASELINE_MIN is IN range.
void test_min_boundary_2000_inclusive_ok() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 2000, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_EQUAL_UINT16(2000, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ITEM CASE: 4050 == BASELINE_MAX is IN range.
void test_max_boundary_4050_inclusive_ok() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 4050, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_EQUAL_UINT16(4050, bc.baseline(0));
}

// ITEM CASE: 1999 — one count below the floor — faults TooLow.
void test_below_min_1999_toolow() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 1999, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_EQUAL_UINT16(1999, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// ITEM CASE: 4051 — one count above the ceiling — faults TooHigh.
void test_above_max_4051_toohigh() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 4051, 3200);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooHigh);
    TEST_ASSERT_EQUAL_UINT16(4051, bc.baseline(0));
}

// ITEM CASE: both sensors can be bad at once (one low, one high) — the FSM
// faults each independently (codes 1 and 2).
void test_both_sensors_faulted() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 1999, 4051);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooHigh);
}

// Sensors are judged independently.
void test_sensors_independent() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 1999);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
}

// ---- median-of-5 robustness ----

// ITEM CASE: one outlier per sensor (a wet-splash low on sensor 1, a wild
// spike high on sensor 2) cannot move the median of 5.
//   s0 sorted: 1000 3400 3405 3408 3410 -> median 3405
//   s1 sorted: 3300 3305 3308 3310 4095 -> median 3308
void test_median_robust_to_one_outlier() {
    const uint16_t s0[] = {3400, 3410, 1000, 3405, 3408};
    const uint16_t s1[] = {3300, 3310, 4095, 3305, 3308};
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i)
        bc.addReading(2000 + 100 * (uint32_t)i, s0[i], s1[i]);
    TEST_ASSERT_EQUAL_UINT16(3405, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(3308, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::Ok);
}

// A single low outlier cannot push a boundary-straddling capture out of
// range: median of {1999,2000,2000,2000,2000} is 2000 -> in range.
void test_one_outlier_cannot_fault_at_boundary() {
    const uint16_t v[] = {1999, 2000, 2000, 2000, 2000};
    BaselineCapture bc;
    bc.begin(0);
    for (int i = 0; i < 5; ++i) bc.addReading(2000 + 100 * (uint32_t)i, v[i], 3400);
    TEST_ASSERT_EQUAL_UINT16(2000, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// A never-sampled sensor emits 0 counts (aggregator convention) -> TooLow,
// i.e. out-of-range by design — the other sensor is unaffected.
void test_never_sampled_sensor_zero_toolow() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 0);
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
}

// ---- fail-safe defaults / reuse / defense ----

// Before done() the accessors are fail-safe: baseline 0 classifies TooLow —
// a premature read can never look Ok (verdict ALWAYS classifies baseline).
void test_accessors_fail_safe_before_done() {
    BaselineCapture bc;
    bc.begin(1000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(1));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooLow);
}

// begin() fully resets a completed capture for the next fill.
void test_begin_resets_for_next_fill() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3400, 3300);
    TEST_ASSERT_TRUE(bc.done());
    bc.begin(5000);
    TEST_ASSERT_FALSE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::TooLow);
    for (uint32_t t = 7000; t <= 7400; t += 100) bc.addReading(t, 2000, 4051);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
    TEST_ASSERT_TRUE(bc.verdict(1) == BaselineCapture::TooHigh);
}

// Settle across the 49.7-day wrap: begin at 0xFFFFF830 — exactly +2000 is
// the wrapped instant 0. Five pairs at 0xFFFFFFFF (elapsed 463 ms) drop;
// pairs from t=0 (elapsed exactly 2000, wrapped) are accepted.
void test_settle_survives_49day_wrap() {
    BaselineCapture bc;
    bc.begin(0xFFFFF830UL);
    for (int i = 0; i < 5; ++i) bc.addReading(0xFFFFFFFFUL, 3400, 3300);
    TEST_ASSERT_FALSE(bc.done());
    for (uint32_t t = 0; t <= 400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_TRUE(bc.done());
    TEST_ASSERT_EQUAL_UINT16(3500, bc.baseline(0));
    TEST_ASSERT_TRUE(bc.verdict(0) == BaselineCapture::Ok);
}

// Out-of-domain sensor indices: defensive 0 / TooLow (aggregator convention).
void test_out_of_domain_sensor_accessors() {
    BaselineCapture bc;
    bc.begin(0);
    for (uint32_t t = 2000; t <= 2400; t += 100) bc.addReading(t, 3500, 2500);
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(2));
    TEST_ASSERT_EQUAL_UINT16(0, bc.baseline(255));
    TEST_ASSERT_TRUE(bc.verdict(2) == BaselineCapture::TooLow);
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_settle_collects_nothing);
    RUN_TEST(test_settle_not_shortcut_at_1999);
    RUN_TEST(test_first_pair_accepted_exactly_at_2000);
    RUN_TEST(test_done_needs_exactly_five_pairs);
    RUN_TEST(test_pairs_after_done_ignored);
    RUN_TEST(test_min_boundary_2000_inclusive_ok);
    RUN_TEST(test_max_boundary_4050_inclusive_ok);
    RUN_TEST(test_below_min_1999_toolow);
    RUN_TEST(test_above_max_4051_toohigh);
    RUN_TEST(test_both_sensors_faulted);
    RUN_TEST(test_sensors_independent);
    RUN_TEST(test_median_robust_to_one_outlier);
    RUN_TEST(test_one_outlier_cannot_fault_at_boundary);
    RUN_TEST(test_never_sampled_sensor_zero_toolow);
    RUN_TEST(test_accessors_fail_safe_before_done);
    RUN_TEST(test_begin_resets_for_next_fill);
    RUN_TEST(test_settle_survives_49day_wrap);
    RUN_TEST(test_out_of_domain_sensor_accessors);
    return UNITY_END();
}
