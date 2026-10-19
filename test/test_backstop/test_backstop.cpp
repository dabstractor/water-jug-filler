// test/test_backstop/test_backstop.cpp — native contract proof for
// backstopMinutes() (P1.M4.T1.S1). Pins PRD §6.2: piecewise-LINEAR
// interpolation between the BACKSTOP_TABLE_MIN knots {2,44},{10,41},
// {20,38},{35,34},{60,30}, CLAMPED at both ends, interpolated result
// FLOORED to whole minutes. The two half-integer midpoints are the floor
// discriminators: recovery 6 -> exact 42.5 -> 42 (a truncating-drop
// implementation returns 43 = ceil = forbidden padding); recovery 15 ->
// exact 39.5 -> 39 (naive form returns 40).
// Segment exact values (all floored): (2,10]: 44 - 3(r-2)/8; (10,20]:
// 41 - 3(r-10)/10; (20,35]: 38 - 4(r-20)/15; (35,60): 34 - 4(r-35)/25.
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M4T1S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- exact values at all 5 knots (item contract) ----

// ITEM CASE: the table's own points return their own backstops, exactly.
void test_knots_exact() {
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(2));
    TEST_ASSERT_EQUAL_UINT32(41, backstopMinutes(10));
    TEST_ASSERT_EQUAL_UINT32(38, backstopMinutes(20));
    TEST_ASSERT_EQUAL_UINT32(34, backstopMinutes(35));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(60));
}

// ---- low clamp: recovery <= 2 min -> 44 (item contract) ----

// ITEM CASE: 0, 1 and 2 minutes of recovery all clamp to the first knot's
// backstop — a refill started immediately gets the longest grant.
void test_low_clamp_zero_one_two() {
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(0));
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(1));
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(2));
}

// ---- high clamp: recovery >= 60 min -> 30 (item contract) ----

// ITEM CASE: 60 (also a knot), 120, 1440 (a day) and the full uint32_t
// range all clamp to the last knot's backstop.
void test_high_clamp_sixty_and_beyond() {
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(60));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(120));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(1440));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(4294967295u));
}

// ---- floor rounding on the midpoints (item contract: "6 -> between 44 and
// 41; 15 -> between 41 and 38"; rounding = floor, pinned) ----

// ITEM CASE: recovery 6 is the (2,10) midpoint: exact 44 - 3*4/8 = 42.5 ->
// FLOOR 42. A truncating-division-on-the-drop implementation returns 43
// here (ceil of the result = padding, PRD §6 "never padded").
void test_midpoint_6_floors_to_42() {
    TEST_ASSERT_EQUAL_UINT32(42, backstopMinutes(6));
}

// ITEM CASE: recovery 15 is the (10,20) midpoint: exact 41 - 3*5/10 = 39.5
// -> FLOOR 39 (naive truncation returns 40).
void test_midpoint_15_floors_to_39() {
    TEST_ASSERT_EQUAL_UINT32(39, backstopMinutes(15));
}

// ---- segment samples across all four interior segments ----

// One sample per band of each segment, hand-derived (floor of exact):
// 3->43.625->43, 5->42.875->42, 9->41.375->41, 11->40.7->40, 19->38.3->38,
// 21->37.733->37, 27->36.133->36, 34->34.267->34, 36->33.84->33,
// 47->32.08->32, 59->30.16->30.
void test_segment_samples() {
    TEST_ASSERT_EQUAL_UINT32(43, backstopMinutes(3));
    TEST_ASSERT_EQUAL_UINT32(42, backstopMinutes(5));
    TEST_ASSERT_EQUAL_UINT32(41, backstopMinutes(9));
    TEST_ASSERT_EQUAL_UINT32(40, backstopMinutes(11));
    TEST_ASSERT_EQUAL_UINT32(38, backstopMinutes(19));
    TEST_ASSERT_EQUAL_UINT32(37, backstopMinutes(21));
    TEST_ASSERT_EQUAL_UINT32(36, backstopMinutes(27));
    TEST_ASSERT_EQUAL_UINT32(34, backstopMinutes(34));
    TEST_ASSERT_EQUAL_UINT32(33, backstopMinutes(36));
    TEST_ASSERT_EQUAL_UINT32(32, backstopMinutes(47));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(59));
}

// ---- item-contract property: monotone non-increasing over recovery ----

// More recovery never grants a longer backstop, and the output is bounded
// by the clamp values [30,44] — checked at EVERY minute of 0..180 (covers
// all segments, both clamps, and all knot boundaries).
void test_monotone_non_increasing_and_bounded() {
    uint32_t prev = backstopMinutes(0);
    for (uint32_t r = 1; r <= 180; ++r) {
        const uint32_t v = backstopMinutes(r);
        TEST_ASSERT_TRUE(prev >= v);           // monotone non-increasing
        TEST_ASSERT_TRUE(v >= 30 && v <= 44);  // bounded by clamp values
        prev = v;
    }
}

// ---- composition: the caller-side minutes derivation (P1.M4.T2.S1) ----

// Recovery minutes arrive as elapsedMs(now, lastFillEndMs) / 60000 — the
// same wrap-safe helper every timed service uses. Exactly 15 min elapsed
// -> recovery 15 -> 39. Documents (does not implement) the caller's path.
void test_composition_recovery_via_elapsedMs() {
    const uint32_t lastEnd = 1000000u;
    const uint32_t now = lastEnd + 15u * 60u * 1000u;   // exactly 15 min later
    TEST_ASSERT_EQUAL_UINT32(39,
                             backstopMinutes(elapsedMs(now, lastEnd) / 60000u));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_knots_exact);
    RUN_TEST(test_low_clamp_zero_one_two);
    RUN_TEST(test_high_clamp_sixty_and_beyond);
    RUN_TEST(test_midpoint_6_floors_to_42);
    RUN_TEST(test_midpoint_15_floors_to_39);
    RUN_TEST(test_segment_samples);
    RUN_TEST(test_monotone_non_increasing_and_bounded);
    RUN_TEST(test_composition_recovery_via_elapsedMs);
    return UNITY_END();
}
