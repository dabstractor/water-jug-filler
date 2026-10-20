# Stage-2 power-rail measurement — rails sheet (SPEC §9.2, §2.2)

Copy this template to `YYYY-MM-DD_rails.md` and fill it at the bench. This check is
BUCK-ONLY with USB UNPLUGGED — no serial stream exists here; the multimeter readings
below ARE the evidence (not recorded in a `.log`).

## Record

- Date: —  Operator: ____
- Meter: ____  Buck supply: ____
- Deviations: none / ____
- Result: PENDING — PASS when both readings are 4.9–5.1 V and all exclusivity ticks hold.

## Source exclusivity (tick before measuring)

> Never USB and buck simultaneously: the RP2040-Zero VSYS pin ties directly to USB VBUS
> with no protective diode (docs/bench/README.md §3 rule 1; bench evidence package §1).

- [ ] USB cable fully unplugged from the board for the whole check.
- [ ] Buck 5 V output disconnected from the board while presetting (measurement 1).
- [ ] At no point were USB and the buck's 5 V connected to the board at the same time.

## Measurements (SPEC §9.2; COMMISSIONING Stage 2)

1. Buck preset, board DISCONNECTED:
   - Reading: `____ V` — within 4.9–5.1 V? [ ] yes [ ] no
2. Buck at the board 5 V pin (buck powering the board, USB unplugged):
   - Reading: `____ V` — within 4.9–5.1 V? [ ] yes [ ] no

## After the measurement

- [ ] Restore bench power for the following sessions (USB with buck 5 V disconnected).
- [ ] Reference this sheet from the Session A record; summarize in README index row A.
- Mismatches → `YYYY-MM-DD-discrepancies.md`.
