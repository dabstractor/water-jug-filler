# Validation Report — water-jug-filler

**Scope:** Full-repo validation of the jug-filler firmware against the bug-fix PRD
(BUG-001/002/003) and the underlying SPEC (`spec/SPEC.md`), including an
independent adversarial pass. Validation script: `./validate.sh` (7 phases,
all passing: toolchain, RP2040 compile, 214 native Unity tests, §10 structural
safety, doc-conformance regressions, bench-evidence integrity, and a 40-assertion
independent E2E harness driving `FillEngine` through the SPEC §5.2 user flows,
fault paths, guard boundaries, +5 stacking, backstop expiry and the millis()
rollover — all behave per spec).

## PRD Bug-Fix Verification

| PRD issue | Status | Evidence |
|---|---|---|
| BUG-001 — impossible `recmin` examples in COMMISSIONING.md §5a | **FIXED** | docs/COMMISSIONING.md:434 now reads `recovery=8200ms recmin=0`; truncation note `599999 ms → 9` correct; all 9 `recovery=/recmin=` example pairs across README/runbook/bench are mutually consistent (script Phase 5a/5b) and match `formatFillLogEvent` (E2E formatter probes). |
| BUG-002 — impossible `BTN short 1234ms` README example | **FIXED** | README.md:50 now shows `BTN short 340ms` / `BTN long 1500ms`; every BTN example in the docs satisfies the ≥1000 ms long-press classification (script Phase 5c). |
| BUG-003 — physical bench validation not executed | **PARTIALLY ADDRESSED — still open** | `docs/bench/` evidence package now exists (index, binding integrity rule, capture procedure, 5 per-session expected-prefix checklists, discrepancy ledger). However all 5 sessions are BLOCKED (rig unavailable — no RP2040-Zero serial device, no physical access) and **0 captured log files exist**; the PRD §9 hardware sign-off remains unexecuted (see Issue 1). |

## Critical Issues (Must Fix)

None.

## Major Issues (Should Fix)

None.

## Minor Issues (Nice to Fix)

### Issue 1: Physical bench validation still not executed — 0/5 sessions BLOCKED, no captured evidence
**Severity**: Minor
**ID**: VAL-001 (carries BUG-003)
**Location**: docs/bench/README.md §2 (session index); plan/001_53984f832581/tasks.json (P1.M7.T2.S1)

**Description**:
The PRD's third requirement — execute the §9 flows 1/2, fault-path blink codes and
optional §10 watchdog check on hardware and capture the serial logs under
`docs/bench/` — has not happened. The bugfix pass built the complete evidence
framework (session templates with expected-prefix checklists, capture procedure,
discrepancy ledger, integrity rule "never fabricate") and honestly recorded every
session as `BLOCKED: rig unavailable` rather than synthesizing logs, which is the
correct process behavior — but the deliverable itself (genuine captures) still does
not exist and cannot be produced on this host. Hardware-validated sign-off of the
§9 commissioning flows and fault paths remains outstanding pending rig availability;
root plan task P1.M7.T2.S1 correspondingly still shows "Researching". No firmware
defect is implicated: the firmware compiles for the board, all 214 native tests
pass, and my independent E2E harness confirms every flow behaves per SPEC.

**Steps to Reproduce**:
1) `ls docs/bench/*.log` → no captures. 2) Session index rows A–E all read
`BLOCKED: … rig unavailable` (0/5 executed). 3) Discrepancy ledger notes
"physical validation … remains OUTSTANDING pending rig availability".

### Issue 2: SPEC §4 sole-indicator claim contradicts the implemented dual-indicator design (open BUG-004)
**Severity**: Minor
**ID**: VAL-002 (ledger D-1 / proposed BUG-004)
**Location**: spec/SPEC.md §1/§2.1/§4 vs firmware/jug_filler/config.h:25,69 and docs/COMMISSIONING.md:96-98

**Description**:
SPEC §4 states the onboard WS2812 (GP16) is the **sole** status/fault indicator and
"there is no external LED", and §2.1 lists GP14 among the unwired spares. The
firmware instead drives an **external green LED on GP14 as the authoritative
indicator** (`PIN_LED = 14`) with the WS2812 as a config-gated color mirror
(`USE_ONBOARD_RGB = true`), and COMMISSIONING.md documents both. The bench
discrepancy ledger records this as D-1 / proposed BUG-004 with status "open" —
the spec text was deliberately left unedited in that changeset, so the
reconciliation is still pending. Documentation-scope only: no behavioral defect
(patterns are identical on both indicators and the mirror fails safe), but a
non-author building from SPEC §2.1 would leave GP14 unwired and lose the
authoritative LED. Note spec/ is read-only for this validation, so this is
reported, not fixed.

