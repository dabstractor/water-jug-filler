// test/test_deadline/test_deadline.cpp — native contract proof for
// RecoveryClock + FillDeadline (P1.M4.T2.S1). Pins PRD §6.1/§6.2/§6 preamble:
// fresh-boot 30-min default; recovery measured from the previous fill's
// valve-close (abort restarts it identically); budget computed once at fill
// start via backstopMinutes(); deadline anchored at the VALVE-OPEN instant;
// +5 long-press stacks on the active fill only; wrap-safe arithmetic.
// Interpolated expectations derive from the frozen backstopMinutes contract
// (P1.M4.T1.S1): 15 min recovery -> 39 (exact 39.5 floored); 0 min -> 44.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ITEM CASE: fresh boot / any reset — no history -> FRESH_BOOT_BACKSTOP_MIN
// (30), recovery input reads 0, and the fresh-boot flag is exposed for the
// §3 backstop log line. (RAM-only state: a reset is just a new object.)
void test_fresh_boot_uses_30min_default() {
    RecoveryClock clock;      // fresh boot: no recordFillEnd ever called
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    TEST_ASSERT_TRUE(dl.freshBoot());
    TEST_ASSERT_EQUAL_UINT32(0, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(30, dl.tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(30, dl.budgetMinutes());
}

// ITEM CASE: history at 15 min recovery -> interpolated value. The item
// gloss says "~36-37, assert computed": the computed normative value from
// the frozen table is 39 (exact 39.5 on the 10->20 segment, floored by
// P1.M4.T1.S1). Assert BOTH the pinned number and the computed one.
void test_history_15min_recovery_interpolates() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    clock.recordFillEnd(fillEnd);
    TEST_ASSERT_TRUE(clock.hasHistory());

    FillDeadline dl;
    dl.beginFill(clock, fillEnd + 15u * 60u * 1000u);   // 15 min later
    TEST_ASSERT_FALSE(dl.freshBoot());
    TEST_ASSERT_EQUAL_UINT32(15u * 60u * 1000u, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(39, dl.tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(backstopMinutes(15), dl.tableMinutes());  // computed
}

// ITEM CASE: abort resets the reference at the abort instant (PRD §6.1):
// recovery for the next fill is measured from the ABORT, not from the
// earlier fill end.
void test_abort_restarts_reference_at_abort_instant() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    const uint32_t abortAt = fillEnd + 5u * 60u * 1000u;   // aborted 5 min in
    clock.recordFillEnd(fillEnd);
    clock.recordFillEnd(abortAt);   // abort == fill end, same call

    FillDeadline dl;
    dl.beginFill(clock, abortAt + 15u * 60u * 1000u);      // 15 min after abort
    TEST_ASSERT_EQUAL_UINT32(15u * 60u * 1000u, dl.recoveryMsAtStart());  // NOT 20 min
    TEST_ASSERT_EQUAL_UINT32(39, dl.tableMinutes());
}

// ITEM CASE: recovery 0 — immediate restart clamps to the table's first
// knot: backstopMinutes(0) == 44.
void test_zero_recovery_clamps_to_44() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    clock.recordFillEnd(fillEnd);
    FillDeadline dl;
    dl.beginFill(clock, fillEnd);   // restarted at the same instant
    TEST_ASSERT_EQUAL_UINT32(0, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(44, dl.tableMinutes());
}

// ITEM CASE: deadline anchored at the VALVE-OPEN instant, not the button
// press: with a 2 s settle, anchor is now+2000, and expiry is measured
// from there (PRD §6 preamble). Boundary: one ms before -> open, exactly
// at -> expired.
void test_anchor_and_expiry_boundary() {
    RecoveryClock clock;                    // fresh boot -> 30 min
    FillDeadline dl;
    const uint32_t press = 1000u;
    dl.beginFill(clock, press);
    TEST_ASSERT_FALSE(dl.expired(press));            // no deadline until anchored
    TEST_ASSERT_FALSE(dl.anchored());
    dl.anchor(press + BASELINE_SETTLE_MS);           // valve opens after settle
    TEST_ASSERT_TRUE(dl.anchored());

    const uint32_t deadline = dl.deadlineMs();
    TEST_ASSERT_EQUAL_UINT32(press + BASELINE_SETTLE_MS + 30u * 60u * 1000u,
                             deadline);
    TEST_ASSERT_FALSE(dl.expired(deadline - 1u));    // last granted ms
    TEST_ASSERT_TRUE(dl.expired(deadline));          // boundary: expired
}

// ITEM CASE: +5 twice moves the deadline EXACTLY 10 min (stacks,
// repeatable) — and a deadline that had just expired opens again.
void test_extend5_stacks() {
    RecoveryClock clock;                    // fresh boot -> 30 min
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    dl.anchor(3000u);
    const uint32_t d0 = dl.deadlineMs();

    dl.extend5();
    TEST_ASSERT_EQUAL_UINT32(35, dl.budgetMinutes());
    dl.extend5();
    TEST_ASSERT_EQUAL_UINT32(40, dl.budgetMinutes());
    TEST_ASSERT_EQUAL_UINT32(d0 + 2u * PLUS5_MIN * 60u * 1000u, dl.deadlineMs());

    TEST_ASSERT_FALSE(dl.expired(d0));               // old deadline: no longer expired
    TEST_ASSERT_FALSE(dl.expired(dl.deadlineMs() - 1u));
    TEST_ASSERT_TRUE(dl.expired(dl.deadlineMs()));
}

// +5 before the valve opens (during the settle window): the budget grows
// first and the anchor then bakes it into the deadline — same total.
void test_extend5_before_anchor() {
    RecoveryClock clock;
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    dl.extend5();
    dl.extend5();
    dl.anchor(3000u);
    TEST_ASSERT_EQUAL_UINT32(40, dl.budgetMinutes());
    TEST_ASSERT_EQUAL_UINT32(3000u + 40u * 60u * 1000u, dl.deadlineMs());
    TEST_ASSERT_FALSE(dl.expired(3000u + 40u * 60u * 1000u - 1u));
    TEST_ASSERT_TRUE(dl.expired(3000u + 40u * 60u * 1000u));
}

// ITEM CASE: wrap-safe arithmetic — anchor near the uint32 rollover and a
// recovery clock read across the rollover both behave (elapsedMs /
// deadlinePassed, P1.M1.T2.S1).
void test_wrap_safe_across_rollover() {
    // Recovery across the rollover: fill ends 0x200 before wrap, read after.
    RecoveryClock clock;
    const uint32_t fillEnd = 0xFFFFFF00u;
    clock.recordFillEnd(fillEnd);
    const uint32_t now = (uint32_t)(fillEnd + 0x200u);   // wraps to 0x100
    TEST_ASSERT_EQUAL_UINT32(0x200u, clock.recoveryMs(now));

    FillDeadline dl;
    dl.beginFill(clock, now);        // recovery 0 ms -> 0 min -> clamp 44
    TEST_ASSERT_EQUAL_UINT32(44, dl.tableMinutes());
    dl.anchor(0xFFFFFFFFu - 1000u);  // valve opens just before the rollover
    const uint32_t deadline = dl.deadlineMs();          // wraps naturally
    TEST_ASSERT_EQUAL_UINT32(
        (uint32_t)(0xFFFFFFFFu - 1000u + 44u * 60u * 1000u), deadline);
    TEST_ASSERT_FALSE(dl.expired((uint32_t)(deadline - 1u)));
    TEST_ASSERT_TRUE(dl.expired(deadline));
}

// A second fill after the first one ends: the clock keeps serving (RAM
// history persists across fills within one boot — only a RESET clears it).
void test_history_persists_across_fills_until_reset() {
    RecoveryClock clock;
    clock.recordFillEnd(500000u);
    clock.recordFillEnd(500000u + 60u * 60u * 1000u);    // 60 min later
    FillDeadline dl;
    dl.beginFill(clock,
                 500000u + 60u * 60u * 1000u + 10u * 60u * 1000u);  // +10 min
    TEST_ASSERT_EQUAL_UINT32(10u * 60u * 1000u, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(41, dl.tableMinutes());     // knot at 10 min
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_boot_uses_30min_default);
    RUN_TEST(test_history_15min_recovery_interpolates);
    RUN_TEST(test_abort_restarts_reference_at_abort_instant);
    RUN_TEST(test_zero_recovery_clamps_to_44);
    RUN_TEST(test_anchor_and_expiry_boundary);
    RUN_TEST(test_extend5_stacks);
    RUN_TEST(test_extend5_before_anchor);
    RUN_TEST(test_wrap_safe_across_rollover);
    RUN_TEST(test_history_persists_across_fills_until_reset);
    return UNITY_END();
}
