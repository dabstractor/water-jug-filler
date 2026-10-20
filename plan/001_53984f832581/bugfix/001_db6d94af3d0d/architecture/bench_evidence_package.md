# Bench validation evidence package — BUG-003 contract

Reconstructed from the original plan task `P1.M7.T2.S1` (recovered from
`plan/001_53984f832581/tasks.json` during research) + spec/SPEC.md §2.2/§4/§5/§7/§9/§10
+ docs/COMMISSIONING.md Stages 0–5 + firmware formatter contracts
(`jug_filler.ino:1340–1499`). PRD §9 steps 1, 2, 4 map to Sessions A–D below;
§10 watchdog maps to Session E.

## 1. BINDING power-safety rules (SPEC §2.2, §9.1–9.2; COMMISSIONING "Safety rules")

Hard finding (plan/001_53984f832581/architecture/hardware_validation.md L9,
verified against Waveshare FAQ): the RP2040-Zero 5V pin (VSYS) ties DIRECTLY to
USB VBUS with NO protective diode.

1. **Never USB + buck simultaneously.** Bench/commissioning sessions are
   USB-powered with the buck's 5 V output disconnected from the board.
2. 12 V may feed the MOSFET module's power-input block ALONE for valve dry-fire.
3. Rail check (Stage 2) is buck-only, USB unplugged → NO serial stream exists
   there; evidence is multimeter readings recorded in a markdown file:
   buck preset (board disconnected) 4.9–5.1 V; buck at board 5 V pin 4.9–5.1 V.
4. Dry-fire valve clicks < 5 s each (solenoid overheats without flow, SPEC §9.1).
   Technique: short-press BOOT to open, short-press again to abort.
5. Data-gathering fills need USB (log stream only exists in USB sessions).

## 2. Session checklists — expected ordered log prefixes + LED observations

Formats are APPEND-ONLY/prefix-anchored (`jug_filler.ino:1343–1344`) so evidence
greps anchor on exact prefixes. Ambient lines in every state:
`JugFiller boot (reset cause: POWER_OR_RESET|WATCHDOG)`, `BTN short|long <n>ms`,
`S1=<n> S2=<n>` (1 Hz).

### Session A — Dry bench check (SPEC §9.1, §4)
- Boot banner `… POWER_OR_RESET`; LED off (IDLE).
- 1 Hz line streaming; counts not pinned at 0/4095.
- `BTN short …ms` and `BTN long …ms` fire on release.
- Dry-fire ×2–3 (each open <5 s), order: `FILL start backstop=…min recovery=0ms
  recmin=0` → `BASE ok s1=… s2=…` → `VALVE open budget=…min` → `VALVE close` →
  `ABORT attemptms=… openms=…`; LED solid during FILLING, off after abort.

### Session B — Flow 1: abort + recomputed restart (SPEC §9.4, §6.1)
1. Fresh boot: `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok …` →
   `VALVE open budget=30min`.
2. Mid-fill short press: `VALVE close` → `ABORT …`.
3. Restart: `FILL start backstop=44min recovery=<small>ms recmin=0` — must NOT be
   `recovery=0ms`; budget from the backstop table, not FRESH_BOOT_BACKSTOP_MIN.

### Session C — Flow 2: successful fill (SPEC §9.4, §7)
`FILL start` → `BASE ok` → `VALVE open budget=…min` → (≥60 s open) dip sensor 1 →
`VALVE close` → `TRIP sensor=1 fillms=…` → dip sensor 2 within 12 s →
`CONFIRM pass sensor=2 durms=…` → `DONE attemptms=… openms=…` (LED green
triple-pulse: 3×200 ms on/200 ms off, 1000 ms pause) → short press → `ACK done`
→ next press starts a new fill.

### Session D — Fault paths (SPEC §7, §4 blink cadences: N-blink red groups,
250 ms on/250 ms off, 2000 ms pause)
- Wet sensor at start: `VALVE close` → `FAULT baseline sensor=N code=N count=…`;
  **critical assertion: no `VALVE open` line precedes the fault**; latches until RESET.
- No-confirm: trip sensor 1 after 60 s, never dip sensor 2 → (12 s) →
  `FAULT confirm sensor=2 code=2 durms=…`; LED red 2-blink groups.
- Optional guards: `FAULT earlywet/earlytrip sensor=N code=N fillms=…`.
- Backstop expiry `FAULT backstop code=3 budget=…min openms=…ms` — only if time
  permits (needs budget-minutes of open time); may be skipped with reason recorded.

### Session E — Optional watchdog check (SPEC §10)
Temporarily-compiled hang during FILLING → ~8 s (`WATCHDOG_TIMEOUT_S`) → board
resets: LED off, valve closed (audible click), banner `… (reset cause: WATCHDOG)`,
no auto fill start. See §5.

## 3. docs/bench/ layout (date-stamped, one file per session)

```
docs/bench/
  README.md                                # index: sessions, dates, firmware git rev, pass/fail per checklist item
  YYYY-MM-DD_session-A-dry-bench.log       # raw 115200 capture + operator header (date, build, wiring, deviations)
  YYYY-MM-DD_session-B-flow1-abort.log
  YYYY-MM-DD_session-C-flow2-success.log
  YYYY-MM-DD_session-D-fault-paths.log
  YYYY-MM-DD_session-E-watchdog.log        # only if run; note the temporary define, reverted after
  YYYY-MM-DD_rails.md                      # Stage-2 multimeter readings (no serial exists on buck-only)
  YYYY-MM-DD-discrepancies.md              # expected-vs-observed, log excerpt, follow-up issue # each
```

## 4. Executability split (critical for PRP agents)

**NOT executable by a software-only agent:** all captures, LED observations,
valve clicks, multimeter readings, the watchdog test flash. The original contract
says plainly: "This is a PHYSICAL task — requires the assembled hardware."

**Preparable without hardware (software agent):** everything in §3 — directory
skeleton, index template, per-session evidence templates with expected-prefix
checklists and pass/fail checkboxes, capture procedure (115200, save-to-file,
header block), discrepancy-list template.

**Integrity rule:** evidence logs must be genuine captures. If the rig is
unavailable, the execution subtask records a BLOCKED status with reason in the
index — NEVER fabricate or synthesize log contents.

## 5. Watchdog-hang mechanism — current state (scout-verified)

- `config.h` contains zero `#define`/`#if` (constexpr only); no `UNIT_TEST` or
  adjacent symbol exists anywhere in `firmware/`, `test/`, or `platformio.ini`.
- Only `#ifdef` boundary in the .ino is `#ifdef ARDUINO` (~L1402) separating the
  pure region from hardware glue.
- Watchdog: pico-sdk `hardware/watchdog.h`, `WATCHDOG_TIMEOUT_S = 8`, fed every
  loop pass; banner carries `watchdog_caused_reboot()` (.ino:85–87, 1657).
- Therefore the deliberate hang must be ADDED temporarily (e.g. a local
  `#if BENCH_HANG_TEST … while(true){} #endif` inside `loop()`), run the session,
  then REVERTED — never committed; `git status` must be clean of it afterward.
  The evidence file records that the patch was reverted.

## 6. Known doc nuance to carry in the discrepancy list (not a fix in this changeset)

COMMISSIONING.md describes an external GP14 green LED mirroring patterns, while
SPEC §4 calls the WS2812 the sole indicator. The firmware drives both (GP14
driver + config-gated WS2812 mirror, original plan P1.M5.T2/T3). If the rig
matches the firmware, record it as a spec/doc discrepancy in
`docs/bench/*-discrepancies.md` — do NOT edit spec/SPEC.md.
