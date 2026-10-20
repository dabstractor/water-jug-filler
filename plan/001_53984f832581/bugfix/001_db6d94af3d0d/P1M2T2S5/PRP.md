# PRP — P1.M2.T2.S5 (bugfix changeset): Consolidate the discrepancy list and finalize the bench evidence index

## Goal

**Feature Goal**: Close out the BUG-003 bench-evidence deliverable as a **finalized evidence package**: one date-stamped discrepancy ledger (`docs/bench/YYYY-MM-DD-discrepancies.md`) holding every expected-vs-observed mismatch collected across bench sessions P1.M2.T2.S1–S4 (each entry: expected value, observed evidence quoted verbatim, proposed follow-up issue), and a finalized `docs/bench/README.md` index in which **no row remains PENDING** — every session dated, the firmware git rev recorded, checklist/status cells closed out including all BLOCKED/SKIPPED entries — plus a verified-clean firmware tree (no leftover watchdog patch).

**Deliverable**:
- `docs/bench/YYYY-MM-DD-discrepancies.md` — NEW file, created from `docs/bench/template-discrepancies.md` (copy, then fill), date = the consolidation day (`date +%F`). Contains at minimum entry **D-1** (the LED-indicator documentation nuance) recorded **as observed** from static sources, with a **proposed follow-up issue**, plus any session-collected discrepancies (in the current BLOCKED reality: none exist — D-1 is the sole entry).
- `docs/bench/README.md` — MODIFIED: all five index rows finalized (Date / Evidence file / FW git rev / Checklist / Status cells — no placeholder `YYYY-MM-DD_…`, no `PENDING`), the trailing discrepancies-filename line pointing at the real file, and a new short `## 5. Package finalization (P1.M2.T2.S5)` note recording the consolidation date, the firmware rev, the watchdog-patch-absence verification, and the honest package verdict.
- Verification evidence (run, not committed): `git status --short firmware/` empty; `grep -rn BENCH_HANG_TEST firmware/` no hits.

**Success Definition**: A reader with zero context — specifically the follow-on task P1.M2.T3.S2 wiring the COMMISSIONING.md "Completion sign-off" cross-reference — can open `docs/bench/README.md`, see all five rows with real dates, one firmware rev, closed-vocabulary statuses carrying the preserved BLOCKED reasons, follow the trailing link to a real discrepancy file whose D-1 entry quotes SPEC.md / COMMISSIONING.md / config.h verbatim and proposes BUG-004, and trust that nothing was fabricated: no `.log` files exist and none were invented. `git status` shows exactly the two doc artifacts above (plus plan/ bookkeeping); firmware/, spec/, test/, platformio.ini, root README.md, COMMISSIONING.md, and every session template are untouched.

## User Persona

**Target User**: Downstream maintainer/agent (P1.M2.T3.S2) and the human signing off BUG-003; not a bench operator — this is a documentation-consolidation pass.

**Use Case**: After all bench sessions have terminated (each PASS/PARTIAL/BLOCKED/SKIPPED per its own gate), one pass gathers every "expected X, observed Y" mismatch into the shared ledger that feeds follow-up fixes, and turns the index from a live tracker into a finalized evidence record.

**User Journey**: Read the live index rows A–E and every session artifact → recover per-row dates (git history of the row-finalizing commits) and the firmware rev (`git log -1 --format=%h -- firmware/`) → copy the discrepancy template to the date-stamped ledger and record D-1 as observed (static quotes), proposing BUG-004 → sweep every index cell to its final value, preserving the recorded BLOCKED reasons verbatim → add the finalization note with the patch-absence verification → run the grep gates.

**Pain Points Addressed**: BUG-003's deliverable is "captured evidence under docs/bench/" — the rig was unavailable (all sessions BLOCKED), so the honest close-out is a finalized index + discrepancy ledger that states exactly what was and wasn't validated, instead of a half-filled tracker with placeholder filenames.

## Why

