# PRP — P1.M1.T2.S2: Hardware watchdog service and unplug-safe serial logging

## Goal

**Feature Goal**: Make the firmware hang-proof and its logging stall-proof, per PRD §10: (a) the RP2040 hardware watchdog is enabled at 8000 ms (legal under the 8388 ms RP2040-E1 ceiling) at the end of `setup()` and fed by `watchdog_update()` as the FIRST statement of every `loop()` pass; (b) a single `logLine(const char* fmt, ...)` wrapper becomes the only blessed logging path — one bounded `vsnprintf` into a fixed 128-byte buffer, one bounded CDC write, newline-terminated, and NEVER gated on USB connection (in service the board is buck-powered with USB unplugged); (c) a boot banner reports the reset cause via `watchdog_caused_reboot()`, with the banner text built by a PURE, natively-tested `formatBootBanner()`.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: two `<stdarg.h>`/`<stdio.h>` includes; two explicit prototypes; `formatBootBanner()` in the UNGUARDED region; `#include <hardware/watchdog.h>`, `LOGLINE_MAX`, `logLine()` at the top of the ARDUINO region; setup() gains banner + `watchdog_enable` (LAST); `loop()`'s first statement becomes `watchdog_update()`.
- `test/test_log_utils.cpp` — NEW Unity test file (7 tests) pinning the pure banner builder: both reset-cause labels, return-value/strlen contract, cause discrimination, common prefix, and safe truncation.

**Success Definition**:
- `pio test -e native` → exit 0 with THREE test projects PASSED (`test_seam`, `test_time_utils`, `test_log_utils`).
- `pio run -e pico` → compile-only `[SUCCESS]` (proves `hardware/watchdog.h` + CDC writes compile under the Arduino builder).
- Structural greps: valve-LOW still the first executable line of `setup()`; `watchdog_update()` the first statement of `loop()`; zero non-comment `while` loops in firmware/; unguarded region still pure.
- `git status`: only the `.ino` modified and `test/test_log_utils.cpp` added. `config.h`, `platformio.ini`, README untouched (item: no config surface change).
- `logLine()` exists as the logging vocabulary for P1.M2.T2.S1, P1.M3.T1.S2, P1.M6.T2.S1 (their PRPs reference it).

## User Persona

**Target User**: The implementing agents of P1.M2–P1.M6 (button service, sensor chain, fill engine, loop wiring) — `logLine()` is their ONLY logging API. Secondary: the human on the bench watching the USB CDC log during P1.M7.T2.S1 dry-runs.

**Use Case**: Any later service logs a state transition / raw sensor count / backstop computation: `logLine("FILL opened t=%lu backstop=%umin", nowMs, minutes);` — one bounded line, newline-terminated, safe with no host attached. Independently, if any future code hangs >8 s, the watchdog resets the board → `setup()` line 1 closes the valve → IDLE (never auto-refill, PRD §5.3), and the next boot banner says `reset cause: WATCHDOG`, telling the bench operator exactly what happened.

**User Journey**: board powered from buck, USB unplugged → fill runs, `logLine` calls cost microseconds (buffer dropped by CDC, never blocks) → USB plugged on bench → banner + event lines appear immediately.

