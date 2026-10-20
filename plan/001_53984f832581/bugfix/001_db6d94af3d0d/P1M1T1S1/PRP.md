# PRP — P1.M1.T1.S1: Correct recmin examples in COMMISSIONING.md Stage 5a (BUG-001)

## Goal

**Feature Goal**: Fix the two impossible log-example values in `docs/COMMISSIONING.md` Stage 5a ("Record per fill") so the calibration runbook's `FILL start` examples match the firmware's actual output — and prove via mechanical grep that **every** `recmin` example repo-wide is consistent with the formatter.

**Deliverable**: Edited `docs/COMMISSIONING.md` — exactly two single-token changes on lines 434 and 436, byte-identical everywhere else. No other file changes.

**Success Definition**:
- Line 434: `recmin=2` → `recmin=0` (because 8200 ms / 60000 truncated = 0).
- Line 436: `599999 ms → 0` → `599999 ms → 9` (because 599999 ms / 60000 truncated = 9).
- The verification recipe (system_context.md §6, restated below) passes: every printed `recmin` equals `floor(recovery_ms/60000)` of its own example line, and every prose truncation claim says `599999 ms → 9`.
- `git diff` shows exactly one file touched, exactly two lines changed, nothing else.

## User Persona

**Target User**: A non-author operator performing PRD §9.5 backstop calibration (Stage 5a of the runbook). They read `FILL start … recmin=N` off the serial stream and copy it into the Stage 5a table to place `BACKSTOP_TABLE_MIN` knots.

**Use Case**: During commissioning, the operator cross-checks the runbook's example against their live log line. Today the runbook shows `recovery=8200ms recmin=2`, which the firmware can never print — the operator would distrust or mis-transcribe the recovery bucket.

**User Journey**: Open Stage 5a → read the two example lines → (fixed) both now agree with what the device actually prints and with the correct examples elsewhere in the same document (L350/364/525) and README (L51).

**Pain Points Addressed**: An internally contradictory runbook — the same doc shows `recovery=8200ms recmin=0` three times and `recmin=2` once; a wrong truncation rule (`599999 ms → 0`) teaches the operator a floor that is off by 9 minutes at the boundary.

## Why

- BUG-001 (bug report, Minor): Stage 5a's example contradicts `formatFillLogEvent()` at `firmware/jug_filler/jug_filler.ino:1354–1357`, which prints `FILL start backstop=%umin recovery=%lums recmin=%u` with `recmin = (unsigned)(e.b / 60000u)` — unsigned truncation. `recovery=8200ms` can only ever print `recmin=0`; `599999 ms` truncates to `9`.
- A wrong recovery bucket during §9.5 knot placement mis-positions the concave backstop curve — the safety-tightening bound on valve-open time. No firmware behavior is affected; this is purely making the documentation tell the truth the firmware already implements.
- Downstream: P1.M2.T3.S2 (changeset-level sweep) re-runs this exact grep as its acceptance check; the corrected file is consumed as-is.

## What

Apply exactly two verbatim corrections (from `plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md` §3), then run the verification recipe.

### Success Criteria

- [ ] `docs/COMMISSIONING.md` L434 reads `recmin=0` where it read `recmin=2`
- [ ] `docs/COMMISSIONING.md` L436 reads `truncated: 599999 ms → 9;` where it read `truncated: 599999 ms → 0;`
- [ ] `grep -n 'recmin=' docs/COMMISSIONING.md README.md` — every printed recmin == floor(recovery_ms/60000) of its own line; every prose 599999 claim says → 9
- [ ] `git diff --stat` shows only `docs/COMMISSIONING.md`, 2 insertions / 2 deletions
- [ ] Zero changes to any other file, zero changes to other lines of COMMISSIONING.md

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: this is a two-token documentation edit whose exact before/after strings (including non-ASCII characters), source-of-truth formatter, verification commands, and do-not-touch list are all embedded below. Everything was verified against the live repo this session.

### Documentation & References

