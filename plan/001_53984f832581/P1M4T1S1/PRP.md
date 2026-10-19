# PRP — P1.M4.T1.S1: backstopMinutes() — piecewise-linear interpolation with clamps (pure)

## Goal

**Feature Goal**: Land the timer model's pure lookup in the UNGUARDED region of the sketch: a FREE function `uint32_t backstopMinutes(uint32_t recoveryMin)` implementing PRD §6.2 exactly — piecewise-LINEAR interpolation between the five `BACKSTOP_TABLE_MIN` knots `{2,44},{10,41},{20,38},{35,34},{60,30}`, CLAMPED at both ends (recovery ≤ 2 min → 44; ≥ 60 min → 30), with the interpolated RESULT FLOORED to whole minutes (explicit contract rounding, documented in a comment). The concave shape is deliberate (tank refills fast then slows) — interpolate between the given knots only, NO smoothing, NO safety padding (PRD §6: the backstop "must hug the real fill time — never padded"). Zero hardware calls, zero new config constants (`BACKSTOP_TABLE_MIN` + `BACKSTOP_TABLE_LEN` + `BackstopTableEntry` already exist in config.h §3, built by P1.M1.T1.S1 explicitly for this task), zero ARDUINO-region changes, zero state (stateless pure function, not a class).

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: (a) ONE prototype line in the explicit-prototype block (free functions get prototypes — house seam rule #3; classes do not), placed directly under the `medianOf` prototype; (b) ONE bannered block inserted at the END of the UNGUARDED region — directly ABOVE `// ---- #ifdef ARDUINO REGION: hardware glue ----` (verified at line 554 of the current file; the preceding block is the false-trip-guards end-banner at line 552, landed by P1.M3.T3.S2). One blank line at each seam.
- `test/test_backstop/test_backstop.cpp` — NEW: 7 Unity native tests proving: exact values at ALL 5 knots; low clamp 0/1/2 → 44; high clamp 60/120/1440/4294967295 (UINT32_MAX) → 30; the two FLOOR-DISCRIMINATOR midpoints (recovery 6 → 42, exact 42.5; recovery 15 → 39, exact 39.5 — a truncating-division implementation returns 43/40 and fails exactly these); 11 hand-derived segment samples across all four segments; the item-contract PROPERTY (monotone non-increasing over recovery, bounded to [30,44], every r in 0..180); one composition test deriving recovery minutes via `elapsedMs`/60000 the way P1.M4.T2.S1 will. All expected values hand-derived AND machine-verified this session against the REAL current `.ino` (merged-copy method, `g++ -std=c++17 -Wall -Wextra -Werror`, **7 tests / 0 failures** — research/notes.md §3).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_backstop` project, plus regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline, test_trip, test_guards).
- `pio run -e pico` → compile-only `[SUCCESS]` — the block compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_backstop/` added by this task.
- The frozen signature `uint32_t backstopMinutes(uint32_t recoveryMin)` is ready for P1.M4.T2.S1 (RecoveryClock + FillDeadline) with zero rework.

## User Persona

**Target User**: The implementing agent of the downstream consumer — P1.M4.T2.S1 (RecoveryClock + FillDeadline: tracks last valve-close instant in RAM, computes recovery minutes at fill start, calls this function ONCE per fill to budget the deadline), and transitively P1.M6.T1.S2 (fill engine: enforces the deadline while FILLING) and P1.M6.T2.S1 (loop wiring).

**Use Case**: A user presses BOOTSEL to start a fill 15 minutes after the previous fill's valve closed. The recovery clock hands `backstopMinutes(15)` its recovery; the function returns 39 (floor of the exact 39.5 on the 10→20 segment) — the maximum valve-open minutes granted. Sensors normally end the fill far earlier; if they fail, the deadline closes the valve at 39 min, hugging the real fill time instead of padding it.

**User Journey**: fill starts → P1.M4.T2.S1 computes recovery minutes since last fill end (or uses FRESH_BOOT_BACKSTOP_MIN when no history) → `backstopMinutes(recovery)` returns the floored, clamped, interpolated budget → the deadline task converts it to a millis deadline → the fill engine closes the valve there if no sensor trip arrived.

**Pain Points Addressed**: a straight-line (chord) approximation would pad the mid-range (e.g. ~15 min recovery) in exactly the wrong direction — longer valve-open time on sensor failure; a ceil-rounded interpolation would pad EVERY fractional segment; the concave knots + floor rounding keep the backstop tight everywhere.

## Why

- PRD §6.2 (normative): "`BACKSTOP_TABLE_MIN` (piecewise-linear between points, clamped at both ends)" with the exact 5-row table; "Every fill's backstop is computed once, at fill start." This function IS that table's semantics.
- PRD §6 (normative framing): the backstop "must hug the real fill time — **never padded**" — this pins the floor rounding and forbids any added margin.
- PRD §8 (frozen contract): the table already lives in config.h §3 — consumed BY NAME (`BACKSTOP_TABLE_MIN`, `BACKSTOP_TABLE_LEN`), never re-hardcoded.
- Architecture (`firmware_architecture.md` line 21): this task's exact named slot — "backstopMinutes / RecoveryClock / FillDeadline (P1.M4)" in the UNGUARDED region; seam rules: pure logic only, no hardware calls.
- Item contract (task breakdown): input = `BACKSTOP_TABLE_MIN` from config.h; output = minutes value consumed by P1.M4.T2.S1; native tests as listed; docs = none (internal).
- TDD-by-SOW: architecture §5 mandates native Unity proofs for every rule this function underpins.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; P1.M4.T2.S1's PRP is written against it):

1. **`uint32_t backstopMinutes(uint32_t recoveryMin)`** — free function (NOT a class; no state). Returns the maximum valve-open minutes for a fill starting after `recoveryMin` minutes of recovery: `BACKSTOP_TABLE_MIN[0].backstopMin` (44) for `recoveryMin <= 2`; `BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN-1].backstopMin` (30) for `recoveryMin >= 60`; otherwise linear interpolation between the bracketing knots, RESULT FLOORED to whole minutes.
2. **Rounding (explicit, commented in-code)**: floor of the interpolated result, via the identity `floor(b0 - x) == b0 - ceil(x)` for integer `b0` on falling segments — the DROP uses ceiling division of strictly non-negative integers. Integer math only, no floats.
3. **Unsigned safety**: knot `uint16_t` fields widened to `uint32_t` locals once, up front — every comparison/subtraction is plain unsigned 32-bit; all segment quantities non-negative; max product ≈ 96 ≪ 2^32.
4. **Segment scan**: after both clamps `2 < recoveryMin < 60` holds, so `while (recoveryMin >= knot[i+1].recoveryMin) ++i;` stops at `i == BACKSTOP_TABLE_LEN-2` at the latest — no out-of-bounds access, no early-exit tricks.
5. **Prototype**: one line in the explicit-prototype block (house rule #3 — host g++ does not run the Arduino .ino prototype generator; every landed FREE function has one).

NOT in scope (owners): `FRESH_BOOT_BACKSTOP_MIN` no-history path and ms→min conversion (P1.M4.T2.S1 — the caller; this function never sees a fabricated recovery); deadline timestamps and `PLUS5_MIN` stacking (P1.M4.T2.S1); valve control (P1.M6); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `backstopMinutes` present exactly once in the UNGUARDED region (directly above the ARDUINO marker), plus exactly one prototype line under `medianOf`'s
- [ ] Knots exact: 2→44, 10→41, 20→38, 35→34, 60→30
- [ ] Low clamp: 0, 1, 2 → 44; high clamp: 60, 120, 1440, 4294967295 → 30
- [ ] Floor discriminators: 6 → 42 (not 43), 15 → 39 (not 40)
- [ ] Property: monotone non-increasing over recovery and bounded to [30,44] for every r in 0..180
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region
- [ ] Zero knot-value literals in the function body (table values only via `BACKSTOP_TABLE_MIN`/`BACKSTOP_TABLE_LEN` names)
- [ ] `test/test_backstop/test_backstop.cpp` exists with explicit `int main()` Unity runner; all 7 listed tests with the exact expected values
- [ ] `pio test -e native` exit 0 all projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` prototype + UNGUARDED block + new test dir only)
- [ ] config.h, platformio.ini, includes, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete block listing, the prototype line, and the complete 7-test file are provided verbatim below — all three were inserted into a copy of the REAL current `.ino` and compiled with `g++ -std=c++17 -Wall -Wextra -Werror` and executed green (**7 tests / 0 failures**) on this host this session (research/notes.md §3); the insertion anchor (`// ---- #ifdef ARDUINO REGION: hardware glue ----` at line 554, preceded by the false-trip-guards end-banner at line 552 — P1.M3.T3.S2 HAS LANDED) was verified in the current file; the consumed config surface (`BackstopTableEntry`, `BACKSTOP_TABLE_MIN`, `BACKSTOP_TABLE_LEN`) was read from the landed config.h; `pio test -e native` / `pio run -e pico` mirror commands proven working in every prior landed subtask's session (most recently P1.M3.T3.S1/S2 — all 8 current test projects green).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified-current landmarks:
        (1) PROTOTYPE ANCHOR — the line
        `uint16_t medianOf(uint16_t* values, size_t n);   // median of n counts, sorts in place (P1.M3.T1.S1)`
        Insert your prototype line DIRECTLY BELOW it (one line, own comment).
        (2) BLOCK ANCHOR — `// ---- #ifdef ARDUINO REGION: hardware glue ----`
        (line 554). Insert your block DIRECTLY ABOVE it, with exactly ONE blank
        line at each seam. The line above the seam blank is
        `// ========= end false-trip guards (consumed by P1.M6.T1.S2, P1.M6.T1.S3) =========`
        (line 552 — P1.M3.T3.S2 landed). DO NOT modify the guards block.
        (3) `elapsedMs(uint32_t now, uint32_t since)` (~line 56) — used only by
        the composition TEST, not by the function itself.
  gotcha: if the false-trip-guards banner is MISSING, P1.M3.T3.S2 has not landed —
        still insert directly ABOVE the ARDUINO marker (position identical), but
        report the state in the completion note. If the ARDUINO marker itself is
        missing, the seam moved — STOP and report; do not improvise.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of everything you consume BY NAME: `BackstopTableEntry`
        (struct, §3), `BACKSTOP_TABLE_MIN` (the 5 knots), `BACKSTOP_TABLE_LEN`
        (its own comment says "helper for P1.M4.T1.S1 iteration"). Do NOT add
        anything — §8 is frozen.
  gotcha: never hardcode 2/10/20/35/60/44/41/38/34/30 in the function — always
        the config names (§9 commissioning may retune the table).

- file: test/test_guards/test_guards.cpp
  why: THE most recent sibling test pattern (landed by P1.M3.T3.S2): relative
        `.ino` include, empty setUp/tearDown, hand-derived expected values with
        derivation comments, ITEM CASE tags on contract-pinned tests, explicit
        `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return UNITY_END(); }`
        (REQUIRED on PIO 6.2.0 — no auto-generated Unity runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity project,
        auto-discovered — no platformio.ini change needed.

- file: plan/001_53984f832581/P1M4T1S1/research/notes.md
  why: this task's research: §1 contract→codebase mapping, §2 THE rounding trap
        (floor-of-result vs floor-of-drop; the b0−ceil(x) identity), §3 machine-
        verification record + full hand-derived value table for all four
        segments, §4 downstream API freeze, §5 formatting pin.
  section: §2 (rounding) and §3 (verified values)

- file: plan/001_53984f832581/P1M3T3S2/PRP.md
  why: CONTRACT for the immediately-preceding parallel item. Its block (GuardVerdict
        + FalseTripGuards) sits directly above yours; treat as landed-and-frozen.
        Its tests (21) must stay green after your edit.
  gotcha: do NOT modify or reformat the guards block or its banner.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: line 21 ("backstopMinutes / RecoveryClock / FillDeadline (P1.M4)"),
        §1 seam rules, invariant #2 (backstop computed at fill start — the
        CALLER's job; this task is just the lookup)
  why: justifies UNGUARDED-region placement and the pure/no-hardware rule.

- url: https://docs.platformio.org/en/stable/plus/unit-testing.html
  why: PlatformIO native Unity conventions — test_dir auto-discovery, one project
        per test/ subdir, explicit runner requirement on PIO 6.2.0.
  critical: never `pio test -e pico` (no on-device transport) — native tests only.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # seam + time helpers + ButtonClassifier
│   │                         #   + ReadingAggregator/medianOf + BaselineCapture
│   │                         #   + TripDetector + FalseTripGuards (P1.M3.T3.S2 —
│   │                         #     LANDED; its end-banner at ~line 552 sits
│   │                         #     directly above YOUR insertion point)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service, ADC
│   │                         #     wiring, setup/loop
│   │                         # ← EDIT (prototype block +1 line; UNGUARDED +1 block)
│   └── config.h              # every §8 tunable — READ-ONLY (BackstopTableEntry /
│                             #   BACKSTOP_TABLE_MIN / BACKSTOP_TABLE_LEN by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/  test_baseline/  test_trip/                     # untouched
│   ├── test_guards/          # P1.M3.T3.S2's — untouched, must stay green (21)
│   └── test_backstop/        # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): +1 prototype line;
│                             #   +1 bannered block: backstopMinutes() — clamped
│                             #   piecewise-linear concave table lookup, floored
└── test/
    └── test_backstop/
        └── test_backstop.cpp # NEW: native Unity proof — knots, both clamps,
                              #   floor midpoints, segment samples, monotone +
                              #   bounded property, elapsedMs composition
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — THE rounding trap: contract says FLOOR the RESULT. A falling
//   segment's naive `drop = (b0-b1)*(r-r0)/(r1-r0)` with truncating '/' floors
//   the DROP, which CEILS the result: at r=6 (exact 42.5) it returns 43 — a
//   LONGER valve grant = forbidden padding. Use the identity
//   floor(b0 - x) == b0 - ceil(x) (integer b0, falling segment):
//     drop = ((b0 - b1) * (recoveryMin - r0) + (r1 - r0) - 1) / (r1 - r0);
//     return b0 - drop;
//   Tests 6→42 and 15→39 discriminate exactly this (naive form fails both).

// CRITICAL: uint16_t knot fields vs uint32_t parameter — widen the knot fields
//   to uint32_t locals BEFORE any comparison/subtraction, or -Wsign-compare
//   (enabled by -Wall/-Wextra, enforced -Werror in this repo's gates) fires on
//   `recoveryMin >= entry.recoveryMin` (uint32_t vs promoted int).

// CRITICAL: do the HIGH clamp (>= 60 → 30) and LOW clamp (<= 2 → 44) BEFORE the
//   segment scan. Only then does 2 < recoveryMin < 60 hold, which is what makes
//   the while-scan terminate at i == BACKSTOP_TABLE_LEN-2 — no bounds risk.

// CRITICAL: no floats, no rounding functions — integer math only (house style;
//   also keeps the function exact and deterministic on both toolchains).

// GOTCHA: this is a FREE function, not a class — it DOES need an entry in the
//   explicit-prototype block (seam rule #3; elapsedMs/formatBootBanner/medianOf
//   precedent). Classes got none; you are not adding a class.

// GOTCHA: config.h §3's BACKSTOP_TABLE_LEN already exists — do not recompute
//   sizeof/sizeof in the .ino; consume the helper BY NAME.

// GOTCHA: UNGUARDED-region purity gate greps hardware tokens on comment-stripped
//   CODE lines only — your block has none anyway; keep prose clean regardless.

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests only;
//   `pio run -e pico` is the compile gate. Never adjust a test's expected value
//   to make it pass — they are machine-verified; a failure means the
//   implementation deviates.
```

## Implementation Blueprint

### Data models and structure

No new types, no state. One free function consuming two existing config names:

```cpp
uint32_t backstopMinutes(uint32_t recoveryMin);   // minutes in → granted valve-open minutes out
// internals: 4 widened knot locals (uint32_t), 1 loop index (uint8_t),
// 4 segment locals (r0,b0,r1,b1 as uint32_t), 1 drop (uint32_t).
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "backstopMinutes" firmware/jug_filler/jug_filler.ino        # EXPECT: 0 (not yet landed)
  - RUN: grep -c "end false-trip guards (consumed by P1.M6.T1.S2" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (P1.M3.T3.S2 landed — preceding block)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "BACKSTOP_TABLE_MIN\|BACKSTOP_TABLE_LEN\|BackstopTableEntry" firmware/jug_filler/config.h  # EXPECT: all present; table exactly {2,44},{10,41},{20,38},{35,34},{60,30}
  - RUN: pio test -e native                                                  # EXPECT: exit 0, all projects [PASSED] (incl. test_guards, 21 tests)
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — two additive edits
  - EDIT A (prototype block): insert ONE line directly below
      `uint16_t medianOf(uint16_t* values, size_t n);   // median of n counts, sorts in place (P1.M3.T1.S1)`
    using the "Prototype line — insert verbatim" listing below.
  - EDIT B (UNGUARDED region): insert the "Task 1 listing" verbatim directly
    ABOVE `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with exactly ONE
    blank line above (after the false-trip-guards end-banner) and ONE below
    (before the ARDUINO marker). Do NOT double-blank the seam.
  - PRESERVE: everything else byte-for-byte — includes, all other prototypes,
    all other UNGUARDED sections (incl. FalseTripGuards), the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and its tests executed
    against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_backstop/test_backstop.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (7 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_backstop matches the
    test_reading/test_baseline/test_trip/test_guards convention. PIO
    auto-discovers the new project — no build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Prototype line — insert verbatim (Task 1, EDIT A):**

```cpp
uint32_t backstopMinutes(uint32_t recoveryMin);  // concave backstop table lookup, floored (P1.M4.T1.S1)
```

**Task 1 listing — insert verbatim (Task 1, EDIT B):**

```cpp
// ===================== Backstop table interpolation (P1.M4.T1.S1) =====================
// Timer model (PRD §6.2), pure: the CONCAVE backstop curve. Given minutes of
// recovery since the previous fill's valve closed (wall-clock recovery owned
// by P1.M4.T2.S1), return the maximum valve-open minutes granted to a fill
// started at that recovery — piecewise-LINEAR between the BACKSTOP_TABLE_MIN
// knots, CLAMPED at both ends:
//   recovery <= 2 min   -> 44   (steep: tank refills fast at low back-pressure)
//   recovery >= 60 min  -> 30   (flat: near-full tank, high pressure, slow fill)
// The concave shape is DELIBERATE (PRD §6.2): a straight line pads the
// mid-range in exactly the wrong direction. Interpolate between the given
// knots ONLY — no smoothing, no safety padding (§6 "never padded").
//
// ROUNDING (contract, explicit): the interpolated RESULT is FLOORED to whole
// minutes. Every segment falls (b1 < b0), and for integer b0:
//   floor(b0 - x) == b0 - ceil(x)
// so the DROP is computed with a CEILING division of strictly non-negative
// integers. A plain truncating '/' on the drop would round the result UP =
// a longer valve time = forbidden padding. Integer math only, no floats.
// Unsigned-safe by construction: (b0-b1), (recoveryMin-r0), (r1-r0) are all
// >= 0 inside a segment and the product stays <= 44*58 << 2^32.
//
// Caller contract (P1.M4.T2.S1 — computed ONCE at fill start, PRD §6.2):
//   budget = backstopMinutes(recoveryMinutesAtFillStart);
// Fresh boot / any reset has NO history: the caller uses FRESH_BOOT_BACKSTOP_MIN
// directly and never calls this with a fabricated recovery. A long-press +5
// (PLUS5_MIN) extends the ACTIVE fill's deadline only — never this table.
uint32_t backstopMinutes(uint32_t recoveryMin) {
    // Widen knot fields once (uint16_t -> uint32_t): every comparison and
    // subtraction below is plain unsigned 32-bit — no mixed-sign arithmetic.
    const uint32_t firstR = (uint32_t)BACKSTOP_TABLE_MIN[0].recoveryMin;
    const uint32_t firstB = (uint32_t)BACKSTOP_TABLE_MIN[0].backstopMin;
    const uint32_t lastR =
        (uint32_t)BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN - 1].recoveryMin;
    const uint32_t lastB =
        (uint32_t)BACKSTOP_TABLE_MIN[BACKSTOP_TABLE_LEN - 1].backstopMin;

    if (recoveryMin >= lastR) return lastB;    // clamp high:  >= 60 min -> 30
    if (recoveryMin <= firstR) return firstB;  // clamp low:   <= 2 min  -> 44

    // Here 2 < recoveryMin < 60: find the bracketing knot pair. The loop stops
    // at i == BACKSTOP_TABLE_LEN - 2 at the latest (recoveryMin < 60), so the
    // i + 1 access never runs out of bounds.
    uint8_t i = 0;
    while (recoveryMin >= (uint32_t)BACKSTOP_TABLE_MIN[i + 1].recoveryMin) ++i;
    const uint32_t r0 = BACKSTOP_TABLE_MIN[i].recoveryMin;      // left knot recovery
    const uint32_t b0 = BACKSTOP_TABLE_MIN[i].backstopMin;      // left knot backstop
    const uint32_t r1 = BACKSTOP_TABLE_MIN[i + 1].recoveryMin;  // right knot recovery
    const uint32_t b1 = BACKSTOP_TABLE_MIN[i + 1].backstopMin;  // right knot backstop

    // Ceiling division of the non-negative drop (see ROUNDING above):
    //   drop_exact = (b0 - b1) * (recoveryMin - r0) / (r1 - r0)
    const uint32_t drop =
        ((b0 - b1) * (recoveryMin - r0) + (r1 - r0) - 1) / (r1 - r0);
    return b0 - drop;  // == floor of the exact interpolated backstop, minutes
}
// ===== end backstop interpolation (consumed by P1.M4.T2.S1, P1.M6.T1.S2) =====
```

**Task 2 listing — `test/test_backstop/test_backstop.cpp` (verbatim):**

```cpp
// test/test_backstop/test_backstop.cpp — native contract proof for
// backstopMinutes() (P1.M4.T1.S1). Pins PRD §6.2: piecewise-LINEAR
// interpolation between the BACKSTOP_TABLE_MIN knots {2,44},{10,41},
// {20,38},{35,34},{60,30}, CLAMPED at both ends, interpolated result
// FLOORED to whole minutes. The two half-integer midpoints are the floor
// discriminators: recovery 6 -> exact 42.5 -> 42 (a truncating-drop
// implementation returns 43 = ceil = forbidden padding); recovery 15 ->
// exact 39.5 -> 39 (naive form returns 40).
// Segment exact values (all floored): (2,10]: 44 - 3(r-2)/8; (10,20]:
// 41 - 3(r-10)/10; (20,35]: 38 - 4(r-20)/15; (35,60): 34 - 4(r-35)/25.
// Expected values hand-derived AND machine-verified against the real .ino
// (plan/001_53984f832581/P1M4T1S1/research/notes.md §3).
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ---- exact values at all 5 knots (item contract) ----

// ITEM CASE: the table's own points return their own backstops, exactly.
void test_knots_exact() {
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(2));
    TEST_ASSERT_EQUAL_UINT32(41, backstopMinutes(10));
    TEST_ASSERT_EQUAL_UINT32(38, backstopMinutes(20));
    TEST_ASSERT_EQUAL_UINT32(34, backstopMinutes(35));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(60));
}

