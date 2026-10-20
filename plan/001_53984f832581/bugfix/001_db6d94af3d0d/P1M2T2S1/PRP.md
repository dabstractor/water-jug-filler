# PRP — P1.M2.T2.S1 (bugfix changeset): Session A dry bench check and Stage-2 power-rail measurement with evidence capture

## Goal

**Feature Goal**: Execute the first physical bench session on the assembled rig — (a) the Stage-2 power-rail measurement (buck-only, multimeter evidence) and (b) the Session A dry bench check (USB-only serial capture: boot banner, 1 Hz sensor streaming, BTN short/long on release, 2–3 valve dry-fires with the ordered `FILL start → BASE ok → VALVE open → VALVE close → ABORT` prefix sequence and LED observations) — producing genuine, date-stamped evidence files under `docs/bench/` that prove the rig operational for Sessions B–E (P1.M2.T2.S2+).

**Deliverable**:
- `docs/bench/YYYY-MM-DD_rails.md` — Stage-2 multimeter sheet (copied from `template-rails.md`, filled at the bench; buck preset w/ board disconnected + buck at board 5V pin, both 4.9–5.1 V, exclusivity ticks).
- `docs/bench/YYYY-MM-DD_session-A-dry-bench.log` — raw 115200 8N1 capture with the mandatory operator header block as its first lines.
- `docs/bench/template-session-A-dry-bench.md` — filled IN PLACE (per the template fill discipline) with the session record and ticked checklist.
- `docs/bench/README.md` — index row A updated: real date, evidence filenames, FW git rev, `pass/total` checklist count, Status (`PASS` / `PARTIAL(n✗)` / `BLOCKED: <reason>`).
- Optionally `docs/bench/YYYY-MM-DD-discrepancies.md` (created from `template-discrepancies.md`) if any expectation mismatches.

**Success Definition**: Evidence files exist with genuine content (rig present), every Session A checklist item ticked with observations, all verification greps on the capture pass (banner, BTN short<1000 / long≥1000, non-pinned S1/S2, ordered prefixes per dry-fire), rails readings within 4.9–5.1 V, index row A finalized — OR, if the rig is unavailable, row A reads `BLOCKED: <reason>` and NO log/rails files are created (integrity rule: never fabricate). `pio run -e pico` still `[SUCCESS]` (no firmware changes). `git status` shows only the evidence/docs-bench additions listed above.

## User Persona

**Target User**: The bench operator (human, or an agent with physical rig access) executing the BUG-003 evidence plan; downstream, the implementing agents of Sessions B–E and P1.M2.T2.S5 (consolidation) reading row A as the "rig proven operational" gate.

**Use Case**: With templates prepared (P1.M2.T1.S2) and firmware flashed, run the two-part hardware validation: first the buck rail check (no serial exists there), then reconfigure USB-only and walk the Session A checklist while capturing the live log stream.

**User Journey**: unplug USB → verify buck into 4.9–5.1 V twice per the sheet → disconnect buck 5 V, reconnect USB → start capture, power/reset → banner `POWER_OR_RESET`, LED off → watch 1 Hz `S1= S2=` lines vary → tap BOOT (`BTN short`), hold ~1.5 s (`BTN long`) → short-press to start a dry-fire (LED solid, `VALVE open`), short-press again within 5 s (`VALVE close`, `ABORT`, LED off) ×2–3 → stop capture, fill template, update index row A.

**Pain Points Addressed**: BUG-003 — the physical bench validation with captured evidence never executed; this session is the first evidence block, proving power rails and the operator/indicator/log baseline before flows (B/C) and faults (D).

## Why

- Bug-fix PRD Issue 3 (BUG-003, Minor): "PRD §9 steps 1, 2, 4 … require a physical bench dry-run … capture … as evidence under docs/bench/" — the directory is now scaffolded (P1.M2.T1.S1/S2); this task supplies the first genuine evidence.
- Bug-fix PRD Recommendations: "Execute the deferred physical bench validation … on USB-only power and capture the serial logs under docs/bench/."
- bench_evidence_package.md §4 executability split: captures/observations/multimeter readings are NOT software-executable; this PRP is a runbook the operator follows, plus post-hoc software gates that verify the evidence once captured.
- Integrity rule (README §1, package §4): genuine captures only; BLOCKED with reason otherwise — this protects the whole evidence package from fabrication.

## What

A two-phase physical procedure plus evidence finalization. **Power exclusivity is binding, not optional**: exactly one source at a time (VSYS ties directly to USB VBUS with no diode).