- Item contract (P1.M2.T2.S5): the discrepancy list is its own date-stamped file feeding fixes before any further phase; the index must show final status per checklist item including BLOCKED/skip entries; the repo must be confirmed free of the temporary watchdog patch.
- Bug-fix PRD Issue 3 (BUG-003) + Recommendations: execute the bench validation and capture the logs under docs/bench/ — where the rig is unavailable, the evidence package's integrity rule (README §1) makes **BLOCKED with reason** the legal terminal state; this task makes that state final and legible.
- Evidence package §6 (binding): the GP14-vs-WS2812 indicator nuance is *known* — record it in the discrepancies ledger as observed; **do NOT edit spec/SPEC.md** (Mode A).
- Downstream dependency: P1.M2.T3.S2's INPUT is literally "the finalized bench evidence package (index + discrepancy list) from P1.M2.T2.S5", anchored at docs/COMMISSIONING.md "Completion sign-off" (L535+). S5 is the last producer before that consumer.

## What

A pure-documentation consolidation (Mode A; MOCKING: none). No firmware, no captures, no spec edits.

1. **Entry state**: sessions S1–S4 have all finalized their index rows (live reality at PRP time: rows A–D `BLOCKED: rig unavailable …` with recorded reasons; row E `BLOCKED` by the parallel S4 — re-read live at Task 0; if reality differs because a rig appeared, the same procedure generalizes: consolidate whatever discrepancies the sessions recorded and finalize whatever rows exist).
2. **Discrepancy ledger**: create `docs/bench/$(date +%F)-discrepancies.md` from the template. One entry minimum — **D-1, LED indicator documentation nuance** — converted from "to confirm" to *recorded as observed*: expected = SPEC §4's "sole status/fault indicator" claim; observed = COMMISSIONING.md wires a GP14 green LED and says the firmware drives two indicators, and the firmware source does drive both (`PIN_LED = 14`, `USE_ONBOARD_RGB = true`). Evidence = verbatim fenced quotes with file:line citations (SPEC.md L9/L31/L33, COMMISSIONING.md L20/L96–98, config.h L25/L69). Honest caveat in the entry: bench confirmation of which indicators physically render patterns is outstanding (rig BLOCKED). Follow-up issue: `proposed: BUG-004 (LED indicator documentation: SPEC §4 sole-indicator claim vs GP14 external LED)` — BUG-004 is verified unused in this repo. Status: `open (proposed)`. Any additional session-recorded mismatches get their own entries, log excerpts quoted verbatim, never retyped.
3. **Index finalization** (README §2 table, rows A–E): Date = the date that row's final status was recorded (recover via `git show -s --format=%cs <commit>` — row A `c3f6901`, rows B/C `aba9108`, row D `5a18d8a`, row E = S4's finalizing commit; fallback if a commit isn't findable: `date +%F`). Evidence file: `— (none; BLOCKED — no capture, integrity rule README §1)` for blocked rows (real filename for any row that ran). FW git rev: the same value in all five rows — `git log -1 --format=%h -- firmware/` (all commits after it are docs-only; this is the firmware identity the whole package validated against). Checklist: `not run` for blocked rows (`pass/total` for any row that ran). Status: keep each row's existing recorded reason **verbatim** — do not rewrite, shorten, or lose it; only verify closed vocabulary and that no row is `PENDING`. Replace the trailing `YYYY-MM-DD-discrepancies.md` placeholder with the real filename.
4. **Finalization note**: append `## 5. Package finalization (P1.M2.T2.S5)` to README: consolidation date, firmware rev, `git status --short firmware/` clean + `grep -rn BENCH_HANG_TEST firmware/` empty (watchdog patch confirmed absent), discrepancy count (1 filed: D-1/BUG-004 proposed), and the verdict line — sessions executed 0/5, all BLOCKED rig-unavailable; physical validation of PRD §9 flows/fault paths remains outstanding pending rig availability; the package is the honest BUG-003 close-out, not a pass.
5. **NOT in scope**: filling or editing session templates A–E, rails sheet, or any `.log` (none exist — never create one); P1.M2.T3.S1/S2 doc sweeps (root README.md, COMMISSIONING.md); spec/SPEC.md (read-only, Mode A); firmware/, test/, platformio.ini; tasks.json (orchestrator-owned).

