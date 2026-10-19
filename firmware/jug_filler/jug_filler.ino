// firmware/jug_filler/jug_filler.ino
//
// ============================== SEAM CONTRACT ==============================
// This single file is BOTH the Arduino sketch and the host-testable logic
// module (architecture: plan/.../firmware_architecture.md §1). Rules:
//
//   1. UNGUARDED region = pure logic ONLY. No Arduino calls whatsoever — no
//      pinMode/digitalWrite/analogRead/millis/Serial/delay, no libraries.
//      Pure C++ that takes `uint32_t nowMs` (and sensor/event data) as
//      parameters. Native tests compile and exercise it on the host.
//   2. #ifdef ARDUINO region = hardware glue ONLY: setup()/loop(), pin
//      writes, Serial logging, watchdog, LED/WS2812 drivers.
//   3. Every function gets an explicit prototype above its definition —
//      host g++ does not run the Arduino .ino prototype generator.
//   4. Tests live in test/*.cpp as:
//      #include "../firmware/jug_filler/jug_filler.ino"
//      and run via `pio test -e native` (which excludes this folder from
//      its own build via build_src_filter in platformio.ini).
//
// SAFETY (PRD §10): the FIRST executable line of setup() drives PIN_VALVE
// LOW. Never insert anything above it. The invariant suite (P1.M7.T1.S1)
// structurally asserts this and scans for delay() in the loop region.
//
// Every later pure-logic subtask lands in the UNGUARDED region below.
// ==========================================================================

#include "config.h"
#include <stdarg.h>   // va_list for the CDC log-line wrapper (host-safe)
#include <stdio.h>    // snprintf/vsnprintf — the only formatting engines (host-safe)

// ---- Prototypes (explicit; native g++ needs them, Arduino tolerates them) ----
uint32_t elapsedMs(uint32_t now, uint32_t since);      // rollover-safe elapsed ms (P1.M1.T2.S1)
bool deadlinePassed(uint32_t now, uint32_t deadline);  // rollover-safe deadline check (P1.M1.T2.S1)
size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused);  // pure banner text (P1.M1.T2.S2)
void logLine(const char* fmt, ...);  // bounded, newline-terminated CDC log line (P1.M1.T2.S2; ARDUINO region only)
void setup();             // hardware only (#ifdef ARDUINO)
void loop();              // hardware only (#ifdef ARDUINO)

// ---- UNGUARDED REGION: pure logic (host-testable) ----

// ===================== Rollover-safe time helpers (P1.M1.T2.S1) =====================
// millis() is uint32_t and wraps 0xFFFFFFFF -> 0x00000000 every 2^32 ms (~49.7 days).
// Unsigned C++ arithmetic is defined modulo 2^32 (ISO C++ [expr.arith.conv]) — so
// subtraction of timestamps NEVER overflows/UBs and yields the true elapsed span for
// any span < 2^32 ms. Direct timestamp comparison (now >= deadline) is FORBIDDEN —
// ordering across the wrap is meaningless. All timed logic goes through these two:
//
//   interval question:  elapsedMs(now, sinceMs)        >= SOME_CONFIG_MS
//   deadline question:  deadlinePassed(now, deadlineTs)  (absolute millis timestamp)
//
// Seam rule: `now` is ALWAYS injected — these functions never call millis().

// Milliseconds elapsed from timestamp `since` to timestamp `now` (both absolute
// millis()-domain uint32_t values). Wrap-safe for any true span < 2^32 ms.
uint32_t elapsedMs(uint32_t now, uint32_t since) {
    return now - since;  // mod-2^32 subtraction IS the rollover safety — keep it raw
}

// 2^31 ms ≈ 24.86 days: the maximum forward window deadlinePassed can certify.
// Mathematical constant (not a tunable) — every firmware deadline is ≤ 44 min.
constexpr uint32_t HALF_WHEEL_MS = 0x80000000u;

