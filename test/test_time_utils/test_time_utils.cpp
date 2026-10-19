// test/test_time_utils.cpp — rollover-safety contract for elapsedMs/deadlinePassed
// (P1.M1.T2.S1). These tests ARE the timing contract for every downstream consumer:
// button debounce (P1.M2), sensor cadence + settle + false-trip windows (P1.M3),
// fill backstop deadlines (P1.M4), LED pattern phases (P1.M5), confirm windows (P1.M6).
// Expected values hand-derived from mod-2^32 arithmetic (see research notes); the
// 49.7-day wrap is exercised directly via 0xFFFFFFFF/0x00000000 timestamps.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// --- elapsedMs: equality → 0 elapsed ---
void test_elapsed_equality_at_zero() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(0UL, 0UL));
}
void test_elapsed_equality_mid_range() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(1000000UL, 1000000UL));
}
void test_elapsed_equality_at_max() {
    TEST_ASSERT_EQUAL_UINT32(0UL, elapsedMs(0xFFFFFFFFUL, 0xFFFFFFFFUL));
}

// --- elapsedMs: plain spans ---
void test_elapsed_simple_span() {
    TEST_ASSERT_EQUAL_UINT32(4000UL, elapsedMs(5000UL, 1000UL));
}

// --- elapsedMs: wrap boundary 0xFFFFFFFF → 0x00000000 ---
void test_elapsed_wrap_boundary_one_ms() {
    TEST_ASSERT_EQUAL_UINT32(1UL, elapsedMs(0UL, 0xFFFFFFFFUL));  // 1 ms across the wrap
}
void test_elapsed_wrap_small_span() {
    // since = 256 ms below the wrap; now = 100 ms after it → 100 + 256 = 356 ms
    TEST_ASSERT_EQUAL_UINT32(356UL, elapsedMs(0x00000064UL, 0xFFFFFF00UL));
}

// --- elapsedMs: large spans ---
void test_elapsed_max_span() {
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFUL, elapsedMs(0xFFFFFFFFUL, 0UL));
}
void test_elapsed_half_wheel_span() {
    TEST_ASSERT_EQUAL_UINT32(0x7FFFFFFFUL, elapsedMs(0x7FFFFFFFUL, 0UL));
}

// --- deadlinePassed: inclusive at equality, ±1 ms edges ---
void test_deadline_at_deadline_is_passed() {
    TEST_ASSERT_TRUE(deadlinePassed(1000UL, 1000UL));  // inclusive: backstop fires at T
}
void test_deadline_one_ms_after() {
    TEST_ASSERT_TRUE(deadlinePassed(1001UL, 1000UL));
}
void test_deadline_one_ms_before() {
    TEST_ASSERT_FALSE(deadlinePassed(999UL, 1000UL));
}

// --- deadlinePassed: across the wrap ---
void test_deadline_wrap_one_ms_after() {
    TEST_ASSERT_TRUE(deadlinePassed(0UL, 0xFFFFFFFFUL));      // due 1 ms before wrap
}
void test_deadline_wrap_one_ms_before() {
    TEST_ASSERT_FALSE(deadlinePassed(0xFFFFFFFEUL, 0xFFFFFFFFUL));
}

// --- config-scale scenario: 30-min backstop anchored 256 ms before the wrap ---
void test_backstop30min_across_wrap_elapsed() {
    const uint32_t valveOpened = 0xFFFFFF00UL;                       // 256 ms below wrap
    // mod-2^32 addition is the sanctioned way to build a deadline; the explicit cast
    // documents the intentional (well-defined) truncation of 4296767040 -> 0x1B7640.
    const uint32_t deadline = (uint32_t)(valveOpened + 30UL * 60000UL);  // = 0x1B7640
    TEST_ASSERT_EQUAL_UINT32(0x1B7640UL, deadline);                  // arithmetic anchor
    TEST_ASSERT_EQUAL_UINT32(1800000UL, elapsedMs(deadline, valveOpened));
}
void test_backstop30min_across_wrap_deadline_edges() {
    const uint32_t deadline = 0x1B7640UL;                     // = 0xFFFFFF00 + 1800000 (wrapped)
    TEST_ASSERT_FALSE(deadlinePassed(0x1B763FUL, deadline));  // 1 ms early: valve still open
    TEST_ASSERT_TRUE(deadlinePassed(0x1B7640UL, deadline));   // at deadline: fires
    TEST_ASSERT_TRUE(deadlinePassed(0x1B7641UL, deadline));   // 1 ms late: fires
}

// --- deadlinePassed: half-wheel domain boundary (2^31 ms ≈ 24.86 days) ---
void test_deadline_half_wheel_boundary() {
    TEST_ASSERT_TRUE(deadlinePassed(0x7FFFFFFFUL, 0UL));      // 1 ms inside: passed
    TEST_ASSERT_FALSE(deadlinePassed(0x80000000UL, 0UL));     // exactly half-wheel: not certifiable
    TEST_ASSERT_FALSE(deadlinePassed(0xFFFFFFFFUL, 0UL));     // 1 ms "before" 0 across wrap
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_elapsed_equality_at_zero);
    RUN_TEST(test_elapsed_equality_mid_range);
    RUN_TEST(test_elapsed_equality_at_max);
    RUN_TEST(test_elapsed_simple_span);
    RUN_TEST(test_elapsed_wrap_boundary_one_ms);
    RUN_TEST(test_elapsed_wrap_small_span);
    RUN_TEST(test_elapsed_max_span);
    RUN_TEST(test_elapsed_half_wheel_span);
    RUN_TEST(test_deadline_at_deadline_is_passed);
    RUN_TEST(test_deadline_one_ms_after);
    RUN_TEST(test_deadline_one_ms_before);
    RUN_TEST(test_deadline_wrap_one_ms_after);
    RUN_TEST(test_deadline_wrap_one_ms_before);
    RUN_TEST(test_backstop30min_across_wrap_elapsed);
    RUN_TEST(test_backstop30min_across_wrap_deadline_edges);
    RUN_TEST(test_deadline_half_wheel_boundary);
    return UNITY_END();
}
