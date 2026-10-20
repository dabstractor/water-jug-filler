# Session E — Optional watchdog-reset check (SPEC §10) — `SKIPPED: <reason>` allowed

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-E-watchdog.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row E.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-E-watchdog.log` |
| FW git rev (patched build = base rev + hang patch) | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 started BEFORE the patched
      flash/reset; header block prepended.
- [ ] Temporary hang patch applied LOCALLY and NEVER committed — e.g. inside `loop()`:
      `#if BENCH_HANG_TEST … while (true) {} … #endif` (bench evidence package §5;
      config.h is constexpr-only, so the guard define goes on the compiler command line
      or a local edit — it must not survive the session).
- [ ] 12 V connected to the MOSFET module if the audible valve-close click on reset is
      wanted (the solenoid sees one ~8 s energization — a recorded deviation from the
      < 5 s click rule, accepted for this check).
- Skip reason (if the whole session is not run): `____`

## Procedure

1. Flash the patched build; start the capture; let it boot (banner `POWER_OR_RESET`).
2. Short press to start a fill: `FILL start …` → `BASE ok …` → `VALVE open budget=…min`;
   LED solid blue (FILLING).
3. Trigger the hang during FILLING.

## Expected observations

- [ ] Board resets ~8 s after the hang begins (WATCHDOG_TIMEOUT_S = 8).
- [ ] Valve closed on reset (audible click if 12 V connected) — `setup()` drives the pin
      LOW as its first action (SPEC §10).
- [ ] Banner: `JugFiller boot (reset cause: WATCHDOG)`
- [ ] LED off after the reset (IDLE).
- [ ] 1 Hz `S1=____ S2=____` ambient resumes.
- [ ] No auto fill start: no `FILL start` line without a subsequent BOOT press
      (SPEC §5.3/§10 — the device never auto-starts a fill after any reset).
- [ ] A short press after the reset DOES start a fill (device healthy again):
      `FILL start backstop=30min recovery=0ms recmin=0` (reset cleared tank history).

## Critical assertions

> The reset cause must read `WATCHDOG`, not `POWER_OR_RESET`. No un-pressed `FILL start`
> may appear after the banner. The hang patch must be reverted — `git status` clean of
> any firmware change — before the session is closed.

## Cleanup (mandatory)

- [ ] Hang patch reverted; `git status` shows no firmware changes.
- [ ] Record here: patch method `____`, applied `____`, reverted `____`.

## Verification greps (operator hints)

    grep -n 'reset cause: WATCHDOG' YYYY-MM-DD_session-E-watchdog.log
    grep -n '^FILL start' YYYY-MM-DD_session-E-watchdog.log   # each must follow a BTN press

## After the session

- [ ] Update README index row E (PASS, or `SKIPPED: <reason>` if not run);
      file mismatches in `YYYY-MM-DD-discrepancies.md`.
