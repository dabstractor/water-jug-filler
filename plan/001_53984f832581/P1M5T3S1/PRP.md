# PRP — P1.M5.T3.S1: Config-gated WS2812 mirror with color map and brightness cap

## Goal

**Feature Goal**: Drive the RP2040-Zero's onboard WS2812 (GP16, firmware-only, PRD §2.1) as a COLOR MIRROR of the authoritative external green LED: same on/off pattern stream (from the shared `LedPatternEngine`), colored per PRD §4's mirror column — IDLE black, FILLING/CONFIRM blue solid, DONE green in the done-pulse pattern, FAULT red in the fault blink-code pattern — capped at `RGB_BRIGHTNESS` (60/255) by scaling the RGB values written, and fully gated by `USE_ONBOARD_RGB` in `config.h`.

**Deliverable**: `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only. Exactly three insertions into the `#ifdef ARDUINO` region (zero deletions, zero changes elsewhere):
1. Three `#include` lines at the top of the ARDUINO region (`hardware/pio.h`, `hardware/clocks.h`, `PIOProgram.h`).
2. A bannered state block `// ---- WS2812 onboard mirror state (P1.M5.T3.S1) ----` after the External-LED state block and before `void setup()` — containing the embedded pioasm-generated WS2812 PIO program, the `PIOProgram` static, mirror state statics, and two static helpers (`wsMirrorInit`, `wsMirrorUpdate`).
3. Two call sites: `wsMirrorInit()` in setup() (after the boot banner, before `watchdog_enable`), and `wsMirrorUpdate(s_led.mode(), ledOn);` inside the loop's LED stage, immediately after the `digitalWrite(PIN_LED, ...)` line.

NO new files. NO native tests (item contract: "Mock: none on device" — pattern semantics are native-tested in P1.M5.T1.S1; the ARDUINO region is excluded from native builds). NO config.h change (§8 frozen — `USE_ONBOARD_RGB` and `RGB_BRIGHTNESS` already exist). NO new libraries/deps (see Critical Research Finding below — the approach uses ONLY what ships inside the installed core).