// ---- low clamp: recovery <= 2 min -> 44 (item contract) ----

// ITEM CASE: 0, 1 and 2 minutes of recovery all clamp to the first knot's
// backstop — a refill started immediately gets the longest grant.
void test_low_clamp_zero_one_two() {
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(0));
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(1));
    TEST_ASSERT_EQUAL_UINT32(44, backstopMinutes(2));
}

// ---- high clamp: recovery >= 60 min -> 30 (item contract) ----

// ITEM CASE: 60 (also a knot), 120, 1440 (a day) and the full uint32_t
// range all clamp to the last knot's backstop.
void test_high_clamp_sixty_and_beyond() {
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(60));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(120));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(1440));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(4294967295u));
}

// ---- floor rounding on the midpoints (item contract: "6 -> between 44 and
// 41; 15 -> between 41 and 38"; rounding = floor, pinned) ----

// ITEM CASE: recovery 6 is the (2,10) midpoint: exact 44 - 3*4/8 = 42.5 ->
// FLOOR 42. A truncating-division-on-the-drop implementation returns 43
// here (ceil of the result = padding, PRD §6 "never padded").
void test_midpoint_6_floors_to_42() {
    TEST_ASSERT_EQUAL_UINT32(42, backstopMinutes(6));
}

// ITEM CASE: recovery 15 is the (10,20) midpoint: exact 41 - 3*5/10 = 39.5
// -> FLOOR 39 (naive truncation returns 40).
void test_midpoint_15_floors_to_39() {
    TEST_ASSERT_EQUAL_UINT32(39, backstopMinutes(15));
}

