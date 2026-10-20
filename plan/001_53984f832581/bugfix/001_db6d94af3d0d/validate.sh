#!/usr/bin/env bash
# validate.sh — comprehensive validation for the jug-filler firmware repo.
#
# Phases (only those that exist for this codebase):
#   1. Toolchain check            (pio, python3, g++)
#   2. Compile/type check         (pio run -e pico — the embedded build IS the type check)
#   3. Unit testing               (pio test -e native — 16 Unity suites)
#   4. Structural safety          (test/check_setup_first_line.py — §10 invariants)
#   5. Doc-conformance regression (BUG-001/BUG-002 example-value checks vs formatter)
#   6. Bench evidence integrity   (docs/bench/ package: statuses, vocabulary, no fabrication)
#   7. E2E user-journey simulation (independent g++ harness driving FillEngine through
#      the SPEC §5.2 user flows, fault paths, +5 stacking, backstop expiry and the
#      millis() rollover — mirroring the workflows documented in README/COMMISSIONING)
#
# Exits 0 iff every check passes. Temporary artifacts live in a mktemp dir;
# the repository is never modified.

set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
FAILURES=0
PHASE_FAIL=""

note()  { printf '%s\n' "$*"; }
pass()  { printf '  [PASS] %s\n' "$*"; }
fail()  { printf '  [FAIL] %s\n' "$*"; FAILURES=$((FAILURES+1)); PHASE_FAIL="$PHASE_FAIL $1"; }
phase() { printf '\n== %s ==\n' "$*"; }

# ---------------------------------------------------------------- 1. toolchain
phase "Phase 1: toolchain"
command -v pio      >/dev/null 2>&1 && pass "pio found"      || fail 1 "pio (PlatformIO CLI) not found"
command -v python3  >/dev/null 2>&1 && pass "python3 found"  || fail 1 "python3 not found"
command -v g++      >/dev/null 2>&1 && pass "g++ found"      || fail 1 "g++ not found"
[ "$FAILURES" -eq 0 ] || { note "toolchain incomplete — aborting"; exit 1; }

# ------------------------------------------------------- 2. compile/type check
phase "Phase 2: compile + type check (pio run -e pico)"
if pio run -e pico >/tmp/validate_pico_build.log 2>&1; then
  pass "RP2040 (pico env) build compiles clean"
else
  fail 2 "RP2040 (pico env) build failed — see /tmp/validate_pico_build.log"
  tail -20 /tmp/validate_pico_build.log
fi

# --------------------------------------------------------------- 3. unit tests
phase "Phase 3: unit tests (pio test -e native)"
pio test -e native > /tmp/validate_native_tests.log 2>&1
rc=$?
PASSED=$(grep -cE '\[PASSED\]$' /tmp/validate_native_tests.log || true)
FAILED=$(grep -cE '\[FAILED\]$' /tmp/validate_native_tests.log || true)
SUITES=$(grep -cE '^native +test_' /tmp/validate_native_tests.log || true)
if [ $rc -eq 0 ] && [ "$FAILED" -eq 0 ] && [ "$PASSED" -ge 200 ]; then
  pass "native Unity suite: $SUITES suites, $PASSED tests passed, $FAILED failed"
else
  fail 3 "native test run unhealthy (rc=$rc passed=$PASSED failed=$FAILED) — see /tmp/validate_native_tests.log"
  grep -E 'FAILED|Error' /tmp/validate_native_tests.log | head -10
fi

# ----------------------------------------------------- 4. structural §10 safety
phase "Phase 4: structural safety invariants"
if python3 "$ROOT/test/check_setup_first_line.py"; then
  pass "check_setup_first_line.py"
else
  fail 4 "check_setup_first_line.py reported violations"
fi

