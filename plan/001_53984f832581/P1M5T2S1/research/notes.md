# Research notes — P1.M5.T2.S1: Drive GP14 from the pattern engine each loop pass

## 1. Verified current tree state (session of this PRP)

- `firmware/jug_filler/jug_filler.ino` = **995 lines** and GROWING MID-SESSION:
  P1.M5.T1.S1 is landing in parallel. Verified present NOW:
  - `class LedPatternEngine` — count 1 (landed)
  - `// ===== end LED pattern engine (consumed by P1.M5.T2.S1, P1.M5.T3.S1, P1.M6.T1.S2) =====` at line 864
  - `// ---- #ifdef ARDUINO REGION: hardware glue ----` at line 866
  - `test/test_led_pattern/` — **NOT yet present** (T1.S1 test not landed yet)
  → My PRP's Task 0 gates on T1.S1 being FULLY complete (class + test dir + native green).
- ARDUINO region layout (verified by direct read):
  - `logLine` → BOOTSEL service state block (`s_button` etc.) → ADC sampling service
    state block (`s_adc` etc.) → `void setup()` → `void loop()` → `#endif`.
  - **setup() ALREADY lands the LED init** (from P1.M1.T1.S2 skeleton):
    `pinMode(PIN_LED, OUTPUT);` then `digitalWrite(PIN_LED, LOW);  // external LED off at boot`
    → This task must NOT touch setup() — contract item 3 is already satisfied there.
  - loop() order today: `watchdog_update()` → `nowMs = millis()` snapshot → BOOTSEL
    poll stage → ADC sample/aggregate/log stage → trailing comment
    `// P1.M6.T2.S1 fills the loop below this line ...` (currently line ~988-992).

## 2. Frozen upstream API (P1.M5.T1.S1 PRP — contract for this task)

- `class LedPatternEngine` in the UNGUARDED region; `enum Mode : uint8_t {Off=0, Solid=1, DonePulse=2, FaultBlink=3}`.
- `bool tick(uint32_t nowMs)` → LED level for this instant; ONE call per loop pass;
  lazily stamps phase/ack anchors; default mode = Off (LED dark at boot — consistent
  with setup()'s initial LOW).
- `setMode`/`triggerAck` are the FILL ENGINE's (P1.M6.T1.S2), NOT this driver's.
- Caller contract verbatim from T1.S1 PRP: `digitalWrite(PIN_LED, led.tick(nowMs));`
  and mirror gets color from `mode()`, level from `tick()`.

## 3. Design decisions for this task

1. **State block**: house pattern = file-scope static in a bannered ARDUINO-region
   block (precedents `s_button`, `s_adc`). Add `static LedPatternEngine s_led;`
   in `// ---- External LED driver state (P1.M5.T2.S1) ----` after the ADC state block.
2. **Loop stage**: LAST stage of loop(), after the ADC stage — the fill-engine routing
   (P1.M6.T2.S1) will insert ABOVE it so mode selection precedes `s_led.tick` in the
   same pass (lazy anchor makes same-pass setMode→tick exact; T1.S1 PRP §design).
3. **Single tick, shared level**: `const bool ledOn = s_led.tick(nowMs);` once, then
   `digitalWrite(PIN_LED, ledOn ? HIGH : LOW);`. The WS2812 mirror (P1.M5.T3.S1)
   reuses `ledOn` — never re-ticks (double-tick with same nowMs is harmless but the
   single-tick shape is the frozen T1.S1 caller contract).
4. **Explicit HIGH/LOW**: bool→int would convert (HIGH=1/LOW=0), but pin levels
   spelled out match Arduino idiom and the item's "digitalWrite(PIN_LED, engine.ledOn)".
5. **No new native tests**: item contract "Mock: none on device; engine patterns
   native-tested in P1.M5.T1.S1". ARDUINO region is `#ifdef`-excluded from native
   builds anyway — validation = pico compile + native regression + structure greps.
6. **No logging in the stage**: per-pass writes would spam CDC; mode-change logs
   belong to the fill engine when it calls setMode (P1.M6.T2.S1).
7. **Trailing comment update**: rewrite the "P1.M6.T2.S1 fills the loop below this
   line" comment to record what landed and where M6.T2.S1/T3.S1 insert next.

## 4. Electrical / spec facts (treat as verified per project instruction)

- GP14 → 330 Ω → LED anode; cathode → GND (PRD §2.1 pin map). Pin SOURCES on HIGH.
- ~10 mA at 3.3 V through 330 Ω — ample within GPIO drive strength (item contract;
  hardware_validation.md line 80 says ≈7 mA — same conclusion).
- External green LED is AUTHORITATIVE; WS2812 (GP16) only mirrors (PRD §4).
- CONFIRM continues Solid, DONE persists until acknowledged (no timeout, PRD §11) —
  all pattern semantics live in the engine, NOT this driver (mode-agnostic).

## 5. Architecture citations

- firmware_architecture.md line 31: loop ends `→ LED drivers (GP14 + WS2812 mirror) → logLine events`.
- line 40: hardware code only in `#ifdef ARDUINO` region. Line 53: no blocking delay.
- hardware_validation.md line 96: LED-pattern bench observation belongs to P1.M7.T2.S1.

## 6. Edit anchors (text-based; line numbers shift with T1.S1 landing)

- A1: insert state block AFTER `// ---- end ADC sampling service state (used by loop, kept by P1.M6.T2.S1) ----`, BEFORE `void setup() {`.
- A2: insert LED stage AFTER `// ---- end ADC sample / aggregate / commissioning log ----`, REPLACING the 3-line trailing comment `// P1.M6.T2.S1 fills the loop below this line ...`.
