# PRP — P1.M2.T3.S2 (bugfix changeset): docs/COMMISSIONING.md changeset-level sweep and sign-off cross-reference to docs/bench/

## Goal

**Feature Goal**: Land the **final state of the calibration runbook** for this bugfix changeset: a `docs/COMMISSIONING.md` that (a) passes both mechanical serial-log consistency recipes re-run over the whole file, and (b) gains exactly one short cross-reference in the **Completion sign-off** section pointing at the captured-evidence location `docs/bench/` — covering flows 1/2, the fault blink codes, and the optional watchdog check — with **no other content changes**.

**Deliverable**: MODIFIED `docs/COMMISSIONING.md` — exactly one insertion: a single short "table note" paragraph placed **after the sign-off table's row 14 and before the `**Closing note:**` paragraph**, linking to the bench evidence index. Plus (only if a recipe gate fails, which live verification says it will NOT): minimal in-place residual fixes inside log examples.

**Success Definition**: A commissioning operator or auditor reaching the end of the runbook can, from the sign-off section itself, hop to `docs/bench/README.md` (one click via the relative link) and find the session index + discrepancy ledger for the hardware validation behind sign-off items 10–11 (flows 1/2), the fault-path blink codes, and the optional watchdog check — without COMMISSIONING.md restating any session state (dates, statuses, verdicts, firmware revs). `git diff` shows a pure insertion (plus plan/ bookkeeping); every other byte of the file is identical; both recipes pass mechanically (script-verified, not eyeball).

## User Persona (if applicable)

**Target User**: A non-author operator finishing commissioning (or an auditor closing out BUG-001/002/003 for this changeset) who has just filled in the 14-item Completion sign-off table.

**Use Case**: "I've signed off items 10–11 (flows) — where is the hardware evidence that these actually ran, and where do discrepancies live?" — answered in place, in one hop, without leaving the runbook.

**User Journey**: Read the sign-off table → see the **Bench evidence** note directly below it → follow `[`docs/bench/`](bench/README.md)` → land on the bench index (§2 session rows A–E, ledger link) → per-session status and captured serial logs (or recorded BLOCKED reasons) are there, owned by that package — the runbook never duplicates them.

**Pain Points Addressed**: BUG-003's deliverable (`docs/bench/`) was invisible from the runbook whose sign-off it substantiates; the runbook's "That's commissioning complete." endpoint had no evidence pointer. Residual risk that P1.M1.T1.S1's example fixes drifted or missed an occurrence.

## Why

