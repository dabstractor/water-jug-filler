# PRP — P1.M2.T3.S1 (bugfix changeset): README.md changeset-level sweep — serial-log consistency and bench-evidence surfacing

## Goal

**Feature Goal**: Land the **final state of the repository's front page** for this bugfix changeset: a README.md that (a) passes both mechanical serial-log consistency recipes re-run over the whole file, and (b) surfaces the new BUG-003 deliverable — the bench validation evidence package under `docs/bench/` — in the repo-layout section so it is discoverable from the front page, by **linking** to `docs/bench/` without restating any session detail.

**Deliverable**: MODIFIED `README.md` (repo root) — exactly two changeset-level touches:
1. One new line in the "Repo layout" fenced block (section at pre-edit L7–14): `docs/bench/` + a short description, matching the block's column alignment.
2. One short pointer sentence appended to the end of the "Commissioning notes (PRD §9)" paragraph (the prose just below the serial-log table, ending "…(bench records from the commissioning session)." before `## Safety note`), linking to `docs/bench/`.
Plus (only if a recipe gate fails, which live verification says it will NOT): the minimal residual fix inside the serial-log examples.

The serial-log table itself (rows at pre-edit L41–78) stays **exactly as fixed by P1.M1.T2.S1** — no row edits.

**Success Definition**: A fresh reader landing on the repo front page can (1) see `docs/bench/` in the repo layout and reach the evidence index in one hop; (2) trust every serial-log example in the table, because the two recipes pass mechanically over the whole file (verified by script, not eyeball); (3) see zero session detail (no dates, statuses, discrepancy counts) duplicated in README — that lives in `docs/bench/`. `git diff` shows only README.md touched (plus plan/ bookkeeping), with no serial-log table row modified.

## User Persona (if applicable)

**Target User**: A maintainer or evaluator entering the repository cold (post-changeset review, audit of BUG-001/002/003 closure, or future contributor).

**Use Case**: "What's in this repo, was the bench validation ever done, and can I trust the documented log formats?" — answered from the front page in under a minute: layout row → `docs/bench/` → index; log table → recipes verified.

**User Journey**: Open README.md → repo-layout block now lists `docs/bench/` alongside firmware/test/platformio.ini → reader knows the evidence package exists and where its index lives → (optionally follows the pointer sentence from the commissioning notes) → opens `docs/bench/README.md` for the session index, discrepancy ledger, and finalization verdict — README never duplicates that state.

**Pain Points Addressed**: BUG-003's deliverable (`docs/bench/`) was invisible from the front page (no `docs/` entry in the layout block at all); residual risk that P1.M1.T2.S1's example fixes drifted or missed an occurrence.

## Why

