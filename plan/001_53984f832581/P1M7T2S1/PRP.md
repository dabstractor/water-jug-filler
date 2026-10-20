name: "P1.M7.T2.S1 — Bench validation: power rules, flows 1/2, fault blink codes, log capture (re-plan, attempt 4 — FINAL, loop-terminating)"
description: "Operator-assisted physical bench validation per PRD §2.2/§4/§5.2/§7/§9. ATTEMPT-4 CORE CHANGE: three dispatches proved the executing environment has no rig, no interactive channel, and no escalation path — so the no-hardware path now returns result 'fail' (blocked-pending-hardware) to END the re-dispatch loop, per the attempt-3 issue's option (c). The full T2–T9 runbook is preserved for a bench-capable dispatch or a human operator running it manually from this PRP. Never fabricate evidence."

---

## Goal

**Feature Goal**: Physically validate the flashed firmware on the assembled bench rig: power rules (§2.2/§9.2), dry bench check (§9.1), dry-run Flow 1 (abort + recomputed backstop) and Flow 2 (success + DONE/acknowledge) per §5.2, fault paths with correct latched blink codes (§7), optional watchdog reset (§10) — capturing timestamped serial logs under `docs/bench/` and filing every deviation as a numbered discrepancy.

**Deliverable**: `docs/bench/` evidence (Mode A artifacts only, never firmware): per-test serial logs captured via `pio device monitor --filter time | tee`, a `SESSION_*.md` index with per-test PASS/FAIL, and `DISCREPANCIES.md` numbered findings feeding pre-Phase-2 fixes. Firmware tree, `platformio.ini`, `README.md`, `spec/` byte-identical at session end (T8's temporary hang block reverted).

**Success Definition**: Every required test T2–T7 has a captured log matching the normative log contract, each PASS backed by a quoted line (or operator-recorded LED/multimeter observation where serial is absent by design), faults latch with the correct blink code and clear only on RESET, and no fabricated artifact exists.

## Why

- Hardware acceptance gate of Phase 1: 192 native tests proved the logic; only the rig proves the safety claims (valve-closed on every path, fault latching, watchdog reset, never-USB-and-buck).
- Phase 2 (P2.M1 calibration/COMMISSIONING.md) consumes this session's observed dry/wet counts and discrepancy list as inputs.
- The pre-existing SPEC.md ↔ PRD/firmware LED conflict (G7) is exactly what this task surfaces and files.

## What

Three years of dispatch evidence (attempts 1–3) established: the executing host has no rig and no interactive channel. Therefore this PRP defines TWO paths and a loop-terminating verdict:

- **PATH A (hardware + operator present — bench host, or interactive session after the operator attaches the rig)**: execute T1–T9 below.
- **PATH B (no rig AND no interactive channel — the thrice-proven case)**: ~30 s probe, NO builds/tests, NO docs/bench/, and return `result: "fail"` (blocked-pending-hardware) — NOT "issue" — so the orchestrator stops re-queueing and a human acts. This implements attempt-3's own resolution option (c).
- If an interactive channel EXISTS but the rig is absent: post the ATTACH-REQUEST (below) up to 2×; operator attaches → PATH A; refuses/silent → PATH B verdict.

This PRP also doubles as the **standalone human runbook**: an operator may execute T2–T9 by hand from this document (flash `.pio/build/pico/firmware.uf2`, capture logs, bank them under `docs/bench/` per the layout below) — that session satisfies the item exactly as an agent-driven one would.

### Success Criteria

- [ ] `docs/bench/logs/` holds date-stamped `.log` files for executed tests (T2 evidence = operator-recorded readings, USB unplugged by design).
- [ ] Flow 1: `FILL start backstop=30min recovery=0ms` → abort → `FILL start backstop=44min recovery=<nonzero>ms` (the §5.2 recomputed timer).
- [ ] Flow 2 chain: `VALVE open` → `TRIP sensor=1 fillms=>=60000` → `VALVE close` → `CONFIRM pass sensor=2` → `DONE` → `ACK done` → next fill `backstop=44min recovery>0`.
- [ ] Faults: `FAULT baseline sensor=1 code=1` with NO `VALVE open` in that attempt; `FAULT confirm sensor=2 code=2`; both latch (BOOT logs `BTN short` but starts nothing); RESET clears to IDLE with recovery=0.
- [ ] LED cadences vs §4/§7 on the GP14 green LED, mirrored in color on the WS2812: FILLING/CONFIRM solid, DONE 3×(200/200)+1000, fault N×(250/250)+2000.
- [ ] No unintended watchdog reset during T3–T7; `git status --porcelain firmware/ platformio.ini README.md spec/` empty at session end.
- [ ] Every deviation recorded in DISCREPANCIES.md — never silently passed.

## All Needed Context

### Context Completeness Check

Validated: a cold implementer (or human operator) gets the exact serial-line contract, every timing constant dictating operator actions, binding power sequencing, flash/monitor mechanics, evidence layout, and both path contracts. Re-plan history: attempts 1–3 all blocked correctly on a hardware-less host (triple-probed each time: no ttyACM/USB serial, no RPI-RP2 mount, no lsusb vendor 2e8a); no evidence was ever fabricated; all software gates green on record (192/192 native, structural check PASS, stock UF2 built, firmware tree clean). Attempt-4 change: PATH B terminates the loop with `fail` instead of feeding it another `issue`.

### Documentation & References

```yaml
# MUST READ - include in your context window before starting
- file: spec/SPEC.md
  why: source of truth — wiring §2, states/LED §4, button §5, lifecycle/faults §7, tunables §8, commissioning §9, safety §10
  gotcha: SPEC §1/§4 says onboard WS2812 (GP16) is the SOLE LED / GP14 spare — the firmware and rig use GP14 external LED (PRD §2.1/§4, config.h PIN_LED=14). Do NOT fix; observe both, file discrepancy G7.

- file: README.md
  why: "Serial logging" table = normative log contract (matches logFillEvent() exactly)

- file: firmware/jug_filler/config.h
  why: all bench-relevant tunables — BASELINE_SETTLE_MS=2000, EARLY_WET_WINDOW_S=5, TRIP_MIN_ELAPSED_S=60, CONFIRM_WINDOW_S=12, DEBOUNCE_COUNT=8 @ READING_INTERVAL_MS=100, FRESH_BOOT_BACKSTOP_MIN=30, table {2,44},{10,41},{20,38},{35,34},{60,30}, WATCHDOG_TIMEOUT_S=8

- file: firmware/jug_filler/jug_filler.ino
  why: logFillEvent() ~line 1383 = exact printf shapes; loop() tail ~1640 = valve/LED/WS2812 wiring; loop() line 1 = watchdog_update(); setup() line 1 = valve LOW (never reorder)
  gotcha: BTN lines log on EVERY accepted release in ANY state — while FAULTed a press still logs `BTN short …ms` with no state change; the assertion is ABSENCE of FILL start/VALVE open, not absence of BTN.

- file: plan/001_53984f832581/architecture/hardware_validation.md
  why: verified facts — VSYS ties directly to USB VBUS with NO diode ⇒ never USB+buck; MOSFET module qualification gate; HW-390 wet counts unverified for the pair in hand; flyback 1N4148 marginal (prefer 1N4007/1N5819)

- file: plan/001_53984f832581/P1M7T2S1/research/notes.md
  why: §9.1 "<5 s dry-fire" vs Flow-2 "≥60 s open" CONFLICT RESOLUTION (flows run with 12 V disconnected; dedicated click test with 12 V connected); deterministic baseline-fault injection (AOUT jumper); watchdog pause_on_debug=true only pauses under an attached debugger

- file: plan/001_53984f832581/P1M7T2S1/research/replan_notes.md
  why: attempts 1–4 post-mortems, BTN-in-fault behavior, hang-define absence, UF2-overwrite hazard, environment-proof timeline

- url: https://www.waveshare.com/wiki/RP2040-Zero
  why: UF2 flash (hold BOOTSEL while plugging USB → RPI-RP2 drive → cp .uf2 → auto-reboot), pinout, VSYS/VBUS FAQ
- url: https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html
  why: pio device monitor — 115200 USB CDC capture
- url: https://docs.platformio.org/en/latest/core/userguide/device/monitoring/filters.html
  why: --filter time prepends wall-clock timestamps (built-in)
- url: https://docs.platformio.org/en/latest/core/envvars.html
  why: PLATFORMIO_BUILD_FLAGS="-DBENCH_HANG_TEST" — inject hang define WITHOUT touching platformio.ini
```

### Current Codebase tree (relevant slice)

```bash
firmware/jug_filler/        # jug_filler.ino (1672 lines) + config.h — THE ONLY FIRMWARE FILES
platformio.ini              # [env:pico] compile-only (no upload); [env:native] host tests
test/                       # 14 Unity suites + check_setup_first_line.py (structural safety gate)
spec/SPEC.md                # source of truth (known LED discrepancy vs firmware — file it)
docs/                       # DOES NOT EXIST YET — this task creates docs/bench/ (Mode A evidence only)
.pio/build/pico/firmware.uf2  # built stock UF2 (verified present; rebuild only in PATH A)
```

### Desired Codebase tree with files to be added

```bash
docs/bench/                                          # NEW — test-evidence artifacts only (never firmware)
  README.md                                          # index: what's here, capture method, naming rule
  SESSION_YYYYMMDD_HHMM.md                           # env, git rev, per-test results table, PARTIAL marking if any
  logs/
    YYYYMMDD_HHMM_T3_powerup_idle.log                # pio device monitor --filter time | tee captures
    YYYYMMDD_HHMM_T4_valve_clicks.log
    YYYYMMDD_HHMM_T5_flow1_abort.log
    YYYYMMDD_HHMM_T6_flow2_success.log
    YYYYMMDD_HHMM_T7a_fault_baseline.log
    YYYYMMDD_HHMM_T7b_fault_confirm.log
    YYYYMMDD_HHMM_T8_watchdog_hang.log               # only if T8 attempted
  DISCREPANCIES.md                                   # numbered findings feeding pre-Phase-2 fixes
```

Filenames use the ACTUAL runtime date (`date +%Y%m%d_%H%M`) — never invent a date.

### Known Gotchas of our codebase & Library Quirks

```text
G0  ORDERING (hard rule): the Task 0 probe is your FIRST tool-call batch. Running
    `pio test` / `pio run` / any build BEFORE path determination is a PRP
    VIOLATION — attempts 1–3 each burned the full suite before a blocked exit
    despite instructions; a no-hardware verdict needs zero gate output (evidence
    is already on record from all three attempts).
G1  POWER (BINDING, §2.2): RP2040-Zero 5V pin (VSYS) ties DIRECTLY to USB VBUS, no
    diode (Waveshare FAQ). NEVER USB and buck simultaneously. Bench order: buck
    checks first with USB unplugged, then buck fully disconnected; everything else
    USB-powered. 12 V may feed the MOSFET module alone.
G2  USB CDC re-enumerates after every flash: the monitor dies. Restart it, then
    press RESET for a fresh `JugFiller boot (reset cause: POWER_OR_RESET)` banner.
G3  [env:pico] is compile-only: flash = UF2 drag-drop (hold BOOTSEL while plugging
    USB → RPI-RP2 drive → cp firmware.uf2 → auto-reboot).
G4  PLATFORMIO_BUILD_FLAGS builds OVERWRITE .pio/build/pico/firmware.uf2 — stash
    stock UF2 (cp to /tmp) BEFORE the T8 hang build; flash stock back after.
G5  TRIP_MIN_ELAPSED_S=60 + EARLY_WET_WINDOW_S=5: first dip legal ≥ 65 s after
    VALVE open; keep the cup away for the first 5 s. Count the wait via the 1 Hz
    S1=/S2= lines. Dipping early yields FAULT earlytrip/earlywet — legal firmware
    behavior, wrong test.
G6  DEBOUNCE_COUNT=8 @ 100 ms: hold each dip ~2–3 s. CONFIRM_WINDOW_S=12: second
    dip must COMPLETE within 12 s of the first TRIP line — dip ~3–5 s after it.
G7  Pre-existing doc discrepancy (FILE, DO NOT FIX): SPEC.md §1/§4 "WS2812 sole
    LED / GP14 spare" vs PRD §2.1/§4 + config.h (PIN_LED=14, USE_ONBOARD_RGB=true
    mirror). Observe BOTH; any level divergence is a real fault (same engine tick).
G8  Watchdog watchdog_enable(8000, true) last in setup(); pause_on_debug only
    pauses under an ATTACHED debugger — none here ⇒ a compiled hang trips ≤ 8 s.
    The hang build re-hangs after every reset (banner repeats ~every 8 s); recover
    by re-flashing stock UF2.
G9  Empty `while(true){}` can be elided at -O2 — the hang block must contain
    `__asm volatile ("nop");`.
G10 Serial while buck-powered is DROPPED by design (no !Serial gate; logging never
    stalls): T2 evidence is LED-observation only.
G11 Buck output MUST read 4.9–5.1 V (multimeter) before the board's first
    buck-powered connection; out of range ⇒ operator adjusts trim pot, re-measure.
G12 Solenoid dry-fire heats the coil: every 12 V energization < 5 s (§9.1). All
    flow/fault tests run with 12 V DISCONNECTED from the module.
```

## Execution Contract — Task 0 runs FIRST (attempt-4 core)

**Determine the path before ANY build.** Work this decision tree exactly:

```text
STEP 1  PROBE (first tool-call batch; ~5 s):
        pio device list
        ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
        lsblk -o NAME,MODEL | grep -iE 'rp2|rp2040'; ls /media/*/RPI-RP2 /run/media/*/RPI-RP2 2>/dev/null
        lsusb | grep 2e8a            # RP2040 vendor ID
        Board present if ANY probe succeeds.

STEP 2a Board PRESENT → operator handshake (below) → PATH A → Task 1.
STEP 2b Board ABSENT and an interactive channel EXISTS (you can post a question
        and expect a reply in-session) → ATTACH-REQUEST (below), end turn, WAIT.
        "attached / GO" → re-probe → PATH A. Refused/silent after 2 asks
        (≥ 60 s apart) → PATH B verdict.
STEP 2c Board ABSENT and NO interactive channel (one-shot pipeline — the
        attempts-1–3 case) → PATH B verdict NOW. No builds. No docs/bench/.
```

**ATTACH-REQUEST** (interactive case only; post verbatim, end turn, wait):

> "BENCH HARDWARE NEEDED — this task validates firmware on the physical rig and I cannot proceed without it. Please attach to THIS machine and reply GO (or tell me what's missing): ① Waveshare RP2040-Zero on USB, buck DISCONNECTED from the board, sensors dry in open air; ② both HW-390 sensors on GP26/GP27; ③ MOSFET module on GP15 with the valve + flyback; ④ green LED on GP14; ⑤ within reach: LM2596 buck + 12 V supply, multimeter, cup of water, one jumper wire. If you cannot attach the rig now, reply NO and I will report the item failed-pending-hardware. Nothing will be fabricated either way."

**Operator handshake (board present)** — post, end turn, wait:

> "BENCH SESSION — I need you as operator for ~25 min: press BOOT/RESET, dip sensors, take two multimeter readings, swap power connections when I say. I capture and verify all serial logs. Reply GO to start, NO to report failed-pending-hardware."

**PATH B verdict — the loop terminator (attempt-3 issue, option (c)).**
After three identical hardware-absent dispatches, another `issue` merely re-queues
into the same loop. PATH B therefore returns:

```json
{
  "result": "fail",
  "message": "FAILED-PENDING-HARDWARE — not a requirements defect: P1.M7.T2.S1 is a PHYSICAL bench task and this dispatch environment has no rig and no interactive channel (probe: <probe summary>). Attempts 1–3 verified the same condition (no ttyACM/ttyUSB, no RPI-RP2 mount, no lsusb 2e8a) with all software gates green (pio test -e native 192/192; check_setup_first_line.py PASS; pico UF2 built) — re-dispatching here cannot converge. No evidence fabricated; docs/bench/ not created; firmware tree untouched. REOPEN when either: (a) the assembled rig (RP2040-Zero USB + 2x HW-390 GP26/GP27 + MOSFET GP15 + LED GP14 + valve/flyback + LM2596 + 12V + multimeter + water) is attached to the executing host with an operator available — the full T2–T9 runbook with grep assertions is in this item's PRP; or (b) the item is dispatched to a bench-capable host; or (c) a human operator executes the PRP runbook manually and banks logs under docs/bench/."
}
```

**Rig tiers** (run what the attached rig supports; bank everything honestly):

| Tier | Hardware present | Tests executable |
|---|---|---|
| T-min | board + USB only | T3-smoke (flash, IDLE, BOOT logging — `FAULT baseline` on floating sensor pins is a VALID smoke outcome proving the log pipeline), T8 watchdog |
| T-sensors | + both HW-390 wired, dry in air, cup + jumper | T5, T6, T7a, T7b (12 V stays disconnected) |
| T-power | + buck, multimeter | T2 |
| T-valve | + 12 V supply, MOSFET module, valve | T4 |

Rules: a partial run marks the SESSION file **PARTIAL — tier <name>**, lists skipped
tests + why, and the final JSON stays `result: issue` (message: "PARTIAL bench
evidence banked under docs/bench/ — tests T… remain; re-invoke with tier …
attached") UNLESS T2–T7 ALL have evidence → `complete`. A later session RESUMES:
read `docs/bench/SESSION_*.md` first; banked PASSes are not re-run unless their
precondition changed (re-flash, config change). T3-smoke alone is never complete.

Never fabricate any log, observation, date, or file. An unmeasured value recorded
as measured is the only unrecoverable failure of this task.

## Operator Protocol (PATH A)

- **Agent = recorder/verifier**: background monitor captures, grep assertions, results table, all evidence files. **Operator = hands**: every physical action is a numbered `OP-n` prompt with what they should SEE. ONE OP at a time; end your turn; verify the log BEFORE the next OP.
- PASS = expected lines found by `grep -E '<pattern>' <logfile>` (or operator-reported reading recorded verbatim as operator-observed).
- Deviation ⇒ FAIL/DEVIATION row + quoted excerpt + numbered discrepancy; continue if safe. STOP and disconnect 12 V if: valve energizes with no `VALVE open` logged, FET hot, smoke/heat/smell, or buck out of range with the board connected.
- Capture pattern per test:

```bash
mkdir -p docs/bench/logs
STAMP=$(date +%Y%m%d_%H%M); TEST=t5_flow1_abort
bash -c 'pio device monitor -b 115200 --filter time 2>&1 | tee docs/bench/logs/'"$STAMP"'_'"$TEST"'.log' &
# ... OPs ... then: grep -E '<pattern>' docs/bench/logs/${STAMP}_${TEST}.log ; kill the monitor job
```

Port permission denied ⇒ `sudo usermod -aG dialout $USER` + re-login, or sudo the monitor and `chown $USER` the log after.

## Implementation Blueprint — the bench runbook (ordered)

### Reference: normative serial-line contract (README.md "Serial logging")

```text
JugFiller boot (reset cause: POWER_OR_RESET)   # setup() banner; WATCHDOG variant after WDT reset
BTN short <ms>ms | BTN long <ms>ms             # every accepted BOOT release, ANY state
S1=<n> S2=<n>                                  # 1 Hz commissioning pair (12-bit counts)
FILL start backstop=<m>min recovery=<ms>ms     # fill authorized; recovery=0 ⇒ fresh boot
BASE ok s1=<n> s2=<n>                          # dry baselines accepted
FAULT baseline sensor=<n> code=<n>             # §7.1 — sensor n 1-based, code = blink group
VALVE open budget=<m>min                       # valve opened (budget incl. stacked +5s)
TRIP sensor=<n> fillms=<ms>                    # first sensor trip (valve-open time)
CONFIRM pass sensor=<n> durms=<ms>             # second sensor confirmed in window
DONE attemptms=<ms> openms=<ms>                # fill complete
ACK done                                       # DONE acknowledged → IDLE
PLUS5 budget=<m>min                            # long-press +5 during FILLING (stacks)
VALVE close                                    # ANY fill end; ALWAYS precedes the reason line
ABORT attemptms=<ms>                           # short-press abort
FAULT earlywet sensor=<n> code=<n>             # wet inside 5 s of open (§7.6)
FAULT earlytrip sensor=<n> code=<n>            # trip before 60 s elapsed (§7.6)
FAULT confirm sensor=<n> code=<n>              # other sensor never tripped in 12 s (§7.5)
FAULT backstop code=3                          # backstop expired, no trip (§7.7)
```

### Reference: timing dictating operator actions (config.h, verified)

```text
press → ~2 s settle → BASE ok → VALVE open            (BASELINE_SETTLE_MS=2000)
first dip legal ≥ 60 s after VALVE open — USE ≥ 65 s   (TRIP_MIN_ELAPSED_S=60; G5 margin)
cup away first 5 s after open                         (EARLY_WET_WINDOW_S=5)
each dip held ~2–3 s                                   (DEBOUNCE_COUNT=8 @ 100 ms)
second dip completes ≤ 12 s after first TRIP           (CONFIRM_WINDOW_S=12)
fresh boot / RESET ⇒ backstop=30min recovery=0ms       (FRESH_BOOT_BACKSTOP_MIN=30, §5.3)
fill ended < 2 min ago ⇒ backstop=44min recovery>0     (table left edge {2,44})
+5 long press in FILLING ⇒ PLUS5 budget=<44+5>min      (PLUS5_MIN=5, stacks)
DONE LED: 3×(200/200) + 1000 off, period 2.2 s         (§4)
FAULT LED: N×(250/250), 2000 off, repeat               (§7; N = fault code)
ACK transient: 3×(100/100) then resumes solid          (§4)
watchdog: 8 s (hang build: WATCHDOG banner ≤ 8 s, recovery=0, IDLE)
```

### Task 1 — Pre-flight (PATH A only; skip already-banked tests per resume rule)

```yaml
Task 1: VERIFY gates + stock UF2 + scaffold
  - RUN: pio test -e native                       # expect all PASSED (192 on record)
  - RUN: python3 test/check_setup_first_line.py   # expect PASS
  - RUN: pio run -e pico; ls -la .pio/build/pico/firmware.uf2   # SUCCESS + present
  - RUN: git rev-parse --short HEAD               # record in SESSION
  - RUN: cp .pio/build/pico/firmware.uf2 /tmp/stock_jugfiller.uf2   # G4 stash
  - CREATE: docs/bench/README.md, logs/, DISCREPANCIES.md skeletons
  - RESUME CHECK: read any existing docs/bench/SESSION_*.md — banked PASSes are not re-run
```

### Task 2 — Power rails (§2.2/§9.2; tier T-power; operator + multimeter)

```yaml
  - OP-1: "Unplug USB. Buck 12 V powered, 5 V output NOT connected to the board.
    Measure the buck 5 V output terminals; read me the value." PASS: 4.9–5.1 V
    (out ⇒ trim pot, re-measure — G11). Record value.
  - OP-2: "Connect buck 5 V → board 5V pin, GND → GND (USB still unplugged).
    Measure AT the board 5V pin." PASS: 4.9–5.1 V under load. Record value.
  - OP-3 (LED-only, G10): "Watch the LED: press BOOT once, wait ~4 s, press BOOT
    briefly." EXPECT: power-up off (IDLE) → solid (FILLING ~2 s after press) → off
    (abort). Record as operator-observed.
  - OP-4: "Disconnect the buck from the board entirely. From now on USB-only;
    12 V may reach the MOSFET module input block alone (T4) and nowhere else."
```

### Task 3 — Flash + §9.1 dry bench check (USB-powered, buck disconnected)

```yaml
  - OP-5: "Hold BOOTSEL, plug USB, release — a drive named RPI-RP2 mounts. Tell me
    the mount path." AGENT: cp .pio/build/pico/firmware.uf2 <mount>/ (auto-reboot, G2/G3)
  - START monitor → *_T3_powerup_idle.log
  - OP-6: "Press and release RESET." EXPECT: POWER_OR_RESET banner; IDLE: S1=/S2=
    at ~1 Hz, LED off, NO VALVE/FILL lines. VERIFY: grep 'reset cause:
    POWER_OR_RESET'; S1= line count grows ~10 s; grep -E 'VALVE|FILL' empty.
    Record observed dry-count range (§9.3 calibration note). T-min note: on floating
    sensor pins the S1/S2 counts are meaningless and a later fill-start may FAULT
    baseline — still a valid log-pipeline smoke; mark T3 accordingly.
  - OP-7a: "Short-press BOOT." EXPECT: BTN short → ~2 s → BASE ok → VALVE open
    budget=30min → LED solid (12 V disconnected ⇒ pin-high only, no click) →
    "Short-press BOOT again." → VALVE close + ABORT attemptms=~5000ms → LED off.
    (T-min: BASE ok may be FAULT baseline instead — valid smoke outcome.)
  - OP-7b: "Long-press BOOT (≥1 s) in IDLE." EXPECT: BTN long …ms, nothing else (§5.1).
  - VERIFY: grep -E 'BTN (short|long)'; 'BASE ok|FAULT baseline'; 'VALVE open
    budget=30min'; 'VALVE close'; 'ABORT attemptms='.
```

### Task 4 — §9.1 valve dry-fire clicks (tier T-valve; 12 V to module alone)

```yaml
  - OP-8: "Connect 12 V to the MOSFET module POWER-INPUT block only (confirm input
    vs output block from silkscreen + multimeter — output dead until TRIG). Board
    stays USB-powered; buck disconnected."
  - START monitor → *_T4_valve_clicks.log; OP: RESET for fresh banner.
  - OP-9 (×3, ≥10 s cooling between): "Short-press BOOT; the instant the valve
    clicks, count ~2–3 s, short-press BOOT again." EACH: BASE ok → VALVE open →
    audible click + module LED on → VALVE close + ABORT → click release. Operator
    confirms: crisp pull-in, module LED, FET cool (§2.2 qualification). STRICT <5 s
    per energization (G12).
  - VERIFY: 'VALVE open' count == 3; 'ABORT' count == 3.
  - OP-10: "Disconnect 12 V from the module — it stays disconnected for ALL
    remaining tests."
```

### Task 5 — Flow 1: abort + fresh recomputed timer (§5.2; tier T-sensors)

```yaml
  - START monitor → *_T5_flow1_abort.log; OP: RESET (fresh: recovery=0, backstop=30).
  - OP-11: "Short-press BOOT." EXPECT: BTN short → ~2 s → BASE ok →
    FILL start backstop=30min recovery=0ms → VALVE open budget=30min → LED solid.
  - OP-12 (~10–20 s in): "Short-press BOOT." EXPECT: VALVE close → ABORT
    attemptms=~10000-20000ms → LED off.
  - OP-13 (immediately, <1 min after abort): "Short-press BOOT." EXPECT:
    FILL start backstop=44min recovery=<nonzero>ms   ← THE recomputed-timer evidence
    (fill ended <2 min ago ⇒ table left edge 44). → BASE ok → VALVE open.
  - OPTIONAL OP-13b: "Long-press BOOT." EXPECT: BTN long + PLUS5 budget=49min +
    LED 3×(100/100) then solid.
  - OP-14: "Short-press BOOT to abort." EXPECT: VALVE close + ABORT → IDLE.
  - VERIFY: 'FILL start backstop=30min recovery=0ms'; 'FILL start backstop=44min
    recovery=[1-9][0-9]*ms'; ABORT count == 2 (3 with OP-13b); every VALVE open has
    a VALVE close; NO FAULT lines. Mismatch ⇒ discrepancy before T6.
```

### Task 6 — Flow 2: success → DONE → acknowledge → next fill (§5.2; ~2.5 min)

```yaml
  - START monitor → *_T6_flow2_success.log; OP: RESET (fresh).
  - OP-15: "Short-press BOOT; cup well away from both tips." EXPECT: BASE ok →
    FILL start backstop=30min recovery=0ms → VALVE open → LED solid.
  - WAIT ≥ 65 s via the 1 Hz S1=/S2= lines (G5).
  - OP-16: "Dip sensor 1's tip to mount depth, hold ~2–3 s, remove." EXPECT:
    TRIP sensor=1 fillms=[6-9][0-9]{4,} → VALVE close → LED stays SOLID (CONFIRM)
    → WS2812 blue. FAULT earlytrip/earlywet instead ⇒ dipped early — discrepancy,
    RESET, restart from OP-15.
  - OP-17 (~3–5 s after TRIP, complete ≤ 12 s after it): "Dip sensor 2's tip,
    hold ~2–3 s, remove." EXPECT: CONFIRM pass sensor=2 durms=<ms> → DONE
    attemptms=<ms> openms=<ms> → green TRIPLE-PULSE 3×(200/200)+1000 repeating
    (~2.2 s period, operator counts against stopwatch). WS2812 same in green.
  - OP-18: "Short-press BOOT." EXPECT: BTN short + ACK done → LED off (IDLE).
  - OP-19: "Short-press BOOT once more." EXPECT: FILL start backstop=44min
    recovery=<nonzero>ms → abort via short press → IDLE.
  - VERIFY: 'TRIP sensor=1'; 'CONFIRM pass sensor=2'; 'DONE attemptms=.* openms=';
    'ACK done'; 'FILL start backstop=44min recovery=[1-9]'; VALVE open/close
    balanced; no FAULT lines. Record wet counts seen during dips (§9.3/P2 input).
```

### Task 7 — Fault paths (tier T-sensors)

```yaml
Task 7a BASELINE FAULT (code 1; valve never opens):
  - START monitor → *_T7a_fault_baseline.log
  - OP-20: "Jumper sensor 1's AOUT to GND (deterministic ~0 counts; a wet-tip dip
    may land in-range — research notes §4). Sensor 2 dry in air."
  - OP-21: "Short-press BOOT." EXPECT ~2 s: FAULT baseline sensor=1 code=1; NO
    'VALVE open' and NO 'BASE ok' this attempt; LED ONE 250 ms blink ~every 2.5 s
    (group 1); WS2812 red.
  - OP-22 (latch): "Short-press BOOT twice." EXPECT: BTN short lines DO appear,
    NO FILL start / VALVE open; pattern unchanged (§7 BOOT-ignored).
  - OP-23: "Press RESET." EXPECT: POWER_OR_RESET banner → IDLE, S1=/S2= resume.
  - VERIFY: 'FAULT baseline sensor=1 code=1'; no VALVE open in the faulted attempt;
    BTN short present while faulted; POWER_OR_RESET after.
  - OP-24: "Remove the jumper."

Task 7b CONFIRM-TIMEOUT FAULT (code 2):
  - START monitor → *_T7b_fault_confirm.log; OP: RESET.
  - OP-25: "Short-press BOOT; cup away." EXPECT normal start → VALVE open, solid.
  - WAIT ≥ 65 s. OP-26: "Dip sensor 1 only, ~2–3 s, remove. Then NOTHING — keep
    sensor 2 dry and wait." EXPECT: TRIP sensor=1 → VALVE close → solid (CONFIRM)
    → after 12 s: FAULT confirm sensor=2 code=2 → TWO 250 ms blinks ~every 2.5 s
    (group 2); WS2812 red. Operator times the 12 s window.
  - OP-27: "Press RESET." EXPECT: POWER_OR_RESET → IDLE.
  - VERIFY: 'TRIP sensor=1'; 'FAULT confirm sensor=2 code=2'; VALVE close precedes
    the fault line (§7.9 ordering); POWER_OR_RESET after.
  - SKIP backstop code=3 (30-min wait; covered natively). Optional 30-s bonus if
    offered: dip inside first 5 s ⇒ FAULT earlywet.
```

### Task 8 — OPTIONAL watchdog hang test (any tier; do LAST; full restore after)

```yaml
  - EDIT (TEMPORARY) firmware/jug_filler/jug_filler.ino — in loop(), immediately
    AFTER the existing first line `watchdog_update();`:
        #ifdef BENCH_HANG_TEST
        logLine("BENCH hang: no watchdog service — expect WATCHDOG reset <=8s");
        while (true) { __asm volatile ("nop"); }   // G9
        #endif
    Touch NOTHING else (setup(), platformio.ini untouched).
  - RUN: PLATFORMIO_BUILD_FLAGS="-DBENCH_HANG_TEST" pio run -e pico   # G4 overwrite
  - OP-28: "Hold BOOTSEL, plug USB, release; give me the mount path."
    cp .pio/build/pico/firmware.uf2 <mount>/   (HANG build)
  - START monitor → *_T8_watchdog_hang.log. EXPECT: POWER_OR_RESET banner, the
    BENCH hang line, then ≤ 8 s later `reset cause: WATCHDOG` — repeating ~every
    8 s (G8). VERIFY: grep -c 'reset cause: WATCHDOG' ≥ 2. LED off each reset,
    valve closed, recovery semantics reset.
  - RESTORE (mandatory, in order): 1) git checkout -- firmware/jug_filler/jug_filler.ino
    2) pio run -e pico (stock builds clean) 3) OP-29: "Hold BOOTSEL, press+release
    RESET, release BOOTSEL; mount path?" cp /tmp/stock_jugfiller.uf2 <mount>/
    4) VERIFY POWER_OR_RESET banner + S1=/S2= + IDLE; git status --porcelain
    firmware/ platformio.ini README.md spec/ EMPTY.
  - Any restore step fails ⇒ STOP and report — a contaminated tree is worse than
    a skipped optional test.
```

### Task 9 — Evidence finalization

```yaml
  - COMPLETE SESSION_YYYYMMDD_HHMM.md: environment (machine, operator), git rev,
    per-test table (test | log file | PASS/FAIL/DEVIATION | evidence lines), T2
    readings verbatim, observed dry/wet count ranges, tier + PARTIAL marking if any.
  - COMPLETE DISCREPANCIES.md — numbered; MUST include at minimum:
    D1 (G7): SPEC.md §1/§4 "WS2812 sole LED / GP14 spare" vs PRD §2.1/§4 +
    config.h PIN_LED=14 external-authoritative — cite both; human decision needed
    before Phase 2 docs. D2+: session deviations with log excerpts (BTN-during-FAULT
    spec silence, counts outside §9.3 expectations, flyback part actually used,
    LED cadence drift).
  - WRITE docs/bench/README.md: Mode A evidence, spec/SPEC.md stays source of
    truth, naming convention, capture command.
  - FINAL: git status --porcelain firmware/ platformio.ini README.md spec/ empty;
    docs/bench/ holds only .md + .log (no binaries/firmware).
```

### Integration Points

```yaml
HARDWARE (operator wiring only, no repo change):
  - USB → board (buck DISCONNECTED) for T3–T8; 12 V → module input block only for T4
EVIDENCE:
  - docs/bench/** — new; Mode A test evidence only
  - DISCREPANCIES.md feeds pre-Phase-2 fixes (P2.M1 input)
NO firmware/ or platformio.ini changes survive (T8 edit reverted same session).
```

## Validation Loop

### Level 0: Path gate (FIRST tool-call batch — see Execution Contract)

Probe → recruitment/handshake (only if interactive) → tier determination. No builds before this resolves.

### Level 1: Software gates (PATH A only, before flashing)

```bash
pio test -e native                       # all PASSED (192/192 on record)
python3 test/check_setup_first_line.py   # PASS
pio run -e pico && ls -la .pio/build/pico/firmware.uf2   # SUCCESS + present
```

### Level 2: Per-test log assertions (the bench validation itself)

After each OP, grep the active log for expected patterns; a test is PASS only when
every expected pattern matches AND every forbidden pattern is absent (`VALVE open`
during baseline fault; `FILL start` while FAULTed; unexplained `FAULT` in T5/T6).
Quote matching lines into the SESSION table.

### Level 3: Session-artifact audit (T9)

```bash
find docs/bench -type f | sort                # only .md and .log
grep -rE '20[0-9]{6}_[0-9]{4}' docs/bench     # real runtime datestamps
git status --porcelain firmware/ platformio.ini README.md spec/   # empty
git diff --stat                               # only docs/bench/ + orchestrator plan/ entries
```

### Level 4: Safety re-checks (continuous)

- USB and buck never simultaneous; every 12 V energization < 5 s; FET cool; every
  `VALVE open` followed by a close in the same capture; stock UF2 on the board at
  session end (POWER_OR_RESET banner, IDLE).

## Final Validation Checklist

### Technical Validation

- [ ] Task 0 path determination executed BEFORE any build (G0)
- [ ] PATH B used the fail verdict (never a fourth bare `issue` re-queue) with zero gate runs
- [ ] Level 1 gates green in PATH A before flashing
- [ ] Every executed test has a timestamped log file; every PASS quotes evidence
- [ ] Forbidden-pattern checks negative (T7a no VALVE open; T7 latch no FILL start)
- [ ] T8 (if run): source reverted, stock UF2 re-flashed, firmware tree clean

### Feature Validation

- [ ] Flow 1: backstop 30→44 with recovery>0 on restart; Flow 2 full chain with fillms ≥ 60000
- [ ] Fault codes 1 and 2 correct, latched, BOOT-ignored, RESET-cleared (recovery=0)
- [ ] §9.1 idle/BOOT-logging/3×<5 s clicks; §9.2 buck 4.9–5.1 V both measurements recorded
- [ ] Watchdog (if run): ≥2 WATCHDOG banners; lands IDLE/valve-closed

### Code Quality Validation

- [ ] No repo file outside docs/bench/ (and orchestrator plan/) modified
- [ ] Evidence is plain text; no fabricated values/dates/observations; PARTIAL marking honest
- [ ] DISCREPANCIES.md includes the G7 LED spec-conflict with citations

### Documentation & Deployment

- [ ] docs/bench/README.md explains capture method + naming; SESSION records tier,
      git rev, results, operator readings
- [ ] Final JSON: complete (T2–T7 evidenced) / issue (PARTIAL-banked) / fail (PATH B)

## Anti-Patterns to Avoid

- ❌ NEVER fabricate or reconstruct a log line, reading, date, or observation — missing = recorded as missing
- ❌ NEVER run builds/tests before the Task 0 path resolves (attempts 1–3 failure mode)
- ❌ NEVER return a bare `issue` on the no-hardware/no-channel path — use the fail verdict to end the loop
- ❌ NEVER have USB and buck connected simultaneously — including "just quickly" in T2
- ❌ NEVER leave BENCH_HANG_TEST code in the tree, the hang UF2 on the board, or skip revert verification
- ❌ NEVER energize the valve ≥ 5 s dry or run flow tests with 12 V connected
- ❌ NEVER dip before 65 s of open time, or dip sensor 2 later than 12 s after sensor 1's TRIP
- ❌ NEVER treat a BTN line during FAULT as a bug — the assertion is absence of FILL start/VALVE open
- ❌ NEVER "fix" SPEC.md LED wording, pins, or any observed discrepancy — file it; fixes belong to follow-ups
- ❌ NEVER report complete on a partial-tier run; NEVER mark a deviating step PASS
