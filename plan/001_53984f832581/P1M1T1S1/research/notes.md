# Research Notes — P1.M1.T1.S1 (config.h)

Session: PRP creation. All findings already synthesized into `../PRP.md`.

## Sources consulted (no new external research needed — pre-verified by the architect)

1. **`plan/001_53984f832581/architecture/system_context.md`** — repo is GREENFIELD; this is
   the first firmware file; no patterns to follow; PRD (spec/SPEC.md) is requirements truth.
2. **`plan/001_53984f832581/architecture/external_deps.md`** — binding facts baked into the PRP:
   - arduino-pico `analogRead` defaults to **10-bit** → `ADC_BITS=12` addition is mandatory
     (source: https://arduino-pico.readthedocs.io/en/latest/analog.html).
   - RP2040 watchdog hard ceiling **8388 ms** (RP2040-E1) → static_assert on WATCHDOG_TIMEOUT_S.
   - `constexpr` (not `#define`) so native tests can static_assert — "Recommended repo layout".
   - WS2812 brightness ≤ 60/255 (3V3 LDO budget) → static_assert on RGB_BRIGHTNESS.
   - Native test seam: tests `#include "../firmware/jug_filler.ino"` → config.h MUST be free of
     Arduino includes; only `<stdint.h>`.
3. **`plan/001_53984f832581/architecture/firmware_architecture.md`** — seam rules; downstream
   consumers of every constant (P1.M1.T1.S2 sketch include, P1.M3.T1.S2 analogReadResolution,
   P1.M4.T1.S1 backstop table iteration, P1.M7.T1.S1 invariant suite); Mode A docs = the
   group/unit comments are the documentation surface.
4. **PRD `spec/SPEC.md` §8 + §2.1** (via selected_prd_content, identical in the file) — verbatim
   constant names/values and the canonical pin map. Cross-checked live in the repo.
5. **`plan/001_53984f832581/tasks.json`** — confirmed sibling P1.M1.T1.S2 owns the dual-build
   skeleton + native test seam (so S1 must NOT create platformio.ini/test/); downstream task
   titles confirmed consumer relationships.

## Tooling verified on this machine

- `g++` (GCC 16.2.1) present at /usr/bin/g++ → PRP validation Levels 1–3 use plain host g++,
  no build system required (correct, since P1.M1.T1.S2 hasn't landed).

## Key decisions encoded in the PRP

- Path `firmware/config.h` (architecture "Recommended repo layout"), `#pragma once`.
- 32 constexpr definitions: 30 scalars + BACKSTOP_TABLE_MIN array + derived BACKSTOP_TABLE_LEN.
- Explicit fixed-width types chosen per unit/domain (uint8_t pins/brightness, uint16_t
  seconds/counts/minutes, uint32_t ms/baud, bool flag) — safe widening into Arduino APIs,
  matches uint32_t millis() domain.
- `BackstopTableEntry` POD struct {recoveryMin, backstopMin} (type allowed; functions are not).
- Semantic static_asserts beyond values: BASELINE_MIN<MAX, MAX≤4095, MIN≥TRIP_DROP_COUNTS,
  watchdog≤8388 ms, RGB≤60, fresh-boot default == last table row (concavity/full-tank anchor).
- Scope fence: nothing but firmware/config.h; scratch validation in /tmp, deleted after.