- Item contract (P1.M2.T3.S1): re-run both recipes over README.md (fix residual if any — that is this subtask's purpose); then apply only changeset-level touches — add `docs/bench/` to the repo-layout section; leave the serial-log table otherwise as fixed by P1.M1.T2.S1; do not restate session detail, link/point to `docs/bench/`.
- Bug-fix PRD Recommendations (h2.5): "capture the serial logs under `docs/bench/`" and fix contradictory example values — this task is the front-page half of closing that loop (COMMISSIONING.md half is P1.M2.T3.S2).
- Changeset coherence: the repo gained a new top-level deliverable directory (`docs/bench/`, from P1.M2.T1–T2); a front page whose layout block omits it is stale the moment the changeset lands.
- Downstream: P1.M2.T3.S2 (COMMISSIONING.md sweep) cross-references the finalized package — a coherent README and COMMISSIONING.md together close P1.M2.T3.

## What

Pure documentation sweep (Mode B — this IS the changeset-level documentation task for README.md; MOCKING: none).

1. **Recipe gate (re-run, whole file)**:
   - Recipe R1 (recmin): every `recmin=` example and every `Xms → N` truncation example in README.md must satisfy `floor(value_ms / 60000) == N`.
   - Recipe R2 (BTN): every `BTN short <N>ms` must have `N < 1000`; every `BTN long <N>ms` must have `N >= 1000` (`BUTTON_LONG_PRESS_MS = 1000`, config.h:28; hold is raw edge-to-edge).
   - Live verified state (research, this session): both PASS — L49 `BTN short 340ms` / `BTN long 1500ms`; L51 `recovery=0ms recmin=0` and `(truncated: 599999ms → 9)`; L73 bare `recmin` mention with no numeric example (exempt). Expected outcome: pass, no fix needed. If any occurrence fails, apply the minimal correction inside that example (short value → any < 1000; long value → any ≥ 1000; recmin/truncation target → `floor(ms/60000)`) — this is the task's stated purpose, not scope creep.
2. **Touch 1 — repo-layout line** (mandatory): add one entry to the fenced block under `## Repo layout`:
   ```
   docs/bench/            Bench validation evidence — index, capture procedure, discrepancy ledger
   ```
   Alignment: name field padded to 23 chars (existing rows: `firmware/jug_filler/`+3 spaces, `test/`+18, `platformio.ini`+9 — descriptions all start at char 24). `docs/bench/` is 11 chars ⇒ **12 trailing spaces** before the description.
3. **Touch 2 — pointer sentence** (mandatory, single sentence, state-agnostic): append to the end of the "Commissioning notes (PRD §9)" paragraph, after "(bench records from the commissioning session).":
   ```markdown
   Captured bench validation evidence (session index, discrepancy ledger): [`docs/bench/`](docs/bench/README.md).
   ```
   Style matches the file's existing markdown-link idiom (e.g. [`firmware/jug_filler/config.h`](firmware/jug_filler/config.h) at L5). **No dates, no session statuses, no BLOCKED/PASS wording, no counts** — the parallel task P1.M2.T2.S5 owns that state and may still be writing it.
4. **NOT in scope**: any edit to serial-log table rows (L41–78); docs/COMMISSIONING.md (P1.M2.T3.S2); adding a `docs/COMMISSIONING.md` line to the layout block (README never listed it; only `docs/bench/` is this changeset's new deliverable); spec/SPEC.md (Mode A, read-only); firmware/, test/, platformio.ini; docs/bench/* contents (read-only existence check only); tasks.json (orchestrator-owned).

### Success Criteria

- [ ] Recipe gates R1 and R2 pass mechanically over the final README.md (script-verified)
- [ ] Repo-layout block contains a `docs/bench/` entry, column-aligned like its siblings
- [ ] Commissioning-notes paragraph ends with the one-line pointer to `docs/bench/`
- [ ] Zero session detail (dates / statuses / verdicts) restated in README.md
- [ ] `git diff` shows the serial-log table rows byte-identical; only the layout line + pointer sentence (+ any recipe residual fix) changed
- [ ] Link target exists: `docs/bench/README.md` present and unmodified by this task

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the PRP carries the exact pre-edit line anchors (layout L7–14, BTN L49, recmin L51/L73, safety note L79), the live recipe-pass state with per-occurrence values, the exact new-line text with computed column padding (23-char name field), the exact pointer-sentence text and placement anchor, the parallel-S5 boundary (state-agnostic wording; no writes to docs/bench/), and runnable verification commands for every gate. All facts were read from disk this session (see `research/readme-sweep-research.md`).

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: README.md
  why: THE artifact. Pre-edit anchors: `## Repo layout` L7 (fenced block
        L9–L13, three entries, no docs/ line); serial-log table header ~L41,
        BTN row L49 (`BTN short 340ms` / `BTN long 1500ms` — P1.M1.T2.S1's
        BUG-002 fix, on disk); L51 FILL-start example (`recovery=0ms recmin=0`,
        "truncated: 599999ms → 9"); L73 bare `recmin` in commissioning notes
        (no numeric value — exempt from R1); `## Safety note` L79.
  pattern: match the layout block's column alignment (23-char name field) and
        the file's [`text`](path) link idiom (see config.h link at L5).
  gotcha: line numbers shift +1 after Touch 1 — use grep anchors, not line
        numbers, for post-edit verification.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T2S5/PRP.md
  why: CONTRACT of the parallel prior item (running concurrently). It finalizes
        docs/bench/README.md (rows A–E, date-stamped discrepancies ledger,
        §5 finalization note). Consequences for us: (a) README must link
        state-agnostically — no dates/statuses/verdicts (they'd race or
        duplicate S5); (b) docs/bench/ is read-only for us — never open for
        write; (c) S5's git scope is docs/bench/ + plan/, ours is README.md —
        no overlap, both valid in one `git status`.

- file: docs/bench/README.md
  why: Link target (existence check only — read-only). "Bench validation
        evidence — index and capture procedure": §1 integrity rule, §2 session
        index (rows A–E), §4 conventions naming the discrepancy ledger
        `YYYY-MM-DD-discrepancies.md`. The README pointer sentence's words
        ("session index, discrepancy ledger") deliberately name artifacts this
        file defines, without quoting its live state.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  section: §2 "Log-line grammar — firmware source of truth" + §5 cross-check
  why: The two mechanical recipes, from firmware ground truth: `holdMs >=
        BUTTON_LONG_PRESS_MS(=1000, config.h:28) ? LongPress : ShortPress`
        ⇒ logged `BTN short` values are ALWAYS < 1000 ms, `BTN long` ≥ 1000;
        recmin derived as `recovery_ms/60000` (truncated) — 599999 ms → 9.
        These are the gate definitions R1/R2 below.

- file: firmware/jug_filler/config.h
  section: L28 (BUTTON_LONG_PRESS_MS = 1000)
  why: The threshold behind recipe R2. Read-only reference; never edited.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M2T3S1/research/readme-sweep-research.md
  why: This session's verified facts: live recipe-pass state, column-alignment
        math (name field 23 chars; `docs/bench/` + 12 spaces), S5 boundary,
        scope fences, drafted verification commands.
```

### Current Codebase tree (run `tree` in the root of the project) to get an overview of the codebase

```bash
water-jug-filler/
├── README.md                  # ← THIS TASK'S ONLY EDIT TARGET (root)
├── platformio.ini
├── firmware/jug_filler/{jug_filler.ino,config.h}
├── test/                      # native Unity tests (214)
├── spec/SPEC.md               # read-only (Mode A)
└── docs/
    ├── COMMISSIONING.md       # P1.M2.T3.S2's sweep target — NOT ours
    └── bench/                 # ← the deliverable to surface (read-only for us)
        ├── README.md          # index (S5 finalizing in parallel)
        ├── template-session-{A..E}-*.md, template-rails.md
        └── template-discrepancies.md   # (S5 adds *-discrepancies.md + edits index)
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
water-jug-filler/
├── README.md                  # MODIFIED — layout line + pointer sentence
│                              #   (+ minimal recipe-residual fix IF a gate fails)
└── (nothing else changes — no new files from this task)
```

### Known Gotchas of our codebase & Library Quirks

```text
# CRITICAL — parallel S5 race: P1.M2.T2.S5 is finalizing docs/bench/README.md
#   CONCURRENTLY. Never read-modify-write docs/bench from here, and never bake
#   its transient state (BLOCKED rows, dates, verdicts) into README prose. The
#   pointer sentence is deliberately stateless: it names the directory's
#   artifact kinds, not their contents.

# CRITICAL — leave the serial-log table alone: rows L41–78 are P1.M1.T2.S1's
#   verified fix (BUG-002). Post-edit `git diff README.md` must show ZERO
#   changed lines matching '^|' (table rows) unless a recipe gate actually
#   failed — in which case only the offending example value changes, in place.

# GOTCHA — layout-block alignment: descriptions start at char 24 (name field
#   padded to 23). `docs/bench/` (11 chars) + 12 spaces. Misalignment is the
#   classic review nit in this block; the existing three rows define the rule.

# GOTCHA — line anchors shift after Touch 1 (+1 line): re-locate sections by
#   grep ('## Repo layout', '## Safety note', 'Commissioning notes'), never by
#   the pre-edit line numbers, when verifying post-edit.

# GOTCHA — recipe R1's exempt case: L73 mentions `recmin` inside backticks
#   WITHOUT '=' and without a numeric example ("set BACKSTOP_TABLE_MIN from
#   `recmin` vs the fill duration"). It is prose, not an example — do not
#   "fix" it, do not count it as an occurrence.

# GOTCHA — tasks.json is orchestrator-owned; do not update statuses
#   (system_context.md §7). plan/ bookkeeping noise in git status is fine.

# GOTCHA — the repo has NO markdown linter / no ruff / no mypy: validation is
#   grep + python3 regex + git diff scope checks (Level 1–3 below). python3 is
#   available (used by the toolchain); plain stdlib only, no pip installs.
```

## Implementation Blueprint

### Data models and structure

No code. One markdown file edit: one line inside an existing fenced code block, one sentence appended to an existing paragraph, and (conditionally) in-place example-value corrections inside table rows. No structure changes, no new files.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: LIVE-STATE AUDIT (before any edit)
  - RUN: grep -n '## Repo layout' README.md && sed -n '/## Repo layout/,/```$/p' README.md | head -8
    # Confirm exactly three entries, no docs/ line. If a docs/bench/ line already
    # exists (sibling drift), Touch 1 collapses to an alignment/wording check.
  - RUN the recipe gate (Task 3's python script) — record pass/fail per occurrence.
    # Expected today: PASS on all (BTN 340/1500; recovery=0ms recmin=0;
    # 599999ms → 9; L73 exempt).
  - RUN: test -f docs/bench/README.md && echo "link target OK"
  - CHECK: git status --short   # know your starting diff (plan/ bookkeeping OK)

Task 1: TOUCH 1 — repo-layout line
  - EDIT README.md fenced block under `## Repo layout`: add as the LAST entry
      docs/bench/            Bench validation evidence — index, capture procedure, discrepancy ledger
    (name field padded to 23 chars — `docs/bench/` + 12 spaces; description
    starts at char 24 like its siblings.)
  - KEEP: the three existing entries byte-identical; block still fenced.

Task 2: TOUCH 2 — pointer sentence in commissioning notes
  - LOCATE the paragraph starting "Commissioning notes (PRD §9):" (prose after
    the serial-log table, before `## Safety note`).
  - APPEND, as a new final sentence after "(bench records from the commissioning
    session).", on the same paragraph flow:
      Captured bench validation evidence (session index, discrepancy ledger): [`docs/bench/`](docs/bench/README.md).
  - FORBIDDEN in this sentence (and anywhere in README): session dates, run
    statuses (PASS/BLOCKED/SKIPPED/PENDING), counts, firmware revs, verdicts.

Task 3: RECIPE RESIDUAL FIX (ONLY if Task 0 recorded a failure — expected: NO)
  - FOR each failing occurrence, minimal in-place correction:
      · `BTN short Nms` with N ≥ 1000 → any value < 1000 (house style: 340).
      · `BTN long Nms` with N < 1000 → any value ≥ 1000 (house style: 1500).
      · `recovery=Xms recmin=N` mismatch → N := X // 60000.
      · `Xms → N` truncation mismatch → N := X // 60000.
  - TOUCH nothing else in the row's Meaning column.

Task 4: VALIDATE (all four levels below); fix markdown mechanics only — never
  widen scope to "improve" other README prose. If a gate cannot pass without an
  out-of-scope edit, STOP and report.
```

### Implementation Patterns & Key Details

```markdown
<!-- Repo-layout block AFTER Touch 1 (alignment: descriptions start at char 24):
```
firmware/jug_filler/   Arduino sketch (jug_filler.ino + config.h) — the only firmware files
test/                  Native host unit tests (Unity), run without any hardware
platformio.ini         PlatformIO build (board) + host-test (native) configuration
docs/bench/            Bench validation evidence — index, capture procedure, discrepancy ledger
```
-->

<!-- Commissioning-notes paragraph tail AFTER Touch 2 (last two sentences):
… record rails with a multimeter (bench records from the commissioning
session). Captured bench validation evidence (session index, discrepancy
ledger): [`docs/bench/`](docs/bench/README.md).
-->
```

### Integration Points

```yaml
DOCS:
  - README.md (root): the only write surface. Consumed by every human reader;
    the layout row + pointer make docs/bench/ discoverable in one hop.
  - docs/bench/README.md: link TARGET ONLY (existence-checked, never written;
    S5 owns its content concurrently).
  - Hand-off to P1.M2.T3.S2: it sweeps docs/COMMISSIONING.md and cross-references
    the same package — no shared edit surface with this task, no conflicts.
FIRMWARE: none (read-only config.h:28 grounds recipe R2).
SPEC / CONFIG / BUILD / ROUTES: none.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Layout block integrity: 4 entries, aligned, still fenced
sed -n '/## Repo layout/,/^```$/p' README.md | grep -c '^\(firmware\|test\|platformio\|docs\)'   # 4
awk '/## Repo layout/,/^```$/' README.md | grep '^docs/bench/' | grep -qE '^docs/bench/ {12}\S'  # 12-space pad

# Table untouched: row count and key rows byte-identical to pre-edit
grep -c '^| `' README.md                          # 21 (unchanged)
grep -n 'BTN short 340ms' README.md               # present, still short<1000

# Expected: all hold. Fix markdown mechanics (spacing, fence) — never content.
```

### Level 2: Unit Tests (Component Validation) — the mechanical recipe gate

```bash
python3 - <<'EOF'
import re
text = open('README.md').read()
fails = []
for m in re.finditer(r'BTN (short|long) (\d+)ms', text):
    kind, ms = m.group(1), int(m.group(2))
    if (kind == 'short') != (ms < 1000):
        fails.append(('R2', m.group(0)))
for m in re.finditer(r'recovery=(\d+)ms recmin=(\d+)', text):
    rms, rmin = int(m.group(1)), int(m.group(2))
    if rms // 60000 != rmin:
        fails.append(('R1', m.group(0)))
for m in re.finditer(r'(\d+)ms → (\d+)', text):
    ms, n = int(m.group(1)), int(m.group(2))
    if ms // 60000 != n:
        fails.append(('R1-trunc', m.group(0)))
print("FAIL:", fails) if fails else print("recipes OK: R1 + R2 pass")
raise SystemExit(1 if fails else 0)
EOF
# Expected: "recipes OK: R1 + R2 pass". If FAIL: apply Task 3 corrections,
# re-run until green. (Live state at PRP time: passes with zero corrections.)
```

### Level 3: Integration Testing (System Validation)

```bash
# Diff scope: ONLY the two touches (+ any Task-3 residual fix) — table rows intact
git diff README.md | grep '^[+-]' | grep -v '^[+-][+-]'        # review the touched lines
git diff README.md | grep '^[+-]|' | grep -v '^[+-][+-]'       # expect EMPTY (no table rows)
# (If Task 3 ran: exactly the corrected example's row may appear here — one row.)

# Link target live (read-only check; S5 may be mid-write — content irrelevant)
test -f docs/bench/README.md && echo "docs/bench/README.md exists"

# State-agnostic wording: no bench session state leaked into README
grep -nE 'BLOCKED|PENDING|PASS|SKIPPED|20[0-9]{2}-[0-9]{2}-[0-9]{2}' README.md | grep -v '115200'
# Expected: no hits (115200 baud is the only digit-string that may match)

# Whole-repo scope
git status --short
# Expected: ' M README.md' (+ plan/… bookkeeping, and S5's docs/bench/ files if
# the parallel task landed — those are S5's, do not touch or revert them).
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Fresh-reader rehearsal (the persona's journey, end to end):
#   (a) front page lists the deliverable:
grep -A5 '## Repo layout' README.md | grep 'docs/bench/'
#   (b) one hop to the index:
test -f docs/bench/README.md && sed -n '1,3p' docs/bench/README.md
#   (c) commissioning-notes reader is pointed at the evidence:
grep -n 'Captured bench validation evidence' README.md
# Expected: (a) the aligned layout line; (b) the index header renders; (c) the
# pointer sentence present exactly once.

# Coherence read-through: sed -n '1,20p' README.md and the Serial-logging
# section tail — confirm no duplicated session detail and unchanged table prose.
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–4 gates green (recipe script exits 0; diff-scope greps clean)
- [ ] `grep -c '^| `' README.md` = 21 — serial-log table rows byte-identical (or exactly one corrected row if Task 3 ran)
- [ ] `git status` scope: README.md only, beside plan/ bookkeeping (and S5's parallel docs/bench/ changes, untouched)

### Feature Validation

- [ ] Recipes R1 (recmin = floor(ms/60000), incl. "→ N" truncation examples) and R2 (BTN short < 1000 ≤ long) pass over the whole file
- [ ] Repo-layout block carries the aligned `docs/bench/` entry
- [ ] Commissioning-notes paragraph ends with the single state-agnostic pointer sentence
- [ ] Link target `docs/bench/README.md` exists; no docs/bench file modified by this task
- [ ] Zero session detail (dates/statuses/verdicts/revs) in README.md

### Code Quality Validation

- [ ] Layout line matches the block's 23-char name-field alignment
- [ ] Pointer sentence matches the file's [`text`](path) link idiom
- [ ] Anti-patterns below avoided

### Documentation & Deployment

- [ ] README reads as the coherent final front page: layout ↔ table ↔ notes ↔ safety note consistent
- [ ] No environment variables, no build steps, no deployment surface affected

## Anti-Patterns to Avoid

- ❌ Don't restate bench session detail (dates, BLOCKED/PASS statuses, discrepancy counts, firmware revs) in README — link, don't duplicate; S5 owns that state in parallel
- ❌ Don't touch the serial-log table rows beyond a proven recipe failure — P1.M1.T2.S1's fix is verified on disk
- ❌ Don't edit docs/bench/ (any file), docs/COMMISSIONING.md, spec/SPEC.md, firmware/, test/, platformio.ini, or tasks.json
- ❌ Don't add a `docs/COMMISSIONING.md` line "for symmetry" — only `docs/bench/` is this changeset's new deliverable; COMMISSIONING.md sweep is P1.M2.T3.S2's
- ❌ Don't "improve" unrelated README prose while in there — two touches (plus conditional residual fix), nothing else
- ❌ Don't verify by eyeball — run the recipe script and the diff-scope greps
- ❌ Don't revert or "clean up" the parallel task's docs/bench/ changes if they appear in git status

---

**Confidence Score: 9/10** — every input was read from disk this session: the live README (anchors L7–14/L41–78/L49/L51/L73/L79, both recipes passing with the exact on-disk values 340/1500, 0→0, 599999→9), the alignment math of the layout block (23-char name field, `docs/bench/`+12 spaces), the S5 PRP contract in full (parallel boundary: state-agnostic link, zero writes to docs/bench/, disjoint git scope), the bench index's artifact vocabulary (session index / discrepancy ledger — the words the pointer sentence uses), and the system_context recipes with their config.h:28 ground truth. Residual risk is sibling drift between PRP time and implementation (e.g., S5 or T3.S2 landing first) — bounded by the Task-0 live audit, the grep-anchored (not line-numbered) post-edit checks, and the explicit "if it already exists, collapse Touch 1 to an alignment check" rule.
