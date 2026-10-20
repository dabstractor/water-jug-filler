# Validation Report — water-jug-filler

**Date:** 2025-10-19 · **Validator:** automated `./validate.sh` + manual deep review
**Scope:** full codebase vs PRD (spec/SPEC.md) and plan/001_53984f832581/tasks.json
**Result:** ✅ **25/25 validation checks PASSED, 0 failed** (2 minor issues + 1 doc warning found — none affect firmware behavior on the target)

---

## What was validated

| Phase | What | Result |
|---|---|---|
| 1 | Toolchain (pio 6.2.0, g++ 16.2.1, python 3.14), repo layout, two-file firmware constraint (PRD §3), platformio envs | PASS |
| 2 | Structural safety invariants (PRD §10): `setup()` line 1 = `digitalWrite(PIN_VALVE, LOW)`; `loop()` line 1 = `watchdog_update()`; watchdog 8000 ms < 8388 ms RP2040 ceiling; no `delay()`; no `while(!Serial)`; `analogReadResolution(12)`; BOOTSEL sampled as bool object (correct arduino-pico API) | PASS |
| 3 | `config.h` contract: every PRD §8 constant (pins, timings, thresholds, table knots, LED cadences, brightness cap ≤ 60, baud, watchdog) verified via compiled `static_assert`s | PASS |
| 4 | Native Unity suite: `pio test -e native` — **214/214 test cases passed** across 16 suites | PASS |
| 5 | Device compile: `pio run -e pico` (arduino-pico, Waveshare RP2040-Zero) — SUCCESS, RAM 3.9%, Flash 3.8% | PASS |
| 6 | **Independent E2E harness** (this validation's own, not the repo's tests): compiles the real sketch logic on the host and drives it through the documented user journeys — **235/235 checks passed** (details below) | PASS |
| 7 | Documentation & plan bookkeeping | PASS + 1 warning |

### E2E user journeys exercised (Phase 6 detail)

The harness mirrors the real `loop()` wiring (aggregator → classifier → `FillEngine::step`) and simulates a user + the physical world at 3 ms resolution:

- **J1** Boot → IDLE, valve closed, LED off, zero spurious events.
- **J2 / PRD §5.2 Flow 1**: press → fill (fresh-boot backstop 30 min, `recovery=0`) → mid-fill abort (valve closed, ABORT logged, recovery clock restarted) → immediate restart recomputes 44 min (≤2 min clamp).
- **J3** +5 long-press during FILLING: stacks (30→35→40 min), LED ack, valve stays open; IDLE long-press does nothing; no PLUS5 from IDLE.
- **J4 / PRD §5.2 Flow 2**: fill → first trip at >60 s valve-open (early-trip guard not tripped) → CONFIRM → second trip → DONE → acknowledge → next fill backstop interpolated from recovery measured **from the valve-close instant**: exactly 27 min → 36 min (concave table, ceiled drop, floored result).
- **J5** Wet sensor at start → baseline fault code 1, **valve never opens**, fault latches, BOOT ignored; fresh instance (reset) = IDLE, no history → 30 min (PRD §5.3).
- **J6** Wet reading inside the 5 s early window → earlywet fault code 2 with correct payload.
- **J7** Confirm-window timeout at exactly 12 s → fault code 2 (the never-tripped sensor).
- **J8** Backstop expiry with dry sensors → fault code 3 at 30 min, budget reported incl. two stacked +5s (fault at ~40 min with budget 40).
- **J9** LED pattern boundary timings: DonePulse (3×200/200 + 1000 gap), FaultBlink(3) (3×250/250 + 2000 pause, cycle 3500 ms), ack overlay (3×100/100 then Solid resumes), mode-change phase reset.
- **J10** Button classifier at 1 ms: 200 ms→Short, 1000 ms→Long, 999 ms→Short, nothing while held, bounce spikes ignored, back-to-back presses.
- **J11** Backstop table: knots exact, both clamps, interpolation (6→42, 15→39, 30→35), monotone non-increasing over 0–120.
- **J12** Seeded 150k-step randomized fuzz with periodic power-cycles: random button chaos + wet/dry bursts — 7 fill starts, ~11 s of valve-open across real fills; invariants held throughout: no valve-open outside IDLE+ShortPress, every FILLING/CONFIRM exit closes the valve first (logs[0]=ValveClosed), faults never unlatch spontaneously, state→valve/LED level coherence.

