# Session C — Flow 2: successful fill through DONE/ACK (SPEC §9.4, §7)

Fill this checklist **in place**; raw capture → `YYYY-MM-DD_session-C-flow2-success.log`
(capture procedure: docs/bench/README.md §3). Summarize in README index row C.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-C-flow2-success.log` |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V disconnected; capture at 115200 before reset; header block prepended.
- [ ] Sensors dry; cup of water at hand for hand-dipping (SPEC §9.4).
- [ ] Stopwatch: dip the first sensor only after ≥ 60 s of fill time — trips before
      TRIP_MIN_ELAPSED_S (60 s) are early-trip FAULTS (Session D territory, not this flow).

## Flow (SPEC §5.2 Flow 2: press 1 start → complete → press 2 ack → press 3 next fill)

### 1. Fill start

- [ ] `FILL start backstop=…min recovery=…ms recmin=…` (fresh boot after RESET prints
      `backstop=30min recovery=0ms recmin=0`)
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=…min`
- [ ] LED solid blue (FILLING).

### 2. First sensor trip (only after ≥ 60 s open)

- [ ] Dip sensor 1 to mount depth → `VALVE close` → `TRIP sensor=1 fillms=____`
- [ ] Valve closes immediately on the trip; LED stays solid blue (CONFIRM continues blue, SPEC §4).

### 3. Confirm window (12 s — CONFIRM_WINDOW_S)

- [ ] Dip sensor 2 within 12 s of the first trip → `CONFIRM pass sensor=2 durms=____`
- [ ] `durms` ≤ ~12000.

### 4. DONE

- [ ] `DONE attemptms=____ openms=____`
- [ ] LED green triple-pulse attention blink: 3 × (on 200 ms / off 200 ms), off 1000 ms, repeat.

### 5. Acknowledge

- [ ] Short press → `ACK done`
- [ ] Blinking stops; LED off (IDLE).

### 6. Next fill (Flow-2 press 3)

- [ ] Short press → a new `FILL start …` line.

## Critical assertions

> `VALVE close` precedes `TRIP sensor=1` (valve-closed-first on every exit, SPEC §7.9).
> Trip no earlier than 60 s of fill time; confirm within 12 s. Violations are faults, not
> successes — retake the session rather than ticking the box.
> Line order: `CONFIRM pass sensor=2 durms=…` BEFORE `DONE attemptms=… openms=…`.

## Verification greps (operator hints)

    grep -n '^TRIP sensor=1\|^CONFIRM pass sensor=2\|^DONE attemptms=\|^ACK done' YYYY-MM-DD_session-C-flow2-success.log

## After the session

- [ ] Update README index row C; file mismatches in `YYYY-MM-DD-discrepancies.md`.