// ---- segment samples across all four interior segments ----

// One sample per band of each segment, hand-derived (floor of exact):
// 3->43.625->43, 5->42.875->42, 9->41.375->41, 11->40.7->40, 19->38.3->38,
// 21->37.733->37, 27->36.133->36, 34->34.267->34, 36->33.84->33,
// 47->32.08->32, 59->30.16->30.
void test_segment_samples() {
    TEST_ASSERT_EQUAL_UINT32(43, backstopMinutes(3));
    TEST_ASSERT_EQUAL_UINT32(42, backstopMinutes(5));
    TEST_ASSERT_EQUAL_UINT32(41, backstopMinutes(9));
    TEST_ASSERT_EQUAL_UINT32(40, backstopMinutes(11));
    TEST_ASSERT_EQUAL_UINT32(38, backstopMinutes(19));
    TEST_ASSERT_EQUAL_UINT32(37, backstopMinutes(21));
    TEST_ASSERT_EQUAL_UINT32(36, backstopMinutes(27));
    TEST_ASSERT_EQUAL_UINT32(34, backstopMinutes(34));
    TEST_ASSERT_EQUAL_UINT32(33, backstopMinutes(36));
    TEST_ASSERT_EQUAL_UINT32(32, backstopMinutes(47));
    TEST_ASSERT_EQUAL_UINT32(30, backstopMinutes(59));
}

