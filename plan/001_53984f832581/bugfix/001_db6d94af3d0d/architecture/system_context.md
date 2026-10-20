# System context — bugfix 001_db6d94af3d0d (doc corrections + bench validation)

Synthesized by the breakdown architect from direct code inspection + two read-only
scout runs (2025-10-19, repo state: git `c4ae295`, working tree clean apart from
plan/ artifacts). All PRD claims in the bug report were independently verified.

## 1. Repository state (verified)

- Firmware: `firmware/jug_filler/jug_filler.ino` (~1700 lines) + `firmware/jug_filler/config.h`
  (constexpr constants only — zero `#define`/`#if` in config.h). Builds clean for
  pico env; 214 native Unity tests pass (bug report Overview, not re-run here).
- Docs: `README.md` (repo layout L7, "Serial logging" table L41–78) and
  `docs/COMMISSIONING.md` (22 KB runbook; Stage 5a "Record per fill" = L430–453;
  "Completion sign-off" = L535+).
- `spec/SPEC.md` = human-owned source of truth (READ-ONLY for implementers).
  §5.1 BOOT button at L117–126: long = hold ≥ 1000 ms (`BUTTON_LONG_PRESS_MS`).
- `docs/bench/` does NOT exist (BUG-003). Original plan task
  `plan/001_53984f832581/tasks.json` P1.M7.T2.S1 (status "Researching") holds the
  full original bench contract — recovered verbatim during research; its content is
  restated in `bench_evidence_package.md` in this directory.

## 2. Log-line grammar — firmware source of truth

Pure formatters (seam rule: snprintf only, no Serial/millis), native-testable:

- `formatBootBanner()` — `jug_filler.ino:85–88`:
  `JugFiller boot (reset cause: POWER_OR_RESET|WATCHDOG)`
- `formatFillLogEvent()` — `jug_filler.ino:1350–1399`. Formats are **APPEND-ONLY,
  prefix-anchored** (comment L1343–1344: bench-evidence greps are prefix-anchored).
  Key line (L1354–1357):
  `FILL start backstop=%umin recovery=%lums recmin=%u` with
  `recmin = (unsigned)(e.b / 60000u)` — **unsigned truncation**.
  Therefore: `recovery=8200ms` ⇒ `recmin=0`; `599999 ms` ⇒ `recmin=9`;
  `recovery=0ms ⇒ recmin=0` (fresh boot, table bypassed).
- BTN emitter — `loop()` BOOTSEL poll, `logLine("BTN %s %lums", …)` at
  `jug_filler.ino:1685`; classification `ButtonClassifier` L150:
  `holdMs >= BUTTON_LONG_PRESS_MS(=1000, config.h:28) ? LongPress : ShortPress`.
  Hold is raw edge-to-edge (debounce windows cancel, L104–112) ⇒ a logged
  `BTN short` value is ALWAYS < 1000 ms; `BTN long` values are ≥ 1000 ms.

## 3. BUG-001 — docs/COMMISSIONING.md, exactly two defective values

Stage 5a (L430–453). Verbatim current → corrected:

- **L434**: ``- from `FILL start backstop=44min recovery=8200ms recmin=2` — the``
  → change `recmin=2` to **`recmin=0`** (8200/60000 truncated = 0).
- **L436**: ``  truncated: 599999 ms → 0; `recovery=0ms` only ever means fresh boot);``
  → change `→ 0` to **`→ 9`** (599999/60000 truncated = 9).

Known-good examples that already agree (do not touch): COMMISSIONING.md L329
(`recovery=0ms recmin=0`), L350/L364/L525 (`recovery=8200ms recmin=0`), L165/L175;
README.md L51 already states "truncated: 599999ms → 9".

## 4. BUG-002 — README.md L49, one impossible example value

Verbatim current row:

```
| `BTN short 1234ms` / `BTN long 1500ms` | BOOT press, classified on release (hold = raw edge-to-edge, diagnostic) |
```

→ replace `BTN short 1234ms` with a value **< 1000 ms** (e.g. `BTN short 340ms`;
COMMISSIONING.md L142 uses `S1=…` style and its BTN examples are prose-consistent).
Keep `BTN long 1500ms` (valid: 1500 ≥ 1000). No other row changes.

## 5. Cross-check result (scout-verified, exhaustive)

Every other log example in README.md's serial-log table and every snippet in
docs/COMMISSIONING.md was diffed against the formatters: boot banner, `BASE ok`,
`VALVE open budget=`, `TRIP sensor=… fillms=`, `CONFIRM pass … durms=`, `DONE/ABORT
attemptms/openms`, `FAULT baseline/earlywet/earlytrip/confirm/backstop` (incl. the
`openms=…ms` suffix quirk on FAULT backstop, formatter L1389–1390), `PLUS5
budget=`, `S1=/S2=` 1 Hz line, `ACK done`, `VALVE close` — **all PASS**. The only
defects in the repo's docs are BUG-001 (two values) and BUG-002 (one value).

## 6. Verification recipe (the "test" for doc fixes)

Doc fixes are verified by mechanical consistency greps, not unit tests:
1. `grep -n "recmin=" docs/COMMISSIONING.md README.md` — every printed `recmin`
   value must equal floor(recovery_ms/60000) of the same example line; every
   prose truncation claim must say 599999 ms → 9.
2. `grep -n "BTN short" README.md docs/COMMISSIONING.md` — every `BTN short
   <n>ms` example must have n < 1000; every `BTN long <n>ms` must have n ≥ 1000.

## 7. Out of scope / ownership notes for implementers

- `plan/001_53984f832581/tasks.json` statuses are **orchestrator-owned** — the bug
  report's optional recommendation to update them is NOT a task for PRP agents.
- `spec/SPEC.md`, the PRD, and `.gitignore` are read-only.
- No firmware behavior changes are in this changeset; firmware files may be touched
  ONLY by the temporary, never-committed watchdog-hang patch (see
  `bench_evidence_package.md` §5) which must be reverted before session close.
- docs/COMMISSIONING.md "Completion sign-off" table (L535+) is the natural anchor
  for a cross-reference to the captured bench evidence under `docs/bench/`.
