# Research notes — P1.M2.T3.S2 (COMMISSIONING.md changeset-level sweep + sign-off cross-reference)

All facts verified against the live repo this session (tool reads, not memory).

## 1. The edit target — docs/COMMISSIONING.md (557 lines)

Structure (grep-anchored headings):
- `## Stage 4 — Dry-run flows (spec §9.4)` L318; `### Flow 1 — abort and fresh timer` L324; `### Flow 2 — successful fill` L359; `### Guard faults` L403 — these flows are what bench sessions B/C/D exercised (item research note; matches docs/bench/README.md §2 rows B/C/D).
- `## Completion sign-off` L535. Table header L537–538, items 1–14 = L539–552, Source column cites Stages 0/1/2/3a–3d/4/5a/5c/5d.
  - Item 10 = Flow 1, item 11 = Flow 2 (both Source: Stage 4). No table item covers fault codes or watchdog — those live in Stage 4 "Guard faults" subsection and the optional §10 check; the cross-reference note is where they get pointed at docs/bench/.
- L553 blank; L554–557 closing note paragraph: "**Closing note:** every commissioned value lives in one place — / `firmware/jug_filler/config.h` (spec §8, single source of truth). Commit the / file with your measured values so the rig's calibration survives the next / flash. Nothing else in the repo needs to change."
- Exact bytes at tail: em-dash U+2014 after "one place", "§" U+00A7, en-dash U+2013 in table item 5 "min–max" and item 13 value "…" (U+2026). File is UTF-8.

Link idiom in this file: backticked text + relative path from docs/ — e.g. L8 ``[`firmware/jug_filler/config.h`](../firmware/jug_filler/config.h)``, L10 `[README](../README.md)`. So the correct relative target for the bench index is `bench/README.md` (same directory).

Prose wrap width ~76–78 cols (L555 = 76). Bold lead-ins are house style ("**Closing note:**", "**Understand these first-fill lines:**").

Currently ZERO occurrences of "docs/bench" in COMMISSIONING.md (grep verified) — the cross-reference is entirely new.

## 2. Mechanical recipe state (re-run result, this session) — BOTH PASS

Recipe R1 (recmin / truncation):
- `recovery=0ms recmin=0` at L165, L329 (+ prose L175) — 0//60000=0 ✓
- `recovery=8200ms recmin=0` at L350, L364, L434, L525 — 8200//60000=0 ✓ (P1.M1.T1.S1's BUG-001 fix, on disk)
- `599999 ms → 9` at L436 ✓ (note the SPACE between "599999" and "ms", unlike README's "599999ms" — the recipe regex must allow `\s*`)

Recipe R2 (BTN):
- L90–91: `BTN short 340ms` / `BTN long 1500ms` ✓
- L153–154: `BTN short 350ms` / `BTN long 1500ms` ✓

Expected outcome of the sweep: pass with zero residual fixes. Residual fixes are the conditional fallback only (sibling drift).

## 3. The bench evidence package (input from P1.M2.T2.S5; read-only)

docs/bench/ on disk: README.md (91 lines, index + capture procedure), template-session-{A..E}-*.md, template-rails.md, template-discrepancies.md, and the consolidated ledger `2026-10-19-discrepancies.md`.

docs/bench/README.md key facts:
- §1 integrity rule (binding): genuine captures only; if rig unavailable, record `BLOCKED: <reason>` — never fabricate. So BLOCKED rows are a legitimate recorded state, NOT a defect to paper over.
- §2 session index, one row per session A–E: A=dry bench + Stage-2 rails (§9.1–9.2, §4); B=Flow 1 abort+restart (§9.4, §6.1); C=Flow 2 success through DONE/ACK (§9.4, §7); D=fault paths baseline/no-confirm/optional guards (§7); E=optional watchdog (§10). Columns: Date / Evidence file / FW git rev / Checklist / Status.
- Status vocabulary: PENDING / PASS / PARTIAL(n✗) / BLOCKED: reason / SKIPPED: reason.
- Ledger naming convention: `YYYY-MM-DD-discrepancies.md`.
- LIVE state at PRP time: all five rows BLOCKED (rig unavailable — no RP2040-Zero serial device on this host; no physical/multimeter access), dated 2026-10-19, FW rev 290e7e8.

⇒ Consequence for the cross-reference wording: it must be STATE-AGNOSTIC — point at the location and its artifact kinds (session index, discrepancy ledger), never claim PASS/capture, never restate dates/statuses/revs. The S5 parallel-finalized state may change (re-runs); COMMISSIONING.md must not race it.

## 4. Parallel-sibling boundary (P1.M2.T3.S1, in flight)

Its PRP (read in full): edits README.md only (repo-layout line + one pointer sentence; recipe re-run over README). Explicit hand-off: "P1.M2.T3.S2 sweeps docs/COMMISSIONING.md and cross-references the same package — no shared edit surface, no conflicts." Disjoint git scope; do not touch README.md from here (S1 may also be mid-write). Both siblings deliberately use the same state-agnostic linking discipline.

## 5. Ownership / scope fences

- system_context.md §7: tasks.json orchestrator-owned; spec/SPEC.md, PRD, .gitignore read-only; no firmware changes in this changeset.
- docs/bench/* is S5's write surface — existence-check + read only.
- The closing note's final sentence ("Nothing else in the repo needs to change.") is about committing calibration values; per item logic ("make no other content changes") it stays byte-identical even though docs/bench/ now exists.
- The sign-off table rows/items 1–14 and their numbering are the anchor contract from the item definition — a table NOTE (sentence after the table) is the sanctioned insertion form; never add/reorder table rows.

## 6. Drafted and verified commands

Recipe script (COMMISSIONING.md variant — allows the space in "599999 ms"):

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
```

Verified against live file: exits 0, "recipes OK".

Pure-insertion diff gate: `git diff docs/COMMISSIONING.md` must show one hunk, N insertions / 0 deletions.

Link-target gate: `test -f docs/bench/README.md` (relative target `bench/README.md` resolves from docs/).

State-leak gate: `grep -nE 'BLOCKED|PENDING|PARTIAL|SKIPPED|PASS|20[0-9]{2}-[0-9]{2}-[0-9]{2}|290e7e8' docs/COMMISSIONING.md` → no new hits in the added lines (pre-existing prose must be unchanged, so any hit is a leak; note "PASS" may appear inside words — use word-boundary review of hits).
