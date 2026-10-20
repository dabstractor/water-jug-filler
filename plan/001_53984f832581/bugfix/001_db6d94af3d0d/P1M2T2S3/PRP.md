# PRP — P1.M2.T2.S3 (bugfix changeset): Session D fault paths — baseline fault, no-confirm fault, optional guard faults, with blink-code verification

## Goal

**Feature Goal**: Execute the Session D fault-path bench session on the rig proven by P1.M2.T2.S1 and exercised by P1.M2.T2.S2 — **D1 (REQUIRED)**: wet-sensor start proving the §7.1 baseline plausibility gate refuses to open the valve (`VALVE close` → `FAULT baseline sensor=1 code=1 count=` with the CRITICAL assertion that NO `VALVE open` line precedes the fault), plus BOOT-ignored-while-latched and RESET-clears-to-fresh-banner; **D2 (REQUIRED)**: the §7.5 no-confirm path (trip sensor 1 after ≥60 s open, never dip sensor 2 → 12 s → `FAULT confirm sensor=2 code=2 durms=`); **D3/D4 (OPTIONAL)**: early-wet/early-trip guard faults and backstop expiry as time permits (recorded skip with reason is acceptable) — all with red blink-group verification (N × 250 ms on / 250 ms off, 2000 ms pause, N = fault code), latched-until-RESET semantics, USB-only power, dry valve, and a genuine date-stamped capture.

**Deliverable**:
- `docs/bench/YYYY-MM-DD_session-D-fault-paths.log` — raw 115200 8N1 capture (operator header block as first lines) containing the D1, D2 (and any D3/D4) segments separated by RESET banners.
- `docs/bench/template-session-D-fault-paths.md` — filled IN PLACE (session record + ticked checklists with observed `count=`/`fillms=`/`durms=` values, blink-cadence observations, skip reasons for D3/D4 if not run).
- `docs/bench/README.md` — index row D finalized (real date, evidence filename, FW git rev, `pass/total`, status from the closed vocabulary).
- `docs/bench/YYYY-MM-DD-discrepancies.md` — appended (or created from `template-discrepancies.md` if this bench day has none yet) for every observed-vs-expected mismatch, including D-1 LED-indicator observations for the red fault blinks.

**Success Definition**: The log is a genuine capture whose D1 segment shows a `FAULT baseline` line with no preceding `VALVE open`; every required checklist item (D1, D2) ticked with observed values and blink groups; optional items either run green or carry a skip reason; RESET between latched faults lands fresh boot banners — OR index row D reads `BLOCKED: <reason>` with NO log file created and template left unfilled (integrity rule: never fabricate). `pio run -e pico` still `[SUCCESS]`; `git status` shows only the additions/edits listed above and nothing belonging to the parallel S2 session (rows B/C, templates B/C, their logs).

## User Persona

**Target User**: The bench operator (human, or an agent with physical rig access) executing the BUG-003 evidence plan; downstream, P1.M2.T2.S5 (consolidation reads row D and the discrepancy ledger).

**Use Case**: With the rig operational and Flows 1/2 evidenced (S2), run the fault-path session per the T1.S2 template on USB-only power: hand-dip a sensor BEFORE starting a fill (baseline gate), withhold the second dip (no-confirm), and optionally dip inside the 5 s / before-60 s guard windows — capturing one stream and ticking each expected prefix, blink cadence, and the no-valve-open assertion.

**User Journey**: Start capture → RESET → hold sensor 1 submerged → short press → observe `VALVE close` → `FAULT baseline sensor=1 code=1` + red 1-blink groups (latched) → press BOOT (logs `BTN short`, NO `FILL start`) → RESET (banner, LED off) → optionally repeat with sensor 2 → RESET → short press → keep valve open ≥60 s (count ~60–65 `S1=` lines) → dip sensor 1 → `TRIP sensor=1` → hold sensor 2 dry for 12 s → `FAULT confirm sensor=2 code=2 durms=` + red 2-blink groups → (optionally D3/D4 with RESET between each) → stop capture → fill template D → finalize row D → file any discrepancies.

**Pain Points Addressed**: BUG-003 — PRD §9 step 4 fault paths (fault blink codes and latching behavior) have no physical evidence; this session supplies it, completing the fault half of the bench sign-off that S2's flow half started.

## Why

- Bug-fix PRD Issue 3 (BUG-003, Minor): "physical bench dry-run — flows 1 and 2 with hand-dipped sensors, **fault-path blink codes** … captured serial logs as evidence under docs/bench/" — the fault-path share of that deliverable.
- Bug-fix PRD Recommendations: "Execute the deferred physical bench validation … on USB-only power and capture the serial logs under docs/bench/."
- Item contract (P1.M2.T2.S3): bench_evidence_package.md §2 Session D is the binding checklist this runbook implements; backstop expiry is explicitly "only if time permits — a recorded skip with reason is acceptable".
- Downstream: P1.M2.T2.S5 consolidates rows A–E and the discrepancy ledger; the D-1 LED-indicator question gains its fault-cadence observation here.

## What

One physical session (single capture, multiple RESET-separated sub-scenarios) plus evidence finalization. **Power exclusivity is binding** (SPEC §2.2; the RP2040-Zero VSYS pin ties directly to USB VBUS with no diode): USB-powered only, buck 5 V disconnected from the board; 12 V may feed the MOSFET module's power input ALONE (optional — enables valve-click audibility on `VALVE close`; for D4's long open it MUST be disconnected per the template's safety hint).

