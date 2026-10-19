// test/test_seam.cpp — proves the native seam BEFORE any logic lands (P1.M1.T1.S2).
// Compiles the sketch itself (via the .ino include) on the host g++; asserts that
// config constants reach native tests and the unguarded region is callable.
#include <unity.h>

#include "../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

void test_config_constants_reach_native_tests() {
    TEST_ASSERT_EQUAL_UINT8(15, PIN_VALVE);              // GP15, HIGH = open (PRD §2.1)
    TEST_ASSERT_EQUAL_UINT8(14, PIN_LED);
    TEST_ASSERT_EQUAL_UINT32(115200, SERIAL_BAUD);       // PRD §3
    TEST_ASSERT_EQUAL_UINT8(12, ADC_BITS);               // 12-bit ADC contract (PRD §8)
    TEST_ASSERT_EQUAL_UINT16(30, FRESH_BOOT_BACKSTOP_MIN);
    TEST_ASSERT_EQUAL_UINT16(8, WATCHDOG_TIMEOUT_S);     // ceiling 8.388 s — never raise
    TEST_ASSERT_EQUAL_UINT8(5, BACKSTOP_TABLE_LEN);
}

void test_unguarded_region_is_callable() {
    // Pure region reachable from host — time helpers replaced the seamVersion() probe
    // when the first real logic landed (P1.M1.T2.S1).
    TEST_ASSERT_EQUAL_UINT32(0, elapsedMs(1000, 1000));
    TEST_ASSERT_TRUE(deadlinePassed(1000, 1000));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner (verified live)
    UNITY_BEGIN();
    RUN_TEST(test_config_constants_reach_native_tests);
    RUN_TEST(test_unguarded_region_is_callable);
    return UNITY_END();
}