**Steps to Reproduce**:
1) spec/SPEC.md §4: "sole status/fault indicator — there is no external LED";
§2.1 spares row includes GP14. 2) config.h: `PIN_LED = 14` (external, authoritative),
`USE_ONBOARD_RGB = true` (mirror). 3) docs/bench/2026-10-19-discrepancies.md D-1
records the conflict with follow-up "proposed: BUG-004 … Status: open".

### Issue 3: Plan tracking drift and uncommitted doc change in the working tree
**Severity**: Minor
**ID**: VAL-003
**Location**: plan/001_53984f832581/tasks.json; git index (staged docs/COMMISSIONING.md)

**Description**:
Root plan bookkeeping lags the executed work (orchestrator-owned, flagged as
optional in the prior validation and still true): P1.M7.T2.S1 shows "Researching"
although the bench evidence package was built (sessions blocked, not started);
P2.M1.T3.S1 ("Update README.md and overview docs") shows "Ready" although the
README/bench cross-references are written; phase headers P1 and P1.M7 likewise
show "Researching". Separately, the working tree carries **staged but uncommitted**
changes — docs/COMMISSIONING.md (a 6-line bench-evidence pointer, content verified
correct) plus bugfix plan files — so HEAD does not yet contain the full BUG-003
close-out documentation. Neither affects firmware behavior.

**Steps to Reproduce**:
1) plan/001_53984f832581/tasks.json: P1.M7.T2.S1 = "Researching", P2.M1.T3.S1 =
"Ready". 2) `git status --short` → `M docs/COMMISSIONING.md` (staged) + added
bugfix plan files. 3) `git diff --cached -- docs/COMMISSIONING.md` → bench pointer
absent from HEAD.

## Testing Summary

- **Unit:** `pio test -e native` — 16 Unity suites, **214/214 passed**, 0 failed.
- **Compile:** `pio run -e pico` — RP2040 (Waveshare RP2040-Zero) build SUCCESS.
- **Structural §10 safety:** setup() line 1 drives PIN_VALVE LOW; loop() line 1
  feeds the watchdog; zero `delay()`; no `while(!Serial)` (check_setup_first_line.py PASS).
- **Independent E2E harness** (40 assertions, all passing) beyond the shipped
  suite — user journeys per README/COMMISSIONing workflows:
  - §5.2 Flow 2 end-to-end: fresh boot 30-min grant → settle/baseline → valve
    open (budget log) → first trip at exactly 60.000 s (legal boundary) →
    valve-close-first ordering (§7.9) → CONFIRM pass durms=800 → DONE
    attemptms=62400 openms=60000 → ACK → next fill's recovery measured from
    valve-close (1000 ms → 44-min clamp).
  - §5.2 Flow 1: abort during settle (openms=0), restart with recomputed
    backstop (500 ms recovery → 44 min — abort restarts the recovery clock),
    mid-fill abort (openms=2100).
  - §7.1 baseline fault: valve never opens, code 1 latched, BOOT ignored while faulted.
  - §7.6 guard boundaries: early-wet at 4.900 s faults; wet at exactly 5.000 s is
    legal and trips into an early-trip fault at 5.700 s.
  - §7.5 confirm timeout at 12.000 s (code 2); §7.7 backstop expiry (code 3,
    budget incl. stacked +5, openms=2400000); +5 stacking 30→35→40 min with ack edges.
  - §6.2 table probes: knots exact, interpolation floored (6→42, 47→32), clamps both ends.
  - millis() rollover: fill spanning the 2^32 wrap — trip/done/confirm durations exact.
- **Doc conformance:** all recovery/recmin example pairs consistent with the
  formatter; all BTN example holds reachable; doc links resolve.
- **Bench evidence integrity:** package shape complete; statuses use allowed
  vocabulary; no fabricated .log files; no leftover BENCH_HANG_TEST patch.
- Total bugs found: 3
- Critical: 0 / Major: 0 / Minor: 3

## Recommendations

- Execute the bench sessions A–E on real hardware when the rig is available and
  drop the captures under docs/bench/ per the capture procedure (closes VAL-001
  and root task P1.M7.T2.S1); until then the BLOCKED rows are the honest state.
- Reconcile SPEC §4/§2.1 with the implemented dual-indicator design (or the
  firmware with the spec) and close ledger D-1 / BUG-004 (VAL-002).
- Commit the staged docs/COMMISSIONING.md bench pointer and refresh the
  orchestrator-owned tasks.json statuses (VAL-003).
