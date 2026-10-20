# PRP — P1.M2.T2.S4 (bugfix changeset): Session E optional watchdog-reset check via temporary never-committed hang patch (or recorded skip)

## Goal

**Feature Goal**: Close out the optional SPEC §10 watchdog evidence for BUG-003, in one of exactly three contract-legal outcomes:
1. **RUN** — on a rig proven by sessions A–D, flash a build carrying a **temporary, never-committed** deliberate-hang patch (`#if BENCH_HANG_TEST … while(true){} … #endif` placed inside `loop()` after the fill-lifecycle stage, conditioned on `FillState::Filling`), start a fill, let the unfed RP2040 watchdog (WATCHDOG_TIMEOUT_S = 8) reset the board mid-FILLING, capture the stream showing `JugFiller boot (reset cause: WATCHDOG)` + valve-closed + LED-off + no-auto-restart + healthy-post-reset fill, then REVERT the patch (firmware tree pristine) and REFLASH the pristine build so the bench board no longer carries the hang.
2. **SKIPPED: <reason>** — a deliberate, documented decision not to run it (e.g. policy against temporary firmware patches, time-boxed bench day) despite a green entry gate. Row E is the ONLY index row where `SKIPPED` is legal.
3. **BLOCKED: <reason>** — rig unavailable or the entry gate (sessions S1–S3 green) is not met; no log file, template unfilled (integrity rule: never fabricate).

**Deliverable**:
- `docs/bench/YYYY-MM-DD_session-E-watchdog.log` — raw 115200 8N1 capture (operator header block as first lines; header records that the hang patch was APPLIED and REVERTED) — only in the RUN outcome.
- `docs/bench/template-session-E-watchdog.md` — filled IN PLACE (session record, ticked checklists, patch method/applied/reverted in the cleanup section) — RUN outcome; or the skip-reason field filled — SKIPPED outcome.
- `docs/bench/README.md` — index row E finalized with a closed-vocabulary status (`PASS` / `PARTIAL(n✗)` / `BLOCKED: reason` / `SKIPPED: reason`).
- A **clean firmware tree**: `git status --short firmware/` empty, and the bench board reflashed with the pristine committed build.
- `docs/bench/YYYY-MM-DD-discrepancies.md` — appended (or created from `template-discrepancies.md` if the bench day has none) for any observed-vs-expected mismatch.

**Success Definition**: Either the log is a genuine capture whose stream shows, in order: `POWER_OR_RESET` banner → `BTN short` → `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok …` → `VALVE open budget=30min` → ≈8 s of 1 Hz ambient → `JugFiller boot (reset cause: WATCHDOG)` → LED-off/valve-closed/no un-pressed `FILL start` → a post-reset `BTN short` that DOES start a fill — with template E filled, row E finalized, patch reverted, pristine build back on the board, and `pio run -e pico` `[SUCCESS]` — OR row E reads `SKIPPED: <real reason>` / `BLOCKED: <real reason>` with no log file and no firmware change ever committed. `git status` shows only the additions/edits listed above; nothing belonging to the parallel S3 session (row D, template D, session-D log).

## User Persona

**Target User**: The bench operator (human, or an agent with physical rig access) executing the BUG-003 evidence plan; downstream, P1.M2.T2.S5 (consolidation reads row E and the discrepancy ledger).

**Use Case**: With the rig operational and sessions A–D evidenced, prove the last SPEC §10 safety claim physically: a hung firmware loop cannot leave the valve open — the hardware watchdog resets the board within ~8 s, `setup()` line 1 re-closes the valve, the device lands IDLE, and it never auto-refills.

**User Journey**: Apply the one-hunk hang patch locally → `pio run -e pico -t upload` (patched) → start capture → tap RESET → `POWER_OR_RESET` banner → short press → `FILL start` → `BASE ok` → `VALVE open` (LED solid blue) → the very next loop pass hangs → ≈8 s → board resets (audible valve click if 12 V connected) → `JugFiller boot (reset cause: WATCHDOG)` → LED off, ambient 1 Hz resumes, no `FILL start` appears → short press → fresh `FILL start backstop=30min recovery=0ms recmin=0` proves the device is healthy → stop capture → `git checkout -- firmware/jug_filler/jug_filler.ino` → rebuild+reflash pristine → fill template E → finalize row E → file discrepancies (if any).

**Pain Points Addressed**: BUG-003 — the optional watchdog-reset check of the physical bench validation (PRD Recommendations: "Execute the deferred physical bench validation … on USB-only power and capture the serial logs under docs/bench/") has no evidence; this session supplies it or records the contract-legal skip.

## Why

- Bug-fix PRD Issue 3 (BUG-003, Minor): the hardware-validated sign-off "including the optional watchdog-reset check" has not been performed or recorded.
- Bug-fix PRD Recommendations: execute the deferred bench validation and capture the logs.
- Item contract (P1.M2.T2.S4): this check is OPTIONAL per the original contract — "a documented skip (with reason, e.g. policy against temporary firmware patches) recorded in the index is an acceptable outcome; never fabricate the capture."
- Evidence package §5 (scout-verified): `config.h` is constexpr-only (zero `#define`/`#if` — verified by grep this session: count 0) and no `UNIT_TEST`-adjacent symbol exists in firmware/, test/, or platformio.ini — so the deliberate hang must be ADDED temporarily and REVERTED; it can never ride an existing build flag.
- Downstream: P1.M2.T2.S5 consolidates rows A–E and the discrepancy ledger; row E must carry a closed-vocabulary status before that.

## What