**Success Definition**:
- `pio run -e pico` → `[SUCCESS]` (the mirror code compiles under arm-gcc + arduino-pico 5.4.1; the exact pattern was pre-verified with a local smoke-test build this session — see research/notes.md §4).
- `pio test -e native` → exit 0, all 11 projects `[PASSED]` — regression only, zero new tests, UNGUARDED region byte-stable.
- Structure greps: mirror block present exactly once, after the T2S1 state block, before `void setup()`; `wsMirrorUpdate` called exactly once in loop() and it sits inside the LED stage right after the GP14 write; setup() still starts with the valve-LOW line; watchdog_enable is still the last statement of setup().
- `git status --short` → ONLY `firmware/jug_filler/jug_filler.ino` modified; the diff is purely additive.
- With `USE_ONBOARD_RGB = true` on hardware: onboard LED is dark at boot, mirrors every external-LED pattern in the mapped color at 60/255 brightness (bench verification itself is P1.M7.T2.S1's deliverable). With `USE_ONBOARD_RGB = false`: no WS2812 code executes, identical binary behavior otherwise.

## User Persona

**Target User**: The implementing agent of P1.M6.T2.S1 (integration loop) and P1.M7.T2.S1 (bench dry-run) — this task is the last LED infrastructure they consume.

**Use Case**: At the bench and in service, the external green LED is authoritative (PRD §4) but is a single-color indicator mounted at the valve; the onboard RGB gives an at-a-glance COLOR-coded duplicate on the board itself (blue=filling/confirming, green=done, red=fault) without any wiring — GP16 and the LED are already on the PCB, powered from 3V3 (PRD §2.1/§2.2).

**User Journey**: power-on → onboard LED dark (mirror pushes black once at init) → short press (later, P1.M6) → fill engine `setMode(Solid)` → external LED solid AND onboard LED blue solid → CONFIRM still blue → DONE → both run the 3×(200/200)+1000 pulse, onboard in green → FAULT → both run the n-blink groups, onboard in red → ack long-press during FILLING → both blink 3× fast (onboard: blue, since mode stays Solid) then resume solid.

**Pain Points Addressed**: a WS2812 left undriven shows whatever it last latched (or random on some clones) — deterministic boot-dark matters; a naive implementation pulls in Adafruit_NeoPixel (violates PRD §3 "builds in Arduino IDE with just the core installed") or uses the nonexistent bundled `WS2812` library (build failure — see Critical Research Finding); full-brightness white would overdraw the 3V3 LDO budget (PRD §2.2 cap ≤60/255); re-sending every loop pass would flood the PIO FIFO for no benefit.

## Why

- PRD §4 (states & LED contract): WS2812 mirror column is normative — Off/Blue solid/Blue solid/Same pattern in green/Same pattern in red. The external LED remains AUTHORITATIVE; the mirror must only ever copy the engine's output.
- PRD §2.1: GP16 is the onboard WS2812, "Firmware only; optional, behind a config flag" → `USE_ONBOARD_RGB`.
- PRD §2.2 (via config.h comment): brightness must stay ≤ 60/255 (3V3 LDO budget) → scale RGB values written; do NOT depend on any library brightness API (none exists in the chosen approach anyway).
- PRD §3: two-file firmware (sketch + config.h), must build in Arduino IDE with just the core installed → the driver embeds a pioasm-generated program header and uses core-internal helpers; NO external library.
- PRD §10: loop never blocks → `pio_sm_put_blocking` cannot block at our update rate (FIFO drained in 30 µs, pushes ≥100 ms apart — research/notes.md §5).
- Item contract: INPUT = LedPatternEngine modes/ledOn (P1.M5.T1.S1 landed) + config flags; LOGIC in the ARDUINO region fully gated by `USE_ONBOARD_RGB`, 1 pixel on GP16, color map per §4 mirror column, "update the pixel only when engine output changes"; OUTPUT = mirror consumed by the integration loop (P1.M6.T2.S1); DOCS = none (internal).

## What

Additive edits to the ARDUINO region of `firmware/jug_filler/jug_filler.ino` only (exact code in Implementation Blueprint):

1. **Includes** (top of ARDUINO region, after `#include <hardware/watchdog.h>`): `#include <hardware/pio.h>`, `#include <hardware/clocks.h>`, `#include "PIOProgram.h"` — all three ship inside the installed arduino-pico core (verified; research/notes.md §2).
2. **State block + helpers** (after the `// ---- end External LED driver state ...` banner, before `void setup()`):
   - Embedded, verbatim pioasm-generated WS2812 program (`ws2812_program_instructions[]` = `0x6321, 0x1223, 0x1200, 0xa242`, the `pio_program` struct, `ws2812_program_get_default_config`, `ws2812_program_init`) — assembled from the canonical `pico-sdk/.../pico_status_led/ws2812.pio` with the installed pioasm (research/notes.md §3).
   - Statics: `static PIOProgram s_wsPgm(&ws2812_program);` + `s_wsPio`/`s_wsSm`/`s_wsOff`/`s_wsOk` + `s_wsLastGrb` (change-detection, initialized to force the first black write).
   - `constexpr uint32_t WS2812_BITRATE_HZ = 800000;` — protocol constant, NOT a §8 tunable (precedent: `ADC_SAMPLE_PACE_MS`, `ACK_BLINK_COUNT`).
   - `static void wsMirrorInit()` — `USE_ONBOARD_RGB`-gated: `s_wsPgm.prepare(&s_wsPio, &s_wsSm, &s_wsOff, PIN_NEOPIXEL, 1)`; on failure one `logLine` and permanent disable (fail-safe: external LED unaffected); on success `ws2812_program_init(s_wsPio, (uint)s_wsSm, (uint)s_wsOff, PIN_NEOPIXEL, (float)WS2812_BITRATE_HZ, false)`, set `s_wsOk`, push boot-dark black.
   - `static void wsMirrorUpdate(LedPatternEngine::Mode mode, bool ledOn)` — `USE_ONBOARD_RGB`- and `s_wsOk`-gated: color map (Solid→blue, DonePulse→green, FaultBlink→red, Off→black; black when `!ledOn`), scale each channel `× RGB_BRIGHTNESS / 255`, pack GRB word `(g<<24)|(r<<16)|(b<<8)`, skip if identical to `s_wsLastGrb`, else `pio_sm_put_blocking`.
3. **setup() call**: `wsMirrorInit();` on its own line(s) AFTER the boot banner `logLine("%s", banner);` and BEFORE `watchdog_enable(...)` (watchdog stays LAST — arch boot-order invariant).
4. **loop() call**: `wsMirrorUpdate(s_led.mode(), ledOn);` as the next line after `digitalWrite(PIN_LED, ledOn ? HIGH : LOW);` — same `ledOn` local, same pass, ONE engine tick total (frozen caller contract of P1.M5.T1.S1: "mirror: color from led.mode(), level from tick()").

NOT in scope (owners — do NOT implement here): mode selection (`setMode`/`triggerAck` — fill engine P1.M6.T1.S2 routed by P1.M6.T2.S1); any change to `LedPatternEngine`, the T2S1 driver code, `config.h` (§8 frozen), `platformio.ini`, or any test; any RGBW/white support (onboard LED is RGB); any fade/animation beyond the §4 map; on-device bench execution (P1.M7.T2.S1); docs (P2.M1.T3.S1).

### Success Criteria

- [ ] ARDUINO region gains exactly: 3 includes, 1 bannered mirror state block (after the External LED state block, before `void setup()`), 1 `wsMirrorInit();` call in setup() (between banner log and watchdog_enable), 1 `wsMirrorUpdate(s_led.mode(), ledOn);` call in loop() (immediately after the GP14 digitalWrite)
- [ ] Color map verified by reading the code: Off→black; Solid→blue; DonePulse→green; FaultBlink→red; `!ledOn`→black regardless of mode; every channel value written is `channel × RGB_BRIGHTNESS / 255` (full-scale 255 → exactly 60)
- [ ] Both helpers begin with `if (!USE_ONBOARD_RGB ...) return;` — flipping the config flag to `false` (and rebuilding) compiles and eliminates all mirror behavior
- [ ] Pixel updates occur ONLY when the packed GRB word changes (`s_wsLastGrb` comparison); black is written exactly once at init
- [ ] `PIN_NEOPIXEL` used for the pin — no literal `16` anywhere in the new code
- [ ] UNGUARDED region byte-stable; region-marker count still exactly 2 (`#ifdef ARDUINO` / `#endif // ARDUINO`); setup() line 1 still `digitalWrite(PIN_VALVE, LOW);`; watchdog_enable still the final statement of setup(); nothing inserted above `watchdog_update()` in loop()
- [ ] `pio run -e pico` → `[SUCCESS]`; `pio test -e native` → exit 0, all 11 projects `[PASSED]`
- [ ] `git status --short` → only ` M firmware/jug_filler/jug_filler.ino`; diff contains no removed lines

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete copy-paste-ready code (PIO program bytes included) is in the Implementation Blueprint and was COMPILED SUCCESSFULLY this session in a throwaway project with an env identical to the repo's `[env:pico]` (research/notes.md §4); every edit anchor is quoted by exact text; the consumed surfaces (`LedPatternEngine::tick/mode`, `USE_ONBOARD_RGB`, `RGB_BRIGHTNESS`, T2S1's `ledOn` local) were read from the live files; the critical external-dependency question was resolved by direct local verification, not documentation claims.

