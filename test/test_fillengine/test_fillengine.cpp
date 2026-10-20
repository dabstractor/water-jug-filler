// test/test_fillengine/test_fillengine.cpp — native contract proof for the
// fill lifecycle engine (P1.M6.T1.S2): §7 steps 1–9 end to end with scripted
// sensor streams, §5.2 flows 1/2, the §6 anchor/reference instants, the
// §7.9 close-before-state-flip ordering (per scenario AND as a seeded fuzz
// property), and the §7 fault paths incl. the exact 60 s guard boundary.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- deterministic loop simulator -----------------------------------------
// 25 ms passes; reading emissions anchored at lastReadingMs=0 land on
// t == 0 (mod 100). A press at t=1000 => baseline accepts from t=3000
// (inclusive 2000 ms settle), 5 pairs => valve opens t=3400 (== 0 mod 100).
// ALL fills open on the emission grid, so exact-time assertions hold.
// DRY=3500 is inside BASELINE_MIN..MAX (2000..4050); WET=2500 is below the
// minimum legal wet threshold (2000-350), so a wet emission always feeds
// the detectors' streak and the early-wet guard.
static constexpr uint16_t DRY = 3500;
static constexpr uint16_t WET = 2500;

struct Sim {
    FillEngine eng;
    FillEngineOutputs last{};
    uint32_t now = 1000;          // first press lands here
    uint32_t lastReadingMs = 0;   // emission cadence anchor
    uint16_t a1 = DRY, a2 = DRY;  // scripted sensor levels
    uint16_t e1 = DRY, e2 = DRY;  // last EMITTED pair (what the loop routes)

    const FillEngineOutputs& pass(ButtonClassifier::Event ev = ButtonClassifier::Event::None) {
        const bool due = elapsedMs(now, lastReadingMs) >= READING_INTERVAL_MS;
        if (due) {  // the loop re-anchors + snapshots at emission
            lastReadingMs = now;
            e1 = a1;
            e2 = a2;
        }
        last = eng.step(now, ev, due, e1, e2);
        now += 25;  // 4 passes per 100 ms reading
        return last;
    }
    const FillEngineOutputs& run(uint32_t ms) {
        const uint32_t end = now + ms;
        while (now < end) pass();
        return last;
    }
    // Advance until `ms` elapsed OR the fill ends mid-run — `last` is then
    // the FILL-ENDING step's output (a plain run() keeps passing past a
    // mid-window fault/backstop and its later empty-log steps would clobber
    // it). Use wherever the ending step's log events are asserted.
    const FillEngineOutputs& runToFillEnd(uint32_t ms) {
        const uint32_t end = now + ms;
        while (now < end) {
            pass();
            if (eng.state() != FillState::Filling && eng.state() != FillState::Confirm) break;
        }
        return last;
    }
    // Advance until exactly one more reading emission has happened.
    void readings(int n) {
        for (int i = 0; i < n; ++i) {
            const uint32_t target = lastReadingMs + READING_INTERVAL_MS;
            while (now <= target) pass();
        }
    }
    // Press and run until the valve opens (dry script). Returns t_open.
    uint32_t pressAndOpen() {
        pass(ButtonClassifier::Event::ShortPress);
        while (!eng.valveOpen()) pass();
        return eng.valveOpenedAtMs();
    }
};

// Deterministic 32-bit LCG (Numerical Recipes constants) — seeded fuzz,
// bit-reproducible on any host g++ (the seed IS the reproducibility contract).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Make `sensor` (0/1) trip with the 8th consecutive wet emission landing at
// EXACTLY fillTime == tripFillMs (a multiple of 100; t_open always is).
static void tripAt(Sim& s, int sensor, uint32_t tripFillMs) {
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_EQUAL_UINT32(0, tOpen % READING_INTERVAL_MS);  // alignment sanity
    const uint32_t tEnd = tOpen + tripFillMs;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();  // dry up to streak start
    if (sensor == 0) s.a1 = WET; else s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);  // 8 wet emissions, last AT tEnd
}

// ---- boot / fill start / baseline (PRD §7.1–§7.2) --------------------------

// ITEM CASE: fresh engine is IDLE, everything off, no events.
void test_boot_idle_all_off() {
    Sim s;
    const FillEngineOutputs& o = s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_EQUAL_UINT8(0, o.faultCode);
    TEST_ASSERT_EQUAL_UINT8(0, o.logCount);
}