### Success Criteria

- [ ] `docs/bench/$(date +%F)-discrepancies.md` exists, derived from the template, with D-1 recorded as observed (verbatim quotes + file:line) and `proposed: BUG-004`
- [ ] No index row is `PENDING`; no cell still contains a `YYYY-MM-DD` placeholder; every row has Date + FW git rev + closed-vocabulary Status (recorded BLOCKED reasons preserved verbatim)
- [ ] README trailing discrepancies line names the real file; new §5 finalization note present with the patch-absence verification results
- [ ] `git status --short firmware/` empty; `grep -rn BENCH_HANG_TEST firmware/` returns nothing
- [ ] `git status` scope = README.md edit + the new discrepancies file (+ plan/ bookkeeping); nothing else touched

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the live index state (all rows BLOCKED, placeholder cells enumerated), the exact per-row date-recovery commands and commit hashes, the firmware-rev command, the verbatim D-1 quote set with file:line anchors, the BUG-004 proposal (verified unused), the template's binding rules (verbatim quotes, never edit a capture, follow-up issue per entry), the status vocabulary and where SKIPPED is legal, the parallel-S4 boundary, and the downstream T3.S2 contract. All contracts were read from disk this session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/bench/README.md
  why: THE artifact you finalize. §1 integrity rule (genuine captures or BLOCKED,
        never fabricate; never edit a captured .log). §2 session index table —
        columns Session|Scope|Date|Evidence file|FW git rev|Checklist|Status; the
        ONLY legal Status values PENDING/PASS/PARTIAL(n✗)/BLOCKED: reason/
        SKIPPED: reason (SKIPPED legal ONLY for row E); the trailing line
        "Discrepancies (expected-vs-observed, with follow-up issue numbers):
        YYYY-MM-DD-discrepancies.md" — replace the placeholder with the real
        filename. §4 conventions: discrepancies file is
        "YYYY-MM-DD-discrepancies.md (one per bench day, shared)" and "Every row
        above is finalized by P1.M2.T2.S5". Live rows at PRP time: A–D BLOCKED
        (rig unavailable; row A also cites no multimeter), E BLOCKED by parallel
        S4 — RE-READ LIVE at Task 0.

