# PRP — P1.M2.T2.S1: Poll BOOTSEL bool object into classifier with event logging

## Goal

**Feature Goal**: Wire the onboard BOOTSEL hardware to the (parallel-landed) pure `ButtonClassifier`: each `loop()` pass reads the button via arduino-pico's `__Bootsel` bool object — `bool pressed = BOOTSEL;` — feeds `classifier.update(nowMs, pressedRaw)`, and logs every emitted event with its hold duration through the blessed `logLine()` API (e.g. `BTN short 213ms` / `BTN long 1024ms`). Zero new pure logic; zero new constants; the change lives entirely in the `#ifdef ARDUINO` region, immediately after `watchdog_update()`.

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only, ARDUINO region: (a) a bannered static-state block (`s_button` classifier instance + three wiring-layer bookkeeping variables) inserted between `logLine()` and `setup()`; (b) a bannered ~12-line "BOOTSEL poll" block inserted into `loop()` as the FIRST content below `watchdog_update()`. NO unguarded-region edits, NO prototype-block edits, NO include/config changes.
- A bench-verified log stream (hardware-conditional): boot banner then one `BTN short/long <n>ms` line per accepted press-release.

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` (regression: this task touches nothing the native tests compile).
- `pio run -e pico` → compile-only `[SUCCESS]` — proves the `BOOTSEL` object read, the classifier instantiation, and the `logLine` call all compile under arm-gcc/arduino-pico.
- Purity grep over the UNGUARDED region still "PURE OK" (untouched); region markers still exactly 2.
- Bench (if a board is attached, USB power only, buck disconnected): quick press → one `BTN short <n>ms` (n < 1000); ~1.5 s hold → one `BTN long <n>ms` (n ≥ 1000); silence while held; sub-40 ms jabs → nothing.
- `git status --short`: only `.ino` modified by this task (plus whatever P1.M2.T1.S1 legitimately produced in parallel — attribute its files to it, don't touch them).

## User Persona

**Target User**: The implementing agent of P1.M6.T2.S1 (integration loop) — this block becomes the second stage of the final loop. Secondary: the human on the bench pressing BOOT during P1.M7.T2.S1 dry-runs.

**Use Case**: Every loop pass, the firmware samples the physical button and converts it into the hardware-sourced `ButtonEvent` stream; the fill engine (later) applies PRD §5.1's state table to it. Today, the stream is observable as USB CDC log lines.

**User Journey**: plug in (BOOTSEL released) → banner appears → tap BOOT → `BTN short 213ms` → later the fill engine starts a fill on that event; hold BOOT ~2 s → (silence while held) → release → `BTN long 1024ms` → later that extends the backstop +5 min.

**Pain Points Addressed**: the PRD's WRONG pseudo-pin pattern (`pinMode(BOOTSEL, INPUT_PULLUP)` / `digitalRead(BOOTSEL)` silently hits GPIO 0–1) — replaced by the verified bool-object read; events invisible during bring-up — solved by one logLine per event with hold duration.

## Why

- PRD §5.1/§5.2: the BOOT button is the device's ONLY control input; its event stream is the input vocabulary of the whole state machine. This subtask is where hardware becomes events.
- PRD §2.1 + external_deps.md §1 (BINDING PRD CORRECTION, verified from arduino-pico `cores/rp2040/Bootsel.cpp`): `BOOTSEL` is a global `__Bootsel` object whose `operator bool()` returns true when pressed; no `pinMode` needed; runtime reads can never enter the bootloader (entry requires BOOTSEL held during RESET) — matching PRD intent.
- Architecture (firmware_architecture.md): `loop(): watchdog_update → sample BOOTSEL → button events → …` — this task lands exactly that second/third stage.
- PRD §3: USB CDC logging is always on; `logLine` (P1.M1.T2.S2) is the only logging path; item spec: "DOCS: none — log lines only."

## What

Hardware glue only, in the ARDUINO region:

1. **Sample**: one `millis()` snapshot per pass (`nowMs`), one `BOOTSEL` bool read (`pressedRaw`, true == pressed). No `pinMode` for BOOTSEL — EVER.
2. **Raw-edge bookkeeping (log-only)**: track `false→true` and `true→false` raw transitions in `s_btnRiseMs` / `s_btnFallMs`. This duplicates NO classifier logic — the classifier remains the sole authority for debounce/classification; the stamps exist only to print a hold duration, because the classifier's contract exposes no hold accessor.
3. **Classify + log**: `ev = s_button.update(nowMs, pressedRaw)`; iff `ev != None`, one `logLine("BTN %s %lums", ev == LongPress ? "long" : "short", elapsedMs(s_btnFallMs, s_btnRiseMs))`.
4. **Placement**: the poll block is the first content in `loop()` below `watchdog_update()`, bannered so P1.M6.T2.S1 keeps it as the loop's second stage; nothing may ever be inserted above `watchdog_update()` (P1.M7.T1.S1 checks this structurally).

NOT in scope (owners): per-state interpretation of events (§5.1 table → P1.M6.T1.S1); ADC/LED/engine stages (P1.M3–P1.M6); README/docs (P2.M1.T3.S1); classifier changes (P1.M2.T1.S1 — its public surface is frozen).

### Success Criteria

- [ ] `.ino` ARDUINO region contains the state block (banner `BOOTSEL button service (P1.M2.T2.S1)`) with `static ButtonClassifier s_button;` + `s_btnPrevRaw`/`s_btnRiseMs`/`s_btnFallMs`
- [ ] `loop()` = `watchdog_update();` then the BOOTSEL poll block; no other loop() content added
- [ ] Exactly one `BOOTSEL` read per pass via bool conversion; zero `pinMode`/`digitalRead` tokens touching BOOTSEL anywhere in the file
- [ ] Every emitted event produces exactly one `BTN short %lums` / `BTN long %lums` line; no log while held
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; purity grep "PURE OK"; markers == 2
- [ ] UNGUARDED region, prototype block, config.h, platformio.ini, all test files byte-stable

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: complete verbatim listings for both edit regions, the exact current `loop()`/`logLine` context they slot into (read this session), the frozen `ButtonClassifier` API from the parallel PRP, the verified BOOTSEL read pattern with sources, and validation commands already proven on this machine by prior subtasks. Dependency risk (parallel P1.M2.T1.S1) is handled by a Task 0 precondition grep with a hard STOP.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Key landmarks (verified current): ARDUINO region starts
        with #include <hardware/watchdog.h>, then LOGLINE_MAX + logLine(), then
        setup(), then loop() whose body is exactly `watchdog_update();` plus a
        comment naming P1.M6.T2.S1. Your state block goes between logLine() and
        setup(); your poll block goes inside loop() below watchdog_update().
  gotcha: P1.M2.T1.S1 lands in parallel — a `class ButtonClassifier` section may
        or may not yet sit in the UNGUARDED region. Task 0 gates on it. Do NOT
        touch/reword anything outside your two insert points.

- file: plan/001_53984f832581/P1M2T1S1/PRP.md
  why: BINDING contract for the classifier you instantiate: public surface is
        EXACTLY `enum Event : uint8_t { None=0, ShortPress=1, LongPress=2 }` and
        `Event update(uint32_t nowMs, bool pressedRaw)` — one event per call,
        fired only on accepted RELEASE. The class lands in the UNGUARDED region
        above the ARDUINO marker, so instantiating it inside #ifdef ARDUINO works.
  gotcha: the class exposes NO hold-duration accessor by design — hence the
        wiring-layer raw-edge stamps (see notes.md §4). Do NOT extend the class.

- file: plan/001_53984f832581/P1M1T2S2/PRP.md
  why: contract for logLine() (landed, verified in file): void logLine(const char*
        fmt, ...) — 128-byte bound, newline-terminated, never gates on USB
        connection. %lu is the house format for uint32_t on arm-gcc.

- file: plan/001_53984f832581/architecture/external_deps.md
  section: "1. BOOTSEL button — VERIFIED with a BINDING PRD CORRECTION"
  why: `bool pressed = BOOTSEL;` — global __Bootsel object, operator bool() true
        == pressed, NO pinMode. PRD §2.1's pseudo-pin pattern DOES NOT EXIST in
        arduino-pico and would silently configure/read GPIO 0-1. Read parks IRQs
        and idles core1 briefly — designed for loop() use, safe at kHz rates.
        Runtime reads can NEVER enter the bootloader (needs BOOTSEL during RESET).
  critical: also the bench gotcha — powering up WITH BOOTSEL held enters the USB
        bootloader and the sketch never runs; release before plugging in.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: loop-order line (`watchdog_update → sample BOOTSEL → button events`) +
        "Rules of the seam"
  why: your poll block IS the loop's second stage; nothing ever above
        watchdog_update() (P1.M7.T1.S1 structurally asserts it).
  gotcha: the doc's diagram shows firmware/jug_filler.ino (pre-scaffold path);
        real path is firmware/jug_filler/jug_filler.ino.

- file: firmware/jug_filler/config.h
  why: READ-ONLY. BUTTON_DEBOUNCE_MS (40) / BUTTON_LONG_PRESS_MS (1000) exist;
        this task adds ZERO constants (thresholds are the classifier's business).

- url: https://arduino-pico.readthedocs.io/en/latest/bootsel.html
  why: official usage example — the canonical loop-context `if (BOOTSEL)` /
        `bool pressed = BOOTSEL;` pattern this task implements.

- url: https://github.com/earlephilhower/arduino-pico/blob/master/cores/rp2040/Bootsel.cpp
  why: the verified source: __Bootsel::operator bool() momentarily floats the
        QSPI flash-CS line with interrupts parked and the other core idled, then
        reads the level — why it's safe from the main loop but must not be called
        from ISRs/both cores concurrently (not our case: loop() only).
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] unity + src filter
├── firmware/
│   └── jug_filler/
│       ├── jug_filler.ino    # seam + time helpers + banner + logLine/watchdog
│       │                     #   (+ ButtonClassifier if T1.S1 has landed) ← EDIT (ARDUINO region only)
│       └── config.h          # all §8 tunables incl. BUTTON_* — READ-ONLY
├── test/
│   ├── test_seam.cpp         # do not touch
│   ├── test_time_utils.cpp   # do not touch
│   ├── test_log_utils.cpp    # do not touch
│   └── test_button.cpp       # (T1.S1's, if landed — do not touch)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain: pio 6.2.0 (~/.local/bin/pio), host g++ 16.2.1, Unity 2.6.1.
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/
│   └── jug_filler/
│       └── jug_filler.ino    # MODIFIED (additive): BOOTSEL state block + loop() poll block
# (no new files — hardware glue inside the existing sketch)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL: BOOTSEL is NOT a pin. `pinMode(BOOTSEL, …)` / `digitalRead(BOOTSEL)`
//   compile (object → int 0/1) but silently configure/read GPIO 0/1. The ONLY
//   correct read is the bool conversion: `const bool pressedRaw = BOOTSEL;`

// CRITICAL: `millis()` is allowed ONLY in the ARDUINO region — this task never
//   edits the UNGUARDED region, so the purity grep (which matches comments too)
//   stays clean automatically. Do not "improve" unguarded comments.

// CRITICAL: the poll block must remain BELOW watchdog_update() — and nothing
//   may ever be inserted ABOVE it (P1.M7.T1.S1 scans for this).

// CRITICAL: BOOTSEL reads park interrupts + idle core1 (~µs each). loop() polls
//   at kHz rates today — fine by design (official docs use it in loop). Do NOT
//   add rate-limiting/throttling logic here; P1.M6.T2.S1 owns loop pacing.

// CRITICAL: uint32_t on arm-none-eabi prints with %lu (house precedent:
//   "…t=%lu" in P1.M1.T2.S2's cheat sheet). Keep elapsedMs() for the hold
//   subtraction — raw timestamp subtraction of stamps is wrap-UNSAFE by style
//   rule even though mod-2^32 subtraction would work; compose the helper.

// GOTCHA (bench): plugging in with BOOTSEL held boots to the USB bootloader —
//   the sketch never runs. Release the button before/during power-up.

// GOTCHA (parallel T1.S1): if `pio run -e pico` fails with ButtonClassifier
//   undefined, the parallel task hasn't landed — STOP (Task 0 gate), do NOT
//   recreate the class yourself.
```