### ⚠️ Critical Research Finding — the assumed library does not exist

`architecture/external_deps.md` §5 says arduino-pico bundles `libraries/WS2812` with a `WS2812(pin, count, pio)` class. **FALSE for the installed core** (arduino-pico 5.4.1, PlatformIO package `1.50401.0`, verified by listing `libraries/`, case-insensitive grep of `libraries/` + `cores/rp2040/`, and full non-shallow git history — research/notes.md §1). Any `#include <WS2812.h>` or `WS2812 strip(...)` **will not compile**. The correct dependency-free path (ALL verified locally):
- The canonical WS2812 PIO program ships INSIDE the core's bundled pico-sdk at `pico-sdk/src/rp2_common/pico_status_led/ws2812.pio` (with `status_led.c` as the official usage example).
- The core exposes the public loader class `PIOProgram` (`cores/rp2040/PIOProgram.h`) — the same helper the core's own `Tone`/`SerialPIO` use.
- Pre-assembled `.pio.h` headers are an established core pattern (`tone2.pio.h`, `pio_uart.pio.h`, `ccount.pio.h`).
- This is what satisfies PRD §3's Arduino-IDE-with-just-the-core constraint (Adafruit_NeoPixel was rejected: external dep).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified TEXT anchors (line numbers shift — T2S1 is
        landing in parallel):
        (1) INCLUDE ANCHOR — `#include <hardware/watchdog.h>` at the top of
        the ARDUINO region; add your three includes directly below it.
        (2) STATE-BLOCK ANCHOR — the banner line `// ---- end External LED
        driver state (used by loop, shared with P1.M5.T3.S1) ----`; insert
        your mirror block AFTER it, BEFORE `void setup() {`.
        (3) CONSUMED ENGINE — `class LedPatternEngine` (UNGUARDED region):
        read `tick(uint32_t) -> bool`, `mode() -> Mode` (enum Off/Solid/
        DonePulse/FaultBlink), and the caller-contract comment "mirror:
        color from led.mode(), level from tick()". Do NOT modify it.
        (4) SETUP — the boot banner block `formatBootBanner(banner, ...)` +
        `logLine("%s", banner);`, then `watchdog_enable(...)` LAST; your
        `wsMirrorInit();` goes between them.
        (5) LOOP — the LED stage T2S1 lands at the end of loop(): `const
        bool ledOn = s_led.tick(nowMs);` then `digitalWrite(PIN_LED, ledOn
        ? HIGH : LOW);`; your call goes right after. Uses the loop's single
        `nowMs` snapshot — but the mirror needs NO time input at all.
  gotcha: if the loop LED stage (`s_led.tick(nowMs)`) is MISSING, T2S1 has
        not finished landing — STOP and report (precondition, Task 0).

- file: firmware/jug_filler/config.h
  why: READ-ONLY. Section "---- 6. LED patterns" already defines
        `constexpr bool USE_ONBOARD_RGB = true;` and `constexpr uint8_t
        RGB_BRIGHTNESS = 60;` (0–255, must stay ≤ 60). Reference BOTH by
        name. Do NOT add anything — §8 is frozen.
  gotcha: there is NO RGB pin constant in config.h (§8 never defined one)
        and none may be added — use the board definition's `PIN_NEOPIXEL`
        (= 16, see below), never a literal 16.

- file: plan/001_53984f832581/P1M5T3S1/research/notes.md
  why: this task's research: §1 the WS2812-library nonexistence proof, §2–§3
        the verified PIO path + verbatim assembled program, §4 smoke-test
        SUCCESS evidence, §5 pin/electrical/FIFO facts, §6 GRB format, §7
        codebase anchors.

