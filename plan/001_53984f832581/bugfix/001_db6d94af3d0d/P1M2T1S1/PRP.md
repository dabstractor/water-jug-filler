# PRP — P1.M2.T1.S1: Create docs/bench/README.md index template and capture procedure

## Goal

**Feature Goal**: Create the `docs/bench/` evidence scaffolding index (BUG-003): a README that (a) indexes bench validation sessions A–E with date / evidence-filename / firmware-git-rev / per-checklist pass-fail / status columns where status supports BLOCKED-with-reason, (b) defines the capture procedure (USB-powered rule, 115200 baud, save-to-file raw stream, mandatory operator header block), and (c) states the evidence-integrity rule verbatim. Pure static markdown authoring — no hardware, no firmware, no code.

**Deliverable**: `docs/bench/README.md` — NEW file (the directory `docs/bench/` does not exist yet; it is created with this file). Complete verbatim content provided in the Task 1 listing below.

**Success Definition**: All Level-4 grep gates pass (five session rows, status vocabulary incl. `BLOCKED:`, `115200`, USB/buck power rule, header-block fields, integrity sentence, filename grammar matching bench_evidence_package.md §3); `git status --porcelain` shows exactly one new file `docs/bench/README.md`; no other repo file touched.

## User Persona

**Target User**: The bench operator executing sessions A–E (P1.M2.T2.S1–S5) — this README is their runbook entry point: how to power the rig, how to capture, what header to write, where to record each result. Secondary: the auditor verifying BUG-003 closure against the index.

**Use Case**: Operator opens `docs/bench/README.md`, reads the power rule (USB powered, buck 5 V disconnected), starts a serial capture at 115200 before reset, saves the raw stream under the session filename, prepends the header block, and fills one index row (date, filename, git rev, checklist summary, status). If the rig is missing, they write `BLOCKED: <reason>` instead — the README makes that the sanctioned path.

**User Journey**: template ships with all rows `PENDING` → operator runs Session A → fills row A (`PASS` or `PARTIAL` + discrepancies link) → … → P1.M2.T2.S5 finalizes the index and reconciles the discrepancy list.

**Pain Points Addressed**: BUG-003 — required §9/§10 bench evidence has no home, no naming convention, no integrity discipline; without this index, "evidence" could be fabricated or ad-hoc, and the P1.M7.T2.S1 sign-off would stay unverifiable.

## Why

- Bug report Issue 3 / Recommendations (BUG-003): "Execute the deferred physical bench validation … and capture the serial logs under docs/bench/." The capture itself is P1.M2.T2 (physical); this subtask is the software-preparable scaffolding the architecture explicitly assigns ("Preparable without hardware: directory skeleton, index template, … capture procedure (115200, save-to-file, header block)") — bench_evidence_package.md §4.
- Binding inputs: bench_evidence_package.md §1 (power-safety rules — RP2040-Zero VSYS ties directly to USB VBUS with no diode, so USB + buck simultaneously is forbidden), §2 (sessions A–E definitions), §3 (date-stamped file layout), §4 (integrity rule, verbatim below); system_context.md §2 (log grammar is APPEND-ONLY/prefix-anchored per jug_filler.ino:1343–1344, so captures must stay raw).
- Downstream contracts: P1.M2.T1.S2 authors the per-session evidence templates/rails sheet/discrepancy template **matching this README's conventions** (its PRP consumes this deliverable); P1.M2.T2.S1–S4 fill rows; P1.M2.T2.S5 finalizes. COMMISSIONING.md's "Completion sign-off" cross-reference (P1.M2.T3.S2) lands after evidence exists.

## What

One markdown file, complete content specified verbatim below. It contains: the integrity rule (block-quoted, verbatim from the item contract), the session index table (A dry bench + rails, B flow-1, C flow-2, D fault paths, E optional watchdog; all rows PENDING with `—` placeholders), the status vocabulary (`PENDING` / `PASS` / `PARTIAL(n✗)` / `BLOCKED: <reason>` / `SKIPPED: <reason>` — the latter only for optional items), the capture procedure (power rule first, 115200 8N1, save-to-file raw, no post-capture edits, header block fields: date, firmware build/git rev, wiring notes, deviations), the rail-check exception (buck-only → multimeter readings in `YYYY-MM-DD_rails.md`, no serial stream exists), the Session-E never-committed-patch note, and the filename grammar table.

**Scope boundaries (do NOT author here)**: per-session expected-prefix checklists and checkbox templates → P1.M2.T1.S2; `YYYY-MM-DD_rails.md` and `YYYY-MM-DD-discrepancies.md` template files → P1.M2.T1.S2; any firmware/spec/config change → none; README.md/COMMISSIONING.md edits → P1.M1.T2.S1 (running in parallel, disjoint files) and P1.M2.T3.

