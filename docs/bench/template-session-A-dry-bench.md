# Session A — Dry bench check (SPEC §9.1, §4)

Per-session evidence template. Fill this checklist **in place** as the session runs; the raw
capture goes to `YYYY-MM-DD_session-A-dry-bench.log` (capture procedure and operator header
block: docs/bench/README.md §3). Summarize the result in README index row A as pass/total.

## Session record (fill at execution)

| Field | Value |
|---|---|
| Date | — |
| Evidence log | `YYYY-MM-DD_session-A-dry-bench.log` |
| Rails sheet | `YYYY-MM-DD_rails.md` (from `template-rails.md`) |
| FW git rev | — |
| Checklist | — / __ |
| Status | PENDING |

## Preconditions

- [ ] USB-powered, buck 5 V output disconnected from the board (never both — README §3 rule 1).
- [ ] Serial capture started BEFORE power/reset: 115200 baud, 8N1, save-to-file, raw.
- [ ] Operator header block prepended to the capture (README §3).
- 12 V may feed the MOSFET module's power input alone if audible valve clicks are wanted.

Power on / RESET the board, then work the checklist in order. Ambient lines (`S1=____ S2=____`
at 1 Hz, `BTN …` on presses) interleave with the event lines — the order below applies to the
event lines (`grep -v '^S1=' <log>` shows the event stream).

## Checklist

### 1. Boot and IDLE

- [ ] First event line: `JugFiller boot (reset cause: POWER_OR_RESET)`
- [ ] LED off in IDLE.
- Observed banner / LED: `____`

### 2. 1 Hz sensor stream

- [ ] `S1=____ S2=____` lines stream at ~1 Hz in every state.
- [ ] Counts not pinned at 0 or 4095 (dry air reads roughly 3000–3900 counts — SPEC §9.3).
- Observed: `S1=____ S2=____`

### 3. Button classification (BOOT/BOOTSEL, acts on release — SPEC §5.1)

- [ ] Short press logs `BTN short ____ms`, value < 1000.
- [ ] Long press logs `BTN long ____ms`, value ≥ 1000.
- Observed: `BTN short ____ms`, `BTN long ____ms`

### 4. Dry-fire the valve ×2–3 — each open < 5 s (solenoid overheats without flow, SPEC §9.1)

First dry-fire (fresh boot: recovery=0ms ⇒ table bypassed, fresh-tank default):
- [ ] `FILL start backstop=30min recovery=0ms recmin=0`
- [ ] `BASE ok s1=____ s2=____`
- [ ] `VALVE open budget=30min`
- [ ] LED solid blue while FILLING.
- [ ] Short press → `VALVE close` then `ABORT attemptms=____ openms=____` (close line first).
- [ ] `openms` < 5000.
- [ ] LED off after the abort (IDLE).

Second/third dry-fire (recovery > 0 now — the abort restarted the recovery clock, SPEC §6.1):
- [ ] `FILL start backstop=…min recovery=…ms recmin=0` — backstop from the §6.2 table
      (44 min when restarted within 2 min of the abort: first-knot clamp), NOT the 30-min default.
- [ ] Same order: `BASE ok s1=____ s2=____` → `VALVE open budget=…min` → `VALVE close` →
      `ABORT attemptms=____ openms=____`; open < 5 s; LED solid blue then off.
- Observed backstop/recovery: `____`

### 5. Stage-2 rail measurement (buck-only — no serial stream exists there)

- [ ] `YYYY-MM-DD_rails.md` created from `template-rails.md` and filled (both readings
      4.9–5.1 V, exclusivity ticks). Link: `____`

## Critical assertions

> `VALVE close` precedes every `ABORT` — valve-closed-first on every exit (SPEC §7.9/§10).
> Every dry-fire open stays < 5 s (SPEC §9.1).
> Mismatches go to the day's `YYYY-MM-DD-discrepancies.md` — never edit the captured log.

## Verification greps (operator hints)

    grep -c '^JugFiller boot (reset cause: POWER_OR_RESET)' YYYY-MM-DD_session-A-dry-bench.log
    grep -c '^VALVE open budget=' YYYY-MM-DD_session-A-dry-bench.log   # = dry-fire count
    grep -c '^ABORT attemptms=' YYYY-MM-DD_session-A-dry-bench.log     # = dry-fire count

## After the session

- [ ] Update README index row A (date, evidence files, FW git rev, pass/total, status).
- [ ] File any mismatch in `YYYY-MM-DD-discrepancies.md`.
