# PRP — P2.M1.T3.S1: Update README.md and overview docs for the whole changeset

## Goal

**Feature Goal**: Make the repo presentable to a newcomer with zero author knowledge: rewrite `README.md` as the coherent front door to the WHOLE changeset — device summary, capability overview, wiring/build/flash/operation/commissioning/safety — under documentation-drift control (SOW §5 Mode B): README POINTS at `spec/SPEC.md` for normative tables (wiring, state/LED, config contract) instead of duplicating them, so future spec edits never drift from docs.

**Deliverable**: A rewritten `README.md` (single primary artifact) plus an explicit staleness sweep of every changeset-level doc (`README.md`, `docs/COMMISSIONING.md` — confirm fresh or fix minimally). No firmware, test, build, or spec changes.

**Success Definition**: A newcomer reading only `README.md` + links it points to can (a) say what the device is and does, (b) wire/flash/operate it safely (knows the NEVER-USB-and-buck rule cold), (c) find the normative contract for every detail. Every doc in the repo is confirmed non-stale by explicit check. `git diff --stat` shows README.md (and at most trivial docs/COMMISSIONING.md link fixes) only; `pio test -e native` still green.

## User Persona (if applicable)

**Target User**: A newcomer — a developer or bench operator who did NOT write this firmware — landing on the repo cold (GitHub or fresh clone).

**Use Case**: First contact: understand what the device is in one paragraph, judge whether it does what they need, then reach build/flash or wiring without reading the spec end-to-end.

**User Journey**: read intro → capabilities → click through to spec §2/§4/§5/§8 for details → build & flash (Arduino IDE or PlatformIO, observing the power rule) → use the operation quick-reference at the bench → follow the commissioning pointer when calibrating.

**Pain Points Addressed**: current README jumps straight to build tooling with no "what is this"; the USB+buck backfeed hazard lives only in spec §2.2 where a newcomer will never see it; commissioning is pointed at as "(planned)" though it now exists; no operation table means opening the spec mid-bench.

## Why

- Architecture decision (plan/001_53984f832581/architecture/system_context.md, finding 5): "Spec is the requirements source of truth — README/docs generated later must point to spec/SPEC.md rather than duplicate its tables (prevents drift)."
- This is the changeset's documentation capstone: P1 shipped firmware + 18 native suites; P2.M1.T1.S1 enriched logging and owns README's log table; P2.M1.T2.S1 authors `docs/COMMISSIONING.md` and explicitly defers the README link to this task ("P2.M1.T3.S1 (later) adds the link from README to this runbook").
- Mode B: this IS the changeset-level documentation task — it summarizes the whole changeset for handoff.

## What

`README.md` after this task contains, in this order:

