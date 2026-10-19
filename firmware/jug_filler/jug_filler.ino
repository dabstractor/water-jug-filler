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

// ---- Prototypes (explicit; native g++ needs them, Arduino tolerates them) ----
uint32_t seamVersion();   // seam probe — replaced when first real logic lands
void setup();             // hardware only (#ifdef ARDUINO)
void loop();              // hardware only (#ifdef ARDUINO)

// ---- UNGUARDED REGION: pure logic (host-testable) ----

// Probe function proving the seam: callable from native Unity tests.
uint32_t seamVersion() { return 1; }

// ---- #ifdef ARDUINO REGION: hardware glue ----
#ifdef ARDUINO

void setup() {
  digitalWrite(PIN_VALVE, LOW);   // LINE 1 — PRD §10: valve closed before all else
  pinMode(PIN_VALVE, OUTPUT);     // drive the pre-latched LOW (glitch-free)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);     // external LED off at boot
  Serial.begin(SERIAL_BAUD);      // USB CDC, always on — NEVER while(!Serial) (PRD §3)
  analogReadResolution(ADC_BITS); // MANDATORY 12-bit — every config count is 12-bit
}

void loop() {
  // Safe placeholder: nothing here can open the valve (button service arrives in P1.M2).
  // P1.M1.T2.S2 makes watchdog_update() the first call here; P1.M6.T2.S1 fills the loop.
}

#endif  // ARDUINO
