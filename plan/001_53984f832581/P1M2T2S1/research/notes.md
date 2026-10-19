# Research notes — P1.M2.T2.S1 (Poll BOOTSEL into classifier + event logging)

## 1. Verified state of the codebase (direct reads, this session)

- `firmware/jug_filler/jug_filler.ino` TODAY contains: seam header, prototype block
  (elapsedMs, deadlinePassed, formatBootBanner, logLine, setup, loop), UNGUARDED
  region (time helpers P1.M1.T2.S1 + formatBootBanner P1.M1.T2.S2), ARDUINO region
  (`#include <hardware/watchdog.h>`, `LOGLINE_MAX=128`, `logLine()`, `setup()`
  ending in `watchdog_enable`, `loop()` = `watchdog_update()` first + filler
  comment naming P1.M6.T2.S1). **P1.M1.T2.S2 has landed.** P1.M2.T1.S1
  (ButtonClassifier class) is landing in parallel — THIS task depends on it.
- `config.h`: `BUTTON_LONG_PRESS_MS = 1000`, `BUTTON_DEBOUNCE_MS = 40` exist.
- Toolchain (proven by earlier PRPs on this machine): `pio` 6.2.0 at
  `~/.local/bin/pio`, host g++ 16.2.1, Unity 2.6.1, `[env:pico]` compile-only,
  `[env:native]` one Unity project per `test/test_*.cpp`.

## 2. Contracts consumed

- **ButtonClassifier (P1.M2.T1.S1 PRP — binding)**: class in UNGUARDED region
  above the ARDUINO marker. Public surface is EXACTLY:
  `enum Event : uint8_t { None=0, ShortPress=1, LongPress=2 }` and
  `Event update(uint32_t nowMs, bool pressedRaw)` — one event per call, fired only
  on accepted RELEASE; `None` steady state incl. while held. No getters, no
  isPressed(). Hold semantics: raw-stable press C..R classifies (R−C) ≥ 1000 → Long.
- **logLine (P1.M1.T2.S2, landed)**: `void logLine(const char* fmt, ...)`;
  bounded 128-byte vsnprintf, newline-terminated, never gates on USB connection,
  drops line on format error. THE only logging API.
- **Loop order (firmware_architecture.md line 28)**:
  `loop(): watchdog_update → sample BOOTSEL → button events → …` — nothing ever
  above `watchdog_update()` (P1.M7.T1.S1 checks structurally).

## 3. BOOTSEL hardware facts (external_deps.md §1, pre-verified)

- `BOOTSEL` is a global `__Bootsel` OBJECT, not a pin. Correct read:
  `bool pressed = BOOTSEL;` (operator bool(), true == pressed). NO pinMode.
- PRD §2.1's `pinMode(BOOTSEL, INPUT_PULLUP)` + `digitalRead(BOOTSEL)` would
  convert the object to int (0/1) and silently touch GPIO 0/1 — forbidden.
- Implementation floats QSPI flash-CS briefly with IRQs parked + other core
  idled — designed for loop() use; safe at kHz poll rates; runtime reads can
  NEVER enter the bootloader (entry needs BOOTSEL held during RESET).
- Sources: https://arduino-pico.readthedocs.io/en/latest/bootsel.html ,
  https://github.com/earlephilhower/arduino-pico/blob/master/cores/rp2040/Bootsel.cpp
- Gotcha for the bench: powering the board with BOOTSEL already held boots to the
  USB bootloader (sketch never runs) — release the button before/during plug-in.

## 4. Design decision — hold duration in the log line

Item spec: log each event WITH hold duration, e.g. `BTN short 213ms` /
`BTN long 1024ms`. The classifier contract exposes NO hold accessor (scope
discipline — cannot and must not extend it in parallel-landing code). Resolution:
the WIRING layer (ARDUINO region) keeps its own raw-edge stamps — `s_btnRiseMs`
(on raw false→true) and `s_btnFallMs` (on raw true→false) — and logs
`elapsedMs(s_btnFallMs, s_btnRiseMs)`. This duplicates NO debounce/classification
logic (classifier stays the sole authority for events); the number is a
diagnostic approximation: raw first-edge to last-edge, within contact-bounce
(~tens of ms) of the classifier's theoretical (R−C). rollover-safe via elapsedMs.
Deviation note: earlier PRP cheat sheets sketched `BTN event=%d t=%lu` —
illustrative only; the item description's format is normative for THIS task.

## 5. Bench procedure (PRD §2.2 / §3: USB power only, buck disconnected)

1. `pio run -e pico -t upload` with the RP2040-Zero on USB.
2. `pio device monitor -b 115200` (USB CDC; never `while(!Serial)` in code).
3. Expect boot banner line; then per press: `BTN short <n>ms` on quick
   press/release; `BTN long <n>ms` on ~1s+ hold (n ≥ 1000). Silence while held.
4. Multiple presses → one line each; sub-40ms jabs → no line.
Upload is allowed HERE (unlike T1.S1) because the item's own acceptance is a
bench check — but compile-only remains the CI-able gate when no board is attached.
