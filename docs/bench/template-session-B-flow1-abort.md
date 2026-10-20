# Session B — Flow 1: abort + recomputed restart (SPEC §9.4, §6.1)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-B-flow1-abort.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row B.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-B-flow1-abort.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 started before reset; header block prepended.
- [ ] Sensors dry in air (a wet sensor at start is Session D's baseline fault, not this flow).

## Flow (SPEC §5.2 Flow 1: press 1 start → press 2 abort → press 3 restart)

### 1. Fresh boot, first fill

- [ ] `JugFiller boot (reset cause: POWER_OR_RESET)`
- [ ] Short press → `FILL start backstop=30min recovery=0ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=30min`
- [ ] LED solid blue (FILLING).

### 2. Mid-fill abort (short press)

- [ ] `VALVE close`
- [ ] `ABORT attemptms=____ openms=____`
- [ ] The abort press also logs `BTN short ____ms` (< 1000).
- [ ] LED off (IDLE); valve audibly closes if 12 V is connected.

### 3. Restart with recomputed backstop

Wait a short known interval (≈1 min) after the abort, then short press:
- [ ] `FILL start backstop=44min recovery=____ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=44min`
- [ ] LED solid blue.
- Wait used: `____` (record it — it sets the expected table row).

## Critical assertions

> **CRITICAL — the restart line must NOT show `recovery=0ms`.** The abort restarts the
> recovery clock (SPEC §6.1); `recovery=0ms` on the restart means history was lost
> (fresh-boot behavior). `recovery=0ms` on restart = FAIL.
>
> **CRITICAL — the restart budget comes from the §6.2 table, not the fresh-boot default.**
> Restarting ≤ 2 min after the abort must print `backstop=44min` and `budget=44min`
> (first-knot clamp of {2,44}), NOT 30 min. Longer waits follow the table:
> 10 min → 41, 20 min → 38, 35 min → 34, ≥ 60 min → 30.
>
> `VALVE close` precedes `ABORT` (valve-closed-first, SPEC §7.9).

## Verification greps (operator hints)

    grep -n '^FILL start' YYYY-MM-DD_session-B-flow1-abort.log   # exactly 2; 2nd NOT recovery=0ms
    grep -n '^VALVE open budget=' YYYY-MM-DD_session-B-flow1-abort.log

## After the session

- [ ] Update README index row B; file mismatches in `YYYY-MM-DD-discrepancies.md`.
