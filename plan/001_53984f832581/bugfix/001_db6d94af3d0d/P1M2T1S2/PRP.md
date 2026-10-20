# PRP — P1.M2.T1.S2: Author per-session evidence templates (A–E), rails sheet, and discrepancy list template

## Goal

**Feature Goal**: Author the `docs/bench/` per-session evidence template set (BUG-003 scaffolding, the software-preparable half of bench_evidence_package.md §4): one checklist template per session A–E whose body is the ordered expected-log-prefix checklist with pass/fail checkboxes, LED-observation lines, and critical assertions; a rails sheet template (two multimeter fields + source-exclusivity ticks); and a discrepancies template (expected-vs-observed, verbatim log excerpt, follow-up issue #). Pure static markdown authoring — no hardware, no firmware, no code.

**Deliverable**: SEVEN new files in `docs/bench/` (complete verbatim content in the Task listings below):
`template-session-A-dry-bench.md`, `template-session-B-flow1-abort.md`, `template-session-C-flow2-success.md`, `template-session-D-fault-paths.md`, `template-session-E-watchdog.md`, `template-rails.md`, `template-discrepancies.md`.

**Success Definition**: All Level-4 grep gates pass (copy-exact prefixes present, critical assertions present, no checked boxes, no fabricated dates, filename grammar byte-matches the README index); `git status --porcelain` shows nothing outside `docs/bench/`; `docs/bench/README.md` (P1.M2.T1.S1's deliverable) untouched.

## User Persona

**Target User**: The bench operator executing sessions A–E (P1.M2.T2.S1–S5). Each template is their per-session worksheet: what power state to set up, which log lines to expect in what order, what to observe on the LED, which assertions are pass/fail critical, and where to record deviations.

**Use Case**: Operator opens the session template, works the ordered checklist while watching the 115200 capture, ticks `- [ ]` boxes, fills `____` blanks with observed values, then updates the README index row and files mismatches in the day's discrepancies file.

**User Journey**: templates ship PENDING/unchecked → operator runs session → ticks boxes in place (sessions A–E) or copies to the date-stamped file and fills (rails, discrepancies) → P1.M2.T2.S5 consolidates.

**Pain Points Addressed**: Without pinned copy-exact prefixes and ordered checklists, "evidence" degrades to eyeballing; the two critical assertions (B restart ≠ `recovery=0ms`; D1 no `VALVE open` before `FAULT baseline`) are exactly the failure modes a casual observer would miss.

## Why

- BUG-003 / bench_evidence_package.md §4 explicitly assigns this as preparable-without-hardware: "per-session evidence templates with expected-prefix checklists and pass/fail checkboxes … discrepancy-list template." The physical captures are P1.M2.T2's; this task defines their format, consumed verbatim by P1.M2.T2.S1–S5.
- Log formats are APPEND-ONLY/prefix-anchored (jug_filler.ino:1343–1344), so evidence greps anchor on exact prefixes — the templates MUST carry the prefixes byte-exact or downstream verification breaks.
- Upstream contract: P1.M2.T1.S1's `docs/bench/README.md` (index, capture procedure, integrity rule, filename grammar, status vocabulary). Its README states "the per-checklist detail lives in the session's evidence template" — these templates ARE that detail and must match its conventions exactly.

## What

Seven markdown files per the verbatim listings in the blueprint. Conventions used throughout:
- Expected log lines in backticks, **copy-exact prefixes**; `…` elides variable numerics inside expected lines; `____` (4 underscores) marks operator-fill blanks; values are pinned only where firmware/config pins them (Session B/A fresh boot: `backstop=30min recovery=0ms recmin=0`; Session B restart: `backstop=44min`).
- Session templates A–E are **filled in place** (persistent checklist records — per S1 README, the index summarizes them as pass/total). `template-rails.md` and `template-discrepancies.md` are **copied to the date-stamped evidence files** (`YYYY-MM-DD_rails.md`, `YYYY-MM-DD-discrepancies.md`) at execution, because bench_evidence_package.md §3 lists those as date-stamped files.
- Everything ships PENDING / `- [ ]` / `—` placeholders; the discrepancy template pre-seeds exactly one KNOWN nuance (bench package §6, GP14-vs-WS2812) as "to confirm at bench".

**Scope boundaries**: do NOT create or modify `docs/bench/README.md` (S1's, in flight), any date-stamped evidence file (no fabrication), firmware/spec/config/README/COMMISSIONING, or plan files.

### Success Criteria

- [ ] 7 template files exist in `docs/bench/` with exactly the Task listing content
- [ ] Every expected-prefix string is byte-exact vs the canonical table below
- [ ] Session B template contains BOTH critical assertions (restart NOT `recovery=0ms`; table value not fresh-boot 30 min)
- [ ] Session D template contains the critical assertion: no `VALVE open` before `FAULT baseline`
- [ ] Rails template has both multimeter fields (4.9–5.1 V) and ≥3 source-exclusivity ticks
- [ ] Discrepancy template has expected/observed/log-excerpt/follow-up-issue fields + pre-seeded D-1 GP14 nuance
- [ ] No `- [x]` boxes, no real dates (only `YYYY-MM-DD`), all statuses PENDING
- [ ] Filename grammar references byte-match S1's README (`YYYY-MM-DD_session-X-<slug>.log`, `YYYY-MM-DD_rails.md`, `YYYY-MM-DD-discrepancies.md`)
- [ ] `git status --porcelain` → nothing outside `docs/bench/`

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: every file's complete content is given verbatim below; every expected line is traceable to the verified prefix table (grepped from jug_filler.ino this tree) or to bench_evidence_package.md §2; validation is mechanical grep. External research: not applicable — static markdown authored against internal binding contracts; no libraries.

### Documentation & References

```yaml
- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  why: THE contract — §1 power rules, §2 sessions A–E (ordered prefixes, LED observations,
        critical assertions), §3 layout/filename grammar, §5 watchdog-hang mechanism (E),
        §6 pre-seeded discrepancy nuance (GP14 vs WS2812).
  gotcha: §2's Session A line pins `recovery=0ms` for the FIRST dry-fire only; subsequent
        dry-fires have recovery>0 (abort restarts the clock) — template says so.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  section: "2. Log-line grammar"
  why: firmware source-of-truth formats; why greps are prefix-anchored and captures stay raw.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T1S1/PRP.md
  why: upstream sibling contract — README.md content (filename grammar, status vocabulary,
        header-block fields, power rule wording, "templates live alongside this index").
  gotcha: S1 runs IN PARALLEL — docs/bench/README.md may not exist yet; never create/edit it.

- file: spec/SPEC.md
  sections: "4" (LED contract), "5.2" (flows 1/2), "5.3" (reset clears history, no auto-start),
        "6.1/6.2" (recovery clock + table {≤2:44, 10:41, 20:38, 35:34, ≥60:30}), "7" (lifecycle,
        faults, codes 1/2/3, latch semantics), "9" (commissioning), "10" (watchdog invariants).

- file: firmware/jug_filler/jug_filler.ino   # READ-ONLY verification of prefixes
  lines: 85–88, 1343–1399 (formatFillLogEvent), 1685 (BTN), 1712 (S1=/S2=)

- file: firmware/jug_filler/config.h         # READ-ONLY
  why: constants cited in templates — LONG_PRESS 1000, FRESH_BOOT 30, table knots, CONFIRM 12 s,
        EARLY_WET 5 s, DEBOUNCE 8, WATCHDOG_TIMEOUT_S 8.

- file: docs/COMMISSIONING.md                # READ-ONLY
  lines: 190–221 (Stage 2 rails), 541–542 (the two rail-checklist field wordings).

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T1S2/research/notes.md
  why: full research digest — verified prefix table w/ line numbers, config facts, design decisions.
```

### Current Codebase tree

```bash
docs/
├── COMMISSIONING.md        # existing, read-only here
└── bench/
    └── README.md           # P1.M2.T1.S1's deliverable (parallel; exists when this task starts)
```

### Desired Codebase tree

```bash
docs/bench/
├── README.md                        # S1's — UNTOUCHED by this task
├── template-session-A-dry-bench.md  # NEW — dry bench checklist (SPEC §9.1, §4) + rails pointer
├── template-session-B-flow1-abort.md# NEW — flow 1 abort/restart (SPEC §9.4, §6.1)
├── template-session-C-flow2-success.md # NEW — flow 2 success (SPEC §9.4, §7)
├── template-session-D-fault-paths.md# NEW — fault paths (SPEC §7, §4)
├── template-session-E-watchdog.md  # NEW — optional watchdog check (SPEC §10)
├── template-rails.md               # NEW — Stage-2 multimeter sheet (SPEC §9.2)
└── template-discrepancies.md       # NEW — expected-vs-observed ledger
```

### Known Gotchas

```text
# CRITICAL: prefixes must be BYTE-EXACT vs the canonical table — greps anchor on them.
#   Watch: `FAULT backstop code=3 budget=…min openms=…ms` (openms carries an `ms` suffix);
#   `JugFiller boot (reset cause: POWER_OR_RESET)` (space after the colon).

# CRITICAL: `recovery=0ms` means FRESH BOOT (table bypassed → 30 min). After an abort the
#   recovery clock restarts, so Session B's restart must show a nonzero recovery and the
#   table value (44 min for ≤2 min recovery — first-knot clamp of {2,44}).

# CRITICAL: fault code == sensor number for baseline/earlywet/earlytrip/confirm (1 or 2);
#   code=3 is the timer/backstop fault. LED blink-group count N == fault code.

# Parallel work: S1 owns docs/bench/README.md. Precondition-check it; never write it.

# Fill discipline: session templates filled IN PLACE; rails/discrepancies COPIED to
#   date-stamped files (§3 layout lists only date-stamped .log for sessions).

# Template purity: no real dates, no [x], no invented prefixes. The ONLY pre-seeded content
#   is discrepancy entry D-1 (the bench-§6 GP14/WS2812 nuance, status "to confirm").

# `S1=<n> S2=<n>` 1 Hz ambient + BTN lines interleave with event lines in every capture —
#   order assertions apply to event lines (templates tell operators to grep -v '^S1=').
```

## Implementation Blueprint

### Canonical prefix table (verified against jug_filler.ino this tree — copy from here, never from memory)

| Exact format string | .ino line | Pinned example(s) used in templates |
|---|---|---|
| `JugFiller boot (reset cause: %s)` → `POWER_OR_RESET`/`WATCHDOG` | 86 | both variants |
| `FILL start backstop=%umin recovery=%lums recmin=%u` | 1356 | `FILL start backstop=30min recovery=0ms recmin=0`; `FILL start backstop=44min recovery=…ms recmin=0` |
| `BASE ok s1=%u s2=%u` | 1359 | `BASE ok s1=____ s2=____` |
| `FAULT baseline sensor=%u code=%u count=%lu` | 1361 | `FAULT baseline sensor=1 code=1 count=____` |
| `VALVE open budget=%umin` | 1364 | `VALVE open budget=30min` / `budget=44min` |
| `TRIP sensor=%u fillms=%lu` | 1366 | `TRIP sensor=1 fillms=____` |
| `CONFIRM pass sensor=%u durms=%lu` | 1369 | `CONFIRM pass sensor=2 durms=____` |
| `DONE attemptms=%lu openms=%lu` | 1373 | `DONE attemptms=____ openms=____` |
| `ABORT attemptms=%lu openms=%lu` | 1377 | `ABORT attemptms=____ openms=____` |
| `FAULT earlywet sensor=%u code=%u fillms=%lu` | 1380 | blanks |
| `FAULT earlytrip sensor=%u code=%u fillms=%lu` | 1383 | blanks |
| `FAULT backstop code=3 budget=%umin openms=%lums` | 1387 | blanks |
| `FAULT confirm sensor=%u code=%u durms=%lu` | 1390 | `FAULT confirm sensor=2 code=2 durms=____` |
| `VALVE close` (literal) | 1353 | — |
| `ACK done` (literal) | 1395 | — |
| `BTN %s %lums` | 1685 | `BTN short ____ms` (<1000), `BTN long ____ms` (≥1000) |
| `S1=%u S2=%u` (1 Hz ambient) | 1712 | `S1=____ S2=____` |

### Implementation Tasks

```yaml
Task 0: PRECONDITION (parallel-work guard)
  - RUN: ls docs/bench/README.md
  - EXPECT: exists (P1.M2.T1.S1 landed). If MISSING → STOP and report; do NOT create it.
  - VERIFY alignment: grep -c "YYYY-MM-DD_session-A-dry-bench.log" docs/bench/README.md  # 1

Task 1: CREATE docs/bench/template-session-A-dry-bench.md   (listing A below)
Task 2: CREATE docs/bench/template-session-B-flow1-abort.md (listing B)
Task 3: CREATE docs/bench/template-session-C-flow2-success.md (listing C)
Task 4: CREATE docs/bench/template-session-D-fault-paths.md (listing D)
Task 5: CREATE docs/bench/template-session-E-watchdog.md    (listing E)
Task 6: CREATE docs/bench/template-rails.md                 (listing R)
Task 7: CREATE docs/bench/template-discrepancies.md         (listing X)
Task 8: RUN Level-4 gates + git scope check. Fix and re-run until green.
```

### Listing A — `docs/bench/template-session-A-dry-bench.md`

````markdown
# Session A — Dry bench check (SPEC §9.1, §4)

Per-session evidence template. Fill this checklist **in place** as the session runs; the raw
capture goes to `YYYY-MM-DD_session-A-dry-bench.log` (capture procedure and operator header
block: docs/bench/README.md §3). Summarize the result in README index row A as pass/total.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-A-dry-bench.log` |
| Rails sheet | `YYYY-MM-DD_rails.md` (from `template-rails.md`) |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V output disconnected from the board (never both — README §3 rule 1).
- [ ] Serial capture started BEFORE power/reset: 115200 baud, 8N1, save-to-file, raw.
- [ ] Operator header block prepended to the capture (README §3).
- 12 V may feed the MOSFET module's power input alone if audible valve clicks are wanted.

Power on / RESET the board, then work the checklist in order. Ambient lines (`S1=____ S2=____`
at 1 Hz, `BTN …` on presses) interleave with the event lines — the order below applies to the
event lines (`grep -v '^S1=' <log>` shows the event stream).

## Checklist

### 1. Boot and IDLE

- [ ] First event line: `JugFiller boot (reset cause: POWER_OR_RESET)`
- [ ] LED off in IDLE.
- Observed banner / LED: `____`

### 2. 1 Hz sensor stream

- [ ] `S1=____ S2=____` lines stream at ~1 Hz in every state.
- [ ] Counts not pinned at 0 or 4095 (dry air reads roughly 3000–3900 counts — SPEC §9.3).
- Observed: `S1=____ S2=____`

### 3. Button classification (BOOT/BOOTSEL, acts on release — SPEC §5.1)

- [ ] Short press logs `BTN short ____ms`, value < 1000.
- [ ] Long press logs `BTN long ____ms`, value ≥ 1000.
- Observed: `BTN short ____ms`, `BTN long ____ms`

### 4. Dry-fire the valve ×2–3 — each open < 5 s (solenoid overheats without flow, SPEC §9.1)

First dry-fire (fresh boot: recovery=0ms ⇒ table bypassed, fresh-tank default):
- [ ] `FILL start backstop=30min recovery=0ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=30min`
- [ ] LED solid blue while FILLING.
- [ ] Short press → `VALVE close` then `ABORT attemptms=____ openms=____` (close line first).
- [ ] `openms` < 5000.
- [ ] LED off after the abort (IDLE).

Second/third dry-fire (recovery > 0 now — the abort restarted the recovery clock, SPEC §6.1):
- [ ] `FILL start backstop=…min recovery=…ms recmin=0` — backstop from the §6.2 table
      (44 min when restarted within 2 min of the abort: first-knot clamp), NOT the 30-min default.
- [ ] Same order: `BASE ok s1=____ s2=____` → `VALVE open budget=…min` → `VALVE close` →
      `ABORT attemptms=____ openms=____`; open < 5 s; LED solid blue then off.
- Observed backstop/recovery: `____`

### 5. Stage-2 rail measurement (buck-only — no serial stream exists there)

- [ ] `YYYY-MM-DD_rails.md` created from `template-rails.md` and filled (both readings
      4.9–5.1 V, exclusivity ticks). Link: `____`

## Critical assertions

> `VALVE close` precedes every `ABORT` — valve-closed-first on every exit (SPEC §7.9/§10).
> Every dry-fire open stays < 5 s (SPEC §9.1).
> Mismatches go to the day's `YYYY-MM-DD-discrepancies.md` — never edit the captured log.

## Verification greps (operator hints)

    grep -c '^JugFiller boot (reset cause: POWER_OR_RESET)' YYYY-MM-DD_session-A-dry-bench.log
    grep -c '^VALVE open budget=' YYYY-MM-DD_session-A-dry-bench.log   # = dry-fire count
    grep -c '^ABORT attemptms=' YYYY-MM-DD_session-A-dry-bench.log     # = dry-fire count

## After the session

- [ ] Update README index row A (date, evidence files, FW git rev, pass/total, status).
- [ ] File any mismatch in `YYYY-MM-DD-discrepancies.md`.
````

### Listing B — `docs/bench/template-session-B-flow1-abort.md`

````markdown
# Session B — Flow 1: abort + recomputed restart (SPEC §9.4, §6.1)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-B-flow1-abort.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row B.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-B-flow1-abort.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 started before reset; header block prepended.
- [ ] Sensors dry in air (a wet sensor at start is Session D's baseline fault, not this flow).

## Flow (SPEC §5.2 Flow 1: press 1 start → press 2 abort → press 3 restart)

### 1. Fresh boot, first fill

- [ ] `JugFiller boot (reset cause: POWER_OR_RESET)`
- [ ] Short press → `FILL start backstop=30min recovery=0ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=30min`
- [ ] LED solid blue (FILLING).

### 2. Mid-fill abort (short press)

- [ ] `VALVE close`
- [ ] `ABORT attemptms=____ openms=____`
- [ ] The abort press also logs `BTN short ____ms` (< 1000).
- [ ] LED off (IDLE); valve audibly closes if 12 V is connected.

### 3. Restart with recomputed backstop

Wait a short known interval (≈1 min) after the abort, then short press:
- [ ] `FILL start backstop=44min recovery=____ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=44min`
- [ ] LED solid blue.
- Wait used: `____` (record it — it sets the expected table row).

## Critical assertions

> **CRITICAL — the restart line must NOT show `recovery=0ms`.** The abort restarts the
> recovery clock (SPEC §6.1); `recovery=0ms` on the restart means history was lost
> (fresh-boot behavior). `recovery=0ms` on restart = FAIL.
>
> **CRITICAL — the restart budget comes from the §6.2 table, not the fresh-boot default.**
> Restarting ≤ 2 min after the abort must print `backstop=44min` and `budget=44min`
> (first-knot clamp of {2,44}), NOT 30 min. Longer waits follow the table:
> 10 min → 41, 20 min → 38, 35 min → 34, ≥ 60 min → 30.
>
> `VALVE close` precedes `ABORT` (valve-closed-first, SPEC §7.9).

## Verification greps (operator hints)

    grep -n '^FILL start' YYYY-MM-DD_session-B-flow1-abort.log   # exactly 2; 2nd NOT recovery=0ms
    grep -n '^VALVE open budget=' YYYY-MM-DD_session-B-flow1-abort.log

## After the session

- [ ] Update README index row B; file mismatches in `YYYY-MM-DD-discrepancies.md`.
````

### Listing C — `docs/bench/template-session-C-flow2-success.md`

````markdown
# Session C — Flow 2: successful fill through DONE/ACK (SPEC §9.4, §7)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-C-flow2-success.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row C.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-C-flow2-success.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 before reset; header block prepended.
- [ ] Sensors dry; cup of water at hand for hand-dipping (SPEC §9.4).
- [ ] Stopwatch: dip the first sensor only after ≥ 60 s of fill time — trips before
      TRIP_MIN_ELAPSED_S (60 s) are early-trip FAULTS (Session D territory, not this flow).

## Flow (SPEC §5.2 Flow 2: press 1 start → complete → press 2 ack → press 3 next fill)

### 1. Fill start

- [ ] `FILL start backstop=…min recovery=…ms recmin=…` (fresh boot after RESET prints
      `backstop=30min recovery=0ms recmin=0`)
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=…min`
- [ ] LED solid blue (FILLING).

### 2. First sensor trip (only after ≥ 60 s open)

- [ ] Dip sensor 1 to mount depth → `VALVE close` → `TRIP sensor=1 fillms=____`
- [ ] Valve closes immediately on the trip; LED stays solid blue (CONFIRM continues blue, SPEC §4).

### 3. Confirm window (12 s — CONFIRM_WINDOW_S)

- [ ] Dip sensor 2 within 12 s of the first trip → `CONFIRM pass sensor=2 durms=____`
- [ ] `durms` ≤ ~12000.

### 4. DONE

- [ ] `DONE attemptms=____ openms=____`
- [ ] LED green triple-pulse attention blink: 3 × (on 200 ms / off 200 ms), off 1000 ms, repeat.

### 5. Acknowledge

- [ ] Short press → `ACK done`
- [ ] Blinking stops; LED off (IDLE).

### 6. Next fill (Flow-2 press 3)

- [ ] Short press → a new `FILL start …` line.

## Critical assertions

> `VALVE close` precedes `TRIP sensor=1` (valve-closed-first on every exit, SPEC §7.9).
> Trip no earlier than 60 s of fill time; confirm within 12 s. Violations are faults, not
> successes — retake the session rather than ticking the box.
> Line order: `CONFIRM pass sensor=2 durms=…` BEFORE `DONE attemptms=… openms=…`.

## Verification greps (operator hints)

    grep -n '^TRIP sensor=1\|^CONFIRM pass sensor=2\|^DONE attemptms=\|^ACK done' YYYY-MM-DD_session-C-flow2-success.log

## After the session

- [ ] Update README index row C; file mismatches in `YYYY-MM-DD-discrepancies.md`.
````

### Listing D — `docs/bench/template-session-D-fault-paths.md`

````markdown
# Session D — Fault paths (SPEC §7, §4)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-D-fault-paths.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row D.

Fault LED cadence (SPEC §4/§7): red N-blink groups — N × (on 250 ms / off 250 ms), then
off 2000 ms, repeat; N = fault code (1 = sensor 1, 2 = sensor 2, 3 = timer/no-trip).
Faults LATCH: cleared only by RESET or power cycle; BOOT is ignored while faulted.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-D-fault-paths.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 before reset; header block prepended.
- [ ] Cup of water at hand.
- RESET the board before EACH sub-scenario: latched faults clear only on reset, and reset
  clears tank history → each sub-scenario's first fill is a fresh-boot
  `FILL start backstop=30min recovery=0ms recmin=0`.

## D1 — Baseline fault: wet sensor at start (SPEC §7.1) — REQUIRED

- [ ] Hold sensor 1 submerged, then short press to start a fill.
- [ ] `VALVE close`
- [ ] `FAULT baseline sensor=1 code=1 count=____`
- [ ] LED red 1-blink groups; latched.
- [ ] BOOT while faulted is ignored: `BTN short ____ms` logs, but NO `FILL start` follows.
- [ ] RESET clears the fault: banner `JugFiller boot (reset cause: POWER_OR_RESET)`, LED off.
- [ ] Optional repeat with sensor 2 wet: `FAULT baseline sensor=2 code=2 count=____`,
      red 2-blink groups.

> **CRITICAL — D1 must show NO `VALVE open` line before `FAULT baseline`.** The baseline
> gate refuses to open the valve on an implausible (wet) baseline (SPEC §7.1). Any
> `VALVE open budget=…` line in the D1 segment = FAIL.

## D2 — No-confirm fault (SPEC §7.5) — REQUIRED

- [ ] RESET. Short press → `FILL start backstop=30min recovery=0ms recmin=0` →
      `BASE ok s1=____ s2=____` → `VALVE open budget=30min`.
- [ ] After ≥ 60 s open, dip sensor 1 → `VALVE close` → `TRIP sensor=1 fillms=____`.
- [ ] Do NOT dip sensor 2; wait out the 12 s confirm window.
- [ ] `FAULT confirm sensor=2 code=2 durms=____`
- [ ] LED red 2-blink groups; latched.

## D3 — Guard faults (SPEC §7.6) — OPTIONAL; may be SKIPPED with reason

- [ ] Early-wet: a wet reading within the first 5 s of fill (EARLY_WET_WINDOW_S) →
      `FAULT earlywet sensor=____ code=____ fillms=____`
- [ ] Early-trip: a trip accepted before 60 s of fill time (TRIP_MIN_ELAPSED_S) →
      `FAULT earlytrip sensor=____ code=____ fillms=____`
- [ ] LED red N-blink groups matching the faulted sensor's code; latched.
- Skip reason (if not run): `____`

## D4 — Backstop expiry (SPEC §7.7) — OPTIONAL; may be SKIPPED with reason

Needs the full budget of open valve time (≥ 30 min fresh-boot). Safety hint: leave 12 V
disconnected from the MOSFET module for this sub-scenario so the solenoid stays
de-energized during the long open (SPEC §9.1/§9.2 permit this; the log lifecycle is
unchanged).
- [ ] RESET. Short press → `FILL start …` → `BASE ok …` → `VALVE open budget=…min`.
- [ ] Leave both sensors dry; wait out the budget.
- [ ] `FAULT backstop code=3 budget=…min openms=…ms`
- [ ] LED red 3-blink groups; latched.
- Skip reason (if not run): `____`

## Verification greps (operator hints)

    grep -n '^VALVE open\|^FAULT baseline' YYYY-MM-DD_session-D-fault-paths.log
    # In the D1 segment: no VALVE open line may precede the FAULT baseline line.
    grep -n '^FAULT ' YYYY-MM-DD_session-D-fault-paths.log

## After the session

- [ ] Update README index row D. Required items (D1, D2) failing → `PARTIAL(n✗)`;
      optional D3/D4 skipped-with-reason do NOT count as failures (README status vocabulary).
- [ ] File mismatches in `YYYY-MM-DD-discrepancies.md`.
````

### Listing E — `docs/bench/template-session-E-watchdog.md`

````markdown
# Session E — Optional watchdog-reset check (SPEC §10) — `SKIPPED: <reason>` allowed

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-E-watchdog.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row E.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-E-watchdog.log` |
| FW git rev (patched build = base rev + hang patch) | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 started BEFORE the patched
      flash/reset; header block prepended.
- [ ] Temporary hang patch applied LOCALLY and NEVER committed — e.g. inside `loop()`:
      `#if BENCH_HANG_TEST … while (true) {} … #endif` (bench evidence package §5;
      config.h is constexpr-only, so the guard define goes on the compiler command line
      or a local edit — it must not survive the session).
- [ ] 12 V connected to the MOSFET module if the audible valve-close click on reset is
      wanted (the solenoid sees one ~8 s energization — a recorded deviation from the
      < 5 s click rule, accepted for this check).
- Skip reason (if the whole session is not run): `____`

## Procedure

1. Flash the patched build; start the capture; let it boot (banner `POWER_OR_RESET`).
2. Short press to start a fill: `FILL start …` → `BASE ok …` → `VALVE open budget=…min`;
   LED solid blue (FILLING).
3. Trigger the hang during FILLING.

## Expected observations

- [ ] Board resets ~8 s after the hang begins (WATCHDOG_TIMEOUT_S = 8).
- [ ] Valve closed on reset (audible click if 12 V connected) — `setup()` drives the pin
      LOW as its first action (SPEC §10).
- [ ] Banner: `JugFiller boot (reset cause: WATCHDOG)`
- [ ] LED off after the reset (IDLE).
- [ ] 1 Hz `S1=____ S2=____` ambient resumes.
- [ ] No auto fill start: no `FILL start` line without a subsequent BOOT press
      (SPEC §5.3/§10 — the device never auto-starts a fill after any reset).
- [ ] A short press after the reset DOES start a fill (device healthy again):
      `FILL start backstop=30min recovery=0ms recmin=0` (reset cleared tank history).

## Critical assertions

> The reset cause must read `WATCHDOG`, not `POWER_OR_RESET`. No un-pressed `FILL start`
> may appear after the banner. The hang patch must be reverted — `git status` clean of
> any firmware change — before the session is closed.

## Cleanup (mandatory)

- [ ] Hang patch reverted; `git status` shows no firmware changes.
- [ ] Record here: patch method `____`, applied `____`, reverted `____`.

## Verification greps (operator hints)

    grep -n 'reset cause: WATCHDOG' YYYY-MM-DD_session-E-watchdog.log
    grep -n '^FILL start' YYYY-MM-DD_session-E-watchdog.log   # each must follow a BTN press

## After the session

- [ ] Update README index row E (PASS, or `SKIPPED: <reason>` if not run);
      file mismatches in `YYYY-MM-DD-discrepancies.md`.
````

### Listing R — `docs/bench/template-rails.md`

````markdown
# Stage-2 power-rail measurement — rails sheet (SPEC §9.2, §2.2)

Copy this template to `YYYY-MM-DD_rails.md` and fill it at the bench. This check is
BUCK-ONLY with USB UNPLUGGED — no serial stream exists here; the multimeter readings
below ARE the evidence (not recorded in a `.log`).

## Record

- Date: —  Operator: ____
- Meter: ____  Buck supply: ____
- Deviations: none / ____
- Result: PENDING — PASS when both readings are 4.9–5.1 V and all exclusivity ticks hold.

## Source exclusivity (tick before measuring)

> Never USB and buck simultaneously: the RP2040-Zero VSYS pin ties directly to USB VBUS
> with no protective diode (docs/bench/README.md §3 rule 1; bench evidence package §1).

- [ ] USB cable fully unplugged from the board for the whole check.
- [ ] Buck 5 V output disconnected from the board while presetting (measurement 1).
- [ ] At no point were USB and the buck's 5 V connected to the board at the same time.

## Measurements (SPEC §9.2; COMMISSIONING Stage 2)

1. Buck preset, board DISCONNECTED:
   - Reading: `____ V` — within 4.9–5.1 V? [ ] yes [ ] no
2. Buck at the board 5 V pin (buck powering the board, USB unplugged):
   - Reading: `____ V` — within 4.9–5.1 V? [ ] yes [ ] no

## After the measurement

- [ ] Restore bench power for the following sessions (USB with buck 5 V disconnected).
- [ ] Reference this sheet from the Session A record; summarize in README index row A.
- Mismatches → `YYYY-MM-DD-discrepancies.md`.
````

### Listing X — `docs/bench/template-discrepancies.md`

````markdown
# Bench discrepancies — <bench day> (expected vs observed)

Copy this template to `YYYY-MM-DD-discrepancies.md` (one per bench day, shared by all
sessions of that day). Every checklist mismatch gets an entry here.

Rules (binding — docs/bench/README.md §1):

- Quote log lines VERBATIM in fenced blocks — copy from the capture; never retype or trim.
- NEVER edit a captured `.log`; corrections and follow-ups live here.
- Each entry carries a follow-up issue number once filed (or `pending`).

## Entries

### D-1 (pre-seeded — to confirm at bench): LED indicator documentation nuance

- Session: A (observe during any session with an LED check)
- Expected (SPEC §4): the onboard WS2812 (GP16) is the sole status/fault indicator.
- Observed (docs/COMMISSIONING.md): describes an external GP14 green LED mirroring the
  patterns; the firmware drives both (GP14 driver + config-gated WS2812 mirror).
- Action: at the bench, record which indicator(s) actually render each pattern; log the
  observation here. Do NOT edit spec/SPEC.md (known nuance — bench evidence package §6).
- Follow-up issue: pending
- Status: to confirm

### D-__ — <title>

- Session: __
- Expected: `<exact expected prefix/line or LED behavior>`
- Observed:

  ```
  <verbatim log excerpt>
  ```

- Follow-up issue: __
- Status: open / filed / resolved-not-here
````

### Integration Points

```yaml
DOCS:
  - Adds 7 files to docs/bench/ (existing only after S1's README lands). Nothing else.
FIRMWARE/BUILD/CONFIG: none — zero code contact.
DOWNSTREAM CONSUMERS (verbatim, per item contract):
  - P1.M2.T2.S1 (Session A + rails): templates A and R.
  - P1.M2.T2.S2 (Sessions B+C): templates B and C.
  - P1.M2.T2.S3 (Session D): template D.
  - P1.M2.T2.S4 (Session E): template E.
  - P1.M2.T2.S5 (consolidation): template X + the README index.
UPSTREAM: docs/bench/README.md (S1) — filename grammar, status vocabulary, header block,
  power rule; templates reference its sections and must not contradict it.
```

## Validation Loop

### Level 1–3

No code, build, or tests apply (markdown-only; `pio test -e native` is unaffected — not this task's gate).

### Level 4: Content & scope gates (mechanical greps — all must pass)

```bash
cd docs/bench

# (0) existence — exactly the seven files (plus S1's README):
ls template-session-A-dry-bench.md template-session-B-flow1-abort.md \
   template-session-C-flow2-success.md template-session-D-fault-paths.md \
   template-session-E-watchdog.md template-rails.md template-discrepancies.md

# (1) copy-exact pinned lines:
grep -cF 'FILL start backstop=30min recovery=0ms recmin=0' template-session-A-dry-bench.md   # >=1
grep -cF 'FILL start backstop=30min recovery=0ms recmin=0' template-session-B-flow1-abort.md # >=1
grep -cF 'FILL start backstop=44min recovery=____ms recmin=0' template-session-B-flow1-abort.md # >=1
grep -cF 'JugFiller boot (reset cause: POWER_OR_RESET)' template-session-A-dry-bench.md      # >=1
grep -cF 'JugFiller boot (reset cause: WATCHDOG)' template-session-E-watchdog.md             # >=1
grep -cF 'FAULT baseline sensor=1 code=1 count=' template-session-D-fault-paths.md           # >=1
grep -cF 'FAULT confirm sensor=2 code=2 durms=' template-session-D-fault-paths.md            # >=1
grep -cF 'FAULT backstop code=3 budget=…min openms=…ms' template-session-D-fault-paths.md    # >=1
grep -cF 'CONFIRM pass sensor=2 durms=' template-session-C-flow2-success.md                  # >=1
grep -cF 'TRIP sensor=1 fillms=' template-session-C-flow2-success.md                         # >=1
grep -cF 'DONE attemptms=____ openms=____' template-session-C-flow2-success.md                # >=1
grep -cF 'ACK done' template-session-C-flow2-success.md                                       # >=1
grep -cF 'VALVE open budget=30min' template-session-B-flow1-abort.md                          # >=1
grep -cF 'VALVE open budget=44min' template-session-B-flow1-abort.md                          # >=1

# (2) critical assertions present:
grep -c 'must NOT show `recovery=0ms`' template-session-B-flow1-abort.md                     # 1
grep -cF 'NOT 30 min' template-session-B-flow1-abort.md                                      # >=1
grep -cF 'NO `VALVE open` line before `FAULT baseline`' template-session-D-fault-paths.md    # 1

# (3) rails sheet: both fields + exclusivity + range:
grep -cF 'Buck preset, board DISCONNECTED' template-rails.md                                  # 1
grep -cF 'Buck at the board 5 V pin' template-rails.md                                        # 1
grep -c '4.9–5.1 V' template-rails.md                                                         # >=2
grep -c 'USB cable fully unplugged' template-rails.md                                         # 1

# (4) discrepancy template fields + pre-seeded nuance:
grep -c 'Follow-up issue' template-discrepancies.md                                           # >=2
grep -c 'GP14' template-discrepancies.md                                                      # >=1
grep -c 'to confirm' template-discrepancies.md                                                # >=1

# (5) checkbox syntax present, none pre-checked:
grep -l '\- \[ \]' template-session-*.md template-rails.md                                    # all 6 listed
grep -c '\[x\]' template-*.md                                                                 # 0 per file

# (6) template purity — no fabricated dates or filled statuses:
grep -nE '20[0-9]{2}-[0-1][0-9]-[0-3][0-9]' template-*.md                                     # no matches
grep -c '| PASS |' template-session-*.md                                                      # 0 per file
grep -c '| PENDING |' template-session-*.md                                                   # 1 per file

# (7) filename-grammar references match S1's README:
grep -cF 'YYYY-MM-DD_session-A-dry-bench.log' template-session-A-dry-bench.md                 # >=1
grep -cF 'YYYY-MM-DD_session-B-flow1-abort.log' template-session-B-flow1-abort.md             # >=1
grep -cF 'YYYY-MM-DD_session-C-flow2-success.log' template-session-C-flow2-success.md         # >=1
grep -cF 'YYYY-MM-DD_session-D-fault-paths.log' template-session-D-fault-paths.md             # >=1
grep -cF 'YYYY-MM-DD_session-E-watchdog.log' template-session-E-watchdog.md                   # >=1
grep -cF 'YYYY-MM-DD_rails.md' template-rails.md                                              # >=1
grep -cF 'YYYY-MM-DD-discrepancies.md' template-discrepancies.md                              # >=1

# (8) git scope — nothing outside docs/bench/, README.md untouched by this task:
git status --porcelain | grep -v 'docs/bench/'    # must print nothing
```

## Final Validation Checklist

- [ ] All Level-4 gates (0–8) pass
- [ ] Prefix table cross-check: every backticked expected line in the templates matches the canonical table byte-exact (manual review against the blueprint table)
- [ ] Both B critical assertions and the D1 critical assertion present and unambiguous
- [ ] Templates reference S1's README sections; no contradiction with its status vocabulary/power rule
- [ ] Session templates ship PENDING/`—`/unchecked; rails + discrepancies carry copy-to-datestamp instructions
- [ ] Discrepancy template pre-seeds only D-1 (GP14 nuance); instructs verbatim quoting, never editing logs
- [ ] `git status --porcelain` clean outside `docs/bench/`

---

## Anti-Patterns to Avoid

- ❌ Don't retype prefixes from memory — copy from the canonical table (greps anchor byte-exact; `openms=…ms` suffix and the boot-banner spacing are traps)
- ❌ Don't pre-tick boxes, pre-fill dates/revs, or "example" observed values — template purity is the integrity rule's foundation
- ❌ Don't create/edit `docs/bench/README.md` (S1's parallel deliverable) or any date-stamped evidence file (fabrication)
- ❌ Don't paraphrase the critical assertions into soft language ("should", "ideally") — they are pass/fail gates
- ❌ Don't invent extra checks beyond bench_evidence_package.md §2's sessions (no +5-min long-press check, no calibration stage) — scope discipline
- ❌ Don't touch firmware, spec/, README.md, COMMISSIONING.md, or plan files (incl. tasks.json)
- ❌ Don't mark optional D3/D4 or Session E as required — SKIPPED-with-reason is their sanctioned path

---

**Confidence Score: 9/10** — complete verbatim content supplied for all seven files; every expected line verified against the firmware source this tree; gates are mechanical greps. The one residual risk is timing: P1.M2.T1.S1's README must land first (Task 0 guards it with STOP-and-report rather than improvising its content).
