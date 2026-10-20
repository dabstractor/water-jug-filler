# PRP — P2.M1.T2.S1: Author docs/COMMISSIONING.md calibration runbook

## Goal

**Feature Goal**: Create `docs/COMMISSIONING.md` — a complete, self-contained commissioning & calibration runbook that lets a **non-author operator** (human or bench-capable agent) execute PRD spec §9 end-to-end: every step mapped to the exact serial log lines to watch (final P2.M1.T1.S1 formats), the exact `config.h` constant to edit (group + name + expected magnitude), and the power-safety preconditions (never USB+buck; buck preset 4.9–5.1 V verified before first buck-powered boot; dry-fire <5 s rule).

**Deliverable**: One new file `docs/COMMISSIONING.md` (Mode A — changeset-scoped doc riding with this task). **No firmware, test, config, README, or build-file changes** (`git diff --stat` shows exactly one path).

**Success Definition**: A reader who has never seen this repo can flash the firmware, run §9.1–§9.5 in order, know from the log stream which numbers to write down, compute and edit `BASELINE_MIN`/`BASELINE_MAX`/`TRIP_DROP_COUNTS` (§9.3) and `BACKSTOP_TABLE_MIN` (§9.5) correctly (concave shape preserved), and verify the result — using only this runbook plus the README. Every config constant and log-line literal quoted in the doc cross-checks green against `config.h` and the normative log-format table.

## User Persona

**Target User**: A bench operator who did NOT write the firmware — a lab tech, a friend of the author, or a hardware-attached agent. Per the P1.M7.T2.S1 failure record, the build host has no rig attached; the physical bench work will be done elsewhere by someone with hands, so the doc must carry ALL operator knowledge.

**Use Case**: First commissioning of a freshly assembled jug-filler board (RP2040-Zero + 2× HW-390 sensors on GP26/GP27, MOSFET module on GP15, green LED on GP14, valve + flyback diode, LM2596 buck, 12 V supply, multimeter, cup of water).

**User Journey**: read safety rules → bench-flash over USB (buck disconnected) → §9.1 dry bench check → §9.2 buck preset verification → §9.3 sensor calibration (record dry/wet counts → edit config.h group 5 → reflash → verify `BASE ok`) → §9.4 dry-run flows 1 & 2 with cup dips → §9.5 attended first fills (record durations vs recovery → edit `BACKSTOP_TABLE_MIN` → reflash) → completion checklist sign-off.

**Pain Points Addressed**: spec §9 tells you WHAT to calibrate but not HOW against the actual log stream; fault lines previously lacked observed values (fixed by P2.M1.T1.S1 — the runbook is the consumer of that enrichment); power-path mistakes (USB+buck together) can damage a PC port; a padded (non-concave) backstop table silently ruins mid-range fills (§6.2).

## Why

- PRD §9 defines commissioning as *reading logged values and setting constants*; the log stream IS the calibration instrument. P2.M1.T1.S1 (parallel, Implementing) completes log coverage — this task writes the human-facing procedure that consumes those final formats. Its PRP explicitly names this task as the downstream consumer: "Formats ready for P2.M1.T2.S1's `docs/COMMISSIONING.md` to reference verbatim."
- P1.M7.T2.S1 (bench validation) is blocked-pending-hardware: its issue record says the flashable UF2 is built and "the complete T2–T9 runbook … lives in the PRP" — burying operator procedure inside a plan PRP. This runbook moves that knowledge into the durable repo doc where the bench operator can actually use it.
- P2.M1.T3.S1 (README/doc sync, later) will link to this file; landing it now keeps the changeset docs coherent.

## What

`docs/COMMISSIONING.md` contains, in order:

