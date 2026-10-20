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
| A | Dry bench check + Stage-2 rail measurement (SPEC §9.1–9.2, §4) | — | `YYYY-MM-DD_session-A-dry-bench.log` + `YYYY-MM-DD_rails.md` | — | — | BLOCKED: rig unavailable — no RP2040-Zero serial device detected on this host (pio device list empty) and no multimeter/physical access for the Phase R buck-rail measurement |
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