// ITEM CASE: press starts the fill — backstop computed ONCE at press (fresh
// boot -> 30), LED Solid FROM THE PRESS (FILLING includes settle, arch
// §3.1/§4), but the valve stays CLOSED through the settle.
void test_press_starts_fill_backstop30_led_solid_valve_still_closed() {
    Sim s;
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_FALSE(o.valveOpen);                       // §7.1: settle first
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_TRUE(s.eng.deadline().freshBoot());
    TEST_ASSERT_EQUAL_UINT32(30, s.eng.deadline().tableMinutes());
    TEST_ASSERT_FALSE(s.eng.deadline().anchored());        // anchors at OPEN
    TEST_ASSERT_EQUAL_UINT8(1, o.logCount);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::FillStart);
    TEST_ASSERT_EQUAL_UINT32(30, o.logs[0].a);
    TEST_ASSERT_EQUAL_UINT32(0, o.logs[0].b);              // b: recovery ms (fresh boot)
}

// ITEM CASE: plausible baselines -> valve opens ~2.5 s after press and the
// deadline is ANCHORED AT THE OPEN INSTANT (settle does not eat the budget).
void test_baseline_ok_opens_valve_and_anchors_deadline() {
    Sim s;
    const uint32_t tPress = s.now;
    s.pass(ButtonClassifier::Event::ShortPress);
    while (!s.eng.valveOpen()) s.pass();
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_TRUE(elapsedMs(tOpen, tPress) >= BASELINE_SETTLE_MS);
    TEST_ASSERT_TRUE(s.last.valveOpen);
    TEST_ASSERT_TRUE(s.eng.deadline().anchored());
    TEST_ASSERT_EQUAL_UINT32(tOpen + 30u * 60000u, s.eng.deadline().deadlineMs());
    TEST_ASSERT_EQUAL_UINT8(2, s.last.logCount);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::BaselineOk);
    TEST_ASSERT_EQUAL_UINT32(DRY, s.last.logs[0].a);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ValveOpen);
    TEST_ASSERT_EQUAL_UINT32(30, s.last.logs[1].a);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::Solid);
}

// ITEM CASE: implausible baseline (§7.1) -> fault THAT sensor, valve NEVER
// opened, ValveClosed still logged FIRST (uniform §7.9), fault latches.
void test_baseline_fault_s1_refuses_valve() {
    Sim s;
    s.a1 = 1000;  // below BASELINE_MIN -> TooLow
    s.pass(ButtonClassifier::Event::ShortPress);
    bool everOpen = false;
    while (s.eng.state() == FillState::Filling) everOpen |= s.pass().valveOpen;
    TEST_ASSERT_FALSE(everOpen);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(1, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    s.pass(ButtonClassifier::Event::ShortPress);  // BOOT ignored while faulted (§7)
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
}

// ITEM CASE: symmetric — sensor 2 implausible -> fault code 2.
void test_baseline_fault_s2_refuses_valve() {
    Sim s;
    s.a2 = 4200;  // above BASELINE_MAX -> TooHigh
    s.pass(ButtonClassifier::Event::ShortPress);
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ---- happy paths (§7.3–§7.8) ----------------------------------------------

// ITEM CASE: full happy path, sensor 1 trips first at exactly 60.000 s (the
// guard boundary is strict '<': legal at equality), sensor 2 confirms 2 s
// later -> DONE with the recovery reference at the valve-CLOSE instant.
void test_happy_path_s1_first() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    TEST_ASSERT_FALSE(s.last.valveOpen);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9 first
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::Trip);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
    TEST_ASSERT_EQUAL_UINT32(60000, s.last.logs[1].a);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::Solid);  // CONFIRM continues
    s.a1 = DRY;
    tripAt(s, 1, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // exit ordering
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmPass);
    TEST_ASSERT_TRUE(s.last.logs[2].kind == FillLogEvent::Kind::Done);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::DonePulse);
    // §7.8: reference == first-trip instant (tOpen+60000), ms-exact.
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory());
    // reference == first-trip instant: now - recoveryMs(now) == tOpen + 60000.
    // Valid because no pass() runs between the Done step and this assert, so
    // s.now is exactly one 25 ms tick past the Done pass and the subtraction
    // cancels: s.now - recovery == reference.
    TEST_ASSERT_EQUAL_UINT32(tOpen + 60000u, s.now - s.eng.clock().recoveryMs(s.now));
}

// ITEM CASE: symmetric — sensor 2 trips first, sensor 1 confirms. Same DONE.
void test_happy_path_s2_first() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 60u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
    s.a2 = DRY;
    tripAt(s, 0, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmPass);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
}

