# PRP — P1.M1.T2.S1: Replace 'BTN short 1234ms' with a <1000 ms value (BUG-002)

## Goal

**Feature Goal**: Make every `BTN short <n>ms` example in the shipped docs a value that can actually appear on the log stream (n < 1000), eliminating the contradiction between the documented example and the ButtonClassifier it documents.

**Deliverable**: Two single-token edits — README.md:49 (serial-log table BTN row, the primary deliverable) and docs/COMMISSIONING.md:90 (same impossible value in prose, required for this item's own verification gate). `1234ms` → `340ms` in both; nothing else changes.

**Success Definition**: The §6 grep gate passes: every `BTN short <n>ms` in README.md + docs/COMMISSIONING.md has n < 1000; every `BTN long <n>ms` has n ≥ 1000; `git status` shows only those two files modified.

## User Persona

**Target User**: A non-author operator/commissioner reading README's serial-log table to interpret live BTN lines during calibration (§9 workflow).

**Use Case / Journey**: operator sees `BTN short 340ms` on the stream, looks it up in the README table, finds a matching example — today's 1234ms example implies a short press can last >1s, teaching the wrong §5.1 boundary.

**Pain Points Addressed**: doc example that can never occur (1234 ms hold always logs `BTN long`), which corrupts the calibration operator's mental model of the short/long boundary.

## Why

- BUG-002 (Minor, bug report §h3.1): README.md:49 shows `BTN short 1234ms`; classification is short < 1000 ms, and the logged hold is the raw edge-to-edge span tracking the debounced hold within bounce — so 1234 ms is impossible for a short.
- Ground truth chain: spec/SPEC.md §5.1 (L117-126) short < 1s / long ≥ 1s → config.h:28 `BUTTON_LONG_PRESS_MS = 1000` → ButtonClassifier (jug_filler.ino:150, `holdMs >= 1000` → LongPress) → BTN emitter (jug_filler.ino:1685, raw edge-to-edge hold). All verified in `plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md` §2/§4.
- COMMISSIONING.md:90 carries the same `BTN short 1234ms` (verified: exactly two occurrences in shipped docs). The sibling P1.M1.T1.S1 (COMMISSIONING.md L434/L436 recmin fix, running in parallel) explicitly excludes BTN examples — its PRP says README:49 is this task's scope — so if this item fixed only the README, the item's own §6 gate ("assert … everywhere" across both files) would still fail, and P1.M2.T3.S1's re-run of the same check would inherit a known failure.
- Firmware behavior unaffected (documentation-only; no code, no tests read these files).

## What

Replace the literal `BTN short 1234ms` with `BTN short 340ms` at its exactly two occurrences (README.md:49 table row; docs/COMMISSIONING.md:90 prose). Keep `BTN long 1500ms` everywhere (valid: 1500 ≥ 1000). Change nothing else in the README row, table, or either file.

### Success Criteria

- [ ] README.md:49 reads `| \`BTN short 340ms\` / \`BTN long 1500ms\` | BOOT press, classified on release (hold = raw edge-to-edge, diagnostic) |` — rest of row byte-identical
- [ ] docs/COMMISSIONING.md:90 reads `…BOOT button (\`BTN short 340ms\`,` — rest of line and L91 byte-identical
- [ ] `grep -rn 'BTN short 1234' README.md docs/ firmware/ test/` → no hits
- [ ] §6 gate (awk one-liner below) exits 0; manual review: 6 BTN matches — short 340×2 + 350 (COMMISSIONING.md:153, untouched) and long 1500×3
- [ ] `git status --porcelain` → exactly ` M README.md` and ` M docs/COMMISSIONING.md`; `git diff --stat` → 2 files, +2/−2 lines, single-token hunks
- [ ] No firmware/test/config/spec/plan files touched

## All Needed Context

### Context Completeness Check

✅ Passes "No Prior Knowledge" test: the full before/after lines are quoted verbatim below; both target strings are unique per file (grep-verified); the verification command is executable as written; no code knowledge required beyond the quoted ground-truth chain.

### Documentation & References

```yaml
- file: README.md
  why: L49 is the primary edit site (Serial logging table, lines 41-78). Read the row before editing.
  pattern: markdown table row, all ASCII — no em-dash/arrow traps.
  gotcha: change ONLY the number inside the first backtick pair; keep `BTN long 1500ms`.

- file: docs/COMMISSIONING.md
  why: L90-91 prose carries the second `BTN short 1234ms` (L91: "`BTN long 1500ms`; the ms value is the raw hold time, diagnostic only).").
  pattern: bullet prose, parenthesized examples.
  gotcha: sibling task edits L434/L436 of this same file in parallel — do NOT touch anything near Stage 5a; L90 is far away, no conflict.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md
  section: "§2/§4" (classifier + emitter ground truth), "§6" (verification recipe)
  why: the normative fix + gate this PRP implements.

- file: spec/SPEC.md
  section: "5.1 BOOT button — acts on RELEASE" (L117-126) — short < 1s, long ≥ 1s. Read-only.

- file: plan/001_53984f832581/bugfix/001_db6d94af3d0d/P1M1T1S1/PRP.md
  why: the parallel sibling's contract — proves COMMISSIONING.md:90 is NOT in its scope (it fixes only L434/L436), so it is in this item's scope via the §6 gate.

- url: (none needed — pure static doc edit; no library or API surface involved)
```

### Current Codebase tree (relevant excerpt)

```bash
water-jug-filler/
├── README.md                    # L49 ← EDIT (BTN table row)
├── docs/
│   └── COMMISSIONING.md         # L90 ← EDIT (BTN prose); L434/L436 = sibling's, DO NOT TOUCH
├── firmware/jug_filler/         # read-only (config.h:28, jug_filler.ino:150/1685 = cited truth)
├── test/                        # read-only; no test reads README/COMMISSIONING (verified)
└── plan/001_53984f832581/bugfix/001_db6d94af3d0d/  # orchestrator-owned, read-only
```

### Desired Codebase tree with files to be added and responsibility of file

```bash
# NO files added. README.md and docs/COMMISSIONING.md each get a 1-token modification.
```

### Known Gotchas of our codebase & Library Quirks

```bash
# CRITICAL: edit by unique string, not by line number — line numbers shift if the sibling's
#   COMMISSIONING.md edit lands first. `BTN short 1234ms` is unique in each file (verified).
# GOTCHA: do NOT "improve" the row while there (no wording, spacing, or punctuation changes) —
#   P1.M2.T3.S1 diffs against this state.
# GOTCHA: value must be < 1000 STRICTLY (340 chosen: item's suggested value, unused, ASCII).
#   Never pick 1000 (that's `BTN long` by the >= boundary).
# NOTE: `BTN long 1500ms` stays — 1500 >= 1000 is valid (item contract point 3).
```

## Implementation Blueprint

### Data models and structure

None — static documentation edit.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: RED — prove the defect is detectable (TDD)
  - RUN: grep -oE 'BTN (short|long) [0-9]+ms' README.md docs/COMMISSIONING.md \
        | awk '{n=substr($3,1,length($3)-2)+0; if(($2=="short"&&n>=1000)||($2=="long"&&n<1000)){print "FAIL: "$0;bad=1}} END{exit bad}'
  - EXPECT: prints "FAIL" twice (README.md BTN short 1234ms, COMMISSIONING.md BTN short 1234ms), exit 1.

Task 1: EDIT README.md (primary deliverable)
  - REPLACE: "| `BTN short 1234ms` / `BTN long 1500ms` |"
       WITH: "| `BTN short 340ms` / `BTN long 1500ms` |"
  - Everything else in the row/table byte-identical. Use the exact-match edit tool.

Task 2: EDIT docs/COMMISSIONING.md (gate-closure instance, same defect BUG-002)
  - REPLACE: "BOOT button (`BTN short 1234ms`,"
       WITH: "BOOT button (`BTN short 340ms`,"
  - Rest of L90/L91 untouched; do NOT touch L434/L436 (sibling's Stage 5a fix).

Task 3: GREEN — re-run Task 0 command
  - EXPECT: exit 0, no FAIL lines. Then eyeball the raw matches:
    grep -n 'BTN short\|BTN long' README.md docs/COMMISSIONING.md
    → short: 340 (README:49), 340 (COMMISSIONING:90), 350 (COMMISSIONING:153);
      long: 1500 ×3 (README:49, COMMISSIONING:91, :154).

Task 4: SCOPE CHECK
  - RUN: git status --porcelain && git diff --stat && grep -rn 'BTN short 1234' README.md docs/ firmware/ test/ || true
  - EXPECT: exactly ` M README.md`, ` M docs/COMMISSIONING.md`; +2/−2 total; grep no hits.
```

### Implementation Patterns & Key Details

```bash
# The only "logic" is the boundary rule, quoted for verification:
#   short ⇔ hold < BUTTON_LONG_PRESS_MS (1000)   — logged value is raw edge-to-edge
#   long  ⇔ hold >= 1000
# 1234 ≥ 1000 ⇒ "short 1234ms" impossible. 340 < 1000 ⇒ possible. 1500 ≥ 1000 ⇒ valid long.
```

### Integration Points

```yaml
DOCS:
  - README.md L49 consumed downstream by P1.M2.T3.S1 (changeset-level README sweep) which
    re-runs the identical grep gate — this item must leave it green.
  - docs/COMMISSIONING.md: L90 fixed here; L434/L436 belong to P1.M1.T1.S1 (no overlap).
CODE / CONFIG / TESTS: none — no firmware, test, or config change of any kind.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Markdown sanity: the edited lines remain well-formed table/prose lines (no stray pipes/backticks):
sed -n '49p' README.md && sed -n '90,91p' docs/COMMISSIONING.md
# Expected: quoted before/after lines from Tasks 1-2, exactly.
```

### Level 2: Unit Tests (Component Validation)

```bash
# The doc test (RED→GREEN gate) — the awk command from Task 0/3. Expected after fix: exit 0.
# Native firmware suite unaffected but cheap to confirm untouched: pio test -e native
# (optional — this item cannot affect it; no firmware files modified).
```

### Level 3: Integration Testing (System Validation)

```bash
git diff README.md docs/COMMISSIONING.md
# Expected: two hunks, each a single-token change 1234→340 inside the first BTN backticks.
git status --porcelain   # Expected: only the two modified files.
```

### Level 4: Creative & Domain-Specific Validation

```bash
# Full §6 recipe, both directions:
grep -n 'BTN short\|BTN long' README.md docs/COMMISSIONING.md
# Assert by inspection: every short value < 1000; every long value >= 1000 (340,340,350; 1500×3).
grep -rn 'BTN short 1234' README.md docs/ firmware/ test/ || echo "IMPOSSIBLE VALUE GONE OK"
```

## Final Validation Checklist

### Technical Validation
- [ ] Level 1–4 pass; awk gate exit 0; `1234` grep clean
- [ ] `git diff --stat` → 2 files, +2/−2, single-token hunks only

### Feature Validation
- [ ] README.md:49 shows `BTN short 340ms` / `BTN long 1500ms`; row otherwise byte-identical
- [ ] COMMISSIONING.md:90 shows `BTN short 340ms`; L91 and Stage-5a lines untouched
- [ ] All BTN examples repo-wide (README + COMMISSIONING) respect the <1000 / ≥1000 boundary

### Code Quality Validation
- [ ] Edit-by-unique-string used (line numbers treated as hints, not anchors)
- [ ] No scope creep: no rewording, reflowing, or "improvements" to either doc

### Documentation & Deployment
- [ ] This subtask IS the doc fix (Mode A); no further doc obligations; no env vars

---

## Anti-Patterns to Avoid

- ❌ Don't change the wording/format of the row or prose — single token swap only
- ❌ Don't replace `BTN long 1500ms` — it is valid (1500 ≥ 1000; item contract point 3)
- ❌ Don't pick 1000 ms as the "short" value — that instant is `long` by the ≥ boundary
- ❌ Don't touch COMMISSIONING.md L434/L436 (sibling P1.M1.T1.S1's Stage 5a fix) or any firmware/test/spec/plan file
- ❌ Don't fix only README.md — the §6 gate spans docs/COMMISSIONING.md too; L90 has the same defect
- ❌ Don't skip the RED run (Task 0) — the gate failing before the edit is the whole test

---

**Confidence Score: 10/10** — two unique-string single-token edits with verbatim before/after lines, an executable RED→GREEN gate, and grep-verified occurrence inventory (exactly two, both listed); the only parallel-work interaction (sibling edits different lines in the same file) is explicitly bounded.
