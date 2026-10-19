// firmware/config.h
//
// Jug-filler configuration — EVERY tunable in one place (PRD spec/SPEC.md §8).
// Single source of truth: downstream code (sketch + native tests) includes this
// file and must never hardcode a firmware constant elsewhere.
//
// This file is CONSTANTS ONLY — no functions, no Arduino includes (it must
// compile under plain host g++ for the native test seam).
// All values: PRD §8 defaults. Commissioning may retune ADC/sensor values (§9).

#pragma once

#include <stdint.h>   // fixed-width types; works in Arduino g++ and host g++

// One point of the concave piecewise-linear backstop curve (PRD §6.2).
struct BackstopTableEntry {
    uint16_t recoveryMin;   // minutes since previous fill end (independent variable)
    uint16_t backstopMin;   // max valve-open minutes granted at that recovery
};

// ---- 1. Pins (RP2040-Zero) — PRD §2.1 ------------------------------------
constexpr uint8_t PIN_SENSOR1 = 26;   // GPIO# — GP26 / A0 / ADC0 → sensor 1 AOUT (PRD §2.1)
constexpr uint8_t PIN_SENSOR2 = 27;   // GPIO# — GP27 / A1 / ADC1 → sensor 2 AOUT (PRD §2.1)
constexpr uint8_t PIN_VALVE   = 15;   // GPIO# — GP15 → MOSFET TRIG/SIG; HIGH = valve open (PRD §2.1)
constexpr uint8_t PIN_LED     = 14;   // GPIO# — GP14 → external green LED anode via 330 Ω (PRD §2.1)

// ---- 2. Button — PRD §5.1 -------------------------------------------------
constexpr uint32_t BUTTON_LONG_PRESS_MS = 1000;   // ms — hold ≥ this = long press (PRD §5.1)
constexpr uint32_t BUTTON_DEBOUNCE_MS   = 40;     // ms — contact debounce time (PRD §5.1)

// ---- 3. Fill backstop model — PRD §6.1 / §6.2 -----------------------------
constexpr uint16_t FRESH_BOOT_BACKSTOP_MIN = 30;   // min — no-history default (fresh boot/reset); assume full tank (PRD §6.1)

// Concave piecewise curve, clamped both ends (PRD §6.2): {recoveryMin, backstopMin} pairs.
constexpr BackstopTableEntry BACKSTOP_TABLE_MIN[] = {
    {2, 44}, {10, 41}, {20, 38}, {35, 34}, {60, 30},
};
constexpr uint8_t BACKSTOP_TABLE_LEN =
    static_cast<uint8_t>(sizeof(BACKSTOP_TABLE_MIN) / sizeof(BACKSTOP_TABLE_MIN[0]));   // count — entries in BACKSTOP_TABLE_MIN (helper for P1.M4.T1.S1 iteration)

constexpr uint16_t PLUS5_MIN = 5;   // min — long-press deadline extension, FILLING only, stacks (PRD §5.1)

// ---- 4. Windows — PRD §7 --------------------------------------------------
constexpr uint16_t CONFIRM_WINDOW_S   = 12;   // s — second sensor must trip within this of the first (PRD §7.5)
constexpr uint16_t EARLY_WET_WINDOW_S = 5;    // s — wet reading this early = false trip → fault (PRD §7.6)
constexpr uint16_t TRIP_MIN_ELAPSED_S = 60;   // s — trips before this elapsed = false trip → fault (PRD §7.6)

// ---- 5. ADC / sensors (provisional; set during commissioning — PRD §9) ----
// ADC_BITS: analogReadResolution(ADC_BITS) is MANDATORY in setup() — the
//   arduino-pico default is 10-bit; every count below is a 12-bit count (0–4095)
//   assuming the 3.3 V rail as full-scale (AVDD reference; 3.3 V ≈ 4095).
constexpr uint8_t  ADC_BITS               = 12;     // bits — ADC resolution the sketch must set (PRD §8 contract)
constexpr uint16_t ADC_SAMPLES_PER_READING = 32;    // samples — averaged per reading (PRD §8)
constexpr uint32_t READING_INTERVAL_MS     = 100;   // ms — cadence between averaged readings (PRD §8)
constexpr uint32_t BASELINE_SETTLE_MS      = 2000;  // ms — settle before baseline capture (PRD §7.1)
constexpr uint16_t BASELINE_MIN            = 2000;  // counts (12-bit) — plausible dry baseline lower bound (PRD §8; commissioning §9)
constexpr uint16_t BASELINE_MAX            = 4050;  // counts (12-bit) — plausible dry baseline upper bound (PRD §8; commissioning §9)
constexpr uint16_t TRIP_DROP_COUNTS        = 350;   // counts (12-bit) — wet threshold = baseline − this (PRD §7.3)
constexpr uint16_t DEBOUNCE_COUNT          = 8;     // readings — consecutive wet readings to accept a trip (PRD §7.3)

// ---- 6. LED patterns — PRD §4 --------------------------------------------
constexpr uint32_t DONE_PULSE_ON_MS  = 200;    // ms — DONE attention pulse on-time (PRD §4)
constexpr uint32_t DONE_PULSE_OFF_MS = 200;    // ms — DONE attention pulse off-time (PRD §4)
constexpr uint8_t  DONE_PULSE_COUNT  = 3;      // pulses — pulses per DONE group (PRD §4)
constexpr uint32_t DONE_GAP_MS       = 1000;   // ms — off-time between DONE groups (PRD §4)
constexpr uint32_t FAULT_BLINK_MS    = 250;    // ms — fault blink on/off time (PRD §7)
constexpr uint32_t FAULT_PAUSE_MS    = 2000;   // ms — pause between fault blink groups (PRD §7)
constexpr uint32_t ACK_BLINK_MS      = 100;    // ms — +5 ack blink on/off (PRD §4 transient)
constexpr bool     USE_ONBOARD_RGB   = true;   // flag — mirror patterns on onboard WS2812 (GP16, no wiring) (PRD §4)
constexpr uint8_t  RGB_BRIGHTNESS    = 60;     // 0–255 — WS2812 brightness cap, must stay ≤ 60 (3V3 LDO budget, PRD §2.2)

// ---- 7. Platform — PRD §3 / §10 ------------------------------------------
constexpr uint32_t SERIAL_BAUD       = 115200;   // baud — USB CDC serial, always on (PRD §3)
constexpr uint16_t WATCHDOG_TIMEOUT_S = 8;       // s — RP2040 watchdog; hard ceiling 8.388 s (RP2040-E1) — do not raise (PRD §10)
