# Research notes — P1.M2.T2.S5: consolidate discrepancies + finalize bench index

All facts read directly from the repo this session (no guessing).

## 1. Live state at PRP-writing time (S4 implementing in parallel)

- `git status --short`: `M docs/bench/README.md` (S4's row-E finalization in flight),
  S4 PRP/research + tasks.json staged. Firmware clean.
- Index rows (docs/bench/README.md §2), columns
  `Session | Scope | Date | Evidence file | FW git rev | Checklist | Status`:
  - Rows A–D: Date `—`, Evidence file = unfilled `YYYY-MM-DD_…` placeholder,
    FW rev `—`, Checklist `—`, Status = `BLOCKED: rig unavailable — …`
    (long recorded reasons; row A also cites no multimeter/physical access).
  - Row E (S4, in flight): `BLOCKED: rig unavailable — entry-gate rows A/B/C are BLOCKED …`
  - No row is `PENDING` anymore once S4 lands. Status vocabulary (README §2):
    PENDING / PASS / PARTIAL(n✗) / BLOCKED: reason / SKIPPED: reason.
    SKIPPED is legal only for row E (and the backstop sub-item inside D).
- `ls docs/bench/`: README.md + 7 templates ONLY. No date-stamped `.log`,
  no `*_rails.md`, **no `YYYY-MM-DD-discrepancies.md` yet** — S5 creates it.
- Session-finalizing commits (git log --oneline): row A `c3f6901`,
  rows B/C `aba9108`, row D `5a18d8a`; row E = S4's (uncommitted at PRP time).
  Commit-date recovery: `git show -s --format=%cs <rev>`.
- Firmware last-touched rev (same for every row — all later commits are docs-only):
  `git log -1 --format=%h -- firmware/` (verify at implementation time).
- All commits since the firmware change are docs/plan-only ⇒ the validated
  firmware identity for the whole package is that one rev.

## 2. D-1 discrepancy evidence — verbatim quote sources (file:line verified)

- spec/SPEC.md L9 (hardware table): "Onboard WS2812 RGB LED on GP16 — the sole
  status/fault indicator (§4)."
- spec/SPEC.md L31: "| GP16 (onboard, no wiring) | WS2812 RGB LED — sole status/fault
  indicator |" — and L33 lists **GP14 as an unwired spare**.
- docs/COMMISSIONING.md L20: "green LED on GP14 via 330 Ω" (wiring/BOM).
- docs/COMMISSIONING.md L96–98: "the firmware drives two indicators that always show
  the same **pattern shape** — the onboard WS2812 mirror (adds color) and the external
  LED on GP14 (on/off)."
- docs/COMMISSIONING.md L127: "external LED still shows every pattern."
- firmware/jug_filler/config.h L25: `constexpr uint8_t PIN_LED = 14; // GPIO# — GP14 →
  external green LED anode via 330 Ω (PRD §2.1)`
- firmware/jug_filler/config.h L69: `constexpr bool USE_ONBOARD_RGB = true; // mirror
  patterns on onboard WS2812 (GP16, no wiring) (PRD §4)`
- firmware/jug_filler/jug_filler.ino L715–747: pure LED pattern engine; L718 comment
  names the drivers (P1.M5.T2.S1 GP14, P1.M5.T3.S1 WS2812 mirror).
- ⇒ Mismatch is SPEC↔COMMISSIONING↔firmware (docs + code), NOT a log capture.
  Bench confirmation of rendered patterns impossible (rig BLOCKED) — record D-1
  as observed from these static sources; never fabricate a serial excerpt.

## 3. Contracts re-read this session

- architecture/bench_evidence_package.md §3 (docs/bench/ layout — discrepancies file
  is `YYYY-MM-DD-discrepancies.md`, one per bench day, shared; index README carries
  sessions/dates/FW git rev/pass-fail per checklist item) + §6 (the D-1 nuance:
  record it, do NOT edit spec/SPEC.md) + §4 (integrity rule: genuine captures or
  BLOCKED, never fabricate).
- docs/bench/template-discrepancies.md: copy to `YYYY-MM-DD-discrepancies.md`;
  pre-seeded D-1 ("to confirm"); rules: verbatim fenced quotes, never edit a
  captured .log, each entry carries a follow-up issue number once filed (or pending).
- Item contract: "each with a log excerpt and a proposed follow-up issue";
  BUG-004 is unused anywhere in the repo (grep: zero hits) ⇒ natural proposed ID
  continuing the BUG-001…003 convention from the bug-hunt report.
- template-session-A…E: per-session "Session record" tables (Date / Evidence log /
  FW git rev / Checklist / Status) — S1–S3 BLOCKED branches left templates unfilled;
  those templates belong to sibling tasks, S5 must NOT fill or alter them.
- Downstream P1.M2.T3.S2 (tasks.json, read this session): INPUT = "the finalized
  bench evidence package (index + discrepancy list) from P1.M2.T2.S5"; anchors at
  docs/COMMISSIONING.md "Completion sign-off" table (L535+, items 1–14).
  system_context.md L93–94 confirms the same anchor. So S5's output must be
  legible as a stand-alone sign-off input: honest verdict + one discrepancy file.
- S4 PRP (parallel contract, read in full): row E ends BLOCKED (gate A/B/C not PASS);
  S4 owns row E + template E; its cleanup ensures `git status --short firmware/`
  empty. S5 verifies but never edits S4 artifacts.
- Sessions A–D recorded NO expected-vs-observed mismatches (no captures exist),
  so D-1 is the sole ledger entry in the BLOCKED world; the PRP still generalizes
  (consolidate any entries the sessions DID record, if reality differed).

## 4. Watchdog-patch absence check (item contract LOGIC clause 3)

- `git status --short firmware/` must print nothing (verified clean now).
- Belt-and-braces: `grep -rn BENCH_HANG_TEST firmware/` → no hits (config.h is
  constexpr-only; the temporary define never lived in the committed tree).
- Record both results in the finalization note (README new §5).