### Verified-OK items worth calling out (manual review)

- **WS2812 mirror**: the embedded PIO program, `sm_config_set_out_shift(&c, false, true, 24)` and GRB packing `(g<<24)|(r<<16)|(b<<8)` were cross-checked against the **official pico-sdk sources shipped in the installed core** (`pico_status_led/status_led.c`: "0xGGRRBB00") — byte-exact match. Brightness scaling yields exactly 0/60 per channel (≤60/255 power budget, PRD §2.2).
- The core bundles **no** WS2812 library (checked `libraries/`), so the embedded-program approach is justified; `PIOProgram` helper exists and the device build links.
- Valve-close-before-state-flip ordering is structural in the engine and mirrored by the loop writing the pin before flushing log events.
- `S1=/S2=` 1 Hz commissioning line, backstop computation lines, and end-of-fill summaries satisfy the §9.3/§9.5 calibration workflow from the log alone (cross-checked against docs/COMMISSIONING.md's documented transcript).

---

## Issues found (itemized)

### Issue 1 — Minor (documentation drift): README calls an existing, committed document "(planned)"
- **File:** `README.md:70` — "Full commissioning log workflow: `docs/COMMISSIONING.md` **(planned)**."
- **Fact:** `docs/COMMISSIONING.md` exists (557 lines), is committed (`6ea8009 docs: commissioning and calibration runbook`), and its content matches the shipped log formats (verified: `FILL start … recmin=`, `BASE ok …` lines appear as documented).
- **Impact:** A reader would believe the calibration runbook doesn't exist yet and skip it.
- **Suggested fix:** Remove "(planned)" (one-word edit).

### Issue 2 — Minor (code hygiene, host-only latent UB): `%lu` varargs mismatches with `uint32_t`
- **Files:** `firmware/jug_filler/jug_filler.ino:1356–1386` (`formatFillLogEvent`) and `:1680` (`logLine("BTN %s %lums", …)`).
- **Fact:** `uint32_t` is `unsigned int` on the LP64 host (where the native tests compile these exact formatters), but `%lu` expects `unsigned long` — 25 `-Wformat` warnings under `g++ -Wall`. On the device target (arm-none-eabi) `uint32_t` **is** `unsigned long`, and on x86-64 SysV the mismatch happens to work due to 32-bit register zero-extension — so behavior is currently correct on both real targets, but it is technically undefined behavior on the host and fragile under any future ABI/toolchain change (and it silences nothing — it's plain UB).
- **Impact:** None today; latent risk in the native-test seam only. Device firmware unaffected.
- **Suggested fix:** Cast payloads at the call sites, e.g. `(unsigned long)e.a` (the code already does exactly this cast style for the `%u` fields via `(unsigned)(e.sensor + 1)`), or use `PRIu32` from `<inttypes.h>`.

### Warning (informational, surfaced by Phase 7)
- `plan/001_53984f832581/tasks.json` is otherwise **accurate** (46/48 subtasks Complete); the two open items are genuinely open: `P1.M7.T2.S1` (physical bench validation — requires hardware, honestly "Researching") and `P2.M1.T3.S1` (final docs sweep — "Ready", its plan dir is untracked). No plan/repo drift beyond Issue 1.

---

## Not covered by this validation (and why)

- **On-hardware behavior** (P1.M7.T2.S1: real ADC noise, real BOOTSEL feel, real valve current, LED visibility, watchdog reset on a live board). The firmware was validated as logic + compile + structure; the bench session with USB-only power remains the remaining open task per plan.
- **Arduino IDE build** was not executed (no Arduino IDE in this environment); the sketch layout (sketch folder name == sketch name, two files) is IDE-legal and the identical core compiles it via PlatformIO.

## Verdict

The codebase is in excellent shape: every PRD behavioral contract tested (by 214 repo tests **plus** 235 independent E2E checks) passes, all §10 safety invariants hold under structural, deterministic-journey, and randomized-fuzz scrutiny, the device build is clean, and only two minor, non-behavioral issues (one stale word in README, one host-only format-string hygiene item) were found.