- file: plan/001_53984f832581/P1M5T2S1/PRP.md
  why: CONTRACT for the T2S1 driver landing in parallel — its "What" defines
        the LED stage your call site joins (`const bool ledOn = s_led.tick
        (nowMs); digitalWrite(PIN_LED, ledOn ? HIGH : LOW);` as the LAST
        loop stage) and its trailing-comment plan explicitly reserves "the
        mirror beside the GP14 write reusing ledOn" for THIS task. Treat as
        implemented-exactly.
  section: "What" + "Success Definition"

- file: plan/001_53984f832581/P1M5T1S1/PRP.md
  why: CONTRACT for the engine you consume — frozen public surface, one-
        tick-per-pass caller contract, mode() semantics. Treat as
        implemented-exactly.
  section: "What" (public surface) + Integration Points

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  why: loop stage order (LED drivers = final stage: "GP14 + WS2812 mirror"),
        hardware code only inside #ifdef ARDUINO, no blocking in loop,
        watchdog LAST in setup.
  section: boot order + loop order lines

- url: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_status_led/ws2812.pio
  why: upstream source of the embedded program (identical to the copy
        bundled in the installed core — verified locally; URL is provenance
        only, do not fetch).
  critical: constants T1=3/T2=3/T3=4 are chosen upstream for WS2812/WS2812B/
        SK6812 compatibility — do NOT "retune" them.

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: board facts — onboard WS2812 on GP16 powered from 3V3 (why brightness
        is capped); confirms the variant's PIN_NEOPIXEL designation.
```

### Current Codebase tree (verified this session; T2S1 mid-landing)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] raspberrypi/waveshare_rp2040_zero/arduino (compile-only);
│                             # [env:native] Unity host tests — NO changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # ~996+ lines: UNGUARDED pure logic (time utils, ButtonClassifier,
│   │                         #   ReadingAggregator, BaselineCapture, TripDetector, guards,
│   │                         #   backstop, RecoveryClock/FillDeadline, LedPatternEngine) +
│   │                         #   ARDUINO region (logLine, BOOTSEL state, ADC state, T2S1's
│   │                         #   External LED state block [landed], setup(), loop()) ← EDIT
│   └── config.h              # USE_ONBOARD_RGB / RGB_BRIGHTNESS already present — READ-ONLY
├── test/                     # 11 Unity projects, all green — regression gate only
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0 at ~/.local/bin/pio;
# installed packages: framework-arduinopico 1.50401.0 = arduino-pico 5.4.1 (git 0affea4),
# toolchain-rp2040-earlephilhower, tool-pioasm-rp2040-earlephilhower; host g++, Unity 2.6.1.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): +3 includes, +WS2812 mirror state block
│                             #   (embedded PIO program, statics, wsMirrorInit/wsMirrorUpdate),
│                             #   +wsMirrorInit() call in setup(), +wsMirrorUpdate() call in
│                             #   the loop LED stage
└── (no new files — no test dir, no library deps, by design; see item contract)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — there is NO bundled WS2812 library (external_deps.md §5 is wrong
//   for the installed core 5.4.1 — verified; research/notes.md §1). Use the
//   embedded-PIO-program + PIOProgram approach in the Blueprint below. It was
//   compile-verified this session against the exact installed toolchain.

// CRITICAL — brightness is enforced by SCALING the written channel values:
//   scaled = channel * RGB_BRIGHTNESS / 255 (item contract; no library
//   brightness API exists to lean on). Full-scale 255 -> exactly 60. Never
//   write raw 255 to a channel.

// CRITICAL — ONE engine tick per pass: reuse T2S1's `ledOn` local; NEVER call
//   s_led.tick() a second time for the mirror. Color comes from
//   s_led.mode(), level from the same ledOn (frozen T1.S1 caller contract).

// CRITICAL — word format is GRB, not RGB: (g<<24)|(r<<16)|(b<<8), MSB-first
//   24-bit autopull (official pico-sdk status_led.c: "0xGGRRBB00").

// CRITICAL — USE_ONBOARD_RGB is a constexpr bool, NOT a macro: `#if
//   USE_ONBOARD_RGB` is illegal. Gate with plain `if (!USE_ONBOARD_RGB)
//   return;` at the top of both helpers (compiler folds it away when false).

// CRITICAL — PIN_NEOPIXEL (defined by the board variant = (16u)) is the pin;
//   do not add a config constant (§8 frozen) and do not hardcode 16.

// GOTCHA — update-only-on-change is also a correctness property, not just an
//   optimization: WS2812 latches on data-line idle >50 µs; unchanged colors
//   are never re-sent (the LED holds its latch indefinitely). Pattern-driven
//   changes are >= 100 ms apart (fastest edge = ACK_BLINK_MS) — 3 orders of
//   magnitude of latch margin; no explicit delay is ever needed.

// GOTCHA — pio_sm_put_blocking cannot stall the loop here: the joined TX
//   FIFO is 8 deep and the SM drains a frame in ~30 µs while pushes are
//   >= 100 ms apart. (PRD §10 non-blocking loop stays true.)

// GOTCHA — static-init order is safe: PIOProgram's constructor only stores a
//   pointer; the pio claim/load happens inside prepare() called from setup().