// ---- item-contract property: monotone non-increasing over recovery ----

// More recovery never grants a longer backstop, and the output is bounded
// by the clamp values [30,44] — checked at EVERY minute of 0..180 (covers
// all segments, both clamps, and all knot boundaries).
void test_monotone_non_increasing_and_bounded() {
    uint32_t prev = backstopMinutes(0);
    for (uint32_t r = 1; r <= 180; ++r) {
        const uint32_t v = backstopMinutes(r);
        TEST_ASSERT_TRUE(prev >= v);           // monotone non-increasing
        TEST_ASSERT_TRUE(v >= 30 && v <= 44);  // bounded by clamp values
        prev = v;
    }
}

// ---- composition: the caller-side minutes derivation (P1.M4.T2.S1) ----

// Recovery minutes arrive as elapsedMs(now, lastFillEndMs) / 60000 — the
// same wrap-safe helper every timed service uses. Exactly 15 min elapsed
// -> recovery 15 -> 39. Documents (does not implement) the caller's path.
void test_composition_recovery_via_elapsedMs() {
    const uint32_t lastEnd = 1000000u;
    const uint32_t now = lastEnd + 15u * 60u * 1000u;   // exactly 15 min later
    TEST_ASSERT_EQUAL_UINT32(39,
                             backstopMinutes(elapsedMs(now, lastEnd) / 60000u));
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_knots_exact);
    RUN_TEST(test_low_clamp_zero_one_two);
    RUN_TEST(test_high_clamp_sixty_and_beyond);
    RUN_TEST(test_midpoint_6_floors_to_42);
    RUN_TEST(test_midpoint_15_floors_to_39);
    RUN_TEST(test_segment_samples);
    RUN_TEST(test_monotone_non_increasing_and_bounded);
    RUN_TEST(test_composition_recovery_via_elapsedMs);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