1. **Purpose & audience** — one short paragraph; states prerequisites (assembled rig per spec §1/§2, multimeter, cup of water, USB cable, serial monitor at 115200) and that no firmware author knowledge is assumed.
2. **Safety rules box (MUST be prominent, before any procedure)** —
   - **Never connect USB while the buck powers the board** (RP2040-Zero 5 V pin = VSYS ties to USB VBUS with no diode — backfeed, PC port damage risk). Bench/commissioning = USB-powered with buck disconnected from the board; in service = buck-powered with USB unplugged; program over USB with buck disconnected (spec §2.2).
   - **Buck preset check**: before the board's FIRST buck-powered boot, measure the LM2596 output with a multimeter — must read **4.9–5.1 V** — with the buck feeding only the board's 5 V pin (12 V may feed the MOSFET module alone for valve tests).
   - **Dry-fire rule**: solenoid clicks <5 s each to avoid overheating (spec §9.1).
   - Keep WS2812 brightness ≤60/255 (already enforced by `RGB_BRIGHTNESS` — do not raise). Route sensor AOUT wiring away from valve wiring (noise) (spec §2.2).
3. **Stage 0 — Bench setup & flashing**: USB-only power (buck disconnected), flash (`pio run -e pico` then UF2 drag-drop, or Arduino IDE), open serial monitor at **115200**, expect `JugFiller boot (reset cause: …)`.
4. **Stage 1 (= §9.1) — Dry bench check**: verify IDLE (LED off, 1 Hz `S1=… S2=…` lines flowing); press BOOT → `BTN short …ms` (events fire on RELEASE, spec §5.1); dry-fire the valve: BOOT press → fill starts (`FILL start …`, `VALVE open budget=…min`) → second BOOT press within seconds → `ABORT attemptms=… openms=…` + `VALVE close` — each click **<5 s**. State the expected LED cues by pattern (FILLING solid, etc.).
5. **Stage 2 (= §9.2) — Power rails**: multimeter procedure (preset buck unloaded/with board, 4.9–5.1 V at the 5 V pin) and the USB/buck exclusivity rules restated as a checklist. **Explicitly note logs cannot show this** — when the buck powers the board, USB is unplugged so no serial stream exists (README documents this; bench record = multimeter reading written down by operator).
6. **Stage 3 (= §9.3) — Sensor calibration** (the core section):
   - Dry: sensors in open air → watch the 1 Hz `S1=… S2=…` line (runs in every state); record ~30 s of dry counts; **expect ≈3000–3900 counts** (12-bit, 3.3 V rail).
   - Trigger a baseline capture (BOOT press → `BASE ok s1=… s2=…`; if out of range instead `FAULT baseline sensor=N code=N count=…` — the `count=` is the observed value).
   - Wet: submerge tips to mount depth → read wet counts off the same `S1=/S2=` line.
   - **Edit config.h group 5** (`// ---- 5. ADC / sensors`): set `BASELINE_MIN`/`BASELINE_MAX` around the observed dry range; set `TRIP_DROP_COUNTS` ≈ **half** the observed dry→wet delta. Include a worked example (e.g. dry 3500, wet 900 → delta 2600 → `TRIP_DROP_COUNTS` ≈ 1300; `BASELINE_MIN` 3100, `BASELINE_MAX` 3900) and a fill-in table for the operator's own numbers.
   - Reflash → re-run a fill start → confirm `BASE ok s1= s2=` (no `FAULT baseline`).
   - Note remaining group-5 constants and when (if ever) to touch them (`DEBOUNCE_COUNT` only for flaky trips, etc.).
