# P1.M2.T2.S4 research — Session E optional watchdog-reset check

## Verified facts (read from disk this session)

### Firmware (firmware/jug_filler/)
- `config.h:74` — `constexpr uint16_t WATCHDOG_TIMEOUT_S = 8;` (hard ceiling 8.388 s RP2040-E1; do not raise).
- `config.h` contains ZERO `#define`/`#if` (grep count = 0) — constexpr only; no `UNIT_TEST`-adjacent symbol anywhere in firmware/, test/, platformio.ini → the hang guard must be a temporary local edit (evidence package §5, scout-verified).
- `jug_filler.ino:1646` — `setup()` line 1: `digitalWrite(PIN_VALVE, LOW);` then `pinMode(PIN_VALVE, OUTPUT);` — valve closed before all else on every reset.
- `jug_filler.ino:1657` — `formatBootBanner(banner, sizeof(banner), watchdog_caused_reboot());` → banner text built by pure fn at lines 85–87: `"JugFiller boot (reset cause: %s)"` with `WATCHDOG` vs `POWER_OR_RESET`.
- `jug_filler.ino:1669` — `watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S * 1000UL, true);` LAST in setup(); `pause_on_debug=true` (bench runs unaffected).
- `jug_filler.ino:1672-1673` — `void loop() { watchdog_update(); // FIRST STATEMENT ...` — fed every loop pass; a `while(true){}` anywhere in the loop body (which never returns) starves it → reset ≈8 s later.
- `jug_filler.ino:1404` — `#include <hardware/watchdog.h>` (pico-sdk, inside `#ifdef ARDUINO` region; only ifdef boundary is `#ifdef ARDUINO` ~L1402).
- Fill engine seam for the hang condition: `static FillEngine s_fill;` (L1477), public `FillState state() const` (L1263), `enum class FillState : uint8_t` (L913) with `Filling` (L940). In `loop()` the fourth stage runs `s_fill.step(...)` (L1721) then `digitalWrite(PIN_VALVE, fill.valveOpen ? HIGH : LOW)` (L1731) and logs — so a patch placed after the fill-lifecycle routing block can test `s_fill.state() == FillState::Filling` and hangs on the first pass after `VALVE open`.
- Expected log shapes (from evidence package §2 / templates): `FILL start backstop=30min recovery=0ms recmin=0` → `BASE ok s1=… s2=…` → `VALVE open budget=30min` → hang → ~8 s → banner `JugFiller boot (reset cause: WATCHDOG)` → LED off, 1 Hz `S1= S2=` resumes, NO `FILL start` without a following BTN press (SPEC §5.3/§10).

### Bench contract (docs/bench/ + architecture)
- `docs/bench/template-session-E-watchdog.md` — the binding checklist: fill IN PLACE; row E may be `SKIPPED: <reason>` (the ONLY row where SKIPPED is legal per README §2); preconditions (USB-only, capture before patched reset, patch never committed, optional 12 V for the close-click = recorded ~8 s deviation from the <5 s dry-fire rule); procedure; expected observations incl. critical assertions (cause must read WATCHDOG; no un-pressed FILL start; patch reverted with git status clean); mandatory cleanup (record patch method/applied/reverted); operator greps.
- `docs/bench/README.md` — §1 integrity rule (genuine captures or BLOCKED, never fabricate; never edit a .log); §2 status vocabulary + LIVE state: rows A/B/C BLOCKED (rig unavailable — no RP2040-Zero serial device on this host), rows D/E PENDING (S3 runs in parallel; its contract has a BLOCKED branch likely firing); §3 capture procedure (USB-only binding — VSYS ties to USB VBUS no diode; 115200 8N1; capture before reset; operator header block mandatory first lines; item 5: Session-E-only rule — the deliberate hang is a temporary never-committed patch, `git status` clean afterward, header records apply+revert); §4 naming `YYYY-MM-DD_session-<X>-<slug>.log`, shared `YYYY-MM-DD-discrepancies.md` per bench day.
- `architecture/bench_evidence_package.md` §1 power rules (12 V to MOSFET module ALONE allowed; dry-fire opens <5 s), §2 Session E expectations, §4 executability split (the watchdog test flash is NOT software-agent executable), §5 hang mechanism (canonical patch shape `#if BENCH_HANG_TEST … while(true){} … #endif` inside loop(); reverted, never committed).

### Sibling contracts
- P1M2T2S3/PRP.md (Session D, parallel/implementing): owns row D + template D + session-D log; BLOCKED branch first-class given rig unavailability; do-not-touch boundaries. S4 must not touch any D artifacts.
- P1M2T2S1/S2 (Sessions A/B/C): rows currently BLOCKED on this host; their evidence discipline (header block, append-only discrepancies) is reused.
- P1.M2.T2.S5 (downstream): consolidates rows A–E + discrepancy ledger — row E must be finalized (any closed-vocabulary status) before S5.

### Build/flash mechanics
- `platformio.ini`: `env:pico` (board build; comment says "Compile-only … No upload from this project yet") and `env:native`. Flashing the patched build requires an explicit upload target: `pio run -e pico -t upload`. arduino-pico upload uses picotool with USB-CDC auto-reboot (1200-baud touch); manual fallback: hold BOOT (BOOTSEL) + tap RESET → board enumerates in bootloader → retry upload. `pio device list` detects the serial device for the entry gate.
- `git status --short firmware/` currently empty (clean tree) — the patch must restore exactly this state afterward.
- No reflash of pristine firmware is required by the template's cleanup text, but leaving hang-patched firmware ON THE BOARD would hang every future FILLING session — PRP makes rebuild+reflash-pristine a mandatory cleanup step (bench hygiene; keeps S5/future sessions safe), verified by a fresh `POWER_OR_RESET` banner boot.

## Decision: patch design
Single contiguous local edit inside `loop()`, placed after the fill-lifecycle routing stage (~L1740, after `// ---- end Fill lifecycle routing ----`), shape:
```cpp
// ---- TEMPORARY Session-E bench hang patch — NEVER COMMIT — REVERT AFTER USE ----
#define BENCH_HANG_TEST 1
#if BENCH_HANG_TEST
if (s_fill.state() == FillState::Filling) { while (true) {} }
#endif
#undef BENCH_HANG_TEST
```
Why: (a) contract names the `#if BENCH_HANG_TEST` shape; (b) self-contained define → one hunk → revert = `git checkout -- firmware/jug_filler/jug_filler.ino`; (c) condition on `FillState::Filling` lets the board boot IDLE, accept the press, log `VALVE open`, THEN hang (matches template "trigger the hang during FILLING"); (d) hangs on the pass after entering FILLING → solenoid energization ≈8 s (the template's recorded deviation); (e) the hang never calls `watchdog_update()` → guaranteed starvation reset.
