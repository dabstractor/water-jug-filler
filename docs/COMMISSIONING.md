# Commissioning & Calibration Runbook

This runbook takes a freshly assembled jug-filler board from first power-up to
commissioned, calibrated hardware. It walks PRD spec §9 ("Commissioning &
calibration") end-to-end: every step names the exact action to perform, the
exact serial log lines to watch, the numbers to write down, and — where
calibration is needed — the exact constant to edit in
[`firmware/jug_filler/config.h`](../firmware/jug_filler/config.h). You do not
need any knowledge of the firmware source; everything required is in this file
plus the [README](../README.md).

**Audience:** a bench operator who did not write the firmware — a lab tech, a
colleague, or a hardware-capable agent.

**Prerequisites:**

- Assembled rig per spec §1/§2: Waveshare RP2040-Zero; two HW-390-type analog
  sensors on GP26 (sensor 1) and GP27 (sensor 2), powered from 3V3; a
  logic-level MOSFET module on GP15 driving the solenoid valve (with flyback
  diode); green LED on GP14 via 330 Ω; LM2596 buck converter from the 12 V
  supply to 5 V.
- 12 V supply, multimeter, cup of water, USB data cable, PC with a serial
  monitor.
- Built firmware: `pio run -e pico` from the repo root (README → "Build with
  PlatformIO") or the Arduino IDE (README → "Build in Arduino IDE").

**Conventions used below:**

- Log lines are quoted verbatim; the numbers in them are illustrative — yours
  will differ. `____` marks a value **you** write down.
- **sensor 1** = GP26 = `sensor=1` / `code=1` in logs. **sensor 2** = GP27 =
  `sensor=2` / `code=2`. Fault `code=3` = the timer (backstop).
- Every edit goes to `firmware/jug_filler/config.h` — the single source of
  truth for all tunables (spec §8). This runbook edits **only** group
  `5. ADC / sensors` and group `3. Fill backstop model`. Never edit pins,
  watchdog, or LED timing constants.

---

## ⚠️ Safety rules — read before touching the hardware

> **1. NEVER connect USB while the buck powers the board.** On the
> RP2040-Zero the 5 V pin (VSYS) ties directly to USB VBUS with no isolation
> diode — connecting both backfeeds one supply into the other and can damage
> your PC's USB port (spec §2.2). Exactly one source at all times:
>
> | Situation | Board power | Buck 5 V to board |
> | --- | --- | --- |
> | Bench / commissioning (Stages 0, 1, 3, 4) | USB | **disconnected** |
> | Rail check (Stage 2) | buck | **connected**, USB unplugged |
> | In service (after commissioning) | buck | **connected**, USB unplugged |
>
> 12 V may feed the MOSFET module's power-input block alone during bench valve
> tests — that is safe — but the buck's 5 V output stays off the board whenever
> USB is plugged in.
>
> **2. Preset the buck before the board's FIRST buck-powered boot.** With the
> buck feeding only the board's 5 V pin, measure its output with a multimeter:
> it must read **4.9–5.1 V**. Never let the board boot from an unpreset buck.
>
> **3. Dry-fire clicks are <5 s each.** Energizing the solenoid without
> flowing water for more than a few seconds overheats it (spec §9.1). Stage 1
> shows the exact two-press technique the firmware gives you for this — there
> is no separate "click" command.
>
> **4. Do not raise `RGB_BRIGHTNESS`** (shipped at 60/255). The cap protects
> the 3.3 V LDO budget (spec §2.2). Commissioning edits are group 5 and group
> 3 only.
>
> **5. Route the sensor AOUT wires away from the valve/solenoid wiring.**
> Valve switching noise couples into the analog readings (spec §2.2).

Two reassuring facts from the firmware's safety invariants (spec §10): the
valve pin is driven LOW as the very first action of `setup()` and the firmware
never auto-starts a fill — the valve opens only after a human BOOT press. If
the firmware ever hangs, the ~8 s hardware watchdog resets the board with the
valve closed.

---

## Reading the log stream (your instrument)

Plug the board into the PC over USB (buck disconnected — rule 1) and open a
serial monitor at **115200 baud** (`SERIAL_BAUD`). The full line-by-line table
lives in the README → "Serial logging"; this runbook quotes each line at the
step where you'll see it. Three things worth knowing up front:

- The 1 Hz commissioning line `S1=3487 S2=3512` runs in **every** state — it is
  your live window on both sensors at any time.
- Button lines print when you **release** the BOOT button (`BTN short 1234ms`,
  `BTN long 1500ms`; the ms value is the raw hold time, diagnostic only).
- A fault latches (LED blinks its code until RESET) and always prints
  `VALVE close` first, then the reason line. Codes: `1` sensor 1, `2` sensor 2,
  `3` timer.

**LED cues:** the firmware drives two indicators that always show the same
**pattern shape** — the onboard WS2812 mirror (adds color) and the external
LED on GP14 (on/off). The shapes you will see: IDLE = off; FILLING and CONFIRM
= solid; DONE = triple-pulse (3× on 200 ms / off 200 ms, then a 1000 ms pause,
repeating); FAULT = blink groups of *code* blinks (250 ms each, 2000 ms pause
between groups); a +5 min ack = a fast 3× flash overlay (100 ms on/off).

---

## Stage 0 — Bench setup & flashing

**Goal:** board flashed, serial monitor connected, boot confirmed. USB power
only; buck's 5 V disconnected from the board (rule 1).

1. Build: `pio run -e pico` at the repo root. The flashable image appears at
   `.pio/build/pico/firmware.uf2`.
2. Hold the board's **BOOTSEL** button while plugging in the USB cable. A
   drive named `RPI-RP2` appears; drag `firmware.uf2` onto it. The board
   reboots into the new firmware (drive disappears). *(Arduino IDE users:
   Sketch → Upload does this instead.)*
3. Open the serial monitor at **115200** baud.

**Watch for** (within a second of boot):

```
JugFiller boot (reset cause: POWER_OR_RESET)
```

`reset cause: WATCHDOG` instead means the previous session ended via the §10
watchdog path — worth noting, not a problem during bench bring-up. If you see
`RGB mirror init FAILED (no PIO SM)`, the onboard mirror is disabled but the
external LED still shows every pattern.

**Record:** boot banner line: `____________________`

---

## Stage 1 — Dry bench check (spec §9.1)

**Goal:** prove IDLE liveness, button handling, and the dry-fire technique —
no water, sensors in open air.

1. **IDLE liveness:** confirm the LED is **off** and the 1 Hz sensor line is
   streaming:

   ```
   S1=3487 S2=3512
   ```

   Values in open air should sit near the top of the 12-bit range (see Stage
   3). If a channel reads a hard 0 or a hard 4095 (or the line is missing a
   sensor), stop and check wiring.

2. **Button reads:** press and release BOOT quickly, then hold one for ~1.5 s
   and release:

   ```
   BTN short 350ms
   BTN long 1500ms
   ```

   Lines print **on release** (spec §5.1). A long press in IDLE correctly does
   nothing.

3. **Dry-fire the valve (<5 s per open — rule 3):** short-press BOOT to start
   a fill, then short-press again within a few seconds to abort. Two presses
   = one safe click:

   ```
   FILL start backstop=30min recovery=0ms recmin=0
   BASE ok s1=3492 s2=3501
   VALVE open budget=30min
   VALVE close
   ABORT attemptms=4000 openms=1900
   ```

   The valve audibly clicks on `VALVE open` and clicks off on `VALVE close`.
   Keep every open **<5 s**. Repeat two or three clicks.

**Understand these first-fill lines:** `recovery=0ms recmin=0` means **fresh
boot** — no tank history exists, so the backstop table was bypassed and
`FRESH_BOOT_BACKSTOP_MIN` (30 min shipped) was granted. A real button-driven
fill can never show `recovery=0ms` (button debounce is 40 ms); only a fresh
boot/reset does. `ABORT attemptms` = press→close, `openms` = valve-open→close
(0 would mean the valve never opened).

**LED cues:** FILLING = solid; back to off after the abort. A fill that ends
by itself would show the triple-pulse (you'll meet it properly in Stage 4).

**Record:** valve click heard on open/close: `Y / N` — dry-fire opens all
<5 s: `Y / N`

---

## Stage 2 — Power rails (spec §9.2)

**Goal:** prove the buck delivers a safe 5 V before the board ever runs from
it. **This stage cannot be watched on the log stream** — with the buck
powering the board, the USB cable is unplugged (rule 1), so no serial monitor
exists. The bench record here is your multimeter reading, written into the
sign-off table at the end of this document.

1. Bench layout: USB **unplugged** from the board; buck output wired to the
   board's 5 V pin (and GND); buck input from 12 V.
2. Before first power-up, power the buck alone (board's 5 V wire still
   disconnected is safest) and measure the buck output: **4.9–5.1 V** required.
   If it's out of range, adjust the LM2596 trimmer and re-measure.
3. Connect the buck to the board's 5 V pin, power up, and re-measure **at the
   board's 5 V pin**: still **4.9–5.1 V**. Confirm the boot banner behavior
   can't be checked here — no USB — so verify by behavior: the LED contract
   stays in IDLE (off) and a BOOT press produces a valve click (keep dry-fire
   <5 s, rule 3).
4. 12 V may separately feed the MOSFET module's power-input block for valve
   tests while the board runs on USB — safe, as long as the buck's 5 V output
   is not connected to the board (rule 1).

**Exclusivity checklist (tick each):**

- [ ] Buck preset reading (board disconnected): `____ V`
- [ ] Buck reading at the board's 5 V pin (buck-powered): `____ V`
- [ ] USB and buck 5 V were never connected simultaneously
- [ ] Powered-down and reverted to USB power for Stages 3–4

---

## Stage 3 — Sensor calibration (spec §9.3)

**Goal:** replace the provisional sensor constants with your rig's measured
values. USB power only (buck 5 V disconnected). Sensors: open air first, then
submerged to their real mounting depth.

### 3a. Dry capture

1. Sensors hanging in open air, away from metal and moisture.
2. Watch the 1 Hz line for ~30 s and note the range each sensor wanders:

   ```
   S1=3487 S2=3512
   ```

   **Expect roughly 3000–3900 counts** (12-bit ADC, 3.3 V rail ≈ 4095 full
   scale). A dry reading outside ~2000–4050 — the shipped
   `BASELINE_MIN`/`BASELINE_MAX` window — means a wiring or sensor problem;
   fix hardware before editing constants.

3. Cross-check with a baseline capture: short-press BOOT, and about 2 s later
   (baseline settle, valve still closed):

   ```
   BASE ok s1=3492 s2=3501
   VALVE open budget=30min
   ```

   then short-press again to abort (<5 s — the fill has begun). If instead you
   see

   ```
   FAULT baseline sensor=1 code=1 count=2170
   ```

   the `count=` field is the failing sensor's **observed dry value** — exactly
   the datum you need. (Fault latches: LED blinks code 1 or 2; press RESET to
   clear, fix wiring, retry.)

### 3b. Wet capture

1. Submerge both sensor tips to the depth they'll sit at in service.
2. Read the wet values off the same 1 Hz `S1=/S2=` line — it runs in every
   state, no fill needed. Wet readings drop hard (HW-390 reads lower when
   wet).

### 3c. Record and edit

| Quantity | Sensor 1 (GP26) | Sensor 2 (GP27) |
| --- | --- | --- |
| Dry min (30 s) | ____ | ____ |
| Dry max (30 s) | ____ | ____ |
| Wet at mount depth | ____ | ____ |
| Dry→wet delta (dry − wet) | ____ | ____ |

Open `firmware/jug_filler/config.h`, group `5. ADC / sensors`, and set:

- `BASELINE_MIN` = a little **under** your observed dry minimum
- `BASELINE_MAX` = a little **over** your observed dry maximum
- `TRIP_DROP_COUNTS` = **≈ half** the dry→wet delta (the wet threshold is
  baseline − this; half the delta keeps it comfortably between dry and wet)

> **Worked example (illustrative numbers, not bench data):** dry ≈ 3500,
> wet ≈ 900 → delta ≈ 2600 → `TRIP_DROP_COUNTS` ≈ 1300; with ±~400 breathing
> room around the dry range:

```cpp
// ---- 5. ADC / sensors (provisional; set during commissioning — PRD §9) ----
constexpr uint16_t BASELINE_MIN     = 3100;   // counts — just under observed dry min (was 2000)
constexpr uint16_t BASELINE_MAX     = 3900;   // counts — just over observed dry max (was 4050)
constexpr uint16_t TRIP_DROP_COUNTS = 1300;   // counts — ≈ half of dry 3500 − wet 900 (was 350)
```

### 3d. Verify

Reflash (USB power only — rule 1), short-press BOOT to start a fill, and
confirm:

```
BASE ok s1=3492 s2=3501
```

with **no** `FAULT baseline` line. Abort the fill (<5 s). Done when both
sensors pass the plausibility gate.

**Other group 5 constants — leave alone unless symptoms:**
`ADC_BITS` (12) is contractual — never change it. `ADC_SAMPLES_PER_READING`
(32), `READING_INTERVAL_MS` (100) and `BASELINE_SETTLE_MS` (2000) shape the
averaging/settling — defaults are fine. `DEBOUNCE_COUNT` (8 consecutive wet
readings to accept a trip) is the only one you might touch later: raise it a
little only if fills trip spuriously from noisy water; the cost is slightly
slower trip detection.

**Record:** your three constants in the sign-off table at the end.

---

## Stage 4 — Dry-run flows (spec §9.4)

**Goal:** rehearse both normative interaction flows (spec §5.2) end-to-end
with sensors dipped in a cup **by hand**. USB power only. Each sub-step names
the exact log sequence and the LED shape.

### Flow 1 — abort and fresh timer

1. Short-press BOOT → fill starts:

   ```
   FILL start backstop=30min recovery=0ms recmin=0
   BASE ok s1=3492 s2=3501
   VALVE open budget=30min
   ```

   LED: solid.

2. Short-press BOOT mid-fill → fill stops:

   ```
   VALVE close
   ABORT attemptms=12000 openms=10000
   ```

   LED: off. (Keep the valve-open time short at the bench — you're allowed to
   abort within 5 s here.)

3. Short-press BOOT again → the next fill starts with a **fresh, recomputed
   timer**:

   ```
   FILL start backstop=44min recovery=8200ms recmin=0
   ```

   The recovery clock restarted at the abort instant (an abort counts as a
   fill end, spec §6.1), so `recovery` is small — but this is **not** a fresh
   boot: no `recovery=0ms`, and the budget (44 min with the shipped table at
   ≤2 min recovery) came from the **backstop table**, not
   `FRESH_BOOT_BACKSTOP_MIN`.

### Flow 2 — successful fill

1. With the previous fill aborted (or after a RESET), short-press BOOT:

   ```
   FILL start backstop=44min recovery=8200ms recmin=0
   BASE ok s1=3492 s2=3501
   VALVE open budget=44min
   ```

2. After at least 60 s of valve-open time (`TRIP_MIN_ELAPSED_S` — a trip
   before that faults as "too early", see below), dip **sensor 1 only** into
   the cup:

   ```
   VALVE close
   TRIP sensor=1 fillms=75000
   ```

   The valve **closes first**, then the trip logs; the fill is now in CONFIRM
   (LED still solid). The second sensor must trip within
   `CONFIRM_WINDOW_S` = **12 s** — have the cup ready.

3. Dip **sensor 2** within the 12 s window:

   ```
   VALVE close
   CONFIRM pass sensor=2 durms=3000
   DONE attemptms=80000 openms=75000
   ```

   LED: **triple-pulse** (3× on 200 ms / off 200 ms, then 1000 ms pause,
   repeating). The second `VALVE close` is normal — every fill-ending step
   prints it first, even though the valve already closed at the trip.

4. Short-press BOOT to acknowledge:

   ```
   ACK done
   ```

   LED: off. (`attemptms` = press→valve-close, includes the ~2 s baseline
   settle; `openms` = valve-open→close — the number Stage 5 records.)

### Guard faults — timing mistakes, not broken hardware

All three below mean **your dip timing** was wrong. The valve is already
closed, the LED blinks the code group, the fault latches until RESET:

| You did this | Log line | Meaning |
| --- | --- | --- |
| Dipped a sensor <5 s after valve open (`EARLY_WET_WINDOW_S`) | `FAULT earlywet sensor=2 code=2 fillms=1200` | wet reading inside the early window |
| Dipped before 60 s of valve-open time (`TRIP_MIN_ELAPSED_S`) | `FAULT earlytrip sensor=1 code=1 fillms=45000` | trip too early to be a real fill |
| Didn't dip sensor 2 within 12 s | `FAULT confirm sensor=2 code=2 durms=12000` | confirm window expired |

Recover: press RESET (or power-cycle) → board reboots to IDLE, valve closed,
history cleared. Then retry the flow.

**Record:** Flow 1 pass `Y / N`; Flow 2 pass `Y / N` (both with LED shapes as
described).

---

## Stage 5 — Attended first fills & backstop tuning (spec §9.5)

**Goal:** measure real fill durations across recovery times and tune
`BACKSTOP_TABLE_MIN` so the backstop *hugs* real fill time. Keep the rig in
service wiring (buck-powered, USB unplugged) or stay on USB with the buck
disconnected — just never both (rule 1). Note the log stream only exists in
USB-powered sessions, so do the data-gathering fills on USB.

### 5a. Record per fill

For each attended fill, write down two numbers from the stream:

- from `FILL start backstop=44min recovery=8200ms recmin=2` — the
  **recovery** (`recmin` = whole minutes since the previous fill *end*,
  truncated: 599999 ms → 0; `recovery=0ms` only ever means fresh boot);
- from `DONE attemptms=80000 openms=75000` — the actual valve-open duration
  (`openms`).

Sample deliberately different recovery times: one fill **immediately** after
the previous one ends, then ~10 min, ~20 min, and ≥60 min later. Tip: an
abort counts as a fill end, so a quick abort-and-refill is a cheap way to
sample near-zero recovery. Don't use RESET to skip waiting — it clears
history and the next fill logs the fresh-boot default instead.

| Fill | `recmin` | `openms` | minutes | margin applied | knot `{recoveryMin, backstopMin}` |
| --- | --- | --- | --- | --- | --- |
| immediate | ____ | ____ | ____ | ____ | ____ |
| ~10 min | ____ | ____ | ____ | ____ | ____ |
| ~20 min | ____ | ____ | ____ | ____ | ____ |
| ~35 min | ____ | ____ | ____ | ____ | ____ |
| ≥60 min | ____ | ____ | ____ | ____ | ____ |

### 5b. Choose the knots

For each recovery time: **backstop = observed duration × safety margin** —
suggest 1.3–1.5×, **rounded up** — then **preserve the concave shape**: a
steep fall at short recovery that flattens toward the full-tank value at long
recovery.

**Never pad the mid-range** (spec §6.2). The tank refills fast at first and
progressively slower, so the real-duration curve is concave — a straight line
(or any padding) gives a fill started ~15 min after the last one ended *more*
time than physics allows. The backstop is the dead-sensor safety net: padded
time is exactly the time a stuck-open valve keeps running with no sensor to
stop it.

> **Worked example (illustrative numbers, not bench data)** — measured
> durations 29/26/24/22/20 min at 0/10/20/35/60 min recovery, margins ~1.5× at
> short recovery easing to ~1.3× at long recovery (bigger margin at short
> recovery also keeps the curve concave):

| `recmin` | measured min | × margin | knot |
| --- | --- | --- | --- |
| 0 | 29 | 1.5 → 43.5 → round up | `{2, 44}` |
| 10 | 26 | 1.4 → 36.4 | `{10, 37}` |
| 20 | 24 | 1.35 → 32.4 | `{20, 33}` |
| 35 | 22 | 1.3 → 28.6 | `{35, 29}` |
| 60 | 20 | 1.3 → 26 | `{60, 26}` |

Falls per minute: 7/8, 4/10, 4/15, 3/25 — each **flatter** than the last:
concave. ✓

### 5c. Edit `config.h`, group `3. Fill backstop model`

The table is C++ array-of-struct initializers — `{recoveryMin, backstopMin}`
pairs. Rules: `recoveryMin` values **ascending**; the `backstopMin` column
falling steeply-to-shallow (concave); the firmware clamps at both ends (below
the first knot uses the first backstop, above the last uses the last).

The file ships with these defaults (the SHAPE example):

```cpp
constexpr BackstopTableEntry BACKSTOP_TABLE_MIN[] = {
    {2, 44}, {10, 41}, {20, 38}, {35, 34}, {60, 30},
};
```

Replace that one definition with your measured knots (here the worked example
from the table above — use **your** numbers):

```cpp
constexpr BackstopTableEntry BACKSTOP_TABLE_MIN[] = {
    {2, 44}, {10, 37}, {20, 33}, {35, 29}, {60, 26},
};
```

Two related constants, same group:

- `FRESH_BOOT_BACKSTOP_MIN` — the fresh-boot / fresh-reset default, granted
  whenever `recovery=0ms` (table bypassed). It should match your full-tank
  (long-recovery) reality — the shipped 30 assumes a full tank fills in well
  under 30 min. If your ≥60 min knot differs a lot from 30, set this to your
  full-tank duration × the same margin.
- `PLUS5_MIN` (5) — a long press during FILLING extends the active fill's
  deadline, stacking. It shows as `PLUS5 budget=35min`, and a backstop expiry
  reports the *stacked* budget: `FAULT backstop code=3 budget=35min
  openms=1800000ms`.

### 5d. Verify

Reflash, run one more attended fill, and confirm it completes normally:

```
FILL start backstop=44min recovery=8200ms recmin=0
...
DONE attemptms=80000 openms=75000
```

with the `backstop=`/`budget=` values matching your new table for that
recovery. That's commissioning complete.

---

## Completion sign-off

| # | Item | Value | Source |
| --- | --- | --- | --- |
| 1 | Boot banner seen | `Y / N` | Stage 0 |
| 2 | Dry-fire clicks <5 s, valve clicks heard | `Y / N` | Stage 1 |
| 3 | Buck preset (board disconnected) | `____ V` | Stage 2 |
| 4 | Buck at board 5 V pin (buck-powered) | `____ V` | Stage 2 |
| 5 | Dry counts min–max, sensor 1 / sensor 2 | `____ / ____` | Stage 3a |
| 6 | Wet counts, sensor 1 / sensor 2 | `____ / ____` | Stage 3b |
| 7 | `BASELINE_MIN` / `BASELINE_MAX` set | `____ / ____` | Stage 3c |
| 8 | `TRIP_DROP_COUNTS` set | `____` | Stage 3c |
| 9 | `BASE ok` after reflash, no `FAULT baseline` | `Y / N` | Stage 3d |
| 10 | Flow 1 (abort + fresh timer) pass | `Y / N` | Stage 4 |
| 11 | Flow 2 (success) pass | `Y / N` | Stage 4 |
| 12 | First-fill record table complete | `Y / N` | Stage 5a |
| 13 | `BACKSTOP_TABLE_MIN` set (concave) | `{…}` | Stage 5c |
| 14 | Post-tuning attended fill reaches `DONE` | `Y / N` | Stage 5d |

**Closing note:** every commissioned value lives in one place —
`firmware/jug_filler/config.h` (spec §8, single source of truth). Commit the
file with your measured values so the rig's calibration survives the next
flash. Nothing else in the repo needs to change.