7. **Stage 4 (= §9.4) — Dry-run flows 1 & 2 with sensors dipped in a cup by hand** (spec §5.2): step-by-step for each flow with the exact expected log sequence and LED cue, e.g. Flow 2: press → `FILL start backstop=…min recovery=…ms recmin=…` → `BASE ok` → `VALVE open` → dip sensor 1 → `TRIP sensor=1 fillms=…` (CONFIRM window opens; `CONFIRM_WINDOW_S` = 12 s — dip the second sensor within it) → `CONFIRM pass sensor=2 durms=…` → `DONE attemptms=… openms=…` → green triple-pulse → press → `ACK done`. Flow 1: abort mid-fill → `ABORT … openms=…`, third press shows the **fresh recomputed timer** in the next `FILL start … recmin=…`. Also point out the guard-fault paths (`FAULT earlywet`/`FAULT earlytrip` with `fillms=`, `FAULT confirm … durms=`: means the dip came too early / too late — not a hardware failure).
8. **Stage 5 (= §9.5) — Attended first fills** → tune `BACKSTOP_TABLE_MIN`:
   - What to record per fill: `FILL start recovery=…ms recmin=…` (time since previous fill END) and `DONE openms=…` (actual valve-open duration). Do several fills at deliberately different recovery times (e.g. right after previous end, ~10 min, ~20 min, ≥60 min).
   - How to choose knots: for each recovery time, backstop = observed duration × safety margin (suggest ~1.3–1.5×, rounded up), then **preserve the concave shape** — steep fall at short recovery, flattening toward the full-tank value near long recovery; **never pad the mid-range** (a straight line pads a fill started ~15 min after the last one ends in exactly the wrong direction — spec §6.2). Give the worked table using the shipped defaults `{ {2,44},{10,41},{20,38},{35,34},{60,30} }` as the shape example, and show the exact C++ edit in group 3 (struct pairs `{recoveryMin, backstopMin}`, clamped both ends).
   - Note `FRESH_BOOT_BACKSTOP_MIN` = the fresh-boot/fresh-reset default (recovery=0ms bypasses the table), and `PLUS5_MIN` long-press stacking is visible as `PLUS5 budget=…min`.
   - Verification: after editing + reflashing, run one more attended fill and confirm it completes (`DONE`) with the budget shown in `VALVE open budget=…min`.
9. **Completion checklist** — sign-off table: dry counts, wet counts, chosen constants, buck voltage reading, flows 1/2 pass, first-fill table; final note that commissioned values live only in `config.h` (single source of truth, spec §8) and to commit them.

### Success Criteria

- [ ] `docs/COMMISSIONING.md` exists, covers §9.1–§9.5 as discrete stages plus a safety-rules box up front
- [ ] Every §9 step names the exact log lines to watch, using the FINAL P2.M1.T1.S1 formats verbatim (see Context)
- [ ] Every constant the operator edits is named exactly as in `config.h` with its group heading (`BASELINE_MIN`, `BASELINE_MAX`, `TRIP_DROP_COUNTS` group 5; `BACKSTOP_TABLE_MIN` group 3)
- [ ] Power-safety preconditions (never USB+buck, 4.9–5.1 V buck preset before first buck boot) and the <5 s dry-fire rule are prominent and precede all procedures
- [ ] §9.5 guidance preserves concavity and includes the §6.2 "never pad" rationale
- [ ] Includes worked examples with expected magnitudes (dry ≈3000–3900 counts; TRIP_DROP_COUNTS ≈ half delta; backstop knots with margin)
- [ ] `git diff --stat` shows ONLY `docs/COMMISSIONING.md`; `pio test -e native` still green (nothing else touched)

## All Needed Context

### Context Completeness Check

✅ Written from: live read of `firmware/jug_filler/config.h` (exact group comments and constant names), the parallel task P2.M1.T1.S1's PRP (final log-line contract — quoted in full below), spec/SPEC.md §2.2/§4/§5.2/§6.2/§9, README.md structure, and the P1.M7.T2.S1 issue record (bench blocked on hardware; runbook is the handoff artifact). No prior repo knowledge assumed below.

### Documentation & References