1. **Title + device summary (§0)** — one paragraph: fills a 4-gallon jug with RO water, shuts the valve automatically at the set level; valve opens ONLY on a human BOOT press; a hard timer backstop (computed from RO-tank recovery) bounds the fill if sensors fail; no WiFi/app/persistence — any reset starts fresh in IDLE. Link `spec/SPEC.md` §0.
2. **What it does (capabilities)** — five states as one-liners (IDLE / FILLING / CONFIRM / DONE / FAULT) → point to spec §4 state table; LED contract pointer (spec §4 pattern shapes); button semantics pointer (spec §5.1, acts on RELEASE); tunables pointer (`firmware/jug_filler/config.h`, contract spec §8).
3. **Wiring** — 2–3 sentences + pointer to spec §2 (§2.1 pin map, §2.2 power cautions, §2.3 complete netlist). NO pin table here (drift control).
4. **Power safety — CRITICAL box** — the NEVER-USB-and-buck-together rule (spec §2.2) in its own highlighted block: bench/flash = USB-powered with buck disconnected; in service = buck-powered with USB unplugged; program over USB with buck disconnected. Include the buck-preset multimeter check (4.9–5.1 V at the 5V pin before first buck-powered boot).
5. **Build & flash** — keep the existing verified Arduino IDE steps (arduino-pico core, board "Waveshare RP2040-Zero", open `firmware/jug_filler/jug_filler.ino`); PlatformIO: `pio run -e pico` (compile-only, no upload config) → UF2 at `.pio/build/pico/firmware.uf2`, flash by BOOTSEL-hold + drag-drop; native tests: `pio test -e native` (host g++ + Unity, no hardware needed).
6. **Operation quick-reference** — ONE compact table (the only sanctioned duplication, per work item): BOOT short/long per state — IDLE: start / nothing; FILLING: abort / +5 min to backstop (stacks); CONFIRM: ignored / ignored; DONE: acknowledge / also acknowledges; FAULT: ignored / ignored. Plus one RESET note: full reboot → IDLE, valve closed, tank history cleared → next fill uses the fresh-tank 30-min backstop (spec §5.3). Point to spec §5 for the normative table and flows.
7. **Serial logging** — PRESERVE the existing "Serial logging" table (owned by P2.M1.T1.S1, Mode A — matches code exactly). Only adjust surrounding text: baud 115200, logging runs with USB unplugged (lines dropped, never stalled), fault codes = LED blink groups (spec §7). Do NOT rewrite formats.
8. **Commissioning** — replace "(planned)" with a real pointer to `docs/COMMISSIONING.md` (spec §9 runbook: sensor calibration, backstop tuning, flows).
9. **Validation status** — honest state: 192/192 native Unity tests, UF2 builds clean, structural safety gates (`python3 test/check_setup_first_line.py`) pass; physical bench validation is pending hardware (never fabricate). If `docs/bench/` exists at implementation time, link it as bench evidence; if not, state "pending" and link `docs/COMMISSIONING.md` as the runbook for it.
10. **Safety invariants recap (spec §10)** — bullet recap: valve pin driven LOW as the first line of `setup()`; valve opens only in FILLING, FILLING entered only by human BOOT press (no auto-open path, watchdog included); ~8 s hardware watchdog fed every loop; no blocking `delay()` in the fill loop; backstops hug real fill times. Keep the existing "never reorder that line" warning.

**Staleness sweep (explicit per doc)**: for each changeset-level doc, verify fresh or fix: `README.md` (this rewrite), `docs/COMMISSIONING.md` (just landed from T2S1 — verify its README references still resolve after the rewrite; fix only broken cross-references, minimally, since T2S1 lands in parallel). Record the per-doc verdict in the implementation summary.

### Success Criteria

- [ ] README has all 10 sections above in order; spec §0 summary present as one paragraph
- [ ] NEVER-USB-and-buck rule appears in a prominent power-safety block AND in the build/flash path where a user would violate it
- [ ] Operation quick-reference table covers all five states × short/long + RESET note
- [ ] `docs/COMMISSIONING.md` linked (no "(planned)"); `spec/SPEC.md`, `config.h` linked
- [ ] README contains NO duplicated normative tables (no pin map, no BOM, no LED column table, no backstop values)
- [ ] Serial-logging table untouched (T1S1-owned formats intact)
- [ ] Every changeset-level doc explicitly confirmed fresh or fixed
- [ ] No fabricated bench claims; validation-status section matches reality
- [ ] `git diff --stat` == README.md (+ at most docs/COMMISSIONING.md link fixes); `pio test -e native` green

## All Needed Context

### Context Completeness Check

✅ Written from live reads: current `README.md` (full), `platformio.ini` (env names verified), `spec/SPEC.md` headings (section numbers verified — note §2.3 is "Complete wiring netlist"), architecture finding on Mode B, P2.M1.T1.S1 and P2.M1.T2.S1 PRPs (ownership boundaries), P1.M7.T2.S1 issue record (bench-blocked facts, UF2 path, test counts). Research notes: `plan/001_53984f832581/P2M1T3S1/research/notes.md`.

### Documentation & References