# --------------------------------------------- 5. doc-conformance regressions
phase "Phase 5: documentation example-value conformance (BUG-001/BUG-002 class)"
# 5a. Every "recovery=<N>ms recmin=<M>" example must satisfy M == N/60000 (truncation).
BAD=0
while IFS= read -r pair; do
  n=$(printf '%s' "$pair" | sed -n 's/recovery=\([0-9]*\)ms recmin=.*/\1/p')
  m=$(printf '%s' "$pair" | sed -n 's/recovery=[0-9]*ms recmin=\([0-9]*\)/\1/p')
  [ -z "$n" ] && continue
  expect=$(( n / 60000 ))
  if [ "$m" != "$expect" ]; then
    fail 5 "inconsistent example: recovery=${n}ms must print recmin=$expect, doc says recmin=$m"
    BAD=1
  fi
done < <(grep -hoE 'recovery=[0-9]+ms recmin=[0-9]+' \
             "$ROOT/README.md" "$ROOT/docs/COMMISSIONING.md" "$ROOT/docs/bench/"*.md 2>/dev/null | sort -u)
[ "$BAD" -eq 0 ] && pass "all recovery/recmin example pairs are mutually consistent"

# 5b. The truncation note must document the correct boundary value (599999 -> 9).
if grep -qE '599999 ?m?s ?(→|->) ?9' "$ROOT/README.md" && grep -qE '599999 ?m?s ?(→|->) ?9' "$ROOT/docs/COMMISSIONING.md"; then
  pass "599999 ms -> 9 truncation note present and correct in README and runbook"
else
  fail 5 "missing/incorrect 599999 ms -> 9 truncation note"
fi

# 5c. BTN examples: "short" holds must be < BUTTON_LONG_PRESS_MS (1000), "long" >= 1000.
BAD=0
while IFS= read -r ex; do
  kind=$(printf '%s' "$ex" | sed -n 's/BTN \(short\|long\) [0-9]*ms/\1/p')
  ms=$(printf '%s' "$ex"  | sed -n 's/BTN \(short\|long\) \([0-9]*\)ms/\2/p')
  [ -z "$ms" ] && continue
  case "$kind" in
    short) [ "$ms" -lt 1000 ] || { fail 5 "impossible example '$ex' (short requires <1000ms)"; BAD=1; } ;;
    long)  [ "$ms" -ge 1000 ] || { fail 5 "impossible example '$ex' (long requires >=1000ms)"; BAD=1; } ;;
  esac
done < <(grep -hoE 'BTN (short|long) [0-9]+ms' \
             "$ROOT/README.md" "$ROOT/docs/COMMISSIONING.md" "$ROOT/docs/bench/"*.md 2>/dev/null | sort -u)
[ "$BAD" -eq 0 ] && pass "all BTN short/long example holds are reachable"

# 5d. README serial-log table documents the normative log families.
if grep -q '^| `JugFiller boot' "$ROOT/README.md" \
   && grep -q '^| `FILL start' "$ROOT/README.md" \
   && grep -q '^| `FAULT backstop' "$ROOT/README.md"; then
  pass "README documents the normative log families"
else
  fail 5 "README serial-log table missing normative rows"
fi

# 5e. Documentation links resolve.
for link in "docs/bench/README.md" "docs/COMMISSIONING.md" "firmware/jug_filler/config.h"; do
  [ -e "$ROOT/$link" ] && pass "link target exists: $link" || fail 5 "broken doc link: $link"
done

# ------------------------------------------------- 6. bench evidence integrity
phase "Phase 6: bench evidence package integrity (BUG-003 deliverable shape)"
BENCH="$ROOT/docs/bench"
[ -f "$BENCH/README.md" ] && pass "docs/bench/README.md index exists" || fail 6 "docs/bench/README.md missing"
# 6a. Session index rows A..E present with allowed status vocabulary.
for s in A B C D E; do
  grep -qE "^\| $s \|" "$BENCH/README.md" && pass "session row $s in index" || fail 6 "session row $s missing from index"
done
BAD=0
while IFS='|' read -r _ _ _ _ _ _ _ status _; do
  st="$(printf '%s' "$status" | xargs)"
  case "$st" in
    PENDING|PASS|PARTIAL*|BLOCKED:*|SKIPPED:*) ;;
    "") ;;
    *) fail 6 "illegal status vocabulary in session index: '$st'"; BAD=1 ;;
  esac
