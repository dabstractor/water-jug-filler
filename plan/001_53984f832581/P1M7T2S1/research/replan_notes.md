# Re-plan research delta — P1.M7.T2.S1 (attempt 2)

Supplements `notes.md` (attempt 1 research, still valid — log shapes, timings,
power rules, UF2 path, monitor/filter URLs all re-verified this session).

## Issue post-mortem (attempt 1)

BLOCKED was CORRECT behavior, not a defect: no hardware on the machine, and the
PRP forbids fabricating evidence. Two real costs to fix in attempt 2:

1. **Ordering**: attempt 1 ran `pio test -e native` (192 tests) + build BEFORE
   probing for hardware → ~10 min wasted to reach the same stop. Fix: hardware
   probe is Task 0 and comes FIRST; software gates only run once the bench path
   is confirmed.
2. **Operator protocol**: attempt 1's PRP implied an operator but never defined
   the handshake. Fix: explicit operator-assisted protocol — agent = recorder/
   verifier (serial capture, grep assertions, evidence files), operator = hands
   (BOOTSEL/RESET/BOT presses, dips, multimeter, wiring). One OP prompt at a
   time; each step verified against captured logs before proceeding.

## New facts verified in source this session

- `BTN short/long <ms>ms` is logged by the CLASSIFIER on every accepted release
  (jug_filler.ino ~line 1604) regardless of state ⇒ while FAULTed a BOOT press
  STILL logs `BTN short …ms` but must produce NO `FILL start`/`VALVE open`.
  Fault-ignore assertion = "BTN line present, fill lines absent".
- No `BENCH_HANG_TEST` (or any hang/debug) define exists in the sketch yet —
  the watchdog test block must be added temporarily and reverted; use
  `__asm volatile ("nop")` inside the busy loop so -O2 cannot elide it, and
  keep `watchdog_update();` as loop()'s literal first line (structural check).
- `PLATFORMIO_BUILD_FLAGS="-DBENCH_HANG_TEST" pio run -e pico` OVERWRITES
  `.pio/build/pico/firmware.uf2` ⇒ stash the stock UF2 in /tmp BEFORE the hang
  build; flash stock back after, then confirm the reverted tree still builds.
- **Pre-existing doc discrepancy (file, don't fix)**: SPEC.md §1/§4 says the
  onboard WS2812 (GP16) is the SOLE LED, no external LED, GP14 spare — but
  PRD §2.1/§4 + `config.h` (`PIN_LED=14`, `USE_ONBOARD_RGB=true` mirror) +
  the built rig use GP14 external green LED as authoritative. Bench observes
  BOTH indicators; discrepancy goes in DISCREPANCIES.md.
- Loop wiring (end of .ino): valve pin written EVERY pass from engine output;
  LED stage = GP14 digitalWrite + wsMirrorUpdate with the same tick level ⇒
  external LED and WS2812 are level-locked; any divergence is a real fault.
- README.md "Serial logging" table is normative and matches logFillEvent()
  exactly (incl. `RGB mirror init FAILED (no PIO SM)` degradation line).
- Buck-power test (§9.2) is LED-only evidence: USB unplugged while buck powers
  the board means serial is dropped by design — observe LED states only.

## Attempt-3 delta (post-mortem of attempt 2)

Attempt 2 blocked identically: no hardware on the dispatch machine (triple-probed:
no ttyACM/USB, no RPI-RP2, no lsusb 2e8a). Two defects in MY attempt-2 PRP:

1. BLOCKED-HW exited immediately — it never ASKED the operator to attach the rig,
   though attempt 2's own resolution text says "have the operator plug the
   RP2040-Zero in and re-invoke". Attempt-2 session was interactive-capable
   (remote-pi, 19 messages restored) ⇒ an in-session ATTACH-REQUEST could have
   recruited the hardware. Fix: Task 0 = recruitment gate (ask → wait → re-probe;
   max 2 asks; only then blocked exit).
2. Attempt 2 STILL ran the full software gates ("fresh verification") before the
   blocked exit, despite the PRP saying to skip them. Fix: absolute rule — Task 0
   probe is the FIRST tool-call batch; any pio test/run before mode determination
   is a PRP violation (gates add zero information to a blocked exit; attempt-1/2
   evidence already on record).

Additions: rig-tier table (what each hardware subset unlocks: board-only → T3
smoke + T8; +sensors/cup → T5–T7; +buck/multimeter → T2; +12 V/module → T4),
partial-evidence banking with PARTIAL marking + resume-from-banked-tests rule,
and the invariant that a partial run NEVER reports result complete. Not a "fail":
the dependency is addable (plug the rig in); requirements are not contradictory.

## Attempt-4 verdict (final re-plan)

Attempt 3 blocked identically AND proved the environment facts: (1) executing
sessions are non-interactive one-shot pipeline runs (no ATTACH-REQUEST possible,
no operator reply channel), (2) supervisor escalation unavailable, (3) three
dispatches → three identical results ⇒ re-queueing cannot converge (the issue's
own RE-DISPATCH LOOP note). Conclusion: no PRP can add the missing dependency
(physical rig + hands on the executing host); only orchestrator/human action can.

Final PRP contract change: the no-hardware/no-channel path now returns
result "fail" (blocked-pending-hardware) instead of "issue" — implementing the
issue's option (c) and ending the retry loop. The complete T2–T9 runbook is
preserved verbatim-quality for (a) a bench-host dispatch or (b) a human operator
running the bench session manually from this PRP (it doubles as the operator
manual; evidence still banks under docs/bench/ with the same structure).
All software-side state verified unchanged across attempts: 192/192 native,
structural check PASS, stock UF2 built, firmware tree clean.
