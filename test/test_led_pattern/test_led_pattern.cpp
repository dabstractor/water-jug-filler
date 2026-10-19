// test/test_led_pattern/test_led_pattern.cpp — native contract proof for
// LedPatternEngine (P1.M5.T1.S1). Pins PRD §4/§7/§11: Solid always on; Off
// always off; DonePulse = 3x(200 on/200 off) + 1000 off repeating forever
// (no timeout); FaultBlink(n) = n x(250 on/250 off) + 2000 off repeating,
// n = 1..3; ack overlay = exactly 3 blinks at 100/100 (600 ms total) then
// the base pattern resumes EXACTLY; mode change mid-pattern resets the
// phase cleanly; all arithmetic wrap-safe across the uint32 rollover.
// Boundary convention (house style): a segment boundary instant belongs to
// the NEW segment — on-windows are [k*P, k*P+ON): t=199 on, t=200 off.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ITEM CASE: IDLE — default-constructed engine is Off: off at any instant,
// forever. Explicit setMode(Off) is identical.
void test_off_default_and_explicit() {
    LedPatternEngine led;                      // boots Off (IDLE, PRD §4)
    TEST_ASSERT_EQUAL_INT(false, led.tick(5000u));   // anchors phase
    TEST_ASSERT_EQUAL_INT(false, led.tick(5001u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(999999u));
    TEST_ASSERT_EQUAL_INT(LedPatternEngine::Off, led.mode());

    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(10000u));   // anchors, Solid on
    led.setMode(LedPatternEngine::Off);
    TEST_ASSERT_EQUAL_INT(false, led.tick(10001u));  // re-anchors, off
    TEST_ASSERT_EQUAL_INT(false, led.tick(1049999u));
}

// ITEM CASE: FILLING/CONFIRM — Solid is on at every instant, any age.
void test_solid_always_on() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor
    TEST_ASSERT_EQUAL_INT(true, led.tick(1u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3600u * 1000u));  // an hour in
}

// ITEM CASE: DONE pulse — exact on/off instants of the first group and the
// boundaries between pulses: on [0,200), off [200,400), on [400,600), off
// [600,800), on [800,1000), off [1000,1200) (PRD §4 3x on200/off200).
void test_done_pulse_boundaries() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    static const uint32_t t[] = {0, 199, 200, 399, 400, 599, 600,
                                 799, 800, 999, 1000, 1199};
    static const bool on[] = {true, true, false, false, true, true, false,
                              false, true, true, false, false};
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(on[i], led.tick(t[i]));
    }
}

// ITEM CASE: DONE gap + repetition — off for the WHOLE 1000 ms gap
// [1200,2200), on again at exactly 2200 (second cycle), same shape, and
// still cycling an hour later (no timeout, PRD §11).
void test_done_pulse_gap_and_repeat() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor
    TEST_ASSERT_EQUAL_INT(false, led.tick(1200u));      // gap starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(2199u));      // last gap ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(2200u));       // cycle 2 starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2400u));      // same shape: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(2600u));       // pulse 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(4399u));      // gap of cycle 2
    TEST_ASSERT_EQUAL_INT(true, led.tick(4400u));       // cycle 3
    // Cycle N start = N*2200. 1636 cycles ~ one hour — still pulsing:
    TEST_ASSERT_EQUAL_INT(true, led.tick(1636u * 2200u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1636u * 2200u + 1200u));
}

// ITEM CASE: FAULT code 1 — one group: on [0,250), off [250,500), pause
// [500,2500), repeat from 2500 (PRD §7 n=1).
void test_fault_one_group() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 1);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor, group starts
    TEST_ASSERT_EQUAL_INT(true, led.tick(249u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(250u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(499u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(500u));       // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2499u));      // last pause ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(2500u));       // cycle 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(2750u));
}

// ITEM CASE: FAULT code 2 — two groups of blinks then the 2000 ms pause:
// on [0,250), off [250,500), on [500,750), off [750,1000), pause
// [1000,3000), repeat from 3000.
void test_fault_two_groups() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 2);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(250u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(500u));        // blink 2
    TEST_ASSERT_EQUAL_INT(false, led.tick(750u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1000u));      // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(2999u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3000u));       // cycle 2, blink 1
    TEST_ASSERT_EQUAL_INT(true, led.tick(3500u));       // cycle 2, blink 2
}

// ITEM CASE: FAULT code 3 — three blinks then pause: on [0,250), [500,750),
// [1000,1250); pause [1500,3500); repeat from 3500.
void test_fault_three_groups() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 3);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(260u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(510u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(760u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(1010u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1260u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));      // pause starts
    TEST_ASSERT_EQUAL_INT(false, led.tick(3499u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(3500u));       // cycle 2
    TEST_ASSERT_EQUAL_INT(LedPatternEngine::FaultBlink, led.mode());
    TEST_ASSERT_EQUAL_UINT8(3, led.faultGroups());
}

// Defensive: setMode(FaultBlink, 0) clamps to one group (an empty group
// would read "off forever" — a latched fault must never look dead).
void test_fault_zero_groups_clamps_to_one() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::FaultBlink, 0);
    TEST_ASSERT_EQUAL_UINT8(1, led.faultGroups());
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(260u));       // 1-group shape
    TEST_ASSERT_EQUAL_INT(false, led.tick(2000u));      // inside the pause
    TEST_ASSERT_EQUAL_INT(true, led.tick(2500u));       // cycle 2 starts
}

