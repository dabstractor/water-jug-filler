// test/test_button/test_button.cpp — native contract proof for
// ButtonClassifier (P1.M2.T1.S1). Pins PRD §5.1 semantics: 40ms stable-sample
// debounce (sub-40ms spikes discarded), >=1000ms hold = Long, <1000ms = Short,
// BOTH fire on release, nothing while held.
// Timing model: press raw-stable from C to R (ms) classifies exactly (R - C).
// Expected values hand-derived (plan/.../P1M2T1S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Pump helper: feed `pressed` at every ms from..until (inclusive); return the
// first non-None event, or None if none occurred. 1ms polls make the raw script
// exact at ms boundaries. Wrap-safe: the range may straddle the 2^32 wrap
// (until < from), so termination is checked AFTER each update instead of in the
// loop condition — an ascending `t <= until` head-check would run zero passes
// for wrapped ranges.
static ButtonClassifier::Event pump(ButtonClassifier& c, uint32_t from,
                                    uint32_t until, bool pressed) {
    for (uint32_t t = from;; ++t) {
        ButtonClassifier::Event ev = c.update(t, pressed);
        if (ev != ButtonClassifier::None) return ev;
        if (t == until) break;
    }
    return ButtonClassifier::None;
}

// Fresh classifier is silent on unpressed input.
void test_fresh_classifier_silent() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 500, false) == ButtonClassifier::None);
}

// ITEM CASE: sub-40ms bounce spike ignored — a 35ms press never happened.
void test_spike_under_40ms_ignored() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 35, true) == ButtonClassifier::None);   // spike
    TEST_ASSERT_TRUE(pump(c, 36, 2000, false) == ButtonClassifier::None); // silence
}

// Bounce (0 true / 10 false / 15 true) settles into ONE press measured from the
// last raw edge: hold = 500 - 15 = 485ms → Short.
void test_bounce_settles_into_press() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 9, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 10, 14, false) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 15, 500, true) == ButtonClassifier::None); // held, no event
    TEST_ASSERT_TRUE(pump(c, 501, 700, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: short release — raw-stable 0..100 → hold 100ms → Short at ~140.
void test_short_press_fires_short_on_release() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 300, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: long release — raw-stable 0..1500 → hold 1500ms → Long.
void test_long_press_fires_long_on_release() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 1499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1500, 1800, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: hold exactly 1000ms → Long (>= inclusive). Also pins the anchor
// math: polling at acceptance gives pressStart = 0+40, hold = 1040-40 = 1000.
// A poll-time-anchored implementation would measure 999 → Short and FAIL here.
void test_hold_exactly_1000ms_is_long() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 999, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1000, 1200, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: release at 999ms → Short (strictly below the threshold).
void test_hold_999ms_is_short() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 998, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 999, 1200, false) == ButtonClassifier::ShortPress);
}

// ITEM CASE: no event while still held — 3s of continuous press, silence
// throughout (crossing the 1000ms threshold mid-hold emits nothing).
void test_no_event_while_still_held() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 3000, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 3001, 3400, false) == ButtonClassifier::LongPress);
}

// ITEM CASE: back-to-back presses — two Shorts, second press works immediately
// after the first release (no lockout, no stuck state, no double event).
void test_back_to_back_short_presses() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 200, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 300, 399, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 400, 600, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 601, 800, false) == ButtonClassifier::None); // no extra
}

// Mixed sequence: Short then Long in one classifier lifetime.
void test_short_then_long_sequence() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 300, false) == ButtonClassifier::ShortPress);
    TEST_ASSERT_TRUE(pump(c, 500, 1999, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 2000, 2200, false) == ButtonClassifier::LongPress);
}

// Mid-hold glitch (5ms raw drop at t=500) neither splits nor resets the press:
// hold still measures 1500 → Long (not 1000, not two Shorts).
void test_midhold_glitch_does_not_split_or_reset() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 500, 504, false) == ButtonClassifier::None); // glitch
    TEST_ASSERT_TRUE(pump(c, 505, 1499, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 1500, 1700, false) == ButtonClassifier::LongPress);
}

// Release-side bounce (false 100 / true 110..114 / false 115) delays acceptance
// but classification uses the LAST raw edge: hold = 115ms → Short.
void test_release_bounce_still_classifies() {
    ButtonClassifier c;
    TEST_ASSERT_TRUE(pump(c, 0, 99, true) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 100, 109, false) == ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, 110, 114, true) == ButtonClassifier::None);  // re-bounce
    TEST_ASSERT_TRUE(pump(c, 115, 400, false) == ButtonClassifier::ShortPress);
}

// Wrap safety: press anchored 0xFFFFFF00, held 1000ms → release raw edge wraps
// to 0x2E8. hold = (0x2E8+40) - (0xFFFFFF00+40) = 1000 mod 2^32 → Long.
void test_hold_across_clock_wrap_is_long() {
    ButtonClassifier c;
    const uint32_t pressEdge = 0xFFFFFF00UL;              // 256ms below the wrap
    const uint32_t releaseEdge = pressEdge + 1000UL;      // = 0x2E8 (wrapped)
    TEST_ASSERT_TRUE(pump(c, pressEdge, releaseEdge - 1UL, true) ==
                    ButtonClassifier::None);
    TEST_ASSERT_TRUE(pump(c, releaseEdge, releaseEdge + 300UL, false) ==
                    ButtonClassifier::LongPress);
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_classifier_silent);
    RUN_TEST(test_spike_under_40ms_ignored);
    RUN_TEST(test_bounce_settles_into_press);
    RUN_TEST(test_short_press_fires_short_on_release);
    RUN_TEST(test_long_press_fires_long_on_release);
    RUN_TEST(test_hold_exactly_1000ms_is_long);
    RUN_TEST(test_hold_999ms_is_short);
    RUN_TEST(test_no_event_while_still_held);
    RUN_TEST(test_back_to_back_short_presses);
    RUN_TEST(test_short_then_long_sequence);
    RUN_TEST(test_midhold_glitch_does_not_split_or_reset);
    RUN_TEST(test_release_bounce_still_classifies);
    RUN_TEST(test_hold_across_clock_wrap_is_long);
    return UNITY_END();
}