1. **Phase R — Stage-2 rail check (buck-only, USB unplugged)**: tick the three source-exclusivity boxes; measure the buck preset with the board disconnected (4.9–5.1 V); connect the buck's 5 V to the board's 5V pin (still no USB) and measure there (4.9–5.1 V); record both in `YYYY-MM-DD_rails.md`; restore USB-only bench power for Phase A.
2. **Phase A — dry bench (USB-only, buck 5 V disconnected; sensors dry in air; 12 V may feed the MOSFET module alone for dry-fire)**: start the 115200 8N1 save-to-file capture BEFORE power/reset; prepend the operator header block; then observe/tick: boot banner `JugFiller boot (reset cause: POWER_OR_RESET)` with LED off in IDLE; 1 Hz `S1=<n> S2=<n>` streaming with counts NOT pinned at 0/4095; `BTN short <n>ms` (<1000) and `BTN long <n>ms` (≥1000) firing on release; dry-fire ×2–3 (each open <5 s) with ordered event prefixes and LED solid during FILLING / off after abort; note valve clicks heard Y/N.
3. **Finalize**: stop capture; fill `template-session-A-dry-bench.md` in place; update README index row A; record any mismatches in the date-stamped discrepancies file; run the verification greps (Level 4).

NOT in scope: Sessions B–E (P1.M2.T2.S2–S4); discrepancy consolidation and index finalization beyond row A (P1.M2.T2.S5); README/COMMISSIONING sweeps (P1.M2.T3); any firmware change (no reflash — item input: firmware already flashed, `pico` env builds clean); editing spec/SPEC.md (source of truth, Mode A evidence-only).

### Success Criteria

- [ ] `docs/bench/YYYY-MM-DD_rails.md` exists, copied from `template-rails.md`, with real meter readings in 4.9–5.1 V and all exclusivity ticks checked (or row A BLOCKED and the file absent)
- [ ] `docs/bench/YYYY-MM-DD_session-A-dry-bench.log` is a genuine capture starting with the operator header block; banner line is its first stream line
- [ ] Capture contains ≥1 `BTN short <n>ms` with n < 1000, ≥1 `BTN long <n>ms` with n ≥ 1000, 1 Hz `S1=/S2=` lines with varying values (never pinned at 0 or 4095)
- [ ] Each of the 2–3 dry-fires shows ascending line numbers for `FILL start backstop=…min recovery=…ms recmin=…` → `BASE ok s1=… s2=…` → `VALVE open budget=…min` → `VALVE close` → `ABORT attemptms=… openms=…`, with `VALVE open`→`VALVE close` gaps < 5 s
- [ ] `template-session-A-dry-bench.md` filled in place (session record + ticked checklist, click Y/N noted)
- [ ] README index row A: real date, evidence filenames, FW git rev, `pass/total`, status from the allowed vocabulary only
- [ ] `pio run -e pico` `[SUCCESS]`; native suite untouched (`pio test -e native` optional sanity, must stay green); `git status` scope = docs/bench/ evidence + template-A fill + README row A edit only

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the complete two-phase runbook with byte-exact expected log prefixes, both rails targets, the header block, the index row columns and status vocabulary, the fill discipline (in-place vs copy), the BLOCKED protocol, and post-capture verification commands. All contracts were read from the artifacts on disk this session (`docs/bench/README.md`, T1.S2 PRP listings, bench_evidence_package.md). The physical steps require the rig — the PRP's software gates still run either way.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/bench/README.md
  why: THE index you update (row A) + §3 capture procedure (115200 8N1, capture
        BEFORE power/reset, operator header block) + §1 integrity rule + §2 status
        vocabulary (PENDING/PASS/PARTIAL(n✗)/BLOCKED:reason/SKIPPED:reason).
        Row A today: `| A | Dry bench check + Stage-2 rail measurement (SPEC
        §9.1–9.2, §4) | — | YYYY-MM-DD_session-A-dry-bench.log + YYYY-MM-DD_rails.md
        | — | — | PENDING |` — replace the four `—`/PENDING cells.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T1S2/PRP.md
  why: CONTRACT for the parallel template work. When it lands, docs/bench/ gains
        template-session-A-dry-bench.md (checklist you fill IN PLACE), template-rails.md
        (sheet you COPY to the date-stamped file), template-discrepancies.md.
        Its canonical prefix table is the byte-exact source for every expected line.
  gotcha: templates B–E are NOT yours; template-rails is not filled in place.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  section: "1. BINDING power-safety rules" + "2. Session A" + "4. Executability split"
  why: §1 the five binding power rules (never USB+buck; 12 V to MOSFET module alone
        OK; rail check buck-only/no serial; dry-fire <5 s each; data sessions USB);
        §2 the Session A ordered-prefix checklist; §4 the integrity rule (BLOCKED,
        never fabricate) and the physical/software split this PRP inherits.

