# PRP — P1.M2.T2.S2 (bugfix changeset): Sessions B and C — Flow 1 (abort + recomputed restart) and Flow 2 (successful fill) with evidence capture

## Goal

**Feature Goal**: Execute the two data-gathering bench sessions on the operational rig proven by P1.M2.T2.S1 — **Session B (Flow 1)**: fresh-boot fill, mid-fill short-press abort, then a restart whose backstop is recomputed from the §6.2 table; **Session C (Flow 2)**: a complete successful fill (≥60 s open → trip sensor 1 → confirm sensor 2 within 12 s → DONE → ACK → next press starts a new fill) — with hand-dipped sensors, USB-only power, valve dry, and genuine date-stamped serial captures as BUG-003 evidence.

**Deliverable**:
- `docs/bench/YYYY-MM-DD_session-B-flow1-abort.log` — raw 115200 8N1 capture (operator header block as first lines) proving Flow 1, including the table-derived restart (`backstop=44min recovery=<non-zero>ms` after a ≤2-min wait).
- `docs/bench/YYYY-MM-DD_session-C-flow2-success.log` — raw capture proving Flow 2 closes the cycle: `TRIP sensor=1` → `CONFIRM pass sensor=2` → `DONE` → `ACK done` → new `FILL start`.
- `docs/bench/template-session-B-flow1-abort.md` and `docs/bench/template-session-C-flow2-success.md` — filled IN PLACE (session record + ticked ordered-prefix checklists, waits/dip timings recorded).
- `docs/bench/README.md` — index rows B and C finalized (real dates, evidence filenames, FW git rev, `pass/total`, status from the closed vocabulary).
- `docs/bench/YYYY-MM-DD-discrepancies.md` — appended (or created from `template-discrepancies.md` if this bench day has none yet) for every expectation mismatch; D-1 (LED indicator nuance) confirmed with observations from these sessions.

**Success Definition**: Both logs are genuine captures starting with the operator header block; every Session B/C checklist item ticked with observed values; the two CRITICAL assertions hold — (B) the restart line shows `recovery != 0ms` AND `backstop=44min`/`budget=44min` (table-derived, NOT the 30-min fresh-boot default), (C) trip occurs at ≥60 s open, confirm within 12 s, `CONFIRM` precedes `DONE`, `ACK done` follows, and a post-ACK press yields a new `FILL start` — OR rows B/C read `BLOCKED: <reason>` with NO log files created (integrity rule: never fabricate). `pio run -e pico` still `[SUCCESS]`; `git status` shows only the additions/edits listed above.

## User Persona

**Target User**: The bench operator (human, or an agent with physical rig access) executing the BUG-003 evidence plan; downstream, P1.M2.T2.S3 (fault paths reuse the wet-sensor hand-dip technique proven here) and P1.M2.T2.S5 (consolidation reads rows B/C).

**Use Case**: With the rig proven operational (S1 row A = PASS), templates authored (T1.S2), and firmware already flashed, run Flow 1 and Flow 2 exactly per their templates on USB-only power, hand-dipping sensors into a cup of water, dry-firing the valve, capturing each session's stream to its date-stamped log.

**User Journey**: Session B — start capture → RESET → short press (fill starts, 30-min fresh-boot backstop) → short press again mid-fill (`VALVE close` → `ABORT`) → wait ~1 min (count ambient lines) → short press (restart; expect `backstop=44min recovery=<small>ms`) → short press to abort the restart fill and close the valve → stop capture, fill template B. Session C — new capture → RESET → short press → keep valve open ≥60 s (count ~60–65 ambient `S1=` lines) → dip sensor 1, hold ~1–2 s (`VALVE close` → `TRIP sensor=1`) → within 12 s dip sensor 2, hold ~1–2 s (`CONFIRM pass sensor=2` → `DONE`, green triple-pulse) → short press (`ACK done`, LED off) → short press again (new `FILL start`) → abort it within <5 s → stop capture, fill template C.

**Pain Points Addressed**: BUG-003 — the §9.4 flow validation (abort/restart semantics and a full successful fill) has no physical evidence; this task supplies the wet-sensor proof that Flow 1 restarts use the recovery table and Flow 2 closes its cycle.

## Why

- Bug-fix PRD Issue 3 (BUG-003, Minor): "physical bench dry-run — flows 1 and 2 with hand-dipped sensors … captured serial logs as evidence under docs/bench/" — scaffolding exists (T1.S1/T1.S2), rig proven (T2.S1); this task is the flow evidence itself.
- Bug-fix PRD Recommendations: "Execute the deferred physical bench validation … on USB-only power and capture the serial logs under docs/bench/."
- bench_evidence_package.md §2 Sessions B and C are the binding checklists this runbook implements; §4 executability split: captures/observations are physical (operator), verification gates are software (agent).
- Downstream: P1.M2.T2.S3's fault-path sessions reuse the hand-dip technique and timing discipline proven here; P1.M2.T2.S5 consolidates rows A–E.

## What

Two sequential physical sessions plus evidence finalization. **Power exclusivity is binding** (SPEC §2.2; the RP2040-Zero VSYS pin ties directly to USB VBUS with no diode): USB-powered only, buck 5 V disconnected from the board; 12 V may feed the MOSFET module's power input ALONE.