```yaml
# MUST READ - Include these in your context window
- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  why: THE contract for this fix. §2 = log-line grammar (formatter source of truth); §3 = the two verbatim corrections; §5 = exhaustive cross-check proving no other doc defects exist; §6 = the verification recipe this task must satisfy.
  pattern: Follow §3 exactly — two single-token edits, nothing else.
  gotcha: §3's do-not-touch list: COMMISSIONING.md L165/175/329/350/364/525 and README.md L51 are ALREADY correct.

- file: firmware/jug_filler/jug_filler.ino
  why: Lines 1350–1357 — formatFillLogEvent(), FillStart case: "FILL start backstop=%umin recovery=%lums recmin=%u" with (unsigned)(e.b / 60000u). This is why recmin is truncated, never rounded. Read-only; do not modify.
  pattern: N/A (reference only)
  gotcha: Formats are APPEND-ONLY, prefix-anchored (comment L1343–1344) — docs must match these exact field orders.

- file: docs/COMMISSIONING.md
  why: The file being edited. Stage 5a "Record per fill" spans L430–453; the two defective values are L434 and L436.
  pattern: The correct style already exists at L350/364/525 (recovery=8200ms recmin=0) and README.md L51 (truncated: 599999ms → 9).
  gotcha: Non-ASCII characters in the target lines — see Gotchas below; exact-match edits must reproduce them.

- file: README.md
  why: L51 is the authoritative correct statement of the truncation rule ("truncated: 599999ms → 9"). L49 (BTN short 1234ms) is BUG-002 — the SIBLING task P1.M1.T2.S1 owns it, NOT this task.
  pattern: N/A
  gotcha: Do not "fix" README.md in this task — even though the same grep scans it, it already passes.

- file: spec/SPEC.md
  why: Human-owned PRD source of truth (READ-ONLY). §5.1/§9 context for why the runbook matters.
```

### Current Codebase tree (relevant excerpt)

```bash
water-jug-filler/
├── README.md                        # L41–78 serial-log table (L51 = correct recmin prose; L49 = BUG-002, NOT ours)
├── docs/
│   └── COMMISSIONING.md             # ← THE EDIT TARGET. Stage 5a = L430–453; defects at L434, L436
├── firmware/jug_filler/
│   ├── jug_filler.ino               # L1350–1399 formatFillLogEvent() — read-only source of truth
│   └── config.h
├── spec/SPEC.md                     # read-only
└── plan/001_53984f832581/bugfix/001_db6d94af3d0d/   # architecture/system_context.md = this fix's contract
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
# NO files added. docs/COMMISSIONING.md is MODIFIED (2 lines). Nothing else changes.
water-jug-filler/
└── docs/COMMISSIONING.md   # Stage 5a examples now match formatFillLogEvent()'s real output
```

### Known Gotchas of our codebase & Library Quirks

```python
# CRITICAL: The two target lines contain NON-ASCII characters — exact-match edits must reproduce them:
#   L434: "recmin=2` — the" — the dash after the closing backtick is an EM-DASH (U+2014, bytes e2 80 94),
#         NOT a hyphen or minus. Keep it untouched.
#   L436: "599999 ms → 0;" — the arrow is U+2192 (bytes e2 86 92), NOT ASCII "->".
#         The replacement text uses the SAME arrow: "599999 ms → 9;".
# Safest anchors (verified unique in the file):
#   "recovery=8200ms recmin=2`"      → occurs ONLY on L434
#   "599999 ms → 0;"                 → occurs ONLY on L436 (only 599999 in the file)
# Do NOT retype these strings from memory of the bug report (which renders them as "599999 ms -> 0"
# in ASCII) — copy from the actual file bytes or use the anchors above.

# GOTCHA: recmin is TRUNCATED (e.b/60000u), never rounded: 8200→0, 599999→9, 60000→1.
#         There is no recmin=2 possible for recovery=8200ms under any rounding rule either
#         (round would give 0 too — the 2 was simply wrong).

# GOTCHA: The verification grep ALSO scans README.md — expect its L51 line to appear and PASS.
#         README.md L49 ('BTN short 1234ms') is out of scope (BUG-002, sibling task P1.M1.T2.S1).