- file: docs/bench/template-rails.md
  why: (after T1.S2 lands) the copy source for YYYY-MM-DD_rails.md — fields: date,
        operator, meter, buck supply, deviations, result; 3 exclusivity ticks;
        measurement 1 (preset, board disconnected) and 2 (at board 5 V pin), both
        4.9–5.1 V; restore-power tick; discrepancies pointer.

- file: docs/bench/template-session-A-dry-bench.md
  why: (after T1.S2 lands) the checklist you tick IN PLACE — session record table
        (date, evidence log, rails sheet, FW git rev) + the §9.1/§4 checklist items
        from bench_evidence_package §2 Session A.

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: hardware reference confirming the 5V/VSYS-USB VBUS tie (no diode) behind the
        binding exclusivity rule; the board's 5V pin, BOOT button, and onboard LED
        locations the operator needs at the bench.

- url: https://arduino-pico.readthedocs.io/en/latest/bootsel.html
  why: BOOT-button behavior notes: runtime presses never enter the bootloader
        (entry needs BOOTSEL during RESET) — safe to press freely during Session A.
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── README.md                      # fixed by P1.M1.T2.S1 — read-only here
├── platformio.ini                 # untouched
├── firmware/jug_filler/           # flashed already; NO edits this task
├── test/                          # untouched
├── docs/
│   ├── COMMISSIONING.md           # read-only here (swept later by P1.M2.T3.S2)
│   └── bench/
│       ├── README.md              # index — EDIT row A only
│       ├── template-session-A-dry-bench.md   # T1.S2 — fill IN PLACE
│       ├── template-session-{B..E}-*.md      # T1.S2 — do not touch
│       ├── template-rails.md                # T1.S2 — copy, do not fill in place
│       └── template-discrepancies.md        # T1.S2 — copy if discrepancies found
└── spec/SPEC.md                   # source of truth — NEVER edited (Mode A)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
docs/bench/
├── README.md                            # MODIFIED: row A finalized (only change)
├── template-session-A-dry-bench.md      # MODIFIED: filled in place = session record
├── YYYY-MM-DD_session-A-dry-bench.log   # NEW: raw capture + operator header
├── YYYY-MM-DD_rails.md                  # NEW: Stage-2 multimeter evidence
└── YYYY-MM-DD-discrepancies.md          # NEW only if mismatches occurred
# (YYYY-MM-DD = the real execution date)
```

### Known Gotchas of our codebase & Library Quirks

```bash
# CRITICAL — power exclusivity (hardware, binding): the RP2040-Zero 5V pin (VSYS)
#   ties DIRECTLY to USB VBUS with NO protective diode. USB + buck simultaneously
#   can damage the board/PC. Exactly ONE source at a time, enforced by the rails
#   sheet's three exclusivity ticks (tick them BEFORE measuring).

# CRITICAL — Phase R produces NO serial: buck-only with USB unplugged means the
#   banner/stream cannot exist there. The rails markdown IS the evidence. Do not
#   "helpfully" create a Phase R .log — that would be fabricated evidence.

# CRITICAL — dry-fire limit: each VALVE open < 5 s (solenoid overheats without
#   flow, SPEC §9.1). Technique: short-press BOOT to start, short-press again to
#   abort. 12 V may feed the MOSFET module's power input ALONE during dry-fire.

# CRITICAL — expected first-vs-later FILL lines differ BY DESIGN: dry-fire #1 after
#   power-up logs `FILL start backstop=30min recovery=0ms recmin=0` (fresh boot,
#   table bypassed); dry-fires #2–3 after an ABORT log `FILL start backstop=44min
#   recovery=<small>ms recmin=0` (recovery clock restarted, first-knot clamp {2,44}).
#   Both are PASS; a later 30min/recovery=0ms after an abort WOULD be a discrepancy.

# CRITICAL — ambient interleaving: `S1=<n> S2=<n>` (1 Hz) and BTN lines appear
#   between event lines in every capture. Order assertions apply to EVENT lines
#   only; filter with `grep -v '^S1='` before checking ordering.