// ITEM CASE: +5 ack overlay — exactly 3 blinks at 100/100 (600 ms total)
// masking Solid, then Solid resumes EXACTLY at t=600 and stays on. The
// overlay is one-shot: ackActive() drops when it completes.
void test_ack_overlay_then_solid_resumes_exactly() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(10000u));      // anchor Solid
    led.triggerAck();
    TEST_ASSERT_TRUE(led.ackActive());
    // First tick after triggerAck anchors the overlay at 20000:
    static const uint32_t t[] = {20000, 20099, 20100, 20199, 20200, 20299,
                                 20300, 20399, 20400, 20499, 20500, 20599};
    static const bool on[] = {true, true, false, false, true, true,
                              false, false, true, true, false, false};
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(on[i], led.tick(t[i]));
    }
    TEST_ASSERT_EQUAL_INT(false, led.tick(20599u));     // LAST overlay ms: off
    TEST_ASSERT_TRUE(led.ackActive());                 // still armed at 599
    TEST_ASSERT_EQUAL_INT(true, led.tick(20600u));     // boundary -> Solid ALREADY on
    TEST_ASSERT_FALSE(led.ackActive());                // one-shot complete
    TEST_ASSERT_EQUAL_INT(true, led.tick(20601u));      // ...and stays on
    TEST_ASSERT_EQUAL_INT(true, led.tick(50000u));
}

// Ack over a BLINKING base: the base phase keeps running under the mask —
// DonePulse's gap elapsed during the 600 ms overlay, so at overlay end the
// base is exactly where it would have been (PRD §4 "resumes", generalized).
void test_ack_over_blinking_base_phase_undisturbed() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));          // anchor at 0
    led.triggerAck();                                   // during pulse 1
    // Overlay anchors at the next tick = 100: 3 blinks until 700.
    TEST_ASSERT_EQUAL_INT(true, led.tick(100u));        // overlay blink 1
    TEST_ASSERT_EQUAL_INT(false, led.tick(200u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(300u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(400u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(600u));
    // At 700 the base shows again: base t=700 -> [600,800) off. Base phase
    // ran continuously under the mask (700 % 2200 = 700 -> off).
    TEST_ASSERT_EQUAL_INT(false, led.tick(700u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(800u));        // base pulse 3
    TEST_ASSERT_EQUAL_INT(false, led.tick(1200u));      // base gap
}

// ITEM CASE: mode change mid-pattern resets phase cleanly — from deep in
// the DonePulse gap, FaultBlink(2) restarts at its t=0 (LED on), and a
// further Solid change lights immediately.
void test_mode_change_mid_pattern_resets_phase() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    TEST_ASSERT_EQUAL_INT(true, led.tick(0u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1500u));      // mid-gap
    led.setMode(LedPatternEngine::FaultBlink, 2);       // mid-pattern change
    TEST_ASSERT_EQUAL_INT(true, led.tick(1600u));       // new phase t=0: on
    TEST_ASSERT_EQUAL_INT(false, led.tick(1600u + 260u));
    TEST_ASSERT_EQUAL_INT(true, led.tick(1600u + 500u));
    TEST_ASSERT_EQUAL_INT(false, led.tick(1600u + 1000u));  // pause
    led.setMode(LedPatternEngine::Solid);               // again, mid-pause
    TEST_ASSERT_EQUAL_INT(true, led.tick(2000u));       // instantly on
}

// ITEM CASE: wrap-safe — phase anchored just below the uint32 rollover and
// ticked across it keeps the exact pattern; the ack overlay crosses the
// rollover too. elapsedMs is mod-2^32 subtraction (P1.M1.T2.S1).
void test_wrap_safe_across_rollover() {
    LedPatternEngine led;
    led.setMode(LedPatternEngine::DonePulse);
    const uint32_t anchor = 0xFFFFFF00u;               // 256 ms below the wrap
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor));          // anchors, t=0: on
    // "anchor + k" wraps naturally in unsigned arithmetic — every instant
    // below k>256 is POST-rollover, exercising elapsedMs across the wrap.
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 200u));  // t=200: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor + 512u));   // t=512: pulse 2 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 600u));  // t=600: off slot
    TEST_ASSERT_EQUAL_INT(false, led.tick(anchor + 1200u)); // t=1200: gap
    // Ack across the rollover: overlay anchored 256 ms below the wrap, all
    // three blinks and the resume land after it.
    led.setMode(LedPatternEngine::Solid);
    TEST_ASSERT_EQUAL_INT(true, led.tick(anchor + 2000u));      // Solid anchor
    led.triggerAck();
    const uint32_t ackAnchor = anchor + 2100u;                  // still pre-wrap
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor));           // blink 1 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 100u));   // blink 1 off
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 200u));    // blink 2 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 300u));   // post-wrap: off
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 400u));    // blink 3 on
    TEST_ASSERT_EQUAL_INT(false, led.tick(ackAnchor + 599u));   // last overlay ms
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 600u));    // Solid resumes
    TEST_ASSERT_EQUAL_INT(true, led.tick(ackAnchor + 601u));    // ...and stays
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_off_default_and_explicit);
    RUN_TEST(test_solid_always_on);
    RUN_TEST(test_done_pulse_boundaries);
    RUN_TEST(test_done_pulse_gap_and_repeat);
    RUN_TEST(test_fault_one_group);
    RUN_TEST(test_fault_two_groups);
    RUN_TEST(test_fault_three_groups);
    RUN_TEST(test_fault_zero_groups_clamps_to_one);
    RUN_TEST(test_ack_overlay_then_solid_resumes_exactly);
    RUN_TEST(test_ack_over_blinking_base_phase_undisturbed);
    RUN_TEST(test_mode_change_mid_pattern_resets_phase);
    RUN_TEST(test_wrap_safe_across_rollover);
    return UNITY_END();
}
