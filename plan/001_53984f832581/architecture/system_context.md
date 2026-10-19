# System Context — Codebase State (research date: this session)

## Verdict: GREENFIELD

The repository at `/home/dustin/projects/water-jug-filler` contains **no firmware code** yet. Complete file inventory:

| Path | Content |
|---|---|
| `.envrc` | `export PRD_FILE=spec/SPEC.md` (direnv; orchestrator input pointer) |
| `spec/SPEC.md` | The PRD itself (verbatim source of the distributed PRD; sections 0–11) |
| `plan/001_53984f832581/prd_snapshot.md` | Orchestrator's PRD snapshot (do not edit) |
| `plan/001_53984f832581/prd_manifest.txt` | `53984f832581 spec/SPEC.md` |
| `plan/001_53984f832581/prd_index.txt` | PRD structure index |
| `.git/` | 3 commits: spec only (`spec: finalize behavior — state machine, BOOT semantics, tank model, config contract` on top) |

## Consequences for planning

1. **No existing patterns to follow** — no build system, no `src/`, no `platformio.ini`, no tests, no CI. The first task creates the layout. PRD §3 dictates it: **two firmware files** — a sketch (`.ino` or `src/main.cpp`) + `config.h` holding every constant from §8 — on arduino-pico (Earle Philhower core), board "Waveshare RP2040-Zero", optionally PlatformIO (`raspberrypi` / `waveshare_rp2040_zero`) but **must also build in Arduino IDE**.
2. **No unresolved PRD includes** — the PRD text is the complete merged `spec/SPEC.md`; no verbatim `@...` lines remain. Nothing failed to resolve.
3. **Architecture is ours to define.** The single most important architectural decision (made in `firmware_architecture.md`) is the **pure-logic/hardware seam** that lets every behavioral rule in PRD §4–§7 (state machine, button classifier, backstop math, trip detection, LED pattern engine) be unit-tested natively on the host with an injected clock, while `setup()`/`loop()` stay thin hardware glue.
4. **Test files are auxiliary, not firmware.** The PRD's "two files" constraint governs the firmware deliverable; a `test/` directory (PlatformIO Unity, native) plus `platformio.ini` are tooling around it, not a violation. Tests will `#include` the sketch source under a `UNIT_TEST` guard (see `external_deps.md` for the validated pattern).
5. **Spec is the requirements source of truth** — README/docs generated later must point to `spec/SPEC.md` rather than duplicate its tables (prevents drift).
6. Hardware is physically real (BOM in PRD §1) but not yet wired; bench/commissioning tasks (PRD §9) are physical-verification tasks and must state their power-safety preconditions (§2.2: never USB + buck simultaneously).