# CRITICAL — BTN values must respect the classification (BUG-002 lesson): a
#   `BTN short <n>ms` line with n ≥ 1000 is a DISCREPANCY, likewise `BTN long`
#   with n < 1000. Hold semantics: events fire on RELEASE only; silence while held.

# GOTCHA — capture BEFORE power/reset: the banner must be the first stream line;
#   if the board was already powered, press RESET (not re-plug) after capture
#   starts. RESET also lands POWER_OR_RESET — banner expectation unchanged.

# GOTCHA — sensor counts "not pinned at 0/4095": sensors are dry in air; a count
#   frozen at exactly 0 or 4095 on every line indicates a wiring fault → tick N,
#   record in discrepancies, do NOT proceed to sessions B/C.

# GOTCHA — template fill discipline (T1.S2 contract): session-A template is filled
#   IN PLACE; template-rails.md and template-discrepancies.md are COPIED to the
#   date-stamped filenames then filled. Filling template-rails in place breaks
#   reuse for future rig re-checks.

# GOTCHA — never press-and-hold BOOT while (re)plugging power: BOOTSEL held during
#   RESET enters the USB bootloader and the sketch never runs. Runtime presses are
#   safe (release before reset).

# BLOCKED protocol: if the rig/hardware is unavailable, set row A Status to
#   `BLOCKED: <reason>` (real reason, e.g. "rig not assembled — no RP2040-Zero on
#   bench") and create NO log/rails files. Never synthesize capture contents —
#   the integrity rule (README §1) is the whole point of this changeset.
```

## Implementation Blueprint

### Data models and structure

No code. Evidence artifacts only, following the T1.S2 template structures exactly (session record table, checklist checkboxes `— [ ]`/`— [x]`, rails measurement fields, header block).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: PRECONDITIONS (software, before touching hardware)
  - RUN: ls docs/bench/README.md docs/bench/template-session-A-dry-bench.md \
         docs/bench/template-rails.md
  - EXPECT: all exist (T1.S1 + T1.S2 landed). If a template is MISSING → STOP and
    report (T1.S2 incomplete); do NOT author templates yourself.
  - RUN: pio run -e pico        # compile-only [SUCCESS] — confirms the flashed
    tree still builds; NO reflash, NO upload (item input: board already flashed).
  - RUN: git rev-parse --short HEAD   # the FW git rev for the header + index row.

Task 1: PHASE R — Stage-2 rail check (buck-only, USB unplugged)
  - COPY: cp docs/bench/template-rails.md docs/bench/$(date +%F)_rails.md
  - DO (operator): (1) unplug USB from the board entirely; (2) tick the three
    source-exclusivity boxes; (3) with the buck's 5 V output DISCONNECTED from the
    board, power the buck from 12 V and adjust until the meter reads 4.9–5.1 V;
    record; (4) connect the buck 5 V to the board's 5V pin (still no USB) and
    measure at that pin; record (4.9–5.1 V); (5) disconnect the buck's 5 V from
    the board and restore USB-only bench power for Phase A.
  - FILL: both readings, meter/supply identity, deviations, result PASS only if
    both in range; tick the restore-power box last.

Task 2: PHASE A — Session A dry bench (USB-only, buck 5 V disconnected)
  - CONFIGURE: sensors dry in air; optionally 12 V to the MOSFET module power
    input ALONE (board stays USB-powered).
  - START CAPTURE BEFORE POWER/RESET: 115200 baud, 8N1, save-to-file
    (e.g. `pio device monitor -b 115200 | tee docs/bench/$(date +%F)_session-A-dry-bench.log`
    or a terminal's raw save — but prepend the header block afterwards as the
    file's first lines, per README §3.3).
  - HEADER BLOCK (mandatory first lines, fill rev/date/deviations):
    # bench evidence — session A — dry bench check
    - date: YYYY-MM-DD
    - firmware: git <rev> (board build of <date>; native suite passing)
    - wiring: USB-powered, buck 5 V disconnected; sensors dry in air; valve dry
      (12 V on MOSFET module only) / dry-fire specifics
    - deviations: none | list
  - OBSERVE + TICK (template-session-A-dry-bench.md, in place):
    1. Power/reset → first stream line `JugFiller boot (reset cause: POWER_OR_RESET)`;
       external LED OFF in IDLE.
    2. `S1=<n> S2=<n>` streaming at ~1 Hz; values vary; NOT pinned at 0/4095.
    3. Quick BOOT tap → on RELEASE `BTN short <n>ms` (n < 1000). Hold ~1.5 s →
       on RELEASE `BTN long <n>ms` (n ≥ 1000). Silence while held.
    4. Dry-fire ×2–3 (each open <5 s): short-press → `FILL start backstop=30min
       recovery=0ms recmin=0` → `BASE ok s1=… s2=…` → `VALVE open budget=30min`
       (LED solid) → short-press again → `VALVE close` → `ABORT attemptms=… openms=…`
       (LED off). Later iterations show `backstop=44min recovery=<small>ms recmin=0`.
       Note valve clicks heard: Y/N per iteration.
  - STOP capture cleanly (Ctrl-C / end save), never edit captured lines.

Task 3: FINALIZE evidence
  - FILL: template-session-A-dry-bench.md session record (date, evidence log name,
    rails sheet name, FW git rev) + checklist pass/total.
  - EDIT docs/bench/README.md row A ONLY: Date, evidence filenames (both), FW git
    rev, `pass/total`, Status (PASS | PARTIAL(n✗) | BLOCKED: reason).
  - IF any mismatch: cp template-discrepancies.md → YYYY-MM-DD-discrepancies.md,
    append expected-vs-observed entries with log excerpts (D-1 pre-seeded stays).
  - RUN: Level 4 verification gates below; fix bookkeeping (never the capture).

Task 4: BLOCKED branch (only if the rig is unavailable)
  - SKIP Tasks 1–3 entirely (no evidence files created).
  - EDIT row A Status → `BLOCKED: <real reason>`; leave other cells `—`.
  - REPORT: state the reason plainly; do not pre-create empty files.
```