// PATTERN: free pure function with an explicit prototype (elapsedMs/
#   formatBootBanner/medianOf precedent) — NOT a class: no state, no ctor,
#   no caller protocol object. One call, one answer.

// CRITICAL: ceiling division for the drop — ((a) + (b) - 1) / (b) for
#   non-negative a, b. This is what makes the RESULT floored. Write the
#   identity in the comment (floor(b0-x) == b0-ceil(x)); a reviewer must be
#   able to check it without external references.

// CRITICAL: clamps FIRST (high, then low), segment scan SECOND — the scan's
#   termination proof (stops at BACKSTOP_TABLE_LEN-2) depends on
#   2 < recoveryMin < 60 already holding.

// PATTERN: consume config BY NAME (BACKSTOP_TABLE_MIN[i].recoveryMin etc.);
#   BACKSTOP_TABLE_LEN exists precisely for this iteration (config.h comment).

// GOTCHA: the composition test uses elapsedMs — include the .ino as every
#   sibling test does; elapsedMs is already in scope from the merge.

// GOTCHA: the P1.M4.T2.S1 integration is DOCUMENTATION ONLY here — do not
#   add ARDUINO-region code or a wrapper; the recovery clock lands next task.
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 prototype line under
    medianOf's; +1 bannered block at the end of the UNGUARDED region (directly
    above the ARDUINO marker). Includes, other prototypes, ARDUINO region,
    setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes BackstopTableEntry / BACKSTOP_TABLE_MIN /
    BACKSTOP_TABLE_LEN from config.h BY NAME.