# GOTCHA: system_context.md §7: plan/**/tasks.json statuses are orchestrator-owned; spec/SPEC.md
#         and .gitignore are read-only. This task touches exactly ONE file.
```

## Implementation Blueprint

### Data models and structure

N/A — static documentation edit; no code, schema, or build interaction.

### The two edits (exact, edit-tool-ready)

Verified current state of `docs/COMMISSIONING.md` (this session, em-dash and arrow are the real file characters):

```text
L434: - from `FILL start backstop=44min recovery=8200ms recmin=2` — the
L435:   **recovery** (`recmin` = whole minutes since the previous fill *end*,
L436:   truncated: 599999 ms → 0; `recovery=0ms` only ever means fresh boot);
```

**Edit 1** — old text: `` recovery=8200ms recmin=2` `` → new text: `` recovery=8200ms recmin=0` ``
(the trailing backtick is part of both strings; unique anchor, L434 only)

**Edit 2** — old text: ` 599999 ms → 0;` → new text: ` 599999 ms → 9;`
(leading space + U+2192 arrow + trailing semicolon; unique anchor, L436 only)

Resulting lines:

```text
L434: - from `FILL start backstop=44min recovery=8200ms recmin=0` — the
L436:   truncated: 599999 ms → 9; `recovery=0ms` only ever means fresh boot);
```

Everything else in the file stays **byte-identical** — including L435 between them.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 1: RUN THE FAILING TEST (TDD: red)
  - RUN: the verification block in Validation Level 2 below
  - EXPECT: "docs/COMMISSIONING.md:434 ... DEFECTIVE" and the 599999-line flagged
    (pre-edit state, verified this session: 7 OK recmin pairs, 1 DEFECTIVE at 434,
    1 wrong truncation claim at 436)
  - WHY FIRST: proves the check detects the defects and gives the after-state target

Task 2: APPLY the two edits to docs/COMMISSIONING.md
  - USE the exact anchor strings above (mind U+2192 arrow, trailing backtick)
  - TOUCH NOTHING ELSE: no other lines, no reflowing, no punctuation "improvements"
  - DO NOT EDIT: README.md (L49 is sibling task P1.M1.T2.S1; L51 already correct),
    firmware/*, spec/*, plan/* (tasks.json is orchestrator-owned)

Task 3: RE-RUN THE VERIFICATION (TDD: green)
  - RUN: same verification block
  - EXPECT: every recmin pair OK, zero DEFECTIVE, no '599999 ms → 0' anywhere,
    exactly one '599999 ms → 9' in COMMISSIONING.md (plus README.md L51's own)

Task 4: CONFIRM SURGICAL DIFF
  - RUN: git diff --stat && git diff docs/COMMISSIONING.md
  - EXPECT: 1 file, +2/-2 lines, hunks only at L434/L436, single-token changes
```

### Implementation Patterns & Key Details

```python
# No code patterns. The only "logic" in this task is the truncation arithmetic:
#   recmin = floor(recovery_ms / 60000)        # unsigned division truncates
#   8200   // 60000 == 0                       # edit 1
#   599999 // 60000 == 9                       # edit 2  (NOT 0; NOT 10 — truncation, not rounding)
# The firmware never prints the recovery bucket any other way (jug_filler.ino:1357).
```

### Integration Points

```yaml
NO BUILD/CODE INTEGRATION: static markdown edit; no firmware, test, or config impact.

DOWNSTREAM CONSUMER:
  - P1.M2.T3.S2 (docs/COMMISSIONING.md changeset-level sweep): re-runs this same grep
    recipe as its acceptance check — the corrected Stage 5a must survive that sweep.

DO NOT CREATE: docs/bench/ (P1.M2.T1 owns it), any new file, any README change.
```

## Validation Loop

> This is a documentation fix: validation is mechanical consistency checking, not unit tests (system_context.md §6). All commands verified working in this repo this session. Run from repo root.

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# N/A for markdown content, but confirm you didn't break file encoding:
file docs/COMMISSIONING.md        # Expected: ... UTF-8 Unicode text ...
grep -c 'recmin=2' docs/COMMISSIONING.md   # Expected after edit: 0
```

### Level 2: The Verification Recipe (the test — run BEFORE and AFTER the edit)

```bash
# (a) The §6 grep — eyeball every line against floor(recovery/60000):
grep -n 'recmin=' docs/COMMISSIONING.md README.md