### Implementation Patterns & Key Details

```bash
# Ordered-prefix verification per dry-fire (event lines only; ambient filtered):
grep -vn '^S1=' docs/bench/$(date +%F)_session-A-dry-bench.log \
  | grep -nE 'FILL start|BASE ok|VALVE open|VALVE close|ABORT|BTN|JugFiller boot'
# → line numbers of each FILL start → BASE ok → VALVE open → VALVE close → ABORT
#   run must be strictly increasing; exactly one banner, before everything.

# Open-duration bound: at most 4 ambient 1 Hz S1= lines between a VALVE open and
#   its VALVE close (<5 s); or use the capture's wall-clock if the terminal adds
#   timestamps (record which method in deviations if it matters).

# Sensor sanity (not pinned): awk over S1= values must show >1 distinct value and
#   no constant 0/4095 run — see Level 4 gate (3).
```

### Integration Points

```yaml
DOCS (the only integration surface):
  - docs/bench/README.md: row A finalized; status vocabulary is CLOSED (README §2).
  - template-session-A-dry-bench.md: becomes the durable session record.
  - Downstream contract: P1.M2.T2.S2 (Sessions B/C) starts only from row A = PASS.
FIRMWARE: none — no edits, no reflash (verify only: pio run -e pico).
SPEC: spec/SPEC.md is the source of truth and is NOT edited (Mode A).
CONFIG/BUILD: none.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Evidence-file "syntax": names, header, and tables well-formed
ls docs/bench/$(date +%F)_rails.md docs/bench/$(date +%F)_session-A-dry-bench.log
head -8 docs/bench/$(date +%F)_session-A-dry-bench.log   # header block present, rev filled
grep -c '^| A |' docs/bench/README.md                    # exactly 1 row A
# Expected: files exist (or BLOCKED branch taken), header complete, one row A.
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
#  M docs/bench/template-session-A-dry-bench.md
# ?? docs/bench/<date>_session-A-dry-bench.log
# ?? docs/bench/<date>_rails.md
# (?? docs/bench/<date>-discrepancies.md only if discrepancies were recorded)
# Nothing under firmware/, test/, spec/, platformio.ini, README.md, COMMISSIONING.md.
```

### Level 4: Creative & Domain-Specific Validation