TEST:
  - New project dir test/test_backstop/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
DOWNSTREAM API (frozen by this task — P1.M4.T2.S1 builds against it):
  - uint32_t backstopMinutes(uint32_t recoveryMin) — whole minutes in, whole
    minutes out, floored; call ONCE at fill start with the true recovery.
  - Caller owns FRESH_BOOT_BACKSTOP_MIN (no history), ms→min conversion, the
    deadline timestamp, PLUS5_MIN stacking, abort re-anchoring (PRD §6.1).
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - recovery-clock state / last-fill-end anchor          -> P1.M4.T2.S1
  - deadline math, +5 stacking, abort semantics           -> P1.M4.T2.S1
  - valve control on deadline, fill-state machine         -> P1.M6.T1.S2
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
pio run -e pico
# Expected: "[SUCCESS]" — the block compiles under arm-gcc/arduino-pico inside
# the sketch build. Compile errors mean a transcription deviation from the
# listing — fix, don't redesign.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio test -e native
# Expected: exit 0; test_seam, test_time_utils, test_log_utils, test_button,
# test_reading, test_baseline, test_trip, test_guards AND the new test_backstop
# all [PASSED] (8/8 tests in test_backstop). ANY failure: read the mismatch —
# the expected values are machine-verified against the real file, so a failure
# means the implementation (not the test) deviates. Never "fix" a test's
# expected value to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_backstop
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_backstop/