// True iff the absolute timestamp `deadline` has been reached or passed at `now`
// (rollover-safe, inclusive at equality: a fill backstop sampled exactly at its
// deadline fires — the valve closes AT the deadline, never 1 ms late).
// now == deadline → true; deadline+1 → true; deadline-1 → false; correct even when
// the pair straddles the 49.7-day wrap.
bool deadlinePassed(uint32_t now, uint32_t deadline) {
    return elapsedMs(now, deadline) < HALF_WHEEL_MS;
}
// ============== end time helpers (consumed by P1.M2–P1.M6) ==============

// ===================== Boot banner builder (P1.M1.T2.S2) =====================
// Pure text builder so the boot banner's reset-cause wording is native-testable;
// the hardware read of the SDK bootcause flag is injected as a plain bool. Uses
// snprintf only — no hardware calls (seam rule). Labels are normative:
//   watchdog boot -> "JugFiller boot (reset cause: WATCHDOG)"          (38 chars)
//   any other     -> "JugFiller boot (reset cause: POWER_OR_RESET)"    (44 chars)
// snprintf contract: returns the would-be length (int); writes <= bufLen-1 chars
// plus a NUL; bufLen == 0 touches nothing.
size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused) {
    return (size_t)snprintf(buf, bufLen, "JugFiller boot (reset cause: %s)",
                            watchdogCaused ? "WATCHDOG" : "POWER_OR_RESET");
}

// ---- #ifdef ARDUINO REGION: hardware glue ----
#ifdef ARDUINO

#include <hardware/watchdog.h>   // pico-sdk watchdog, shipped inside arduino-pico (verified)

// Bounded CDC log line (P1.M1.T2.S2) — the ONLY logging API; later subtasks
// (P1.M2.T2.S1, P1.M3.T1.S2, P1.M6.T2.S1) must use it, never raw CDC writes.
// Policy (PRD §2.2/§3/§10): one vsnprintf into a fixed buffer, one bounded write,
// one newline — NEVER gated on connection state (USB is unplugged in service), so
// logging can never stall the loop. Overlong lines truncate (drop the excess, keep
// the newline); a formatting error (n < 0) drops the line.
constexpr size_t LOGLINE_MAX = 128;  // bound, not a tunable — sole consumer is logLine

void logLine(const char* fmt, ...) {
    char line[LOGLINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0) return;  // formatting error: drop the line, keep the loop alive
    size_t len = ((size_t)n >= sizeof(line)) ? sizeof(line) - 1 : (size_t)n;
    Serial.write(reinterpret_cast<const uint8_t*>(line), len);
    Serial.write('\n');
}

void setup() {
  digitalWrite(PIN_VALVE, LOW);   // LINE 1 — PRD §10: valve closed before all else
  pinMode(PIN_VALVE, OUTPUT);     // drive the pre-latched LOW (glitch-free)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);     // external LED off at boot
  Serial.begin(SERIAL_BAUD);      // USB CDC, always on — NEVER while(!Serial) (PRD §3)
  analogReadResolution(ADC_BITS); // MANDATORY 12-bit — every config count is 12-bit

  // Boot banner (P1.M1.T2.S2): reset cause from the SDK bootcause flag, read once
  // BEFORE the watchdog is re-enabled; ONE bounded CDC write via logLine.
  char banner[64];  // 44-char worst case fits with headroom
  formatBootBanner(banner, sizeof(banner), watchdog_caused_reboot());
  logLine("%s", banner);

  // Hardware watchdog LAST in setup() (arch boot order): 8000 ms < 8388 ms ceiling
  // (RP2040-E1); pause_on_debug=true so bench debugging never trips it. loop() feeds
  // it as its first statement every pass — a hang resets -> setup() line 1 closes
  // the valve -> IDLE, never auto-refill (PRD §10, §5.3).
  watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true);
}

void loop() {
  watchdog_update();   // FIRST STATEMENT — PRD §10: hang -> reset -> valve LOW -> IDLE
  // P1.M6.T2.S1 fills the loop below this line (BOOTSEL poll, ADC, fill engine,
  // LED, event logs via logLine). Nothing may ever be inserted above watchdog_update().
}

#endif  // ARDUINO