```bash
L=docs/bench/$(date +%F)_session-A-dry-bench.log
# (1) banner once, first stream line after the header, correct cause:
grep -c '^JugFiller boot (reset cause: POWER_OR_RESET)$' "$L"        # 1
grep -ci watchdog "$L" || echo "no-watchdog OK"                      # 0 hits
# (2) button events on release, values respecting classification:
grep -E '^BTN short [0-9]+ms$' "$L" | awk '$2 ~ /short/ {v=$2; gsub(/[^0-9]/,"",v); if (v+0>=1000) exit 1}' && echo "SHORT-VALUES OK"
grep -E '^BTN long [0-9]+ms$'  "$L" | awk '{v=$2; gsub(/[^0-9]/,"",v); if (v+0<1000) exit 1}'  && echo "LONG-VALUES OK"
# (3) 1 Hz ambient present, values varying, never pinned:
grep -c '^S1=[0-9]* S2=[0-9]*$' "$L"                                 # >= ~30 (30 s stream)
grep -oE '^S1=[0-9]+' "$L" | sort -u | wc -l                         # >= 2 distinct
# (4) per-dry-fire ordered prefixes (event lines, ambient filtered) — run the
#     Blueprint grep and eyeball 2–3 monotone FILL→BASE→VALVE open→VALVE close→ABORT runs
# (5) rails sheet: both readings parse into [4.9, 5.1], exclusivity ticks checked:
grep -E '^\s*- Reading: `?[0-9.]+ V' docs/bench/$(date +%F)_rails.md
grep -c '\[x\]' docs/bench/$(date +%F)_rails.md                      # >= 4 ticks
# (6) index row A carries a real date/rev and an allowed status token:
grep -E '^\| A \|' docs/bench/README.md | grep -vE 'PENDING|—'       # filled row
# (7) integrity: capture is genuine — if row A is BLOCKED, NO log/rails files exist
ls docs/bench/*_session-A-dry-bench.log 2>/dev/null || echo "BLOCKED-consistency: no fabricated files OK"
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (or BLOCKED branch consistent: row marked, no files, no fabrication)
- [ ] `pio run -e pico` `[SUCCESS]`; firmware/test trees byte-identical (`git status` scope exact)

### Feature Validation

- [ ] Rails: two readings 4.9–5.1 V recorded; three exclusivity ticks checked; power restored USB-only afterward
- [ ] Session A checklist: banner POWER_OR_RESET + LED off; 1 Hz non-pinned S1/S2; BTN short (<1000) and long (≥1000) on release; dry-fires ×2–3 with ordered prefixes and <5 s opens; LED solid during FILLING/off after abort; clicks Y/N noted
- [ ] Later dry-fires show the restarted-recovery form (44min/small recovery) without being flagged wrong
- [ ] Index row A finalized with pass/total and allowed status; template-A filled in place

### Code Quality Validation

- [ ] Fill discipline honored: template-A in place; rails/discrepancies copied to date-stamped files; B–E templates untouched
- [ ] Header block complete (date, git rev, wiring, deviations) as the log's first lines
- [ ] No edits to spec/SPEC.md, README.md (root), COMMISSIONING.md, firmware/, test/

### Documentation & Deployment

- [ ] Evidence filenames date-stamped per README §4 conventions; discrepancies ledger updated if any mismatch
- [ ] Row A result legible to P1.M2.T2.S2 (the next session's entry gate)

---

## Anti-Patterns to Avoid

- ❌ Never connect USB and the buck's 5 V to the board simultaneously — VSYS ties to USB VBUS with no diode; exclusivity ticks exist to enforce this
- ❌ Never let a dry-fire exceed 5 s open — abort with a second short-press promptly
- ❌ Never create a Phase-R serial log or synthesize/edit capture contents — genuine evidence or BLOCKED, nothing else
- ❌ Never reword captured lines, trim the stream, or "clean up" the log — header is prepended, body untouched
- ❌ Never mark `BTN short` with n ≥ 1000 (or long < 1000) as a pass — that's a real discrepancy (BUG-002 lesson)
- ❌ Don't flag the second/third dry-fire's `backstop=44min recovery=<small>ms` as wrong — recovery-clock restart is spec behavior
- ❌ Don't fill template-rails.md in place — copy it; don't fill templates B–E at all
- ❌ Don't edit spec/SPEC.md to match observations — discrepancies go to the ledger (Mode A)
- ❌ Don't reflash firmware or "fix" anything in firmware/test if a gate fails — record the mismatch; fixes are a human decision
- ❌ Don't leave row A PENDING after execution — PASS, PARTIAL(n✗), or BLOCKED: reason, always

---

**Confidence Score: 9/10** — every input contract was read from disk this session (README index/procedure, T1.S2 template listings incl. the rails sheet and prefix table, evidence-package power rules and Session A checklist); the PRP carries the complete runbook, byte-exact expected lines (including the intentional first-vs-later FILL-line difference), the BLOCKED integrity branch, and machine-checkable post-capture gates. The single inherent risk is physical rig availability, which the task's own contract handles via BLOCKED — not a PRP defect.