// GOTCHA — helpers must be DEFINED above setup() (they are — the state block
//   sits before it); file-scope statics defined-before-use need no prototypes
//   (same as every landed state block). The UNGUARDED region must stay
//   byte-identical — native tests #include the .ino and are the regression gate.

// GOTCHA — never `pio test -e pico` (no on-device transport): `pio run -e
//   pico` is the compile gate; on-LED visual verification is P1.M7.T2.S1's
//   bench deliverable, NOT this task's.

// GOTCHA — designated initializers in pio_program must stay in declaration
//   order (instructions, length, origin[, pio_version]) — g++ requirement;
//   the Blueprint's form is the verified-compiling one.
```

## Implementation Blueprint

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: VERIFY preconditions (do not skip)
  - RUN: grep -n "s_led.tick(nowMs)" firmware/jug_filler/jug_filler.ino
    → exactly one hit inside loop()'s LED stage (T2S1 landed). If missing:
    STOP and report — P1.M5.T2.S1 is still landing.
  - RUN: grep -c "class LedPatternEngine" firmware/jug_filler/jug_filler.ino → 1
  - RUN: grep -n "USE_ONBOARD_RGB\|RGB_BRIGHTNESS" firmware/jug_filler/config.h → both present
  - RUN: grep -n "P1.M5.T3.S1" firmware/jug_filler/jug_filler.ino → the mirror
    is NOT already implemented (only engine-comment mentions of the task id)

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — includes (top of ARDUINO region)
  - FIND: `#include <hardware/watchdog.h>   // pico-sdk watchdog, shipped inside arduino-pico (verified)`
  - ADD directly below it:
      #include <hardware/pio.h>       // pico-sdk PIO — WS2812 driver engine (P1.M5.T3.S1)
      #include <hardware/clocks.h>    // clock_get_hz(clk_sys) for the PIO clkdiv
      #include "PIOProgram.h"         // arduino-pico core helper: claim SM + load program
  - WHY these paths: all three verified present in the installed core
    (cores/rp2040/PIOProgram.h is on the sketch include path — the core's own
    Tone/SerialPIO include it exactly this way).

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — mirror state block + helpers
  - FIND: `// ---- end External LED driver state (used by loop, shared with P1.M5.T3.S1) ----`
  - INSERT after it (and before `void setup() {`) the COMPLETE block from
    "Implementation Patterns & Key Details" below, verbatim.
  - NAMING: house style — bannered block comment `// ---- WS2812 onboard mirror
    state (P1.M5.T3.S1) ----` ... `// ---- end WS2812 onboard mirror state
    (used by setup/loop, kept for P1.M6.T2.S1/P1.M7.T2.S1) ----`; statics
    prefixed `s_ws` (precedents: s_button, s_adc, s_led); helpers
    wsMirrorInit/wsMirrorUpdate (camelCase, like formatBootBanner).
  - PLACEMENT: after the External LED state block, before void setup().

Task 3: MODIFY firmware/jug_filler/jug_filler.ino — setup() call
  - FIND inside setup(): `  logLine("%s", banner);` then (a few lines later)
    `  watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true);`
  - INSERT between them (aligned with the surrounding 2-space style, with a
    short comment):
      // Onboard WS2812 mirror (P1.M5.T3.S1): GP16, config-gated, brightness-
      // capped; pushes black once so boot is deterministically dark. Must
      // precede watchdog_enable (which stays LAST, arch boot order).
      wsMirrorInit();
  - PRESERVE: `digitalWrite(PIN_VALVE, LOW);` remains the FIRST executable
    line of setup(); watchdog_enable remains the LAST statement.

Task 4: MODIFY firmware/jug_filler/jug_filler.ino — loop() call site
  - FIND the LED stage at the end of loop(): `  digitalWrite(PIN_LED, ledOn ? HIGH : LOW);`
  - ADD immediately after it (same 2-space indent):
      wsMirrorUpdate(s_led.mode(), ledOn);  // PRD §4 mirror: color from mode, level from same tick
  - PRESERVE: one tick per pass (reuse ledOn — never a second tick call);
    nothing inserted above watchdog_update(); the LED stage remains the LAST
    loop stage.

Task 5: VALIDATE (see Validation Loop) — pio run -e pico, pio test -e native,
  structure greps, git status. Fix and iterate until all green.
