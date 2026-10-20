// test/test_logfmt/test_logfmt.cpp — native contract proof for the pure
// fill-log formatter formatFillLogEvent() (P2.M1.T1.S1): every FillLogEvent
// Kind renders its EXACT normative line (the README "Serial logging" table),
// recmin is derived from recovery ms by truncation, sensor payloads print
// 1-based, and snprintf truncation is safe. The hardware side (logLine CDC
// writes) stays board-only; these tests pin the text the bench will read.
#include <unity.h>

#include <string.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

using K = FillLogEvent::Kind;

// Helper: format into a fresh 96-byte buffer (logFillEvent's size) and
// assert the render fits (return < sizeof) — no line may ever truncate there.
static char g_buf[96];
static size_t render(K kind, uint8_t sensor, uint32_t a, uint32_t b) {
    FillLogEvent e{kind, sensor, a, b};
    size_t n = formatFillLogEvent(g_buf, sizeof(g_buf), e);
    TEST_ASSERT_TRUE(n < sizeof(g_buf));  // fits the shipped buffer
    TEST_ASSERT_EQUAL_UINT32((unsigned)strlen(g_buf), (unsigned)n);
    return n;
}

// ---- one exact test per Kind (final formats table, P2.M1.T1.S1) ------------

void test_valve_closed() {
    render(K::ValveClosed, 0, 0, 0);
    TEST_ASSERT_EQUAL_STRING("VALVE close", g_buf);
}

void test_fill_start_recmin_truncates_from_recovery() {
    render(K::FillStart, 0, 30, 599999u);  // 9.99 min of recovery -> 9 whole
    TEST_ASSERT_EQUAL_STRING("FILL start backstop=30min recovery=599999ms recmin=9", g_buf);
}

void test_fill_start_fresh_boot_recmin_zero() {
    render(K::FillStart, 0, 30, 0);  // fresh boot: recovery=0ms => recmin=0
    TEST_ASSERT_EQUAL_STRING("FILL start backstop=30min recovery=0ms recmin=0", g_buf);
}

void test_fill_start_recmin_exact_minute() {
    render(K::FillStart, 0, 44, 60000u);  // exactly one minute -> recmin=1
    TEST_ASSERT_EQUAL_STRING("FILL start backstop=44min recovery=60000ms recmin=1", g_buf);
}

void test_baseline_ok() {
    render(K::BaselineOk, 0, 3502, 3499);
    TEST_ASSERT_EQUAL_STRING("BASE ok s1=3502 s2=3499", g_buf);
}

void test_baseline_fault_carries_observed_count() {
    render(K::BaselineFault, 0, 1, 1000u);  // sensor 1, code 1, too-low count
    TEST_ASSERT_EQUAL_STRING("FAULT baseline sensor=1 code=1 count=1000", g_buf);
}

void test_valve_open() {
    render(K::ValveOpen, 0, 30, 0);
    TEST_ASSERT_EQUAL_STRING("VALVE open budget=30min", g_buf);
}

void test_trip() {
    render(K::Trip, 0, 60000u, 0);
    TEST_ASSERT_EQUAL_STRING("TRIP sensor=1 fillms=60000", g_buf);
}

void test_confirm_pass() {
    render(K::ConfirmPass, 1, 3000u, 0);
    TEST_ASSERT_EQUAL_STRING("CONFIRM pass sensor=2 durms=3000", g_buf);
}

void test_done() {
    render(K::Done, 0, 63000u, 60000u);
    TEST_ASSERT_EQUAL_STRING("DONE attemptms=63000 openms=60000", g_buf);
}

void test_abort_never_opened_openms_zero() {
    render(K::Abort, 0, 5000u, 0);  // settle abort: valve never opened
    TEST_ASSERT_EQUAL_STRING("ABORT attemptms=5000 openms=0", g_buf);
}

void test_abort_after_open_carries_openms() {
    render(K::Abort, 0, 44000u, 40000u);  // mid-fill abort: valve-open span
    TEST_ASSERT_EQUAL_STRING("ABORT attemptms=44000 openms=40000", g_buf);
}

void test_early_wet_fault_carries_fillms() {
    render(K::EarlyWetFault, 1, 2, 100u);
    TEST_ASSERT_EQUAL_STRING("FAULT earlywet sensor=2 code=2 fillms=100", g_buf);
}

void test_early_trip_fault_carries_fillms() {
    render(K::EarlyTripFault, 0, 1, 45000u);
    TEST_ASSERT_EQUAL_STRING("FAULT earlytrip sensor=1 code=1 fillms=45000", g_buf);
}

void test_backstop_budget_and_openms_with_literal_code3() {
    render(K::BackstopExpired, 0, 30, 1800000u);  // code is a LITERAL 3
    TEST_ASSERT_EQUAL_STRING("FAULT backstop code=3 budget=30min openms=1800000ms", g_buf);
}