done < <(grep -E '^\| [A-E] \|' "$BENCH/README.md")
[ "$BAD" -eq 0 ] && pass "session statuses use the allowed vocabulary"
# 6b. Integrity: BLOCKED rows claiming "no capture" must not have fabricated .log files.
NLOGS=$(find "$BENCH" -name '*.log' 2>/dev/null | wc -l)
BLOCKED_ROWS=$(grep -cE '^\| [A-E] \|.*BLOCKED' "$BENCH/README.md")
if [ "$NLOGS" -eq 0 ] && [ "$BLOCKED_ROWS" -gt 0 ]; then
  pass "no fabricated .log captures alongside BLOCKED rows ($BLOCKED_ROWS rows blocked, 0 log files)"
else
  [ "$NLOGS" -eq 0 ] || fail 6 "$NLOGS .log file(s) present — verify genuineness per bench README §1"
fi
# 6c. Discrepancy ledger exists.
ls "$BENCH"/20*-discrepancies.md >/dev/null 2>&1 && pass "discrepancy ledger exists" || fail 6 "discrepancy ledger missing"
# 6d. No leftover watchdog bench patch in firmware.
if grep -rn "BENCH_HANG_TEST" "$ROOT/firmware/" >/dev/null 2>&1; then
  fail 6 "leftover BENCH_HANG_TEST patch in firmware/"
else
  pass "no leftover BENCH_HANG_TEST watchdog patch in firmware/"
fi

# ------------------------------------------- 7. E2E user-journey simulation
phase "Phase 7: E2E user-journey simulation (independent g++ harness)"
TMP="$(mktemp -d /tmp/jfvalidate.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/e2e.cpp" <<'HARNESS'
// Independent E2E harness: drives the PURE fill engine exactly like the loop()
// glue does (one step() per pass, injected clock, averaged pairs at 100 ms
// cadence) through the user journeys documented in README / SPEC §5.2 / §7.
#include "jug_filler.ino"
#include <string>
#include <cstdio>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (cond) printf("  [PASS] %s\n", msg); \
                              else { printf("  [FAIL] %s\n", msg); ++g_fail; } } while (0)

static bool hasLine(const FillEngineOutputs& o, const char* want) {
    char b[96];
    for (uint8_t i = 0; i < o.logCount; ++i) {
        formatFillLogEvent(b, sizeof b, o.logs[i]);
        if (std::string(b) == want) return true;
    }
    return false;
}
// Advance one 100 ms "loop pass" with a reading emission (mirrors loop glue).
static FillEngineOutputs tick(FillEngine& e, uint32_t& t, uint16_t a1, uint16_t a2,
                              ButtonClassifier::Event ev = ButtonClassifier::Event::None) {
    t += 100;
    return e.step(t, ev, true, a1, a2);
}
// Step until the engine leaves `st` (or safety-caps the iteration count).
static FillEngineOutputs runWhile(FillEngine& e, uint32_t& t, FillState st,
                                  uint16_t a1, uint16_t a2, uint32_t maxIters = 50000000) {
    FillEngineOutputs o{};
    uint32_t iters = 0;                    // iteration cap: a t+capMs stamp would wrap near 2^32
    while (e.state() == st && iters++ < maxIters) o = tick(e, t, a1, a2);
    return o;
}
// Step until the engine both is in Filling and has the valve open.
static FillEngineOutputs runToValveOpen(FillEngine& e, uint32_t& t, uint16_t a1, uint16_t a2) {
    FillEngineOutputs o{};
    while (!(e.state() == FillState::Filling && e.valveOpen())) o = tick(e, t, a1, a2);
    return o;
}