```

### Implementation Patterns & Key Details

The mirror state block — COMPLETE, copy-paste-ready, **compile-verified this session** (research/notes.md §4) against the exact installed toolchain. Paste verbatim between the External-LED state block and `void setup()`:

```cpp
// ---- WS2812 onboard mirror state (P1.M5.T3.S1) ----
// Hardware side of the PRD §4 mirror column: the onboard WS2812 on GP16
// (variant constant PIN_NEOPIXEL — firmware-only pin, PRD §2.1; powered from
// 3V3, hence the RGB_BRIGHTNESS <= 60/255 LDO cap, PRD §2.2) duplicates the
// AUTHORITATIVE external green LED with color coding: IDLE off (black),
// FILLING/CONFIRM blue solid, DONE green in the done-pulse pattern, FAULT red
// in the blink-code pattern. Color comes from s_led.mode(), level from the
// SAME ledOn the GP14 driver wrote (one engine tick per pass — frozen
// P1.M5.T1.S1 caller contract); the ack overlay shows through as a blue blink
// because the mode stays Solid while tick() masks it.
//
// Driver: NO external library (none is bundled with the installed core —
// verified; research notes §1). Instead this embeds the pico-sdk's canonical
// WS2812 PIO program (pico-sdk/src/rp2_common/pico_status_led/ws2812.pio,
// shipped inside arduino-pico), pre-assembled with the installed pioasm —
// the same pattern as the core's own tone2.pio.h — and loads it through the
// core's PIOProgram helper (the core's Tone/SerialPIO loader). Builds in
// Arduino IDE with just the core (PRD §3).
//
// Update discipline: the pixel is written ONLY when the packed GRB word
// changes (s_wsLastGrb). WS2812 latches on data-line idle > 50 us and holds
// color indefinitely, so unchanged colors are never re-sent; pattern-driven
// changes are >= ACK_BLINK_MS (100 ms) apart — 3 orders of magnitude of
// latch margin, no waits. pio_sm_put_blocking cannot stall the loop: the
// joined TX FIFO (8 deep) drains a 24-bit frame in ~30 us while pushes are
// >= 100 ms apart (PRD §10 non-blocking invariant).
constexpr uint32_t WS2812_BITRATE_HZ = 800000;  // WS2812 protocol bit rate — NOT a §8 tunable (task contract; precedent ADC_SAMPLE_PACE_MS)

// ---- embedded ws2812.pio.h — autogenerated by pioasm from pico-sdk's
// ws2812.pio (do not hand-edit; T1/T2/T3 chosen upstream for WS2812/WS2812B/
// SK6812 compatibility) ----
#define ws2812_wrap_target 0
#define ws2812_wrap 3
#define ws2812_pio_version 0

#define ws2812_T1 3
#define ws2812_T2 3
#define ws2812_T3 4

static const uint16_t ws2812_program_instructions[] = {
            //     .wrap_target
    0x6321, //  0: out    x, 1            side 0 [3]
    0x1223, //  1: jmp    !x, 3           side 1 [2]
    0x1200, //  2: jmp    0               side 1 [2]
    0xa242, //  3: nop                    side 0 [2]
            //     .wrap
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
    .pio_version = ws2812_pio_version,
};