One optional physical session (single capture) or one recorded skip/block. **Power exclusivity is binding** (SPEC §2.2; RP2040-Zero VSYS ties directly to USB VBUS with no diode): USB-powered only, buck 5 V disconnected from the board. 12 V MAY feed the MOSFET module's power input ALONE if the audible close-click on the watchdog reset is wanted — the solenoid sees ONE ~8 s energization, a recorded deviation from the <5 s dry-fire rule, explicitly accepted for this check by the session-E template.

1. **Entry gate (software)**: template E and the bench README exist; LIVE index re-read (rows A–D — S3 finalizes row D in parallel); rows A/B/C must be PASS and row D finalized (not `PENDING`) for the RUN branch; `pio device list` shows the RP2040-Zero serial device; `pio run -e pico` compiles `[SUCCESS]`; record `git rev-parse --short HEAD`.
2. **The temporary hang patch (never committed)**: ONE contiguous local edit inside `loop()`, placed after the fill-lifecycle routing stage (immediately after the `// ---- end Fill lifecycle routing ----` comment, ~jug_filler.ino:1740):
   ```cpp
   // ---- TEMPORARY Session-E bench hang patch — NEVER COMMIT — REVERT AFTER USE ----
   #define BENCH_HANG_TEST 1
   #if BENCH_HANG_TEST
   if (s_fill.state() == FillState::Filling) { while (true) {} }
   #endif
   #undef BENCH_HANG_TEST
   ```
   Rationale (verified against the source this session): `static FillEngine s_fill;` (L1477) and `FillState state() const` (L1263) with `FillState::Filling` (L913/940) are visible in `loop()`; the condition lets the board boot IDLE and accept the button press, then hangs on the first pass after the engine enters FILLING (right after `VALVE open` logs and the valve pin goes HIGH — L1721/1731); the `while (true) {}` never returns so `watchdog_update()` (loop's first statement, L1673) never runs again → guaranteed starvation reset ≈8 s later. Alternative accepted by the template: keep only the `#if BENCH_HANG_TEST` block in the edit and supply `-DBENCH_HANG_TEST=1` on the command line (`PLATFORMIO_BUILD_FLAGS='-DBENCH_HANG_TEST=1' pio run -e pico`) — but the self-contained single-hunk variant above is preferred because revert is exactly `git checkout -- firmware/jug_filler/jug_filler.ino`.
3. **Run (physical)**: flash patched (`pio run -e pico -t upload`; if auto-reboot upload fails: hold BOOT + tap RESET to enter BOOTSEL, retry) → start capture with the operator header block as first lines (header records: base git rev, "patched build = base rev + temporary BENCH_HANG_TEST hang, applied <time>, reverted <time>", 12-V state, the ~8 s solenoid deviation if 12 V is connected) → tap RESET → `JugFiller boot (reset cause: POWER_OR_RESET)` is the first stream line → short press → `BTN short <n>ms` → `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok s1=… s2=…` → `VALVE open budget=30min` (LED solid blue) → the next pass hangs (do nothing) → ≈8 s (≈7–9 ambient `S1=` lines) → board resets: valve click (if 12 V), `JugFiller boot (reset cause: WATCHDOG)`, LED off → 1 Hz ambient resumes → verify NO `FILL start` appears without a press → short press → `BTN short <n>ms` → `FILL start backstop=30min recovery=0ms recmin=0` (reset cleared tank history; device healthy) → stop capture.
4. **Cleanup (mandatory)**: `git checkout -- firmware/jug_filler/jug_filler.ino`; `git status --short firmware/` must be EMPTY; rebuild + reflash the pristine build (`pio run -e pico -t upload`) so the bench board runs committed firmware again — a surviving hang-patched flash would hang EVERY later FILLING session; optionally tap RESET and confirm a plain `POWER_OR_RESET` banner. Record patch method / applied / reverted in the template's cleanup section.
5. **Finalize**: fill template E in place (session record: date, evidence log, FW rev = `<base> (patched: base + BENCH_HANG_TEST hang, reverted)`, checklist pass/total, status); update index row E only; append/create the date-stamped discrepancies file for any mismatch (including LED/valve observations that disagree).

NOT in scope: Sessions A/B/C/D (done by S1/S2/S3 — do not touch their rows, templates, or logs); consolidation of rows beyond E (S5); README/COMMISSIONING sweeps (P1.M2.T3.x); ANY committed firmware change (the patch exists only on the bench flash during the session and is reverted in the tree); editing spec/SPEC.md (Mode A); modifying config.h (stays untouched — the define lives in the temporary ino hunk only, per the item contract's DOCS clause).

### Success Criteria

- [ ] Task 0 entry gate evaluated against the LIVE index; if unmet, the BLOCKED or SKIPPED branch was taken — row E carries the real reason, no log file, template unfilled (BLOCKED) or skip-reason-only (SKIPPED)
- [ ] RUN outcome: `docs/bench/YYYY-MM-DD_session-E-watchdog.log` genuine capture, header block first lines, exactly two boot banners (`POWER_OR_RESET` first stream line, `WATCHDOG` after the hang)
- [ ] Reset cause reads `WATCHDOG` (not `POWER_OR_RESET`) on the post-hang banner
- [ ] ≈8 s hang→reset interval (≈7–9 ambient 1 Hz lines between `VALVE open` and the `WATCHDOG` banner; WATCHDOG_TIMEOUT_S = 8)
- [ ] Valve-closed on reset (audible click observed/noted if 12 V connected; template item ticked either way from the banner + LED evidence)
- [ ] LED off after reset; ambient `S1=/S2=` resumes; NO `FILL start` line without a preceding `BTN` press anywhere in the stream (no auto-restart, SPEC §5.3/§10)
- [ ] Post-reset short press DOES start a fill: `FILL start backstop=30min recovery=0ms recmin=0`
- [ ] Patch reverted: `git status --short firmware/` empty; pristine build rebuilt + reflashed; template cleanup fields (method/applied/reverted) filled; evidence header records apply+revert
- [ ] Template E filled in place; index row E finalized with a closed-vocabulary status; every mismatch in the discrepancies ledger verbatim
- [ ] `pio run -e pico` `[SUCCESS]`; `git status` scope = row E edit + template E fill + the one new log (+ discrepancies file only if created/appended by this task); nothing under firmware/ (post-revert), test/, spec/, platformio.ini, README.md root, COMMISSIONING.md, or S1/S2/S3 artifacts

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the exact one-hunk patch (with the verified in-scope symbols `s_fill`/`FillState::Filling` and the verified placement after the fill-lifecycle routing stage), the flash/upload mechanics for a project whose platformio.ini is compile-only by default, the byte-exact expected log lines, the ~8 s timing and its 1-Hz ambient-line signature, the three contract-legal outcomes with their exact index-status vocabulary (including that SKIPPED is legal ONLY for row E), the 12-V deviation rule, the mandatory revert+reflash-pristine cleanup, the header-block contract including the apply/revert record, the parallel-session boundaries (S3 owns row D), and post-capture machine-checkable gates. All contracts were read from disk this session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/bench/template-session-E-watchdog.md
  why: THE binding checklist for this session — fill IN PLACE. Carries the
        preconditions (USB-only, capture started before the patched reset, patch
        never committed, optional 12 V = recorded ~8 s deviation), the procedure,
        every expected observation with its checkbox, the critical assertions
        (cause must read WATCHDOG; no un-pressed FILL start; patch reverted),
        the mandatory cleanup section (patch method/applied/reverted), and the
        operator verification greps.

- file: docs/bench/README.md
  why: THE index you update (row E only) + §1 integrity rule (genuine captures or
        BLOCKED, never fabricate; never edit a captured .log) + §2 closed status
        vocabulary — SKIPPED is allowed ONLY for Session E (and the backstop
        sub-item inside D), never elsewhere + §3 capture procedure (USB-only
        binding rule, 115200 8N1, operator header block; item 5 = Session-E-only:
        the hang is a temporary never-committed patch, git status clean afterward,
        the header records apply+revert) + §4 naming. TODAY rows A/B/C read
        `BLOCKED: rig unavailable …` and rows D/E are PENDING — re-read live
        state at Task 0; do not touch rows A–D.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  section: "1. BINDING power-safety rules" + "2. Session E" + "4. Executability
        split" + "5. Watchdog-hang mechanism — current state (scout-verified)"
  why: §1 the five binding power rules (never USB+buck simultaneously; 12 V to
        the MOSFET module alone OK; dry-fire opens <5 s — Session E's single ~8 s
        energization is the template-sanctioned deviation). §2 Session E expected
        prefixes. §4 the watchdog test flash is NOT software-agent executable —
        physical rig required. §5 why the hang must be a temporary ADDED patch
        (config.h constexpr-only, no UNIT_TEST symbol anywhere) with the canonical
        `#if BENCH_HANG_TEST … while(true){} … #endif` shape inside loop().

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S3/PRP.md
  why: CONTRACT for the parallel prior item (Session D, currently implementing).
        Assumes: row D finalized by S3 (PASS / PARTIAL / BLOCKED per its own
        gate), the shared bench-day discrepancies file may exist (then APPEND,
        never recreate), the header-block and no-fabrication discipline proven.
        Your entry gate reads S3's row-D outcome; never touch D's row/template/log.

- file: firmware/jug_filler/jug_filler.ino
  section: setup() L1646-1670 + loop() L1672-1674 + fill routing L1715-1745 +
        formatBootBanner L85-87 + FillEngine::state() L1263
  why: the patch's anchor points, all verified: setup() LINE 1 drives PIN_VALVE
        LOW (valve closed before all else on every reset); watchdog_update() is
        loop()'s FIRST statement (L1673); watchdog_enable(8000, pause_on_debug)
        is LAST in setup() (L1669); the fill routing stage (s_fill.step L1721,
        valve pin write L1731, log emission) is where the hang hunk goes AFTER
        (`// ---- end Fill lifecycle routing ----` ~L1740); `s_fill` (L1477) and
        `FillState::Filling` (L913) are the in-scope symbols; the banner text is
        exactly "JugFiller boot (reset cause: WATCHDOG|POWER_OR_RESET)" (L85-87).
        Read-only EXCEPT for the temporary hunk, which is reverted before close.

- file: firmware/jug_filler/config.h
  why: WATCHDOG_TIMEOUT_S = 8 (L74; hard ceiling 8.388 s RP2040-E1 — never
        raised); confirms constexpr-only (zero #define/#if) so the guard define
        must live in the temporary ino hunk or on the command line — config.h
        stays untouched (item contract DOCS clause).

- file: platformio.ini
  why: env:pico is compile-only by default ("No upload from this project yet") —
        flashing requires the explicit target `pio run -e pico -t upload`.
        Untouched by this task (never add build_flags to it).

- file: spec/SPEC.md
  section: §10 (watchdog: hang → reset → valve LOW → IDLE, never auto-refill;
        fed first every loop pass) + §5.3 (no auto fill start after any reset) +
        §2.2 (USB-only power rule)
  why: source of truth for WHY the check exists and what the reset must prove.
        NEVER edited (Mode A).

- url: https://arduino-pico.readthedocs.io/en/latest/platformio.html
  why: pico env + upload mechanics for arduino-pico under PlatformIO
        (picotool/UF2 upload; auto-reboot via USB-CDC touch; manual BOOTSEL
        entry = hold BOOT + tap RESET when auto-reboot fails).

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: hardware reference for the 5V/VSYS-USB VBUS tie (no diode) behind the
        binding power-exclusivity rule, the onboard BOOT (BOOTSEL) button, and
        the reset button used between phases.
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── README.md                      # read-only here (swept by P1.M2.T3.S1)
├── platformio.ini                 # untouched (compile-only comment stays)
├── firmware/jug_filler/           # jug_filler.ino + config.h — CLEAN at start;
│   │                              # carries the temporary hunk ONLY during the
│   │                              # RUN, reverted + pristine-reflashed at close
├── test/                          # untouched
├── docs/
│   ├── COMMISSIONING.md           # read-only here (swept by P1.M2.T3.S2)
│   └── bench/
│       ├── README.md              # index — rows A-C BLOCKED-or-PASS (S1/S2),
│       │                          # row D finalized by parallel S3; EDIT row E only
│       ├── template-session-E-watchdog.md  # T1.S2 — fill IN PLACE (this task)
│       ├── template-session-{A,B,C,D}-*.md # owned by S1/S2/S3 — do not touch
│       ├── template-rails.md / template-discrepancies.md
│       ├── YYYY-MM-DD_session-{A..D}-*.log # sibling evidence — do not touch
│       └── YYYY-MM-DD-discrepancies.md     # may exist — APPEND
└── spec/SPEC.md                   # source of truth — NEVER edited (Mode A)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
docs/bench/
├── README.md                          # MODIFIED: row E finalized (only change)
├── template-session-E-watchdog.md     # MODIFIED: filled in place = session record
├── YYYY-MM-DD_session-E-watchdog.log  # NEW (RUN only): raw capture + operator header
└── YYYY-MM-DD-discrepancies.md        # NEW only if the bench day lacks one; else appended
firmware/jug_filler/jug_filler.ino     # UNMODIFIED at task end (temporary hunk reverted)
# (YYYY-MM-DD = the session's real execution date)
```

### Known Gotchas of our codebase & Library Quirks

```cpp
// CRITICAL — the patch is the isolation mechanism (item contract MOCKING clause):
//   it exists ONLY on the bench flash during the session, NEVER in the committed
//   tree. config.h is constexpr-only (zero #define/#if — grep-verified) and no
//   UNIT_TEST-adjacent symbol exists anywhere in firmware/, test/, or
//   platformio.ini, so there is NO existing build flag to ride: the hang must be
//   a temporary ADDED hunk. Revert = `git checkout -- firmware/jug_filler/jug_filler.ino`
//   (one contiguous hunk — keep the #define INSIDE the hunk so the revert is atomic).
//   NEVER: `git add`/`git commit` the hunk; never add -DBENCH_HANG_TEST to
//   platformio.ini build_flags; never touch config.h.

// CRITICAL — hang placement/condition: put the hunk AFTER the fill-lifecycle
//   routing stage and condition it on `s_fill.state() == FillState::Filling`.
//   An UNCONDITIONAL while(true){} at loop() top would hang at boot — before the
//   button press — and prove nothing (no VALVE open, no fill context). The
//   hang fires on the first pass after the engine enters Filling, i.e. right
//   after `VALVE open` logs; the operator does NOTHING further after the press.

// CRITICAL — the hang must never feed the watchdog: `while (true) {}` with an
//   EMPTY body. Any watchdog_update() / delay() inside would defeat the check.
//   watchdog_update() is loop()'s FIRST statement (L1673) — the hung pass simply
//   never loops back around, and watchdog_enable(8000 ms, pause_on_debug=true)
//   (L1669) fires ≈8 s later. Under an attached debugger pause_on_debug would
//   pause it — run the bench WITHOUT a debugger session.

// CRITICAL — power exclusivity (hardware, binding): RP2040-Zero 5V pin (VSYS)
//   ties DIRECTLY to USB VBUS with NO diode. Never USB + buck simultaneously.
//   This session = USB-powered, buck 5 V disconnected. 12 V to the MOSFET
//   module's power input ALONE is optional (audible close-click on reset); it
//   means ONE ~8 s solenoid energization — the template-sanctioned recorded
//   deviation from the <5 s dry-fire rule; write it in the header's deviations.

// CRITICAL — flash mechanics: platformio.ini is compile-only by default; the
//   patched build is flashed with the explicit target `pio run -e pico -t upload`.
//   arduino-pico auto-reboots the board via USB-CDC touch; if upload can't
//   reset the board, hold BOOT + tap RESET (BOOTSEL bootloader) and retry —
//   deliberate bootloader entry is fine HERE, but never hold BOOT while
//   (re)plugging power during normal operation.

// CRITICAL — cleanup is two-part: (1) revert the tree (git status clean of
//   firmware changes — the template's mandatory item) AND (2) reflash the
//   PRISTINE build so the bench board stops carrying the hang. The template's
//   cleanup text only names (1), but a surviving patched flash would hang EVERY
//   later FILLING session (S5 re-runs, future commissioning) — this PRP makes
//   (2) mandatory; verify with a fresh `POWER_OR_RESET` banner boot.

// GOTCHA — expected timing signature: WATCHDOG_TIMEOUT_S = 8 → the reset lands
//   ≈8 s after the hang starts; at the 1 Hz ambient cadence that is ≈7–9 `S1=`
//   lines between `VALVE open` and the WATCHDOG banner. Substantially fewer
//   (>4 s early) or more (>10 s) = discrepancy, record it, never edit the log.

// GOTCHA — banner causes: a normal RESET or the post-flash reboot yields
//   `POWER_OR_RESET`; ONLY the starved-watchdog reset yields `WATCHDOG`
//   (watchdog_caused_reboot() read once in setup() before the watchdog is
//   re-enabled, L1657). The capture must show BOTH banners in this order.

// GOTCHA — no auto-restart (SPEC §5.3/§10): after the WATCHDOG banner the
//   device must sit IDLE — LED off, ambient 1 Hz — with NO `FILL start` line
//   unless a `BTN` press precedes it. The template's grep proves it: every
//   `^FILL start` must have a BTN line before it. The post-reset health check
//   (short press → `FILL start backstop=30min recovery=0ms recmin=0`) is the
//   deliberate counter-example proving the button still works.

// GOTCHA — capture discipline: save the raw stream (115200 8N1) with the
//   operator header block as the first lines; tap RESET only after the capture
//   is running so the banner is the first stream line. If the monitor drops on
//   flash/reset (USB re-enumeration), restart the capture APPENDING to the same
//   file and note it in deviations — never edit prior lines. Never hold BOOT
//   while (re)plugging power outside the deliberate upload flow.

// GOTCHA — fill discipline (T1.S2 contract): template E is filled IN PLACE;
//   the discrepancies file is COPIED once per bench day (if absent) then
//   APPENDED. Do not touch rows A–D, templates A–D, rails, or sibling logs.
//   Row E is the ONE row where `SKIPPED: <reason>` is a legal final status;
//   rows A–D may never use it.

// BLOCKED/SKIPPED protocol (live reality: rows A/B/C are currently
//   `BLOCKED: rig unavailable — no RP2040-Zero serial device detected on this
//   host`): if at Task 0 the rig is unavailable or the gate fails, set row E to
//   `BLOCKED: <real reason>` (could-not-run) — or, for a deliberate choice not
//   to run despite a green gate (e.g. policy against temporary firmware
//   patches), `SKIPPED: <real reason>` — and create NO log file. Never
//   synthesize a capture; the integrity rule (README §1) is the whole point.
```

## Implementation Blueprint

### Data models and structure

No committed code. Evidence artifacts only, following the T1.S2 template structure exactly (session record table, `— [ ]`/`— [x]` checklists, blank `____` fields filled with observed values, header block as the log's first lines). The one code artifact — the hang hunk — is deliberately ephemeral (apply → flash → run → revert).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITIONS + ENTRY GATE (software, before touching hardware)
  - RUN: ls docs/bench/README.md docs/bench/template-session-E-watchdog.md \
         docs/bench/template-discrepancies.md
  - EXPECT: all exist (T1.S1 + T1.S2 landed). Missing → STOP and report.
  - RUN: grep -E '^\| [A-D] \|' docs/bench/README.md   # LIVE state (S3 parallel)
  - GATE (RUN branch requires ALL): rows A/B/C = PASS (rig proven, flows green
    per the item INPUT "rig operational with all prior sessions green (S1-S3)")
    AND row D finalized (not PENDING; PASS or PARTIAL acceptable if the rig is
    still operational — D's fault outcomes don't gate the watchdog mechanism).
  - RUN: pio device list        # RP2040-Zero serial device present?
  - RUN: pio run -e pico        # compile-only [SUCCESS] on the PRISTINE tree.
  - RUN: git status --short firmware/   # MUST be empty before you start.
  - RUN: git rev-parse --short HEAD     # base rev for header + index row.
  - IF the gate fails OR the device is absent → Task 6 (BLOCKED branch).
  - IF the gate passes but the operator/policy declines the temporary patch →
    Task 7 (SKIPPED branch).

Task 1: APPLY + FLASH the temporary hang patch (the ONLY firmware touch)
  - EDIT firmware/jug_filler/jug_filler.ino — ONE contiguous hunk placed
    immediately AFTER the `// ---- end Fill lifecycle routing ----` comment
    (~L1740, after the fill.led/faultCode CHANGE-gated block, before the
    external-LED stage):
      // ---- TEMPORARY Session-E bench hang patch — NEVER COMMIT — REVERT AFTER USE ----
      #define BENCH_HANG_TEST 1
      #if BENCH_HANG_TEST
      if (s_fill.state() == FillState::Filling) { while (true) {} }
      #endif
      #undef BENCH_HANG_TEST
  - CHECK: pio run -e pico      # patched build compiles [SUCCESS].
  - FLASH: pio run -e pico -t upload    # explicit target (ini is compile-only)
    · if upload cannot auto-reset the board: hold BOOT, tap RESET, release BOOT
      (BOOTSEL bootloader), retry the upload.
  - RECORD: patch method ("single contiguous in-loop hunk, #if BENCH_HANG_TEST,
    FillState::Filling-gated"), applied time — for the template cleanup section.

Task 2: RUN the check (physical; USB-only power, buck 5 V disconnected)
  - CONFIGURE: USB connected; sensors dry in air; cup NOT needed (no dips);
    optionally 12 V to the MOSFET module power input ALONE for the audible
    close-click (records the ~8 s single-energization deviation).
  - START CAPTURE: 115200 8N1 save-to-file with the operator header block as
    the first lines, e.g.
      pio device monitor -b 115200 | tee docs/bench/$(date +%F)_session-E-watchdog.log
    Header block (mandatory, fill rev/date/deviations):
      # bench evidence — session E — optional watchdog-reset check
      - date: YYYY-MM-DD
      - firmware: git <base rev> (patched build = base + temporary
        BENCH_HANG_TEST in-loop hang; applied <time>, reverted <time>)
      - wiring: USB-powered, buck 5 V disconnected; sensors dry in air; valve
        dry (12 V on MOSFET module only / disconnected); no debugger attached
      - deviations: none | single ~8 s solenoid energization (12 V connected)
  - TAP RESET → the banner `JugFiller boot (reset cause: POWER_OR_RESET)` must
    be the first stream line; LED off; 1 Hz `S1= S2=` ambient streaming.
  - SHORT PRESS (clean press-and-release; the event fires on RELEASE) → expect
    `BTN short <n>ms` → `FILL start backstop=30min recovery=0ms recmin=0` →
    `BASE ok s1=____ s2=____` → `VALVE open budget=30min`; LED solid blue.
  - DO NOTHING — the next loop pass hits the hang (state == Filling). Watch the
    ambient stream: ≈7–9 `S1=` lines (≈8 s, WATCHDOG_TIMEOUT_S=8), then the
    board resets.
  - OBSERVE the reset: audible valve click (if 12 V connected); banner
    `JugFiller boot (reset cause: WATCHDOG)`; LED OFF (IDLE); ambient 1 Hz
    resumes; NO `FILL start` appears on its own.
  - HEALTH CHECK: wait a few seconds of clean IDLE ambient, then SHORT PRESS →
    `BTN short <n>ms` → `FILL start backstop=30min recovery=0ms recmin=0`
    (reset cleared tank history — device fully functional again). Then, e.g.,
    short-press again to abort (VALVE close → ABORT …) or simply RESET — the
    stream is already sufficient; keep whatever you do inside the capture.
  - STOP the capture cleanly (Ctrl-C / end save). Never edit captured lines.

Task 3: CLEANUP — revert + pristine reflash (mandatory, in this order)
  - RUN: git checkout -- firmware/jug_filler/jug_filler.ino
  - VERIFY: git status --short firmware/    # output must be EMPTY
  - REBUILD + REFLASH the pristine build: pio run -e pico && pio run -e pico -t upload
    (the bench board must NOT keep the hang — every later FILLING would reset).
  - VERIFY (optional but recommended): tap RESET → plain
    `JugFiller boot (reset cause: POWER_OR_RESET)` banner, normal IDLE behavior.
  - RECORD in the template cleanup section: patch method, applied time,
    reverted time; tick "git status shows no firmware changes".

Task 4: FINALIZE evidence (software)
  - FILL template-session-E-watchdog.md IN PLACE: session record (date, evidence
    log, FW git rev = "<base> (patched build = base + BENCH_HANG_TEST hang,
    reverted)", Checklist pass/total, Status), tick every observed checkbox with
    observed values (`S1=____ S2=____` resumption, timing between VALVE open and
    the WATCHDOG banner, click heard y/n), the critical assertions, and the
    cleanup fields.
  - EDIT docs/bench/README.md row E ONLY: real date, evidence filename, FW git
    rev, `pass/total`, Status ∈ {PASS, PARTIAL(n✗), BLOCKED: reason,
    SKIPPED: reason} — SKIPPED is legal for THIS row alone.
  - DISCREPANCIES: for every expectation mismatch (e.g. reset interval far from
    8 s, wrong banner cause, an un-pressed FILL start, LED not off), ensure
    docs/bench/$(date +%F)-discrepancies.md exists (copy template-discrepancies.md
    ONLY if this bench day has none; earlier sessions may have made it — then
    APPEND). Quote log lines VERBATIM in fenced blocks.
  - RUN: Level 4 verification gates below; fix bookkeeping, NEVER the captures.

Task 5: ABORT-MID-RUN branch (patch applied, run cannot complete)
  - IF the physical run is interrupted (rig fault, capture dropped beyond
    recovery, watchdog does NOT fire): stop, execute Task 3 (revert + pristine
    reflash) FIRST — the tree and the board must be clean regardless — then set
    row E to `PARTIAL(…✗)` or `BLOCKED: <reason>` as appropriate, keep any
    GENUINE partial capture as evidence (header notes the interruption), and
    file the mismatch in the discrepancies ledger.

Task 6: BLOCKED branch (rig unavailable or Task-0 gate fails)
  - SKIP Tasks 1–5 (no patch, no flash, no evidence file; template left unfilled).
  - EDIT row E Status → `BLOCKED: <real reason>` (e.g. "rig unavailable — no
    RP2040-Zero serial device on host; entry-gate rows A-C BLOCKED"). Leave
    other cells `—`.
  - REPORT: state the reason plainly; do not pre-create empty files.

Task 7: SKIPPED branch (deliberate decision despite a green gate)
  - USE when: policy against temporary firmware patches, time-boxed bench day,
    or an explicit decision to bank the optional check as skipped (the item
    contract explicitly permits this).
  - NO firmware touch, NO flash, NO log file.
  - FILL ONLY the template's precondition skip-reason field:
    `SKIPPED: <reason>` (e.g. "policy: no temporary firmware patches on the
    bench board; watchdog behavior already covered by native Unity tests").
  - EDIT row E: Status → `SKIPPED: <same reason>`; other cells `—`.
```

### Implementation Patterns & Key Details

```bash
# Expected event-line skeleton of the RUN stream (ambient `S1=… S2=…` 1 Hz
# interleaved; operator header block above it as the file's first lines):

#   JugFiller boot (reset cause: POWER_OR_RESET)      ← tap RESET after capture start
#   S1=… S2=…                                          ← IDLE ambient
#   BTN short <n>ms
#   FILL start backstop=30min recovery=0ms recmin=0
#   BASE ok s1=<n> s2=<n>
#   VALVE open budget=30min                            ← LED solid blue; next pass HANGS
#   S1=… S2=…   (× ≈7–9 lines ≈ 8 s, WATCHDOG_TIMEOUT_S=8)
#   JugFiller boot (reset cause: WATCHDOG)             ← THE assertion (38 chars)
#   S1=… S2=…                                          ← IDLE ambient resumes; LED off;
#                                                      ←   NO FILL start may appear alone
#   BTN short <n>ms                                    ← health check press
#   FILL start backstop=30min recovery=0ms recmin=0    ← device healthy again

# The temporary hunk (single contiguous edit, reverted at Task 3):
#   // ---- TEMPORARY Session-E bench hang patch — NEVER COMMIT — REVERT AFTER USE ----
#   #define BENCH_HANG_TEST 1
#   #if BENCH_HANG_TEST
#   if (s_fill.state() == FillState::Filling) { while (true) {} }
#   #endif
#   #undef BENCH_HANG_TEST
# Placement: immediately after `// ---- end Fill lifecycle routing ----` in loop().
# The empty-body while(true) never feeds the watchdog and never returns.
```

### Integration Points

```yaml
DOCS (the durable integration surface):
  - docs/bench/README.md: row E finalized; §2 vocabulary respected (SKIPPED legal
    only here); §3 item 5 satisfied (header records patch applied + reverted).
  - template-session-E-watchdog.md: becomes the durable session record incl. the
    cleanup fields (patch method / applied / reverted).
  - YYYY-MM-DD-discrepancies.md: appended for any mismatch; shared per bench day.
  - Upstream contract: consumed from P1.M2.T2.S1/S2/S3 (rig proof, rows A–D,
    header-block discipline, possibly the shared bench-day discrepancies file).
  - Downstream contract: P1.M2.T2.S5 consolidates rows A–E and the ledger; row
    E's status (any closed-vocabulary value, SKIPPED included) feeds that.
FIRMWARE:
  - temporary only: one in-loop hunk (apply → flash → run → `git checkout --` →
    pristine rebuild + reflash). Net change to the committed tree: NONE.
  - config.h and platformio.ini are NEVER touched (no build_flags, no defines).
SPEC: spec/SPEC.md is the source of truth and is NOT edited (Mode A).
CONFIG/BUILD: none persistent.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Evidence-file "syntax": names, headers, index row well-formed
ls docs/bench/$(date +%F)_session-E-watchdog.log   # exists (RUN) — absent for SKIPPED/BLOCKED
head -8 docs/bench/$(date +%F)_session-E-watchdog.log  # header block; rev + patch record filled
grep -c '^| E |' docs/bench/README.md              # exactly 1
# Expected: file exists (RUN) with a complete header incl. the applied/reverted
# record, or is absent with row E carrying SKIPPED/BLOCKED; exactly one row E.
```

### Level 2: Unit Tests (Component Validation)

```bash
pio run -e pico          # pristine rebuild after revert — [SUCCESS]
pio test -e native       # optional sanity: all projects still [PASSED]
# Expected: no regressions (net code change is zero).
```

### Level 3: Integration Testing (System Validation)

```bash
git status --short
# Expected exactly (RUN outcome):
#  M docs/bench/README.md                       (row E only)
#  M docs/bench/template-session-E-watchdog.md
#  ?? docs/bench/<date>_session-E-watchdog.log
# (?? docs/bench/<date>-discrepancies.md only if newly created this task)
# NOTHING under firmware/ (post-revert), test/, spec/, platformio.ini,
# README.md root, COMMISSIONING.md, or any A–D artifact.
# SKIPPED/BLOCKED outcome: only the row-E edit (+ template skip field for SKIPPED).
```

### Level 4: Creative & Domain-Specific Validation

```bash
E=docs/bench/$(date +%F)_session-E-watchdog.log

# (E-1) banner sequence and causes — exactly one of each, in this order:
grep -n 'reset cause:' "$E"
#   eyeball: POWER_OR_RESET first; WATCHDOG second; no third unless you RESET'd
#           during the health check (then subsequent banners are POWER_OR_RESET)

# (E-2) THE critical assertion — watchdog cause after the hang:
grep -c 'reset cause: WATCHDOG' "$E"                      # ≥1 (normally exactly 1)

# (E-3) fill context before the hang — the chain, in order:
grep -nE '^FILL start|^BASE ok|^VALVE open' "$E" | head -3
#   eyeball ascending line numbers: FILL start < BASE ok < VALVE open

# (E-4) ≈8 s hang window — ambient 1 Hz lines between VALVE open and the
#        WATCHDOG banner should be ≈7–9:
awk '/^VALVE open/{f=1;n=0;next} f&&/^S1=/{n++} /reset cause: WATCHDOG/{if(f){print n; exit}}' "$E"
#   expect a printed count in 7–9 (WATCHDOG_TIMEOUT_S = 8)

# (E-5) no auto fill start — every ^FILL start must have a BTN line before it:
grep -n '^FILL start' "$E"          # each must follow a BTN press (eyeball pairs)
grep -n '^BTN ' "$E"

# (E-6) health check — a post-WATCHDOG-banner BTN press starts a fill:
awk '/reset cause: WATCHDOG/{w=1} w&&/^FILL start backstop=30min recovery=0ms recmin=0$/{print "POST-RESET FILL OK"; exit}' "$E"

# (E-7) cleanup integrity — tree pristine, patch recorded:
git status --short firmware/        # MUST print nothing
grep -iE 'revert' docs/bench/$(date +%F)_session-E-watchdog.log   # header records it
grep -E 'patch method|applied|reverted' docs/bench/template-session-E-watchdog.md | head -3

# (E-8) index + template consistent:
grep -E '^\| E \|' docs/bench/README.md | grep -vE 'PENDING'     # finalized
grep -E '^\| E \|' docs/bench/README.md | grep -qE 'BLOCKED|SKIPPED' && \
  { ls docs/bench/*_session-E-watchdog.log 2>/dev/null && echo "FAIL: file despite BLOCKED/SKIPPED" \
    || echo "BLOCKED/SKIPPED-consistency OK"; } || true
grep -c '\[x\]' docs/bench/template-session-E-watchdog.md        # >0 for RUN/SKIPPED fill
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (or SKIPPED/BLOCKED branch consistent: row E marked, no log file, no firmware change)
- [ ] `pio run -e pico` `[SUCCESS]` on the pristine tree after revert; bench board reflashed pristine
- [ ] `git status --short firmware/` empty; no commit/stash of the hunk anywhere; platformio.ini and config.h untouched

### Feature Validation

- [ ] RUN: `POWER_OR_RESET` banner first; press → `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok` → `VALVE open budget=30min`; hang ≈8 s (≈7–9 ambient lines); `JugFiller boot (reset cause: WATCHDOG)`; valve closed (click if 12 V); LED off; ambient resumes; NO un-pressed `FILL start`; post-reset press starts a fresh fill
- [ ] Template E filled in place (record, checklist, cleanup fields); index row E finalized; every mismatch in the discrepancies ledger verbatim
- [ ] SKIPPED: reason recorded in both the template skip field and row E; BLOCKED: reason in row E, template unfilled, no files created

### Code Quality Validation

- [ ] Fill discipline honored: template E in place; discrepancies file copied-once-then-appended; templates A–D, rails sheet, sibling logs and rows untouched
- [ ] Header block complete (date, base rev + patch apply/revert record, wiring incl. 12-V state, deviations incl. the ~8 s energization if applicable) as the log's first lines
- [ ] No edits to spec/SPEC.md, README.md (root), COMMISSIONING.md, firmware/ (net), test/, platformio.ini

### Documentation & Deployment

- [ ] Evidence filename date-stamped per README §4 conventions; row E legible to P1.M2.T2.S5 (consolidation)
- [ ] The temporary define documented in the evidence file, NOT in config.h (item contract DOCS clause — config.h stays untouched)

---

## Anti-Patterns to Avoid

- ❌ Never commit, stage, or stash the hang patch — `git checkout -- firmware/jug_filler/jug_filler.ino` before the session closes; never add `-DBENCH_HANG_TEST` to platformio.ini or any define to config.h
- ❌ Never hang UNCONDITIONALLY at loop() top — the board must boot IDLE and enter FILLING first, or the evidence proves nothing (no `VALVE open` context)
- ❌ Never put anything inside the `while (true) {}` body — no `watchdog_update()`, no `delay()`, no Serial writes; the empty body IS the mechanism
- ❌ Never leave the patched build on the bench board — revert the tree AND reflash pristine; a surviving hang resets every future FILLING session
- ❌ Never connect USB and the buck's 5 V simultaneously — VSYS ties to USB VBUS with no diode
- ❌ Never run with a debugger attached — `pause_on_debug=true` would suspend the watchdog and defeat the check
- ❌ Never accept `POWER_OR_RESET` as the post-hang cause — only `watchdog_caused_reboot()` yields the WATCHDOG banner; anything else is a failed check → discrepancy ledger, not a tick
- ❌ Never edit, trim, or retype captured lines; never synthesize a capture — genuine evidence, SKIPPED, or BLOCKED, nothing else
- ❌ Never record a click/LED observation you did not perceive — note "not observed" honestly and file it as a discrepancy
- ❌ Never edit spec/SPEC.md to match observations — discrepancies go to the ledger (Mode A)
- ❌ Never use SKIPPED for any row other than E (and the backstop sub-item inside D)
- ❌ Never touch rows A–D, templates A–D, or S1/S2/S3 log files — they belong to sibling sessions, and S3 runs in parallel
- ❌ Never press-and-hold ≥1 s — long presses during FILLING log `PLUS5` (+5 min); all presses are short and fire on release

---

**Confidence Score: 9/10** — every input contract was read from disk this session: the Session E template (checklist, critical assertions, cleanup fields, operator greps, the template-sanctioned 12-V/~8 s deviation), the bench README's integrity rule + status vocabulary (SKIPPED legal ONLY for row E) + capture procedure item 5 + the LIVE row states (A–C BLOCKED on this host), the evidence package §1/§2/§4/§5 (power rules, Session E expectations, the physical-executability split, and the scout-verified constexpr-only/no-build-flag fact that forces the temporary-hunk approach), the firmware anchors (setup() line-1 valve LOW at L1646, watchdog_update() as loop's first statement at L1673, watchdog_enable(8000, pause_on_debug) at L1669, formatBootBanner at L85-87/1657, and the in-scope `s_fill`/`FillState::Filling` symbols at L1477/L913 with the verified post-routing placement point ~L1740), config.h's WATCHDOG_TIMEOUT_S=8 with zero `#define`/`#if`, platformio.ini's compile-only default (hence the explicit `-t upload`), and the parallel S3 PRP's boundaries. The PRP carries the exact one-hunk patch, flash/revert/reflash mechanics, three contract-legal outcomes with first-class BLOCKED and SKIPPED branches matching the current rig-unavailable reality, and machine-checkable post-capture gates. The single inherent risk is physical rig availability — which the item contract itself resolves via the optional-skip/BLOCKED outcomes, not a PRP defect.