```yaml
- file: README.md
  why: the artifact being rewritten. Existing sections verified good and to KEEP:
    "Repo layout", "Build in Arduino IDE" (6 steps, board URL), "Build with
    PlatformIO", "Run native tests", "Serial logging" table (T1S1-owned — do
    not touch formats), "Safety note" (fold into §10 recap).
  gotcha: current text says commissioning doc is "(planned)" — replace with a
    real link; it exists by the time this task runs (T2S1 lands first).

- file: spec/SPEC.md
  why: READ-ONLY link target. Verified section map: §0 Summary, §2 Wiring
    (2.1 pin map, 2.2 power cautions, 2.3 complete netlist), §4 States/LED,
    §5 Button (5.1/5.2/5.3), §7 faults, §8 config.h contract, §9 commissioning,
    §10 safety invariants, §11 limitations.
  gotcha: spec §2.3 heading is "Complete wiring netlist" (the PRD index called
    it "block diagram") — link generically to "spec §2" or quote the real
    heading. GOTCHA: spec §4 contains "there is no external LED / WS2812 is
    the sole indicator" — this CONTRADICTS shipped firmware (external green
    LED on GP14 is authoritative, WS2812 mirror config-gated; config.h PIN_LED,
    P1.M5.T2/T3 complete). Describe indicators as shipped; point to spec §4
    for pattern SHAPES; never repeat that sentence. Never edit the spec.

- file: docs/COMMISSIONING.md
  why: created by parallel task P2.M1.T2.S1 per its PRP contract (self-contained
    §9 runbook). This task adds the README pointer and sweeps it for staleness.
  gotcha: it lands concurrently — verify-only by default; fix ONLY broken
    cross-references to README sections this rewrite renames/reorders.

- file: firmware/jug_filler/config.h
  why: link target for the tunables pointer (spec §8 contract). Read-only here.

- file: plan/001_53984f832581/architecture/system_context.md
  why: finding 5 = the Mode B rule this task implements (point, don't duplicate).

- file: plan/001_53984f832581/P2M1T1S1/PRP.md
  why: T1S1 OWNS README's "Serial logging" table (Mode A, matches code exactly,
    enriched fields recmin=/count=/openms=). Preserve that section byte-for-byte
    except surrounding prose; formats there are authoritative.

- file: plan/001_53984f832581/P1M7T2S1/issue_feedback.md
  why: bench-blocked record (3rd attempt, hardware absent). Grounds the honest
    validation-status section: 192/192 native tests, UF2 at
    .pio/build/pico/firmware.uf2, check_setup_first_line.py PASS, NO docs/bench/
    (never fabricated).

- file: plan/001_53984f832581/P2M1T3S1/research/notes.md
  why: full research audit trail for this PRP.
```

### Current Codebase tree (relevant excerpt)

```bash
water-jug-filler/
├── firmware/jug_filler/        # jug_filler.ino + config.h (the firmware)
├── docs/                       # created by parallel T2S1: COMMISSIONING.md
├── test/                       # 18 native Unity suites + check_setup_first_line.py
├── platformio.ini              # envs: pico (board), native (default, tests)
├── README.md                   # THIS TASK'S artifact
└── spec/SPEC.md                # PRD — read-only link target
```

### Desired Codebase tree with files to be added

```bash
README.md                      # REWRITTEN (10-section structure above)
docs/COMMISSIONING.md          # from T2S1; at most minimal link fixes here
```

### Known Gotchas of our codebase & Library Quirks