1. **Preconditions (software)**: templates exist; index row A reads PASS (the S1-defined entry gate — if it doesn't, take the BLOCKED branch); `pio run -e pico` compiles clean; record `git rev-parse --short HEAD` for the headers/index rows.
2. **Session B — Flow 1 (abort + recomputed restart)**: start capture BEFORE RESET; header block prepended. Fresh boot banner; short press → `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok` → `VALVE open budget=30min` (LED solid). Short press again (clean, <1 s hold) → `VALVE close` → `ABORT attemptms= openms=` (LED off). Wait a recorded interval (~1 min; any wait ≤2 min is unambiguous) → short press → `FILL start backstop=44min recovery=<non-zero>ms recmin=<0|1>` → `BASE ok` → `VALVE open budget=44min`. Then abort the restart fill too (short press, <5 s open) to close the valve, and stop the capture. Fill template B in place; run the greps.
3. **Session C — Flow 2 (successful fill)**: new capture BEFORE a fresh RESET; header block prepended. Short press → `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok` → `VALVE open budget=30min`. Keep sensors dry and out of the cup; count ~60–65 ambient `S1=` lines (1 Hz) after `VALVE open`, then dip sensor 1 to mount depth, holding ~1–2 s → `VALVE close` → `TRIP sensor=1 fillms=<≥60000>`. Within 12 s dip sensor 2, hold ~1–2 s → `CONFIRM pass sensor=2 durms=<≤12000>` → `DONE attemptms= openms=<≥60000>`; observe the green triple-pulse (3 × on 200 ms / off 200 ms, then 1000 ms off, repeating) and record which indicator(s) render it (D-1). Short press → `ACK done` (blinking stops, LED off). Short press again → new `FILL start …` (expect `backstop=44min` for a quick press — recovery since the DONE close). Abort that fill within <5 s to close the valve; stop capture. Fill template C in place; run the greps.
4. **Finalize**: update index rows B and C only; append/create the date-stamped discrepancies file for any mismatch (record, never edit evidence); verify git scope.

NOT in scope: Session A (done in T2.S1); fault-path sessions (T2.S3 — a wet sensor at start, a missed confirm window, or any `FAULT …` line belongs THERE, and a faulted Session C attempt must be retaken, not ticked); watchdog check (T2.S4); consolidation beyond rows B/C (T2.S5); README/COMMISSIONING sweeps (T2.S3.x); ANY firmware change or reflash; editing spec/SPEC.md (source of truth, Mode A).

### Success Criteria

- [ ] Row A reads PASS before starting; `docs/bench/template-session-B-flow1-abort.md` and `template-session-C-flow2-success.md` exist at Task 0
- [ ] `docs/bench/YYYY-MM-DD_session-B-flow1-abort.log` genuine capture: banner first stream line; first fill `backstop=30min recovery=0ms recmin=0`; mid-fill `VALVE close` → `ABORT attemptms= openms=`; restart `FILL start backstop=44min recovery=<≠0>ms` with `VALVE open budget=44min` (wait ≤2 min); every abort press logs `BTN short <n>ms` with n < 1000
- [ ] `docs/bench/YYYY-MM-DD_session-C-flow2-success.log` genuine capture: `VALVE close` → `TRIP sensor=1 fillms=<≥60000>`; `CONFIRM pass sensor=2 durms=<≤~12000>` BEFORE `DONE attemptms= openms=<≥60000>`; `ACK done`; a later `FILL start` follows the ACK
- [ ] Both templates filled in place (session record: date, evidence log, FW git rev, checklist pass/total, waits/dip notes); D-1 LED observation recorded in the discrepancies file
- [ ] Index rows B and C finalized with real dates/filenames/rev/pass-total and statuses from the closed vocabulary
- [ ] `pio run -e pico` `[SUCCESS]`; `git status` scope = the two logs + two template fills + README rows B/C (+ discrepancies file); nothing under firmware/, test/, spec/, README.md root, COMMISSIONING.md

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the complete two-session runbook with byte-exact expected log lines (copied from `jug_filler.ino` format strings and the T1.S2 templates), the §6.2 backstop table with expected rows per wait, the recmin truncation semantics, the dip/debounce timing technique (8 readings × 100 ms), the 60-s/12-s guard boundaries, the LED contract, the header block, index row columns, fill discipline, BLOCKED protocol, and post-capture machine-checkable gates. All contracts were read from disk this session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/bench/README.md
  why: THE index you update (rows B and C) + §3 capture procedure (USB-only binding
        rule, 115200 8N1, capture BEFORE power/reset, operator header block, no .log
        edits ever) + §1 integrity rule (genuine captures or BLOCKED) + §2 closed
        status vocabulary + §4 naming conventions (one discrepancies file per bench
        day, shared). Rows B/C today: PENDING with `—` cells — replace them.

- file: docs/bench/template-session-B-flow1-abort.md
  why: THE Session B checklist — fill IN PLACE. Carries the expected lines, the
        preconditions (sensors dry in air — a wet sensor at start is Session D's
        baseline fault), the "Wait used: __" field, and the two CRITICAL assertions.
  gotcha: its restart example hardcodes `recmin=0`; with the suggested ≈1-min wait a
        printed recmin of 1 is equally correct (recmin truncates recovery/60000).

- file: docs/bench/template-session-C-flow2-success.md
  why: THE Session C checklist — fill IN PLACE. Carries the ≥60-s-open precondition
        (TRIP_MIN_ELAPSED_S), the 12-s confirm window, DONE pulse description, the
        ACK/next-fill steps, and the retake-on-fault rule.
  gotcha: "retake the session rather than ticking the box" — an early trip or missed
        confirm is a FAULT (Session D territory); start a fresh RESET attempt.

- file: docs/bench/template-discrepancies.md
  why: copy to YYYY-MM-DD-discrepancies.md ONLY if no file exists for that bench day
        yet (S1 may have created it); otherwise APPEND entries. D-1 (WS2812 GP16 vs
        external GP14 green LED) is pre-seeded — confirm it with B/C LED observations.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S1/PRP.md
  why: CONTRACT for the completed prior item. Assumes: rails + Session-A evidence
        landed, row A = PASS, firmware already flashed (NO reflash), discrepancy-day
        file may exist. Your entry gate is row A's status.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  section: "1. BINDING power-safety rules" + "2. Session B" + "2. Session C" + "4. Executability split"
  why: §1 the five binding power rules (never USB+buck simultaneously; 12 V to the
        MOSFET module alone OK; dry-fire opens <5 s where the flow aborts; data
        sessions USB-powered). §2 the byte-exact ordered prefixes for B and C. §4 the
        physical-vs-software split and the never-fabricate integrity rule.

- file: firmware/jug_filler/jug_filler.ino
  section: formatFillLogEvent (~lines 1343–1395)
  why: byte-exact format strings: `FILL start backstop=%umin recovery=%lums recmin=%u`,
        `BASE ok s1=%u s2=%u`, `VALVE open budget=%umin`, `VALVE close`,
        `ABORT attemptms=%lu openms=%lu`, `TRIP sensor=%u fillms=%lu`,
        `CONFIRM pass sensor=%u durms=%lu`, `DONE attemptms=%lu openms=%lu`, `ACK done`.
        Read-only reference — do NOT edit.

- file: firmware/jug_filler/config.h
  why: the tunables that bound the runbook: FRESH_BOOT_BACKSTOP_MIN=30,
        BACKSTOP_TABLE_MIN {2→44, 10→41, 20→38, 35→34, 60→30} (piecewise-linear,
        clamped), TRIP_MIN_ELAPSED_S=60, EARLY_WET_WINDOW_S=5, CONFIRM_WINDOW_S=12,
        DEBOUNCE_COUNT=8, READING_INTERVAL_MS=100, BUTTON_LONG_PRESS_MS=1000,
        DONE_PULSE 3×(200/200)+1000.

- file: spec/SPEC.md
  section: §5.2 (flows), §6.1–6.2 (recovery clock + backstop table), §7 (lifecycle,
        valve-closed-first §7.9, guards §7.6), §4 (LED contract)
  why: source of truth for WHY the restart must not be recovery=0ms and why the
        budget comes from the table. NEVER edited (Mode A).

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: hardware reference for the 5V/VSYS-USB VBUS tie (no diode) behind the
        exclusivity rule, and BOOT button location.

- url: https://arduino-pico.readthedocs.io/en/latest/bootsel.html
  why: BOOT-button notes — runtime presses never enter the bootloader (entry needs
        BOOTSEL during RESET); safe to press freely, but never hold BOOT while
        (re)plugging power.
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── README.md                      # read-only here (swept by P1.M2.T3.S1)
├── platformio.ini                 # untouched
├── firmware/jug_filler/           # flashed already; NO edits, NO reflash this task
├── test/                          # untouched
├── docs/
│   ├── COMMISSIONING.md           # read-only here (swept by P1.M2.T3.S2)
│   └── bench/
│       ├── README.md              # index — row A finalized (PASS); EDIT rows B, C only
│       ├── template-session-A-dry-bench.md   # filled by S1 — do not touch
│       ├── template-session-B-flow1-abort.md # T1.S2 — fill IN PLACE (this task)
│       ├── template-session-C-flow2-success.md # T1.S2 — fill IN PLACE (this task)
│       ├── template-session-{D,E}-*.md       # T1.S2 — do not touch
│       ├── template-rails.md                 # S1 copied it — do not touch
│       ├── YYYY-MM-DD_session-A-dry-bench.log + YYYY-MM-DD_rails.md  # S1 evidence
│       └── YYYY-MM-DD-discrepancies.md       # may exist from S1 — APPEND, else copy template
└── spec/SPEC.md                   # source of truth — NEVER edited (Mode A)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
docs/bench/
├── README.md                              # MODIFIED: rows B and C finalized (only changes)
├── template-session-B-flow1-abort.md      # MODIFIED: filled in place = session record
├── template-session-C-flow2-success.md    # MODIFIED: filled in place = session record
├── YYYY-MM-DD_session-B-flow1-abort.log   # NEW: raw Flow-1 capture + operator header
├── YYYY-MM-DD_session-C-flow2-success.log # NEW: raw Flow-2 capture + operator header
└── YYYY-MM-DD-discrepancies.md            # NEW only if the bench day lacks one; else appended
# (YYYY-MM-DD = each session's real execution date; may differ between B and C)
```

### Known Gotchas of our codebase & Library Quirks

```bash
# CRITICAL — power exclusivity (hardware, binding): RP2040-Zero 5V pin (VSYS) ties
#   DIRECTLY to USB VBUS with NO diode. Never USB + buck simultaneously. Bench/data
#   sessions = USB-powered, buck 5 V disconnected from the board. 12 V may feed the
#   MOSFET module's power input ALONE (so the valve can click).

# CRITICAL — Flow-1 restart assertions (THE point of Session B):
#   restart line MUST NOT be `recovery=0ms` (that means history was lost = FAIL), and
#   its budget MUST come from the §6.2 table. Wait ≤ 2 min after the abort → first-knot
#   clamp → `backstop=44min` + `VALVE open budget=44min`. A restart showing 30min /
#   recovery=0ms after an abort is a DISCREPANCY (fresh-boot default leaking).

# CRITICAL — recmin semantics (BUG-001 lesson): recmin = recovery_ms / 60000,
#   integer-truncated. recovery=8200ms → recmin=0. With the suggested ≈1-min wait the
#   printed recmin is 0 OR 1 — both correct. Never "fix" the evidence; only
#   recovery=0ms or backstop=30min on a restart are failures.

# CRITICAL — wait-row table (SPEC §6.2, piecewise-linear between knots, clamped at
#   both ends): ≤2 min → 44; 10 min → 41; 20 min → 38; 35 min → 34; ≥60 min → 30.
#   Between-knot waits interpolate then floor (e.g. 15 min → 39). Prefer a ~30–90 s
#   wait (inside the clamp) so the expected value is unambiguous 44. RECORD the wait.

# CRITICAL — Flow-2 guard boundaries (violations are FAULTS, Session D territory —
#   retake, never tick): first dip only after ≥ 60 s of valve-open time
#   (TRIP_MIN_ELAPSED_S=60, inclusive at 60 s); keep BOTH sensors dry for the first
#   5 s (EARLY_WET_WINDOW_S=5); sensor 2 must trip within 12 s of sensor 1's trip
#   (CONFIRM_WINDOW_S=12). Early-trip lines look like `FAULT earlytrip sensor=N …`.

# CRITICAL — long press during FILLING = +5 min to the backstop (§5.1), NOT abort.
#   If you mis-hold and see `BTN long`, the fill continues — just short-press again.
#   All abort/ack/start presses here are SHORT: press and release cleanly; the event
#   fires on RELEASE (silence while held).

# CRITICAL — trip debounce (config.h DEBOUNCE_COUNT=8 readings @ 100 ms each): a dip
#   registers only after ~0.8 s submerged. HOLD each dip ~1–2 s at mount depth. Within
#   the 12-s confirm window, dip sensor 2 promptly (within ~4 s of sensor 1's TRIP) so
#   its debounce completes in time.

# CRITICAL — 60+s dry valve open in Session C is INHERENT to Flow 2 (the item
#   contract: "keep opens <5 s where the flow requires aborting" — Flow 2 doesn't
#   abort). The solenoid may warm; run it once, do not repeat needlessly. The
#   post-ACK extra fill and every Session-B fill ARE aborts — keep those opens <5 s.

# CRITICAL — valve-closed-first on EVERY exit (§7.9): `VALVE close` precedes `ABORT`,
#   `TRIP`, and `FAULT` lines. If a close line is missing, that's a discrepancy.

# GOTCHA — ambient interleaving: `S1=<n> S2=<n>` (1 Hz) and `BTN …` lines appear
#   between event lines. Ordered-prefix assertions apply to EVENT lines only; the 1-Hz
#   stream is your clock — count ~60–65 S1= lines after `VALVE open` before dipping
#   sensor 1, and ~60 lines for Session B's ≈1-min restart wait.

# GOTCHA — capture BEFORE power/reset in BOTH sessions: banner must be the first
#   stream line. If already powered, press RESET (not re-plug); RESET lands
#   POWER_OR_RESET and clears recovery history → first fill prints 30/0/0 in both
#   sessions. Never hold BOOT while (re)plugging (BOOTSEL during RESET = bootloader).

# GOTCHA — LED observation nuance (D-1, pre-seeded): SPEC §4 names the onboard WS2812
#   (GP16) the sole indicator; COMMISSIONING describes an external GP14 green LED
#   mirroring patterns. Record which indicator(s) actually render the solid-blue fill
#   and the green DONE triple-pulse — as a D-1 observation, never a spec edit.

# GOTCHA — fill discipline (T1.S2 contract): session B/C templates are filled IN
#   PLACE; the discrepancies template is COPIED to a date-stamped file once per bench
#   day then APPENDED. Do not fill templates D/E; do not touch S1's artifacts.

# BLOCKED protocol: if the rig is unavailable (or row A is not PASS), set rows B and C
#   to `BLOCKED: <real reason>` and create NO log files. Never synthesize captures —
#   the integrity rule (README §1) is the whole point of this changeset.
```

## Implementation Blueprint

### Data models and structure

No code. Evidence artifacts only, following the T1.S2 template structures exactly (session record tables, `— [ ]`/`— [x]` checklists, blank `____` fields filled with observed values, header block as the logs' first lines).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITIONS (software, before touching hardware)
  - RUN: ls docs/bench/README.md docs/bench/template-session-B-flow1-abort.md \
         docs/bench/template-session-C-flow2-success.md docs/bench/template-discrepancies.md
  - EXPECT: all exist (T1.S1 + T1.S2 landed). Missing → STOP and report.
  - RUN: grep -E '^\| A \|' docs/bench/README.md
  - EXPECT: row A ends in PASS. If PARTIAL/BLOCKED/PENDING → the rig is NOT proven;
    take the Task-4 BLOCKED branch (do not run B/C on an unproven rig without a human
    decision — record it as the reason).
  - RUN: pio run -e pico        # compile-only [SUCCESS]; NO reflash, NO upload.
  - RUN: git rev-parse --short HEAD   # FW rev for both headers and index rows.

Task 1: SESSION B — Flow 1: abort + recomputed restart (USB-only; sensors dry)
  - CONFIGURE: buck 5 V disconnected from board; USB connected; sensors DRY in air
    (a wet sensor at start = Session D's baseline fault, not this flow); optionally
    12 V to the MOSFET module power input ALONE; cup of water NOT needed yet.
  - START CAPTURE BEFORE RESET: 115200 8N1 save-to-file, e.g.
    pio device monitor -b 115200 | tee docs/bench/$(date +%F)_session-B-flow1-abort.log
    (or terminal raw-save). Press RESET after the capture is running.
  - HEADER BLOCK (mandatory first lines; fill rev/date/deviations):
    # bench evidence — session B — flow 1 abort + recomputed restart
    - date: YYYY-MM-DD
    - firmware: git <rev> (board build of <date>; native suite passing)
    - wiring: USB-powered, buck 5 V disconnected; sensors dry in air; valve dry
      (12 V on MOSFET module only); rig specifics
    - deviations: none | list
  - STEP 1 (fresh boot, first fill): verify first stream line
    `JugFiller boot (reset cause: POWER_OR_RESET)`. Short press → on release expect
    `BTN short <n>ms` (n < 1000) then `FILL start backstop=30min recovery=0ms
    recmin=0` → `BASE ok s1=__ s2=__` → `VALVE open budget=30min`. LED solid blue.
  - STEP 2 (mid-fill abort, keep open < 5 s): short press again within ~5 s →
    `BTN short <n>ms` → `VALVE close` → `ABORT attemptms=__ openms=__`. LED off;
    valve click audible if 12 V connected.
  - STEP 3 (wait + restart): wait a recorded interval — ~30–90 s (template suggests
    ≈1 min; any wait ≤2 min keeps the expected budget unambiguous at 44). Count
    ambient S1= lines as the clock; write the wait used in the template's
    "Wait used: ____" field. Short press → expect
    `FILL start backstop=44min recovery=<wait-ish, NON-ZERO>ms recmin=<0|1>` →
    `BASE ok` → `VALVE open budget=44min`. LED solid blue.
    CRITICAL CHECKS: recovery != 0ms; backstop AND budget = 44min (not 30). If the
    restart printed 30min/recovery=0ms → still complete the session, but tick the
    item failed and file a discrepancy entry (history-loss suspicion).
  - STEP 4 (close the valve): abort the restart fill with a short press within <5 s
    of `VALVE open` → `VALVE close` → `ABORT …`. (This second ABORT is expected —
    it is not on the checklist but must be in the log; note it in deviations: none.)
  - STOP capture cleanly (Ctrl-C / end save). Never edit captured lines.
  - FILL template-session-B-flow1-abort.md IN PLACE: session record (date, evidence
    log, FW rev, checklist pass/total), observed values into every `____`, waits.

Task 2: SESSION C — Flow 2: successful fill through DONE/ACK (USB-only; wet dips)
  - CONFIGURE: same power setup; sensors dry at start; cup of water at hand;
    stopwatch optional (the 1-Hz S1= stream is the clock).
  - START a NEW capture BEFORE a fresh RESET (separate file):
    … | tee docs/bench/$(date +%F)_session-C-flow2-success.log
    HEADER BLOCK: "# bench evidence — session C — flow 2 successful fill …" (same
    field set; wiring notes the hand-dip cup).
  - STEP 1 (fill start): RESET → banner; short press → `FILL start backstop=30min
    recovery=0ms recmin=0` → `BASE ok` → `VALVE open budget=30min`. LED solid blue.
  - STEP 2 (≥60 s open — early-trip guard): keep BOTH sensors dry and out of the
    cup; count ~60–65 ambient `S1= …` lines after `VALVE open` (≈60–65 s) before
    touching water.
  - STEP 3 (first dip): dip sensor 1 to mount depth; HOLD ~1–2 s (8-reading debounce
    at 100 ms/reading) → expect `VALVE close` → `TRIP sensor=1 fillms=<≥60000>`.
    Valve closes on the trip; LED stays solid blue through CONFIRM (§4). Record
    fillms in the template. If a `FAULT earlytrip/earlywet …` line appears instead:
    the attempt is void — RESET and retake from STEP 1 (record the retake in
    deviations; do not tick the box).
  - STEP 4 (confirm window): within ~4 s of the TRIP (leaving margin for the
    debounce inside the 12-s window), dip sensor 2; HOLD ~1–2 s → expect
    `CONFIRM pass sensor=2 durms=<≤~12000>` → `DONE attemptms=__ openms=<≥60000>`.
    CONFIRM line MUST precede DONE. Record durms.
  - STEP 5 (DONE indication): observe the green triple-pulse — 3 × (on 200 ms /
    off 200 ms) then off 1000 ms, repeating — and note WHICH indicator(s) rendered
    it (onboard WS2812 and/or external GP14 LED) → D-1 observation.
  - STEP 6 (acknowledge): short press → `BTN short <n>ms` → `ACK done`. Blinking
    stops; LED off (IDLE).
  - STEP 7 (Flow-2 press 3 — cycle closes): short press again → a NEW
    `FILL start backstop=44min recovery=<small>ms recmin=<0|1>` (quick press after
    ACK → recovery < 2 min → 44; a longer pause follows the wait-row table) →
    `BASE ok` → `VALVE open budget=44min`. Then abort it within <5 s (`VALVE close`
    → `ABORT …`) to close the valve.
  - STOP capture cleanly. FILL template-session-C-flow2-success.md IN PLACE
    (record fillms/durms/attemptms/openms observed; retakes noted).

Task 3: FINALIZE evidence (software)
  - EDIT docs/bench/README.md rows B and C ONLY: real dates, evidence filenames,
    FW git rev, `pass/total` per template, Status ∈ {PASS, PARTIAL(n✗),
    BLOCKED: reason} — SKIPPED is NOT allowed for B/C (mandatory sessions).
  - DISCREPANCIES: for every expectation mismatch (including a confirmed D-1 LED
    observation), ensure docs/bench/$(date +%F)-discrepancies.md exists (copy
    template-discrepancies.md ONLY if this bench day has no file yet; S1 may have
    created it — then APPEND). Quote log lines VERBATIM in fenced blocks.
  - RUN: Level 4 verification gates below; fix bookkeeping, NEVER the captures.

Task 4: BLOCKED branch (only if the rig is unavailable or row A ≠ PASS)
  - SKIP Tasks 1–3 (no evidence files created).
  - EDIT rows B and C Status → `BLOCKED: <real reason>` (e.g. "rig unavailable" or
    "row A not PASS — rig unproven"). Leave other cells `—`.
  - REPORT: state the reason plainly; do not pre-create empty files.
```

### Implementation Patterns & Key Details

```bash
# Expected event-line sequences (ambient `S1=… S2=…` + `BTN …` interleaved; only the
# EVENT ordering is asserted — filter with `grep -v '^S1='` when eyeballing).

# SESSION B (event skeleton):
#   JugFiller boot (reset cause: POWER_OR_RESET)
#   [BTN short <n>ms]  FILL start backstop=30min recovery=0ms recmin=0
#   BASE ok s1=<n> s2=<n>
#   VALVE open budget=30min
#   [BTN short <n>ms]  VALVE close
#   ABORT attemptms=<n> openms=<n>
#   … ~30–90 s of ambient lines (the recorded wait) …
#   [BTN short <n>ms]  FILL start backstop=44min recovery=<non-zero>ms recmin=<0|1>
#   BASE ok s1=<n> s2=<n>
#   VALVE open budget=44min
#   [BTN short <n>ms]  VALVE close
#   ABORT attemptms=<n> openms=<n>

# SESSION C (event skeleton):
#   JugFiller boot (reset cause: POWER_OR_RESET)
#   [BTN short <n>ms]  FILL start backstop=30min recovery=0ms recmin=0
#   BASE ok s1=<n> s2=<n>
#   VALVE open budget=30min
#   … ≥60 ambient lines (~60–65 s, sensors dry) …
#   VALVE close                       ← valve-closed-first
#   TRIP sensor=1 fillms=<≥60000>
#   … ≤ a few seconds …
#   CONFIRM pass sensor=2 durms=<≤~12000>
#   DONE attemptms=<n> openms=<≥60000>
#   … green triple-pulse observed …
#   [BTN short <n>ms]  ACK done
#   [BTN short <n>ms]  FILL start backstop=44min recovery=<small>ms recmin=<0|1>
#   BASE ok … / VALVE open budget=44min
#   [BTN short <n>ms]  VALVE close / ABORT attemptms=<n> openms=<n>

# Wait→expected-budget table (SPEC §6.2; interpolation floors between knots):
#   wait ≤2 min → 44 | 10 → 41 | 20 → 38 | 35 → 34 | ≥60 → 30
```

### Integration Points

```yaml
DOCS (the only integration surface):
  - docs/bench/README.md: rows B and C finalized; status vocabulary CLOSED (§2);
    SKIPPED not permitted for B/C (optional items only, per §2).
  - template-session-B/C-*.md: become the durable session records.
  - YYYY-MM-DD-discrepancies.md: appended; D-1 gains its bench confirmation.
  - Upstream contract: consumed from P1.M2.T2.S1 — row A = PASS, flashed firmware,
    rails evidence; this task performs NO reflash and NO firmware edits.
  - Downstream contract: P1.M2.T2.S3 (fault paths) reuses the hand-dip technique
    and the ≥60-s/12-s timing discipline proven here; P1.M2.T2.S5 consolidates.
FIRMWARE: none — verify only (pio run -e pico).
SPEC: spec/SPEC.md is the source of truth and is NOT edited (Mode A).
CONFIG/BUILD: none.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Evidence-file "syntax": names, headers, index rows well-formed
ls docs/bench/$(date +%F)_session-B-flow1-abort.log docs/bench/$(date +%F)_session-C-flow2-success.log
head -8 docs/bench/$(date +%F)_session-B-flow1-abort.log    # header block, rev filled
head -8 docs/bench/$(date +%F)_session-C-flow2-success.log  # header block, rev filled
grep -c '^| B |' docs/bench/README.md   # exactly 1
grep -c '^| C |' docs/bench/README.md   # exactly 1
# Expected: files exist (or BLOCKED branch taken), headers complete, one row each.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio run -e pico          # compile-only [SUCCESS] — firmware tree untouched
pio test -e native       # optional sanity: all projects still [PASSED]
# Expected: no regressions (this task changes docs only).
```

### Level 3: Integration Testing (System Validation)

```bash
git status --short
# Expected exactly:
#  M docs/bench/README.md
#  M docs/bench/template-session-B-flow1-abort.md
#  M docs/bench/template-session-C-flow2-success.md
# ?? docs/bench/<date>_session-B-flow1-abort.log
# ?? docs/bench/<date>_session-C-flow2-success.log
# (?? docs/bench/<date>-discrepancies.md only if newly created this task)
# Nothing under firmware/, test/, spec/, platformio.ini, README.md, COMMISSIONING.md,
# template-session-A/D/E, template-rails, or S1's log/rails evidence.
```

### Level 4: Creative & Domain-Specific Validation

```bash
B=docs/bench/$(date +%F)_session-B-flow1-abort.log
C=docs/bench/$(date +%F)_session-C-flow2-success.log

# (B1) banner once, first stream line, correct cause, in each log:
grep -c '^JugFiller boot (reset cause: POWER_OR_RESET)$' "$B"   # 1
grep -c '^JugFiller boot (reset cause: POWER_OR_RESET)$' "$C"   # 1

# (B2) exactly two fills; first fresh-boot form; restart table-derived & NON-zero:
grep -c '^FILL start' "$B"                                       # 2
grep '^FILL start' "$B" | head -1 | grep -q 'backstop=30min recovery=0ms recmin=0' && echo "FILL1 OK"
grep '^FILL start' "$B" | tail -1 | grep -q 'recovery=0ms' && echo "FAIL: restart lost history" \
  || echo "RESTART recovery NON-ZERO OK"
grep '^FILL start' "$B" | tail -1 | grep -q 'backstop=44min' && echo "RESTART table budget OK"
grep '^VALVE open budget=' "$B"                                  # 30min then 44min, in order

# (B3) abort chain ordering + all abort presses are SHORT:
grep -nE 'VALVE close|ABORT attemptms=|BTN ' "$B" | grep -v '^S1=' \
  # eyeball: each VALVE close (line N) precedes its ABORT (line N+k); every BTN line
  # adjacent to an abort reads `BTN short <n>ms` with n < 1000
grep '^BTN short' "$B" | sed 's/[^0-9]//g' | awk '$0+0>=1000{exit 1}' && echo "SHORT-VALUES OK"

# (C1) Flow-2 core events present exactly once, in order, with guard-bounded values:
grep -c '^TRIP sensor=1 ' "$C"                                   # 1
grep -c '^CONFIRM pass sensor=2 ' "$C"                           # 1
grep -c '^DONE attemptms=' "$C"                                  # 1
grep -c '^ACK done$' "$C"                                        # 1
grep -nE '^VALVE close$|^TRIP sensor=1 |^CONFIRM pass sensor=2 |^DONE attemptms=|^ACK done$|^FILL start' "$C" \
  # eyeball ascending line numbers: VALVE close < TRIP < CONFIRM < DONE < ACK done < FILL start(post-ACK)
grep '^TRIP sensor=1 ' "$C" | sed 's/.*fillms=\([0-9]*\)ms.*/\1/' | awk '$1>=60000{print "TRIP>=60s OK"; exit}'
grep '^CONFIRM pass sensor=2 ' "$C" | sed 's/.*durms=\([0-9]*\)ms.*/\1/' | awk '$1<=12500{print "CONFIRM<=12s OK"; exit}'
grep '^DONE attemptms=' "$C" | sed 's/.*openms=\([0-9]*\)ms.*/\1/' | awk '$1>=60000{print "OPEN>=60s OK"; exit}'

# (C2) no fault lines leaked into a "success" session (a fault means retake):
grep -c '^FAULT ' "$C"                                           # 0

# (C3) post-ACK fill exists and is table-derived for a quick press:
grep -n '^ACK done$' "$C" | cut -d: -f1                          # line X
awk -v x="$(grep -n '^ACK done$' "$C" | cut -d: -f1)" 'NR>x && /^FILL start/' "$C" | head -1 \
  # shows backstop=44min (quick press) — or matches the recorded wait-row

# (4) templates filled in place + index rows finalized:
grep -E '^\| B \|' docs/bench/README.md | grep -vE 'PENDING|—'   # filled row B
grep -E '^\| C \|' docs/bench/README.md | grep -vE 'PENDING|—'   # filled row C
grep -c '\[x\]' docs/bench/template-session-B-flow1-abort.md     # >0 (checklist ticked)
grep -c '\[x\]' docs/bench/template-session-C-flow2-success.md   # >0 (checklist ticked)

# (5) integrity: genuine captures or consistent BLOCKED — if rows B/C are BLOCKED,
#     NO session-B/C log files exist:
ls docs/bench/*_session-B-flow1-abort.log docs/bench/*_session-C-flow2-success.log 2>/dev/null \
  || echo "BLOCKED-consistency: no fabricated files OK"
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (or BLOCKED branch consistent: rows marked, no files, no fabrication)
- [ ] `pio run -e pico` `[SUCCESS]`; firmware/test trees untouched (`git status` scope exact)

### Feature Validation

- [ ] Session B: fresh-boot trio (30-min/0ms/recmin=0 → BASE → open budget=30min); mid-fill short-press abort (`VALVE close` → `ABORT`); recorded wait; restart shows `recovery != 0ms` AND `backstop=44min`/`budget=44min`; restart fill aborted <5 s
- [ ] Session C: ≥60 s open before first dip; `VALVE close` → `TRIP sensor=1 fillms≥60000`; `CONFIRM pass sensor=2 durms≤~12000` before `DONE attemptms= openms≥60000`; green triple-pulse observed (D-1 indicators recorded); `ACK done` stops blinking; post-ACK press starts a new fill
- [ ] Every checklist item ticked with observed values; both session records complete (date, log, rev, pass/total)
- [ ] Index rows B and C finalized; discrepancies ledger carries every mismatch verbatim

### Code Quality Validation

- [ ] Fill discipline honored: B/C templates in place; discrepancies file copied-once-then-appended; templates A/D/E, rails sheet, and S1 evidence untouched
- [ ] Header blocks complete (date, git rev, wiring incl. hand-dip notes, deviations) as each log's first lines
- [ ] No edits to spec/SPEC.md, README.md (root), COMMISSIONING.md, firmware/, test/

### Documentation & Deployment

- [ ] Evidence filenames date-stamped per README §4 conventions; D-1 confirmed with bench observation
- [ ] Rows B/C legible to P1.M2.T2.S3 (technique precedent) and P1.M2.T2.S5 (consolidation)

---

## Anti-Patterns to Avoid

- ❌ Never connect USB and the buck's 5 V simultaneously — VSYS ties to USB VBUS with no diode
- ❌ Never let an ABORTING fill (Session B fills, post-ACK fill) stay open ≥5 s — the solenoid overheats dry; only Flow 2's inherent ≥60-s open is excepted, once
- ❌ Never dip sensor 1 before ~60 ambient lines have passed since `VALVE open` — that's an early-trip FAULT (Session D territory); retake, don't tick
- ❌ Never hold a press ≥1 s when you mean to abort/start/ack — long press during FILLING adds +5 min instead of aborting; all presses here are short, firing on release
- ❌ Never mark a restart line `recovery=0ms` or `backstop=30min` as a pass — that is THE Session B failure signature (history loss / table bypass)
- ❌ Never "fix" a printed recmin of 1 with a ~1-min wait — recmin truncates (8200 ms → 0); 0 and 1 are both correct there
- ❌ Never edit, trim, or retype captured lines; never synthesize a capture — genuine evidence or BLOCKED, nothing else
- ❌ Never tick a faulted Session C attempt — RESET and retake; record the retake in discrepancies
- ❌ Never edit spec/SPEC.md to match observations — discrepancies go to the ledger (Mode A)
- ❌ Never reflash firmware or "fix" firmware/test if a gate fails — record the mismatch; fixes are a human decision
- ❌ Never use SKIPPED for rows B/C — mandatory sessions; only BLOCKED with a real reason
- ❌ Never fill templates A/D/E or duplicate S1's discrepancy D-1 — append to the day's file instead

---

**Confidence Score: 9/10** — every input contract was read from disk this session (bench README index/procedure/status vocabulary, both session templates with their critical assertions and grep hints, the binding evidence-package power rules and byte-exact Session B/C prefixes, SPEC §6.1/6.2 recovery/backstop semantics, and the firmware's exact format strings at jug_filler.ino:1343–1395 plus the config tunables bounding the timing technique). The PRP carries the complete runbook including the two CRITICAL Flow-1 assertions, the recmin truncation nuance (BUG-001 lesson), the debounce/dip timing derived from DEBOUNCE_COUNT×READING_INTERVAL_MS, the retake-on-fault rule, the BLOCKED integrity branch, and machine-checkable post-capture gates. The single inherent risk is physical rig availability, which the task's own contract handles via BLOCKED — not a PRP defect.