# (2) Structural placement + counts:
grep -c "uint32_t backstopMinutes" firmware/jug_filler/jug_filler.ino   # 2 (prototype + definition)
grep -c "class.*backstop\|BackstopMinutes" firmware/jug_filler/jug_filler.ino  # 0 (free function, no class)
grep -n "end backstop interpolation (consumed by P1.M4.T2.S1" firmware/jug_filler/jug_filler.ino  # 1 hit
grep -n "end false-trip guards" firmware/jug_filler/jug_filler.ino      # 1 hit, ABOVE your block
grep -nE -- "---- UNGUARDED REGION|---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   your block must sit between the UNGUARDED marker's line no. and the ARDUINO
#   marker's line no. — directly above the ARDUINO marker.

# (3) Seam formatting: exactly ONE blank line between the false-trip-guards
#     end-banner and your block's first comment line, and ONE between your
#     end-banner and the ARDUINO marker (no doubled blanks):
sed -n '/end false-trip guards/,/#ifdef ARDUINO REGION/p' firmware/jug_filler/jug_filler.ino | grep -c "^$"
# Expected: 2

# (4) No churn where none belongs:
git diff --stat firmware/jug_filler/config.h platformio.ini   # empty
```

### Level 4: Creative & Domain-Specific Validation

```bash
# (1) Purity of the unguarded region — strip // comments first (comments
#     legitimately name hardware concepts; only CODE must be hardware-free):
sed -n '/---- UNGUARDED/,/---- #ifdef ARDUINO/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "millis|delay|Serial|pinMode|digitalWrite|analogRead" \
  || echo "PURE OK (comments stripped)"