```text
CRITICAL: docs-mostly task — diff-scope gate. Zero changes under firmware/,
  test/, platformio.ini, spec/. README.md is the artifact; docs/COMMISSIONING.md
  only for broken cross-links.

CRITICAL: drift control cuts both ways. README must POINT at spec tables, not
  copy them — EXCEPT the one sanctioned compact operation table and the §10
  bullet recap the work item explicitly orders. No pin map, no BOM, no LED
  column table, no BACKSTOP_TABLE values in README.

GOTCHA: spec-vs-firmware LED divergence (spec §4 "no external LED" vs shipped
  GP14 external + WS2812 mirror). README describes shipped reality, references
  spec §4 only for pattern shapes. Do not "fix" the spec.

GOTCHA: bench evidence does not exist (P1.M7.T2.S1 blocked-pending-hardware;
  docs/bench/ was never created — fabrication forbidden). README's validation
  section must conditionally link docs/bench/ ONLY if it exists at
  implementation time; otherwise state pending honestly.

GOTCHA: T1S1's "Serial logging" table is Mode-A-owned (matches code exactly).
  Rewriting or reformatting its rows breaks its contract — preserve verbatim.

GOTCHA: parallel landing order. T2S1 (COMMISSIONING.md) and T1S1 (log table)
  land around this task. If their working-tree changes are absent when you
  start, re-check git status; link to docs/COMMISSIONING.md regardless (it is
  this changeset's committed deliverable), but do not quote its internals.

GOTCHA: platformio.ini has NO upload configuration (compile-only by design) —
  do not document `pio run -t upload`; the flash path is UF2 drag-drop or
  Arduino IDE upload. `pio test` bare also runs native (default_envs).
```

## Implementation Blueprint

### Data models and structure

None — documentation only. The README's "data model" is its 10-section skeleton (see What). Markdown conventions: ATX headings, fenced blocks for commands, one table per sanctioned duplication.

### Implementation Tasks (ordered by dependencies)

```yaml
Task 0: RECONCILE with parallel landings
  - RUN: git status --short; ls docs/ 2>/dev/null
  - CONFIRM: docs/COMMISSIONING.md exists (T2S1) — if absent, proceed but link
    it anyway (changeset deliverable) and note in summary
  - SNAPSHOT: the T1S1 "Serial logging" table from current README — it must
    survive the rewrite byte-for-byte

Task 1: REWRITE README.md — sections 1-5 (summary, capabilities, wiring, power, build)
  - WRITE: §0 device paragraph (link spec §0); capabilities with five one-line
    states -> spec §4/§5.1/§8 pointers; wiring -> spec §2 pointer only
  - WRITE: power-safety CRITICAL block (never USB+buck; bench=USB/buck-disconnected,
    service=buck/USB-unplugged, program-over-USB; buck preset 4.9-5.1V multimeter
    check before first buck boot) — spec §2.2
  - CARRY OVER verbatim-good: "Repo layout", "Build in Arduino IDE" steps;
    EXTEND PlatformIO section with UF2 path + BOOTSEL drag-drop flash;
    keep 'pio test -e native' + first-run-downloads note
  - NAMING: section titles as in the What list; 'Power safety (CRITICAL)' as
    a blockquote or bold-callout block, placed BEFORE build instructions

Task 2: REWRITE README.md — sections 6-10 (operation, logging, commissioning, status, safety)
  - WRITE: compact operation table (5 states x short/long, exact semantics from
    spec §5.1) + RESET note (history cleared -> 30-min fresh backstop, §5.3);
    normative pointer to spec §5 incl. flows §5.2
  - PRESERVE: T1S1 "Serial logging" table verbatim; adjust only the intro
    sentence around it (baud, unplug-safe, spec §7 fault-code pointer)
  - WRITE: commissioning pointer to docs/COMMISSIONING.md (replace "(planned)")
  - WRITE: validation-status section — 192/192 native tests, UF2 build,
    structural gates pass, bench PENDING hardware; conditional docs/bench/ link
  - WRITE: §10 safety recap bullets folding in the existing "Safety note"
    (valve-LOW-first-line warning preserved verbatim in intent)

Task 3: SWEEP every changeset-level doc for staleness (explicit per-doc verdict)
  - CHECK docs/COMMISSIONING.md: links to README sections still resolve after
    rewrite (its PRP references README's 'Serial logging' section and build
    sections); fix ONLY broken anchors/section names, minimally
  - CHECK README.md: all links resolve (spec/SPEC.md, docs/COMMISSIONING.md,
    firmware/jug_filler/config.h, firmware/jug_filler/jug_filler.ino)
  - RECORD: one-line verdict per doc in the implementation summary
    ("<doc>: fresh" or "<doc>: fixed <what>")

Task 4: VALIDATE all levels below; confirm diff scope
```

