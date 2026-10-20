# Research notes — P2.M1.T3.S1 (changeset-level doc sync)

## Verified facts (live reads)

**Repo doc surface (all of it):**
- `README.md` (root) — exists; current sections: intro/§"Repo layout", "Build in Arduino IDE" (good, keep), "Build with PlatformIO" (`pio run -e pico` only), "Run native tests" (`pio test -e native`), "Serial logging" (log table), "Safety note" (valve-pin-first only). MISSING: device summary, capability/state summary, wiring pointer, USB/buck power rule, operation quick-reference table, commissioning pointer (currently says "(planned)"), §10 invariants recap.
- `spec/SPEC.md` — READ-ONLY (PRD). Section numbers verified: §0 Summary, §1 BOM, §2 Wiring (2.1 pin map, 2.2 power cautions, **2.3 = "Complete wiring netlist"** — NOT "block diagram" as in PRD index), §3 platform, §4 states/LED, §5 button (5.1 table, 5.2 flows, 5.3 reset), §6 timer, §7 faults, §8 config.h contract, §9 commissioning, §10 safety invariants, §11 limitations.
- `docs/` — does NOT exist yet; T2S1 (parallel, implementing) creates `docs/COMMISSIONING.md` per its PRP contract. T3.S1 links to it (lands before T3.S1 implementation).
- No other changeset-level docs. `plan/` is scaffolding, not repo docs.

**Architecture finding (system_context.md #5):** "Spec is the requirements source of truth — README/docs generated later must point to spec/SPEC.md rather than duplicate its tables (prevents drift)." → Mode B.

**Build facts (platformio.ini, verified):**
- envs: `pico` (raspberrypi / waveshare_rp2040_zero / arduino, compile-only, no upload), `native` (Unity host tests, default_envs=native). src_dir = firmware/jug_filler, test_dir = test.
- Verified working commands from bench record: `pio test -e native` → 192/192 PASS; `pio run -e pico` → SUCCESS, UF2 at `.pio/build/pico/firmware.uf2`; `python3 test/check_setup_first_line.py` PASS.

**Parallel-sibling ownership (avoid collisions):**
- P2.M1.T1.S1 (Implementing) OWNS README's "Serial logging" table (Mode A: table matches code exactly, incl. enriched fields recmin=/count=/openms=). T3.S1 must NOT rewrite formats — preserve that section, only adjust surrounding pointers.
- P2.M1.T2.S1 (Implementing) creates docs/COMMISSIONING.md only; its PRP says "P2.M1.T3.S1 (later) adds the link from README to this runbook".

**Known divergence — spec §4 vs shipped firmware:**
- spec/SPEC.md §4 says: "The onboard WS2812 RGB LED (GP16) is the sole status/fault indicator — there is no external LED."
- Merged PRD §4 + shipped firmware (config.h PIN_LED=GP14, P1.M5.T2 complete, P1.M5.T3 WS2812 mirror config-gated): external green LED on GP14 is authoritative, WS2812 mirrors.
- README must describe SHIPPED reality and point to spec §4 for pattern SHAPES; never repeat the "no external LED" sentence. Same handling as T2S1 PRP.

**Bench status (P1.M7.T2.S1 issue_feedback, attempt 3/3):** BLOCKED-pending-hardware. Native/UF2/structural gates all green; no rig on host; **no docs/bench/ exists — never fabricated**. Item description lists "bench evidence under docs/bench/" as input, but it may not exist at implementation time → README must state validation status honestly and CONDITIONALLY link docs/bench/ only if it exists.

**Operation quick-reference source (PRD §5.1, sanctioned compact duplication):**
| State | short | long |
| IDLE | start fill | nothing |
| FILLING | abort | +5 min (stacks) |
| CONFIRM | ignored | ignored |
| DONE | ack | ack |
| FAULT | ignored | ignored |
Plus RESET note: full reboot → IDLE, valve closed, tank history cleared → 30-min fresh backstop.

**No-drift markers:** README must NOT contain pin-map table content (e.g. `ADC0`, `| Board pin`), full BOM, full §4 LED table with columns, or BACKSTOP_TABLE values.

## Decisions
- Diff scope: README.md (+ at most minimal touch-ups to docs/COMMISSIONING.md cross-links if broken; prefer verify-only there since T2S1 lands in parallel).
- Validation = structural markdown checks, link/anchor existence, content greps, no-drift greps, diff-scope gate, `pio test -e native` regression.
