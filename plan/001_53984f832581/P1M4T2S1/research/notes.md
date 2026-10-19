# Research notes — P1.M4.T2.S1 (RecoveryClock + FillDeadline)

## 1. Contract → codebase mapping (verified this session)

| Contract clause | Codebase fact (verified) |
| --- | --- |
| INPUT: backstopMinutes() from P1.M4.T1.S1 | **LANDED** in `firmware/jug_filler/jug_filler.ino`: prototype line 35, definition ends with banner `// ===== end backstop interpolation (consumed by P1.M4.T2.S1, P1.M6.T1.S2) =====` at **line 610**; `test/test_backstop/` exists. My block inserts directly above `// ---- #ifdef ARDUINO REGION: hardware glue ----` (line 612) → lands directly below T1S1's end-banner (correct dependency order: consumer below consumed). |
| INPUT: time utilities P1.M1.T2.S1 | `uint32_t elapsedMs(uint32_t now, uint32_t since)` (prototype line 32) and `bool deadlinePassed(uint32_t now, uint32_t deadline)` (body at line 70: `return elapsedMs(now, deadline) < HALF_WHEEL_MS;` — true iff `now` is at/after `deadline` within the last half-wheel). Both wrap-safe; both reused, neither reimplemented. |
| INPUT: config constants | `config.h`: `constexpr uint16_t FRESH_BOOT_BACKSTOP_MIN = 30` (line 32), `constexpr uint16_t PLUS5_MIN = 5` (line 41), `BASELINE_SETTLE_MS` (used by test only), `BACKSTOP_TABLE_MIN`/`BACKSTOP_TABLE_LEN` (used only via backstopMinutes). All consumed BY NAME; §8 frozen — zero additions. |
| "Pure classes" | House pattern: bannered class blocks in UNGUARDED region (region markers at lines 41 / 612); classes get NO prototype (only free functions do — seam rule #3; `backstopMinutes` needed one, `RecoveryClock`/`FillDeadline` do not). const-method precedent exists (6 `) const` occurrences). |
| Tests: native Unity | One dir per project under `test/`, auto-discovered; explicit `int main()` Unity runner REQUIRED on PIO 6.2.0; relative `.ino` include. New dir: `test/test_deadline/` (9 existing dirs + this = 10). |
| Validation commands | `pio test -e native`, `pio run -e pico`, purity grep (comment-stripped), marker count 2 — all proven in prior sessions; host g++ + Unity at `.pio/libdeps/native/Unity/src` (unity.h + unity.c must BOTH be given to g++ — unity.c is the implementation; linking only unity.h fails with undefined `UnityAssertEqualNumber`). |

## 2. Design decisions (and why)

1. **Budget minutes at press, deadline at valve-open (two instants, one object).**
   PRD §6.2: "computed once, at fill start" (button press). PRD §6 preamble: backstop bounds VALVE-OPEN time; the 2 s baseline settle precedes valve open. → `beginFill(clock, nowMs)` computes & snapshots minutes at press; `anchor(valveOpenedAtMs)` fixes the origin later; `expired()` is false until anchored (settle window runs no deadline).
2. **Storage = anchor + budgetMs, not a baked deadline.** `expired(now) = anchored_ && deadlinePassed(now, anchorMs_ + budgetMs_)`. Wrap-safe by reusing P1.M1.T2.S1 helpers; `extend5()` (before OR after anchor) just grows `budgetMs_`; `deadlineMs()` accessor computes `anchorMs_ + budgetMs_` (natural unsigned wrap — log display only; expiry never uses it directly).
3. **Fresh-boot branch lives in FillDeadline, not RecoveryClock.** `recoveryMs()` with no history is meaningless; `beginFill` checks `hasHistory()` → `FRESH_BOOT_BACKSTOP_MIN`, else `backstopMinutes(recovery/60000)`. T1S1's frozen contract: "the caller uses FRESH_BOOT_BACKSTOP_MIN directly and never calls this with a fabricated recovery" — this task IS that caller.
4. **Sub-minute recovery truncates** (`recoveryMs/60000`). House integer style; table is minute-granular; matches T1S1's frozen composition test (`elapsedMs/60000`). ≤1 min of recovery error moves the grant at most one table step.
5. **Abort = another `recordFillEnd` at the abort instant** (PRD §6.1: "restarts at the abort instant, exactly like a normal fill end"). Done/abort/fault all close the valve → the fill engine (P1.M6.T1.S2) calls the same method at every valve close.
6. **Reset clears history = RAM object lifetime** (PRD §5.3). Nothing to implement: a reset re-constructs the objects → `hasHistory_ == false` → fresh-boot path. Documented in the class banner + test comment.
7. **extend5 before anchor is legal** (defensive): +5 is FILLING-only per §5.1 and FILLING spans settle+open, so a press during settle is possible; budget-first storage handles both orders identically.

## 3. Item-description discrepancy — RESOLVED

Item gloss: "history at 15 min recovery -> interpolated value (~36-37, assert computed)".
Normative table {2,44},{10,41},{20,38},{35,34},{60,30}: 15 min sits on the 10→20 segment → 41 − 3·(15−10)/10 = 39.5 → **floor 39** (T1S1 machine-verified & pinned by its frozen test `test_midpoint_15_floors_to_39`). The "~36-37" gloss is wrong (it would correspond to recovery ≈ 25–27 min); "assert computed" governs. Test asserts BOTH pinned 39 and computed `backstopMinutes(15)`. PRD table wins over item prose — documented in test comment.

## 4. Machine-verification record (this session)

- Merged copy at /tmp/t2s1: REAL current `.ino` (T1S1 landed) + my block inserted above the ARDUINO marker; test at test/test_deadline/test_deadline.cpp.
- `g++ -std=c++17 -Wall -Wextra -Werror -I<Unity>/src test_deadline.cpp <Unity>/src/unity.c -o run` → clean compile, zero warnings.
- `./run` → **9 Tests 0 Failures 0 Ignored, OK, exit 0** (fresh-boot 30; 15-min→39 pinned+computed; abort reference; zero-recovery→44; anchor+boundary (deadline−1 open / deadline expired, via BASELINE_SETTLE_MS); +5×2 = exactly +10 min and un-expires old deadline; +5 before anchor; rollover wrap for both clock and deadline; history persists across fills until reset).

## 5. Downstream API freeze (for P1.M6.T1.S2 / P1.M6.T2.S1)

- `RecoveryClock::recordFillEnd(nowMs)` at EVERY valve close (done/abort/fault); `hasHistory()`; `recoveryMs(nowMs)`.
- `FillDeadline::beginFill(clock, pressMs)` on button-press→FILLING; `anchor(valveOpenedAtMs)` at valve open; `expired(nowMs)` each loop while FILLING; `extend5()` on +5 long-press.
- Log line (PRD §3): `freshBoot()`, `recoveryMsAtStart()`, `tableMinutes()`, `deadlineMs()`, `budgetMinutes()`.