## Implementation Blueprint

### Data models and structure

No new types. One file-scope static instance of the (existing) pure class + three bookkeeping variables, all inside `#ifdef ARDUINO`:

```cpp
static ButtonClassifier s_button;   // the classifier instance (RAM state, no init needed)
static bool     s_btnPrevRaw = false; // previous raw BOOTSEL level (edge detect, log only)
static uint32_t s_btnRiseMs  = 0;     // stamp of raw press edge (false→true)
static uint32_t s_btnFallMs  = 0;     // stamp of raw release edge (true→false)
```

`static` keeps internal linkage; no prototypes needed for variables (seam rule 3 covers functions only).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first)
  - RUN: grep -c "class ButtonClassifier" firmware/jug_filler/jug_filler.ino  # EXPECT: 1
  - RUN: grep -c "---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 1
  - RUN: grep -c "void logLine(const char\* fmt, \.\.\.)" firmware/jug_filler/jug_filler.ino  # EXPECT: >= 1
  - RUN: grep -c "watchdog_update();" firmware/jug_filler/jug_filler.ino      # EXPECT: >= 1
  - ButtonClassifier absent → P1.M2.T1.S1 hasn't landed: STOP and report; do NOT create it.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — state block
  - EDIT: insert the "Task 1 listing" verbatim into the ARDUINO region between the
    end of logLine()'s definition and `void setup() {` (one blank line each side).
  - PRESERVE: everything else byte-for-byte (header, prototypes, unguarded region,
    setup(), loop() until Task 2, #endif).

Task 2: MODIFY firmware/jug_filler/jug_filler.ino — loop() poll block
  - EDIT: replace loop()'s trailing comment block (the lines after
    `watchdog_update();`) with the "Task 2 listing" verbatim, keeping
    watchdog_update() as the untouched first statement.
  - PRESERVE: setup(), #endif, everything outside loop().