**Pain Points Addressed**: hang-with-valve-open (the catastrophic failure mode, killed structurally by the hardware watchdog); `while(!Serial)` freezes on unpowered-USB devices (banned — `logLine` never checks connection); unbounded `Serial.print` floods stalling the loop when no host drains CDC (killed by the fixed 128-byte bound); silent watchdog resets (killed by the banner's reset-cause line).

## Why

- PRD §10 (non-negotiable): "RP2040 hardware watchdog (~8s), fed every loop pass. A hang resets → valve closed → IDLE awaiting a press." This subtask is that sentence, as code.
- PRD §3 + §2.2: USB CDC at 115200, always on; in service the board runs from the buck with USB unplugged — logging must never stall the loop (no `while(!Serial)`, small writes).
- PRD §5.3: any reset — watchdog included — lands in IDLE with the valve closed; the banner's `reset cause: WATCHDOG` label makes §5.3 observable on the bench.
- Architecture (plan/001_53984f832581/architecture/firmware_architecture.md boot sequence): `setup(): … Serial.begin + banner/watchdog_caused_reboot → watchdog_enable(8000, true)`; `loop(): watchdog_update → …`; `logLine events` named as the logging edge of the loop. external_deps.md §2 pre-verified the exact pico-sdk watchdog API shipped inside arduino-pico, and §6 assigns THIS task ownership of the unplug-safe logging policy.
- Foundation for the whole changeset: every later subtask that logs (P1.M2.T2.S1, P1.M3.T1.S2, P1.M6.T2.S1) consumes `logLine()`; P1.M7.T2.S1's bench log capture reads its lines.

## What

Add the watchdog service and the logging primitive to the existing skeleton. No new config constants (`WATCHDOG_TIMEOUT_S` and `SERIAL_BAUD` already exist in config.h); no new libraries (pico-sdk watchdog header ships inside arduino-pico; `vsnprintf` is standard C). The pure/hardware seam dictates placement: banner TEXT generation is pure and native-tested; watchdog calls and CDC writes are `#ifdef ARDUINO` hardware glue.

**Normative decisions**:
- `logLine` formats with `vsnprintf` into a fixed stack buffer (`LOGLINE_MAX = 128`), truncating overlong lines (drop the excess, keep the newline) — NOT `Serial.printf` — so the byte bound is a structural property, identical on host and board.
- `logLine` never inspects connection state; it always writes. On format error (`n < 0`) it drops the line silently rather than emit garbage.
- Banner labels are fixed strings: `JugFiller boot (reset cause: WATCHDOG)` (38 chars) and `JugFiller boot (reset cause: POWER_OR_RESET)` (44 chars). `watchdog_caused_reboot()` is read ONCE, before `watchdog_enable()`, passed into the pure builder as a bool.
- `watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true)` runs LAST in `setup()` (everything above it already completed; the countdown then governs `loop()`); `pause_on_debug = true` so bench debugging never trips it.
- `watchdog_update()` is the first executable statement of `loop()` — nothing may be inserted above it, ever (P1.M7.T1.S1 structurally checks this).

### Success Criteria

- [ ] `.ino` prototypes block contains `size_t formatBootBanner(char*, size_t, bool);` and `void logLine(const char*, ...);` (explicit, per seam rule 3)
- [ ] UNGUARDED region contains `formatBootBanner()` using only `snprintf`; region-marker comments byte-stable; purity grep clean
- [ ] ARDUINO region starts with `#include <hardware/watchdog.h>`, defines `constexpr size_t LOGLINE_MAX = 128;` and `logLine()` exactly per the Task 2 listing
- [ ] `setup()`: line 1 still `digitalWrite(PIN_VALVE, LOW);`; after `analogReadResolution` come banner + `logLine("%s", banner)` then `watchdog_enable(...)` as the final statement; `watchdog_caused_reboot()` called exactly once
- [ ] `loop()`: first statement `watchdog_update();`
- [ ] `test/test_log_utils.cpp` (7 tests, explicit `main()`) passes natively: watchdog label, power/reset label, labels differ, common prefix `JugFiller boot`, truncation safety (8-byte buffer: return ≥ 8, strlen 7, NUL at [7], prefix kept), bufLen==0 untouched, banner fits 64 bytes
- [ ] `pio test -e native` exit 0, three projects PASSED; `pio run -e pico` `[SUCCESS]`
- [ ] Only 2 files in `git status` (`M` .ino, `??` test/test_log_utils.cpp); config.h/platformio.ini/README untouched
- [ ] No `while` loop anywhere in `firmware/` outside comments; no `delay(` in the ARDUINO region

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: complete verbatim listings for every edit region (prototypes, includes, unguarded addition, ARDUINO-region header, final `setup()`/`loop()`) and the complete test file with hand-verified expected values (research/notes.md §5–6). The watchdog API, ceiling, and unplug policy are pre-verified facts from external_deps.md §2/§6 with sources. The only prerequisite state (skeleton + S1 time helpers) is pinned by precondition greps below.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit — current skeleton (region markers, prototype block, setup
        with valve-LOW line 1, empty loop with the "P1.M1.T2.S2 makes watchdog_update()
        the first call here" placeholder comment you are replacing).
  gotcha: if `seamVersion` is still present, S1 (P1.M1.T2.S1) has not landed — STOP,
        wait/re-check; your edits assume elapsedMs/deadlinePassed are in place.

- file: plan/001_53984f832581/P1M1T2S1/PRP.md
  why: contract for the time helpers that share the unguarded region — your
        formatBootBanner is ADDED BELOW their "end time helpers" banner; do not touch
        their block, prototypes, or test files. Its test-convention gotchas (explicit
        main() on PIO 6.2.0, UL suffixes, one project per test_*.cpp) apply verbatim.

- file: plan/001_53984f832581/P1M1T1S2/PRP.md
  section: "Task 2 listing" (the skeleton) — the source of the exact lines your
        setup()/loop() edits preserve; "Task 4 listing" (test_seam.cpp shape).

- file: plan/001_53984f832581/architecture/external_deps.md
  section: "2. Hardware watchdog — VERIFIED" + "6. USB CDC Serial when unplugged — POLICY"
  why: the verified API surface (watchdog_enable(delay_ms, pause_on_debug),
        watchdog_update, watchdog_caused_reboot; 8388 ms RP2040-E1 ceiling; no Arduino
        wdt_* API) and the logging policy THIS task owns (never while(!Serial); small,
        line-oriented, bounded writes). Also "Risks & gotchas" items 3–4.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  why: boot sequence you must match (banner → watchdog_enable last; loop starts with
        watchdog_update) and the seam rules (hardware inside #ifdef ARDUINO, explicit
        prototypes, injected inputs for pure functions).
  gotcha: its layout diagram may show firmware/jug_filler.ino (pre-S2 path) — the real
        path is firmware/jug_filler/jug_filler.ino.

- file: firmware/jug_filler/config.h
  why: READ-ONLY. Confirms the two constants you consume — SERIAL_BAUD (115200,
        already wired into Serial.begin) and WATCHDOG_TIMEOUT_S (uint16_t 8; multiply
        by 1000UL for ms). NO new constants (item: no config surface change).

- file: test/test_seam.cpp
  why: the test-file shape to copy (include path, setUp/tearDown, explicit main(),
        assertion styles). It already asserts WATCHDOG_TIMEOUT_S==8 — config stays
        compatible automatically.

- file: plan/001_53984f832581/P1M1T2S2/research/notes.md
  why: full research digest — watchdog API verification with sources, unplug policy,
        vsnprintf-vs-Serial.printf rationale, banner string lengths (38/44 chars),
        hand-derived truncation expectations backing every test value.

- url: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_watchdog/include/hardware/watchdog.h
  why: the exact header shipped inside arduino-pico — signatures and the 8388 ms max
        comment (RP2040-E1) cited by the item contract.
  critical: watchdog_enable's 2nd arg is `bool pause_on_debug`; delay is uint32_t ms.

- url: https://raspberrypi.github.io/pico-sdk-doxygen/group__hardware__watchdog.html
  why: official API docs for the three calls; watchdog_caused_reboot() reads the
        bootcause scratch flag that survives until the next reset.

- url: https://arduino-pico.readthedocs.io/en/latest/serial.html
  why: arduino-pico CDC behavior — Serial.begin is always safe; writes with no USB
        host do not stall the sketch; why while(!Serial) is forbidden on this core.

- url: https://en.cppreference.com/w/c/io/vsnprintf
  why: vsnprintf/snprintf semantics the tests pin — returns the would-be length
        (int), writes at most bufLen-1 chars + NUL; bufLen==0 writes nothing.
  critical: truncation (return ≥ bufLen) is NOT an error — logLine truncates and
        still emits the newline; only n < 0 drops the line.
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # contains ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] unity + src filter
├── firmware/
│   └── jug_filler/
│       ├── jug_filler.ino    # skeleton + S1 time helpers (elapsedMs/deadlinePassed
│       │                     #   replace seamVersion); regions: UNGUARDED then
│       │                     #   #ifdef ARDUINO (setup/loop)
│       └── config.h          # all §8 tunables incl. SERIAL_BAUD, WATCHDOG_TIMEOUT_S — READ-ONLY
├── test/
│   ├── test_seam.cpp         # config assertions + helper call
│   └── test_time_utils.cpp   # S1 rollover tests (16)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified live): pio 6.2.0 at ~/.local/bin/pio, host g++ 16.2.1,
# raspberrypi platform + Unity 2.6.1 installed under ~/.platformio.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── jug_filler/
│       └── jug_filler.ino    # MODIFIED (additive): formatBootBanner (unguarded,
│                             #   pure) + logLine/watchdog glue (ARDUINO region) +
│                             #   boot banner + watchdog_enable in setup() +
│                             #   watchdog_update first in loop()
└── test/
    └── test_log_utils.cpp    # NEW: 7 Unity tests for formatBootBanner
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — purity grep trips on COMMENTS: the unguarded region is checked with
//   grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead"
// so formatBootBanner's comment must NOT contain those words (say "CDC log line",
// "boot banner", "host-visible" — never the banned tokens).

// CRITICAL: `#include <hardware/watchdog.h>` must sit INSIDE the #ifdef ARDUINO
// region (or a #ifdef ARDUINO include block) — host g++ cannot find pico-sdk headers;
// a bare top-level include breaks `pio test -e native` at compile time.

// CRITICAL: PIO 6.2.0 — every test_*.cpp needs an explicit int main()
// { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); } or the link fails with
// "undefined reference to `main'". Each test file is its OWN Unity project; after
// this task `pio test -e native` must report THREE projects.

// CRITICAL: WATCHDOG_TIMEOUT_S is uint16_t (8). Multiply as
// WATCHDOG_TIMEOUT_S * 1000UL (or cast to uint32_t first) — plain int math is fine
// at 8000 but keep the UL habit; NEVER raise the value (ceiling 8388 ms, RP2040-E1).

// CRITICAL: never gate on connection — no `while(!Serial)`, no `if(!Serial) return;`
// in logLine. USB is unplugged in service (PRD §2.2); CDC writes with no host are
// dropped by the core, not blocking (external_deps.md §6).

// PATTERN (from S1): non-tunable constants live as local constexpr beside their only
// consumer — LOGLINE_MAX (128) and the banner buffer (64) are NOT config.h entries.

// snprintf/vsnprintf return int (would-be length, possibly negative on error) while
// formatBootBanner returns size_t — cast deliberately; tests pin the return==strlen
// contract for fitting lines and return>=bufLen for truncated ones.

// Region-marker comment lines are STRUCTURAL ANCHORS for P1.M7.T1.S1 — copy them
// back byte-exact; only insert content BETWEEN them.

// loop()'s first statement is watchdog_update() — P1.M6.T2.S1 adds the rest BELOW it;
// P1.M7.T1.S1 will assert this structurally, so no blank line or comment above it
// (comment lines above would still pass that check, but keep it first-and-obvious).
```

## Implementation Blueprint

### Data models and structure

None — no new types. One `constexpr size_t LOGLINE_MAX = 128;` (ARDUINO region, sole consumer `logLine`), one 64-byte banner buffer local to `setup()`, two free functions. config.h models untouched.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first)
  - RUN: grep -c "seamVersion" firmware/jug_filler/jug_filler.ino
  - EXPECT: 0 (S1 landed). If non-zero → S1 not landed; STOP and re-check later —
    do NOT recreate or edit around the probe yourself.
  - RUN: grep -c "elapsedMs" firmware/jug_filler/jug_filler.ino   # EXPECT: ≥3

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — headers, prototypes, pure helper
  - EDIT 1 — after `#include "config.h"` add the two host-safe includes (Listing 1A).
  - EDIT 2 — prototype block: after the deadlinePassed prototype line, add the two
    new prototypes (Listing 1A).
  - EDIT 3 — UNGUARDED region: below the time helpers' "end time helpers" banner
    line, append formatBootBanner (Listing 1B). PRESERVE everything else byte-for-byte.

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — ARDUINO region glue
  - EDIT 4 — immediately after `#ifdef ARDUINO`, insert the watchdog include +
    LOGLINE_MAX + logLine (Listing 2A).
  - EDIT 5 — setup(): append the banner + watchdog_enable block AFTER the
    analogReadResolution line (Listing 2B). Nothing above `digitalWrite(PIN_VALVE,
    LOW);` changes — it stays the first executable line (PRD §10).
  - EDIT 6 — loop(): replace the placeholder comment body with `watchdog_update();`
    as the first statement + one orientation comment (Listing 2B).

Task 3: CREATE test/test_log_utils.cpp
  - IMPLEMENT: the complete Listing 3 verbatim (expected values hand-verified).
  - PATTERN: test_seam.cpp shape — same include line, setUp/tearDown, explicit
    main() (mandatory on PIO 6.2.0), UL/unsigned casts on size_t comparisons.
  - PLACEMENT: test/ (auto-becomes its own Unity project; no platformio.ini change).

Task 4: RUN native tests (PRIMARY GATE)
  - RUN: pio test -e native
  - EXPECT: THREE projects PASSED (test_seam 2 tests, test_time_utils 16,
    test_log_utils 7), exit 0. NEVER `pio test -e pico` (no transport there).

Task 5: RUN board compile-only check
  - RUN: pio run -e pico        # NEVER upload
  - EXPECT: [SUCCESS] — proves hardware/watchdog.h + Serial.write compile under the
    Arduino/ARM toolchain (it also proves the include is reachable in the IDE-style
    build, since arduino-pico ships the header).

Task 6: RUN structural/safety greps (Level 4 below)
  - EXPECT: "PURE OK", markers == 2, valve-first OK, watchdog-first-in-loop OK,
    while-loop scan shows only the two NEVER-comments, git scope exact.
```

**Listing 1A — includes + prototypes (exact insertions):**

```cpp
// after `#include "config.h"`:
#include <stdarg.h>   // va_list for the CDC log-line wrapper (host-safe)
#include <stdio.h>    // snprintf/vsnprintf — the only formatting engines (host-safe)
```

```cpp
// in the prototype block, after the deadlinePassed prototype:
size_t formatBootBanner(char* buf, size_t bufLen, bool watchdogCaused);  // pure banner text (P1.M1.T2.S2)
void logLine(const char* fmt, ...);  // bounded, newline-terminated CDC log line (P1.M1.T2.S2; ARDUINO region only)
```

**Listing 1B — unguarded region addition (verbatim):**

```cpp
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
```

**Listing 2A — top of the ARDUINO region (verbatim, inserted right after `#ifdef ARDUINO`):**

```cpp
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
```

**Listing 2B — setup() tail + loop() (final forms of the changed parts):**

```cpp
// setup(): everything above analogReadResolution is the S2 skeleton, untouched.
// Append AFTER the analogReadResolution line:

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

// loop() — replace the placeholder comment with:
void loop() {
  watchdog_update();   // FIRST STATEMENT — PRD §10: hang -> reset -> valve LOW -> IDLE
  // P1.M6.T2.S1 fills the loop below this line (BOOTSEL poll, ADC, fill engine,
  // LED, event logs via logLine). Nothing may ever be inserted above watchdog_update().
}
```

**Listing 3 — `test/test_log_utils.cpp` (complete file):**

```cpp
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
```

*(7 RUN_TEST lines / 7 test functions — count when copying; every function appears exactly once in main().)*

### Implementation Patterns & Key Details

```cpp
// The whole task is glue + one pure function; the SAFETY is in the invariants:
//   - logLine byte-bounded: worst case 128+1 bytes per line regardless of caller.
//   - logLine never checks Serial connection: with USB unplugged the CDC stack
//     drops the write; the loop cannot stall on logging (PRD §10/§2.2).
//   - watchdog fed FIRST each pass, before any later service (P1.M6.T2.S1 must
//     keep it first) — even a hung LED driver or ADC loop can't outlive 8 s.
//   - watchdog_enable LAST in setup(): the pre-loop work (pin modes, CDC start,
//     ADC resolution, banner) is done; countdown then governs loop() only.
//
// CALLER CHEAT SHEET (downstream subtasks):
//   logLine("BTN event=%d t=%lu", (int)ev, nowMs);            // P1.M2.T2.S1
//   logLine("ADC raw=%u avg=%u", raw, avg);                   // P1.M3.T1.S2
//   logLine("STATE %s->%s t=%lu", from, to, nowMs);           // P1.M6.T2.S1
//   logLine("%s", banner);  // the only current caller, in setup()
// Lines are '\n'-terminated, ≤128 bytes, documented at P2.M1.T1.S1.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes. UNGUARDED region grows by
    formatBootBanner BELOW the S1 time helpers (S1's block untouched). ARDUINO
    region gains the watchdog include + logLine at its top, banner/watchdog_enable
    at setup()'s tail, watchdog_update as loop()'s first statement.
BUILD:
  - No platformio.ini change: [env:native] auto-discovers test_log_utils.cpp as a
    third Unity project; [env:pico] compiles the enlarged sketch. No new libs —
    hardware/watchdog.h ships inside arduino-pico (IDE build stays green too).
CONFIG:
  - None. WATCHDOG_TIMEOUT_S and SERIAL_BAUD already exist and are consumed as-is.
DOCS:
  - None (item spec: "DOCS: none — no config surface change; logging formats
    documented at P2.M1.T1.S1"). README NOT updated here.
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - BOOTSEL polling / button events      -> P1.M2.T2.S1
  - ADC sampling / commissioning logs    -> P1.M3.T1.S2
  - state-machine wiring into loop()     -> P1.M6.T2.S1 (must keep watchdog_update first)
  - on-hardware watchdog/banner bench validation -> P1.M7.T2.S1
  - log-format audit / documentation     -> P2.M1.T1.S1
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Native build IS the syntax check — host g++ compiles the unguarded region +
# prototypes (and rejects any stray hardware symbol there):
pio test -e native
# Expected: THREE projects (test_seam, test_time_utils, test_log_utils); every test
# [PASSED]; exit 0. "undefined reference to `main'" => a test file lacks main().
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native -v
# Expected: 7 test_log_utils tests + 2 test_seam + 16 test_time_utils, all PASSED.
# Failure triage order: (1) transcription slip in main() vs function list,
# (2) banner label typo vs the normative strings, (3) missing unsigned cast —
# the EXPECTED VALUES are pre-verified; don't "fix" them, fix the code.
```

### Level 3: Integration Testing (System Validation)

```bash
pio run -e pico        # compile-only; NEVER upload in this subtask
# Expected: "[SUCCESS]" under .pio/build/pico/ — proves hardware/watchdog.h,
# watchdog_* calls, and Serial.write overloads compile for the board.

# Scope check — exactly two intended changes:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
# ?? test/test_log_utils.cpp
# and NOTHING else (config.h, platformio.ini, README, .gitignore untouched).

# Real hardware exercise (banner text, watchdog reset cause) is owned by the
# bench dry-run P1.M7.T2.S1 — NOT this subtask (item: cannot be native-tested).
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity: unguarded region free of hardware tokens (comments included):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" || echo "PURE OK"

# (2) Region markers still byte-stable (P1.M7.T1.S1 anchors):
grep -c "---- UNGUARDED REGION: pure logic (host-testable) ----\|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Valve LOW is still setup()'s first executable line (PRD §10):
awk '/^void setup\(\)/{f=1;next} f && /^[[:space:]]*[a-zA-Z]/{if (/digitalWrite\(PIN_VALVE, LOW\)/) {print "valve-first OK"; exit} else {print "VIOLATION line " NR; exit}}' \
  firmware/jug_filler/jug_filler.ino

# (4) watchdog_update is loop()'s first statement (PRD §10):
sed -n '/^void loop()/,/^}/p' firmware/jug_filler/jug_filler.ino | sed -n '2p' \
  | grep -q "watchdog_update" && echo "watchdog-first OK" || echo "VIOLATION"

# (5) No real while-loops in firmware (only the two NEVER-policy comments may match):
grep -rn "while" firmware/
# Expected: exactly 2 hits, both comment lines containing "NEVER" (Serial policy in
# setup + logLine comment). Any other hit = unplug-unsafe blocking — fix it.

# (6) No delay() in the ARDUINO region (PRD §10):
sed -n '/---- #ifdef ARDUINO/,$p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "[^a-zA-Z_]delay[[:space:]]*\(" || echo "NO-DELAY OK"

# (7) Watchdog ordering + single bootcause read:
test "$(grep -c "watchdog_caused_reboot" firmware/jug_filler/jug_filler.ino)" = "1" && echo "bootcause-once OK"
awk '/watchdog_caused_reboot/{c=NR} /watchdog_enable\(/{if (c && NR>c) {print "banner-before-enable OK"; exit}}' \
  firmware/jug_filler/jug_filler.ino

# (8) logLine is the only CDC writer (later subtasks rely on this):
sed -n '/---- #ifdef ARDUINO/,$p' firmware/jug_filler/jug_filler.ino \
  | grep -n "Serial\." | grep -v "Serial.begin\|Serial.write" || echo "logLine-only OK"
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio test -e native` exit 0 with THREE projects `[PASSED]`
- [ ] `pio run -e pico` → `[SUCCESS]` (compile-only; no upload attempted)
- [ ] `git status --short` shows exactly: `.ino` modified, `test/test_log_utils.cpp` new

### Feature Validation

- [ ] All "What" success-criteria boxes tickable from command output (7 tests green)
- [ ] Purity "PURE OK"; markers == 2; valve-first OK; watchdog-first OK; bootcause read exactly once, banner before enable
- [ ] while-scan shows only the 2 NEVER-comments; NO-DELAY OK; logLine-only OK
- [ ] No config/API surface change; README/config.h/platformio.ini untouched (item: "DOCS: none")

### Code Quality Validation

- [ ] Seam rules honored: pure builder unguarded (snprintf only), hardware glue inside `#ifdef ARDUINO`, explicit prototypes for both new functions
- [ ] S1's time-helper block, test files, and all skeleton lines outside the specified edits byte-stable
- [ ] No scope creep: exactly two functions, one constexpr, one include added — no LED/button/ADC logic
- [ ] Test file mirrors test_seam.cpp conventions (include path, setUp/tearDown, explicit main(), unsigned casts)

### Documentation & Deployment

- [ ] Function comments carry the caller cheat-sheet and the unplug-safety policy rationale
- [ ] Banner labels documented in-code as normative (formats formally documented at P2.M1.T1.S1)
- [ ] No new environment variables; Arduino IDE build stays viable (header ships with the core)

---

## Anti-Patterns to Avoid

- ❌ Don't gate logging on connection — no `while(!Serial)`, no `if (Serial)` guard in logLine; USB is unplugged in service (PRD §2.2)
- ❌ Don't use `Serial.printf`/raw `Serial.print` anywhere — `logLine()` is the single bounded channel; raw writes bypass the 128-byte guarantee
- ❌ Don't move `watchdog_enable()` earlier in setup() or call it before the banner — match the arch boot order (banner → enable last)
- ❌ Don't insert anything above `watchdog_update()` in loop() — first statement, forever (P1.M7.T1.S1 asserts it)
- ❌ Don't raise or "tune" `WATCHDOG_TIMEOUT_S` — 8000 ms is already near the 8388 ms RP2040-E1 hard ceiling
- ❌ Don't put `#include <hardware/watchdog.h>` at file top unguarded — native tests die at compile; it belongs inside the ARDUINO region
- ❌ Don't write the words millis/delay/Serial/pinMode/digitalWrite/analogRead in UNGUARDED-region comments — the purity grep matches comments too
- ❌ Don't add LOGLINE_MAX or banner sizes to config.h — they're bounds, not tunables (S1 constexpr precedent)
- ❌ Don't "fix" failing test expectations — banner strings and truncation math are hand-verified (research/notes.md §5–6)
- ❌ Don't omit the explicit `main()` in test_log_utils.cpp — PIO 6.2.0 link failure (verified live)
- ❌ Don't run `pio test -e pico` or upload anything — native is the only test transport; board env is compile-only; bench exercise belongs to P1.M7.T2.S1
- ❌ Don't touch S1's time helpers, test_time_utils.cpp, test_seam.cpp, setup()'s existing lines, config.h, platformio.ini, README, or .gitignore

---

**Confidence Score: 10/10** — every edit is a verbatim listing against the skeleton's exact current text; the watchdog API, 8388 ms ceiling, and unplug-safe CDC behavior are pre-verified platform facts (external_deps.md §2/§6, with sources); the test expectations are hand-derived; and all validation commands were proven working on this exact machine/toolchain by the S1/S2 research (`pio test -e native` green, PIO 6.2.0 + host g++ 16.2.1, three-project layout). The only external dependency is S1 landing per its PRP (stop-and-check precondition is Task 0).