# (2) Region markers intact (note: -E -- required; a bare pattern starting
#     with dashes is parsed as an option and the grep fails):
grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" \
  firmware/jug_filler/jug_filler.ino   # Expected: 2

# (3) Constants by name, no knot-value literals in the function body:
sed -n '/uint32_t backstopMinutes(uint32_t recoveryMin) {/,/^}/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "\b(44|41|38|34|30|60|35|20|10)\b" \
  || echo "NO KNOT LITERALS"
# Expected: NO KNOT LITERALS (the ceiling-division '- 1'/'+ 1' and array
#   indices 0 / i+1 / BACKSTOP_TABLE_LEN-1 are the only numerals allowed).

# (4) Safety-reasoning audit (mirrors research/notes.md §2/§3):
#     - Floor direction: recovery 6 -> 42 and 15 -> 39 (NOT 43/40) — rounding
#       the result DOWN is the never-padded direction (PRD §6).
#     - Clamp order: high clamp first, low clamp second, scan only inside
#       (2,60) — the bounds proof depends on it.
#     - Widths: knot fields widened to uint32_t before any arithmetic —
#       no mixed-sign comparisons anywhere in the body.
#     - Monotonicity + bounds property green over all of 0..180.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_backstop 8/8; test_guards and all siblings still green)
- [ ] "PURE OK (comments stripped)"; marker count 2; function + prototype exactly once each; seam blanks exactly 2; "NO KNOT LITERALS"
- [ ] `git status --short`: `.ino` + `test/test_backstop/` only; config.h/platformio.ini untouched; ARDUINO region untouched

### Feature Validation

- [ ] Knots exact: 2→44, 10→41, 20→38, 35→34, 60→30
- [ ] Clamps: 0/1/2→44; 60/120/1440/4294967295→30
- [ ] Floor discriminators: 6→42, 15→39 (the rounding contract, pinned)
- [ ] Segment samples: 3→43, 5→42, 9→41, 11→40, 19→38, 21→37, 27→36, 34→34, 36→33, 47→32, 59→30
- [ ] Property: monotone non-increasing and bounded [30,44] at every minute of 0..180
- [ ] Composition: elapsedMs-derived recovery 15 → 39

### Code Quality Validation

- [ ] Seam rules honored: zero hardware tokens in non-comment UNGUARDED lines; free function with explicit prototype under medianOf's; no state, no class
- [ ] Zero new config constants; `BACKSTOP_TABLE_MIN`/`BACKSTOP_TABLE_LEN` by name; no knot-value literals in the body
- [ ] Ceiling-division drop with the floor identity documented in-code; no floats
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M4.T2.S1, P1.M6.T1.S2); the caller contract (compute once at fill start; FRESH_BOOT path; +5 semantics) is quoted in the block comment
- [ ] Rounding, clamp order, and unsigned-safety rationale documented in-code (they are — keep them)
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't use a plain truncating `/` on the drop — that CEILS the result (padding). The drop needs the ceiling form `((a)+(b)-1)/(b)`; the floor midpoints 6→42 / 15→39 exist precisely to catch this
- ❌ Don't interpolate with floats, `round()`, `lround()`, or any rounding function — integer math only, and the contract rounding is FLOOR, not round-half-anywhere
- ❌ Don't add smoothing, splines, or "safety" margin to the curve — PRD §6: the backstop must hug the real fill time, never padded; the knots and only the knots
- ❌ Don't hardcode the table (2/44/…/30) or recompute the table length — `BACKSTOP_TABLE_MIN` / `BACKSTOP_TABLE_LEN` by name (§9 commissioning may retune)
- ❌ Don't add state, a class, a `begin()`, or a cached result — this is ONE stateless lookup called once per fill; the recovery clock, deadline math, and FRESH_BOOT path belong to P1.M4.T2.S1
- ❌ Don't compare/subtract the raw `uint16_t` knot fields against the `uint32_t` parameter — widen first, or -Wsign-compare fails the -Werror gate
- ❌ Don't touch the FalseTripGuards block, the ARDUINO region, config.h, or any sibling test — additive-only edit at the two named anchors