```yaml
- file: plan/001_53984f832581/P2M1T1S1/PRP.md
  why: CONTRACT for the final log-line formats this runbook quotes. Its "What"
    table lists every final format (append-only enrichment, e.g. 'FILL start
    backstop=%umin recovery=%lums recmin=%u', 'FAULT baseline sensor=%u
    code=%u count=%lu', 'FAULT backstop code=3 budget=%umin openms=%lums',
    'ABORT attemptms=%lu openms=%lu'). Quote these EXACTLY.
  gotcha: T1S1 is landing in parallel. If its README/.ino changes are not yet
    in the working tree when you write, source formats from its PRP table (the
    contract), NOT from the current .ino — the current lines lack recmin/count/
    openms fields. Never invent formats.

- file: firmware/jug_filler/config.h
  why: single source of truth for constant names, groups, units, defaults the
    runbook tells the operator to edit. Groups: '5. ADC / sensors' holds
    BASELINE_MIN=2000, BASELINE_MAX=4050, TRIP_DROP_COUNTS=350, DEBOUNCE_COUNT=8;
    '3. Fill backstop model' holds FRESH_BOOT_BACKSTOP_MIN=30, BACKSTOP_TABLE_MIN[]
    = { {2,44},{10,41},{20,38},{35,34},{60,30} } (BackstopTableEntry
    {recoveryMin, backstopMin}), PLUS5_MIN=5. Counts are 12-bit (0-4095, 3.3V≈4095).
  pattern: copy constant spellings and the group heading text verbatim into the doc.
  gotcha: file header says "Commissioning may retune ADC/sensor values (§9)" —
    the runbook's edits are exactly the sanctioned ones; do NOT tell the
    operator to edit pins, watchdog, or LED constants.

- file: spec/SPEC.md
  why: normative source for §9 (workflow), §2.2 (power cautions, 4.9-5.1V,
    USB/buck exclusivity), §6.2 (concave backstop rationale + default table),
    §5.2 (flows 1 & 2 definitions), §5.1 (button acts on release), §4 (LED
    pattern shapes: DONE green triple-pulse 3x200/200+1000, fault blink-code
    groups, +5 ack fast blink).
  gotcha: §4 contains the sentence "there is no external LED" — this
    CONTRADICTS the shipped firmware (config.h PIN_LED=GP14 external green LED,
    P1.M5.T2 complete, plus WS2812 mirror). Describe LED cues by PATTERN SHAPE
    (pulse counts, cadence, blink-code counts) which holds for both drivers;
    do not repeat that sentence.

- file: README.md
  why: sections "Serial logging" (normative log table once T1S1 lands),
    "Build with PlatformIO" (pio run -e pico), "Build in Arduino IDE",
    "Safety note" (line ~72). Link to these instead of duplicating; state
    serial monitor baud 115200 (SERIAL_BAUD).
  gotcha: do NOT edit README in this task (T1S1 owns the log table; T3.S1
    owns broader doc sync). This task adds docs/COMMISSIONING.md only.

- file: plan/001_53984f832581/P1M7T2S1/issue_feedback.md
  why: documents the bench-blocked state and the physical actions the operator
    must perform (BOOTSEL hold + UF2 copy, BOOT presses, water dips inside the
    12s confirm window, multimeter rail check, <5s dry-fire clicks). Align
    terminology with that record; the runbook is the durable version of the
    procedure its PRP carried.

- file: plan/001_53984f832581/P2M1T2S1/research/notes.md
  why: full audit trail for this PRP: final log formats, config.h facts,
    spec facts, LED-contract discrepancy note, validation approach.

- url: https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html  (background only, optional)
  why: RP2040 watchdog ceiling 8.388s and BOOTSEL behavior context if the doc
    wants a one-line "why" for the watchdog. Not required — spec already states it.
```

### Current Codebase tree (relevant excerpt)

```bash
water-jug-filler/
├── firmware/jug_filler/
│   ├── jug_filler.ino        # firmware (1672 lines; pure region + ARDUINO region)
│   └── config.h              # §8 tunables — the file the operator edits
├── docs/                     # DOES NOT EXIST YET — you create it
├── test/                     # 17 native Unity suites
├── platformio.ini
├── README.md                 # build + serial-logging + safety sections
└── spec/SPEC.md              # PRD (read-only)
```