### Success Criteria

- [ ] `docs/bench/README.md` exists with exactly the Task 1 listing content
- [ ] Index has exactly five session rows A–E with columns: date, evidence filename, FW git rev, checklist summary, status; all shipped `PENDING`
- [ ] Status vocabulary documented incl. `BLOCKED: <reason>` (mandatory-support requirement of the item contract)
- [ ] Capture procedure states: USB-powered with buck 5 V disconnected (never both), 115200 baud, save-to-file, raw untouched stream, operator header block with the four mandated fields
- [ ] Integrity rule present verbatim: "evidence logs must be genuine captures; if the rig is unavailable record BLOCKED with reason — never fabricate log contents."
- [ ] Filename grammar matches bench_evidence_package.md §3 byte-for-byte (`YYYY-MM-DD_session-X-….log`, `YYYY-MM-DD_rails.md`, `YYYY-MM-DD-discrepancies.md`)
- [ ] `git status --porcelain` → only `?? docs/bench/` (containing just README.md)

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the complete file content is given verbatim in the Task 1 listing; every convention is traceable to bench_evidence_package.md §1–§4 (sessions, filenames, power rules, integrity rule) and system_context.md §2 (log grammar facts); validation is mechanical grep, executable without hardware. No other repo file is read or modified.

### Documentation & References

```yaml
- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  why: THE contract — §1 power rules, §2 sessions A–E, §3 file layout (your filename
        grammar must match byte-for-byte), §4 integrity rule + preparable-without-hardware
        split (this task) vs physical captures (P1.M2.T2).
  gotcha: §5's watchdog-hang patch is Session E's execution detail (P1.M2.T2.S4) —
        reference it, never implement it.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  section: "2. Log-line grammar"
  why: why captures must stay raw/unedited: formats are append-only and prefix-anchored
        (jug_filler.ino:1343–1344) — evidence greps anchor on exact prefixes.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M1T2S1/PRP.md
  why: parallel-work contract — it edits README.md:49 + docs/COMMISSIONING.md:90 only;
        fully disjoint from docs/bench/README.md. No coordination needed beyond
        not touching its files.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T1S1/research/notes.md
  why: digest of every fact encoded into the README (facts table, design decisions,
        grep-gate list).

- file: docs/COMMISSIONING.md
  why: stylistic sibling for operator-facing markdown (stage headers, safety-callout
        tone). READ-ONLY for this task.
```

### Current Codebase tree

```bash
water-jug-filler/
├── README.md                     # serial-log table (parallel task's scope)
├── docs/
│   └── COMMISSIONING.md          # only doc today — docs/bench/ does NOT exist
├── firmware/…, test/…, spec/SPEC.md (read-only), plan/… (read-only)
```

### Desired Codebase tree

```bash
docs/
├── COMMISSIONING.md              # untouched here
└── bench/
    └── README.md                 # NEW — index + capture procedure + integrity rule
```

### Known Gotchas

```text
# CRITICAL: filename grammar is CONTRACT for S2/S5 — copy §3 names exactly
#   (e.g. `YYYY-MM-DD_session-A-dry-bench.log`, `YYYY-MM-DD-discrepancies.md`
#   with the hyphen between date and "discrepancies").

# CRITICAL: integrity rule must appear VERBATIM (item contract wording) — do not
# paraphrase it into a weaker form.

# CRITICAL: never suggest editing a captured .log — greps are prefix-anchored and
# formats append-only; corrections belong in the discrepancies file.

# Power rule must be stated as USB-with-buck-5V-DISCONNECTED (never both), not
# merely "USB powered" — VSYS↔VBUS has no diode (hard finding, §1).

# Status `SKIPPED:<reason>` is ONLY for optional items (Session E; backstop-expiry
# check inside D). Required sessions use BLOCKED when the rig is unavailable.

# No date literals in the shipped template — placeholders `—` / `YYYY-MM-DD` only;
# nothing has been captured yet (fabricating dates would violate the integrity rule).
```

## Implementation Blueprint

### Implementation Tasks

```yaml
Task 0: PRECONDITION
  - RUN: ls docs/bench 2>/dev/null; echo $?
  - EXPECT: non-zero (directory absent). If docs/bench/README.md already exists →
    STOP and report (re-planning needed); do not overwrite.

Task 1: CREATE docs/bench/README.md
  - WRITE the Task 1 listing verbatim (mkdir -p docs/bench happens automatically).
  - PRESERVE: every other repo file byte-for-byte.

Task 2: RUN Level-4 grep gates + git scope check (below). All must pass.
```

