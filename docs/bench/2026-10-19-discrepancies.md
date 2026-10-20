# Bench discrepancies — 2026-10-19 (expected vs observed)

Consolidated discrepancy ledger for the BUG-003 bench evidence package. One ledger
per bench day, shared by all sessions of that day. This file was created at
consolidation time (P1.M2.T2.S5, 2026-10-19) covering bench sessions
P1.M2.T2.S1–S4: no bench session ran (all BLOCKED — rig unavailable, see the
docs/bench/README.md §2 index), so there are no captured-log mismatches to record;
the single entry below is documentation-scope and is recorded from static sources.

Rules (binding — docs/bench/README.md §1):

- Quote log lines VERBATIM in fenced blocks — copy from the capture; never retype or trim.
- NEVER edit a captured `.log`; corrections and follow-ups live here.
- Each entry carries a follow-up issue number once filed (or `pending`).

## Entries

### D-1 — LED indicator documentation nuance → proposed BUG-004 (recorded as observed; bench confirmation outstanding)

- Session: — (documentation-scope; originally seeded for Session A)
- Expected (SPEC §4, quoted): the onboard WS2812 on GP16 is the sole status/fault
  indicator; GP14 is listed among the unwired spares.

  ```
  spec/SPEC.md:9: | Waveshare RP2040-Zero (RP2040, 2MB flash, 3.3 V logic) | Controller. Onboard BOOT + RESET buttons (both used). Onboard WS2812 RGB LED on GP16 — the sole status/fault indicator (§4). Exposed pins: GP0–GP15 and GP26–GP29. **GP16–GP25 exist only as microscopic back pads — treat as unusable.** |
  spec/SPEC.md:31: | GP16 (onboard, no wiring) | WS2812 RGB LED — sole status/fault indicator | Driven in firmware via a NeoPixel library (e.g. Adafruit_NeoPixel, supported on arduino-pico) |
  spec/SPEC.md:33: Spares: GP28/GP29 (ADC2/ADC3), GP14, and GP2–GP13, all unwired. Reserved: GP0/GP1 (UART0 TX/RX) for debug serial. Unusable: GP16–GP25 (back pads).
  ```

- Observed (docs + firmware, quoted): the firmware drives BOTH indicators — the GP14
  external green LED (`PIN_LED = 14`) and the config-gated WS2812 mirror on GP16
  (`USE_ONBOARD_RGB = true`) — and docs/COMMISSIONING.md documents both, while SPEC §4
  calls the WS2812 the sole indicator. No serial capture exists (all sessions BLOCKED
  — rig unavailable); which indicators physically render each pattern was NOT
  bench-confirmed.

  ```
  docs/COMMISSIONING.md:20: diode); green LED on GP14 via 330 Ω; LM2596 buck converter from the 12 V
  docs/COMMISSIONING.md:96: **LED cues:** the firmware drives two indicators that always show the same
  docs/COMMISSIONING.md:97: **pattern shape** — the onboard WS2812 mirror (adds color) and the external
  docs/COMMISSIONING.md:98: LED on GP14 (on/off). The shapes you will see: IDLE = off; FILLING and CONFIRM
  docs/COMMISSIONING.md:127: external LED still shows every pattern.
  firmware/jug_filler/config.h:25: constexpr uint8_t PIN_LED     = 14;   // GPIO# — GP14 → external green LED anode via 330 Ω (PRD §2.1)
  firmware/jug_filler/config.h:69: constexpr bool     USE_ONBOARD_RGB   = true;   // flag — mirror patterns on onboard WS2812 (GP16, no wiring) (PRD §4)
  ```

- Action: documentation reconciliation only — file a follow-up issue; do NOT edit
  spec/SPEC.md in this changeset (bench evidence package §6; Mode A).
- Follow-up issue: BUG-004 — LED indicator documentation (SPEC §4 sole-indicator
  claim vs GP14 external LED + WS2812 mirror)
- Resolution: spec/SPEC.md reconciled to the implemented dual-indicator firmware
  (BUG-004): §1 BOM names the GP14 green LED + 330 Ω as the authoritative
  indicator with the GP16 WS2812 as color mirror; §2.1 pins GP14 and drops it
  from the spares; §2.3 netlist + numbered connections add the GP14 wiring;
  §4 states the dual-indicator contract (GP14 authoritative, WS2812 config-gated
  mirror, mirror init failure fails safe) with `RGB_BRIGHTNESS = 60`;
  §8 config contract gains `PIN_LED`/`USE_ONBOARD_RGB`/`RGB_BRIGHTNESS`;
  §9 dry-bench check covers both indicators. Physical bench confirmation of
  which indicators render each pattern remains outstanding with sessions A–E
  (all BLOCKED — rig unavailable; captured logs still required per §9 of the
  PRD). The quoted SPEC lines above are kept verbatim as the historical record
  of the pre-reconciliation state.
- Status: closed (documentation reconciled; physical bench confirmation outstanding)
