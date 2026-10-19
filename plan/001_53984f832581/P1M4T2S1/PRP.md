# PRP — P1.M4.T2.S1: RecoveryClock + FillDeadline — fresh-boot default, valve-open anchor, stacking +5 (pure)

## Goal

**Feature Goal**: Land the timer model's STATE in the UNGUARDED region of the sketch: two pure classes `RecoveryClock` (PRD §6.1 — RAM-only wall-clock recovery since the previous fill's VALVE CLOSE, whatever ended it: done, abort or fault; fresh boot / any reset → no history) and `FillDeadline` (PRD §6 preamble + §6.2 — per-fill valve-open budget computed ONCE at fill start via `hasHistory() ? backstopMinutes(recoveryMin) : FRESH_BOOT_BACKSTOP_MIN`, deadline ANCHORED at the valve-open instant — NOT the button press, because the 2 s baseline settle precedes valve open — with a stacking, repeatable `extend5()` = PLUS5_MIN on the ACTIVE fill only). Wrap-safe via the existing `elapsedMs`/`deadlinePassed` helpers (P1.M1.T2.S1); the backstop lookup via the landed `backstopMinutes` (P1.M4.T1.S1); config BY NAME (`FRESH_BOOT_BACKSTOP_MIN`, `PLUS5_MIN`) with ZERO new constants. Zero hardware calls, zero ARDUINO-region changes, zero prototypes (classes get none — house seam rule #3 is for free functions only). Exposes the §3 log inputs (`freshBoot`, `recoveryMsAtStart`, `tableMinutes`, `deadlineMs`).

**Deliverable**:
- `firmware/jug_filler/jug_filler.ino` — MODIFIED, additive only: ONE bannered block inserted at the END of the UNGUARDED region — directly ABOVE `// ---- #ifdef ARDUINO REGION: hardware glue ----` (verified at line 612 of the current file; the line above the seam blank is T1S1's landed end-banner `// ===== end backstop interpolation (consumed by P1.M4.T2.S1, P1.M6.T1.S2) =====` at line 610). Exactly ONE blank line at each seam. No prototype-block edit (classes don't get prototypes).
- `test/test_deadline/test_deadline.cpp` — NEW: 9 Unity native tests proving: fresh boot → 30; history at 15 min recovery → interpolated 39 (asserted pinned AND computed — see discrepancy note); abort re-anchors the reference at the abort instant; zero recovery → 44; valve-open anchoring + expiry boundary (deadline−1 open / deadline expired); +5 twice moves the deadline EXACTLY +10 min and un-expires the old deadline; +5 before anchor bakes in identically; wrap-safe arithmetic for both clock and deadline across the uint32 rollover; history persists across fills until reset. All expected values machine-verified this session against the REAL current `.ino` (merged-copy method, `g++ -std=c++17 -Wall -Wextra -Werror` + Unity 2.6.1, **9 tests / 0 failures** — research/notes.md §4).

**Success Definition**:
- `pio test -e native` → exit 0, every project `[PASSED]` — including the NEW `test_deadline` (10th project), plus regressions (test_seam, test_time_utils, test_log_utils, test_button, test_reading, test_baseline, test_trip, test_guards, test_backstop).
- `pio run -e pico` → compile-only `[SUCCESS]` — the block compiles under arm-gcc/arduino-pico inside the sketch build.
- Comment-stripped purity grep over the UNGUARDED region prints PURE OK; region-marker count still exactly 2.
- `git status --short`: only `firmware/jug_filler/jug_filler.ino` modified and `test/test_deadline/` added by this task.
- The frozen public surface (below) is ready for P1.M6.T1.S2 (fill engine: `beginFill` at press, `recordFillEnd` at every valve close, `extend5` on +5 long-press, `expired` each loop) and P1.M6.T2.S1 (loop wiring + §3 log line) with zero rework.

## User Persona

**Target User**: The implementing agent of the downstream consumer — P1.M6.T1.S2 (fill lifecycle orchestration: baseline → valve → trip → CONFIRM → DONE) and P1.M6.T2.S1 (setup()/loop() wiring + logging).

**Use Case**: A user presses BOOTSEL 15 minutes after the previous fill's valve closed. The fill engine calls `deadline.beginFill(clock, pressMs)` → recovery 15 min → `backstopMinutes(15)` = 39 (exact 39.5 floored) is snapshotted. After the 2 s baseline settle the valve opens and the engine calls `deadline.anchor(valveOpenedAtMs)`. If the user long-presses twice, `extend5()` runs twice → the valve may stay open 49 min total. If sensors never trip, `expired()` closes the valve at the anchored deadline (backstop = sensor-failure bound, hugging the real fill time — never padded).

**User Journey**: press → `beginFill` (minutes computed once; log inputs snapshotted) → settle 2 s → valve opens → `anchor` → each loop `expired(now)`? → no: keep filling / yes: end fill (P1.M6) → at EVERY fill end (done, abort, fault): `clock.recordFillEnd(valveClosedMs)` → next fill's recovery measured from there. Any reset: objects re-constructed → no history → next fill uses FRESH_BOOT_BACKSTOP_MIN = 30 (PRD §5.3).

**Pain Points Addressed**: anchoring at the button press would silently grant 2 s of valve-closed time as valve-open budget; a non-stacking +5 would break PRD §5.1; measuring recovery from fill START instead of valve CLOSE would over/under-count; non-wrap-safe millis math would fail after ~49.7 days of uptime.

## Why

- PRD §6.1 (normative): recovery reference = previous fill's valve close (whatever ended it); fresh boot/reset → `FRESH_BOOT_BACKSTOP_MIN = 30`; abort restarts the clock at the abort instant "exactly like a normal fill end".
- PRD §6 preamble (normative): "the backstop bounds how long the valve may stay open" → anchor at valve-open, not press (the 2 s settle precedes valve open).
- PRD §6.2 (normative): "Every fill's backstop is computed once, at fill start. A long-press +5 modifies the active fill's deadline only" (stacks, repeatable — §5.1).
- PRD §5.3 (normative): reset clears tank history → next fill uses the 30-min fresh-tank backstop. RAM-only state (§3: no persistence) makes this trivially true — documented in the class banner.
- PRD §3: the backstop computation LOG LINE consumes this task's exposed inputs (recovery, table result, deadline).
- Item contract: input = `backstopMinutes()` (P1.M4.T1.S1, LANDED) + time utilities (P1.M1.T2.S1) + config constants; output = backstop/deadline objects consumed by the fill engine (P1.M6.T1.S2) and its +5 long-press handling; docs = none (internal).
- Architecture (`firmware_architecture.md` line 21): "backstopMinutes / RecoveryClock / FillDeadline (P1.M4)" in the UNGUARDED region — pure logic, host-testable.

## What

Pure logic only, in the UNGUARDED region. Public surface EXACTLY (frozen; P1.M6.T1.S2's PRP will be written against it — item contract shapes, plus house const-correctness):

1. **`class RecoveryClock`** — `void recordFillEnd(uint32_t nowMs)` (call at EVERY fill end — done, abort, fault: whatever closed the valve); `bool hasHistory() const` (false until the first record, i.e. on every fresh boot); `uint32_t recoveryMs(uint32_t nowMs) const` (wall-clock ms since the last valve close, wrap-safe via `elapsedMs`; caller MUST check `hasHistory()` first — with no history the return is meaningless).
2. **`class FillDeadline`** — `void beginFill(const RecoveryClock& clock, uint32_t nowMs)` (compute the budget ONCE at fill start / button press: no history → `FRESH_BOOT_BACKSTOP_MIN`; history → `backstopMinutes(recoveryMs / 60000)`; snapshot the §3 log inputs; resets `anchored_ = false`); `void anchor(uint32_t valveOpenedAtMs)` (fix the deadline origin at the VALVE-OPEN instant); `bool expired(uint32_t nowMs) const` (`anchored_ && deadlinePassed(nowMs, anchorMs_ + budgetMs_)` — false until anchored: the settle window runs no deadline); `void extend5()` (`budgetMs_ += PLUS5_MIN * 60000` — stacks, repeatable, valid before or after anchor); log accessors `freshBoot()`, `recoveryMsAtStart()`, `tableMinutes()` (grant at fill start), `budgetMinutes()` (incl. +5 stacks), `deadlineMs()` (= `anchorMs_ + budgetMs_`, wraps naturally), `anchored()`.
3. **Storage model**: `anchorMs_ + budgetMs_`, NOT a baked deadline — `extend5()` just grows the budget; expiry recomputes the wrapped deadline each call through the existing rollover-safe `deadlinePassed` (P1.M1.T2.S1). No new time math.
4. **Sub-minute recovery truncates** to whole minutes (`/ 60000`) — house integer style; the table is minute-granular; matches T1S1's frozen composition test.
5. **Reset semantics = RAM object lifetime** (PRD §5.3): a reset re-constructs the objects → `hasHistory_ = false` → fresh-boot path. Documented, not implemented.

**Item-description discrepancy — RESOLVED (read before writing tests)**: the item gloss says "history at 15 min recovery → (~36-37, assert computed)". The normative table {2,44},{10,41},{20,38},{35,34},{60,30} gives 15 min on the 10→20 segment: 41 − 3·5/10 = 39.5 → **floor 39** (machine-verified and pinned by T1S1's frozen test `test_midpoint_15_floors_to_39`; "~36-37" would correspond to recovery ≈ 25–27 min). "Assert computed" governs and the PRD table wins over item prose: the test asserts BOTH the pinned 39 and the computed `backstopMinutes(15)`.

NOT in scope (owners): valve control on expiry and the state machine (P1.M6.T1.S1/S2); calling `recordFillEnd`/`beginFill`/`anchor`/`extend5` from hardware events (P1.M6.T2.S1); the +5 LED ack (P1.M5/P1.M6); any config.h addition; any ARDUINO-region edit; docs (item spec: none — internal).

### Success Criteria

- [ ] `RecoveryClock` + `FillDeadline` present exactly once each in the UNGUARDED region (single bannered block directly above the ARDUINO marker, directly below T1S1's end-banner); NO prototype-block change
- [ ] Fresh boot → `tableMinutes() == 30` (`FRESH_BOOT_BACKSTOP_MIN`), `freshBoot() == true`, `recoveryMsAtStart() == 0`
- [ ] 15-min recovery → `tableMinutes() == 39` (pinned AND computed via `backstopMinutes(15)`)
- [ ] Abort re-anchors: recovery measured from the ABORT instant, not the earlier fill end (15 min after abort → 900000 ms recovery, NOT 20 min)
- [ ] Zero recovery → 44 (table low clamp through the composition)
- [ ] Anchor at valve-open: `deadlineMs() == press + BASELINE_SETTLE_MS + budget`; `expired()` false until anchored; `expired(deadline − 1) == false`; `expired(deadline) == true`
- [ ] `extend5()` twice → budget 30→35→40, `deadlineMs()` moves EXACTLY `2 * PLUS5_MIN * 60000` ms; a just-expired old deadline is no longer expired; +5 before `anchor()` bakes in identically
- [ ] Wrap-safe: recovery read across the uint32 rollover (fill end 0xFFFFFF00, now wrapped to 0x100 → 0x200 ms) and deadline anchored at `0xFFFFFFFF − 1000` both behave
- [ ] Zero occurrences of `millis`, `delay`, `Serial`, `pinMode`, `digitalWrite`, `analogRead` in non-comment lines of the UNGUARDED region
- [ ] `test/test_deadline/test_deadline.cpp` exists with explicit `int main()` Unity runner; all 9 listed tests pass
- [ ] `pio test -e native` exit 0 all 10 projects; `pio run -e pico` `[SUCCESS]`; marker count 2; git scope exact (`.ino` one block + new test dir only)
- [ ] config.h, platformio.ini, prototype block, all other test files, ARDUINO region byte-stable (this task's diff)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete block listing and the complete 9-test file are provided verbatim below — both were inserted into a copy of the REAL current `.ino` (which already contains T1S1's landed `backstopMinutes`) and compiled with `g++ -std=c++17 -Wall -Wextra -Werror` + Unity 2.6.1 and executed green (**9 tests / 0 failures**) on this host this session (research/notes.md §4); the insertion anchor (`// ---- #ifdef ARDUINO REGION: hardware glue ----` at line 612, preceded by T1S1's landed end-banner at line 610 + one blank at 611) was verified in the current file; the consumed surface (`backstopMinutes`, `elapsedMs`, `deadlinePassed`, `FRESH_BOOT_BACKSTOP_MIN`, `PLUS5_MIN`, `BASELINE_SETTLE_MS`) was read from the landed files; `pio test -e native` / `pio run -e pico` mirror commands proven working in every prior landed subtask's session (most recently P1.M4.T1.S1 — test_backstop is green in the current tree).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: firmware/jug_filler/jug_filler.ino
  why: THE file you edit. Verified-current landmarks:
        (1) BLOCK ANCHOR — `// ---- #ifdef ARDUINO REGION: hardware glue ----`
        (line 612). Insert your block DIRECTLY ABOVE it, with exactly ONE
        blank line at each seam. The line above the seam blank is T1S1's
        landed end-banner `// ===== end backstop interpolation (consumed by
        P1.M4.T2.S1, P1.M6.T1.S2) =====` (line 610). DO NOT modify the
        backstop-interpolation block — your classes call its function.
        (2) HELPERS you reuse (do NOT reimplement): `elapsedMs(uint32_t now,
        uint32_t since)` (prototype line 32) and `deadlinePassed(uint32_t now,
        uint32_t deadline)` (body at line 70: `elapsedMs(now, deadline) <
        HALF_WHEEL_MS` — true iff now is at/after the deadline within the last
        half-wheel). `backstopMinutes` prototype at line 35, definition ends
        at line 610's banner.
  gotcha: if T1S1's end-banner is MISSING, the dependency has not landed —
        STOP and report (your block calls backstopMinutes; Task 0 checks
        this). If the ARDUINO marker itself is missing, the seam moved —
        STOP and report; do not improvise.

- file: firmware/jug_filler/config.h
  why: READ-ONLY source of everything you consume BY NAME:
        `FRESH_BOOT_BACKSTOP_MIN` (constexpr uint16_t = 30, line 32),
        `PLUS5_MIN` (constexpr uint16_t = 5, line 41), `BASELINE_SETTLE_MS`
        (test only). Do NOT add anything — §8 is frozen.
  gotcha: both constants are uint16_t — widen with (uint32_t) BEFORE
        multiplying by 60000u in extend5()/beginFill() (the listings
        already do).

- file: test/test_backstop/test_backstop.cpp
  why: THE most recent sibling test pattern (landed by P1.M4.T1.S1): relative
        `.ino` include, empty setUp/tearDown, hand-derived expected values
        with derivation comments, ITEM CASE tags on contract-pinned tests,
        explicit `int main() { UNITY_BEGIN(); RUN_TEST(...); ... return
        UNITY_END(); }` (REQUIRED on PIO 6.2.0 — no auto-generated runner).
  gotcha: one .cpp per test dir; each test/ subdir is a separate Unity
        project, auto-discovered — no platformio.ini change needed.

- file: plan/001_53984f832581/P1M4T2S1/research/notes.md
  why: this task's research: §1 contract→codebase mapping (line numbers),
        §2 design decisions (anchor-vs-press, anchor+budget storage,
        fresh-boot branch placement, truncation, abort, reset, pre-anchor
        +5), §3 the "~36-37 vs 39" discrepancy resolution, §4 machine-
        verification record, §5 downstream API freeze.
  section: §2 (design) and §3 (discrepancy)

- file: plan/001_53984f832581/P1M4T1S1/PRP.md
  why: CONTRACT for the dependency you consume. Its block sits directly
        above yours; treat as landed-and-frozen. Its frozen function
        `uint32_t backstopMinutes(uint32_t recoveryMin)` (floored, clamped,
        table-driven) is called by FillDeadline::beginFill. Its tests must
        stay green after your edit.
  gotcha: do NOT modify or reformat its block or banner.

- file: plan/001_53984f832581/architecture/firmware_architecture.md
  section: line 21 ("backstopMinutes / RecoveryClock / FillDeadline
        (P1.M4)"), §1 seam rules (pure logic, no hardware calls; classes
        need no prototypes).
  why: justifies UNGUARDED-region placement and the class shape.

- url: https://docs.platformio.org/en/stable/plus/unit-testing.html
  why: PlatformIO native Unity conventions — test_dir auto-discovery, one
        project per test/ subdir, explicit runner requirement on PIO 6.2.0.
  critical: never `pio test -e pico` (no on-device transport) — native
        tests only.
```

### Current Codebase tree (verified this session; expected at implementation start)

```bash
water-jug-filler/
├── .gitignore                # ".pio/"
├── README.md
├── platformio.ini            # [env:pico] compile-only; [env:native] Unity — no changes
├── firmware/jug_filler/
│   ├── jug_filler.ino        # 741 lines: seam + time helpers (elapsedMs,
│   │                         #   deadlinePassed) + ButtonClassifier +
│   │                         #   ReadingAggregator/medianOf + BaselineCapture
│   │                         #   + TripDetector + FalseTripGuards
│   │                         #   + backstopMinutes (P1.M4.T1.S1 — LANDED;
│   │                         #     end-banner at line 610 sits directly
│   │                         #     above YOUR insertion point)
│   │                         #   + ARDUINO region: logLine, BOOTSEL service,
│   │                         #     ADC wiring, setup/loop
│   │                         # ← EDIT (UNGUARDED region +1 block; NO prototype)
│   └── config.h              # every §8 tunable — READ-ONLY
│                             #   (FRESH_BOOT_BACKSTOP_MIN / PLUS5_MIN by name)
├── test/
│   ├── test_seam/  test_time_utils/  test_log_utils/  test_button/   # untouched
│   ├── test_reading/  test_baseline/  test_trip/  test_guards/       # untouched
│   ├── test_backstop/        # P1.M4.T1.S1's — untouched, must stay green
│   └── test_deadline/        # ← NEW (this task)
└── plan/001_53984f832581/    # read-only plan material
# Toolchain (verified on this machine): PlatformIO Core 6.2.0, host g++ (C++17),
# Unity 2.6.1 via Library Manager (.pio/libdeps/native/Unity).
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── firmware/jug_filler/
│   └── jug_filler.ino        # MODIFIED (additive): +1 bannered block:
│                             #   RecoveryClock (RAM recovery since last valve
│                             #   close) + FillDeadline (once-at-press budget,
│                             #   valve-open anchor, stacking +5, wrap-safe)
└── test/
    └── test_deadline/
        └── test_deadline.cpp # NEW: native Unity proof — fresh-boot 30,
                              #   15-min→39, abort reference, zero→44, anchor
                              #   + expiry boundary, +5 stacking (exact +10),
                              #   pre-anchor +5, rollover wrap, persistence
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — TWO instants, don't merge them: the budget MINUTES are computed
//   at the button PRESS (PRD §6.2 "computed once, at fill start") but the
//   deadline is ANCHORED at VALVE OPEN (PRD §6 preamble — the backstop bounds
//   valve-open time; the 2 s BASELINE_SETTLE_MS precedes valve open). Storage
//   is anchorMs_ + budgetMs_ precisely so extend5() works before OR after
//   anchor() and expiry stays wrap-safe via deadlinePassed.

// CRITICAL — expiry comparison MUST go through deadlinePassed (P1.M1.T2.S1),
//   never a raw `now >= deadlineMs_`: plain comparison breaks across the
//   uint32 millis rollover (~49.7 days uptime). deadlinePassed(now, d) is
//   `elapsedMs(now, d) < HALF_WHEEL_MS` — true iff now is at/after d within
//   the last half-wheel. Boundary: now == d → true (expired AT the deadline).

// CRITICAL — the fresh-boot branch lives in FillDeadline::beginFill, and it
//   must NOT call backstopMinutes with a fabricated recovery: T1S1's frozen
//   contract says no-history callers use FRESH_BOOT_BACKSTOP_MIN directly.

// CRITICAL — config constants are constexpr uint16_t: widen with (uint32_t)
//   before `* 60000u` (the listings do). 44 min = 2,640,000 ms fits uint32.

// GOTCHA: classes get NO entry in the explicit-prototype block (seam rule #3
//   covers FREE functions — elapsedMs/formatBootBanner/medianOf/backstopMinutes
//   precedent; ButtonClassifier & friends got none). You are adding classes:
//   prototype block stays byte-stable.

// GOTCHA: expired() must return false until anchored() — the settle window
//   runs no deadline (anchorMs_ is stale/garbage before anchor()).

// GOTCHA: the "~36-37" gloss in the item description for 15-min recovery is
//   WRONG (see What §discrepancy): the normative value is 39. Never adjust a
//   test's expected value to match prose; the values are machine-verified.

// GOTCHA: host-verification compile needs BOTH Unity files:
//   g++ ... -I<Unity>/src test.cpp <Unity>/src/unity.c   (unity.h alone links
//   to undefined UnityAssertEqualNumber).

// GOTCHA: never `pio test -e pico` (no on-device transport) — native tests
//   only; `pio run -e pico` is the compile gate.
```

## Implementation Blueprint

### Data models and structure

Two pure classes, no new types, no persistence, no dynamic memory:

```cpp
RecoveryClock:  bool hasHistory_; uint32_t lastFillEndMs_;          // RAM only (PRD §3/§5.3)
FillDeadline:   bool freshBoot_; bool anchored_;
                uint32_t recoveryMsAtStart_;  // §3 log input
                uint32_t tableMinutes_;       // §3 log input (grant at press)
                uint32_t budgetMs_;           // tableMinutes + PLUS5 stacks, in ms
                uint32_t anchorMs_;           // valve-open instant
```

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITION CHECK (do first; STOP and report on any miss)
  - RUN: grep -c "end backstop interpolation (consumed by P1.M4.T2.S1" firmware/jug_filler/jug_filler.ino  # EXPECT: 1 (P1.M4.T1.S1 LANDED — hard dependency)
  - RUN: grep -cE -- "---- UNGUARDED REGION: pure logic \(host-testable\) ----|---- #ifdef ARDUINO REGION: hardware glue ----" firmware/jug_filler/jug_filler.ino  # EXPECT: 2
  - RUN: grep -n "FRESH_BOOT_BACKSTOP_MIN\|PLUS5_MIN\|BASELINE_SETTLE_MS" firmware/jug_filler/config.h  # EXPECT: all present
  - RUN: grep -c "RecoveryClock\|FillDeadline" firmware/jug_filler/jug_filler.ino  # EXPECT: 0 (not yet landed)
  - RUN: pio test -e native                                                  # EXPECT: exit 0, all 9 projects [PASSED] (incl. test_backstop)
  - A miss means the seam moved or upstream regressed — do NOT improvise anchors.

Task 1: MODIFY firmware/jug_filler/jug_filler.ino — ONE additive edit
  - Insert the "Task 1 listing" verbatim directly ABOVE
    `// ---- #ifdef ARDUINO REGION: hardware glue ----`, with exactly ONE
    blank line above (after T1S1's `end backstop interpolation` banner) and
    ONE below (before the ARDUINO marker). Do NOT double-blank the seam.
    Do NOT touch the prototype block.
  - PRESERVE: everything else byte-for-byte — includes, prototype block, all
    other UNGUARDED sections (incl. backstopMinutes), the entire ARDUINO region.
  - The listing was compiled with -Wall -Wextra -Werror and its tests executed
    against the real current file — transcribe, don't redesign.

Task 2: CREATE test/test_deadline/test_deadline.cpp
  - IMPLEMENT: the "Task 2 listing" verbatim (9 tests + explicit Unity main).
  - NAMING: test_<behavior> functions; dir/file test_deadline follows the
    test_backstop/test_guards convention. PIO auto-discovers the new
    project — no build changes.

Task 3: RUN validation gates (all four levels below); fix and re-run until green.
```

**Task 1 listing — insert verbatim (directly above the ARDUINO marker):**

```cpp
// ===================== Recovery clock & fill deadline (P1.M4.T2.S1) =====================
// Timer-model STATE (PRD §6): wall-clock recovery between fills, and the
// per-fill valve-open budget. RAM only — NO persistence (PRD §3): any reset
// re-constructs these objects, which clears history, so the next fill uses
// FRESH_BOOT_BACKSTOP_MIN (PRD §5.3 "tank history cleared", §6.1). That is
// the entire §5.3 story for this state — documented, not implemented.
//
// RecoveryClock (PRD §6.1): the reference point is the moment the previous
// fill's VALVE CLOSED — whatever ended it (done, abort or fault): the fill
// engine (P1.M6.T1.S2) calls recordFillEnd() at that instant for all three.
// Recovery for a new fill = wall-clock ms since that reference, wrap-safe
// via elapsedMs (P1.M1.T2.S1). Abort restarts the clock exactly like a
// normal fill end — same call, same meaning.
//
// FillDeadline (PRD §6 preamble + §6.2): the backstop bounds how long the
// VALVE may stay open, so the deadline is ANCHORED at the valve-open
// instant, NOT at the button press (the 2 s baseline settle precedes valve
// open). The budget MINUTES are computed ONCE at fill start (button press):
//     no history  -> FRESH_BOOT_BACKSTOP_MIN (assume full tank: fast fill)
//     has history -> backstopMinutes(recoveryMs / 60000)   (P1.M4.T1.S1)
// Sub-minute recovery truncates to whole minutes (house integer style; the
// table itself is minute-granular). A long-press +5 (PLUS5_MIN, PRD §5.1,
// FILLING only) extends the ACTIVE fill's deadline only — stacks,
// repeatable — and never touches the table or the recovery clock.

class RecoveryClock {
public:
    // Every fill end — done, abort or fault: whatever closed the valve.
    void recordFillEnd(uint32_t nowMs) {
        lastFillEndMs_ = nowMs;
        hasHistory_ = true;
    }

    // False until the first recordFillEnd() — i.e. on every fresh boot.
    bool hasHistory() const { return hasHistory_; }

    // Wall-clock ms since the previous fill's valve closed (wrap-safe).
    // Caller MUST check hasHistory() first: with no history the return is
    // meaningless — use FRESH_BOOT_BACKSTOP_MIN, never this value.
    uint32_t recoveryMs(uint32_t nowMs) const {
        return elapsedMs(nowMs, lastFillEndMs_);
    }

private:
    bool hasHistory_ = false;    // RAM only: cleared by any reset (PRD §5.3)
    uint32_t lastFillEndMs_ = 0;
};

class FillDeadline {
public:
    // Compute the budget ONCE at fill start (button press, PRD §6.2) and
    // snapshot the log inputs (recovery, table result — PRD §3 log line).
    void beginFill(const RecoveryClock& clock, uint32_t nowMs) {
        freshBoot_ = !clock.hasHistory();
        recoveryMsAtStart_ = freshBoot_ ? 0 : clock.recoveryMs(nowMs);
        tableMinutes_ = freshBoot_
                            ? FRESH_BOOT_BACKSTOP_MIN
                            : backstopMinutes(recoveryMsAtStart_ / 60000u);
        budgetMs_ = (uint32_t)tableMinutes_ * 60000u;
        anchored_ = false;  // valve not open yet — baseline settle comes first
    }

    // Fix the deadline origin at the VALVE-OPEN instant (PRD §6 preamble):
    // the backstop bounds valve-open time, not button-press time.
    void anchor(uint32_t valveOpenedAtMs) {
        anchorMs_ = valveOpenedAtMs;
        anchored_ = true;
    }

    // True once the valve-open budget — including every +5 extension — is
    // used up. False until anchored(): the settle window runs no deadline.
    // deadlineMs (anchor + budget) wraps naturally; deadlinePassed
    // (P1.M1.T2.S1) is the rollover-safe comparison.
    bool expired(uint32_t nowMs) const {
        return anchored_ && deadlinePassed(nowMs, anchorMs_ + budgetMs_);
    }

    // Long-press +5 (PRD §5.1): stacks, repeatable, ACTIVE fill only.
    // Valid before or after anchor() — the budget grows either way and the
    // deadline (anchor + budget) follows.
    void extend5() { budgetMs_ += (uint32_t)PLUS5_MIN * 60000u; }

    // ---- log inputs for the backstop computation line (PRD §3) ----
    bool freshBoot() const { return freshBoot_; }
    uint32_t recoveryMsAtStart() const { return recoveryMsAtStart_; }
    uint32_t tableMinutes() const { return tableMinutes_; }        // grant at fill start
    uint32_t budgetMinutes() const { return budgetMs_ / 60000u; }  // incl. +5 stacks
    uint32_t deadlineMs() const { return anchorMs_ + budgetMs_; }  // wraps naturally
    bool anchored() const { return anchored_; }

private:
    bool freshBoot_ = true;
    bool anchored_ = false;
    uint32_t recoveryMsAtStart_ = 0;
    uint32_t tableMinutes_ = 0;
    uint32_t budgetMs_ = 0;
    uint32_t anchorMs_ = 0;
};
// ===== end recovery clock & fill deadline (consumed by P1.M6.T1.S2, P1.M6.T2.S1) =====
```

**Task 2 listing — `test/test_deadline/test_deadline.cpp` (verbatim):**

```cpp
// test/test_deadline/test_deadline.cpp — native contract proof for
// RecoveryClock + FillDeadline (P1.M4.T2.S1). Pins PRD §6.1/§6.2/§6 preamble:
// fresh-boot 30-min default; recovery measured from the previous fill's
// valve-close (abort restarts it identically); budget computed once at fill
// start via backstopMinutes(); deadline anchored at the VALVE-OPEN instant;
// +5 long-press stacks on the active fill only; wrap-safe arithmetic.
// Interpolated expectations derive from the frozen backstopMinutes contract
// (P1.M4.T1.S1): 15 min recovery -> 39 (exact 39.5 floored); 0 min -> 44.
#include <unity.h>

#include "../../firmware/jug_filler/jug_filler.ino"

void setUp() {}
void tearDown() {}

// ITEM CASE: fresh boot / any reset — no history -> FRESH_BOOT_BACKSTOP_MIN
// (30), recovery input reads 0, and the fresh-boot flag is exposed for the
// §3 backstop log line. (RAM-only state: a reset is just a new object.)
void test_fresh_boot_uses_30min_default() {
    RecoveryClock clock;      // fresh boot: no recordFillEnd ever called
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    TEST_ASSERT_TRUE(dl.freshBoot());
    TEST_ASSERT_EQUAL_UINT32(0, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(30, dl.tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(30, dl.budgetMinutes());
}

// ITEM CASE: history at 15 min recovery -> interpolated value. The item
// gloss says "~36-37, assert computed": the computed normative value from
// the frozen table is 39 (exact 39.5 on the 10->20 segment, floored by
// P1.M4.T1.S1). Assert BOTH the pinned number and the computed one.
void test_history_15min_recovery_interpolates() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    clock.recordFillEnd(fillEnd);
    TEST_ASSERT_TRUE(clock.hasHistory());

    FillDeadline dl;
    dl.beginFill(clock, fillEnd + 15u * 60u * 1000u);   // 15 min later
    TEST_ASSERT_FALSE(dl.freshBoot());
    TEST_ASSERT_EQUAL_UINT32(15u * 60u * 1000u, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(39, dl.tableMinutes());
    TEST_ASSERT_EQUAL_UINT32(backstopMinutes(15), dl.tableMinutes());  // computed
}

// ITEM CASE: abort resets the reference at the abort instant (PRD §6.1):
// recovery for the next fill is measured from the ABORT, not from the
// earlier fill end.
void test_abort_restarts_reference_at_abort_instant() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    const uint32_t abortAt = fillEnd + 5u * 60u * 1000u;   // aborted 5 min in
    clock.recordFillEnd(fillEnd);
    clock.recordFillEnd(abortAt);   // abort == fill end, same call

    FillDeadline dl;
    dl.beginFill(clock, abortAt + 15u * 60u * 1000u);      // 15 min after abort
    TEST_ASSERT_EQUAL_UINT32(15u * 60u * 1000u, dl.recoveryMsAtStart());  // NOT 20 min
    TEST_ASSERT_EQUAL_UINT32(39, dl.tableMinutes());
}

// ITEM CASE: recovery 0 — immediate restart clamps to the table's first
// knot: backstopMinutes(0) == 44.
void test_zero_recovery_clamps_to_44() {
    RecoveryClock clock;
    const uint32_t fillEnd = 1000000u;
    clock.recordFillEnd(fillEnd);
    FillDeadline dl;
    dl.beginFill(clock, fillEnd);   // restarted at the same instant
    TEST_ASSERT_EQUAL_UINT32(0, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(44, dl.tableMinutes());
}

// ITEM CASE: deadline anchored at the VALVE-OPEN instant, not the button
// press: with a 2 s settle, anchor is now+2000, and expiry is measured
// from there (PRD §6 preamble). Boundary: one ms before -> open, exactly
// at -> expired.
void test_anchor_and_expiry_boundary() {
    RecoveryClock clock;                    // fresh boot -> 30 min
    FillDeadline dl;
    const uint32_t press = 1000u;
    dl.beginFill(clock, press);
    TEST_ASSERT_FALSE(dl.expired(press));            // no deadline until anchored
    TEST_ASSERT_FALSE(dl.anchored());
    dl.anchor(press + BASELINE_SETTLE_MS);           // valve opens after settle
    TEST_ASSERT_TRUE(dl.anchored());

    const uint32_t deadline = dl.deadlineMs();
    TEST_ASSERT_EQUAL_UINT32(press + BASELINE_SETTLE_MS + 30u * 60u * 1000u,
                             deadline);
    TEST_ASSERT_FALSE(dl.expired(deadline - 1u));    // last granted ms
    TEST_ASSERT_TRUE(dl.expired(deadline));          // boundary: expired
}

// ITEM CASE: +5 twice moves the deadline EXACTLY 10 min (stacks,
// repeatable) — and a deadline that had just expired opens again.
void test_extend5_stacks() {
    RecoveryClock clock;                    // fresh boot -> 30 min
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    dl.anchor(3000u);
    const uint32_t d0 = dl.deadlineMs();

    dl.extend5();
    TEST_ASSERT_EQUAL_UINT32(35, dl.budgetMinutes());
    dl.extend5();
    TEST_ASSERT_EQUAL_UINT32(40, dl.budgetMinutes());
    TEST_ASSERT_EQUAL_UINT32(d0 + 2u * PLUS5_MIN * 60u * 1000u, dl.deadlineMs());

    TEST_ASSERT_FALSE(dl.expired(d0));               // old deadline: no longer expired
    TEST_ASSERT_FALSE(dl.expired(dl.deadlineMs() - 1u));
    TEST_ASSERT_TRUE(dl.expired(dl.deadlineMs()));
}

// +5 before the valve opens (during the settle window): the budget grows
// first and the anchor then bakes it into the deadline — same total.
void test_extend5_before_anchor() {
    RecoveryClock clock;
    FillDeadline dl;
    dl.beginFill(clock, 1000u);
    dl.extend5();
    dl.extend5();
    dl.anchor(3000u);
    TEST_ASSERT_EQUAL_UINT32(40, dl.budgetMinutes());
    TEST_ASSERT_EQUAL_UINT32(3000u + 40u * 60u * 1000u, dl.deadlineMs());
    TEST_ASSERT_FALSE(dl.expired(3000u + 40u * 60u * 1000u - 1u));
    TEST_ASSERT_TRUE(dl.expired(3000u + 40u * 60u * 1000u));
}

// ITEM CASE: wrap-safe arithmetic — anchor near the uint32 rollover and a
// recovery clock read across the rollover both behave (elapsedMs /
// deadlinePassed, P1.M1.T2.S1).
void test_wrap_safe_across_rollover() {
    // Recovery across the rollover: fill ends 0x200 before wrap, read after.
    RecoveryClock clock;
    const uint32_t fillEnd = 0xFFFFFF00u;
    clock.recordFillEnd(fillEnd);
    const uint32_t now = (uint32_t)(fillEnd + 0x200u);   // wraps to 0x100
    TEST_ASSERT_EQUAL_UINT32(0x200u, clock.recoveryMs(now));

    FillDeadline dl;
    dl.beginFill(clock, now);        // recovery 0 ms -> 0 min -> clamp 44
    TEST_ASSERT_EQUAL_UINT32(44, dl.tableMinutes());
    dl.anchor(0xFFFFFFFFu - 1000u);  // valve opens just before the rollover
    const uint32_t deadline = dl.deadlineMs();          // wraps naturally
    TEST_ASSERT_EQUAL_UINT32(
        (uint32_t)(0xFFFFFFFFu - 1000u + 44u * 60u * 1000u), deadline);
    TEST_ASSERT_FALSE(dl.expired((uint32_t)(deadline - 1u)));
    TEST_ASSERT_TRUE(dl.expired(deadline));
}

// A second fill after the first one ends: the clock keeps serving (RAM
// history persists across fills within one boot — only a RESET clears it).
void test_history_persists_across_fills_until_reset() {
    RecoveryClock clock;
    clock.recordFillEnd(500000u);
    clock.recordFillEnd(500000u + 60u * 60u * 1000u);    // 60 min later
    FillDeadline dl;
    dl.beginFill(clock,
                 500000u + 60u * 60u * 1000u + 10u * 60u * 1000u);  // +10 min
    TEST_ASSERT_EQUAL_UINT32(10u * 60u * 1000u, dl.recoveryMsAtStart());
    TEST_ASSERT_EQUAL_UINT32(41, dl.tableMinutes());     // knot at 10 min
}

int main() {  // REQUIRED on PIO 6.2.0 — no auto-generated Unity runner
    UNITY_BEGIN();
    RUN_TEST(test_fresh_boot_uses_30min_default);
    RUN_TEST(test_history_15min_recovery_interpolates);
    RUN_TEST(test_abort_restarts_reference_at_abort_instant);
    RUN_TEST(test_zero_recovery_clamps_to_44);
    RUN_TEST(test_anchor_and_expiry_boundary);
    RUN_TEST(test_extend5_stacks);
    RUN_TEST(test_extend5_before_anchor);
    RUN_TEST(test_wrap_safe_across_rollover);
    RUN_TEST(test_history_persists_across_fills_until_reset);
    return UNITY_END();
}
```

### Implementation Patterns & Key Details

```cpp
# PATTERN: bannered pure-class block in the UNGUARDED region
#   (ButtonClassifier/BaselineCapture/TripDetector/FalseTripGuards precedent):
#   banner comment cites PRD sections + task ID; end-banner names downstream
#   consumers. NO prototype entry (seam rule #3 is free-functions-only).

# CRITICAL: two instants — beginFill() at the PRESS computes & freezes the
#   minutes (PRD §6.2 "once, at fill start"); anchor() at VALVE OPEN fixes
#   the origin (PRD §6 preamble). Never bake the deadline at press time.

# CRITICAL: expired() = anchored_ && deadlinePassed(now, anchorMs_+budgetMs_)
#   — reuses P1.M1.T2.S1's rollover-safe helper; guard on anchored_ so the
#   settle window (anchor not yet called) reports false.

# PATTERN: consume config BY NAME — FRESH_BOOT_BACKSTOP_MIN, PLUS5_MIN —
#   widen uint16_t → uint32_t before *60000u. §9 commissioning may retune.

# GOTCHA: deadlineMs() wraps naturally (unsigned overflow is defined
#   behavior) — fine for the §3 log line and the tests; expiry never uses
#   it directly (always via deadlinePassed).

# GOTCHA: beginFill() must NOT call backstopMinutes() when there is no
#   history — use FRESH_BOOT_BACKSTOP_MIN directly (T1S1's frozen contract).
```

### Integration Points

```yaml
FIRMWARE:
  - Only firmware/jug_filler/jug_filler.ino changes: +1 bannered block at the
    end of the UNGUARDED region (directly above the ARDUINO marker, directly
    below T1S1's end-banner). Prototype block, includes, ARDUINO region,
    setup()/loop() all untouched.
BUILD:
  - None (platformio.ini already compiles the sketch for pico and auto-discovers
    test/ dirs for native).
CONFIG:
  - None. ZERO new constants; consumes FRESH_BOOT_BACKSTOP_MIN, PLUS5_MIN
    (and BASELINE_SETTLE_MS in tests only) from config.h BY NAME.
TEST:
  - New project dir test/test_deadline/ (single .cpp, explicit Unity main).
DOCS:
  - None (item spec: internal; README sync is P2.M1.T3.S1).
DOWNSTREAM API (frozen by this task — P1.M6.T1.S2 / P1.M6.T2.S1 build on it):
  - RecoveryClock: recordFillEnd(nowMs) at EVERY valve close (done/abort/
    fault); hasHistory(); recoveryMs(nowMs).
  - FillDeadline: beginFill(clock, pressMs) on press→FILLING; anchor(
    valveOpenedAtMs) at valve open; expired(nowMs) each loop while FILLING;
    extend5() on +5 long-press; log accessors freshBoot()/recoveryMsAtStart()/
    tableMinutes()/budgetMinutes()/deadlineMs().
SCOPE BOUNDARIES (downstream owners — do NOT implement here):
  - valve control on expiry, state machine, CONFIRM/DONE/FAULT      -> P1.M6
  - hardware event wiring + the actual §3 logLine call              -> P1.M6.T2.S1
  - +5 LED ack                                                      -> P1.M5/P1.M6
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
# Expected: exit 0; all 10 projects [PASSED] — test_seam, test_time_utils,
# test_log_utils, test_button, test_reading, test_baseline, test_trip,
# test_guards, test_backstop AND the new test_deadline (9/9 tests).
# ANY failure: read the mismatch — the expected values are machine-verified
# against the real file, so a failure means the implementation (not the test)
# deviates. Never "fix" a test's expected value to make it pass.

# Targeted re-run while iterating on one area:
pio test -e native -f test_deadline
```

### Level 3: Integration Testing (System Validation)

```bash
# (1) Scope check — only this task's artifacts:
git status --short
# Expected:
#  M firmware/jug_filler/jug_filler.ino
#  ?? test/test_deadline/

# (2) Structural placement: your block sits between T1S1's end-banner and the
#     ARDUINO marker, one blank line at each junction:
grep -n -A2 "end backstop interpolation" firmware/jug_filler/jug_filler.ino
#   Expected: banner line, blank, then "// ===================== Recovery clock & fill deadline"
grep -n -B2 -- "---- #ifdef ARDUINO REGION" firmware/jug_filler/jug_filler.ino
#   Expected: blank, "// ===== end recovery clock & fill deadline ... =====", marker

# (3) Classes present exactly once, no prototype added:
grep -c "class RecoveryClock" firmware/jug_filler/jug_filler.ino   # 1
grep -c "class FillDeadline" firmware/jug_filler/jug_filler.ino    # 1
git diff firmware/jug_filler/jug_filler.ino | grep -c "^+.*;$" | head -1   # added lines are inside the one block

# (4) No churn where none belongs:
git diff --stat firmware/jug_filler/config.h platformio.ini   # empty
git diff firmware/jug_filler/jug_filler.ino | grep "^-[^-]" | head   # empty — purely additive
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

# (3) Constants by name — no minute-value literals in the class bodies
#     (30/44/39/41 etc. appear only in TESTS, where they are pinned contract
#     values; the CLASSES must use FRESH_BOOT_BACKSTOP_MIN / PLUS5_MIN /
#     backstopMinutes()):
sed -n '/class RecoveryClock/,/end recovery clock & fill deadline/p' firmware/jug_filler/jug_filler.ino \
  | sed 's://.*$::' | grep -nE "\b(30|44|39|41|35|40|5)\b" \
  || echo "NO MINUTE LITERALS IN CLASSES"

# (4) Safety-reasoning audit (mirrors research/notes.md §2/§4):
#     - Anchor semantics: expiry measured from press+BASELINE_SETTLE_MS, not
#       press (test_anchor_and_expiry_boundary pins the exact deadline).
#     - Wrap safety: expired() routes through deadlinePassed; recoveryMs()
#       through elapsedMs — no raw comparisons anywhere.
#     - +5 stacking: two extend5() calls move deadlineMs() by EXACTLY
#       2*PLUS5_MIN*60000 and un-expire a just-passed old deadline.
#     - Fresh boot never reaches backstopMinutes() with fabricated recovery.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 all pass; `pio run -e pico` `[SUCCESS]`; `pio test -e native` exit 0, every project `[PASSED]` (test_deadline 9/9; test_backstop and all siblings still green)
- [ ] "PURE OK (comments stripped)"; marker count 2; "NO MINUTE LITERALS IN CLASSES"; purely additive diff
- [ ] `git status --short`: `.ino` + `test/test_deadline/` only; config.h/platformio.ini/prototype block/ARDUINO region untouched

### Feature Validation

- [ ] Fresh boot → 30 (`FRESH_BOOT_BACKSTOP_MIN`), `freshBoot()` true, recovery input 0
- [ ] 15-min recovery → 39 (pinned AND computed via `backstopMinutes(15)`; item gloss "~36-37" superseded by the normative table)
- [ ] Abort re-anchors the reference at the abort instant (15 min after abort → 900000 ms, NOT 20 min)
- [ ] Zero recovery → 44; 60-min-later history → 41 (knot) via persistence test
- [ ] Valve-open anchoring: `deadlineMs() == press + BASELINE_SETTLE_MS + 30 min`; `expired()` false until anchored; boundary `deadline−1` open / `deadline` expired
- [ ] `extend5()` stacking: 30→35→40; deadline moves exactly +10 min; pre-anchor +5 identical
- [ ] Wrap-safe across the uint32 rollover for both clock and deadline

### Code Quality Validation

- [ ] Seam rules honored: zero hardware tokens in non-comment UNGUARDED lines; bannered class block, NO prototype entry; const-correct getters
- [ ] Zero new config constants; `FRESH_BOOT_BACKSTOP_MIN`/`PLUS5_MIN` by name; existing helpers (`elapsedMs`, `deadlinePassed`, `backstopMinutes`) reused, not reimplemented
- [ ] House test conventions followed: relative `.ino` include, hand-derived expected values in comments, ITEM CASE tags, explicit Unity `main()`
- [ ] Listing transcribed verbatim (it is machine-verified); any deliberate deviation re-justified in the completion report

### Documentation & Deployment

- [ ] Banner comments name the task ID and downstream consumers (P1.M6.T1.S2, P1.M6.T2.S1); PRD §5.3 reset semantics documented in-code (RAM-only state)
- [ ] Caller protocol documented in-code: compute-once-at-press, anchor-at-valve-open, recordFillEnd-at-every-close, hasHistory-before-recoveryMs
- [ ] No README/config/env/docs changes (item spec: docs none)

---

## Anti-Patterns to Avoid

- ❌ Don't anchor the deadline at the button press — PRD §6 preamble: the backstop bounds VALVE-OPEN time; the 2 s settle precedes valve open (test pins `press + BASELINE_SETTLE_MS + budget`)
- ❌ Don't compare millis with raw `now >= deadline` — breaks at the uint32 rollover; always `deadlinePassed` / `elapsedMs` (P1.M1.T2.S1)
- ❌ Don't call `backstopMinutes()` with a fabricated recovery on fresh boot — no-history path uses `FRESH_BOOT_BACKSTOP_MIN` directly (frozen T1S1 contract)
- ❌ Don't bake the deadline at beginFill and add deltas to it — store `anchorMs_ + budgetMs_` so `extend5()` is order-independent and wrap-safe
- ❌ Don't add persistence (EEPROM/flash) for the recovery clock — PRD §3/§6.1: RAM only; a reset clearing history IS the §5.3 contract
- ❌ Don't let `expired()` answer before `anchor()` — stale `anchorMs_` would fabricate an expiry during the settle window
- ❌ Don't add a prototype for these classes, a `begin()`-style hardware hook, or any ARDUINO-region wiring — pure objects only; the fill engine and loop wiring own the calls
- ❌ Don't "fix" the 39 expectation to match the item's "~36-37" gloss — the PRD table and T1S1's frozen test both say 39; a failure means the implementation deviates
- ❌ Don't touch the backstopMinutes block, the FalseTripGuards block, config.h, or any sibling test — additive-only edit at the one named anchor