- Item contract (P1.M2.T3.S2): re-run both mechanical recipes over `docs/COMMISSIONING.md` (must pass; fix residuals if any — that is this subtask's sweep purpose); add a short cross-reference so the completion sign-off section points to the captured evidence location `docs/bench/` (a sentence or table note referencing the bench index, covering flows 1/2, fault codes, and the optional watchdog check); make no other content changes. Mode B — this IS the changeset-level documentation task for `docs/COMMISSIONING.md`. MOCKING: none.
- Bug-fix PRD (h2.3 Issue 3 / h2.5 Recommendations): "capture the serial logs under `docs/bench/`" and fix contradictory example values — this task is the runbook half of closing that loop (front-page half is P1.M2.T3.S1, running in parallel on README.md with a disjoint edit surface).
- system_context.md §7: "docs/COMMISSIONING.md 'Completion sign-off' table (L535+) is the natural anchor for a cross-reference to the captured bench evidence under `docs/bench/`."
- Changeset coherence: the runbook's Stage 4 flows (L318–421: Flow 1, Flow 2, Guard faults) are exactly what bench sessions B/C/D exercised; the optional watchdog check is session E. A runbook whose sign-off section predates the evidence package is stale the moment the changeset lands.
- Downstream: this is the **final state of the runbook for this changeset** (P1.M2.T3 closes with S1+S2); nothing after it edits COMMISSIONING.md.

## What

Pure documentation sweep (no code, no firmware, no build interaction).

1. **Recipe gate (re-run, whole file — MUST pass before and after the edit)**:
   - Recipe R1 (recmin): every `recovery=<R>ms recmin=<N>` example must satisfy `R // 60000 == N`; every prose truncation example `<X> ms → <N>` (any spacing between `X`, `ms`, `→`) must satisfy `X // 60000 == N`.
   - Recipe R2 (BTN): every `BTN short <N>ms` must have `N < 1000`; every `BTN long <N>ms` must have `N >= 1000` (`BUTTON_LONG_PRESS_MS = 1000`, `firmware/jug_filler/config.h:28`; hold is raw edge-to-edge).
   - Live verified state (research, this session): both PASS — BTN L90–91 (`short 340ms`/`long 1500ms`), L153–154 (`short 350ms`/`long 1500ms`); recmin L165/L329 (`0ms→0`), L350/L364/L434/L525 (`8200ms→0`); truncation L436 (`599999 ms → 9`, note the space before `ms`). Expected outcome: pass, no fix needed. If any occurrence fails (sibling drift), apply the minimal in-place correction inside that example only — that is the task's stated sweep purpose, not scope creep.
2. **The cross-reference (mandatory, single insertion)**: one short paragraph placed between the sign-off table's last row and the `**Closing note:**` paragraph, text (UTF-8; em-dashes U+2014, en-dash U+2013 exactly as shown):

   ```markdown
   **Bench evidence:** the hardware validation behind this table — flows 1 and 2
   (items 10–11), the fault blink codes, and the optional watchdog-reset check —
   lives in the bench evidence package at [`docs/bench/`](bench/README.md); see
   its session index and discrepancy ledger for per-session status and captured
   serial-log evidence.
   ```

   - Placement anchor (edit-tool-ready): insert immediately before the unique string `**Closing note:** every commissioned value lives in one place —` (with a blank line before and after the inserted paragraph).
   - Relative link target `bench/README.md` is correct because COMMISSIONING.md lives in `docs/` (same idiom as the file's existing L8/L10 links `[…](../firmware/…)`, `[README](../README.md)`).
   - Style matches the file: bold lead-in (like `**Closing note:**`), ~76-col prose wrap, backticked path text.
   - **State-agnostic wording (hard requirement)**: no dates, no session statuses (`PASS`/`BLOCKED`/`PENDING`/`PARTIAL`/`SKIPPED`), no discrepancy counts, no firmware revs, no "was captured"/"passed" claims — the live bench index currently records all sessions BLOCKED (rig unavailable; its §1 integrity rule makes that a legitimate recorded state), and the parallel task P1.M2.T2.S5 owns that state; COMMISSIONING.md must not race or duplicate it. "Lives in … see its session index and discrepancy ledger" is true under every allowed status.
3. **NOT in scope**: the sign-off table itself (rows/items 1–14, their numbering, the `Y / N` value cells, the Source column — never add a table row); the `**Closing note:**` paragraph including its final sentence "Nothing else in the repo needs to change." (leave byte-identical); README.md (P1.M2.T3.S1's surface, in flight); docs/bench/* (S5's surface — existence check + read only); spec/SPEC.md (Mode A, read-only); firmware/, test/, platformio.ini; tasks.json (orchestrator-owned); any other COMMISSIONING.md prose, headings, or examples.

### Success Criteria

- [ ] Recipe gates R1 and R2 pass mechanically over the final `docs/COMMISSIONING.md` (script-verified, exit 0)
- [ ] Exactly one `docs/bench` reference exists in COMMISSIONING.md — the new note's link (pre-edit count: zero)
- [ ] The note sits between table row 14 and `**Closing note:**`, names flows 1/2 (items 10–11), fault blink codes, and the optional watchdog-reset check, and links `[`docs/bench/`](bench/README.md)`
- [ ] Zero session state (dates/statuses/verdicts/revs/counts) leaked into COMMISSIONING.md
- [ ] `git diff docs/COMMISSIONING.md` shows a pure insertion (one hunk, N insertions / 0 deletions; more only if a recipe residual fix ran, each an in-place single-token change)
- [ ] Link target exists and is untouched: `docs/bench/README.md` present, not modified by this task

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the exact insertion anchor (`**Closing note:** every commissioned value lives in one place —`, verified unique), the verbatim note text including its non-ASCII characters and correct relative link path, the live recipe-pass state with per-occurrence line numbers and values, the S5/parallel-sibling boundaries (state-agnostic wording; disjoint write surfaces), the runnable recipe script (space-tolerant truncation regex), and diff/state-leak gates. All facts were read from disk this session (see `research/commissioning-sweep-research.md`).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: docs/COMMISSIONING.md
  why: THE artifact (557 lines). Key anchors: `## Completion sign-off` L535;
        table header L537–538; items 1–14 = L539–552 (item 10 Flow 1, item 11
        Flow 2, both Source "Stage 4"); L553 blank; L554–557 closing note
        (insertion anchor = its first line). Stage 4 flows L318–421 (Flow 1
        L324, Flow 2 L359, Guard faults L403). Recipe occurrences: BTN L90–91,
        L153–154; recmin L165/175/329/350/364/434/525; truncation L436
        ("599999 ms → 9" — SPACE between number and ms).
  pattern: bold lead-in paragraphs; ~76-col wrap; links as
        [`text`](relative/path) (L8/L10 use ../ from docs/ — bench index is
        same-directory, so bench/README.md).
  gotcha: line numbers shift +N after insertion — verify by grep anchors
        ('## Completion sign-off', '**Closing note:**'), not line numbers.
        Non-ASCII: em-dash U+2014, en-dash U+2013, § U+00A7, … U+2026.

- file: docs/bench/README.md
  why: Link TARGET (existence check + read only — S5's write surface). §1
        integrity rule (BLOCKED-with-reason is legitimate recorded evidence
        state — never "fix" or comment on it from here); §2 session index rows
        A–E (B=Flow 1, C=Flow 2, D=fault paths, E=optional watchdog) and the
        ledger naming `YYYY-MM-DD-discrepancies.md` — the artifact kinds the
        note's words ("session index and discrepancy ledger") deliberately name
        without quoting live state.
  pattern: N/A (reference only).
  gotcha: LIVE state at PRP time: all five rows BLOCKED (rig unavailable,
        2026-10-19, FW rev 290e7e8). This is why the note wording is
        state-agnostic — never bake PASS/capture claims into COMMISSIONING.md.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  section: §2 (log-line grammar: formatFillLogEvent recmin = e.b/60000u
        truncated; BTN holdMs >= 1000 ⇒ long), §5 (exhaustive cross-check —
        only BUG-001/002 existed), §6 (the two mechanical recipes = R1/R2),
        §7 (ownership: tasks.json orchestrator-owned; sign-off table L535+ is
        THE natural cross-reference anchor).
  why: Ground truth for the recipe gates and the do-not-touch fences.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M1T1S1/PRP.md
  why: The completed BUG-001 fix whose output is this task's input ("the
        corrected COMMISSIONING.md"). Its corrections (L434 recmin=0, L436 → 9)
        are on disk and must SURVIVE this sweep byte-identical (no recipe
        residual expected; never "re-fix" what already passes).

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T3S1/PRP.md
  why: CONTRACT of the PARALLEL sibling (in flight). It edits README.md only
        (layout line + pointer sentence + its own recipe re-run) and explicitly
        hands off: "P1.M2.T3.S2 sweeps docs/COMMISSIONING.md … no shared edit
        surface, no conflicts." Consequences: never touch README.md from here;
        expect S1's + S5's files in git status alongside ours — leave them
        alone.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S5/PRP.md
  why: CONTRACT of the input producer ("finalized bench evidence package —
        index + discrepancy list"). S5 finalizes docs/bench/README.md rows and
        the dated ledger; our note consumes the package by LOCATION and
        ARTIFACT KIND only, so it cannot race S5's state.

- file: firmware/jug_filler/config.h
  section: L28 (BUTTON_LONG_PRESS_MS = 1000)
  why: The threshold behind recipe R2. Read-only; never edited.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T3S2/research/commissioning-sweep-research.md
  why: This session's verified facts: live recipe-pass state with line numbers,
        bench index structure and BLOCKED state, link-idiom examples, wrap
        width, drafted verification commands and their expected output.
```

### Current Codebase tree (run `tree` in the root of the project) to get an overview of the codebase

```bash
water-jug-filler/
├── README.md                  # P1.M2.T3.S1's sweep target (parallel) — NOT ours
├── platformio.ini
├── firmware/jug_filler/{jug_filler.ino,config.h}   # read-only ground truth
├── test/                      # native Unity tests (214)
├── spec/SPEC.md               # read-only (Mode A)
└── docs/
    ├── COMMISSIONING.md       # ← THIS TASK'S ONLY EDIT TARGET (557 lines)
    └── bench/                 # the evidence package (S5's surface — read-only)
        ├── README.md          # index: §2 session rows A–E + ledger pointer
        ├── 2026-10-19-discrepancies.md
        └── template-{session-A..E-*,rails,discrepancies}.md
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
└── docs/COMMISSIONING.md      # MODIFIED — one inserted note paragraph in the
                               #   Completion sign-off section
                               #   (+ minimal recipe-residual fix IF a gate fails)
# No new files. Nothing else changes.
```

### Known Gotchas of our codebase & Library Quirks

```text
# CRITICAL — pure insertion, nothing else: the item logic says "make no other
#   content changes". git diff must show ONE hunk of pure insertions (0
#   deletions) unless a recipe gate failed — then each residual is an in-place
#   single-token change (recmin/truncation target := floor(ms/60000); BTN short
#   value := any < 1000, long := any >= 1000).

# CRITICAL — state-agnostic link (parallel S5 + integrity rule): the bench
#   index may legitimately record BLOCKED rows (rig unavailable). The note must
#   never claim evidence "was captured"/"passed", never carry dates, statuses,
#   discrepancy counts, or FW revs. "Lives in … see its session index and
#   discrepancy ledger" is true under every status in the index's vocabulary.

# CRITICAL — leave the table and closing note alone: rows/items 1–14 and their
#   Stage sources are the anchor contract ("items 1-14 sourced from Stages
#   0-5"); a table NOTE (paragraph after the table) is the sanctioned form —
#   never add row 15. The closing note's last sentence ("Nothing else in the
#   repo needs to change.") is about committing calibration values — leave it
#   byte-identical even though docs/bench/ now exists.

# GOTCHA — non-ASCII bytes: the inserted note uses em-dashes (U+2014, bytes
#   e2 80 94) around the appositive and an en-dash (U+2013) in "items 10–11";
#   the file also carries § (U+00A7) and … (U+2026). Write real UTF-8
#   characters, never ASCII "--"/"->" stand-ins. The anchor string
#   "**Closing note:** every commissioned value lives in one place —" ends
#   with an em-dash — copy it from the file, not from memory.

# GOTCHA — line anchors shift +5 after insertion (5-line note + blanks):
#   re-locate the table tail and closing note by grep
#   ('## Completion sign-off', '**Closing note:**'), never by pre-edit numbers.

# GOTCHA — recipe R1's spacing variant: COMMISSIONING.md L436 writes
#   "599999 ms → 9" (space between number and ms) while README writes
#   "599999ms → 9" — the truncation regex must tolerate optional whitespace
#   ((\d+)\s*ms\s*→\s*(\d+)). A space-intolerant script false-passes/fails.

# GOTCHA — relative link path: COMMISSIONING.md lives IN docs/, so the target
#   is bench/README.md (no ../). The file's existing links (L8/L10) show the
#   relative-path idiom; README.md's own docs/bench/ link (from repo root,
#   written by the parallel sibling) uses docs/bench/README.md — both are
#   correct from their respective locations; do not "harmonize" either.

# GOTCHA — tasks.json is orchestrator-owned; do not update statuses
#   (system_context.md §7). plan/ bookkeeping noise in git status is fine, and
#   so are the parallel tasks' files (README.md from S1, docs/bench/ from S5) —
#   never touch or revert them.

# GOTCHA — the repo has NO markdown linter / no ruff / no mypy: validation is
#   grep + python3 regex + git diff scope checks (Level 1–4 below). python3
#   stdlib only, no pip installs.
```

## Implementation Blueprint

### Data models and structure

N/A — static documentation edit; no code, schema, or build interaction. One markdown insertion (and, conditionally, in-place single-token example fixes).

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: LIVE-STATE AUDIT (before any edit)
  - RUN: grep -n 'docs/bench' docs/COMMISSIONING.md
    # Expected: zero hits (pre-edit). If a cross-reference already exists
    # (sibling drift), collapse this task to verifying/repairing it against
    # the required coverage + state-agnostic wording instead of inserting a
    # second one.
  - RUN the recipe gate (Task 3's python script) — record pass/fail per
    occurrence. Expected today: PASS on all (BTN 340/350 short, 1500 long ×2;
    recmin 0→0 ×3, 8200→0 ×4; 599999 ms → 9).
  - RUN: test -f docs/bench/README.md && echo "link target OK"
  - CHECK: git status --short   # starting diff: plan/ bookkeeping + possibly
    # S1's README.md / S5's docs/bench/ changes (parallel) — know them, leave them.

Task 1: THE CROSS-REFERENCE (the one insertion)
  - LOCATE the sign-off section: grep -n '## Completion sign-off' docs/COMMISSIONING.md
    (~L535). The table ends at row "| 14 | Post-tuning attended fill reaches
    `DONE` | `Y / N` | Stage 5d |" (~L552); blank line; then the closing note.
  - INSERT, between the table and the closing note (blank line before and
    after), this exact paragraph (UTF-8, em-dashes/en-dash as real characters):

    **Bench evidence:** the hardware validation behind this table — flows 1 and 2
    (items 10–11), the fault blink codes, and the optional watchdog-reset check —
    lives in the bench evidence package at [`docs/bench/`](bench/README.md); see
    its session index and discrepancy ledger for per-session status and captured
    serial-log evidence.

  - EDIT-TOOL FORM: oldText = "**Closing note:** every commissioned value lives in one place —"
    newText = "<the paragraph above>\n\n**Closing note:** every commissioned value lives in one place —"
    (anchor is unique in the file; it ends with an EM-DASH — copy exact bytes).
  - FORBIDDEN anywhere in the insertion (and everywhere else): dates, run
    statuses (PASS/BLOCKED/PENDING/PARTIAL/SKIPPED), discrepancy counts,
    firmware revs, "was captured"/"passed" claims.
  - TOUCH NOTHING ELSE: no table rows, no closing-note text, no other prose,
    no reflowing, no punctuation "improvements".

Task 2: RECIPE RESIDUAL FIX (ONLY if Task 0 recorded a failure — expected: NO)
  - FOR each failing occurrence, minimal in-place correction:
      · `BTN short Nms` with N >= 1000 → any value < 1000 (house style: 340).
      · `BTN long Nms` with N < 1000 → any value >= 1000 (house style: 1500).
      · `recovery=Xms recmin=N` mismatch → N := X // 60000.
      · `X ms → N` truncation mismatch → N := X // 60000.
  - TOUCH nothing else on the edited line.

Task 3: VALIDATE (all four levels below); fix markdown mechanics only — never
  widen scope to "improve" other runbook prose. If a gate cannot pass without
  an out-of-scope edit, STOP and report.
```

### Implementation Patterns & Key Details

```markdown
<!-- Sign-off section tail AFTER the edit (pure insertion; everything else
     byte-identical). The table's last two rows, then the new note, then the
     unchanged closing note:

| 13 | `BACKSTOP_TABLE_MIN` set (concave) | `{…}` | Stage 5c |
| 14 | Post-tuning attended fill reaches `DONE` | `Y / N` | Stage 5d |

**Bench evidence:** the hardware validation behind this table — flows 1 and 2
(items 10–11), the fault blink codes, and the optional watchdog-reset check —
lives in the bench evidence package at [`docs/bench/`](bench/README.md); see
its session index and discrepancy ledger for per-session status and captured
serial-log evidence.

**Closing note:** every commissioned value lives in one place —
`firmware/jug_filler/config.h` (spec §8, single source of truth). …
-->

# The only "logic" in this task is the recipe arithmetic (unsigned truncation):
#   recmin = floor(recovery_ms / 60000)   → 8200→0, 599999→9, 0→0
#   BTN short ⇔ hold < 1000 (BUTTON_LONG_PRESS_MS, config.h:28)
```

### Integration Points

```yaml
DOCS:
  - docs/COMMISSIONING.md: the only write surface; the runbook's final state
    for this changeset — its sign-off section now closes the loop to evidence.
  - docs/bench/README.md: link TARGET ONLY (existence-checked, read-only;
    S5 owns content). The note names the package's artifact kinds (session
    index, discrepancy ledger) — vocabulary defined by that index's §2/§4.
  - Hand-off from P1.M2.T3.S1 (parallel): disjoint surfaces (README.md vs
    docs/COMMISSIONING.md); both may land in one commit/git status.
FIRMWARE: none (read-only config.h:28 grounds recipe R2).
SPEC / CONFIG / BUILD / ROUTES / DATABASE: none.
DOWNSTREAM: none — P1.M2.T3.S2 is the terminal runbook task of the changeset.
```

## Validation Loop

> Documentation sweep: validation is mechanical consistency checking, not unit tests (system_context.md §6). All commands verified working in this repo this session. Run from repo root.

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Encoding intact after the UTF-8 insertion (em-dash/en-dash written correctly):
file docs/COMMISSIONING.md          # Expected: ... UTF-8 Unicode text ...

# The note is present exactly once, in the right place, linking correctly:
grep -c 'docs/bench' docs/COMMISSIONING.md    # 1
grep -n 'Bench evidence:' docs/COMMISSIONING.md   # exactly one hit, between
        # the row-14 line and the '**Closing note:**' line (grep anchors, not numbers)
grep -o '\[`docs/bench/`](bench/README.md)' docs/COMMISSIONING.md   # exactly one

# Table integrity: still exactly 14 numbered items, closing note intact:
grep -cE '^\| (1[0-4]|[1-9]) \|' docs/COMMISSIONING.md   # 14
grep -c 'Nothing else in the repo needs to change.' docs/COMMISSIONING.md  # 1 (unchanged)
```

### Level 2: Unit Tests (Component Validation) — the mechanical recipe gate

```bash
python3 - <<'EOF'
import re
text = open('docs/COMMISSIONING.md', encoding='utf-8').read()
fails = []
for m in re.finditer(r'BTN (short|long) (\d+)ms', text):
    kind, ms = m.group(1), int(m.group(2))
    if (kind == 'short') != (ms < 1000):
        fails.append(('R2', m.group(0)))
for m in re.finditer(r'recovery=(\d+)ms recmin=(\d+)', text):
    rms, rmin = int(m.group(1)), int(m.group(2))
    if rms // 60000 != rmin:
        fails.append(('R1', m.group(0)))
for m in re.finditer(r'(\d+)\s*ms\s*→\s*(\d+)', text):
    ms, n = int(m.group(1)), int(m.group(2))
    if ms // 60000 != n:
        fails.append(('R1-trunc', m.group(0)))
print("FAIL:", fails) if fails else print("recipes OK: R1 + R2 pass over docs/COMMISSIONING.md")
raise SystemExit(1 if fails else 0)
EOF
# Expected: "recipes OK: R1 + R2 pass over docs/COMMISSIONING.md", exit 0.
# (Live state at PRP time: passes with zero corrections; the space-tolerant
#  truncation regex catches L436's "599999 ms → 9". If FAIL: apply Task 2
#  corrections, re-run until green.)
```

### Level 3: Integration Testing (System Validation)

```bash
# Diff scope: a pure insertion — one hunk, zero deletions (unless Task 2 ran):
git diff --stat docs/COMMISSIONING.md        # 1 file, N insertions(+), 0 deletions
git diff docs/COMMISSIONING.md | grep '^-' | grep -v '^--'   # expect EMPTY
# (If Task 2 ran: exactly the corrected example lines may appear as -/+ pairs.)

# P1.M1.T1.S1's BUG-001 fix survived the sweep byte-identical:
grep -n 'recovery=8200ms recmin=0' docs/COMMISSIONING.md    # 4 hits (350/364/434/525)
grep -n '599999' docs/COMMISSIONING.md                       # 1 hit: "→ 9"

# Link target live (read-only; S5 owns content):
test -f docs/bench/README.md && echo "docs/bench/README.md exists"
# Relative target correctness: link text docs/bench/ ↔ target bench/README.md
# resolves from docs/ (same-directory idiom as the file's ../ links).

# State-agnostic wording: no bench session state leaked (review any hits —
# they must all be pre-existing prose, not new):
grep -nE 'BLOCKED|PENDING|PARTIAL|SKIPPED|20[0-9]{2}-[0-9]{2}-[0-9]{2}|290e7e8' docs/COMMISSIONING.md
grep -nE '\bPASS\b' docs/COMMISSIONING.md   # no hit inside the inserted note

# Whole-repo scope:
git status --short
# Expected: ' M docs/COMMISSIONING.md' (+ plan/… bookkeeping; possibly the
# parallel tasks' README.md / docs/bench/ changes — S1's and S5's, untouched).
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Fresh-reader rehearsal (the persona's journey, end to end):
#   (a) sign-off section still reads coherently with the note in place:
sed -n '/## Completion sign-off/,$p' docs/COMMISSIONING.md   # read the whole tail
#   (b) one hop to the evidence index:
test -f docs/bench/README.md && sed -n '1,3p' docs/bench/README.md
#   (c) coverage triple present in the note (flows, fault codes, watchdog):
grep -n 'Bench evidence:' docs/COMMISSIONING.md | grep -qf /dev/null; \
  sed -n '/Bench evidence:/,+4p' docs/COMMISSIONING.md | \
  grep -c -e 'flows 1 and 2' -e 'fault blink codes' -e 'watchdog-reset check'   # 3

# Coherence cross-check with the sibling front-page pointer (parallel S1):
grep -n 'docs/bench' README.md    # S1's pointer sentence — must exist and be
# consistent in KIND (location pointer), independent in wording; do not edit it.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (recipe script exits 0; diff-scope and state-leak greps clean)
- [ ] `grep -c 'docs/bench' docs/COMMISSIONING.md` = 1; note sits between table row 14 and `**Closing note:**`
- [ ] `git diff docs/COMMISSIONING.md`: one hunk, pure insertion (0 deletions; or +Task-2 in-place single-token fixes)
- [ ] File remains valid UTF-8; em-dashes/en-dash/§ preserved; anchor line's trailing em-dash intact
- [ ] `git status` scope: docs/COMMISSIONING.md only, beside plan/ bookkeeping and the parallel tasks' files (untouched)

### Feature Validation

- [ ] Recipes R1 (recmin = floor(ms/60000), incl. "→ N" truncation with either spacing) and R2 (BTN short < 1000 ≤ long) pass over the whole file
- [ ] The note covers all three required subjects: flows 1/2 tied to sign-off items 10–11, the fault blink codes, the optional watchdog-reset check
- [ ] The note links `[`docs/bench/`](bench/README.md)` — target exists, correct relative path from docs/
- [ ] Zero session state (dates/statuses/verdicts/revs/counts) in the insertion; P1.M1.T1.S1's corrected examples byte-identical
- [ ] Sign-off table rows/items 1–14 and the closing note paragraph byte-identical

### Code Quality Validation

- [ ] Note matches house style: bold lead-in, ~76-col wrap, backticked-path link idiom
- [ ] Anti-patterns below avoided; no scope creep into "improving" runbook prose
- [ ] No new files; no edits to README.md, docs/bench/*, spec/, firmware/, test/, platformio.ini, tasks.json

### Documentation & Deployment

- [ ] This subtask IS the changeset-level documentation task for docs/COMMISSIONING.md (Mode B) — no separate doc ride-along
- [ ] The runbook's sign-off section now answers "where is the hardware evidence?" in one hop; final state for this changeset
- [ ] No environment variables, build steps, or deployment surface affected

## Anti-Patterns to Avoid

- ❌ Don't restate bench session state (dates, BLOCKED/PASS statuses, discrepancy counts, FW revs) in COMMISSIONING.md — link, don't duplicate; S5 owns that state, and BLOCKED is a legitimate recorded outcome under the index's integrity rule
- ❌ Don't add a 15th table row, renumber items, or fill the `Y / N` cells — the table is the anchor contract; the sanctioned form is a note paragraph after the table
- ❌ Don't touch the closing note (including "Nothing else in the repo needs to change.") or any Stage prose beyond a proven recipe failure — pure insertion is the hard requirement
- ❌ Don't retype the anchor or note from the ASCII-rendered item text — the file uses U+2014/U+2013/§; copy exact bytes (the anchor ends with an em-dash)
- ❌ Don't edit README.md, docs/bench/*, or revert the parallel tasks' files if they appear in git status — disjoint surfaces by contract
- ❌ Don't "fix" already-passing examples (L90–91/153–154, L165/175/329/350/364/525, L436) — consistency means they stay as P1.M1.T1.S1 verified them
- ❌ Don't update plan/tasks.json statuses — orchestrator-owned (system_context.md §7)
- ❌ Don't verify by eyeball — run the recipe script and the diff/state-leak greps
- ❌ Don't use a space-intolerant truncation regex — L436 writes "599999 ms → 9"; a `(\d+)ms`-only pattern silently skips it

---

**Confidence Score: 10/10** — every input was read from disk this session: the live COMMISSIONING.md tail (table L539–552, unique closing-note anchor with its em-dash, link idiom at L8/L10, wrap width), the recipe-pass state with per-occurrence values and the L436 spacing quirk (script verified green), the bench index's structure/status vocabulary/live BLOCKED state (motivating the state-agnostic wording), and the two parallel contracts in full (S1's README hand-off, S5's package finalization). Residual risk is sibling drift between PRP time and implementation — bounded by the Task-0 live audit, grep-anchored post-edit checks, and the explicit "if the cross-reference already exists, verify it instead of inserting a second one" rule.
