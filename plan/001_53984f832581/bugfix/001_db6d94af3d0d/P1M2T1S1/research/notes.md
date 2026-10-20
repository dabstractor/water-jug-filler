# Research notes — bugfix P1.M2.T1.S1 (docs/bench/README.md index + capture procedure)

## 1. Source contracts (read this session, verbatim)

- `plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md`
  §1 power rules, §2 session checklists A–E, §3 docs/bench/ layout, §4 executability split
  + integrity rule. THE binding input for this task.
- `plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md` §2 —
  log-line grammar (append-only, prefix-anchored, formatter line refs) and the ambient
  prefixes (`JugFiller boot (reset cause: …)`, `BTN short|long <n>ms`, `S1=<n> S2=<n>` 1 Hz).
- Parallel item P1.M1.T2.S1 PRP: touches README.md:49 + COMMISSIONING.md:90 only —
  fully disjoint from docs/bench/ creation. No ordering dependency either way.

## 2. Facts the README must encode

- Sessions (from §2/§3, exact filenames): A `YYYY-MM-DD_session-A-dry-bench.log` (+ rails);
  B `…_session-B-flow1-abort.log`; C `…_session-C-flow2-success.log`;
  D `…_session-D-fault-paths.log`; E `…_session-E-watchdog.log` (only if run);
  plus `YYYY-MM-DD_rails.md` (multimeter, NO serial exists buck-only) and
  `YYYY-MM-DD-discrepancies.md`.
- Power rules (§1, binding): bench sessions USB-powered with buck 5 V disconnected from
  board (VSYS ties directly to USB VBUS, no diode — never both); 12 V alone allowed for
  valve dry-fire; dry-fire opens < 5 s; rail check is buck-only/USB-unplugged → markdown
  evidence, not a log.
- Capture: 115200 baud (SERIAL_BAUD), save-to-file, raw untouched stream (prefix-anchored
  greps; formats append-only per jug_filler.ino:1343–1344).
- Operator header block fields (§3): date, firmware build/git rev, wiring notes, deviations.
- Integrity rule (§4, item says state verbatim): evidence logs must be genuine captures;
  if the rig is unavailable record BLOCKED with reason — never fabricate log contents.
- Session E nuance (§5): temporary never-committed hang patch, reverted after, evidence
  records the revert. README must reference but not implement.
- Status vocabulary needed by index: PENDING / PASS / PARTIAL / BLOCKED:<reason> /
  SKIPPED:<reason> (optional items only: session E, backstop-expiry check).
- Downstream: P1.M2.T1.S2 authors per-session templates that MUST match this README's
  conventions; P1.M2.T2.S5 finalizes the index. Detailed per-session checklists are S2's
  scope — README defines conventions + index only (scope boundary).

## 3. Design decisions

- Index rows ship with `—` placeholders and status PENDING (template state; no capture
  has happened — BUG-003). Per-checklist column format `n/N ✓` summarized; details live in
  S2's per-session templates + discrepancies file.
- Integrity rule quoted verbatim as a blockquote at the top (it governs everything below).
- Validation is mechanical greps (no hardware): session rows A–E, status vocab words,
  `115200`, USB/buck rule, header fields, integrity sentence, filename grammar vs §3.
- git scope: exactly one new file `docs/bench/README.md` (new dir). No other file touched.
