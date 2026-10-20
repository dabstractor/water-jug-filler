# Research notes — P1.M2.T2.S2 (Sessions B & C evidence capture)

All facts below were read directly from the repo this session (no guessing).

## Sources consulted

1. `plan/.../bugfix/001_db6d94af3d0d/P1M2T2S1/PRP.md` — S1 contract (what exists when this
   task starts): rails sheet + Session-A log + row A finalized PASS is the entry gate;
   S1 already created `YYYY-MM-DD_rails.md`, `YYYY-MM-DD_session-A-dry-bench.log`,
   filled `template-session-A-dry-bench.md`, possibly `YYYY-MM-DD-discrepancies.md`
   (with pre-seeded D-1 LED-indicator entry).
2. `docs/bench/README.md` — index (rows B/C PENDING today), §1 integrity rule, §2 status
   vocabulary (PENDING/PASS/PARTIAL(n✗)/BLOCKED:reason/SKIPPED:reason), §3 capture
   procedure (USB-only binding rule, 115200 8N1, capture BEFORE power/reset, operator
   header block, no .log edits ever), §4 naming conventions.
3. `docs/bench/template-session-B-flow1-abort.md` — checklist to fill IN PLACE; expected
   lines; critical assertions (restart NOT recovery=0ms; restart ≤2 min after abort →
   backstop=44min; wait-row table 2→44, 10→41, 20→38, 35→34, ≥60→30); verification
   grep hints.
4. `docs/bench/template-session-C-flow2-success.md` — checklist to fill IN PLACE; ≥60 s
   open before first dip (TRIP_MIN_ELAPSED_S); 12 s confirm window; DONE green
   triple-pulse; ACK done; post-ACK press starts new fill; retake-on-fault guidance.
5. `docs/bench/template-discrepancies.md` — copy once per bench day, append entries;
   pre-seeded D-1 (WS2812 GP16 vs external GP14 green LED nuance) must be confirmed at
   the bench, not fixed.
6. `plan/.../architecture/bench_evidence_package.md` §1 (5 binding power rules), §2
   Session B/C ordered prefixes (byte-exact), §4 executability split (captures are
   physical, gates are software).
7. `spec/SPEC.md` §6.1 (recovery clock: reference = previous fill's valve close; abort
   restarts the clock; fresh boot/reset → no history → FRESH_BOOT_BACKSTOP_MIN=30),
   §6.2 (BACKSTOP_TABLE_MIN piecewise-linear, clamped at both ends).
8. `firmware/jug_filler/jug_filler.ino` — exact format strings at lines ~1353–1395:
   `VALVE close`, `FILL start backstop=%umin recovery=%lums recmin=%u`,
   `BASE ok s1=%u s2=%u`, `VALVE open budget=%umin`, `TRIP sensor=%u fillms=%lu`,
   `CONFIRM pass sensor=%u durms=%lu`, `DONE attemptms=%lu openms=%lu`,
   `ABORT attemptms=%lu openms=%lu`, `FAULT ...`, `ACK done`. Sensors averaged into one
   reading pair per READING_INTERVAL_MS = 100 ms (line ~166/1450).
9. `firmware/jug_filler/config.h` — BUTTON_LONG_PRESS_MS=1000, BUTTON_DEBOUNCE_MS=40,
   FRESH_BOOT_BACKSTOP_MIN=30, BACKSTOP_TABLE_MIN entries, CONFIRM_WINDOW_S=12,
   EARLY_WET_WINDOW_S=5, TRIP_MIN_ELAPSED_S=60, DEBOUNCE_COUNT=8 (consecutive wet
   readings to accept a trip), DONE_PULSE_ON_MS=200 / OFF=200 / COUNT=3 (+1000 ms pause
   per jug_filler.ino:725).

## Key derived facts for the runbook

- recmin = recovery_ms / 60000 integer-truncated (BUG-001 lesson: 8200 ms → 0). With the
  template's ≈1-min restart wait, recmin prints 0 or 1 — BOTH fine; the FAIL conditions
  are `recovery=0ms` (history lost) or `backstop=30min` (fresh-boot default leaked into
  a restart).
- 8-reading trip debounce at 100 ms/reading → hold each hand-dip submerged ~1–2 s.
- Session C needs ≥60 s valve open → count ~60–65 ambient `S1= …` lines (1 Hz) after
  `VALVE open` before dipping sensor 1. This 60+s DRY open is inherent to Flow 2 per the
  item contract ("keep opens <5 s where the flow requires aborting" — Flow 2 doesn't
  abort); the post-ACK extra fill must be aborted within <5 s.
- Long press during FILLING = +5 min to the backstop (NOT abort) — a mis-held abort
  press logs `BTN long`, fill continues; just short-press again.
- Both sessions begin with a RESET (capture started first) so the banner is the first
  stream line and history is cleared → first fill prints 30/0/0.
- Index rows B and C are the only README cells this task touches; status vocabulary is
  closed (README §2).