Task 3: RUN gates
  - RUN: pio test -e native        # regression: every project [PASSED], exit 0
  - RUN: pio run -e pico           # compile-only [SUCCESS] — NEVER 'pio test -e pico'
  - RUN: Level 4 structural greps below.

Task 4 (CONDITIONAL — only if an RP2040-Zero is attached): bench check
  - RUN: pio run -e pico -t upload
  - RUN: pio device monitor -b 115200
  - DO: quick press/release → expect one `BTN short <n>ms` (n < 1000);
        ~1.5s hold → one `BTN long <n>ms` (n >= 1000); silence while held;
        sub-40ms jabs → no line; several presses → one line each.
  - POWER RULE (PRD §2.2/§3): USB power only, buck/12V disconnected.
  - No board attached → record "bench deferred" in the completion report; Tasks 1–3
    remain the acceptance gates (upload is permitted for THIS subtask because the
    item's own acceptance names a bench check — unlike T1.S1 which forbids upload).
```

**Task 1 listing — state block (verbatim):**

```cpp
// ---- BOOTSEL button service (P1.M2.T2.S1) ----
// Hardware side of the BOOT button: sample the __Bootsel bool object each loop
// pass, feed the pure classifier (P1.M2.T1.S1), log each event with its hold
// duration. NO pinMode for BOOTSEL — it is a global object, not a pin; the bool
// conversion (true == pressed) momentarily floats the QSPI flash-CS line with
// IRQs parked and the other core idled — designed for loop() use (external_deps
// §1). Runtime reads can never enter the bootloader (entry needs BOOTSEL held
// during RESET), matching PRD §2.1's intent.
static ButtonClassifier s_button;      // classifier instance; RAM state only (PRD §3)
static bool     s_btnPrevRaw = false;  // previous raw level (edge stamps below)
static uint32_t s_btnRiseMs  = 0;      // stamp of raw press edge    (false -> true)
static uint32_t s_btnFallMs  = 0;      // stamp of raw release edge  (true  -> false)
// Hold-duration policy: the classifier's contract exposes events only (no hold
// accessor, by design), so the wiring layer stamps the RAW edges itself. The
// printed duration is raw first-edge to last-edge — within contact-bounce
// (~tens of ms) of the classifier's theoretical hold. Diagnostic only: the
// classifier stays the sole authority for debounce and short/long classification.
// ---- end BOOTSEL button service state (used by loop, kept by P1.M6.T2.S1) ----
```

**Task 2 listing — the new loop() (complete function, verbatim):**

```cpp
void loop() {
  watchdog_update();   // FIRST STATEMENT — PRD §10: hang -> reset -> valve LOW -> IDLE
  // ---- BOOTSEL poll (P1.M2.T2.S1) — the loop's second stage (arch loop order) ----
  const uint32_t nowMs = millis();     // one clock snapshot per pass, shared downstream
  const bool pressedRaw = BOOTSEL;     // __Bootsel operator bool(): true == pressed
  if (pressedRaw != s_btnPrevRaw) {    // raw edge: stamp it (log-only bookkeeping)
    if (pressedRaw) s_btnRiseMs = nowMs;
    else            s_btnFallMs = nowMs;
    s_btnPrevRaw = pressedRaw;
  }
  const ButtonClassifier::Event ev = s_button.update(nowMs, pressedRaw);
  if (ev != ButtonClassifier::None) {  // fired exactly once, on accepted RELEASE
    const uint32_t holdMs = elapsedMs(s_btnFallMs, s_btnRiseMs);  // wrap-safe
    logLine("BTN %s %lums",
            ev == ButtonClassifier::LongPress ? "long" : "short", holdMs);
  }
  // ---- end BOOTSEL poll ----
  // P1.M6.T2.S1 fills the loop below this line (ADC, fill engine, LED, event
  // logs via logLine). Nothing may ever be inserted above watchdog_update().
}
```

### Implementation Patterns & Key Details

```cpp
// Event consumption is LEVEL-BLIND: this task logs whatever the classifier emits
// and does NOT interpret it — PRD §5.1's per-state actions/ignores are the fill
// engine's table (P1.M6.T1.S1). The event stream shape handed downstream:
//   P1.M6.T2.S1 reads ev right here (or keeps this block intact and adds stages).

// One event -> one log line, on release only. While held: update() returns None
// every pass, so nothing prints — verified on the bench by the "silence while
// held" check.

// elapsedMs() (P1.M1.T2.S1) composes the hold so the ~49.7-day wrap cannot
// corrupt the log — same rule as every other timed quantity in this codebase.

// If a press spans the wrap: raw-edge stamps wrap too; elapsedMs yields the true
// span for any span < 2^32 ms. No special casing.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes; ARDUINO region grows the
    state block + loop poll block. UNGUARDED region and prototype block untouched.
BUILD:
  - No platformio.ini change ([env:pico] already compiles the sketch).
CONFIG:
  - None. Zero new constants; BUTTON_* belong to the classifier, already in config.h.
DOCS:
  - None (item spec: log lines only; README operation section is P2.M1.T3.S1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - per-state interpretation of events (§5.1 table)  -> P1.M6.T1.S1
  - ADC/LED/fill-engine loop stages                  -> P1.M3–P1.M6
  - loop() restructure/pacing                        -> P1.M6.T2.S1 (keeps this block)
  - full bench dry-run of flows 1/2                  -> P1.M7.T2.S1
  - classifier semantics/tests                       -> P1.M2.T1.S1 (frozen)
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — BOOTSEL object read, classifier instantiation, logLine
# call all compile under arm-gcc + arduino-pico framework.
# "ButtonClassifier undeclared" => Task 0 gate was skipped (T1.S1 not landed).
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0, every project [PASSED] (test_seam, test_time_utils,
# test_log_utils, test_button if landed). This task adds no native tests BY
# DESIGN: the added code is #ifdef ARDUINO hardware glue — invisible to host
# builds; its logic (classification) is already native-proven by T1.S1's tests.
# ANY failure here means you touched something outside the ARDUINO region.
```

### Level 3: Integration Testing (System Validation)

```bash
# Scope check — only the .ino, plus T1.S1's legitimate parallel output:
git status --short
# Expected (attributed to THIS task):
#  M firmware/jug_filler/jug_filler.ino
# (?? test/test_button.cpp belongs to T1.S1 if it appears — leave it alone.)
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region (untouched => still clean; comments count):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" || echo "PURE OK"

# (2) Region markers byte-stable:
grep -c "---- UNGUARDED REGION: pure logic (host-testable) ----\|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) The PRD-correct BOOTSEL pattern landed, and the WRONG one is absent:
grep -n "bool pressedRaw = BOOTSEL" firmware/jug_filler/jug_filler.ino  # 1 hit, in loop()
grep -n "pinMode(BOOTSEL\|digitalRead(BOOTSEL" firmware/jug_filler/jug_filler.ino \
  || echo "NO-PSEUDOPIN OK"                                            # must print OK

# (4) Loop structure: watchdog first, poll second, exactly one BOOTSEL read:
grep -A2 "watchdog_update();" firmware/jug_filler/jug_filler.ino | head -3
grep -c "BOOTSEL;" firmware/jug_filler/jug_filler.ino   # Expected: 1

# (5) Log vocabulary: exactly one BTN logLine, matching the item's format:
grep -c 'logLine("BTN %s %lums"' firmware/jug_filler/jug_filler.ino  # Expected: 1

# (6) BENCH (hardware-conditional; USB power only, buck disconnected — PRD §2.2):
pio run -e pico -t upload && pio device monitor -b 115200
#   quick tap        -> one line  "BTN short <n>ms", n < 1000
#   ~1.5s hold       -> nothing while held, then "BTN long <n>ms", n >= 1000
#   sub-40ms jabs    -> no line
#   several presses  -> one line each, in order
#   board replug with BOOTSEL held = USB bootloader (sketch absent) — release
#   the button before powering; that is expected hardware behavior, not a bug.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0 all projects `[PASSED]`
- [ ] PURE OK; markers == 2; NO-PSEUDOPIN OK; exactly 1 `BOOTSEL;` read; exactly 1 `logLine("BTN %s %lums"`
- [ ] `git status --short`: only the `.ino` from this task (T1.S1's files untouched/attributed)

### Feature Validation

- [ ] State block + poll block landed verbatim; loop() order: watchdog_update → nowMs snapshot → BOOTSEL read → classify → log
- [ ] Bench (or recorded deferral): short/long lines with hold durations; silence while held; bounce jabs silent
- [ ] No event interpretation added (state-agnostic stream for P1.M6.T2.S1)

### Code Quality Validation

- [ ] Seam rules honored: hardware glue confined to `#ifdef ARDUINO`; classifier untouched; no new constants; elapsedMs composed for the hold
- [ ] No blocking calls, no `while`, no connection-gated logging; logLine is the only logging path
- [ ] UNGUARDED region, prototype block, config.h, platformio.ini, all tests byte-stable

### Documentation & Deployment

- [ ] Comments document the __Bootsel correction, the hold-duration approximation policy, and the P1.M6.T2.S1 keep-contract
- [ ] No README/config/env changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't use `pinMode(BOOTSEL, …)` or `digitalRead(BOOTSEL)` — the object converts to int 0/1 and you'd silently touch GPIO 0/1 (reserved for UART0 debug)
- ❌ Don't insert anything above `watchdog_update()` — or between it and the poll block
- ❌ Don't extend ButtonClassifier (add a hold getter, isPressed, etc.) — its surface is frozen by the parallel PRP; hold duration comes from the wiring layer's raw-edge stamps
- ❌ Don't interpret events here (start fill / abort / +5min) — that's the fill engine's §5.1 table (P1.M6.T1.S1)
- ❌ Don't add native tests for this glue — it's `#ifdef ARDUINO`-only; host builds can't see it (T1.S1's tests already prove the logic)
- ❌ Don't rate-limit or throttle BOOTSEL reads, and don't cache across passes — one read per pass is the design; loop pacing belongs to P1.M6.T2.S1
- ❌ Don't log while held or log the raw level — only released events produce lines
- ❌ Don't touch the UNGUARDED region (even comments — the purity grep matches comments) or T1.S1's class/test files
- ❌ Don't run `pio test -e pico` (no transport there) or upload without the buck disconnected / USB-only power rule
- ❌ Don't "fix" a missing ButtonClassifier by recreating it — Task 0 hard-stops instead

---

**Confidence Score: 9/10** — complete verbatim listings for both insert points against the verified current file contents; frozen upstream contracts (classifier API, logLine, elapsedMs) all read directly; the only residual risk is the parallel T1.S1 landing (mitigated by the Task 0 gate + hard-stop) and hardware availability for the bench check (explicitly conditional, with deferral protocol). Toolchain and commands are proven on this machine by prior subtasks.