int main() {
    const uint16_t DRY = 3500, WET = 3150;  // wet threshold = 3500-350 = 3150 (inclusive)

    // ---- backstop table probe (SPEC §6.2 knots + floor interpolation) ----
    CHECK(backstopMinutes(0) == 44 && backstopMinutes(2) == 44, "table clamps low (<=2 -> 44)");
    CHECK(backstopMinutes(6) == 42,  "interp 6 min -> 42 (floored)");
    CHECK(backstopMinutes(10) == 41 && backstopMinutes(20) == 38 && backstopMinutes(35) == 34,
          "knot values exact (10->41, 20->38, 35->34)");
    CHECK(backstopMinutes(47) == 32, "interp 47 min -> 32 (floored)");
    CHECK(backstopMinutes(60) == 30 && backstopMinutes(1000) == 30, "table clamps high (>=60 -> 30)");

    // ---- formatter probe: recmin truncation (BUG-001 class) ----
    {   FillLogEvent e{FillLogEvent::Kind::FillStart, 0, 41, 599999}; char b[96];
        formatFillLogEvent(b, sizeof b, e);
        CHECK(std::string(b) == "FILL start backstop=41min recovery=599999ms recmin=9",
              "formatter: 599999 ms -> recmin=9"); }
    {   FillLogEvent e{FillLogEvent::Kind::FillStart, 0, 44, 8200}; char b[96];
        formatFillLogEvent(b, sizeof b, e);
        CHECK(std::string(b) == "FILL start backstop=44min recovery=8200ms recmin=0",
              "formatter: 8200 ms -> recmin=0"); }

    // ================= Journey 1: SPEC §5.2 Flow 2 (successful fill) =========
    {
        FillEngine e; uint32_t t = 400;
        auto o = e.step(500, ButtonClassifier::Event::ShortPress, false, DRY, DRY);  // press 1
        CHECK(hasLine(o, "FILL start backstop=30min recovery=0ms recmin=0"),
              "flow2: fresh boot grants 30 min (recovery=0)");
        CHECK(o.led == LedPatternEngine::Mode::Solid, "flow2: LED solid on fill start");
        o = runToValveOpen(e, t, DRY, DRY);                    // settle + baseline (valve @2900)
        CHECK(hasLine(o, "BASE ok s1=3500 s2=3500") && hasLine(o, "VALVE open budget=30min"),
              "flow2: baselines accepted, valve opens with 30 min budget");
        CHECK(o.valveOpen, "flow2: valve level HIGH in valve-open sub-phase");
        while (t < 500 + 2400 + 59200) o = tick(e, t, DRY, DRY);  // dry through fill second 59.2
        (void)o;
        o = runWhile(e, t, FillState::Filling, WET, DRY);        // 8 wet readings: trip at 60000
        CHECK(hasLine(o, "VALVE close") && hasLine(o, "TRIP sensor=1 fillms=60000"),
              "flow2: first trip at exactly 60.000 s closes valve (legal boundary)");
        CHECK(o.logs[0].kind == FillLogEvent::Kind::ValveClosed, "flow2: VALVE close is logs[0] (§7.9)");
        CHECK(e.state() == FillState::Confirm && !o.valveOpen, "flow2: CONFIRM entered, valve stays closed");
        o = runWhile(e, t, FillState::Confirm, WET, WET);        // second sensor trips at +800 ms
        CHECK(hasLine(o, "CONFIRM pass sensor=2 durms=800"),
              "flow2: second sensor confirmed in window (durms=800)");
        CHECK(hasLine(o, "DONE attemptms=62400 openms=60000"),
              "flow2: DONE attemptms=62400 openms=60000");
        CHECK(e.state() == FillState::Done && o.led == LedPatternEngine::Mode::DonePulse,
              "flow2: DONE state with attention-pulse LED");
        o = e.step(t + 100, ButtonClassifier::Event::ShortPress, false, WET, WET);  // press 2: ack
        CHECK(hasLine(o, "ACK done") && e.state() == FillState::Idle,
              "flow2: short press acknowledges DONE -> IDLE");
        o = e.step(t + 200, ButtonClassifier::Event::ShortPress, false, DRY, DRY);  // press 3
        CHECK(hasLine(o, "FILL start backstop=44min recovery=1000ms recmin=0"),
              "flow2: press 3 starts the next fill (recovery measured from valve close)");
    }

    // ================= Journey 2: SPEC §5.2 Flow 1 (abort + restart) =========
    {
        FillEngine e; uint32_t t = 900;
        e.step(1000, ButtonClassifier::Event::ShortPress, false, DRY, DRY);  // press 1: start
        {   auto o = e.step(3000, ButtonClassifier::Event::ShortPress, true, DRY, DRY);  // press 2: abort during settle
            CHECK(hasLine(o, "VALVE close") && hasLine(o, "ABORT attemptms=2000 openms=0"),
                  "flow1: settle abort -> openms=0 (valve never opened)");
            CHECK(e.state() == FillState::Idle && !o.valveOpen, "flow1: aborted back to IDLE"); }
        {   auto o = e.step(3500, ButtonClassifier::Event::ShortPress, false, DRY, DRY);  // press 3: restart
            CHECK(hasLine(o, "FILL start backstop=44min recovery=500ms recmin=0"),
                  "flow1: abort restarted the recovery clock (500ms -> table clamp 44 min)"); }
        t = 3500;
        auto o = runToValveOpen(e, t, DRY, DRY);                // restart reaches valve open
        CHECK(hasLine(o, "VALVE open budget=44min"), "flow1: restart grants 44 min budget");
        while (t < 8000) o = tick(e, t, DRY, DRY);
        (void)o;
        o = e.step(t, ButtonClassifier::Event::ShortPress, true, DRY, DRY);  // abort mid-fill
        CHECK(hasLine(o, "ABORT attemptms=4500 openms=2100"), "flow1: mid-fill abort -> openms=2100");
    }

    // ================= Journey 3: baseline plausibility fault (§7.1) =========
    {
        FillEngine e; uint32_t t = 0;
        e.step(0, ButtonClassifier::Event::ShortPress, false, 1500, DRY);
        auto o = runWhile(e, t, FillState::Filling, 1500, DRY);
        CHECK(hasLine(o, "FAULT baseline sensor=1 code=1 count=1500"),
              "fault: wet sensor 1 refused at baseline (count logged)");
        CHECK(!hasLine(o, "VALVE open") && e.state() == FillState::Fault && e.faultCode() == 1,
              "fault: valve NEVER opened; FAULT latched code 1");
        CHECK(o.led == LedPatternEngine::Mode::FaultBlink && o.faultCode == 1, "fault: FaultBlink LED code 1");
        o = e.step(t + 100, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        CHECK(e.state() == FillState::Fault && o.logCount == 0,
              "fault: BOOT ignored while faulted (latched until RESET)");
    }

    // ================= Journey 4: early-wet / early-trip guards (§7.6) ========
    {
        FillEngine e; uint32_t t = 0;                            // wet INSIDE the 5 s window
        e.step(0, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        runToValveOpen(e, t, DRY, DRY);                          // valve @2400
        while (t < 2400 + 4800) tick(e, t, DRY, DRY);
        auto o = runWhile(e, t, FillState::Filling, WET, DRY);   // first wet at fill time 4900
        CHECK(hasLine(o, "FAULT earlywet sensor=1 code=1 fillms=4900"),
              "guard: wet at 4.900 s -> early-wet fault code 1");
    }
    {
        FillEngine e; uint32_t t = 0;                            // wet at exactly 5.000 s: legal
        e.step(0, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        runToValveOpen(e, t, DRY, DRY);                          // valve @2400
        while (t < 2400 + 4900) tick(e, t, DRY, DRY);
        auto o = runWhile(e, t, FillState::Filling, WET, DRY);   // wet from 5000; trip latches at 5700
        CHECK(hasLine(o, "FAULT earlytrip sensor=1 code=1 fillms=5700"),
              "guard: legal wet at 5.000 s, trip at 5.700 s -> early-trip fault");
    }

    // ================= Journey 5: confirm-window timeout (§7.5) ==============
    {
        FillEngine e; uint32_t t = 400;
        e.step(500, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        runToValveOpen(e, t, DRY, DRY);
        while (t < 500 + 2400 + 59200) tick(e, t, DRY, DRY);
        auto o = runWhile(e, t, FillState::Filling, WET, DRY);   // trip 1 at 60 s
        o = runWhile(e, t, FillState::Confirm, WET, DRY);        // sensor 2 stays dry -> timeout
        CHECK(hasLine(o, "FAULT confirm sensor=2 code=2 durms=12000"),
              "fault: no second trip in 12.000 s window -> confirm timeout code 2");
        CHECK(o.logs[0].kind == FillLogEvent::Kind::ValveClosed, "fault: valve-close-first ordering on confirm timeout");
    }

    // ================= Journey 6: +5 stacking and backstop expiry (§5.1/§7.7)
    {
        FillEngine e; uint32_t t = 0;
        e.step(0, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        runToValveOpen(e, t, DRY, DRY);                          // valve @2400, budget 30 min
        auto o = e.step(5000, ButtonClassifier::Event::LongPress, true, DRY, DRY);
        CHECK(hasLine(o, "PLUS5 budget=35min") && o.ackBlink && o.plus5Applied,
              "plus5: first long press -> 35 min + ack blink");
        o = e.step(5100, ButtonClassifier::Event::LongPress, true, DRY, DRY);
        CHECK(hasLine(o, "PLUS5 budget=40min"), "plus5: stacks -> 40 min");
        o = runWhile(e, t, FillState::Filling, DRY, DRY);        // run to the 40-min deadline
        CHECK(hasLine(o, "FAULT backstop code=3 budget=40min openms=2400000ms"),
              "fault: backstop expiry code 3, budget incl. +5, openms=2400000");
        CHECK(e.state() == FillState::Fault && e.faultCode() == 3, "fault: code 3 latched");
    }

    // ================= Journey 7: fill spanning the millis() rollover ========
    {
        const uint32_t WRAP = 0xFFFFFFFFu;
        FillEngine e; uint32_t t = WRAP - 63000u;                // press ~63 s before the wrap
        e.step(t, ButtonClassifier::Event::ShortPress, false, DRY, DRY);
        runToValveOpen(e, t, DRY, DRY);                          // valve @WRAP-60600
        CHECK(e.valveOpen(), "rollover: valve opens before the wrap");
        while (t < WRAP - 100) tick(e, t, DRY, DRY);             // dry run up to the wrap
        auto o = runWhile(e, t, FillState::Filling, WET, DRY);   // wet streak starts AT the wrap, trips past it
        CHECK(hasLine(o, "TRIP sensor=1 fillms=61300"), "rollover: trip fillms exact across the wrap");
        o = runWhile(e, t, FillState::Confirm, WET, WET);
        CHECK(hasLine(o, "DONE attemptms=63700 openms=61300"),
              "rollover: DONE attempt/open ms exact across the wrap");
        CHECK(hasLine(o, "CONFIRM pass sensor=2 durms=800"), "rollover: confirm duration exact across the wrap");
    }

    printf("\nE2E harness: %d failure(s)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
HARNESS
if g++ -std=c++17 -Wall -I "$ROOT/firmware/jug_filler" "$TMP/e2e.cpp" -o "$TMP/e2e" 2> "$TMP/e2e_build.log"; then
  pass "E2E harness compiled (seam contract holds)"
  if "$TMP/e2e" > "$TMP/e2e_run.log" 2>&1; then
    pass "E2E user-journey simulation: all journeys behave per SPEC §5.2/§6/§7"
    NP=$(grep -c '\[PASS\]' "$TMP/e2e_run.log"); note "  ($NP E2E assertions passed)"
  else
    fail 7 "E2E user-journey simulation reported failures"
    grep -E 'FAIL' "$TMP/e2e_run.log"
  fi
else
  fail 7 "E2E harness failed to compile — see $TMP/e2e_build.log"
  head -20 "$TMP/e2e_build.log"
fi

# ------------------------------------------------------------------ verdict
printf '\n==================== VALIDATION SUMMARY ====================\n'
if [ "$FAILURES" -eq 0 ]; then
  note "ALL CHECKS PASSED"
  exit 0
else
  note "FAILURES: $FAILURES (phases:$PHASE_FAIL)"
  exit 1
fi
