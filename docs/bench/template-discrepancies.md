# Bench discrepancies — <bench day> (expected vs observed)

Copy this template to `YYYY-MM-DD-discrepancies.md` (one per bench day, shared by all
sessions of that day). Every checklist mismatch gets an entry here.

Rules (binding — docs/bench/README.md §1):

- Quote log lines VERBATIM in fenced blocks — copy from the capture; never retype or trim.
- NEVER edit a captured `.log`; corrections and follow-ups live here.
- Each entry carries a follow-up issue number once filed (or `pending`).

## Entries

### D-1 (pre-seeded — to confirm at bench): LED indicator documentation nuance

- Session: A (observe during any session with an LED check)
- Expected (SPEC §4): the onboard WS2812 (GP16) is the sole status/fault indicator.
- Observed (docs/COMMISSIONING.md): describes an external GP14 green LED mirroring the
  patterns; the firmware drives both (GP14 driver + config-gated WS2812 mirror).
- Action: at the bench, record which indicator(s) actually render each pattern; log the
  observation here. Do NOT edit spec/SPEC.md (known nuance — bench evidence package §6).
- Follow-up issue: pending
- Status: to confirm

### D-__ — <title>

- Session: __
- Expected: `<exact expected prefix/line or LED behavior>`
- Observed:

  ```
  <verbatim log excerpt>
  ```

- Follow-up issue: __
- Status: open / filed / resolved-not-here
