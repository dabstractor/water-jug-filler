# Hardware Validation — component claims vs sources

> Method note: produced from an async research agent's salvaged run log (the agent
> was killed by an infrastructure failure before it could write this file) plus the
> architect's cross-checks. Items marked UNVERIFIED must be measured at
> commissioning (PRD §9) — they do not block firmware work.

## RP2040-Zero board — VERIFIED
- **5V pin (VSYS) ties directly to USB VBUS with NO protective diode** — verified
  from Waveshare's own FAQ: *"The VSYS pin of the RP2040 is connected to the VUSB
  pin directly in RP2040-Zero… you need to add a diode to avoid backflow."*
  ⇒ PRD §2.2's rule is CORRECT and binding: **never connect USB while the buck
  powers the board** (PC port backfeed risk). Bench = USB with buck disconnected;
  service = buck with USB unplugged.
- Exposed pins GP0–GP15 + GP26–GP29 (+3V3/5V/GND); GP16–GP25 only as microscopic
  back pads (treat as unusable); onboard WS2812 on GP16; onboard BOOT + RESET.
  Consistent with the Waveshare wiki pinout. Board is powered via 5V pin → onboard
  LDO → 3V3 (no battery-management circuitry).
- Source: https://www.waveshare.com/wiki/RP2040-Zero (wiki + FAQ).

## Level sensors (2x HW-390 v2.0) — VERIFIED class, calibrate values
- HW-390 is the common **capacitive** soil-moisture sensor v2.0 class board
  (marking HW-390): supply ~3.3–5.5 V, **analog AOUT falls when wet**, no exposed
  electrodes on the sensing face (corrosion-resistant; suitable for permanently
  wet tips), 1.5-ish–3 V output swing over dry→wet depending on supply and
  immersion depth.
- At 3.3 V supply the PRD's expectation (dry ≈ 3000–3900 of 4095 counts; wet drops
  by several hundred counts or more) is plausible but **UNVERIFIED for the
  specific pair in hand** — §9.3 calibration exists exactly for this: log dry
  baselines and wet values, then set `BASELINE_MIN/MAX` and
  `TRIP_DROP_COUNTS ≈ ½·(dry−wet)`.
- Sources: vendor pages/tutorials for "HW-390 capacitive soil moisture sensor v2.0"
  (e.g. SinricPro HW-390 tutorial); no single authoritative datasheet exists.

## Valve & MOSFET driver — VERIFIED class
- **FPD180A-class 12 V DC solenoid, normally closed** (opens when energized;
  power loss = closed — matches PRD's fail-safe intent). Coil ~400 mA is typical
  for this valve class. **UNVERIFIED (specific part):** exact coil current and
  flow rate — confirm the part marking on the bench; 12 V 1 A supply leaves ample
  headroom for ~0.4 A coil + buck quiescent draw.
- **Flyback:** PRD's 1N4148 across the coil (stripe to +12 V) is the classic
  pattern, but a 1N4148 (200 mA avg, 500 mA peak surge) is marginal for a 400 mA
  coil's flyback current spike; the PRD's "second optional, in parallel" note
  helps. **Recommendation (bench-level, not firmware): prefer a 1N4007 or
  Schottky (e.g. 1N5819) if available**; firmware work is unaffected either way.
  Record the final choice in the bench notes.
- **Dry-firing:** shorting the valve with no water flow heats the coil — PRD §9.1's
  "<5 s clicks" rule is correct; keep it.
- **MOSFET module:** IRF520-based "blue modules" are NOT reliably enhanced at a
  3.3 V gate (standard-gate MOSFET, Vgs(th) up to ~4 V); **IRLZ44N/IRL520-class
  logic-level parts (Vgs(th) ~1–2 V) are the right class** — PRD §2.2 is correct
  and its qualification test (module indicator LED lights, valve pulls in crisply,
  FET stays cool at TRIG=3.3 V with the real solenoid) is the acceptance gate.
  Typical 2-terminal-block module: power-input block + switched-output block +
  TRIG/PWM & GND header, with an onboard gate pull-down keeping the output OFF
  when TRIG floats (covers boot/BOOTSEL/flashing/brownout — PRD §2.2 claim sound).
  **Verify input vs output blocks with a multimeter before connecting the
  solenoid** (PRD §2.2 instruction — keep it in the commissioning runbook).
- Sources: electronics.stackexchange IRLZ44N 3.3 V gate-drive threads; module
  vendor listings; PRD §2.2 internal consistency.

## Power path — VERIFIED
- **LM2596 buck:** adjustable, 12 V→5 V trivially set, 2–3 A rating, fine at the
  light load here (board LDO + 2 sensors + capped WS2812 ≪ 1 A). Accuracy under
  light load is adequate for an LDO front-end, but **preset to 4.9–5.1 V with a
  multimeter BEFORE first connection** (PRD §2.2 rule kept).
- Board 3V3 LDO budget: RP2040 (~30–50 mA active) + 2 HW-390s (few mA each) +
  WS2812 capped at 60/255 (≈5 mA/channel worst case) — comfortably within the
  onboard LDO. **WS2812 brightness cap 60/255 is enforced in firmware** (PRD §2.2).
- 100 µF electrolytic on the 5V rail + 100 nF ceramic per AOUT: standard, sensible
  decoupling for cleaner ADC readings; keep analog wiring away from solenoid runs
  (PRD §2.2 routing note kept).

## Electrical limits & signal integrity — VERIFIED (RP2040 datasheet)
- GPIO/ADC are **3.3 V-only, NOT 5 V tolerant** — sensors MUST be fed from 3V3
  (PRD §2.1 correct; never 5 V).
- Only GP26–GP29 expose the ADC (ADC0–3) — GP26/GP27 for the two sensors is the
  only valid choice among exposed pins. Correct per PRD.
- Default GPIO drive 4–8 mA is ample for a MOSFET-module TRIG input (high-impedance
  gate + module circuitry) and for the external LED via 330 Ω (≈7 mA at 3.3 V).
- During boot/flash/brownout GPIOs are Hi-Z — see external_deps.md item 8 for the
  safety chain (module pull-down + "valve LOW first line of setup()").

## Corrections/risks for the PRD
1. **§2.1 BOOTSEL pseudo-pin prescription is wrong for arduino-pico** — see
   external_deps.md item 1 (use the `BOOTSEL` bool object). Firmware binding
   correction, already reflected in task P1.M2.T2.S1.
2. **§8 ADC constants assume 12-bit counts but arduino-pico defaults to 10-bit** —
   `analogReadResolution(12)` is mandatory. Reflected in P1.M1.T1.S1/P1.M3.T1.S2.
3. Flyback: prefer 1N4007/Schottky over a single 1N4148 for a ~400 mA coil
   (bench-level note; PRD's parallel-second-diode option acceptable).
4. Watchdog ceiling is 8388 ms — PRD's 8 s is legal; never raise (external_deps.md).
5. No PRD contradictions found in the board pinout, power path, or sensor
   approach; the PRD is feasible as specified with corrections 1–2.

## Sources
- https://www.waveshare.com/wiki/RP2040-Zero (+ official FAQ answers on VSYS/VUSB)
- RP2040 datasheet (electrical characteristics, GPIO/ADC)
- electronics.stackexchange.com IRLZ44N @3.3 V gate drive discussions
- Vendor listings: LM2596 modules, HW-390 sensors, FPD180A valves, MOSFET modules
