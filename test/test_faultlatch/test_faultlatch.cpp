// test/test_faultlatch/test_faultlatch.cpp — native contract proof for the
// FAULT half of the fill engine (P1.M6.T1.S3): PRD §7's fault table closed
// end to end — every producer of codes 1/2/3 (baseline out of range, false-
// trip guards, confirm-window miss, backstop expiry), the LATCHING semantics
// (all button input ignored while faulted; the valve refuses to open; only
// re-construction = RESET/power cycle clears), the blink-code PRESENTATION
// handoff (FillEngineOutputs.led/.faultCode -> LedPatternEngine::setMode ->
// the §7 cadence n x 250/250 then 2000 off), and §5.3/§11's reset contract
// (fresh instance = RAM wipe: fault gone, tank history gone, next fill = the
// 30-min fresh-tank default, never auto-starts).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- deterministic loop simulator (mirrors test_fillengine's harness) ----
// 25 ms passes; reading emissions anchored at lastReadingMs=0 land on
// t == 0 (mod 100). A press at t=1000 => the baseline accepts pairs from
// t=3000 (inclusive 2000 ms settle), 5 pairs => the verdict fires at t=3400:
// valve opens (both plausible) or the baseline fault latches (out of range).
static constexpr uint16_t DRY     = 3500;  // inside BASELINE_MIN..MAX (2000..4050)
static constexpr uint16_t WET     = 2500;  // below DRY - TRIP_DROP_COUNTS (3150)
static constexpr uint16_t TOO_LOW  = 1000; // < BASELINE_MIN -> TooLow verdict
static constexpr uint16_t TOO_HIGH = 4200; // > BASELINE_MAX -> TooHigh verdict

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
    void readings(int n) {
        for (int i = 0; i < n; ++i) {
            const uint32_t target = lastReadingMs + READING_INTERVAL_MS;
            while (now <= target) pass();
        }
    }
    uint32_t pressAndOpen() {
        pass(ButtonClassifier::Event::ShortPress);
        while (!eng.valveOpen()) pass();
        return eng.valveOpenedAtMs();
    }
};

// Deterministic 32-bit LCG (house reproducibility contract — the seed IS the
// reproducibility contract; never <random>).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Make `sensor` (0/1) trip with the 8th consecutive wet emission landing at
// EXACTLY fillTime == tripFillMs (a multiple of 100; t_open always is).
static void tripAt(Sim& s, int sensor, uint32_t tripFillMs) {
    const uint32_t tOpen = s.eng.valveOpenedAtMs();
    TEST_ASSERT_EQUAL_UINT32(0, tOpen % READING_INTERVAL_MS);  // alignment sanity
    const uint32_t tEnd = tOpen + tripFillMs;
    while (s.lastReadingMs < tEnd - 8u * READING_INTERVAL_MS) s.pass();
    if (sensor == 0) s.a1 = WET; else s.a2 = WET;
    while (s.lastReadingMs < tEnd) s.readings(1);
}

// Fastest deterministic fault: out-of-range baseline on `sensor`, verdict at
// ~2.4 s after the press. (WET is baseline-PLAUSIBLE — never use it for fast
// fault entry; see the file-header gotcha in the PRP.)
static void faultBaseline(Sim& s, int sensor) {
    if (sensor == 0) s.a1 = TOO_LOW; else s.a2 = TOO_HIGH;
    s.pass(ButtonClassifier::Event::ShortPress);
    while (s.eng.state() == FillState::Filling) s.pass();
}

// ---- §7 fault table: every producer of every code -------------------------

// ITEM CASE: baseline out of range on sensor 1 -> code 1, valve NEVER opened
// (through the fault instant and 100 s of faulted life after — §7.1 "refuse
// to open"), ValveClosed logged FIRST (§7.9), FaultBlink + code 1 as levels.
void test_code1_baseline_s1_valve_never_opened() {
    Sim s;
    faultBaseline(s, 0);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(1, s.last.faultCode);   // level output
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);  // §7.9 first
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);                          // sensor 1
    bool everOpen = false;
    for (int i = 0; i < 4000; ++i) everOpen |= s.pass().valveOpen;  // 100 s faulted
    TEST_ASSERT_FALSE(everOpen);
}