### Desired Codebase tree with files to be added

```bash
docs/
└── COMMISSIONING.md          # NEW — the calibration runbook (only artifact of this task)
```

### Known Gotchas of our codebase & Library Quirks

```text
CRITICAL: formats move fast, doc must not drift. Source every quoted log line
  from the T1S1 final-format contract (its PRP "What" table) — append-only
  fields (recmin=, count=, openms=, fillms=, durms=, budget=) are the whole
  point; quoting today's pre-enrichment lines would teach the operator to
  expect lines that will never print.

CRITICAL: this is a DOCS-ONLY task. Zero changes under firmware/, test/,
  platformio.ini, README.md, spec/. Validation includes a diff-scope gate.

GOTCHA: §9.2 is deliberately NOT a log-watching step — when the buck powers
  the board USB is unplugged, so no serial stream exists. The runbook must say
  this explicitly (operator writes the multimeter reading into the sign-off
  table) or they will hunt for a log line that cannot appear.

GOTCHA: recovery=0ms / recmin=0 in 'FILL start' means FRESH BOOT (table
  bypassed; budget came from FRESH_BOOT_BACKSTOP_MIN, not BACKSTOP_TABLE_MIN).
  A real button-driven recovery of exactly 0ms is impossible (40ms debounce).
  Document this — otherwise the operator misreads their first post-flash fill.

GOTCHA: BACKSTOP_TABLE_MIN is C++ array-of-struct initializers
  ({recoveryMin, backstopMin} pairs, ascending recovery, clamped both ends).
  The doc's edit instructions must show valid syntax and say knots must stay
  ascending in recoveryMin; the backstop column must stay CONCAVE (falling,
  steep-to-shallow). Never a straight line, never padded (spec §6.2).

GOTCHA: LED contract discrepancy — spec §4 says "no external LED" but
  firmware ships GP14 external LED + WS2812 mirror (USE_ONBOARD_RGB=true).
  Describe patterns by shape; mention both indicators show the same pattern.

GOTCHA: sensor id mapping — logs print 1-based sensors (sensor=1 / sensor=2 =
  GP26 / GP27 = sensor 1 / sensor 2). Fault code 3 = backstop timeout; codes
  1/2 = the failing sensor. Keep the doc consistent with this.

GOTCHA: dry-fire in this firmware = BOOT press (fill starts, valve opens) then
  a second BOOT press (abort closes valve). There is no dedicated 'click'
  command — the runbook must spell out this two-press technique with the <5s
  budget per open.
```

## Implementation Blueprint

### Data models and structure

None — documentation only. The doc's "data model" is its structure: safety box → stage 0..5 (mirroring §9.1–§9.5) → completion sign-off table. Keep each stage in the same shape: **Goal → Setup → Do (numbered) → Watch (exact log lines) → Record (table) → Edit (config.h group + constant + expected magnitude) → Verify**.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: CREATE docs/COMMISSIONING.md — skeleton + safety box + stage 0/1/2
  - WRITE: title, purpose/audience paragraph, prerequisites list (rig, multimeter,
    cup, USB cable, serial monitor @115200)
  - WRITE: safety rules box FIRST (never USB+buck; buck preset 4.9-5.1V measured
    at the 5V pin before first buck-powered boot; dry-fire <5s; WS2812 <=60/255
    note; sensor-wiring routing note) — all sourced from spec §2.2/§9.1
  - WRITE: Stage 0 (bench flash over USB, buck disconnected, expect boot banner)
    and Stage 1 (§9.1 dry bench: IDLE liveness via 1Hz 'S1= S2=', 'BTN short|long
    <ms>ms' on release, dry-fire two-press technique, <5s per open,
    'FILL start/VALVE open/ABORT ... openms=/VALVE close' expected sequence)
  - WRITE: Stage 2 (§9.2): multimeter procedure + exclusivity checklist + the
    explicit 'logs cannot show this (USB unplugged)' note
  - NAMING: section headings 'Stage N — <name> (spec §9.X)'

