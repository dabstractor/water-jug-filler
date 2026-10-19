// test/test_log_utils.cpp — native tests for the pure boot-banner builder
// (P1.M1.T2.S2). The hardware side (watchdog_enable/update/caused_reboot, CDC
// writes) is board-only and is exercised on the bench at P1.M7.T2.S1; the seam
// keeps everything testable about logging pure — these tests pin that contract.
#include <unity.h>

#include <string.h>

#include "../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// Watchdog-reset boot: exact normative label; return == strlen when it fits.
void test_banner_watchdog_cause() {
    char buf[64];
    size_t n = formatBootBanner(buf, sizeof(buf), true);
    TEST_ASSERT_EQUAL_STRING("JugFiller boot (reset cause: WATCHDOG)", buf);
    TEST_ASSERT_EQUAL_UINT32((unsigned)strlen(buf), (unsigned)n);
}

// Power-on / RESET-button boot: the other normative label.
void test_banner_power_reset_cause() {
    char buf[64];
    size_t n = formatBootBanner(buf, sizeof(buf), false);
    TEST_ASSERT_EQUAL_STRING("JugFiller boot (reset cause: POWER_OR_RESET)", buf);
    TEST_ASSERT_EQUAL_UINT32((unsigned)strlen(buf), (unsigned)n);
}

// A refactor must never collapse the two causes into one label — the banner's
// whole diagnostic value is telling watchdog resets from normal boots.
void test_banner_distinguishes_causes() {
    char w[64], p[64];
    formatBootBanner(w, sizeof(w), true);
    formatBootBanner(p, sizeof(p), false);
    TEST_ASSERT_TRUE(strcmp(w, p) != 0);
}

// Stable prefix keeps bench grep/parsers simple across subtasks.
void test_banner_common_prefix() {
    char buf[64];
    formatBootBanner(buf, sizeof(buf), true);
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)strncmp(buf, "JugFiller boot", 14));
}

// Truncation is safe, never a buffer overrun (snprintf semantics).
void test_banner_truncation_is_safe() {
    char tiny[8];
    size_t n = formatBootBanner(tiny, sizeof(tiny), true);
    TEST_ASSERT_TRUE(n >= sizeof(tiny));                 // wanted more space
    TEST_ASSERT_EQUAL_UINT32(7, (unsigned)strlen(tiny)); // filled to the brim
    TEST_ASSERT_EQUAL_UINT8(0, tiny[sizeof(tiny) - 1]);  // NUL at the last byte
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)strncmp(tiny, "JugFiller", 7)); // prefix kept
}

// Degenerate buffer: writes nothing at all.
void test_banner_zero_len_buffer() {
    char sink[1] = {'x'};
    formatBootBanner(sink, 0, true);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'x', (uint8_t)sink[0]);
}

// Both labels fit setup()'s 64-byte banner buffer — no truncated boot banner.
void test_banner_fits_in_64() {
    char buf[64];
    TEST_ASSERT_TRUE(formatBootBanner(buf, sizeof(buf), true) < sizeof(buf));
    TEST_ASSERT_TRUE(formatBootBanner(buf, sizeof(buf), false) < sizeof(buf));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_banner_watchdog_cause);
    RUN_TEST(test_banner_power_reset_cause);
    RUN_TEST(test_banner_distinguishes_causes);
    RUN_TEST(test_banner_common_prefix);
    RUN_TEST(test_banner_truncation_is_safe);
    RUN_TEST(test_banner_zero_len_buffer);
    RUN_TEST(test_banner_fits_in_64);
    return UNITY_END();
}