// ITEM CASE: symmetric — baseline out of range on sensor 2 -> code 2.
void test_code2_baseline_s2() {
    Sim s;
    faultBaseline(s, 1);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(2, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BaselineFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);  // sensor 2
}

// ITEM CASE: sensor 1 trips first, sensor 2 NEVER trips in the confirm
// window -> code 2 (§7.5 "fault that sensor" = the one that never tripped).
void test_code2_confirm_miss_s2() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 60u * 1000u);                 // legal first trip (>= 60 s)
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.runToFillEnd((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);  // widen BEFORE x1000
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);  // sensor 2 never tripped
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: symmetric direction — sensor 2 trips first, sensor 1 never
// trips -> code 1 (completes the confirm-miss row of the §7 table).
void test_code1_confirm_miss_s1() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 60u * 1000u);                 // sensor 2 first
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Confirm);
    s.runToFillEnd((uint32_t)CONFIRM_WINDOW_S * 1000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::ConfirmTimeout);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);  // sensor 1 never tripped
}

// ITEM CASE: wet reading inside EARLY_WET_WINDOW_S on sensor 1 -> guard
// faults sensor 1 -> code 1 (§7.6 false trip / short).
void test_code1_early_wet_s1() {
    Sim s;
    s.pressAndOpen();
    s.a1 = WET;                                // set AFTER open: baseline stays dry
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ITEM CASE: symmetric — early wet on sensor 2 -> code 2.
void test_code2_early_wet_s2() {
    Sim s;
    s.pressAndOpen();
    s.a2 = WET;
    while (s.eng.state() == FillState::Filling) s.pass();
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyWetFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: trip accepted at 59.8 s (< TRIP_MIN_ELAPSED_S) on sensor 1 ->
// false trip -> code 1 (guards run BEFORE the trip check: fault, not CONFIRM).
void test_code1_early_trip_s1() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 0, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(1, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
    TEST_ASSERT_EQUAL_UINT8(0, s.last.logs[1].sensor);
}

// ITEM CASE: symmetric — early trip on sensor 2 -> code 2.
void test_code2_early_trip_s2() {
    Sim s;
    s.pressAndOpen();
    tripAt(s, 1, 59800u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(2, s.eng.faultCode());
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::EarlyTripFault);
    TEST_ASSERT_EQUAL_UINT8(1, s.last.logs[1].sensor);
}

// ITEM CASE: backstop expires with no trip -> code 3 ("timer, no trip"),
// ValveClosed first, valve level low, FaultBlink + 3 as levels. 30-min sim
// (~72k passes — sub-second natively).
void test_code3_backstop_expiry() {
    Sim s;
    s.pressAndOpen();
    s.runToFillEnd(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, s.eng.faultCode());
    TEST_ASSERT_EQUAL_UINT8(3, s.last.faultCode);
    TEST_ASSERT_TRUE(s.last.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_TRUE(s.last.logs[0].kind == FillLogEvent::Kind::ValveClosed);
    TEST_ASSERT_TRUE(s.last.logs[1].kind == FillLogEvent::Kind::BackstopExpired);
    TEST_ASSERT_FALSE(s.last.valveOpen);
}

// ---- latching: FAULT absorbs ALL input (§7 "BOOT is ignored… valve
//      refuses to open") -----------------------------------------------------

// ITEM CASE: the COMPLETE ButtonClassifier event vocabulary, cycled across a
// 3-minute faulted soak with hostile wet sensor streams — state, code, LED
// mode, valve level, log silence and edge silence frozen on EVERY pass.
void test_fault_ignores_every_button_event_forever() {
    Sim s;
    faultBaseline(s, 0);
    const uint8_t code = s.eng.faultCode();
    static const ButtonClassifier::Event kEvents[] = {
        ButtonClassifier::Event::None,
        ButtonClassifier::Event::ShortPress,
        ButtonClassifier::Event::LongPress,
    };
    s.a1 = WET;  s.a2 = WET;  // hostile: wet sensors while faulted
    for (int i = 0; i < 7200; ++i) {          // 180 s at 25 ms
        const FillEngineOutputs& o = s.pass(kEvents[i % 3]);
        TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);   // latched
        TEST_ASSERT_EQUAL_UINT8(code, s.eng.faultCode());      // code kept
        TEST_ASSERT_EQUAL_UINT8(code, o.faultCode);            // level out
        TEST_ASSERT_FALSE(o.valveOpen);                        // refuses to open
        TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
        TEST_ASSERT_EQUAL_UINT8(0, o.logCount);                // fully silent
        TEST_ASSERT_FALSE(o.plus5Applied || o.acknowledged || o.ackBlink);
    }
}

// ---- blink-code presentation: the §7 cadence via the T2.S1 wiring ---------

// Drive a fresh LedPatternEngine EXACTLY as P1.M6.T2.S1 will (setMode(o.led,
// o.faultCode) ONCE, then tick every pass) and verify PRD §7's cadence
// ms-exactly: N x (on 250 / off 250), then off 2000, repeat forever. The
// pattern phase anchors at the FIRST tick after setMode (engine contract).
static void checkBlinkCadence(Sim& s) {
    const FillEngineOutputs& o = s.pass();     // steady faulted levels
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    const uint8_t n = o.faultCode;
    TEST_ASSERT_TRUE(n >= 1 && n <= 3);        // §7 table domain
    LedPatternEngine led;
    led.setMode(o.led, o.faultCode);           // the wiring under proof
    TEST_ASSERT_TRUE(led.mode() == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_EQUAL_UINT8(n, led.faultGroups());
    const uint32_t t0 = 5000;                  // first tick anchors the phase
    TEST_ASSERT_TRUE(led.tick(t0));            // anchor lands ON (blink 1)
    const uint32_t cycle = (uint32_t)n * 2u * FAULT_BLINK_MS + FAULT_PAUSE_MS;
    for (uint32_t cyc = 0; cyc < 2u; ++cyc) {
        const uint32_t base = t0 + cyc * cycle;
        for (uint8_t k = 0; k < n; ++k) {      // blink k of this cycle
            const uint32_t blinkAt = base + (uint32_t)k * 2u * FAULT_BLINK_MS;
            TEST_ASSERT_TRUE(led.tick(blinkAt + 200));                    // mid-on
            TEST_ASSERT_FALSE(led.tick(blinkAt + FAULT_BLINK_MS + 25));   // just off
            TEST_ASSERT_FALSE(led.tick(blinkAt + 2u * FAULT_BLINK_MS - 25)); // end off
        }
        const uint32_t pauseAt = base + (uint32_t)n * 2u * FAULT_BLINK_MS;
        TEST_ASSERT_FALSE(led.tick(pauseAt + 1000));                      // mid-gap
        TEST_ASSERT_FALSE(led.tick(pauseAt + FAULT_PAUSE_MS - 25));       // gap end
    }
    TEST_ASSERT_TRUE(led.tick(t0 + 2u * cycle));  // cycle 3 restarts: ON
}

// ITEM CASE: codes 1, 2 and 3 each render the §7 cadence through the engine
// outputs — code 3 via the (slow but native-cheap) backstop path.
void test_fault_blink_cadence_codes_1_2_3() {
    Sim sa;
    faultBaseline(sa, 0);
    TEST_ASSERT_EQUAL_UINT8(1, sa.eng.faultCode());
    checkBlinkCadence(sa);
    Sim sb;
    faultBaseline(sb, 1);
    TEST_ASSERT_EQUAL_UINT8(2, sb.eng.faultCode());
    checkBlinkCadence(sb);
    Sim sc;
    sc.pressAndOpen();
    sc.run(30u * 60000u + 100u);
    TEST_ASSERT_TRUE(sc.eng.state() == FillState::Fault);
    TEST_ASSERT_EQUAL_UINT8(3, sc.eng.faultCode());
    checkBlinkCadence(sc);
}

// ---- §5.3 / §11: RESET & power cycle == re-construction --------------------

// ITEM CASE: a faulted engine WITH tank history is replaced by a fresh
// instance (wall clock continues): IDLE, fault cleared, history WIPED — the
// next fill uses the 30-min fresh-tank default, NOT the recovery-interpolated
// backstop (~1-min recovery would give 44; 30 is reachable ONLY fresh). The
// fresh instance also NEVER auto-starts: 5 minutes of hostile idle, no press.
void test_reset_clears_fault_and_tank_history() {
    Sim s;
    s.pass(ButtonClassifier::Event::ShortPress);  // fill 1 starts (settle)
    s.pass(ButtonClassifier::Event::ShortPress);  // abort: records history
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory()); // RAM history exists
    s.run(60000);                                 // 1 min of recovery
    faultBaseline(s, 0);                          // fill 2 -> fault, code 1
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
    TEST_ASSERT_TRUE(s.eng.clock().hasHistory());

    Sim t;                       // POWER CYCLE: fresh engine, clock continues
    t.now = s.now;
    const FillEngineOutputs& o0 = t.pass();
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);  // §5.3: lands IDLE
    TEST_ASSERT_FALSE(o0.valveOpen);                    // valve closed
    TEST_ASSERT_EQUAL_UINT8(0, o0.faultCode);           // fault cleared
    TEST_ASSERT_TRUE(o0.led == LedPatternEngine::Mode::Off);
    t.a1 = WET;  t.a2 = WET;                            // hostile idle streams
    t.run(5u * 60000u);                                 // 5 min, no press
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle); // never auto-starts
    TEST_ASSERT_FALSE(t.last.valveOpen);
    TEST_ASSERT_FALSE(t.eng.clock().hasHistory());      // §5.3: history wiped
    t.a1 = DRY;  t.a2 = DRY;
    t.pass(ButtonClassifier::Event::ShortPress);        // the next fill
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Filling);
    TEST_ASSERT_TRUE(t.eng.deadline().freshBoot());
    TEST_ASSERT_EQUAL_UINT32(FRESH_BOOT_BACKSTOP_MIN, t.eng.deadline().tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(30, t.eng.deadline().tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(0, t.eng.deadline().recoveryMsAtStart());
}

// ---- fuzz: the latch absorbs arbitrary input; reset always lands IDLE -----

// ITEM CASE: seeded fuzz — from a latched fault, NO random input sequence
// (button events + wet/dry sensor scripts, 2000 passes) can leave FAULT,
// open the valve, change the code, or emit a single log event; a fresh
// instance then starts IDLE with the valve closed. Complements
// test_fillengine's fuzz law E (fault entry preserved) with ABSORPTION.
// Entry is deterministic-fast (baseline fault; random entry scripts mostly
// abort/restart or run to a 30-min backstop — entry variety is tests 1–9).
void test_fuzz_fault_absorbs_all_input() {
    const uint32_t SEED_BASE = 0xFA17C0DEu;
    const int SEQUENCES = 8;
    const int SOAK = 2000;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;
        Sim s;
        faultBaseline(s, seq % 2);            // codes 1 and 2 across seeds
        const uint8_t code = s.eng.faultCode();
        TEST_ASSERT_TRUE(code == 1 || code == 2);
        for (int i = 0; i < SOAK; ++i) {
            const uint32_t r = rngStep(rng) % 1000u;
            ButtonClassifier::Event ev = ButtonClassifier::Event::None;
            if (r < 150u) ev = ButtonClassifier::Event::ShortPress;
            else if (r < 300u) ev = ButtonClassifier::Event::LongPress;
            s.a1 = (rngStep(rng) % 100u < 50u) ? WET : DRY;
            s.a2 = (rngStep(rng) % 100u < 50u) ? WET : DRY;
            const FillEngineOutputs o = s.pass(ev);
            TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);  // latched
            TEST_ASSERT_EQUAL_UINT8(code, o.faultCode);           // code frozen
            TEST_ASSERT_FALSE(o.valveOpen);                       // refuses
            TEST_ASSERT_EQUAL_UINT8(0, o.logCount);               // silent
        }
        Sim t;                                 // RESET: re-construction
        t.now = s.now;
        TEST_ASSERT_FALSE(t.pass().valveOpen);
        TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_code1_baseline_s1_valve_never_opened);
    RUN_TEST(test_code2_baseline_s2);
    RUN_TEST(test_code2_confirm_miss_s2);
    RUN_TEST(test_code1_confirm_miss_s1);
    RUN_TEST(test_code1_early_wet_s1);
    RUN_TEST(test_code2_early_wet_s2);
    RUN_TEST(test_code1_early_trip_s1);
    RUN_TEST(test_code2_early_trip_s2);
    RUN_TEST(test_code3_backstop_expiry);
    RUN_TEST(test_fault_ignores_every_button_event_forever);
    RUN_TEST(test_fault_blink_cadence_codes_1_2_3);
    RUN_TEST(test_reset_clears_fault_and_tank_history);
    RUN_TEST(test_fuzz_fault_absorbs_all_input);
    return UNITY_END();
}
