# Research notes — P1.M2.T2.S1 (bugfix changeset): Session A dry bench + Stage-2 rails

## 1. Contracts read this session

- `plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/bench_evidence_package.md`
  (117 lines, read in full): §1 BINDING power rules, §2 Session A checklist,
  §3 docs/bench/ layout, §4 executability split + integrity rule, §6 D-1 nuance.
- `docs/bench/README.md` (landed by P1.M2.T1.S1 — verified on disk): §1 integrity
  rule, §2 session index table (row A columns: Date, Evidence file, FW git rev,
  Checklist pass/total, Status) + status vocabulary (PENDING/PASS/PARTIAL(n✗)/
  BLOCKED:reason/SKIPPED:reason), §3 capture procedure (115200 8N1, capture BEFORE
  power/reset, operator header block, rail-check exception §3.4), §4 conventions.
- `plan/.../bugfix/001_db6d94af3d0d/P1M2T1S2/PRP.md` (774 lines, key sections
  extracted): creates `docs/bench/template-session-{A..E}-*.md`, `template-rails.md`,
  `template-discrepancies.md`. Canonical prefix table (byte-exact, .ino lines
  1343–1712). Fill discipline: **session templates filled IN PLACE; rails/
  discrepancies templates COPIED to date-stamped files**. Listing R = rails sheet
  fields (exclusivity ticks, 2 measurements 4.9–5.1 V, restore-power tick).
  Listing A = Session A checklist + session record table (date, evidence log,
  rails sheet, FW git rev).
- `docs/bench/` on disk today: README.md only (templates land via parallel T1.S2).

## 2. Expected log lines for Session A (canonical prefix table, byte-exact)

Fresh boot / first dry-fire:
`JugFiller boot (reset cause: POWER_OR_RESET)` → `S1=<n> S2=<n>` @1 Hz →
`BTN short <n>ms` (<1000) / `BTN long <n>ms` (≥1000) on release →
`FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok s1=<n> s2=<n>` →
`VALVE open budget=30min` → `VALVE close` → `ABORT attemptms=<n> openms=<n>`.
Dry-fires 2–3 after an ABORT: recovery clock restarted →
`FILL start backstop=44min recovery=<small>ms recmin=0` (first-knot clamp {2,44}
for recovery ≤ 2 min) — EXPECTED, not a discrepancy. Ambient `S1=`/`BTN` lines
interleave; order assertions apply to event lines only (`grep -v '^S1='`).

## 3. Safety (binding, from evidence package §1 + README §3.1)

- RP2040-Zero 5V pin (VSYS) ties DIRECTLY to USB VBUS, NO diode (hardware_validation
  L9 / Waveshare FAQ) → never USB + buck simultaneously; exactly one source at a time.
- Rail check = buck-only, USB unplugged → NO serial; evidence is the multimeter
  markdown sheet: preset w/ board disconnected 4.9–5.1 V; at board 5V pin 4.9–5.1 V.
- Dry-fire valve clicks < 5 s each (solenoid overheats w/o flow, SPEC §9.1);
  12 V may feed the MOSFET module ALONE for dry-fire.
- Session A serial part = USB-only, buck 5 V disconnected.

## 4. Verification design (what an agent CAN check after capture)

Greps on the .log: banner exactly once at top; ≥1 `BTN short` with value <1000 and
≥1 `BTN long` with value ≥1000; S1=/S2= present and not pinned (values vary, none
equal 0 or 4095 on every line); per dry-fire iteration, ascending line numbers for
FILL start → BASE ok → VALVE open → VALVE close → ABORT (grep -n); VALVE open→close
gap ≤ ~5 s (≤4 ambient 1 Hz lines between them, or capture wall-clock). Rails sheet:
both readings parse 4.9–5.1 V, exclusivity ticks checked. Index row A filled with
real date/rev and PASS/PARTIAL/BLOCKED status.

## 5. Boundary conditions

- Rig unavailable → index row A `BLOCKED: <reason>`; NEVER fabricate log contents
  (README §1, package §4 integrity rule).
- No reflash needed (item input: firmware already flashed; pico env builds clean —
  `pio run -e pico` is the only build gate).
- Template in-place fill means `template-session-A-dry-bench.md` becomes the record;
  `template-rails.md` is COPIED to `YYYY-MM-DD_rails.md`. B–E templates untouched.
- spec/SPEC.md, README.md, COMMISSIONING.md: read-only. Discrepancies go to
  `YYYY-MM-DD-discrepancies.md` (create from template if absent; D-1 pre-seeded).
- git scope: only docs/bench/ additions + template-A fill + README row A edit.
