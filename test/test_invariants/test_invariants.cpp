// test/test_invariants/test_invariants.cpp — the §10 safety-invariant suite
// (P1.M7.T1.S1). Machine-proves, over seeded randomized event+sensor storms
// and fresh-instance (reset-proxy) scenarios against the REAL FillEngine:
//   P1  valveOpen HIGH only in FILLING (never Idle/Confirm/Done/Fault)
//   P2  §7.9: every Filling/Confirm exit step logs ValveClosed FIRST and
//       drives the valve LOW on that same step
//   P3  FILLING is entered ONLY via Idle+ShortPress (human BOOT press)
//   P4  valve HIGH anywhere implies a human start happened earlier
//   P5  FAULT latches and the valve refuses to open while faulted (§7)
//   P6  log capacity respected; output level == engine level
// plus: fresh engine = Idle/valve-LOW/no logs/freshBoot (§5.3 reset proxy,
// watchdog included), no-ShortPress storms NEVER fill, clock-wraparound
// safety (rollover-safe arithmetic, §10), and mid-fill reset → Idle+closed
// until a NEW press. Adversarial event streams (events possibly every pass)
// are a deliberate SUPERSET of the legal classifier output — the invariants
// must hold regardless. Backstop hugging (§10e) is P1.M4.T1.S1's table
// tests; bench watchdog/power proof is P1.M7.T2.S1's.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Same calibration language as test_fillengine.cpp: DRY=3500 is inside
// BASELINE_MIN..MAX (2000..4050); WET=2500 is below the minimum legal wet
// threshold (2000-350), so a wet emission always feeds detector streaks and
// the early-wet guard. Wild values exercise baseline faults and the rails.
static constexpr uint16_t DRY = 3500;
static constexpr uint16_t WET = 2500;

// Deterministic 32-bit LCG (Numerical Recipes constants) — house precedent
// (test_fillengine.cpp): bit-reproducible on any host g++; the seed IS the
// reproducibility contract.
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// Adversarial sensor level: mostly dry, often wet, occasionally wild
// (baseline-breaking / rail values — forces BaselineFault paths too).
static uint16_t randomLevel(uint32_t& rng) {
    const uint32_t r = rngStep(rng) % 100u;
    if (r < 75u) return DRY;
    if (r < 92u) return WET;
    static const uint16_t wild[] = {0, 100, 1999, 2000, 4050, 4051, 4095};
    return wild[rngStep(rng) % 7u];
}

static ButtonClassifier::Event randomEvent(uint32_t& rng, bool allowShortPress) {
    const uint32_t r = rngStep(rng) % 1000u;
    if (allowShortPress && r < 55u) return ButtonClassifier::Event::ShortPress;
    if (r < 110u) return ButtonClassifier::Event::LongPress;
    return ButtonClassifier::Event::None;
}

// Property-harness: the scenario Sim adapted for property work —
// configurable start time (wraparound families) and a RANDOM per-pass
// stride (5..55 ms), so fills do NOT open on the reading grid and the
// invariants are proven order-of-event, off-grid. readingDue follows the
// loop contract: re-anchor + snapshot the emitted pair at emission.
struct PropSim {
    FillEngine eng;
    FillEngineOutputs last{};
    uint32_t now;
    uint32_t lastReadingMs;
    uint16_t a1 = DRY, a2 = DRY;  // scripted sensor levels
    uint16_t e1 = DRY, e2 = DRY;  // last EMITTED pair (what the loop routes)
    explicit PropSim(uint32_t start = 1000) : now(start), lastReadingMs(start) {}
    const FillEngineOutputs& pass(ButtonClassifier::Event ev, uint32_t dt) {
        const bool due = elapsedMs(now, lastReadingMs) >= READING_INTERVAL_MS;
        if (due) { lastReadingMs = now; e1 = a1; e2 = a2; }
        last = eng.step(now, ev, due, e1, e2);
        now += dt;
        return last;
    }
};

// The shared oracle: run one seeded storm and assert P1..P6 at EVERY step.
static void runStorm(uint32_t seed, int passes, uint32_t startNow,
                     bool allowShortPress) {
    uint32_t rng = seed;
    PropSim s(startNow);
    bool humanStart = false;
    for (int i = 0; i < passes; ++i) {
        const ButtonClassifier::Event ev = randomEvent(rng, allowShortPress);
        s.a1 = randomLevel(rng);
        s.a2 = randomLevel(rng);
        const uint32_t dt = 5u + rngStep(rng) % 51u;  // 5..55 ms stride
        const FillState before = s.eng.state();
        const FillEngineOutputs o = s.pass(ev, dt);
        const FillState after = s.eng.state();
        TEST_ASSERT_TRUE_MESSAGE(!o.valveOpen || after == FillState::Filling,
                                 "P1 valve HIGH outside FILLING");
        if (before != after &&
            (before == FillState::Filling || before == FillState::Confirm)) {
            TEST_ASSERT_TRUE_MESSAGE(o.logCount >= 1, "P2 exit with empty log");
            TEST_ASSERT_TRUE_MESSAGE(
                o.logs[0].kind == FillLogEvent::Kind::ValveClosed,
                "P2 exit step logs[0] != ValveClosed (§7.9)");
            TEST_ASSERT_FALSE_MESSAGE(o.valveOpen, "P2 valve HIGH on exit step");
        }
        if (after == FillState::Filling && before != FillState::Filling) {
            TEST_ASSERT_TRUE_MESSAGE(before == FillState::Idle &&
                                     ev == ButtonClassifier::Event::ShortPress,
                "P3 FILLING entered without Idle+ShortPress (auto-open)");
            humanStart = true;
        }
        if (o.valveOpen) TEST_ASSERT_TRUE_MESSAGE(humanStart, "P4 valve without human start");
        if (before == FillState::Fault) {
            TEST_ASSERT_TRUE_MESSAGE(after == FillState::Fault, "P5 FAULT unlatched");
            TEST_ASSERT_FALSE_MESSAGE(o.valveOpen, "P5 valve open while faulted");
        }
        TEST_ASSERT_TRUE_MESSAGE(o.logCount <= FILL_LOG_CAPACITY, "P6 log overflow");
        TEST_ASSERT_EQUAL_MESSAGE(s.eng.valveOpen(), o.valveOpen,
                                  "P6 output/engine valve level disagree");
    }
}