- file: docs/bench/template-discrepancies.md
  why: SOURCE for the new ledger — copy to the date-stamped file, then fill.
        Carries the pre-seeded D-1 entry (expected: SPEC §4 WS2812 sole
        indicator; observed: GP14 external LED) and the binding rules quoted in
        its header: verbatim fenced quotes copied from the source, NEVER edit a
        captured .log, each entry carries a follow-up issue number once filed
        (or pending).

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md
  section: "3. docs/bench/ layout" + "4. Executability split" + "6. Known doc
        nuance to carry in the discrepancy list"
  why: §3 defines the deliverable layout (index + YYYY-MM-DD-discrepancies.md
        with "expected-vs-observed, log excerpt, follow-up issue # each"). §4
        the integrity rule (BLOCKED with reason, never fabricate). §6 THE D-1
        mandate: record it in docs/bench/*-discrepancies.md; do NOT edit
        spec/SPEC.md.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S4/PRP.md
  why: CONTRACT of the parallel prior item (row E owner). Defines what exists
        when S5 starts: row E finalized with a closed-vocabulary status, its
        BLOCKED branch leaves no log file and template E unfilled, and its
        cleanup guarantees `git status --short firmware/` empty. S5 verifies
        that outcome but never edits S4 artifacts (row E's Status text is
        S4's — preserve it verbatim like every other row).

- file: spec/SPEC.md
  section: L9 + L31 + L33 (hardware table / pin table)
  why: D-1 "expected" side, quoted verbatim: L9 "Onboard WS2812 RGB LED on
        GP16 — the sole status/fault indicator (§4)."; L31 pin-table row "GP16 …
        WS2812 RGB LED — sole status/fault indicator"; L33 lists GP14 among
        unwired spares. READ-ONLY — never edited (Mode A).

- file: docs/COMMISSIONING.md
  section: L20 + L96–98 + L127
  why: D-1 "observed" side, quoted verbatim: L20 "green LED on GP14 via 330 Ω";
        L96–98 "the firmware drives two indicators that always show the same
        pattern shape — the onboard WS2812 mirror (adds color) and the external
        LED on GP14 (on/off)"; L127 "external LED still shows every pattern."
        Read-only here — the changeset sweep is P1.M2.T3.S2's, not S5's.

- file: firmware/jug_filler/config.h
  section: L25 + L69
  why: firmware proof that both indicators are driven (quote verbatim in D-1):
        L25 `constexpr uint8_t PIN_LED = 14;` (GP14 external green LED) and
        L69 `constexpr bool USE_ONBOARD_RGB = true;` (WS2812 mirror on GP16).
        Read-only.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S5/research/consolidation-research.md
  why: this session's verified facts: live row states, session-finalizing
        commit hashes (A=c3f6901, B/C=aba9108, D=5a18d8a), the D-1 quote set,
        BUG-004 availability, firmware-rev command, downstream T3.S2 contract.

# Git archaeology (run, don't guess):
#   git show -s --format=%cs c3f6901   # row A finalizing commit date
#   git show -s --format=%cs aba9108   # rows B/C
#   git show -s --format=%cs 5a18d8a   # row D
#   git log --oneline --grep='session E\|watchdog' -i   # row E commit (S4)
#   git log -1 --format=%h -- firmware/                  # FW rev for ALL rows
```

### Current Codebase tree (expected at implementation start)

```bash
water-jug-filler/
├── README.md                      # read-only here (swept by P1.M2.T3.S1)
├── platformio.ini                 # untouched
├── firmware/jug_filler/           # CLEAN (S4 verified; S5 re-verifies)
├── test/                          # untouched
├── spec/SPEC.md                   # source of truth — NEVER edited (Mode A)
├── docs/
│   ├── COMMISSIONING.md           # read-only here (swept by P1.M2.T3.S2)
│   └── bench/
│       ├── README.md              # EDIT: finalize rows A–E + trailing link + §5 note
│       ├── template-session-{A..E}-*.md   # sibling-owned — DO NOT touch
│       ├── template-rails.md / template-discrepancies.md   # copy-source only
│       └── (no date-stamped files yet — none may be invented)
└── plan/…/P1M2T2S5/               # this PRP + research/ (bookkeeping)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
docs/bench/
├── README.md                       # MODIFIED — finalized index (rows A–E, trailing
│                                   #   discrepancies link, new §5 finalization note)
└── YYYY-MM-DD-discrepancies.md     # NEW — the consolidated discrepancy ledger:
                                    #   D-1 recorded-as-observed (verbatim quotes,
                                    #   proposed BUG-004), one entry per every
                                    #   expected-vs-observed mismatch from S1–S4
# (YYYY-MM-DD = consolidation day, `date +%F` at implementation time)
```

### Known Gotchas of our codebase & Library Quirks

```text
# CRITICAL — integrity rule (docs/bench/README.md §1, binding): evidence logs must
#   be genuine captures; never fabricate or synthesize log contents. NO sessions
#   ran (rig unavailable) ⇒ NO .log files exist and NONE may be created by S5.
#   D-1's "observed" evidence is STATIC: verbatim quotes from SPEC.md,
#   COMMISSIONING.md, and config.h with file:line citations — clearly labeled as
#   source quotes, never presented as serial captures. The entry must state that
#   bench confirmation of rendered patterns is outstanding.

# CRITICAL — do NOT edit spec/SPEC.md (Mode A + evidence package §6): the D-1 fix
#   belongs to a follow-up issue (proposed BUG-004), not to this changeset. Same
#   for COMMISSIONING.md's GP14 text — the changeset-level sweep is P1.M2.T3.S2's.

# CRITICAL — preserve recorded BLOCKED reasons VERBATIM: each row's Status cell
#   was written by its owning session (S1–S4). Finalization fills the empty
#   cells (Date / Evidence file / FW git rev / Checklist) and replaces
#   placeholders; it must not rewrite, paraphrase, or shorten any existing
#   Status text. Rows/templates/logs of sibling sessions are hands-off.

# GOTCHA — "every session dated" for BLOCKED rows means the date the outcome was
#   recorded, NOT an execution date (none exists). Recover from the
#   row-finalizing commits (A=c3f6901, B/C=aba9108, D=5a18d8a, E=S4's commit —
#   find via git log at implementation time); fallback `date +%F`. Keep the
#   Evidence-file cell explicit that no capture exists ("— (none; BLOCKED …)").

# GOTCHA — one FW git rev for all rows: every commit after the last firmware
#   touch is docs/plan-only, so the firmware identity is uniform. Record
#   `git log -1 --format=%h -- firmware/` (short hash) in all five rows AND the
#   §5 note. Do not use HEAD's rev blindly if firmware/ moved — the command is
#   the source of truth.

# GOTCHA — the discrepancies file is per BENCH DAY ("one per bench day, shared");
#   no bench day occurred, so the consolidation day stamps it. If S4 (parallel)
#   already created one for its own day, APPEND to it instead of creating a
#   second file — check `ls docs/bench/*-discrepancies.md` first. The template
#   is COPIED once, never filled in place.

# GOTCHA — follow-up issue numbers: the repo's convention is BUG-NNN (bug-hunt
#   report used BUG-001…003). BUG-004 is verified unused (grep, zero hits).
#   Mark it "proposed:" — nothing is filed anywhere by this task; filing is a
#   human/orchestrator act. Never renumber the pre-seeded D-1.

# GOTCHA — tasks.json is orchestrator-owned; do not "helpfully" update statuses
#   (system_context.md §7). Status of THIS task is the orchestrator's business.

# GOTCHA — markdown table surgery: the index rows contain long BLOCKED reasons
#   with em-dashes and pipes-free text; keep the pipe structure intact, one line
#   per row; do not reflow. Verify with `grep -c '^| [A-E] |' docs/bench/README.md` = 5.
```

## Implementation Blueprint

### Data models and structure

No code. Two markdown artifacts: (a) the ledger, structured exactly as `template-discrepancies.md` (header rules kept, `### D-n — title` entries with Session / Expected / Observed fenced excerpt / Follow-up issue / Status fields); (b) the index — an existing 7-column markdown table plus a new §5 note section.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: LIVE-STATE AUDIT (before any edit)
  - RUN: sed -n '/## 2. Session index/,/## 3/p' docs/bench/README.md   # live rows
  - CHECK: every row A–E has a closed-vocabulary Status (no PENDING); note each
    row's current Status text (preserve verbatim). If ANY row is still PENDING,
    STOP and report — a sibling session has not finalized; S5 must not guess.
  - RUN: ls docs/bench/*-discrepancies.md 2>/dev/null
    # If one EXISTS (a parallel session's bench day): APPEND to it in Task 1
    # instead of creating a new file. Expected today: none.
  - RUN: git status --short firmware/          # MUST be empty (S4 cleanup)
  - RUN: grep -rn BENCH_HANG_TEST firmware/ || echo "absent"   # MUST be absent
  - RUN: git log -1 --format=%h -- firmware/   # FWREV — record for all rows
  - RUN: git show -s --format=%cs c3f6901 aba9108 5a18d8a      # row dates A, B/C, D
  - RUN: git log --oneline -15                 # locate S4's row-E commit; its
    # date (or `date +%F` if not yet committed) is row E's date.
  - RECONCILE discrepancy inputs: re-read docs/bench/template-session-{A..E}-*.md
    and the index Status texts for any recorded expected-vs-observed mismatch
    (current reality: none — sessions all BLOCKED, templates unfilled). Each
    found mismatch becomes a ledger entry in Task 1.

Task 1: CREATE the discrepancy ledger
  - COPY: cp docs/bench/template-discrepancies.md docs/bench/$(date +%F)-discrepancies.md
    (SKIP the copy and APPEND instead if Task 0 found an existing file.)
  - FILL the header: <bench day> → the consolidation date; keep the binding rules.
  - REWRITE entry D-1 in place (same numbering) — recorded as observed:
      ### D-1 — LED indicator documentation nuance (recorded as observed; bench
               confirmation outstanding)
      - Session: — (documentation-scope; originally seeded for Session A)
      - Expected (SPEC §4, quoted): fenced block with L9 + L31 (+ L33 "GP14 …
        unwired spare") verbatim lines, each prefixed by its file:line.
      - Observed (docs + firmware, quoted): fenced block with COMMISSIONING.md
        L20 + L96–98, and config.h L25 (`PIN_LED = 14`) + L69
        (`USE_ONBOARD_RGB = true`) verbatim, each with file:line. State plainly:
        the firmware drives BOTH indicators (GP14 external green LED + config-
        gated WS2812 mirror on GP16); COMMISSIONING.md documents both; SPEC §4
        calls the WS2812 the sole indicator. No serial capture exists (all
        sessions BLOCKED — rig unavailable); which indicators physically render
        each pattern was NOT bench-confirmed.
      - Action: documentation reconciliation only — follow-up issue, do NOT edit
        spec/SPEC.md in this changeset (evidence package §6; Mode A).
      - Follow-up issue: proposed: BUG-004 — LED indicator documentation
        (SPEC §4 sole-indicator claim vs GP14 external LED + WS2812 mirror)
      - Status: open (proposed)
  - ADD one entry per mismatch found in Task 0's reconcile step (none expected):
    Session / Expected / Observed (verbatim fenced log excerpt — a REAL excerpt
    only if a real capture exists; else source-file quotes as in D-1) /
    Follow-up issue (proposed: BUG-00n, next free) / Status: open (proposed).

Task 2: FINALIZE the index (docs/bench/README.md §2 table)
  - FOR EACH row A–E (touch ONLY the Date / Evidence file / FW git rev /
    Checklist cells; Status text stays exactly as recorded):
      · Date → the Task-0-recovered date for that row.
      · Evidence file → `— (none; BLOCKED — no capture, integrity rule §1)`
        for BLOCKED rows; the real filename for any row that actually ran.
      · FW git rev → FWREV (same short hash, all rows).
      · Checklist → `not run` for BLOCKED rows; real `pass/total` for any row
        that ran.
  - REPLACE the trailing placeholder line
      "Discrepancies (expected-vs-observed, with follow-up issue numbers):
       `YYYY-MM-DD-discrepancies.md`."
    with the real filename from Task 1.
  - VERIFY: grep -c '^| [A-E] |' docs/bench/README.md   # exactly 5
           grep -n 'PENDING' docs/bench/README.md       # only the §2 vocabulary
                                                        # definition, no row

Task 3: APPEND the finalization note (README §5)
  - ADD after §4, mirroring the doc's voice:
      ## 5. Package finalization (P1.M2.T2.S5)
      - date: <consolidation date>
      - firmware: git <FWREV> (unchanged across all sessions; docs-only commits since)
      - sessions executed: 0/5 — all BLOCKED (rig unavailable; reasons per row above)
      - discrepancies: 1 filed (D-1, proposed BUG-004); physical validation of the
        PRD §9 flows/fault paths and the §10 watchdog check remains OUTSTANDING
        pending rig availability — this package is the honest BUG-003 close-out,
        not a pass.
      - watchdog-patch check: `git status --short firmware/` clean;
        `grep -rn BENCH_HANG_TEST firmware/` — no hits (no leftover patch).
    (Adjust the sessions/discrepancy lines if the live state differs — a row
    that ran gets counted, and its discrepancies summarized.)

Task 4: VALIDATE (Level 1–4 gates below); fix bookkeeping only — never "fix"
  evidence by inventing content. If a gate cannot pass honestly, STOP and report.
```

### Implementation Patterns & Key Details

```markdown
<!-- Index row, finalized (BLOCKED world) — pipe discipline preserved:
| A | Dry bench check + Stage-2 rail measurement (SPEC §9.1–9.2, §4) | 2025-XX-XX | — (none; BLOCKED — no capture, integrity rule §1) | <FWREV> | not run | BLOCKED: rig unavailable — <existing reason preserved VERBATIM> |
-->

<!-- D-1 Observed block shape — every quote carries its source; NO serial lines: -->
- Observed (docs + firmware, quoted):
  ```
  docs/COMMISSIONING.md:20:  … green LED on GP14 via 330 Ω …
  docs/COMMISSIONING.md:96:  **LED cues:** the firmware drives two indicators …
  firmware/jug_filler/config.h:25:  constexpr uint8_t PIN_LED     = 14; …
  firmware/jug_filler/config.h:69:  constexpr bool     USE_ONBOARD_RGB   = true; …
  ```
```

### Integration Points

```yaml
DOCS (the only integration surface):
  - docs/bench/README.md: finalized index — consumed by P1.M2.T3.S2, which
    cross-references it from docs/COMMISSIONING.md "Completion sign-off" (L535+).
  - docs/bench/YYYY-MM-DD-discrepancies.md: the ledger feeding follow-up fixes
    "before any further phase" (item contract); D-1 → proposed BUG-004.
  - Upstream consumed: rows/templates from S1–S4 (read-only); S4's firmware-clean
    guarantee (re-verified here).
FIRMWARE: none (verified untouched: git status + BENCH_HANG_TEST grep).
SPEC: read-only (Mode A). CONFIG/BUILD/ROUTES: none.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
D=$(date +%F)
ls docs/bench/$D-discrepancies.md                 # exists
grep -c '^| [A-E] |' docs/bench/README.md         # exactly 5
grep -n 'YYYY-MM-DD' docs/bench/README.md         # ONLY §3/§4 procedure/convention
                                                  # prose — no unfilled placeholder
                                                  # in the table or trailing line
grep -c '^### D-' docs/bench/$D-discrepancies.md  # ≥1 (D-1 present)
# Expected: all hold. Fix markdown, never content provenance.
```

### Level 2: Unit Tests (Component Validation)

```bash
# No code — component checks are content invariants:
grep -A2 '^### D-1' docs/bench/$(date +%F)-discrepancies.md | grep -q 'BUG-004'   # proposed issue present
grep -q 'Follow-up issue' docs/bench/$(date +%F)-discrepancies.md                 # every entry carries one
grep -n 'sole status/fault indicator' docs/bench/$(date +%F)-discrepancies.md     # SPEC quote present
grep -n 'PIN_LED     = 14\|USE_ONBOARD_RGB' docs/bench/$(date +%F)-discrepancies.md  # firmware quotes present
# Expected: every grep hits; D-1 quotes are verbatim (spot-check against the files).
```

### Level 3: Integration Testing (System Validation)

```bash
git status --short
# Expected exactly:
#  M docs/bench/README.md
#  ?? docs/bench/<date>-discrepancies.md
# (+ plan/… bookkeeping only). NOTHING under firmware/, spec/, test/,
# platformio.ini, README.md (root), docs/COMMISSIONING.md, or template-* files.

git status --short firmware/      # empty
grep -rn BENCH_HANG_TEST firmware/ || echo "watchdog patch: absent"   # absent
ls docs/bench/*.log 2>/dev/null || echo "no captures (correct for BLOCKED world)"

# Index honesty: no row PENDING (only the vocabulary definition may mention it)
awk -F'|' '/^\| [A-E] \|/{print $2, $NF}' docs/bench/README.md | grep -v PENDING
# Expected: 5 rows printed, each ending in PASS/PARTIAL/BLOCKED/SKIPPED with reason.
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Downstream-consumer rehearsal — what P1.M2.T3.S2 must be able to do:
#   (a) find the finalized package from the repo root in one hop:
grep -n 'Package finalization' docs/bench/README.md        # §5 note present
#   (b) follow the discrepancies link and find D-1 + issue:
grep -n 'discrepancies.md' docs/bench/README.md            # names the real file
#   (c) read a verdict without opening anything else:
sed -n '/## 5. Package finalization/,$p' docs/bench/README.md | head -12
# Expected: date, FWREV, 0/5 sessions BLOCKED, 1 discrepancy (BUG-004 proposed),
# watchdog-patch verification lines.

# Anti-fabrication audit: the ledger quotes only real source lines.
grep -n 'spec/SPEC.md\|COMMISSIONING.md\|config.h' docs/bench/$(date +%F)-discrepancies.md
# Expected: each quoted line cites its file; ZERO lines resembling serial output
# (no 'JugFiller boot', 'S1=', 'VALVE', 'FILL start' anywhere in the ledger).
grep -cE 'JugFiller boot|^S1=|VALVE |FILL start' docs/bench/$(date +%F)-discrepancies.md   # 0
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green
- [ ] `git status --short firmware/` empty; `BENCH_HANG_TEST` absent from firmware/
- [ ] `git status` scope = README.md + the new discrepancies file (+ plan/ bookkeeping)

### Feature Validation

- [ ] Ledger exists at `docs/bench/$(date +%F)-discrepancies.md` with D-1 recorded as observed (verbatim SPEC/COMMISSIONING/config.h quotes + file:line), honestly caveated (bench confirmation outstanding), follow-up `proposed: BUG-004`
- [ ] Every expected-vs-observed mismatch from S1–S4 consolidated (reconcile step documented; current reality: D-1 only)
- [ ] Index fully finalized: 5 rows, real dates, one FW rev, no placeholder, no PENDING, recorded BLOCKED reasons verbatim; trailing link names the real file; §5 note complete
- [ ] No `.log` file created or edited; no session template touched

### Code Quality Validation

- [ ] Markdown table structure intact (7 columns, one line per row, pipes balanced)
- [ ] Template rules honored: ledger derived from template-discrepancies.md (copy-then-fill), quotes verbatim in fenced blocks
- [ ] Anti-patterns below avoided

### Documentation & Deployment

- [ ] Package legible stand-alone to P1.M2.T3.S2 (sign-off cross-reference consumer)
- [ ] Verdict line states plainly that physical validation remains outstanding (no false "pass" implication)

## Anti-Patterns to Avoid

- ❌ Never fabricate, synthesize, or "illustrate" serial-log content — no captures exist; source-file quotes with citations are the only evidence in the ledger
- ❌ Never edit spec/SPEC.md, docs/COMMISSIONING.md, root README.md, firmware/, test/, platformio.ini, tasks.json, or any `template-session-*` / `template-rails.md` file
- ❌ Never rewrite, shorten, or paraphrase a row's recorded Status reason — fill empty cells, replace placeholders, preserve text
- ❌ Never renumber D-1 or drop the pre-seeded entry's provenance
- ❌ Never mark any row SKIPPED (only row E may ever carry it, and only its owner's text); never leave a row PENDING
- ❌ Never create a second discrepancies file for the same day — check for an existing one first (append if present)
- ❌ Never "fix" a failing validation gate by inventing content — fix bookkeeping or STOP and report
- ❌ Never fill sibling sessions' templates to make the index look complete — BLOCKED means unfilled is correct

---

**Confidence Score: 9/10** — every input was read from disk this session: the live index (all rows BLOCKED with recorded reasons, placeholder cells enumerated), the discrepancy template with its pre-seeded D-1 and binding rules, the evidence package §3/§4/§6 (ledger layout, integrity rule, the record-don't-fix mandate), the verbatim D-1 quote set across SPEC.md L9/L31/L33 / COMMISSIONING.md L20/L96–98/L127 / config.h L25/L69, the session-finalizing commits and date-recovery commands, the uniform firmware-rev fact (`git log -1 --format=%h -- firmware/`), BUG-004's verified availability, the parallel-S4 boundary (row E + firmware-clean guarantee), and the downstream T3.S2 contract (sign-off anchor L535+, input = this package). The residual risk is drift between PRP-time and implementation-time state (e.g., a rig appearing so S4 runs for real) — handled by the Task-0 live-state audit and the generalize-don't-assume wording throughout.
