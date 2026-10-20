# Research — P1.M2.T3.S1 README.md changeset-level sweep

Verified facts (all read from disk this session, 2025 session):

## 1. Current README.md state (input = post-P1.M1.T2.S1)

- Repo-layout section: `## Repo layout` at L7, fenced block L9–L13, three entries
  only (`firmware/jug_filler/`, `test/`, `platformio.ini`). **No `docs/` entry of
  any kind** — docs/bench/ is undiscoverable from the front page today.
- Name-column alignment inside the block: descriptions start at char 24
  (name field padded to 23 chars: `firmware/jug_filler/`+3sp, `test/`+18sp,
  `platformio.ini`+9sp). `docs/bench/` = 11 chars ⇒ +12 spaces.
- Serial-log table: header ~L41; BTN row at **L49**: `` `BTN short 340ms` / `BTN long 1500ms` ``
  — BUG-002 fix from P1.M1.T2.S1 IS on disk (340 < 1000 ✓, 1500 ≥ 1000 ✓).
- recmin occurrences: L51 (`FILL start backstop=30min recovery=0ms recmin=0`, plus
  prose "(truncated: 599999ms → 9)") and L73 (bare `recmin` mention in the
  commissioning-notes paragraph, no numeric example). Both numeric cases pass
  floor(recovery_ms/60000): 0→0 ✓, 599999//60000=9 ✓.
- Commissioning-notes paragraph ends with "(bench records from the commissioning
  session)." just before `## Safety note` at L79. This paragraph is the natural
  landing spot for a one-line bench-evidence pointer.
- Table rows count: `grep -c '^| `' README.md` = 21 (boot/BTN/S1/FILL/BASE/VALVE open/
  TRIP/CONFIRM/DONE/ACK/PLUS5/VALVE close/ABORT/FAULT baseline/earlywet/earlytrip/
  confirm/backstop/RGB + header + separator). Must be unchanged after this task.

## 2. Mechanical recipes (system_context.md §2/§5; item contract)

- recmin = floor(recovery_ms/60000), applies to every `recmin=` example AND every
  "Xms → N" truncation example.
- BTN short ⇒ hold < 1000 ms (BUTTON_LONG_PRESS_MS, config.h:28); BTN long ⇒ ≥ 1000.
  Hold is raw edge-to-edge ⇒ short examples MUST be < 1000.
- **Live result: both recipes PASS on the current README.** The sweep's job is to
  re-run them as the changeset gate (fix-any-residual branch included).

## 3. S5 parallel boundary (P1.M2.T2.S5 PRP, read in full)

- S5 finalizes docs/bench/README.md index rows A–E + creates the date-stamped
  discrepancies ledger + appends §5 finalization note — CONCURRENTLY with this task.
- Consequence: README.md must reference docs/bench/ **state-agnostically** (no
  dates, no statuses, no "0/5 BLOCKED", no discrepancy counts). Never open for
  write, never read-modify-write any docs/bench file from this task.
- S5's git scope = docs/bench/README.md + new ledger; this task's = README.md
  (root). No overlap. Both + plan/ bookkeeping.

## 4. Scope fences (from item contract + plan tree)

- Serial-log TABLE (L41–78 rows) = untouched (P1.M1.T2.S1 owns it; verified fixed).
- docs/COMMISSIONING.md = P1.M2.T3.S2's sweep, not ours. Do not add a
  COMMISSIONING.md line to the layout block either (README never listed it; only
  docs/bench/ is the changeset's new deliverable).
- firmware/, spec/SPEC.md, test/, platformio.ini, tasks.json = untouched.
- "Do not restate session detail in README — link/point to docs/bench/."

## 5. Verification commands (drafted and dry-run logic checked)

- Recipe gate: python3 regex over README.md —
  `BTN (short|long) (\d+)ms` ⇒ short<1000, long≥1000;
  `recovery=(\d+)ms recmin=(\d+)` ⇒ rms//60000==rmin;
  `(\d+)ms → (\d+)` ⇒ v//60000==n.
- Diff-scope gate: `git diff README.md` touched lines must NOT match '^|' (table
  rows) or 'BTN short'; only the layout block + one prose sentence.
- Link gate: `test -f docs/bench/README.md` (read-only existence check).