### Implementation Patterns & Key Details

```markdown
<!-- Power-safety block pattern (place before build instructions):
> **⚠️ Power rule — never USB and buck together.** The RP2040-Zero's 5V pin
> ties to USB VBUS with no diode (spec §2.2). Bench/flash: USB-powered, buck
> disconnected from the board. In service: buck-powered, USB unplugged.
> Verify buck output 4.9–5.1 V with a multimeter before the board's first
> buck-powered boot.
-->

<!-- Operation table pattern (the ONLY sanctioned table duplication):
| State | Short press (<1 s) | Long press (≥1 s) |
| IDLE | Start fill | — |
| FILLING | Abort (valve closed) | +5 min to backstop (stacks) |
| CONFIRM | ignored | ignored |
| DONE | Acknowledge | Acknowledge |
| FAULT | ignored | ignored |
Presses act on RELEASE (spec §5.1). RESET: full reboot → IDLE, valve closed,
tank history cleared → next fill uses the 30-min fresh backstop (spec §5.3).
-->
```

```text
PATTERN: every "details" claim ends in a spec pointer: "(spec §N)". If a
  sentence in README fully restates a spec table, delete the sentence and
  keep the pointer — except the two sanctioned duplications.

PATTERN: link relative paths from repo root as the README does today
  (firmware/jug_filler/config.h).

CRITICAL: do not renumber/retitle the "Serial logging" section — T2S1's
  runbook and T1S1's contract reference it by that name.
```

### Integration Points

```yaml
DOCS: README.md REWRITTEN; docs/COMMISSIONING.md at most minimal link fixes
  (T2S1 owns its content; parallel-landing courtesy).
FIRMWARE: none (read-only references: config.h, jug_filler.ino).
BUILD: none. platformio.ini untouched.
DOWNSTREAM: this is the final documentation task of the changeset (P2 capstone);
  repo handoff readiness is the exit criterion.
```

## Validation Loop

### Level 1: Syntax & Style (Immediate Feedback)

```bash
# Structure: all 10 sections present, in order; fences balanced
grep -n '^#' README.md
python3 - <<'EOF'
t=open('README.md').read()
assert t.count('```')%2==0, 'unbalanced fences'
for s in ['spec/SPEC.md','docs/COMMISSIONING.md','firmware/jug_filler/config.h',
          'pio test -e native','pio run -e pico','115200','4.9','IDLE','FILLING',
          'CONFIRM','DONE','FAULT','+5 min','watchdog','first','reset']:
    assert s in t, f'missing: {s}'
print('OK')
EOF
# Expected: headings ordered per What; OK.
```

### Level 2: Drift-control & content cross-checks

```bash
# No duplicated normative tables (Mode B negative checks)
! grep -qE 'ADC0|Board pin' README.md            # no pin map
! grep -q 'BACKSTOP_TABLE' README.md            # no config values
! grep -q '(planned)' README.md                 # stale pointer gone

# Operation table complete: 5 states x both press types
grep -c '^| *\(IDLE\|FILLING\|CONFIRM\|DONE\|FAULT\)' README.md   # == 5

# USB/buck rule appears at least twice (safety block + build path)
[ $(grep -ci 'usb' README.md) -ge 2 ] && grep -qi 'buck' README.md && echo rule-ok

# Serial-logging table survived: spot-check T1S1 format keywords intact
for kw in 'FILL start' 'BASE ok' 'FAULT backstop' 'DONE attemptms'; do
  grep -q "$kw" README.md || echo "log table damaged: $kw"; done; echo done

# Conditional bench honesty: no fabricated evidence claims
grep -n 'docs/bench' README.md || echo "no bench link (ok if docs/bench absent)"
[ -d docs/bench ] || grep -qi 'pending' README.md || echo "must state bench pending"

