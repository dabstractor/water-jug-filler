# Research notes — P1.M2.T2.S3 (Session D fault paths)

## Facts read from disk this session

### FAULT log formats (firmware/jug_filler/jug_filler.ino, formatFillLogEvent ~1355–1395)
- `FAULT baseline sensor=%u code=%u count=%lu` — b = failing sensor's observed count
- `FAULT confirm sensor=%u code=%u durms=%lu` — b = confirm-window elapsed ms (ConfirmTimeout)
- `FAULT earlywet sensor=%u code=%u fillms=%lu` — b = fill ms at fault
- `FAULT earlytrip sensor=%u code=%u fillms=%lu` — b = fill ms at fault
- `FAULT backstop code=3 budget=%umin openms=%lums` — a = budget incl. +5s, b = open ms
- sensor printed as `e.sensor + 1` → 1 or 2; code mirrors sensor number (3 = backstop)
- Supporting: `FILL start backstop=%umin recovery=%lums recmin=%u`, `BASE ok s1=%u s2=%u`,
  `VALVE open budget=%umin`, `VALVE close`, `TRIP sensor=%u fillms=%lu`

### config.h tunables bounding the session
- CONFIRM_WINDOW_S=12, EARLY_WET_WINDOW_S=5, TRIP_MIN_ELAPSED_S=60
- DEBOUNCE_COUNT=8 × READING_INTERVAL_MS=100 → ~0.8 s wet-dwell to register
- FAULT_BLINK_MS=250 (on/off), FAULT_PAUSE_MS=2000 (between groups)
- FRESH_BOOT_BACKSTOP_MIN=30; BUTTON_LONG_PRESS_MS=1000; BUTTON_DEBOUNCE_MS=40

### docs/bench/template-session-D-fault-paths.md (the checklist to fill IN PLACE)
- D1 REQUIRED: wet sensor at start → `VALVE close` → `FAULT baseline sensor=1 code=1 count=`;
  CRITICAL: no `VALVE open` before the fault; BOOT-press-while-faulted logs `BTN short` but no
  `FILL start`; RESET clears to fresh banner; optional sensor-2 repeat (code=2, 2-blink)
- D2 REQUIRED: ≥60 s open → dip sensor 1 → `TRIP sensor=1` → never dip sensor 2 → 12 s →
  `FAULT confirm sensor=2 code=2 durms=`; red 2-blink, latched
- D3 OPTIONAL: earlywet (wet <5 s) + earlytrip (trip ≥5 s but <60 s); SKIPPED-with-reason OK
- D4 OPTIONAL: backstop expiry — needs full budget of open time; safety hint in template:
  leave 12 V off the MOSFET module; SKIPPED-with-reason OK
- RESET before EACH sub-scenario (latch clears only on reset; fresh-boot 30/0/0 each time)
- After-session: index row D; D1/D2 fail → PARTIAL(n✗); optional skips don't count as failures

### docs/bench/README.md
- Row A currently `BLOCKED: rig unavailable — no RP2040-Zero serial device detected on this host…
  no multimeter/physical access`. Rows B–E PENDING. S2 (in flight) gates on row A = PASS →
  its expected branch is also BLOCKED. S3 must re-read live state at Task 0.
- Status vocabulary: PENDING / PASS / PARTIAL(n✗) / BLOCKED: reason / SKIPPED: reason
  (SKIPPED allowed ONLY for Session E and the backstop-expiry sub-item inside Session D —
  never for the whole row D).
- §1 integrity: genuine captures or BLOCKED; never edit .log files.
- §3 capture: USB-only (VSYS ties to USB VBUS, no diode); 115200 8N1; capture BEFORE reset;
  operator header block mandatory. Naming: `YYYY-MM-DD_session-D-fault-paths.log`,
  `YYYY-MM-DD-discrepancies.md` (one per bench day, shared — may be created by S1/S2).

### S2 PRP (parallel contract)
- Assumes: templates landed, firmware flashed, no reflash, rows B/C owned by S2 (don't touch).
- Techniques to inherit: hand-dip ~1–2 s at mount depth (0.8 s debounce); count 1 Hz `S1=` lines
  as clock (~60–65 lines = ≥60 s); short presses fire on release; long press during FILLING = +5
  not abort; BOOT (onboard user button) never held during (re)plug; valve dry-fire opens <5 s.
- Entry gate pattern: row A must be PASS else BLOCKED branch with no files.

### bench_evidence_package.md §2 Session D (binding)
- Wet sensor at start: `VALVE close` → `FAULT baseline sensor=N code=N count=…`; critical
  assertion no `VALVE open` precedes; latches until RESET.
- No-confirm: trip sensor 1 after 60 s, never dip sensor 2 → (12 s) → `FAULT confirm sensor=2
  code=2 durms=…`; red 2-blink. Optional guards + backstop expiry only if time permits.

## Technique derivations
- Early-wet dip timing: dip within ~1–2 s of `VALVE open` so the 8×100 ms debounce completes
  inside the 5 s window → `fillms` < 5000.
- Early-trip dip timing: count ~20–30 `S1=` lines after `VALVE open` (clear of the 5 s window,
  well under 60 s) → `fillms` in ~(5000, 60000).
- D2 durms: counts from sensor-1 trip; expect ≈12000 (window elapsed).
- D4 (if run): fresh-boot budget 30 min → `FAULT backstop code=3 budget=30min openms≈1800000ms`;
  wall-clock cost ~30 min; template's safety hint = 12 V off the MOSFET module.
