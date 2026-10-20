# Research notes — P1.M2.T1.S2 (per-session evidence templates, rails sheet, discrepancy template)

## 1. Verified firmware log-format strings (jug_filler.ino — source of truth)

All verified by grep against `firmware/jug_filler/jug_filler.ino` on this working tree:

| Exact prefix / format | .ino line | Notes |
|---|---|---|
| `JugFiller boot (reset cause: %s)` → `POWER_OR_RESET` \| `WATCHDOG` | 86 | banner; comment L81–82 |
| `VALVE close` (literal) | 1353 | valve-closed-first on every exit |
| `FILL start backstop=%umin recovery=%lums recmin=%u` | 1356 | recmin = recovery/60000 truncated |
| `BASE ok s1=%u s2=%u` | 1359 | |
| `FAULT baseline sensor=%u code=%u count=%lu` | 1361 | code = sensor number (1/2) |
| `VALVE open budget=%umin` | 1364 | |
| `TRIP sensor=%u fillms=%lu` | 1366 | |
| `CONFIRM pass sensor=%u durms=%lu` | 1369 | |
| `DONE attemptms=%lu openms=%lu` | 1373 | |
| `ABORT attemptms=%lu openms=%lu` | 1377 | |
| `FAULT earlywet sensor=%u code=%u fillms=%lu` | 1380 | |
| `FAULT earlytrip sensor=%u code=%u fillms=%lu` | 1383 | |
| `FAULT backstop code=3 budget=%umin openms=%lums` | 1387 | note `openms=…ms` suffix |
| `FAULT confirm sensor=%u code=%u durms=%lu` | 1390 | |
| `ACK done` (literal) | 1395 | |
| `BTN %s %lums` → `BTN short <n>ms` / `BTN long <n>ms` | 1685 | short <1000 ms, long ≥1000 ms |
| `S1=%u S2=%u` | 1712 | 1 Hz ambient, every state |

Formats are APPEND-ONLY / prefix-anchored (comment L1343–1344) — hence "copy-exact prefixes" requirement.

## 2. Config facts (firmware/jug_filler/config.h)

- `BUTTON_LONG_PRESS_MS = 1000` (L28); `FRESH_BOOT_BACKSTOP_MIN = 30` (L32)
- `BACKSTOP_TABLE_MIN[] = { {2,44}, {10,41}, {20,38}, {35,34}, {60,30} }` (L35–37), clamped both ends
  → recovery ≤ 2 min (incl. any small nonzero recovery) ⇒ backstop **44 min**; fresh boot (recovery=0ms)
  **bypasses the table** ⇒ 30 min. This is why Session B restart must show `backstop=44min` and
  must NOT show `recovery=0ms`.
- `CONFIRM_WINDOW_S = 12` (L44); `EARLY_WET_WINDOW_S = 5` (L45); `DEBOUNCE_COUNT = 8` (L59);
  `WATCHDOG_TIMEOUT_S = 8` (L74)
- SPEC §7.6 names `TRIP_MIN_ELAPSED_S` (60 s) for early-trip guard.

## 3. SPEC facts

- §4 LED: IDLE off; FILLING solid blue; CONFIRM solid blue continues; DONE green triple-pulse
  3×(200 on/200 off) + 1000 off; FAULT red N-blink groups N×(250 on/250 off)+2000 off.
  WS2812 (GP16) "sole indicator" — but COMMISSIONING describes GP14 external mirror (known nuance,
  bench package §6 → pre-seed in discrepancies template; do NOT edit spec).
- §5.2 Flow 1 (abort): press1 start → press2 abort → press3 restart recomputed. Flow 2: press1 start →
  complete → press2 ack (blink stops) → press3 next fill.
- §5.3: any reset clears tank history → next fill 30 min; never auto-starts a fill after any reset.
- §6.1: recovery clock reference = previous fill's valve-close instant; abort restarts it like a normal end.
- §7: baseline gate (2 s settle, median baselines, refuses valve open, `VALVE close` → `FAULT baseline`);
  faults latch, BOOT ignored while faulted, only RESET clears; fault codes 1/2/3 = sensor1/sensor2/timer.
- §9.1 dry bench; §9.2 rails 4.9–5.1 V, never USB+buck; §9.4 dry-run flows with hand-dipped sensors.
- §10: watchdog ~8 s, fed every loop; hang → reset → valve closed → IDLE, no auto fill.

## 4. COMMISSIONING.md cross-refs

- Stage 2 (L190) = power rails; summary table rows 541–542 give the exact two fields:
  "Buck preset (board disconnected) `____ V`" and "Buck at board 5 V pin (buck-powered) `____ V`".
- Serial instrument section: 115200 baud; `S1=3487 S2=3512` example of the 1 Hz line.

## 5. S1 sibling contract (P1M2T1S1/PRP.md — being implemented in parallel)

- S1 creates `docs/bench/README.md` ONLY (index + capture procedure + integrity rule).
- Conventions S2 must match: filename grammar `YYYY-MM-DD_session-<X>-<slug>.log`
  (slugs: dry-bench, flow1-abort, flow2-success, fault-paths, watchdog), `YYYY-MM-DD_rails.md`,
  `YYYY-MM-DD-discrepancies.md`; status vocabulary PENDING / PASS / PARTIAL(n✗) /
  BLOCKED: <reason> / SKIPPED: <reason> (SKIPPED = optional items only: Session E, D's
  backstop-expiry); operator header block fields (date, firmware git rev, wiring, deviations);
  checklist summarized in index as pass/total; "per-checklist detail lives in the session's
  evidence template" → session templates are filled IN PLACE (they are the persistent records);
  rails + discrepancies are date-stamped §3 evidence files → copy template → date-stamped file.
- S2 must NOT create or modify docs/bench/README.md. Precondition: it must exist before S2 starts
  (STOP if missing — dependency not landed).

## 6. Design decisions for the template set

1. Seven new files, all in docs/bench/:
   template-session-A-dry-bench.md, -B-flow1-abort.md, -C-flow2-success.md, -D-fault-paths.md,
   -E-watchdog.md, template-rails.md, template-discrepancies.md (slugs mirror §3 grammar).
2. Copy-exact prefixes inside backticks; `…` for elided variable numerics in expected lines;
   `____` for operator-fill blanks; pinned values only where contract/firmware pin them
   (B: 30min/0ms fresh, 44min restart; A first dry-fire 30min/0ms).
3. Session templates carry: scope line, session-record block (mirrors index columns),
   preconditions (power rule pointer), ordered checklist w/ `- [ ]` + observed blanks,
   LED observation lines, CRITICAL blockquote assertions, verification-grep hints,
   after-session steps. All ship PENDING/unchecked, no real dates.
4. Critical assertions per item contract: B restart NOT `recovery=0ms` + table value not
   fresh-boot default; D1 NO `VALVE open` before `FAULT baseline`.
5. Discrepancies template pre-seeds the GP14-vs-WS2812 known nuance (bench §6) as entry D-1
   "to confirm at bench", follow-up pending, do-not-edit-spec note.
6. External research: not applicable — static markdown authored against internal binding
   contracts (bench_evidence_package.md + system_context.md §2 + SPEC). No libraries involved.

## 7. Repo state at research time

- docs/bench/ does NOT exist yet (S1 in flight). docs/ contains only COMMISSIONING.md.
- No test framework applies (markdown-only); validation is mechanical grep gates + git scope.
