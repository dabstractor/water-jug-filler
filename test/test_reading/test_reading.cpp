// test/test_reading/test_reading.cpp — native contract proof for
// ReadingAggregator + medianOf (P1.M3.T1.S1). Pins PRD §7/§8: one averaged
// pair per READING_INTERVAL_MS (100 ms), each the truncated mean of the last
// ADC_SAMPLES_PER_READING (32) samples (first sample pre-fills the window);
// cadence re-anchored at emission (no catch-up burst), wrap-safe; median
// odd/even/truncation/in-place/uint32-overflow.
// Expected values hand-derived AND machine-verified
// (plan/001_53984f832581/P1M3T1S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- averaging ----

// ITEM CASE: first sample pre-fills the whole window -> reading == sample.
void test_first_sample_prefills_window() {
    ReadingAggregator agg;
    agg.addSample(0, 3000, 0);
    agg.addSample(1, 2500, 0);
    uint16_t a1 = 999, a2 = 999;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(3000, a1);
    TEST_ASSERT_EQUAL_UINT16(2500, a2);
}

// ITEM CASE: exact averaging over a full window — 32 samples 1000..1031,
// sum 32496, 32496/32 = 1015.5 -> 1015 (truncation pinned).
void test_exact_average_full_window() {
    ReadingAggregator agg;
    for (uint16_t v = 1000; v <= 1031; ++v) agg.addSample(0, v, 0);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(1015, a1);
    TEST_ASSERT_EQUAL_UINT16(0, a2);   // sensor 1 never sampled -> 0
}

// Window slides: after 32x1000, one 1032 evicts a 1000 ->
// (32000 - 1000 + 1032)/32 = 1001.
void test_window_slides_oldest_evicted() {
    ReadingAggregator agg;
    for (int i = 0; i < 32; ++i) agg.addSample(0, 1000, 0);
    agg.addSample(0, 1032, 1);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(1001, a1);
}

// ITEM CASE: under-fill warmup — two samples: (64000 - 2000 + 3000)/32 = 2031.
void test_warmup_two_samples() {
    ReadingAggregator agg;
    agg.addSample(0, 2000, 0);
    agg.addSample(0, 3000, 1);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(2031, a1);
}

// Out-of-domain sensor indices are dropped; sensors are independent.
void test_out_of_domain_sensor_ignored() {
    ReadingAggregator agg;
    agg.addSample(0, 3000, 0);
    agg.addSample(2, 5000, 1);
    agg.addSample(255, 4095, 2);
    uint16_t a1 = 0, a2 = 0;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_EQUAL_UINT16(3000, a1);
    TEST_ASSERT_EQUAL_UINT16(0, a2);
}

// ---- cadence (injected clock; inclusive at exactly 100; wrap-safe) ----

// ITEM CASE: due at exactly 100 (inclusive); overdue stays due.
void test_due_exactly_at_interval_inclusive() {
    ReadingAggregator agg;
    TEST_ASSERT_FALSE(agg.readingDue(99));
    TEST_ASSERT_TRUE(agg.readingDue(100));
    TEST_ASSERT_TRUE(agg.readingDue(100000));
}

// Emission re-anchors the cadence at the emission instant.
void test_take_reading_reanchors_cadence() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    TEST_ASSERT_TRUE(agg.readingDue(100));
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(199));
    TEST_ASSERT_TRUE(agg.readingDue(200));
}

// A long stall emits exactly ONE reading (no catch-up burst) — DEBOUNCE_COUNT
// always means ~800 ms of real time.
void test_no_catchup_burst_after_stall() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    agg.takeReading(100, a1, a2);
    TEST_ASSERT_TRUE(agg.readingDue(5000));
    agg.takeReading(5000, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(5099));
    TEST_ASSERT_TRUE(agg.readingDue(5100));
}

// ITEM CASE: cadence across the 49.7-day wrap — anchor at 0xFFFFFF9C is
// exactly 100 ms below the wrap: not due at 0xFFFFFFFF (99 elapsed), due at
// 0x00000000 (exactly 100, wrapped).
void test_cadence_survives_49day_wrap() {
    ReadingAggregator agg;
    uint16_t a1, a2;
    agg.addSample(0, 3000, 0);
    agg.takeReading(0xFFFFFF9CUL, a1, a2);
    TEST_ASSERT_FALSE(agg.readingDue(0xFFFFFFFFUL));
    TEST_ASSERT_TRUE(agg.readingDue(0x00000000UL));
    agg.takeReading(0, a1, a2);
    TEST_ASSERT_TRUE(agg.readingDue(100));
}

// ---- medianOf ----

void test_median_odd_takes_middle() {
    uint16_t v[] = {3500, 2000, 3000};
    TEST_ASSERT_EQUAL_UINT16(3000, medianOf(v, 3));
}

void test_median_even_means_middle_pair() {
    uint16_t v[] = {10, 20, 30, 40};
    TEST_ASSERT_EQUAL_UINT16(25, medianOf(v, 4));
}

void test_median_even_truncates() {
    uint16_t v[] = {1, 2};
    TEST_ASSERT_EQUAL_UINT16(1, medianOf(v, 2));  // 1.5 -> 1
}

// ITEM CASE: single-outlier robustness — one wild 4095 cannot move the median.
void test_median_shrugs_off_single_outlier() {
    uint16_t v[] = {3000, 3010, 4095, 3005, 3008};
    TEST_ASSERT_EQUAL_UINT16(3008, medianOf(v, 5));
}

// CONTRACT: the buffer is sorted ascending in place (BaselineCapture passes
// scratch memory and relies on this being the only mutation).
void test_median_sorts_in_place_ascending() {
    uint16_t v[] = {3, 1, 2};
    medianOf(v, 3);
    TEST_ASSERT_EQUAL_UINT16(1, v[0]);
    TEST_ASSERT_EQUAL_UINT16(2, v[1]);
    TEST_ASSERT_EQUAL_UINT16(3, v[2]);
}

// Overflow trap: 65535 + 65534 = 131069 needs the uint32 accumulator
// (a uint16 sum wraps to 533 and yields 32766 — wrong).
void test_median_max_pair_no_uint16_overflow() {
    uint16_t v[] = {65535, 65534};
    TEST_ASSERT_EQUAL_UINT16(65534, medianOf(v, 2));
}

void test_median_edge_sizes() {
    uint16_t v[] = {7};
    TEST_ASSERT_EQUAL_UINT16(0, medianOf(v, 0));
    TEST_ASSERT_EQUAL_UINT16(7, medianOf(v, 1));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_prefills_window);
    RUN_TEST(test_exact_average_full_window);
    RUN_TEST(test_window_slides_oldest_evicted);
    RUN_TEST(test_warmup_two_samples);
    RUN_TEST(test_out_of_domain_sensor_ignored);
    RUN_TEST(test_due_exactly_at_interval_inclusive);
    RUN_TEST(test_take_reading_reanchors_cadence);
    RUN_TEST(test_no_catchup_burst_after_stall);
    RUN_TEST(test_cadence_survives_49day_wrap);
    RUN_TEST(test_median_odd_takes_middle);
    RUN_TEST(test_median_even_means_middle_pair);
    RUN_TEST(test_median_even_truncates);
    RUN_TEST(test_median_shrugs_off_single_outlier);
    RUN_TEST(test_median_sorts_in_place_ascending);
    RUN_TEST(test_median_max_pair_no_uint16_overflow);
    RUN_TEST(test_median_edge_sizes);
    return UNITY_END();
}