// ITEM CASE: both sensors trip on the SAME emission -> Filling->Confirm->Done
// in ONE step; worst-case log = 5 events, ValveClosed still first.
void test_double_trip_same_step_done() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    const uint32_t tEnd = tOpen + 60000u;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();
    s.a1 = WET;
    s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_EQUAL_UINT8(5, s.last.logCount);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[4].kind == FillLogEvent::Kind::Done);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- §5.2 flow 1: abort -> restart with fresh RECOMPUTED timer -------------

// ITEM CASE: abort DURING SETTLE (valve never opened) is honored, restarts
// the recovery clock at the abort instant, and the restart's backstop is
// RECOMPUTED (recovery 1 min -> 44, vs fresh-boot 30).
void test_flow1_abort_in_settle_restarts_recovery() {
    Sim s;
    s.pass(ButtonClassifier::Event::ShortPress);
    s.run(500);  // still settling (valve opens ~2.5 s after press)
    TEST_ASSERT_FALSE(s.eng.valveOpen());
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);  // abort
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9, uniform
    TEST_ASSERT_TRUE(o.logs[1].kind == FillLogEvent::Kind::Abort);
    s.run(60000);  // 60 s of recovery since the abort
    const FillEngineOutputs& o2 = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(44, s.eng.deadline().tableMinutes());  // recomputed!
    TEST_ASSERT_EQUAL_UINT32(44, o2.logs[0].a);
}

// ITEM CASE: abort MID-FILL (valve open) -> restart 10 min after the abort
// recomputes from the ABORT instant: knot (10 min) -> 41.
void test_flow1_abort_midfill_recomputes() {
    Sim s;
    s.pressAndOpen();
    s.run(10000);
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    s.run(10u * 60000u);
    s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(41, s.eng.deadline().tableMinutes());
}

// ---- §5.2 flow 2: done -> acknowledge -> next fill --------------------------

// ITEM CASE: complete flow 2. The third press's recovery is measured from
// the VALVE-CLOSE instant (first trip), pinned ms-exact — the table alone is
// too coarse (20 min vs 19.97 min both interpolate to 38).
void test_flow2_done_ack_next_fill() {
    Sim s;
    const uint32_t tOpen = s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);
    tripAt(s, 1, 62u * 1000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Done);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::DonePulse);
    const uint32_t tClose = tOpen + 60000u;  // §7.8 reference instant
    const FillEngineOutputs& oAck = s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(oAck.acknowledged);
    TEST_ASSERT_TRUE(oAck.led == LedPatternEngine::Mode::Off);
    s.run(20u * 60000u);
    const uint32_t tPress3 = s.now;
    s.pass(ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_EQUAL_UINT32(tPress3 - tClose, s.eng.deadline().recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(38, s.eng.deadline().tableMinutes());  // 20-min row
}

// ---- fault paths (§7.5–§7.7) ------------------------------------------------

// ITEM CASE: confirm window times out -> fault the OTHER sensor (never
// tripped). Timeout inclusive at 12.000 s.
void test_confirm_timeout_faults_other_sensor() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);  // S1 trips; S2 stays dry
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.runToFillEnd((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: wet reading inside EARLY_WET_WINDOW_S -> guard faults that
// sensor immediately (false trip / short).
void test_early_wet_fault() {
    Sim s;
    s.pressAndOpen();
    s.a1 = WET;
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: trip ACCEPTED before 60 s (8th wet emission at 59.8 s) is a
// false trip -> EarlyTripFault (strict '<' window).
void test_early_trip_fault() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
}

// ITEM CASE: a trip accepted AT EXACTLY 60.000 s is LEGAL -> Confirm.
void test_trip_exactly_60s_legal() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60000u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
}

// ITEM CASE: backstop expires with no trip -> fault code 3; the valve was
// open for exactly the budget (fresh boot: 30 min).
void test_backstop_expiry_fault3() {
    Sim s;
    s.pressAndOpen();
    s.runToFillEnd(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BackstopExpired);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- +5 extension (§5.1/§6) --------------------------------------------------

// ITEM CASE: one +5 extends the fresh-boot 30 min to 35 — the fill now
// survives past 30 min but faults at 35.
void test_plus5_extends_backstop() {
    Sim s;
    s.pressAndOpen();
    const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::LongPress);
    TEST_ASSERT_TRUE(o.plus5Applied);
    TEST_ASSERT_TRUE(o.ackBlink);
    TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::Plus5);
    TEST_ASSERT_EQUAL_UINT32(35, o.logs[0].a);
    TEST_ASSERT_EQUAL_UINT32(35, s.eng.deadline().budgetMinutes());
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen);
    s.run(30u * 60000u);  // past the ORIGINAL deadline — still filling
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);
    s.run(5u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
}