// ---- fresh instance = safe state (watchdog reset / power cycle, §5.3/§10)

void test_fresh_engine_boots_idle_valve_low() {
    PropSim s;
    const FillEngineOutputs o = s.pass(ButtonClassifier::Event::None, 25);
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_FALSE(s.eng.valveOpen());
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_EQUAL_UINT8(0, o.faultCode);
    TEST_ASSERT_EQUAL_UINT8(0, o.logCount);
    TEST_ASSERT_TRUE(s.eng.deadline().freshBoot());  // §5.3: reset clears tank history
}

// §10/§5.3: with NO human BOOT press there is NO fill — whatever the
// sensors do. Long presses and wet storms never open the valve.
void test_no_shortpress_never_opens_valve() {
    for (int seq = 0; seq < 16; ++seq)
        runStorm(0xC0FFEEu + (uint32_t)seq * 40503u, 4000, 1000, false);
}

// The core storms: full adversarial event+sensor mix, thousands of steps.
void test_random_storms_invariants() {
    for (int seq = 0; seq < 48; ++seq)
        runStorm(0xF11EC0DEu + (uint32_t)seq * 2654435761u, 5000, 1000, true);
}

// Same properties with the clock starting near UINT32_MAX — the sequence
// crosses the millis() wrap: rollover-safe arithmetic must keep every
// invariant (§10). A failure here is a REAL engine bug — report it.
void test_wraparound_storms_invariants() {
    for (int seq = 0; seq < 8; ++seq)
        runStorm(0x0BADC0DEu + (uint32_t)seq * 2246822519u, 4000,
                 0xFFFFFF00u - (uint32_t)seq * 1000u, true);
}

// §7: faults latch; BOOT is ignored while faulted; the valve refuses to
// open. Storm the latched state with press bursts — nothing may give.
void test_fault_latch_valve_refuses() {
    PropSim s;
    s.pass(ButtonClassifier::Event::ShortPress, 25);
    s.a1 = 100; s.a2 = 100;  // baseline out of range -> BaselineFault latches
    while (s.eng.state() != FillState::Fault) s.pass(ButtonClassifier::Event::None, 25);
    for (int i = 0; i < 2000; ++i) {
        const FillEngineOutputs o = s.pass(
            (i % 7 == 0) ? ButtonClassifier::Event::ShortPress
                         : ButtonClassifier::Event::None, 25);
        TEST_ASSERT_TRUE(s.eng.state() == FillState::Fault);
        TEST_ASSERT_FALSE(o.valveOpen);
        TEST_ASSERT_TRUE(o.faultCode >= 1 && o.faultCode <= 3);
    }
}

// §10 hang->reset path, native proxy: a fill is running with the valve OPEN,
// the device "resets" (fresh engine, RAM state gone) mid-stream — the fresh
// engine is Idle+closed under a WET adversarial stream, tank history is
// cleared (freshBoot), and only a NEW human press starts the next fill.
void test_reset_midfill_lands_idle_closed() {
    PropSim s;
    s.pass(ButtonClassifier::Event::ShortPress, 25);
    for (int i = 0; i < 400 && s.eng.state() == FillState::Filling; ++i)
        s.pass(ButtonClassifier::Event::None, 25);  // dry: settle -> baseline -> open
    TEST_ASSERT_TRUE(s.eng.state() == FillState::Filling);  // pre: fill alive
    TEST_ASSERT_TRUE(s.eng.valveOpen());                   // pre: valve OPEN at the "hang"
    PropSim t(s.now);              // fresh instance = the reset
    t.a1 = WET; t.a2 = WET;        // adversarial: wet tips at reboot
    for (int i = 0; i < 200; ++i) {
        const FillEngineOutputs o = t.pass(ButtonClassifier::Event::None, 25);
        TEST_ASSERT_TRUE(t.eng.state() == FillState::Idle);
        TEST_ASSERT_FALSE(o.valveOpen);
    }
    TEST_ASSERT_TRUE(t.eng.deadline().freshBoot());  // §5.3 fresh 30-min tank
    const FillEngineOutputs o2 = t.pass(ButtonClassifier::Event::ShortPress, 25);
    TEST_ASSERT_TRUE(t.eng.state() == FillState::Filling);  // press still works
    TEST_ASSERT_FALSE(o2.valveOpen);  // §7.1: settle first — still closed this step
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_engine_boots_idle_valve_low);
    RUN_TEST(test_no_shortpress_never_opens_valve);
    RUN_TEST(test_random_storms_invariants);
    RUN_TEST(test_wraparound_storms_invariants);
    RUN_TEST(test_fault_latch_valve_refuses);
    RUN_TEST(test_reset_midfill_lands_idle_closed);
    return UNITY_END();
}
