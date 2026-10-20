// test/test_fillcore/test_fillcore.cpp — native contract proof for the
// fill-state transition core (P1.M6.T1.S1). Pins PRD §5.1's BOOT-button
// table cell by cell (including every ignored cell), §5.2's normative flows
// (the lifecycle hop simulated by direct state assignment — the function is
// stateless and the caller owns the state, exactly how P1.M6.T1.S2 drives
// it), boot = Idle/valve closed (§5.3/§10), +5 stacking, and the §10 valve
// invariant as a seeded-fuzz property: no event sequence ever commands the
// valve open except via the human Idle+ShortPress cell.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- helpers ------------------------------------------------------------

// Seed a state, fire one event, capture the result state and ALL outputs.
static FillState runCell(FillState from, ButtonClassifier::Event ev, FillOutputs& out) {
    FillState st = from;
    out = fillApplyButton(st, ev);
    return st;
}

// Deterministic 32-bit LCG (Numerical Recipes constants) — seeded fuzz with
// zero dependencies; bit-reproducible on any host g++ (the seed IS the
// reproducibility contract).
static uint32_t rngStep(uint32_t& s) { return s = s * 1664525u + 1013904223u; }

// ---- boot / initial state (PRD §5.3, §10) --------------------------------

// ITEM CASE: boot initial state = Idle with valveOpen=false. The core is
// stateless; "boot" is the caller's initialization — the only legal initial
// value. A reset is re-initialization (RAM-only model, cf. RecoveryClock).
void test_boot_is_idle_valve_closed() {
    FillState st = FillState::Idle;                       // boot line (PRD §5.3)
    FillOutputs out = fillLevelOutputs(st);
    TEST_ASSERT_TRUE(out.valveOpen == false);             // §10: never auto-open
    TEST_ASSERT_TRUE(out.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(out.plus5Applied);
    TEST_ASSERT_FALSE(out.acknowledged);
    // And a None event on a fresh boot changes nothing:
    FillOutputs o2;
    TEST_ASSERT_TRUE(runCell(FillState::Idle, ButtonClassifier::Event::None, o2) == FillState::Idle);
    TEST_ASSERT_FALSE(o2.valveOpen);
}

// ---- §5.1 table, one test per cell (all five outputs pinned each) --------

// IDLE + Short: START FILL -> FILLING, valve level high, Solid, no edges.
void test_idle_short_starts_fill() {
    FillOutputs o;
    const FillState next = runCell(FillState::Idle, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen == true);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// IDLE + Long: nothing (PRD §5.1) — "abort long-press does nothing in Idle".
void test_idle_long_nothing() {
    FillOutputs o;
    const FillState next = runCell(FillState::Idle, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FILLING + Short: ABORT — valve closed -> IDLE.
void test_filling_short_aborts() {
    FillOutputs o;
    const FillState next = runCell(FillState::Filling, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FILLING + Long: +5 applied, STAYS Filling (valve level unchanged), LED ack
// is downstream (plus5Applied flag -> triggerAck in P1.M6.T1.S2).
void test_filling_long_plus5_stays_filling() {
    FillOutputs o;
    const FillState next = runCell(FillState::Filling, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Filling);
    TEST_ASSERT_TRUE(o.valveOpen == true);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_TRUE(o.plus5Applied == true);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// CONFIRM + Short: ignored — state kept, SOLID level held (PRD §4
// "solid on (continues)"), valve closed, no edges.
void test_confirm_short_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Confirm, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Confirm);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// CONFIRM + Long: ignored.
void test_confirm_long_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Confirm, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Confirm);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// DONE + Short: ACKNOWLEDGE — light off -> IDLE (led level Off), ack edge.
void test_done_short_acknowledges() {
    FillOutputs o;
    const FillState next = runCell(FillState::Done, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_TRUE(o.acknowledged == true);
}

// DONE + Long: "also acknowledges" (PRD §5.1) — identical outcome to short.
void test_done_long_also_acknowledges() {
    FillOutputs o;
    const FillState next = runCell(FillState::Done, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Idle);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Off);
    TEST_ASSERT_TRUE(o.acknowledged == true);
}

// FAULT + Short: ignored — latched, FaultBlink level held (group count is
// P1.M6.T1.S3's), valve closed, no edges.
void test_fault_short_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Fault, ButtonClassifier::Event::ShortPress, o);
    TEST_ASSERT_TRUE(next == FillState::Fault);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// FAULT + Long: ignored.
void test_fault_long_ignored() {
    FillOutputs o;
    const FillState next = runCell(FillState::Fault, ButtonClassifier::Event::LongPress, o);
    TEST_ASSERT_TRUE(next == FillState::Fault);
    TEST_ASSERT_FALSE(o.valveOpen);
    TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::FaultBlink);
    TEST_ASSERT_FALSE(o.plus5Applied);
    TEST_ASSERT_FALSE(o.acknowledged);
}

// ---- totality & stacking --------------------------------------------------

// Event::None is a no-op in EVERY state (classifier's steady-state return).
void test_none_noop_all_states() {
    const FillState states[5] = {FillState::Idle, FillState::Filling, FillState::Confirm,
                                 FillState::Done, FillState::Fault};
    for (int i = 0; i < 5; ++i) {
        FillOutputs o;
        const FillState next = runCell(states[i], ButtonClassifier::Event::None, o);
        TEST_ASSERT_TRUE(next == states[i]);
        TEST_ASSERT_TRUE(o.valveOpen == (states[i] == FillState::Filling));
        TEST_ASSERT_FALSE(o.plus5Applied);
        TEST_ASSERT_FALSE(o.acknowledged);
    }
}

// +5 stacks / is repeatable (PRD §5.1): every long release re-fires the
// flag; state, valve level, LED all stay FILLING throughout.
void test_plus5_stacks_repeatable() {
    FillState st = FillState::Idle;
    (void)fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    for (int i = 0; i < 3; ++i) {
        FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::LongPress);
        TEST_ASSERT_TRUE(o.plus5Applied == true);          // one edge per press
        TEST_ASSERT_TRUE(st == FillState::Filling);
        TEST_ASSERT_TRUE(o.valveOpen == true);
        TEST_ASSERT_TRUE(o.led == LedPatternEngine::Mode::Solid);
    }
}

// §10 "no path": from CONFIRM/DONE/FAULT no button event (any of the three)
// ever reaches FILLING or commands the valve open.
void test_no_button_path_reenters_filling() {
    const FillState locked[3] = {FillState::Confirm, FillState::Done, FillState::Fault};
    const ButtonClassifier::Event events[3] = {ButtonClassifier::Event::None,
                                               ButtonClassifier::Event::ShortPress,
                                               ButtonClassifier::Event::LongPress};
    for (int s = 0; s < 3; ++s) {
        for (int e = 0; e < 3; ++e) {
            FillOutputs o;
            const FillState next = runCell(locked[s], events[e], o);
            TEST_ASSERT_TRUE(next != FillState::Filling);
            TEST_ASSERT_FALSE(o.valveOpen);
        }
    }
}

// ---- §5.2 normative flows --------------------------------------------------

// Flow 1 (abort): press -> fill; press -> stops; press -> fills again. (The
// "fresh, recomputed timer" is P1.M6.T1.S2's FillDeadline wiring — out of
// scope here; the STATE story is complete in the table.)
void test_flow1_abort_sequence() {
    FillState st = FillState::Idle;
    FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // abort mid-fill
    TEST_ASSERT_TRUE(st == FillState::Idle && !o.valveOpen);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // next fill
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
}

// Flow 2 (success): press -> fill; fill completes (lifecycle hop — simulated
// by direct state assignment, exactly how P1.M6.T1.S2 will drive it); press
// -> blinking stops (ack -> Idle); press -> next fill starts.
void test_flow2_success_sequence_with_lifecycle_hop() {
    FillState st = FillState::Idle;
    (void)fillApplyButton(st, ButtonClassifier::Event::ShortPress);
    TEST_ASSERT_TRUE(st == FillState::Filling);
    st = FillState::Done;  // P1.M6.T1.S2: second sensor confirmed -> DONE
    TEST_ASSERT_TRUE(fillLevelOutputs(st).led == LedPatternEngine::Mode::DonePulse);
    FillOutputs o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // acknowledge
    TEST_ASSERT_TRUE(st == FillState::Idle && o.acknowledged &&
                    o.led == LedPatternEngine::Mode::Off);
    o = fillApplyButton(st, ButtonClassifier::Event::ShortPress);   // next fill
    TEST_ASSERT_TRUE(st == FillState::Filling && o.valveOpen);
}

// ---- §10 valve invariant as a seeded-fuzz property -------------------------

// Property: no event sequence ever yields valveOpen=true except via the
// Idle+ShortPress transition. Asserted at EVERY step of every sequence:
//   A) out.valveOpen == (state == Filling)          — valve high only in FILLING
//   B) Filling is ENTERED only by Idle+ShortPress   — the human-press cell
//   C) out.valveOpen => an Idle+ShortPress happened earlier in the sequence
//   D) plus5Applied => (prior Filling AND LongPress)  [edge sanity]
//   E) acknowledged => (prior Done AND event != None) [edge sanity]
void test_fuzz_valve_only_via_idle_short() {
    const uint32_t SEED_BASE = 0x600DF00Du;
    const int SEQUENCES = 512;
    const int MAX_EVENTS = 48;
    for (int seq = 0; seq < SEQUENCES; ++seq) {
        uint32_t rng = SEED_BASE + (uint32_t)seq * 2654435761u;  // per-sequence seed
        FillState st = FillState::Idle;                          // boot (PRD §5.3)
        bool humanStartOccurred = false;
        const int n = 1 + (int)(rngStep(rng) % (uint32_t)MAX_EVENTS);
        for (int i = 0; i < n; ++i) {
            const uint32_t r = rngStep(rng) % 3u;
            const ButtonClassifier::Event ev =
                (r == 0u) ? ButtonClassifier::Event::None
              : (r == 1u) ? ButtonClassifier::Event::ShortPress
                          : ButtonClassifier::Event::LongPress;
            const FillState before = st;
            const FillOutputs o = fillApplyButton(st, ev);
            TEST_ASSERT_TRUE(o.valveOpen == (st == FillState::Filling));      // A
            if (before == FillState::Idle && ev == ButtonClassifier::Event::ShortPress) {
                humanStartOccurred = true;
                TEST_ASSERT_TRUE(st == FillState::Filling);                   // B (entry)
            } else {
                TEST_ASSERT_TRUE(st != FillState::Filling || before == FillState::Filling);  // B (no other entry)
            }
            if (o.valveOpen) TEST_ASSERT_TRUE(humanStartOccurred);            // C
            if (o.plus5Applied) {
                TEST_ASSERT_TRUE(before == FillState::Filling &&
                                 ev == ButtonClassifier::Event::LongPress);   // D
            }
            if (o.acknowledged) {
                TEST_ASSERT_TRUE(before == FillState::Done &&
                                 ev != ButtonClassifier::Event::None);        // E
            }
        }
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_boot_is_idle_valve_closed);
    RUN_TEST(test_idle_short_starts_fill);
    RUN_TEST(test_idle_long_nothing);
    RUN_TEST(test_filling_short_aborts);
    RUN_TEST(test_filling_long_plus5_stays_filling);
    RUN_TEST(test_confirm_short_ignored);
    RUN_TEST(test_confirm_long_ignored);
    RUN_TEST(test_done_short_acknowledges);
    RUN_TEST(test_done_long_also_acknowledges);
    RUN_TEST(test_fault_short_ignored);
    RUN_TEST(test_fault_long_ignored);
    RUN_TEST(test_none_noop_all_states);
    RUN_TEST(test_plus5_stacks_repeatable);
    RUN_TEST(test_no_button_path_reenters_filling);
    RUN_TEST(test_flow1_abort_sequence);
    RUN_TEST(test_flow2_success_sequence_with_lifecycle_hop);
    RUN_TEST(test_fuzz_valve_only_via_idle_short);
    return UNITY_END();
}