void test_confirm_timeout_carries_durms() {
    render(K::ConfirmTimeout, 1, 2, 12000u);
    TEST_ASSERT_EQUAL_STRING("FAULT confirm sensor=2 code=2 durms=12000", g_buf);
}

void test_plus5() {
    render(K::Plus5, 0, 35, 0);
    TEST_ASSERT_EQUAL_STRING("PLUS5 budget=35min", g_buf);
}

void test_ack() {
    render(K::Ack, 0, 0, 0);
    TEST_ASSERT_EQUAL_STRING("ACK done", g_buf);
}

// ---- sensor id mapping: engine 0-based, printed 1-based (§7 codes) ---------

void test_sensor_mapping_one_based() {
    FillLogEvent e1{K::BaselineFault, 0, 1, 1234u};
    formatFillLogEvent(g_buf, sizeof(g_buf), e1);
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)strncmp(g_buf, "FAULT baseline sensor=1 ", 24));
    FillLogEvent e2{K::BaselineFault, 1, 2, 4100u};
    formatFillLogEvent(g_buf, sizeof(g_buf), e2);
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)strncmp(g_buf, "FAULT baseline sensor=2 ", 24));
}

// ---- truncation safety (snprintf contract, same gates as test_log_utils) ---

void test_truncation_is_safe() {
    char tiny[8];
    size_t n = formatFillLogEvent(tiny, sizeof(tiny),
                                  FillLogEvent{K::BackstopExpired, 0, 30, 1800000u});
    TEST_ASSERT_TRUE(n >= sizeof(tiny));                  // wanted more space
    TEST_ASSERT_EQUAL_UINT32(7, (unsigned)strlen(tiny));  // filled to the brim
    TEST_ASSERT_EQUAL_UINT8(0, tiny[sizeof(tiny) - 1]);   // NUL at the last byte
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)strncmp(tiny, "FAULT b", 7));  // prefix kept
}

void test_zero_len_buffer_writes_nothing() {
    char sink[1] = {'x'};
    formatFillLogEvent(sink, 0, FillLogEvent{K::Ack, 0, 0, 0});
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'x', (uint8_t)sink[0]);
}

// Every final line fits logFillEvent's 96-byte buffer with room to spare —
// guard against a future format silently exceeding the transmit buffer.
void test_all_lines_fit_96_bytes() {
    const FillLogEvent samples[] = {
        {K::ValveClosed, 0, 0, 0},
        {K::FillStart, 0, 999, 599999u},
        {K::BaselineOk, 0, 4095, 4095},
        {K::BaselineFault, 1, 2, 65535u},
        {K::ValveOpen, 0, 999, 0},
        {K::Trip, 1, 4294967295u, 0},
        {K::ConfirmPass, 0, 4294967295u, 0},
        {K::Done, 0, 4294967295u, 4294967295u},
        {K::Abort, 0, 4294967295u, 4294967295u},
        {K::EarlyWetFault, 1, 2, 4294967295u},
        {K::EarlyTripFault, 0, 1, 4294967295u},
        {K::BackstopExpired, 0, 999, 4294967295u},
        {K::ConfirmTimeout, 1, 2, 4294967295u},
        {K::Plus5, 0, 999, 0},
        {K::Ack, 0, 0, 0},
    };
    char buf[96];
    for (const FillLogEvent& e : samples) {
        TEST_ASSERT_TRUE(formatFillLogEvent(buf, sizeof(buf), e) < sizeof(buf));
    }
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_valve_closed);
    RUN_TEST(test_fill_start_recmin_truncates_from_recovery);
    RUN_TEST(test_fill_start_fresh_boot_recmin_zero);
    RUN_TEST(test_fill_start_recmin_exact_minute);
    RUN_TEST(test_baseline_ok);
    RUN_TEST(test_baseline_fault_carries_observed_count);
    RUN_TEST(test_valve_open);
    RUN_TEST(test_trip);
    RUN_TEST(test_confirm_pass);
    RUN_TEST(test_done);
    RUN_TEST(test_abort_never_opened_openms_zero);
    RUN_TEST(test_abort_after_open_carries_openms);
    RUN_TEST(test_early_wet_fault_carries_fillms);
    RUN_TEST(test_early_trip_fault_carries_fillms);
    RUN_TEST(test_backstop_budget_and_openms_with_literal_code3);
    RUN_TEST(test_confirm_timeout_carries_durms);
    RUN_TEST(test_plus5);
    RUN_TEST(test_ack);
    RUN_TEST(test_sensor_mapping_one_based);
    RUN_TEST(test_truncation_is_safe);
    RUN_TEST(test_zero_len_buffer_writes_nothing);
    RUN_TEST(test_all_lines_fit_96_bytes);
    return UNITY_END();
}