1. **Preconditions (software)**: template D and the bench README exist; index rows for A (and B/C from the parallel S2 session) re-read live at execution; `pio run -e pico` compiles clean; record `git rev-parse --short HEAD` for the header and index row.
2. **D1 — baseline fault (REQUIRED)**: capture running, RESET, hold sensor 1 submerged at mount depth, short press → expect `FILL start backstop=30min recovery=0ms recmin=0` → (baseline capture runs; NO `BASE ok`, NO `VALVE open`) → `VALVE close` → `FAULT baseline sensor=1 code=1 count=<observed>`. Observe red **1-blink groups** (on 250 ms / off 250 ms ×1, then off 2000 ms, repeating), latched. Press BOOT while faulted → `BTN short <n>ms` logs but NO `FILL start` follows. RESET → fresh banner `JugFiller boot (reset cause: POWER_OR_RESET)`, LED off. Optionally repeat the whole sub-scenario with sensor 2 submerged → `FAULT baseline sensor=2 code=2 count=` + red 2-blink groups.
3. **D2 — no-confirm fault (REQUIRED)**: RESET → short press → `FILL start …backstop=30min recovery=0ms recmin=0` → `BASE ok s1= s2=` → `VALVE open budget=30min` → keep BOTH sensors dry ~60–65 s (count ambient `S1=` lines) → dip sensor 1, hold ~1–2 s → `VALVE close` → `TRIP sensor=1 fillms=<≥60000>` → keep sensor 2 dry and wait out the 12 s confirm window → `FAULT confirm sensor=2 code=2 durms=<≈12000>`. Observe red **2-blink groups**, latched.
4. **D3 — guard faults (OPTIONAL; skip with reason if time is short)**: after RESET and a fresh fill start, (a) dip sensor 1 immediately (~1–2 s after `VALVE open`, so the 0.8 s debounce completes inside the 5 s window) → `VALVE close` → `FAULT earlywet sensor=1 code=1 fillms=<5000` with red 1-blink groups; (b) RESET, fresh fill, dip sensor 1 at ~20–30 s of open time (clear of the 5 s window, well under 60 s) → `FAULT earlytrip sensor=1 code=1 fillms=<between 5000 and 60000>` with red 1-blink groups. Either or both may be skipped; write the reason in the template's skip-reason field.
5. **D4 — backstop expiry (OPTIONAL; expected skip)**: needs a full fresh-boot budget of open valve time (~30 min wall clock). If run: disconnect 12 V from the MOSFET module (solenoid stays de-energized during the long open — the template's safety hint, permitted by SPEC §9.1/§9.2; the log lifecycle is unchanged), RESET, start a fill, leave both sensors dry, wait out the budget → `VALVE close` → `FAULT backstop code=3 budget=30min openms=<≈1800000>ms` with red 3-blink groups, latched. A recorded skip with reason is acceptable per the item contract.
6. **Finalize**: fill template D in place (observed values into every `____`, blink-cadence notes, skip reasons); update index row D only; append/create the date-stamped discrepancies file for any mismatch (record, never edit evidence); verify git scope.

NOT in scope: Sessions A/B/C (A is done in T2.S1; B/C belong to the parallel T2.S2 session — do not touch their rows, templates, or logs); the watchdog check (T2.S4); consolidation beyond row D (T2.S5); README/COMMISSIONING sweeps (T2.S3.x); ANY firmware change or reflash; editing spec/SPEC.md (source of truth, Mode A).

### Success Criteria

- [ ] Task 0 entry gate evaluated against the LIVE index: if row A (or the S2 rows B/C) is not PASS, the BLOCKED branch was taken — row D `BLOCKED: <reason>`, no log file, template unfilled
- [ ] `docs/bench/YYYY-MM-DD_session-D-fault-paths.log` genuine capture, header block first lines, RESET banners between sub-scenarios
- [ ] D1 segment: `VALVE close` → `FAULT baseline sensor=1 code=1 count=<n>`; **zero `VALVE open` lines between the segment's `FILL start` and the fault**; BOOT press while faulted logs `BTN short` with no following `FILL start`; RESET lands a fresh banner
- [ ] D2 segment: `VALVE open` → (≥60 s) → `VALVE close` → `TRIP sensor=1 fillms=<≥60000>` → (12 s, sensor 2 dry) → `FAULT confirm sensor=2 code=2 durms=<≈12000>`
- [ ] D3/D4 either run with correct `FAULT earlywet/earlytrip/backstop` lines and N-blink groups, or carry written skip reasons (skips are NOT failures for row D)
- [ ] Blink-code verification recorded per fault: red N-blink groups (250/250 ms, 2000 ms pause), N = code; latched until RESET; D-1 indicator observation (onboard WS2812 and/or external GP14) noted
- [ ] Template D filled in place; index row D finalized; discrepancies ledger carries every mismatch verbatim
- [ ] `pio run -e pico` `[SUCCESS]`; `git status` scope = row D edit + template D fill + the one new log (+ discrepancies file only if created/appended by this task); nothing under firmware/, test/, spec/, README.md root, COMMISSIONING.md, or S1/S2 artifacts

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the complete sub-scenario runbook with byte-exact expected log lines (copied from `jug_filler.ino` format strings and the T1.S2 template), the blink cadence constants (FAULT_BLINK_MS=250 / FAULT_PAUSE_MS=2000), the guard boundaries (5 s / 60 s / 12 s), the dip/debounce timing technique (8 readings × 100 ms), the wet-start / withheld-dip fault-induction methods, the latch/RESET/BOOT-ignored semantics, the header block, index row columns, the BLOCKED integrity branch (given row A is currently BLOCKED on this host, the runbook makes that path first-class), and post-capture machine-checkable gates. All contracts were read from disk this session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/bench/README.md
  why: THE index you update (row D only) + §1 integrity rule (genuine captures or
        BLOCKED, never fabricate) + §2 closed status vocabulary (SKIPPED is allowed
        ONLY for Session E and the backstop-expiry sub-item INSIDE Session D — never
        for row D itself) + §3 capture procedure (USB-only binding rule, 115200 8N1,
        capture BEFORE reset, operator header block, no .log edits ever) + §4 naming
        (one shared discrepancies file per bench day). TODAY row A reads
        `BLOCKED: rig unavailable — no RP2040-Zero serial device detected …` and rows
        B–E are PENDING — re-read live state at Task 0; do not touch rows A/B/C.

- file: docs/bench/template-session-D-fault-paths.md
  why: THE Session D checklist — fill IN PLACE. Carries the sub-scenario steps, the
        expected FAULT lines with blank `____` fields, the CRITICAL D1 assertion (no
        VALVE open before FAULT baseline), the RESET-before-each-sub-scenario rule,
        the skip-reason fields for D3/D4, and the operator grep hints.

- file: docs/bench/template-discrepancies.md
  why: copy to YYYY-MM-DD-discrepancies.md ONLY if no file exists for that bench day
        yet (S1/S2 may have created it — then APPEND). D-1 (WS2812 GP16 vs external
        GP14 LED) is pre-seeded — add the fault-blink (red) observation here.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S2/PRP.md
  why: CONTRACT for the parallel prior item. Assumes: rows B/C finalized by S2
        (PASS or BLOCKED per its own gate), no reflash, the shared bench-day
        discrepancies file may exist, hand-dip technique proven. Your entry gate
        mirrors its row-A gate; never touch S2's rows/templates/logs.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  section: "1. BINDING power-safety rules" + "2. Session D" + "4. Executability split"
  why: §1 the five binding power rules (never USB+buck simultaneously; 12 V to the
        MOSFET module alone OK; dry-fire opens <5 s; data sessions USB-powered).
        §2 the byte-exact Session D expectations incl. the no-VALVE-open critical
        assertion. §4 physical-vs-software split and never-fabricate rule.

- file: firmware/jug_filler/jug_filler.ino
  section: formatFillLogEvent (~lines 1343–1395)
  why: byte-exact format strings: `FAULT baseline sensor=%u code=%u count=%lu`
        (count = failing sensor's observed count), `FAULT confirm sensor=%u code=%u
        durms=%lu` (durms = confirm-window elapsed), `FAULT earlywet|earlytrip
        sensor=%u code=%u fillms=%lu`, `FAULT backstop code=3 budget=%umin
        openms=%lums`; sensor prints as e.sensor+1; plus `FILL start backstop=%umin
        recovery=%lums recmin=%u`, `BASE ok s1=%u s2=%u`, `VALVE open budget=%umin`,
        `VALVE close`, `TRIP sensor=%u fillms=%lu`, `BTN short <n>ms`. Read-only.

- file: firmware/jug_filler/config.h
  why: tunables bounding the runbook: CONFIRM_WINDOW_S=12, EARLY_WET_WINDOW_S=5,
        TRIP_MIN_ELAPSED_S=60, DEBOUNCE_COUNT=8 @ READING_INTERVAL_MS=100 (≈0.8 s
        wet-dwell), FAULT_BLINK_MS=250, FAULT_PAUSE_MS=2000,
        FRESH_BOOT_BACKSTOP_MIN=30, BUTTON_LONG_PRESS_MS=1000.

- file: spec/SPEC.md
  section: §7 (lifecycle: §7.1 baseline gate, §7.5 confirm window, §7.6 guards,
        §7.7 backstop, §7.9 valve-closed-first on every exit, fault codes 1/2/3
        latching) + §4 (LED contract: red N-blink groups) + §9 (bench flows)
  why: source of truth for WHY the gate refuses to open and why faults latch.
        NEVER edited (Mode A).

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: hardware reference for the 5V/VSYS-USB VBUS tie (no diode) behind the
        exclusivity rule, and the onboard BOOT button used as the §5.1 user button.

- url: https://arduino-pico.readthedocs.io/en/latest/bootsel.html
  why: BOOT-button notes — runtime presses never enter the bootloader (entry needs
        BOOTSEL during RESET); safe to press freely while faulted (D1's
        BOOT-ignored check), but never hold BOOT while (re)plugging power.
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
│       ├── README.md              # index — row A BLOCKED-or-PASS (S1's), rows B/C
│       │                          # finalized by parallel S2; EDIT row D only
│       ├── template-session-D-fault-paths.md  # T1.S2 — fill IN PLACE (this task)
│       ├── template-session-{A,B,C,E}-*.md    # owned by S1/S2/T1.S2 — do not touch
│       ├── template-rails.md / template-discrepancies.md
│       ├── YYYY-MM-DD_session-A…log + _rails.md   # S1 evidence — do not touch
│       ├── YYYY-MM-DD_session-{B,C}-*.log         # S2 evidence (if created) — don't touch
│       └── YYYY-MM-DD-discrepancies.md            # may exist from S1/S2 — APPEND
└── spec/SPEC.md                   # source of truth — NEVER edited (Mode A)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
docs/bench/
├── README.md                          # MODIFIED: row D finalized (only change)
├── template-session-D-fault-paths.md  # MODIFIED: filled in place = session record
├── YYYY-MM-DD_session-D-fault-paths.log # NEW: raw fault-path capture + operator header
└── YYYY-MM-DD-discrepancies.md        # NEW only if the bench day lacks one; else appended
# (YYYY-MM-DD = the session's real execution date)
```

### Known Gotchas of our codebase & Library Quirks

```bash
# CRITICAL — power exclusivity (hardware, binding): RP2040-Zero 5V pin (VSYS) ties
#   DIRECTLY to USB VBUS with NO diode. Never USB + buck simultaneously. Bench/data
#   sessions = USB-powered, buck 5 V disconnected from the board. 12 V may feed the
#   MOSFET module's power input ALONE (valve clicks on VALVE close); for D4's
#   ~30-min open it MUST be DISCONNECTED so the solenoid stays de-energized for the
#   long open (template safety hint; SPEC §9.1/§9.2 permit it, log lifecycle
#   unchanged — an energized dry solenoid for 30 min would overheat).

# CRITICAL — D1's core assertion (THE point of Session D): the baseline plausibility
#   gate (§7.1) REFUSES to open the valve when a sensor is wet at fill start. The D1
#   segment must show `VALVE close` → `FAULT baseline …` with ZERO `VALVE open` lines
#   since the segment's `FILL start`. A `VALVE open budget=…` before the fault = FAIL
#   (and a hardware-safety incident — the valve would fire dry on a wet sensor).

# CRITICAL — valve-closed-first on EVERY exit (§7.9): `VALVE close` precedes every
#   FAULT line, even when the valve never opened (D1/D3-earlywet log a close for an
#   un-opened valve — that is the contract, not a discrepancy).

# CRITICAL — faults LATCH (§7): cleared ONLY by RESET or power cycle. BOOT presses
#   while faulted are logged (`BTN short <n>ms`) but cannot start a fill. RESET
#   before EACH sub-scenario; every sub-scenario's first fill is fresh-boot
#   `FILL start backstop=30min recovery=0ms recmin=0` (reset clears tank history).

# CRITICAL — blink-code verification is HUMAN observation (the log cannot prove LED
#   behavior): red N-blink groups = N × (on 250 ms / off 250 ms) then off 2000 ms,
#   repeating; N = fault code (1 = sensor 1, 2 = sensor 2, 3 = timer). Record the
#   observed N and cadence in the template; mismatches go to discrepancies, never
#   to spec edits. D-1: note WHICH indicator renders red (WS2812 GP16 and/or GP14).

# CRITICAL — guard boundaries for D3 (violating them is the point here, not a
#   mistake): earlywet = wet reading within the first 5 s of fill
#   (EARLY_WET_WINDOW_S=5) → dip IMMEDIATELY (~1–2 s after VALVE open so the 0.8 s
#   debounce completes inside the window) → fillms < 5000. earlytrip = trip accepted
#   before 60 s of fill time (TRIP_MIN_ELAPSED_S=60) but after the 5 s window → dip
#   at ~20–30 s → fillms ∈ (5000, 60000). Do NOT dawdle near the boundaries.

# CRITICAL — trip debounce (DEBOUNCE_COUNT=8 readings @ 100 ms each): a dip
#   registers only after ~0.8 s submerged. HOLD each dip ~1–2 s at mount depth. In
#   D2, dip sensor 1 at ≥60 s; then keep sensor 2 dry the FULL 12 s
#   (CONFIRM_WINDOW_S=12) — durms counts from sensor 1's TRIP, expect ≈12000.

# CRITICAL — long press during FILLING = +5 min backstop (§5.1), NOT abort/ack. All
#   presses here are SHORT: press and release cleanly; the event fires on RELEASE
#   (silence while held is normal). A `BTN long` during D2's open window would log
#   `PLUS5 budget=35min` — harmless but note it in deviations if it happens.

# GOTCHA — ambient interleaving: `S1=<n> S2=<n>` (1 Hz) and `BTN …` lines appear
#   between event lines. Ordered-prefix assertions apply to EVENT lines only
#   (grep -v '^S1=' when eyeballing); the 1-Hz stream is your clock for the ≥60 s
#   D2 open (~60–65 lines) and the D3 earlytrip dip (~20–30 lines).

# GOTCHA — capture BEFORE power/reset; the banner must be the first stream line,
#   and RESET between sub-scenarios keeps the SAME capture running (new banner
#   lands in-stream). If the monitor drops on reset (USB re-enumeration), restart
#   the capture APPENDING to the same file and note it in deviations — never edit
#   prior lines. Never hold BOOT while (re)plugging power (BOOTSEL during RESET =
#   bootloader).

# GOTCHA — D1 logs `FILL start` before the fault (the press does start a fill
#   attempt; the gate then refuses). NO `BASE ok` appears in D1 (baseline failed).
#   Both are expected; the template only asserts VALVE close → FAULT baseline and
#   the no-VALVE-open rule.

# GOTCHA — fill discipline (T1.S2 contract): template D is filled IN PLACE; the
#   discrepancies file is COPIED once per bench day (if absent) then APPENDED. Do
#   not touch S1/S2 artifacts or templates A/B/C/E. Optional D3/D4 skips are
#   written in the template's skip-reason fields and do NOT count as failures —
#   but the whole row D may never use SKIPPED (only BLOCKED/PARTIAL/PASS).

# BLOCKED protocol (live reality: row A is currently `BLOCKED: rig unavailable —
#   no RP2040-Zero serial device detected on this host`, and S2's contract gates on
#   row A = PASS): if at Task 0 the rig is unavailable or rows A/B/C are not PASS,
#   set row D to `BLOCKED: <real reason>` and create NO log file; leave the template
#   unfilled. Never synthesize captures — the integrity rule (README §1) is the
#   whole point of this changeset.
```

## Implementation Blueprint

### Data models and structure

No code. Evidence artifacts only, following the T1.S2 template structure exactly (session record table, `— [ ]`/`— [x]` checklists, blank `____` fields filled with observed values, header block as the log's first lines).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITIONS + ENTRY GATE (software, before touching hardware)
  - RUN: ls docs/bench/README.md docs/bench/template-session-D-fault-paths.md \
         docs/bench/template-discrepancies.md
  - EXPECT: all exist (T1.S1 + T1.S2 landed). Missing → STOP and report.
  - RUN: grep -E '^\| [ABC] \|' docs/bench/README.md     # LIVE state, parallel S2 may have acted
  - GATE: row A must end in PASS (rig proven). If row A is BLOCKED/PARTIAL/PENDING,
    and rows B/C are likewise not PASS → take the Task-6 BLOCKED branch (record the
    exact reason read from the index, e.g. "rig unavailable — upstream session(s)
    not executed"). Running fault paths on an unproven rig requires a recorded human
    decision — put that decision in the BLOCKED/PARTIAL reason if it happens.
  - RUN: pio run -e pico        # compile-only [SUCCESS]; NO reflash, NO upload.
  - RUN: git rev-parse --short HEAD   # FW rev for the header and index row.

Task 1: START CAPTURE (physical)
  - CONFIGURE: buck 5 V disconnected from the board; USB connected; cup of water at
    hand; sensors dry in air; optionally 12 V to the MOSFET module power input ALONE
    (valve click on VALVE close); for D4 (if run) DISCONNECT that 12 V.
  - START CAPTURE BEFORE RESET: 115200 8N1 save-to-file, e.g.
    pio device monitor -b 115200 | tee docs/bench/$(date +%F)_session-D-fault-paths.log
    (or terminal raw-save). Press RESET after the capture is running; the banner
    `JugFiller boot (reset cause: POWER_OR_RESET)` must be the first stream line.
  - HEADER BLOCK (mandatory first lines; fill rev/date/deviations):
    # bench evidence — session D — fault paths (baseline, no-confirm, guards)
    - date: YYYY-MM-DD
    - firmware: git <rev> (board build of <date>; native suite passing)
    - wiring: USB-powered, buck 5 V disconnected; sensors dry in air at start; valve
      dry (12 V on MOSFET module only / disconnected for D4); cup for hand-dips
    - deviations: none | list

Task 2: D1 — baseline fault: wet sensor at start (REQUIRED, SPEC §7.1)
  - STEP 1: RESET (fresh banner). Hold sensor 1 SUBMERGED at mount depth BEFORE any
    press. Short press → expect `BTN short <n>ms` → `FILL start backstop=30min
    recovery=0ms recmin=0` → (baseline capture; NO `BASE ok`, NO `VALVE open`) →
    `VALVE close` → `FAULT baseline sensor=1 code=1 count=<observed>`.
    CRITICAL CHECK: zero `VALVE open` lines between this segment's FILL start and
    the FAULT. Record count= in the template.
  - STEP 2 (blink + latch): observe red 1-blink groups — on 250 ms / off 250 ms,
    then off 2000 ms, repeating — continuing until RESET (latched). Record which
    indicator rendered it (WS2812 / GP14 / both) → D-1.
  - STEP 3 (BOOT ignored while faulted): short press BOOT → expect `BTN short <n>ms`
    logged and NO `FILL start` following it (the fault state swallows the request).
  - STEP 4 (RESET clears): press RESET → expect banner
    `JugFiller boot (reset cause: POWER_OR_RESET)` and LED off (latch cleared).
  - STEP 5 (optional repeat, sensor 2): hold sensor 2 submerged, short press →
    `VALVE close` → `FAULT baseline sensor=2 code=2 count=<n>` + red 2-blink groups;
    RESET. Tick the optional box only if run.

Task 3: D2 — no-confirm fault (REQUIRED, SPEC §7.5)
  - STEP 1: RESET. Short press → `FILL start backstop=30min recovery=0ms recmin=0`
    → `BASE ok s1=____ s2=____` → `VALVE open budget=30min`.
  - STEP 2 (≥60 s open): keep BOTH sensors dry; count ~60–65 ambient `S1=` lines
    after `VALVE open` (the 1-Hz stream is the clock).
  - STEP 3 (trip sensor 1): dip sensor 1 to mount depth; HOLD ~1–2 s (0.8 s debounce)
    → expect `VALVE close` → `TRIP sensor=1 fillms=<≥60000>`. Record fillms.
  - STEP 4 (withhold sensor 2): keep sensor 2 DRY and out of the cup; wait out the
    12 s confirm window (≈12 ambient lines) → expect
    `FAULT confirm sensor=2 code=2 durms=<≈12000>`. Record durms.
  - STEP 5 (blink + latch): observe red 2-blink groups (250/250 ms, 2000 ms pause),
    latched. Leave faulted or RESET — next sub-scenario RESETs anyway.

Task 4: D3 — guard faults (OPTIONAL; skip any with reason, SPEC §7.6)
  - EARLY-WET: RESET → short press → `FILL start …` → `BASE ok …` → `VALVE open
    budget=30min` → dip sensor 1 IMMEDIATELY (~1–2 s after open; debounce must
    complete inside the 5 s window) → expect `VALVE close` → `FAULT earlywet
    sensor=1 code=1 fillms=<5000`. Red 1-blink groups, latched. Record fillms.
  - EARLY-TRIP: RESET → fresh fill → count ~20–30 `S1=` lines after `VALVE open`
    (clear of the 5 s window, well under 60 s) → dip sensor 1, hold ~1–2 s → expect
    `VALVE close` → `FAULT earlytrip sensor=1 code=1 fillms=<(5000..60000)>`.
    Red 1-blink groups, latched. Record fillms.
  - Either sub-item skipped → write the reason in the template's D3 skip field.
    RESET between the two if both are run.

Task 5: D4 — backstop expiry (OPTIONAL; recorded skip expected, SPEC §7.7)
  - IF RUN: DISCONNECT 12 V from the MOSFET module first (template safety hint: the
    solenoid stays de-energized during the long open; SPEC §9.1/§9.2 permit this,
    log lifecycle unchanged). RESET → short press → `FILL start …` → `BASE ok …` →
    `VALVE open budget=30min` → leave both sensors dry ~30 min (≈1800 ambient
    lines) → expect `VALVE close` → `FAULT backstop code=3 budget=30min
    openms=<≈1800000>ms` + red 3-blink groups, latched.
  - IF SKIPPED (expected default): write the reason (e.g. "time-boxed session; ~30
    min wall-clock not permitted") in the template's D4 skip field. A skip is NOT a
    failure for row D.

Task 6: FINALIZE evidence (software)
  - STOP capture cleanly (Ctrl-C / end save). Never edit captured lines.
  - FILL template-session-D-fault-paths.md IN PLACE: session record (date, evidence
    log, FW git rev, Checklist pass/total, Status), observed values into every
    `____` (count=, fillms=, durms=, BTN hold ms), blink-cadence notes per fault,
    skip reasons for any D3/D4 items not run.
  - EDIT docs/bench/README.md row D ONLY: real date, evidence filename, FW git rev,
    `pass/total` (required items only; optional skips excluded), Status ∈
    {PASS, PARTIAL(n✗), BLOCKED: reason} — SKIPPED is NOT allowed for row D.
  - DISCREPANCIES: for every expectation mismatch (and the D-1 fault-LED
    observation), ensure docs/bench/$(date +%F)-discrepancies.md exists (copy
    template-discrepancies.md ONLY if this bench day has none; S1/S2 may have made
    it — then APPEND). Quote log lines VERBATIM in fenced blocks.
  - RUN: Level 4 verification gates below; fix bookkeeping, NEVER the captures.

Task 7: BLOCKED branch (only if the rig is unavailable or the Task-0 gate fails)
  - SKIP Tasks 1–6 (no evidence file created; template left unfilled).
  - EDIT row D Status → `BLOCKED: <real reason>` (e.g. "rig unavailable — no serial
    device on host; upstream sessions not executed"). Leave other cells `—`.
  - REPORT: state the reason plainly; do not pre-create empty files.
```

### Implementation Patterns & Key Details

```bash
# Expected event-line skeletons per sub-scenario (ambient `S1=… S2=…` + `BTN …`
# interleaved; only EVENT ordering is asserted — filter with `grep -v '^S1='`).

# D1 — baseline fault (the CRITICAL one):
#   JugFiller boot (reset cause: POWER_OR_RESET)
#   [sensor 1 already submerged]
#   BTN short <n>ms
#   FILL start backstop=30min recovery=0ms recmin=0
#   … NO `BASE ok`, NO `VALVE open` here …
#   VALVE close                    ← valve-closed-first even though never opened
#   FAULT baseline sensor=1 code=1 count=<n>
#   … red 1-blink groups, latched …
#   BTN short <n>ms                ← BOOT while faulted: logged, IGNORED
#   … NO FILL start follows that BTN …
#   JugFiller boot (reset cause: POWER_OR_RESET)   ← RESET clears the latch

# D2 — no-confirm fault:
#   JugFiller boot (reset cause: POWER_OR_RESET)
#   BTN short <n>ms → FILL start backstop=30min recovery=0ms recmin=0
#   BASE ok s1=<n> s2=<n>
#   VALVE open budget=30min
#   … ~60–65 ambient lines (sensors dry) …
#   VALVE close
#   TRIP sensor=1 fillms=<≥60000>
#   … ~12 ambient lines (sensor 2 stays dry) …
#   FAULT confirm sensor=2 code=2 durms=<≈12000>
#   … red 2-blink groups, latched …

# D3 — guards (optional):
#   earlywet:  VALVE open … dip ≤2 s … VALVE close → FAULT earlywet  sensor=1 code=1 fillms=<5000
#   earlytrip: VALVE open … ~20–30 lines … dip … VALVE close → FAULT earlytrip sensor=1 code=1 fillms=<5000..60000>

# D4 — backstop (optional):
#   VALVE open budget=30min … ~1800 ambient lines … VALVE close →
#   FAULT backstop code=3 budget=30min openms=<≈1800000>ms   (red 3-blink)

# Blink contract (human-observed, recorded in template): red N-blink groups =
#   N × (on 250 ms / off 250 ms) then off 2000 ms, repeat; N = code (1/2/3).
```

### Integration Points

```yaml
DOCS (the only integration surface):
  - docs/bench/README.md: row D finalized; status vocabulary CLOSED (§2); SKIPPED
    permitted only for the backstop sub-item inside the template, never for row D.
  - template-session-D-fault-paths.md: becomes the durable session record.
  - YYYY-MM-DD-discrepancies.md: appended; D-1 gains its red-fault-blink observation.
  - Upstream contract: consumed from P1.M2.T2.S1 (rig proof / row A) and the
    parallel P1.M2.T2.S2 (flow technique, shared bench-day discrepancies file);
    this task performs NO reflash and NO firmware edits.
  - Downstream contract: P1.M2.T2.S5 consolidates rows A–E and the ledger; row D's
    pass/total and any D-3+ discrepancy entries feed that consolidation.
FIRMWARE: none — verify only (pio run -e pico).
SPEC: spec/SPEC.md is the source of truth and is NOT edited (Mode A).
CONFIG/BUILD: none.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Evidence-file "syntax": names, headers, index row well-formed
ls docs/bench/$(date +%F)_session-D-fault-paths.log   # exists (or BLOCKED branch)
head -8 docs/bench/$(date +%F)_session-D-fault-paths.log  # header block, rev filled
grep -c '^| D |' docs/bench/README.md                 # exactly 1
# Expected: file exists (or BLOCKED branch taken), header complete, one row D.
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
#  M docs/bench/README.md                        (row D only)
#  M docs/bench/template-session-D-fault-paths.md
#  ?? docs/bench/<date>_session-D-fault-paths.log
# (?? docs/bench/<date>-discrepancies.md only if newly created this task)
# NOTHING under firmware/, test/, spec/, platformio.ini, README.md root,
# COMMISSIONING.md, or any S1/S2 artifact (templates A/B/C/E, their logs, rails,
# rows A/B/C).
```

### Level 4: Creative & Domain-Specific Validation

```bash
D=docs/bench/$(date +%F)_session-D-fault-paths.log

# (D-1a) banner present, first stream line, correct cause:
head -1 "$D" | grep -q '^JugFiller boot (reset cause: POWER_OR_RESET)$' && echo "BANNER OK"

# (D-1b) THE critical assertion — no VALVE open between the D1 FILL start and its fault.
# Segment = from the first `FILL start` to the first `FAULT baseline`:
awk '/^FILL start/{s=1} s&&!/^FAULT baseline/{print} /^FAULT baseline/{exit}' "$D" \
  | grep -q '^VALVE open' && echo "FAIL: gate opened on wet baseline" || echo "NO-VALVE-OPEN OK"

# (D-1c) baseline fault present with the right shape; BOOT ignored while faulted:
grep -c '^FAULT baseline sensor=1 code=1 ' "$D"        # ≥1 (2 if sensor-2 repeat run)
grep -n '^FAULT baseline\|^FILL start\|^BTN ' "$D" | grep -v '^S1=' \
  # eyeball: any BTN line AFTER the first FAULT baseline and BEFORE the next banner
  # has NO FILL start following it inside the faulted segment

# (D-2) no-confirm chain, in order, with guard-bounded values:
grep -nE '^VALVE open budget=30min$|^TRIP sensor=1 |^FAULT confirm ' "$D" \
  # eyeball ascending line numbers: open < TRIP < FAULT confirm
grep '^TRIP sensor=1 ' "$D" | sed 's/.*fillms=\([0-9]*\)ms.*/\1/' | awk '$1>=60000{print "TRIP>=60s OK"; exit}'
grep '^FAULT confirm sensor=2 code=2 ' "$D" | sed 's/.*durms=\([0-9]*\)ms.*/\1/' \
  | awk '$1>=11500&&$1<=13000{print "CONFIRM-WINDOW ~12s OK"; exit}'

# (D-3/4) optional guards / backstop if run (absent = skip reason required in template):
grep -E '^FAULT (earlywet|earlytrip|backstop) ' "$D" || echo "D3/D4 skipped — check template reason"
grep '^FAULT earlywet '  "$D" | sed 's/.*fillms=\([0-9]*\)ms.*/\1/' | awk '$1<5000{print "EARLYWET<5s OK"; exit}'
grep '^FAULT earlytrip ' "$D" | sed 's/.*fillms=\([0-9]*\)ms.*/\1/' | awk '$1>5000&&$1<60000{print "EARLYTRIP window OK"; exit}'

# (D-5) every FAULT line is preceded (within its segment) by VALVE close — §7.9:
grep -nE '^VALVE close$|^FAULT ' "$D"   # eyeball: each FAULT has a close before it, no open after

# (D-6) template filled in place + row D finalized + BLOCKED-consistency:
grep -c '\[x\]' docs/bench/template-session-D-fault-paths.md          # >0 (or unfilled if BLOCKED)
grep -E '^\| D \|' docs/bench/README.md | grep -vE 'PENDING'          # finalized row
grep -E '^\| D \|' docs/bench/README.md | grep -q 'BLOCKED' && \
  { ls docs/bench/*_session-D-fault-paths.log 2>/dev/null && echo "FAIL: files despite BLOCKED" \
    || echo "BLOCKED-consistency OK"; } || true

# (D-7) blink observations recorded (human evidence lives in the template, not the log):
grep -iE 'blink|LED' docs/bench/template-session-D-fault-paths.md     # cadence notes present
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (or BLOCKED branch consistent: row D marked, no file, no fabrication)
- [ ] `pio run -e pico` `[SUCCESS]`; firmware/test trees untouched (`git status` scope exact; no S1/S2 artifacts touched)

### Feature Validation

- [ ] D1: wet-start → `FILL start` (no `BASE ok`) → `VALVE close` → `FAULT baseline sensor=1 code=1 count=<n>`; **no `VALVE open` in the segment**; red 1-blink groups latched; BOOT press logged but ignored; RESET clears to a fresh banner; (optional) sensor-2 repeat shows code=2 + 2-blink groups
- [ ] D2: ≥60 s open → `TRIP sensor=1 fillms≥60000` → 12 s with sensor 2 dry → `FAULT confirm sensor=2 code=2 durms≈12000`; red 2-blink groups latched
- [ ] D3/D4: run with correct `FAULT earlywet/earlytrip/backstop` lines, values inside their windows, N-blink groups — OR written skip reasons in the template
- [ ] Blink-code verification recorded per fault (N = code, 250/250 ms + 2000 ms pause, latched); D-1 indicator observation noted in the discrepancies ledger
- [ ] Template D filled in place (date, log, rev, pass/total); index row D finalized; every mismatch in the discrepancies ledger verbatim

### Code Quality Validation

- [ ] Fill discipline honored: template D in place; discrepancies file copied-once-then-appended; templates A/B/C/E, rails sheet, S1/S2 evidence and rows untouched
- [ ] Header block complete (date, git rev, wiring incl. 12-V state per sub-scenario, deviations) as the log's first lines
- [ ] No edits to spec/SPEC.md, README.md (root), COMMISSIONING.md, firmware/, test/

### Documentation & Deployment

- [ ] Evidence filename date-stamped per README §4 conventions; row D legible to P1.M2.T2.S5 (consolidation)
- [ ] Optional-skip reasons recorded where used; no SKIPPED status on row D itself

---

## Anti-Patterns to Avoid

- ❌ Never connect USB and the buck's 5 V simultaneously — VSYS ties to USB VBUS with no diode
- ❌ Never allow a `VALVE open` line in the D1 segment — that IS the failure signature (gate failed open on a wet sensor); it is also a dry-fire safety event
- ❌ Never clear a latched fault with anything but RESET — BOOT presses while faulted must be shown to be ignored (that's a checked item, not a workaround)
- ❌ Never dip sensor 1 before ~60 ambient lines have passed in D2 — that turns D2 into an earlytrip fault (D3 territory); retake from RESET, don't tick
- ❌ Never dip sensor 2 during D2's 12 s window — that turns D2 into Session C's confirm path; retake, don't tick
- ❌ Never dawdle at the D3 boundaries — dip within ~1–2 s for earlywet (<5 s) and at ~20–30 s for earlytrip (5–60 s); values near the edges risk misclassification
- ❌ Never hold a press ≥1 s — long press during FILLING logs `PLUS5` (+5 min) instead of starting/aborting; all presses are short and fire on release
- ❌ Never run D4 with 12 V on the MOSFET module — the solenoid would stay energized dry for ~30 min; disconnect per the template's safety hint
- ❌ Never edit, trim, or retype captured lines; never synthesize a capture — genuine evidence or BLOCKED, nothing else
- ❌ Never record an LED observation you did not watch — blink codes are human-observed evidence; note "not observed" honestly if missed, and file it as a discrepancy
- ❌ Never edit spec/SPEC.md to match observations — discrepancies go to the ledger (Mode A)
- ❌ Never reflash firmware or "fix" firmware/test if a gate fails — record the mismatch; fixes are a human decision
- ❌ Never use SKIPPED for row D — only the D3/D4 sub-items inside the template may be skipped with reason; the row itself is PASS / PARTIAL(n✗) / BLOCKED
- ❌ Never touch rows A/B/C, templates A/B/C/E, or S1/S2 log files — they belong to sibling sessions, and S2 runs in parallel

---

**Confidence Score: 9/10** — every input contract was read from disk this session (the Session D template with its checklists, critical assertion, skip fields and grep hints; the bench README's integrity rule, status vocabulary — including that SKIPPED is legal only for the backstop sub-item inside D — capture procedure and the live row-A BLOCKED state; the binding evidence-package Session D expectations; the firmware's byte-exact FAULT format strings at jug_filler.ino:1343–1395; and the config tunables FAULT_BLINK_MS/FAULT_PAUSE_MS, CONFIRM_WINDOW_S, EARLY_WET_WINDOW_S, TRIP_MIN_ELAPSED_S, DEBOUNCE_COUNT×READING_INTERVAL_MS). The PRP carries the full sub-scenario runbook with derived dip timings for each guard window, the latch/RESET/BOOT-ignored semantics, the parallel-session boundary (S2 owns rows B/C), a first-class BLOCKED branch matching the current rig-unavailable reality, and machine-checkable post-capture gates. The single inherent risk is physical rig availability, which the item contract itself resolves via BLOCKED / recorded skips — not a PRP defect.