Task 2: EXTEND docs/COMMISSIONING.md — Stage 3 (§9.3 sensor calibration)
  - WRITE: dry capture in open air from the 1Hz line (expect ~3000-3900 counts);
    baseline verdict lines ('BASE ok s1= s2=' / 'FAULT baseline sensor= code=
    count=' with count= = observed value)
  - WRITE: wet capture by dipping to mount depth (same 1Hz line, runs in all states)
  - WRITE: the edit: config.h group 5 — BASELINE_MIN/MAX around observed dry
    range, TRIP_DROP_COUNTS = ~half the dry->wet delta; worked example + blank
    record table; reflash + verify via 'BASE ok'
  - INCLUDE: one-paragraph note on the other group-5 constants (settle/samples/
    debounce — defaults are fine unless symptoms)

Task 3: EXTEND docs/COMMISSIONING.md — Stage 4 (§9.4 flows 1 & 2)
  - WRITE: step-by-step for Flow 1 (abort: press->fill->press->ABORT->press->
    fresh recomputed timer visible in next 'FILL start ... recmin=') and
    Flow 2 (success: press->BASE ok->VALVE open->dip s1->'TRIP sensor=1
    fillms='->dip s2 within CONFIRM_WINDOW_S=12s->'CONFIRM pass sensor=2
    durms='->'DONE attemptms= openms='->green triple-pulse->press->'ACK done')
  - WRITE: LED cue per step (pattern shape) and the guard-fault outcomes
    ('FAULT earlywet/earlytrip ... fillms=', 'FAULT confirm ... durms=' =
    dip timing, not broken hardware)
  - SOURCE: spec §5.2 definitions verbatim in intent; timings from config.h

Task 4: EXTEND docs/COMMISSIONING.md — Stage 5 (§9.5) + completion checklist
  - WRITE: record per fill: 'FILL start recovery=ms recmin=' + 'DONE openms=';
    sample recoveries spread (immediate, ~10min, ~20min, >=60min)
  - WRITE: knot-choosing method (duration x1.3-1.5 margin, round up), concave
    shape requirement with the §6.2 'never pad the mid-range' rationale
    (straight line wrong for a fill started ~15min after last end), exact C++
    array edit in group 3 shown with the shipped defaults as shape example,
    ascending-recoveryMin requirement, clamp behavior at both ends
  - WRITE: FRESH_BOOT_BACKSTOP_MIN (recovery=0ms => fresh boot, table bypassed)
    and PLUS5_MIN stacking ('PLUS5 budget=' lines) notes; post-edit verification
    (one attended fill reaches DONE)
  - WRITE: completion sign-off table (dry counts, wet counts, chosen constants,
    buck volts, flows 1/2, first-fill table) + 'commissioned values live in
    config.h — commit them' closing note

Task 5: VALIDATE all levels below; confirm git diff --stat == docs/COMMISSIONING.md only
```

### Implementation Patterns & Key Details

```markdown
<!-- Per-stage shape (repeat for stages 1-5) — keeps a non-author on rails:
## Stage 3 — Sensor calibration (spec §9.3)
**Goal / You will need / Steps (numbered) /**
**Watch for** (exact lines, monospace):
    S1=3487 S2=3512          <- 1 Hz, every state; record ~30 s dry
    BASE ok s1=3489 s2=3510  <- baseline verdict at fill start
**Record** (table: | quantity | sensor 1 | sensor 2 |)
**Edit** `firmware/jug_filler/config.h`, group `5. ADC / sensors`:
    BASELINE_MIN  = 3100   // just under observed dry minimum
    BASELINE_MAX  = 3900   // just over observed dry maximum
    TRIP_DROP_COUNTS = 1300  // ~half of (dry ~3500 - wet ~900)
**Verify**: reflash, start a fill, expect `BASE ok s1= s2=` (no FAULT baseline)
-->
```

```text
PATTERN: quote log lines in fenced/monospace blocks exactly as the T1S1
  contract prints them; put operator-fillable numbers as placeholders ONLY
  where the firmware prints values (S1=____), never inside format keywords.

PATTERN: every 'Edit' block names the file path + group heading text copied
  from config.h + the constant in code formatting + units + expected magnitude.

CRITICAL: the backstop edit example must remain CONCAVE — if your worked
  numbers came out near-linear, adjust the margin per knot (bigger margin at
  short recovery) and say why.
```

### Integration Points

```yaml
DOCS: docs/COMMISSIONING.md is NEW; README.md untouched here — P2.M1.T3.S1
  (later) adds the link from README to this runbook; T1S1's README log table
  is the cross-reference target for 'Watch for' lines.
FIRMWARE: none. config.h values referenced read-only in the doc text.
BUILD: none. platformio.ini untouched.
DOWNSTREAM: P2.M1.T3.S1 doc sync links to this file; the bench operator
  (P1.M7.T2.S1 re-dispatch) executes it on hardware.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Structural sanity of the new doc (headings present, in order, no broken fences)
grep -n '^#' docs/COMMISSIONING.md
python3 - <<'EOF'
import re,sys
t=open('docs/COMMISSIONING.md').read()
fences=t.count('```'); assert fences%2==0, 'unbalanced code fences'
for s in ['§9.1','§9.2','§9.3','§9.4','§9.5','4.9','5.1','<5 s','BASELINE_MIN','BASELINE_MAX','TRIP_DROP_COUNTS','BACKSTOP_TABLE_MIN']:
    assert s in t, f'missing: {s}'
print('OK')
EOF
# Expected: all five §9 stages, safety values, and the four calibratable
# constants present; fences balanced.
```

### Level 2: Cross-checks (contract conformance)

```bash
# Every config constant named in the doc exists verbatim in config.h
grep -oE '\b(BASELINE_MIN|BASELINE_MAX|TRIP_DROP_COUNTS|DEBOUNCE_COUNT|BACKSTOP_TABLE_MIN|FRESH_BOOT_BACKSTOP_MIN|PLUS5_MIN|CONFIRM_WINDOW_S|SERIAL_BAUD|RGB_BRIGHTNESS)\b' docs/COMMISSIONING.md | sort -u | while read c; do grep -q "$c" firmware/jug_filler/config.h || echo "MISSING IN config.h: $c"; done; echo done

# Quoted log lines match the normative surface: check the format keywords the
# doc quotes appear in README's table (once T1S1 landed) or, if not yet
# landed, in the T1S1 PRP contract table.
for kw in 'FILL start' 'BASE ok' 'FAULT baseline' 'VALVE open' 'TRIP sensor' 'CONFIRM pass' 'DONE attemptms' 'ABORT attemptms' 'FAULT backstop' 'BTN ' 'recmin'; do grep -q "$kw" docs/COMMISSIONING.md || echo "doc never mentions: $kw"; done; echo done

# No stale/pre-enrichment formats taught: the doc must not present fault lines
# WITHOUT the enrichment fields as the thing to expect (grep should find the
# enriched spellings)
grep -c 'count=' docs/COMMISSIONING.md      # >=1 (FAULT baseline)
grep -c 'openms=' docs/COMMISSIONING.md     # >=3 (DONE/ABORT/FAULT backstop)
# Expected: no MISSING lines; counts met.
```

### Level 3: Integration (repo-level gates)

```bash
# Nothing but the doc changed — the hard scope gate for this task
git diff --stat
# Expected: exactly docs/COMMISSIONING.md (new file)

# Firmware regression sanity: code untouched, tests still green
pio test -e native 2>&1 | tail -3
# Expected: all suites PASS (192+ tests)

# Doc renders (no pathological markdown): quick lint via python markdown parse
python3 -c "import markdown; markdown.markdown(open('docs/COMMISSIONING.md').read(), extensions=['tables']); print('renders OK')" 2>/dev/null || echo "markdown module absent — visual check instead"
```

### Level 4: Domain review (human-level pass)

```bash
# Narrative walkthrough — read the doc top to bottom and verify:
#  1) A stranger can do §9.1->§9.5 without asking questions
#  2) Safety box precedes all procedures; USB/buck exclusivity stated >= twice
#  3) Every 'Watch for' line appears verbatim in the README log table (T1S1)
#  4) The backstop worked example is concave (steep short-recovery fall,
#     flattening long-recovery) and cites the never-pad rationale
#  5) No instruction edits pins/watchdog/LED constants (only group 3 + group 5
#     calibration values)
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–3 commands all green; fences balanced; all §9 stages present
- [ ] Every constant named in doc exists in `firmware/jug_filler/config.h`
- [ ] Every quoted log line matches the T1S1 final-format contract (enriched fields present)
- [ ] `git diff --stat` == `docs/COMMISSIONING.md` only; `pio test -e native` still green

### Feature Validation

- [ ] Covers §9.1–§9.5 as ordered stages + stage 0 (flash/setup) + completion sign-off
- [ ] Power-safety preconditions prominent: never USB+buck; buck 4.9–5.1 V multimeter check before first buck-powered boot; §9.2 not-loggable note
- [ ] Dry-fire <5 s rule stated with the two-press technique
- [ ] §9.3: dry ~3000–3900 counts expectation; BASELINE_MIN/MAX around observed dry; TRIP_DROP_COUNTS ≈ half dry→wet delta; worked example + record table
- [ ] §9.5: durations vs recovery recorded from `DONE openms` / `FILL start recovery= recmin=`; BACKSTOP_TABLE_MIN edit with concave shape preserved and §6.2 never-pad rationale; fresh-boot & +5 notes
- [ ] All success criteria from "What" section met

### Code Quality Validation

- [ ] Markdown conventions match repo docs (headings, tables, fenced blocks)
- [ ] No duplication of README content beyond necessity — links where possible
- [ ] No contradictions with spec §2.2/§4/§5.2/§6.2/§8/§9 (LED discrepancy handled by pattern-shape wording)
- [ ] Placeholders clearly marked for operator-entered values

### Documentation & Deployment

- [ ] The doc is self-contained for a non-author (persona test)
- [ ] Constants quoted with units and expected magnitudes
- [ ] Sign-off table present; closing note: commissioned values live in config.h, commit them

## Anti-Patterns to Avoid

- ❌ Don't quote pre-enrichment log formats (missing `recmin=`/`count=`/`openms=`) — the operator would watch for lines that never print
- ❌ Don't turn the runbook into a spec rewrite — it references spec sections; it doesn't replace them
- ❌ Don't instruct edits to anything beyond group 5 sensor constants and group 3 backstop knots (pins/watchdog/LED timings are frozen)
- ❌ Don't suggest a straight-line or padded backstop table — concavity is a physics requirement (§6.2), not a style preference
- ❌ Don't bury the power-safety rules mid-document — they come first
- ❌ Don't touch firmware/tests/README/platformio.ini — docs-only task, diff-scope gate enforces it
- ❌ Don't fabricate observed calibration values as if they were real bench data — worked examples must be labeled as examples

---

**Confidence Score: 9/10** — the doc's content is fully determined by three in-repo contracts (T1S1 final log formats, config.h exact constants, spec §2.2/§4/§5.2/§6.2/§9), all read live during research; validation is mechanical (grep cross-checks + diff-scope gate). Residual risks: T1S1 landing slightly different final wording than its PRP contract (mitigate: before writing Task 2+ sections, re-check `README.md` "Serial logging" table in the working tree and prefer it if enrichment has landed), and the spec §4 external-LED contradiction (mitigated by pattern-shape wording rule).
