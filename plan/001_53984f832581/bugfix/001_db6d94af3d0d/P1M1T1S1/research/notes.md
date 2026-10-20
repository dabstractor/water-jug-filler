# Research Notes — bugfix P1.M1.T1.S1 (COMMISSIONING.md recmin corrections)

Session: PRP creation. Everything below verified first-hand in the repo this session;
synthesized into `../PRP.md`.

## Verified facts

1. **Formatter source of truth** — `firmware/jug_filler/jug_filler.ino:1354–1357`:
   `FILL start backstop=%umin recovery=%lums recmin=%u` with `recmin=(unsigned)(e.b/60000u)`
   (unsigned truncation). ⇒ 8200→0, 599999→9, 60000→1.
2. **Defective lines** (docs/COMMISSIONING.md, Stage 5a = L430–453):
   - L434: `- from \`FILL start backstop=44min recovery=8200ms recmin=2\` — the`
   - L436: `  truncated: 599999 ms → 0; \`recovery=0ms\` only ever means fresh boot);`
   Byte-level: L434's dash = U+2014 (e2 80 94); L436's arrow = U+2192 (e2 86 92).
3. **Pre-edit grep state** (`grep -n 'recmin=' docs/COMMISSIONING.md README.md`):
   COMMISSIONING 165/175/329 (recovery=0ms recmin=0), 350/364/525 (8200ms recmin=0),
   434 (recmin=2 ← DEFECTIVE), 436 (→ 0 ← DEFECTIVE); README:51 (correct, incl.
   "truncated: 599999ms → 9"). Exactly two defects repo-wide — matches bugfix
   architecture system_context.md §3/§5.
4. **Unique anchors for surgical edit**: `recovery=8200ms recmin=2\`` occurs only on
   L434; `599999 ms → 0;` occurs only on L436 (sole 599999 in the file).
5. **Ownership fences**: README.md:49 (BTN short 1234ms) = BUG-002 → sibling task
   P1.M1.T2.S1. tasks.json = orchestrator-owned. spec/SPEC.md, firmware read-only.

## Verification recipe (mechanical, from system_context.md §6)

- `grep -n 'recmin=' docs/COMMISSIONING.md README.md` + floor-check that every
  printed recmin == floor(recovery_ms/60000) of its own line, and every prose
  599999 claim says → 9. Embeded as an executable python heredoc in the PRP
  (Validation Level 2), run before (red: 2 DEFECTIVE) and after (green: ALL OK).
- `git diff --stat` must show exactly 1 file, +2/−2.

## Sources

- plan/001_53984f832581/bugfix/001_db6d94af3d0d/architecture/system_context.md (§2, §3, §5, §6, §7)
- docs/COMMISSIONING.md L430–453 (live state)
- firmware/jug_filler/jug_filler.ino L1343–1360 (live state)
- Bug report BUG-001 (selected PRD content): location, repro, correct values