**Task 1 listing — `docs/bench/README.md` (complete file):**

````markdown
# Bench validation evidence — index and capture procedure

Evidence home for the physical bench validation of PRD §9 flows and fault paths and
the §10 watchdog check (BUG-003; original plan task P1.M7.T2.S1, executed by this
changeset's P1.M2.T2.S1–S5). The firmware side is complete; this directory holds the
hardware-validated sign-off. Sessions and checklists are defined in
`plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md`
(the binding contract); per-session templates live alongside this index.

## 1. Evidence integrity rule (binding)

> Evidence logs must be genuine captures. If the rig is unavailable, record BLOCKED
> with reason in the index below — never fabricate or synthesize log contents.

Captured files are raw serial output: firmware log formats are append-only and
prefix-anchored, so evidence is verified by grepping exact prefixes. Never edit a
captured `.log` — observations that disagree with expectations go in the session's
discrepancies entry (`YYYY-MM-DD-discrepancies.md`), quoting the log, never altering it.

## 2. Session index

One row per session. Fill columns when a session completes; the per-checklist detail
lives in the session's evidence template (P1.M2.T1.S2), summarized here as `pass/total`.

| Session | Scope | Date | Evidence file | FW git rev | Checklist | Status |
|---|---|---|---|---|---|---|
| A | Dry bench check + Stage-2 rail measurement (SPEC §9.1–9.2, §4) | — | `YYYY-MM-DD_session-A-dry-bench.log` + `YYYY-MM-DD_rails.md` | — | — | PENDING |
| B | Flow 1: abort + recomputed restart (SPEC §9.4, §6.1) | — | `YYYY-MM-DD_session-B-flow1-abort.log` | — | — | PENDING |
| C | Flow 2: successful fill through DONE/ACK (SPEC §9.4, §7) | — | `YYYY-MM-DD_session-C-flow2-success.log` | — | — | PENDING |
| D | Fault paths: baseline, no-confirm, optional guards (SPEC §7) | — | `YYYY-MM-DD_session-D-fault-paths.log` | — | — | PENDING |
| E | Optional watchdog-reset check (SPEC §10) | — | `YYYY-MM-DD_session-E-watchdog.log` | — | — | PENDING |

Discrepancies (expected-vs-observed, with follow-up issue numbers): `YYYY-MM-DD-discrepancies.md`.

**Status vocabulary** (the only values allowed in the Status column):

- `PENDING` — not yet run (initial state of every row).
- `PASS` — every checklist item green.
- `PARTIAL(n✗)` — ran, n items failed; each failure has a discrepancies entry.
- `BLOCKED: <reason>` — could not run (rig unavailable, missing hardware, safety
  concern); required sessions may stay blocked only with the reason recorded.
- `SKIPPED: <reason>` — deliberately not run; **optional items only** (Session E;
  the backstop-expiry check inside Session D).

## 3. Capture procedure

1. **Power rule (binding — SPEC §2.2):** bench and commissioning sessions are
   **USB-powered with the buck 5 V output disconnected from the board**. The
   RP2040-Zero VSYS pin ties directly to USB VBUS with no protective diode — never
   connect USB and the buck simultaneously. 12 V may feed the MOSFET module's power
   input alone for valve dry-fire (opens < 5 s each; the solenoid overheats without flow).
2. Start the serial capture **before** power/reset: 115200 baud, 8N1, save-to-file.
   Save the raw stream — no trimming, no copy-paste retyping, no edits ever.
3. Prepend the operator header block below as the first lines of every evidence file
   (or save it as the file's opening section when the terminal supports it).
4. **Rail-check exception (Session A):** the Stage-2 measurement is buck-only with USB
   unplugged — no serial stream exists there. Record multimeter readings (buck preset
   with board disconnected, and at the board 5 V pin; both 4.9–5.1 V) in
   `YYYY-MM-DD_rails.md`, not a `.log`.
5. **Session E only:** the deliberate hang is a temporary, never-committed firmware
   patch (`git status` clean of it afterward). The evidence header records that the
   patch was applied and reverted.

**Operator header block** (mandatory, first lines of every evidence file):

```text
# bench evidence — session <A|B|C|D|E> — <session name>
- date: YYYY-MM-DD
- firmware: git <rev> (board build of <date>; native suite passing)
- wiring: <USB-powered, buck 5 V disconnected; sensors dry in air; valve dry / 12 V
  for dry-fire; any rig specifics>
- deviations: <none | list>
```

## 4. Conventions (binding on downstream artifacts)

- File naming: `YYYY-MM-DD_session-<X>-<slug>.log` for captures;
  `YYYY-MM-DD_rails.md`; `YYYY-MM-DD-discrepancies.md` (one per bench day, shared).
- Every row above is finalized by P1.M2.T2.S5; per-session templates (P1.M2.T1.S2)
  carry the expected-prefix checklists this index summarizes.
````

### Integration Points

```yaml
DOCS:
  - Creates docs/bench/ (new) with README.md only. COMMISSIONING.md "Completion
    sign-off" cross-reference to docs/bench/ is P1.M2.T3.S2's edit, not here.
FIRMWARE/BUILD/CONFIG:
  - None. Zero code contact.
DOWNSTREAM CONSUMERS:
  - P1.M2.T1.S2: per-session templates A–E + rails sheet + discrepancy template —
    MUST match this README's filename grammar, header block, status vocabulary.
  - P1.M2.T2.S1–S5: fill index rows; S5 finalizes.
```

## Validation Loop

### Level 1–3

No code, no build, no tests apply. `pio test -e native` is expected to stay green but
is unaffected by a markdown-only addition (do not re-run the full suite; it is not
this task's gate).

### Level 4: Content & scope gates (mechanical greps)

```bash
# (1) five session rows with all columns present:
for s in A B C D E; do grep -c "^| $s |" docs/bench/README.md; done   # each: 1
# (2) status vocabulary incl. BLOCKED-with-reason:
grep -c "BLOCKED: <reason>" docs/bench/README.md          # >= 1
grep -c "SKIPPED: <reason>" docs/bench/README.md          # >= 1
# (3) capture procedure essentials:
grep -c "115200" docs/bench/README.md                     # >= 1
grep -c "USB-powered with the buck 5 V output disconnected" docs/bench/README.md  # >= 1
# (4) header-block fields:
grep -c "^- date: YYYY-MM-DD" docs/bench/README.md        # >= 1
grep -c "^- firmware: git <rev>" docs/bench/README.md     # >= 1
grep -c "^- wiring:" docs/bench/README.md                 # >= 1
grep -c "^- deviations:" docs/bench/README.md             # >= 1
# (5) integrity rule verbatim core:
grep -c "never fabricate or synthesize log contents" docs/bench/README.md  # 1
# (6) filename grammar matches the §3 contract:
grep -c "YYYY-MM-DD_session-A-dry-bench.log" docs/bench/README.md   # 1
grep -c "YYYY-MM-DD-discrepancies.md" docs/bench/README.md          # >= 1
grep -c "YYYY-MM-DD_rails.md" docs/bench/README.md                  # >= 1
# (7) template purity: no fabricated dates/statuses:
grep -cE "\| 20[0-9]{2}-" docs/bench/README.md            # 0 (placeholders only)
grep -c "| PASS |" docs/bench/README.md                   # 0 (all rows PENDING)
# (8) git scope — exactly one new file:
git status --porcelain    # expected: ?? docs/bench/  (containing only README.md)
```

## Final Validation Checklist

- [ ] All Level-4 gates pass (1–8)
- [ ] Five session rows A–E; every row PENDING with `—` placeholders
- [ ] Integrity rule verbatim; power rule states the never-both constraint; 115200; header block with 4 fields
- [ ] Filename grammar byte-matches bench_evidence_package.md §3
- [ ] `git status --porcelain` shows only `?? docs/bench/`; no other file modified
- [ ] Scope kept: no per-session checklists authored (S2's), no firmware/spec/README/COMMISSIONING contact

---

## Anti-Patterns to Avoid

- ❌ Don't fabricate or soften the integrity rule — quote it; a weakened paraphrase defeats BUG-003's whole point
- ❌ Don't pre-fill dates, revs, or PASS statuses — the template ships PENDING; filling rows is P1.M2.T2's physical work
- ❌ Don't restate the full per-session expected-prefix checklists here — they live in S2's templates; the index summarizes
- ❌ Don't describe the power rule as merely "USB powered" — the buck-disconnection is the safety-critical half
- ❌ Don't rename the §3 files or invent a `templates/` layout — S2's PRP will anchor on these exact names
- ❌ Don't touch firmware, spec/, README.md, COMMISSIONING.md, or any plan file (incl. tasks.json)

---

**Confidence Score: 10/10** — static markdown with the complete content supplied verbatim; every convention traceable to the binding architecture contract (bench_evidence_package.md §1–§4); validation is mechanical greps; no hardware, code, or parallel-work conflict (P1.M1.T2.S1 edits disjoint files).
