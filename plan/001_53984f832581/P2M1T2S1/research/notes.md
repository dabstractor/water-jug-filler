# Research notes — P2.M1.T2.S1 (docs/COMMISSIONING.md runbook)

## Input contract: P2.M1.T1.S1 (parallel, Implementing) — FINAL log formats
Treat its PRP as the contract (append-only enrichment of existing lines). Final
normative formats the runbook may quote verbatim:

- `JugFiller boot (reset cause: …)` — boot banner (pure `formatBootBanner`)
- `BTN short <ms>ms` / `BTN long <ms>ms` — BOOT press events (on release)
- `S1=%u S2=%u` — 1 Hz averaged ADC counts, printed in EVERY state (dry AND wet values for §9.3)
- `FILL start backstop=%umin recovery=%lums recmin=%u` — recmin = recovery/60000 truncated; recovery=0ms ⇒ fresh boot (table bypassed, FRESH_BOOT_BACKSTOP_MIN)
- `BASE ok s1=%u s2=%u` — baseline capture verdict at fill start
- `FAULT baseline sensor=%u code=%u count=%lu` — observed count of failing sensor
- `VALVE open budget=%umin` / `VALVE close`
- `TRIP sensor=%u fillms=%lu`
- `CONFIRM pass sensor=%u durms=%lu`
- `DONE attemptms=%lu openms=%lu` — §9.5 fill-duration datum (openms = valve-open span)
- `ABORT attemptms=%lu openms=%lu` — openms=0 ⇒ valve never opened
- `FAULT earlywet sensor=%u code=%u fillms=%lu`
- `FAULT earlytrip sensor=%u code=%u fillms=%lu`
- `FAULT backstop code=3 budget=%umin openms=%lums`
- `FAULT confirm sensor=%u code=%u durms=%lu`
- `PLUS5 budget=%umin` / `ACK done`
- §9.2 power rails are NOT loggable by design (USB unplugged when buck powers board ⇒ CDC absent) — multimeter record only; T1S1 documents this in README.

## config.h facts (read live, 2025 — exact names & groups)
- Group 5 `// ---- 5. ADC / sensors (provisional; set during commissioning — PRD §9) ----`:
  ADC_BITS=12, ADC_SAMPLES_PER_READING=32, READING_INTERVAL_MS=100,
  BASELINE_SETTLE_MS=2000, **BASELINE_MIN=2000, BASELINE_MAX=4050,
  TRIP_DROP_COUNTS=350**, DEBOUNCE_COUNT=8. Header comment: counts are 12-bit
  (0–4095), 3.3 V ≈ 4095.
- Group 3 `// ---- 3. Fill backstop model — PRD §6.1 / §6.2`:
  FRESH_BOOT_BACKSTOP_MIN=30, **BACKSTOP_TABLE_MIN[] = { {2,44},{10,41},{20,38},{35,34},{60,30} }**
  (struct BackstopTableEntry {recoveryMin, backstopMin}, piecewise-linear, clamped
  both ends), PLUS5_MIN=5.
- File header: "Commissioning may retune ADC/sensor values (§9)" — the runbook's
  edits are exactly the sanctioned ones.

## Spec facts (spec/SPEC.md)
- §5.2 Flow 1 (abort): press → fill starts → press mid-fill → stops → press 3 →
  fresh recomputed timer. Flow 2 (success): press → fill completes → green
  triple-pulse → press (ack, blinking stops) → press 3 → next fill.
- §4 LED contract: IDLE off; FILLING/CONFIRM solid blue (WS2812 mirror;
  external GP14 LED shows the same pattern shape); DONE green triple-pulse
  3×(200/200)+1000; FAULT red blink-code groups; +5 ack = fast white 3×.
  NOTE: spec §4 sentence "no external LED" contradicts config.h PIN_LED=GP14 +
  P1.M5.T2 (external LED shipped) — runbook must describe patterns (shape,
  counts, cadence) which hold for BOTH drivers, and not repeat that sentence.
- §9.3 magnitudes: dry ≈ 3000–3900 counts @3.3 V; wet = submerged to mount depth.
- §6.2 concavity rationale: tank refills fast at first, slower as pressure
  builds ⇒ steep at short recovery, flattening near full-tank value; straight
  line pads mid-range in the wrong direction; never padded.
- §2.2 power: never USB while buck powers board (VSYS=VBUS, no diode —
  backfeed risk); buck 5 V → board 5 V pin feeds LDO; verify 4.9–5.1 V with
  multimeter BEFORE first buck-powered boot; program over USB with buck
  disconnected; WS2812 ≤60/255; MOSFET module gate-drive qualification note;
  sensor AOUT wiring routed away from valve wiring.
- §9.1 dry-fire rule: short clicks <5 s each (solenoid overheating).

## Bench-task context (P1.M7.T2.S1 — Failed/blocked)
Blocked ONLY because no physical rig is attached to the build host (no
/dev/ttyACM*, no RPI-RP2 drive, no RP2040 on lsusb). All native/build gates
green (192/192 tests, UF2 builds). Its PRP already contains a T2–T9 operator
runbook with greps — COMMISSIONING.md is the durable, human-facing calibration
guide; align terminology, do not contradict. This is why "calibration by a
non-author" matters: the author won't be at the bench.

## Conventions
- Mode A doc: rides with this task's changeset at docs/COMMISSIONING.md.
- README.md:41-70 "Serial logging" = normative table (T1S1 finalizes);
  README:72 "Safety note" exists — COMMISSIONING.md should link, not duplicate.
- Serial monitor: USB CDC 115200 (SERIAL_BAUD); unplug-safe (lines dropped).
- Build cmds: `pio run -e pico`, `pio test -e native`, `python3 test/check_setup_first_line.py`.
- No markdownlint config in repo; validation = structural greps + link checks.

## Validation approach for a docs-only task (no existing doc-test pattern)
- grep cross-checks: every `CONSTANT_NAME` mentioned exists in config.h; every
  quoted log-line prefix matches README table and/or `formatFillLogEvent` in
  the .ino (once T1S1 lands) — else validate against T1S1 PRP final-format
  table (contract).
- `git diff --stat` scope gate: only docs/COMMISSIONING.md.
- Firmware regression sanity: `pio test -e native` still green (proves no code touched).

## External research
None needed — runbook content derives from internal contracts (spec §2.2/§4/
§5.2/§6.2/§8/§9 + T1S1 formats + config.h). No library/API surface involved.
