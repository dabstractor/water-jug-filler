# Session D — Fault paths (SPEC §7, §4)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-D-fault-paths.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row D.

Fault LED cadence (SPEC §4/§7): red N-blink groups — N × (on 250 ms / off 250 ms), then
off 2000 ms, repeat; N = fault code (1 = sensor 1, 2 = sensor 2, 3 = timer/no-trip).
Faults LATCH: cleared only by RESET or power cycle; BOOT is ignored while faulted.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-D-fault-paths.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 before reset; header block prepended.
- [ ] Cup of water at hand.
- RESET the board before EACH sub-scenario: latched faults clear only on reset, and reset
  clears tank history → each sub-scenario's first fill is a fresh-boot
  `FILL start backstop=30min recovery=0ms recmin=0`.

## D1 — Baseline fault: wet sensor at start (SPEC §7.1) — REQUIRED

- [ ] Hold sensor 1 submerged, then short press to start a fill.
- [ ] `VALVE close`
- [ ] `FAULT baseline sensor=1 code=1 count=____`
- [ ] LED red 1-blink groups; latched.
- [ ] BOOT while faulted is ignored: `BTN short ____ms` logs, but NO `FILL start` follows.
- [ ] RESET clears the fault: banner `JugFiller boot (reset cause: POWER_OR_RESET)`, LED off.
- [ ] Optional repeat with sensor 2 wet: `FAULT baseline sensor=2 code=2 count=____`,
      red 2-blink groups.

> **CRITICAL — D1 must show NO `VALVE open` line before `FAULT baseline`.** The baseline
> gate refuses to open the valve on an implausible (wet) baseline (SPEC §7.1). Any
> `VALVE open budget=…` line in the D1 segment = FAIL.

## D2 — No-confirm fault (SPEC §7.5) — REQUIRED

- [ ] RESET. Short press → `FILL start backstop=30min recovery=0ms recmin=0` →
      `BASE ok s1=____ s2=____` → `VALVE open budget=30min`.
- [ ] After ≥ 60 s open, dip sensor 1 → `VALVE close` → `TRIP sensor=1 fillms=____`.
- [ ] Do NOT dip sensor 2; wait out the 12 s confirm window.
- [ ] `FAULT confirm sensor=2 code=2 durms=____`
- [ ] LED red 2-blink groups; latched.

## D3 — Guard faults (SPEC §7.6) — OPTIONAL; may be SKIPPED with reason

- [ ] Early-wet: a wet reading within the first 5 s of fill (EARLY_WET_WINDOW_S) →
      `FAULT earlywet sensor=____ code=____ fillms=____`
- [ ] Early-trip: a trip accepted before 60 s of fill time (TRIP_MIN_ELAPSED_S) →
      `FAULT earlytrip sensor=____ code=____ fillms=____`
- [ ] LED red N-blink groups matching the faulted sensor's code; latched.
- Skip reason (if not run): `____`

## D4 — Backstop expiry (SPEC §7.7) — OPTIONAL; may be SKIPPED with reason

Needs the full budget of open valve time (≥ 30 min fresh-boot). Safety hint: leave 12 V
disconnected from the MOSFET module for this sub-scenario so the solenoid stays
de-energized during the long open (SPEC §9.1/§9.2 permit this; the log lifecycle is
unchanged).
- [ ] RESET. Short press → `FILL start …` → `BASE ok …` → `VALVE open budget=…min`.
- [ ] Leave both sensors dry; wait out the budget.
- [ ] `FAULT backstop code=3 budget=…min openms=…ms`
- [ ] LED red 3-blink groups; latched.
- Skip reason (if not run): `____`

## Verification greps (operator hints)

    grep -n '^VALVE open\|^FAULT baseline' YYYY-MM-DD_session-D-fault-paths.log
    # In the D1 segment: no VALVE open line may precede the FAULT baseline line.
    grep -n '^FAULT ' YYYY-MM-DD_session-D-fault-paths.log

## After the session

- [ ] Update README index row D. Required items (D1, D2) failing → `PARTIAL(n✗)`;
      optional D3/D4 skipped-with-reason do NOT count as failures (README status vocabulary).
- [ ] File mismatches in `YYYY-MM-DD-discrepancies.md`.