# Spec pointers hit real sections
for s in '0. Summary' '2. Wiring' '4. States' '5. Button' '8. config.h' '10. Safety'; do
  grep -q "$s" spec/SPEC.md || echo "spec section missing: $s"; done; echo done
```

### Level 3: Integration (repo-level gates)

```bash
# Diff-scope gate: README (+ at most COMMISSIONING link fixes), nothing else
git diff --stat
git diff --stat -- firmware/ test/ platformio.ini spec/ | wc -l   # == 0

# Nothing broken: full native suite + structural gate still green
pio test -e native 2>&1 | tail -2          # 192/192 PASSED (or suite count >= pre-task)
python3 test/check_setup_first_line.py     # PASS

# Link targets exist
for f in spec/SPEC.md docs/COMMISSIONING.md firmware/jug_filler/config.h firmware/jug_filler/jug_filler.ino; do
  [ -f "$f" ] || echo "broken link target: $f"; done; echo done
```

### Level 4: Domain review (human-level pass)

```bash
# Read README top-to-bottom as a newcomer; verify:
#  1) Can state what the device is after paragraph 1
#  2) Cannot miss the USB/buck rule before attempting to flash
#  3) Could run a first fill from the operation table alone
#  4) Every "see spec §N" pointer resolves to the right content
#  5) No sentence contradicts shipped firmware (external GP14 LED + mirror)
#  6) Per-doc staleness verdicts recorded in the implementation summary
```

## Final Validation Checklist

### Technical Validation

- [ ] Level 1–3 commands green; fences balanced; 10 sections ordered
- [ ] Operation table = 5 state rows; log table intact; no pin-map/config-value duplication
- [ ] `git diff --stat` scope clean; `pio test -e native` and structural gate green
- [ ] All link targets exist; spec pointers resolve to real headings

### Feature Validation

- [ ] All success criteria from "What" section met
- [ ] Newcomer persona pass (Level 4 walkthrough)
- [ ] Power rule prominent and repeated at the violation point (build path)
- [ ] Validation status honest (bench pending, no fabrication; conditional docs/bench/ link)
- [ ] Every changeset-level doc has an explicit fresh/fixed verdict

### Code Quality Validation

- [ ] Markdown conventions match repo style (ATX headings, fenced commands)
- [ ] Mode B discipline: pointers over duplication, two sanctioned exceptions only
- [ ] No contradictions with spec or shipped firmware (LED divergence handled by shipped-reality wording)
- [ ] T1S1/T2S1 ownership boundaries respected (log table verbatim; COMMISSIONING content untouched)

### Documentation & Deployment

- [ ] README is self-sufficient as the repo front door
- [ ] "(planned)" gone; commissioning pointer live
- [ ] No environment variables or config introduced (docs-only)

## Anti-Patterns to Avoid

- ❌ Don't copy spec tables into README — that is the drift this task exists to prevent (two sanctioned exceptions: operation quick-ref table, §10 recap)
- ❌ Don't touch the T1S1 "Serial logging" table rows or retitle that section
- ❌ Don't edit spec/SPEC.md, firmware/, test/, or platformio.ini — diff-scope gate enforces it
- ❌ Don't hide or soften the bench-blocked status, and never fabricate docs/bench/ evidence
- ❌ Don't repeat spec §4's "no external LED" sentence — shipped firmware has GP14 external + WS2812 mirror
- ❌ Don't document `pio run -t upload` — no upload env exists; flash is UF2 drag-drop or Arduino IDE
- ❌ Don't make sweeping edits to docs/COMMISSIONING.md — it lands from T2S1 in parallel; fix only broken cross-links

---

**Confidence Score: 9/10** — the artifact is a single file whose content is fully determined by verified in-repo contracts (spec section map, current README, platformio.ini envs, sibling-task ownership boundaries, bench-blocked record), and validation is mechanical (grep cross-checks + drift negative-checks + diff-scope gate). Residual risks: T2S1/T1S1 landing order (mitigated by Task 0 reconcile + link-anyway rule) and the spec §4 LED divergence (mitigated by shipped-reality wording rule).