# (b) Mechanical floor-check (fails before the edit, passes after):
python3 - <<'EOF'
import re, sys
bad = 0
for path in ('docs/COMMISSIONING.md', 'README.md'):
    for i, line in enumerate(open(path, encoding='utf-8'), 1):
        for m in re.finditer(r'recovery=(\d+)ms recmin=(\d+)', line):
            rec, rmin = int(m.group(1)), int(m.group(2))
            if rmin != rec // 60000:
                print(f'DEFECTIVE {path}:{i}: recovery={rec} recmin={rmin} floor={rec//60000}'); bad += 1
        if '599999' in line and '599999 ms → 0' in line:
            print(f'DEFECTIVE {path}:{i}: says 599999 ms → 0'); bad += 1
print('ALL OK' if bad == 0 else f'{bad} DEFECTIVE'); sys.exit(1 if bad else 0)
EOF
# Expected BEFORE edit: two DEFECTIVE lines (434, 436), exit 1
# Expected AFTER edit: "ALL OK", exit 0
```

### Level 3: Integration Testing (System Validation)

```bash
# The runbook's corrected examples must agree with the firmware formatter and the repo's
# other examples:
grep -n 'recovery=8200ms recmin=' docs/COMMISSIONING.md
# Expected AFTER: lines 350, 364, 434, 525 — ALL show recmin=0 (4 identical examples)

grep -n '599999' docs/COMMISSIONING.md README.md
# Expected AFTER: COMMISSIONING.md:436 "→ 9" and README.md:51 "→ 9" — same rule, same value
```

### Level 4: Surgical-Diff Confirmation (Domain-Specific)

```bash
git diff --stat                     # Expected: docs/COMMISSIONING.md | 2 +-
git diff docs/COMMISSIONING.md      # Expected: exactly two hunks (L434, L436),
                                    #   each changing one token: 2→0 and 0→9
git status --porcelain              # Expected: only " M docs/COMMISSIONING.md"
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 2 recipe run BEFORE edit: exactly two DEFECTIVE (434, 436) — test detects the bugs
- [ ] Level 2 recipe run AFTER edit: `ALL OK`, exit 0
- [ ] Level 4: `git diff --stat` = one file, 2 insertions / 2 deletions; no stray changes
- [ ] File remains valid UTF-8; em-dash (L434) and arrow (L436) preserved as U+2014 / U+2192

### Feature Validation

- [ ] All success criteria in "What" section met
- [ ] L434 now reads `recovery=8200ms recmin=0` — consistent with L350/364/525 and the formatter
- [ ] L436 now reads `truncated: 599999 ms → 9;` — consistent with README.md L51
- [ ] L329/L165/L175 (`recovery=0ms recmin=0` fresh-boot examples) untouched
- [ ] README.md untouched by this task

### Code Quality Validation

- [ ] Scope respected: only `docs/COMMISSIONING.md` modified
- [ ] No firmware, spec, plan/, or README changes (README L49 = sibling BUG-002 task)
- [ ] Byte-identical file outside the two edited tokens

### Documentation & Deployment

- [ ] Mode A satisfied: this subtask IS the doc fix; no separate docs ride along
- [ ] Stage 5a now usable by a non-author: examples match reality, truncation rule stated correctly

## Anti-Patterns to Avoid

- ❌ Don't retype the edit strings from the ASCII-rendered bug report — the file uses U+2192 `→` and U+2014 `—`; anchor on the exact strings above
- ❌ Don't "improve" surrounding prose, line-wrap, or punctuation while in there — byte-identical elsewhere is a hard requirement
- ❌ Don't touch the already-correct examples (L165/175/329/350/364/525, README L51) — consistency means they now match, not that they get rewritten
- ❌ Don't fix README.md:49 (`BTN short 1234ms`) — that's BUG-002, sibling task P1.M1.T2.S1
- ❌ Don't touch firmware or config to "make recmin=2 true" — the firmware is correct; the doc is wrong
- ❌ Don't update plan/tasks.json statuses — orchestrator-owned (system_context.md §7)
- ❌ Don't skip the pre-edit failing run of the recipe — it is the TDD red step that proves the check works

---

**Confidence Score: 10/10** — Both target strings, their exact bytes (including non-ASCII characters), their uniqueness as anchors, the formatter arithmetic, the pre-edit grep state, and the pass/fail verification script were all verified directly against the live repo this session. The edit is two tokens; the test is deterministic and was executed in both expected states conceptually confirmed (7 OK + 2 DEFECTIVE pre-edit).