static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812_wrap_target, offset + ws2812_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    float div = clock_get_hz(clk_sys) / (freq * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
// ---- end embedded ws2812.pio.h ----

static PIOProgram s_wsPgm(&ws2812_program);  // claims SM + loads program at prepare()
static PIO        s_wsPio = pio0;            // PIO block prepare() selected (unused until then)
static int        s_wsSm  = -1;              // state machine index (-1 until prepared)
static int        s_wsOff = -1;              // program load offset (-1 until prepared)
static bool       s_wsOk  = false;           // mirror live (init succeeded AND config enabled)
static uint32_t   s_wsLastGrb = 0xFFFFFFFFu; // last pushed GRB word (init value forces first write)

// setup-time init. Fully gated by USE_ONBOARD_RGB (constexpr bool — a plain
// if, folded away when false; it is not a macro and cannot be #if'd). On PIO
// allocation failure: one diagnostic line, mirror permanently off — the
// external LED (authoritative, PRD §4) is unaffected: fail-safe direction.
static void wsMirrorInit() {
    if (!USE_ONBOARD_RGB) return;
    if (!s_wsPgm.prepare(&s_wsPio, &s_wsSm, &s_wsOff, PIN_NEOPIXEL, 1)) {
        logLine("RGB mirror init FAILED (no PIO SM)");  // mirror off; external LED unaffected
        return;
    }
    ws2812_program_init(s_wsPio, (uint)s_wsSm, (uint)s_wsOff, PIN_NEOPIXEL,
                        (float)WS2812_BITRATE_HZ, false);  // RGB (not RGBW), 800 kHz
    s_wsOk = true;
    wsMirrorUpdate(LedPatternEngine::Mode::Off, false);  // deterministic boot-dark (PRD §4 IDLE)
}

// Per-pass mirror update. color = mode() per PRD §4 mirror column; level =
// the SAME ledOn written to GP14 (includes the ack overlay: mode stays Solid
// so the blink shows blue — mirror fidelity over color purity). Brightness
// cap by SCALING each channel: channel * RGB_BRIGHTNESS / 255 (the map only
// writes 0 or 255 per channel, so scaled values are exactly 0 or 60). Word
// format GRB MSB-first 24-bit autopull: (g<<24)|(r<<16)|(b<<8) — official
// pico-sdk status_led.c "0xGGRRBB00". Writes only on change (see block
// comment above).
static void wsMirrorUpdate(LedPatternEngine::Mode mode, bool ledOn) {
    if (!USE_ONBOARD_RGB || !s_wsOk) return;
    uint8_t r = 0, g = 0, b = 0;
    if (ledOn) {
        switch (mode) {
            case LedPatternEngine::Mode::Solid:      b = 255; break;  // FILLING / CONFIRM
            case LedPatternEngine::Mode::DonePulse:  g = 255; break;  // DONE attention pulse
            case LedPatternEngine::Mode::FaultBlink: r = 255; break;  // FAULT blink codes
            case LedPatternEngine::Mode::Off:        break;           // IDLE (ledOn false anyway)
        }
    }
    const uint32_t grb = (((uint32_t)g * RGB_BRIGHTNESS / 255u) << 24)
                       | (((uint32_t)r * RGB_BRIGHTNESS / 255u) << 16)
                       | (((uint32_t)b * RGB_BRIGHTNESS / 255u) << 8);
    if (grb == s_wsLastGrb) return;  // update only when engine output changes
    s_wsLastGrb = grb;
    pio_sm_put_blocking(s_wsPio, (uint)s_wsSm, grb);
}
// ---- end WS2812 onboard mirror state (used by setup/loop, kept for P1.M6.T2.S1/P1.M7.T2.S1) ----
```

Loop call site (inside the T2S1 LED stage, final loop stage) — the added line in context:

```cpp
  // ---- External LED drive (P1.M5.T2.S1) + WS2812 mirror (P1.M5.T3.S1) ----
  const bool ledOn = s_led.tick(nowMs);           // ONE tick per pass (frozen engine contract)
  digitalWrite(PIN_LED, ledOn ? HIGH : LOW);      // authoritative external green LED (GP14)
  wsMirrorUpdate(s_led.mode(), ledOn);            // PRD §4 mirror: color from mode, level from same tick
```

(Keep whatever banner/comment lines T2S1 actually landed around its two lines — add ONLY the `wsMirrorUpdate` line.)

### Integration Points

```yaml
CONFIG: none — config.h is FROZEN. USE_ONBOARD_RGB (bool) and RGB_BRIGHTNESS
  (60) already exist in section "---- 6. LED patterns"; PIN_NEOPIXEL (16u)
  comes from the board variant header, not config (no §8 constant exists and
  none may be added).

PIO RESOURCES: one state machine + 4 instruction slots in one PIO block,
  claimed via PIOProgram::prepare() at setup() time. Nothing else in this
  firmware uses PIO (tone/SerialPIO unused), so no contention; if a future
  task adds PIO users, PIOProgram's shared-load map handles coexistence.

DOWNSTREAM (P1.M6.T2.S1): the integration loop inserts fill-engine routing
  ABOVE the LED stage; its setMode() calls then reach the mirror through the
  same s_led instance — zero additional integration work (OUTPUT contract of
  this item). P1.M7.T2.S1 bench-verifies the mirror colors/patterns against
  the external LED.

LOGGING: exactly ONE possible line (init failure). No per-pass logging —
  mode-change logs belong to the fill engine (P1.M6.T2.S1).
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Compile gate — the ONLY device-side gate available without hardware.
pio run -e pico
# Expected: ========================= [SUCCESS] ...
#   (The exact code pattern above compiled [SUCCESS] this session in a
#   throwaway project with an identical env — a failure means a transcription
#   deviation: re-diff your inserted block against the Blueprint.)

# Regression gate — native tests #include the .ino; the UNGUARDED region must
# be untouched for all 11 projects to stay green.
pio test -e native
# Expected: exit 0, all of: test_seam test_time_utils test_log_utils test_button
#   test_reading test_baseline test_trip test_guards test_backstop
#   test_deadline test_led_pattern -> [PASSED]

# NO ruff/mypy in this project (C++/PlatformIO). NO pio test -e pico (no
# on-device transport exists).
```

### Level 2: Structure & Diff Validation (this task's "unit" checks)

```bash
# Mirror block present exactly once, correctly placed (after T2S1 state block, before setup):
grep -c "WS2812 onboard mirror state (P1.M5.T3.S1)" firmware/jug_filler/jug_filler.ino          # -> 1
grep -n "end External LED driver state\|WS2812 onboard mirror state\|^void setup()" firmware/jug_filler/jug_filler.ino
#   -> banner order must be: ...end External LED driver state... THEN ...WS2812 onboard mirror... THEN void setup()

# Includes landed at the top of the ARDUINO region:
grep -n "hardware/pio.h\|hardware/clocks.h\|PIOProgram.h" firmware/jug_filler/jug_filler.ino    # -> 3 hits, all after #ifdef ARDUINO

# Call sites: exactly one each, correctly placed:
grep -c "wsMirrorInit();" firmware/jug_filler/jug_filler.ino                                     # -> 1 (in setup)
grep -c "wsMirrorUpdate(s_led.mode(), ledOn);" firmware/jug_filler/jug_filler.ino               # -> 1 (in loop, after the GP14 write)

# Invariants preserved:
grep -n "watchdog_enable" firmware/jug_filler/jug_filler.ino   # -> after wsMirrorInit() in setup()
sed -n '/^void setup/,/^  digitalWrite(PIN_VALVE, LOW);/p' firmware/jug_filler/jug_filler.ino | head -2
#   -> valve LOW still the first executable line of setup()
grep -c "s_led.tick" firmware/jug_filler/jug_filler.ino        # -> 1 (single tick; mirror reuses ledOn)

# Config-name discipline (no literals, no config edits):
grep -n "PIN_NEOPIXEL" firmware/jug_filler/jug_filler.ino      # -> hits in init + update paths
git diff firmware/jug_filler/config.h                          # -> EMPTY (read-only file untouched)

# Purely additive diff:
git status --short   # -> only " M firmware/jug_filler/jug_filler.ino"
git diff --stat      # -> insertions only, zero deletions
```

### Level 3: Gating Simulation (config flag off)

```bash
# Verify the gate compiles both ways — temporarily flip the flag, compile, flip back.
# (This is a THROWAWAY local edit for validation ONLY — restore the file after!)
sed -i 's/constexpr bool     USE_ONBOARD_RGB   = true;/constexpr bool     USE_ONBOARD_RGB   = false;/' firmware/jug_filler/config.h
pio run -e pico            # Expected: [SUCCESS] — dead branch folds away, statics unused but legal
git checkout -- firmware/jug_filler/config.h   # RESTORE — config.h must end untouched
git diff --stat firmware/jug_filler/config.h   # -> empty (verify the restore!)
```

### Level 4: On-Device Bench Validation — P1.M7.T2.S1's deliverable, NOT this task's

No hardware run is required or possible from this task (no upload target configured). When P1.M7.T2.S1 executes its bench dry-run it must observe on the onboard LED: boot dark; blue solid through FILLING+CONFIRM; green 3×(200/200)+1000 pulse in DONE; red n-blink groups in FAULT; blue 3×(100/100) ack blink during FILLING long-press — each matching the external green LED's on/off stream exactly, at ~60/255 brightness. This PRP's Success Criteria end at Levels 1–3.

## Final Validation Checklist

### Technical Validation

- [ ] `pio run -e pico` → `[SUCCESS]`
- [ ] `pio test -e native` → exit 0, all 11 projects `[PASSED]` (regression only)
- [ ] All Level-2 structure greps return the expected counts/order
- [ ] Level-3 gate-off simulation compiled `[SUCCESS]` AND config.h restored byte-identical (`git diff` empty)

### Feature Validation

- [ ] All success criteria from "What" section met (block/banner/naming/placement, color map, brightness scaling, change-only updates, PIN_NEOPIXEL, single tick)
- [ ] Color map read-verified against PRD §4 mirror column: Off→black, Solid→blue, DonePulse→green, FaultBlink→red, `!ledOn`→black
- [ ] Full-scale channel values written are exactly `RGB_BRIGHTNESS` (60) — never 255
- [ ] Both helpers lead with the `USE_ONBOARD_RGB` guard; `s_wsOk` double-guards the update path
- [ ] Mirror follows the SAME ledOn stream as GP14 (ack overlay included) — mirror fidelity

### Code Quality Validation

- [ ] Follows house conventions: bannered state block, `s_ws*` statics, camelCase helpers, explicit-comment density matching sibling blocks
- [ ] Placement matches desired structure: state block between T2S1 block and setup(); calls inside existing stages; nothing above `watchdog_update()`; watchdog still last in setup()
- [ ] Anti-patterns avoided (see below) — especially: no external library, no second tick, no raw 255, no literal pin, no `#if` on a constexpr bool
- [ ] Diff purely additive; UNGUARDED region byte-stable; region-marker count still 2

### Documentation & Deployment

- [ ] Comments explain the WHY (no-library rationale, latch semantics, GRB format, fail-safe) — code is self-documenting; no external docs (item contract: DOCS none)
- [ ] Exactly one possible log line (init failure); no per-pass logging
- [ ] No new environment variables, no new files, no platformio.ini change

---

## Anti-Patterns to Avoid

- ❌ Don't use a WS2812/NeoPixel library — none is bundled with the installed core (verified); Adafruit_NeoPixel violates PRD §3's "just the core" constraint. Use the embedded PIO program.
- ❌ Don't call `s_led.tick()` twice per pass — the mirror consumes the SAME `ledOn` the GP14 write used.
- ❌ Don't write raw (un-scaled) channel values — RGB_BRIGHTNESS cap is an LDO-budget safety constraint (PRD §2.2), enforced by scaling.
- ❌ Don't add a `PIN_RGB`/`WS2812_PIN` constant to config.h — §8 is frozen; `PIN_NEOPIXEL` from the board variant is the canonical constant.
- ❌ Don't gate with `#if USE_ONBOARD_RGB` — it's a `constexpr bool`, not a macro; that silently evaluates wrong/illegal. Plain `if` folds it away.
- ❌ Don't re-send unchanged colors every pass — WS2812 latches; change-only updates are the contract.
- ❌ Don't touch setup() line 1 (valve LOW), the watchdog ordering, or anything above `watchdog_update()` in loop() — PRD §10 invariants.
- ❌ Don't add native tests for the mirror — the item contract excludes them; the UNGUARDED region must stay byte-stable.
- ❌ Don't "retune" the PIO timing constants or reorder the designated initializers — the embedded block is the pioasm-verified output; hand edits break timing or the build.

---

**Confidence Score: 9/10** — the exact implementation code was compile-verified this session against the identical toolchain/board env; the only residual risk is T2S1's parallel landing differing from its PRP (mitigated by Task 0's precondition check and text-anchored edits) and first-hardware behavior (owned by P1.M7.T2.S1, not this task).