// ITEM CASE: +5 stacks (every long release re-fires) and the edges are
// one-step pulses.
void test_plus5_stacks_edges_each_press() {
    Sim s;
    s.pressAndOpen();
    for (int i = 1; i <= 3; ++i) {
        const FillEngineOutputs& o = s.pass(ButtonClassifier::Event::LongPress);
        TEST_ASSERT_TRUE(o.plus5Applied && o.ackBlink);
        TEST_ASSERT_EQUAL_UINT32(30u + 5u * (uint32_t)i, s.eng.deadline().budgetMinutes());
    }
    const FillEngineOutputs& o2 = s.pass();
    TEST_ASSERT_FALSE(o2.plus5Applied || o2.ackBlink);
}

// ---- stream scope + fuzz -----------------------------------------------------

// ITEM CASE: readings are ignored outside a fill — wet sensors in IDLE never
// fault or log.
void test_readings_ignored_outside_fill_states() {
    Sim s;
    s.a1 = WET;
    s.a2 = WET;
    s.run(20000);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logCount);
}

// ITEM CASE: seeded fuzz over random presses and wet/dry scripts — the
// structural laws hold at EVERY step of every sequence:
//   A) valve level high only in FILLING
//   B) §7.9: every Filling/Confirm exit logs ValveClosed FIRST
//   C) FILLING is entered only via Idle+ShortPress (human press)
//   D) valve high implies a human start occurred earlier
//   E) FAULT latches (no button exit)
//   F) log capacity respected
void test_fuzz_lifecycle_invariants() {
    const uint32_t SEED_BASE = 0xF11EC0DEu;
    const int SEQUENCES = 64;
    const int PASSES = 3000;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;
        Sim s;
        bool humanStart = false;
        for (int i = 0; i < PASSES; ++i) {
            const uint32_t r = rngStep(rng) % 1000u;
            ButtonClassifier::Event ev = ButtonClassifier::Event::None;
            if (r < 25u) ev = ButtonClassifier::Event::ShortPress;
            else if (r < 50u) ev = ButtonClassifier::Event::LongPress;
            s.a1 = (rngStep(rng) % 100u < 70u) ? DRY : WET;
            s.a2 = (rngStep(rng) % 100u < 70u) ? DRY : WET;
            const FillState before = s.eng.state();
            const FillEngineOutputs o = s.pass(ev);
            const FillState after = s.eng.state();
            TEST_ASSERT_TRUE(!o.valveOpen || after == FillState::Filling);              // A
            if (before != after &&
                (before == FillState::Filling || before == FillState::Confirm)) {       // B
                TEST_ASSERT_TRUE(o.logCount >= 1);
                TEST_ASSERT_TRUE(o.logs[0].kind == FillLogEvent::Kind::ValveClosed);
            }
            if (after == FillState::Filling && before != FillState::Filling) {          // C
                TEST_ASSERT_TRUE(before == FillState::Idle &&
                                 ev == ButtonClassifier::Event::ShortPress);
                humanStart = true;
            }
            if (o.valveOpen) TEST_ASSERT_TRUE(humanStart);                              // D
            if (before == FillState::Fault) TEST_ASSERT_TRUE(after == FillState::Fault);// E
            TEST_ASSERT_TRUE(o.logCount <= FILL_LOG_CAPACITY);                          // F
        }
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_boot_idle_all_off);
    RUN_TEST(test_press_starts_fill_backstop30_led_solid_valve_still_closed);
    RUN_TEST(test_baseline_ok_opens_valve_and_anchors_deadline);
    RUN_TEST(test_baseline_fault_s1_refuses_valve);
    RUN_TEST(test_baseline_fault_s2_refuses_valve);
    RUN_TEST(test_happy_path_s1_first);
    RUN_TEST(test_happy_path_s2_first);
    RUN_TEST(test_double_trip_same_step_done);
    RUN_TEST(test_flow1_abort_in_settle_restarts_recovery);
    RUN_TEST(test_flow1_abort_midfill_recomputes);
    RUN_TEST(test_flow2_done_ack_next_fill);
    RUN_TEST(test_confirm_timeout_faults_other_sensor);
    RUN_TEST(test_early_wet_fault);
    RUN_TEST(test_early_trip_fault);
    RUN_TEST(test_trip_exactly_60s_legal);
    RUN_TEST(test_backstop_expiry_fault3);
    RUN_TEST(test_plus5_extends_backstop);
    RUN_TEST(test_plus5_stacks_edges_each_press);
    RUN_TEST(test_readings_ignored_outside_fill_states);
    RUN_TEST(test_fuzz_lifecycle_invariants);
    return UNITY_END();
}
