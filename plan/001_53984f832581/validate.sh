#!/usr/bin/env bash
# =============================================================================
# validate.sh — comprehensive validation for the jug-filler firmware
#
# Phases:
#   1  Toolchain & repo sanity
#   2  Structural safety invariants (PRD §10)
#   3  config.h contract conformance vs PRD §8 (compiled static_asserts)
#   4  Native unit tests (Unity, `pio test -e native`)
#   5  Device compile (arduino-pico, `pio run -e pico`)
#   6  E2E user-journey simulation — an INDEPENDENT harness (not the repo's
#      test suite) that compiles the real sketch logic on the host and drives
#      the documented workflows: PRD §5.2 Flow 1 & Flow 2, all fault paths,
#      backstop table behavior, +5 stacking, LED pattern timings, button
#      semantics, and a seeded randomized invariant fuzz.
#   7  Documentation & plan bookkeeping drift checks
#
# Exit code: number of failed checks (capped at 125); WARNs do not fail.
# =============================================================================
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP="$(mktemp -d /tmp/jugfill-validate.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

PASS=0; FAIL=0; WARN=0
phase() { echo; echo "=========== PHASE $1: $2 ==========="; }
ok()   { echo "  [PASS] $1"; PASS=$((PASS+1)); }
bad()  { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
warn() { echo "  [WARN] $1"; WARN=$((WARN+1)); }

# ---------------------------------------------------------------------------
phase 1 "Toolchain & repo sanity"
for t in pio g++ python3 git; do
  if command -v "$t" >/dev/null 2>&1; then ok "$t present ($(command -v $t))"; else bad "$t NOT FOUND"; fi
done
for f in firmware/jug_filler/jug_filler.ino firmware/jug_filler/config.h \
         platformio.ini README.md docs/COMMISSIONING.md spec/SPEC.md; do
  if [ -f "$ROOT/$f" ]; then ok "$f exists"; else bad "$f MISSING"; fi
done
SKETCH_FILES=$(find "$ROOT/firmware" -type f | wc -l)
if [ "$SKETCH_FILES" -eq 2 ]; then ok "two-file firmware constraint (PRD §3): $SKETCH_FILES files under firmware/"; else bad "firmware/ has $SKETCH_FILES files (PRD §3 requires exactly 2)"; fi
grep -q "env:pico" "$ROOT/platformio.ini" && grep -q "env:native" "$ROOT/platformio.ini" \
  && ok "platformio.ini defines pico + native envs" || bad "platformio.ini envs incomplete"

# ---------------------------------------------------------------------------
phase 2 "Structural safety invariants (PRD §10)"
if python3 "$ROOT/test/check_setup_first_line.py" >"$TMP/struct.log" 2>&1; then
  ok "structural check: $(cat "$TMP/struct.log")"
else
  bad "structural check FAILED:"; sed 's/^/    /' "$TMP/struct.log"
fi
# Independent re-checks (do not trust only the repo's own script)
INO="$ROOT/firmware/jug_filler/jug_filler.ino"
SETUP_BODY=$(awk '/^void setup\(\) \{/,/^\}/' "$INO" | sed 's://.*$::' | grep -vE '^\s*$')
FIRST=$(echo "$SETUP_BODY" | sed -n '2p' | tr -d ' ')
if [ "$FIRST" = "digitalWrite(PIN_VALVE,LOW);" ]; then ok "setup() line 1 is digitalWrite(PIN_VALVE, LOW)"; else bad "setup() first statement is: $FIRST"; fi
LOOP_FIRST=$(awk '/^void loop\(\) \{/,/^\}/' "$INO" | sed 's://.*$::' | grep -vE '^\s*$' | sed -n '2p' | tr -d ' ')
if [ "$LOOP_FIRST" = "watchdog_update();" ]; then ok "loop() first statement is watchdog_update()"; else bad "loop() first statement is: $LOOP_FIRST"; fi
if grep -q "watchdog_enable((uint32_t)WATCHDOG_TIMEOUT_S \* 1000UL, true)" "$INO"; then ok "watchdog enabled at 8000 ms (< 8388 ms RP2040 ceiling)"; else bad "watchdog_enable call not found/incorrect"; fi
if grep -q "analogReadResolution(ADC_BITS)" "$INO"; then ok "analogReadResolution(12) present (12-bit counts contract)"; else bad "analogReadResolution(ADC_BITS) missing"; fi
if grep -q "const bool pressedRaw = BOOTSEL;" "$INO"; then ok "BOOTSEL sampled as bool object (correct arduino-pico API)"; else bad "BOOTSEL polling pattern not found"; fi

# ---------------------------------------------------------------------------
phase 3 "config.h contract vs PRD §8 (compiled static_asserts)"
cat > "$TMP/config_check.cpp" <<'EOF'
#include "config.h"
#include <cstdio>
static_assert(PIN_SENSOR1 == 26, "PIN_SENSOR1");
static_assert(PIN_SENSOR2 == 27, "PIN_SENSOR2");
static_assert(PIN_VALVE   == 15, "PIN_VALVE");
static_assert(PIN_LED     == 14, "PIN_LED");
static_assert(BUTTON_LONG_PRESS_MS == 1000, "BUTTON_LONG_PRESS_MS");
static_assert(BUTTON_DEBOUNCE_MS   == 40,   "BUTTON_DEBOUNCE_MS");
static_assert(FRESH_BOOT_BACKSTOP_MIN == 30, "FRESH_BOOT_BACKSTOP_MIN");
static_assert(BACKSTOP_TABLE_MIN[0].recoveryMin ==  2 && BACKSTOP_TABLE_MIN[0].backstopMin == 44, "knot0");
static_assert(BACKSTOP_TABLE_MIN[1].recoveryMin == 10 && BACKSTOP_TABLE_MIN[1].backstopMin == 41, "knot1");
static_assert(BACKSTOP_TABLE_MIN[2].recoveryMin == 20 && BACKSTOP_TABLE_MIN[2].backstopMin == 38, "knot2");
static_assert(BACKSTOP_TABLE_MIN[3].recoveryMin == 35 && BACKSTOP_TABLE_MIN[3].backstopMin == 34, "knot3");
static_assert(BACKSTOP_TABLE_MIN[4].recoveryMin == 60 && BACKSTOP_TABLE_MIN[4].backstopMin == 30, "knot4");
static_assert(BACKSTOP_TABLE_LEN == 5, "table length");
static_assert(PLUS5_MIN == 5, "PLUS5_MIN");
static_assert(CONFIRM_WINDOW_S   == 12, "CONFIRM_WINDOW_S");
static_assert(EARLY_WET_WINDOW_S == 5,  "EARLY_WET_WINDOW_S");
static_assert(TRIP_MIN_ELAPSED_S == 60, "TRIP_MIN_ELAPSED_S");
static_assert(ADC_BITS == 12, "ADC_BITS");
static_assert(ADC_SAMPLES_PER_READING == 32, "ADC_SAMPLES_PER_READING");
static_assert(READING_INTERVAL_MS     == 100, "READING_INTERVAL_MS");
static_assert(BASELINE_SETTLE_MS      == 2000, "BASELINE_SETTLE_MS");
static_assert(BASELINE_MIN == 2000, "BASELINE_MIN");
static_assert(BASELINE_MAX == 4050, "BASELINE_MAX");
static_assert(TRIP_DROP_COUNTS == 350, "TRIP_DROP_COUNTS");
static_assert(DEBOUNCE_COUNT == 8, "DEBOUNCE_COUNT");
static_assert(DONE_PULSE_ON_MS  == 200, "DONE_PULSE_ON_MS");
static_assert(DONE_PULSE_OFF_MS == 200, "DONE_PULSE_OFF_MS");
static_assert(DONE_PULSE_COUNT  == 3,   "DONE_PULSE_COUNT");
static_assert(DONE_GAP_MS       == 1000,"DONE_GAP_MS");
static_assert(FAULT_BLINK_MS    == 250, "FAULT_BLINK_MS");
static_assert(FAULT_PAUSE_MS    == 2000,"FAULT_PAUSE_MS");
static_assert(ACK_BLINK_MS      == 100, "ACK_BLINK_MS");
static_assert(USE_ONBOARD_RGB == true, "USE_ONBOARD_RGB");
static_assert(RGB_BRIGHTNESS  == 60,   "RGB_BRIGHTNESS");
static_assert(RGB_BRIGHTNESS  <= 60,   "RGB_BRIGHTNESS cap (PRD 2.2)");
static_assert(SERIAL_BAUD      == 115200, "SERIAL_BAUD");
static_assert(WATCHDOG_TIMEOUT_S == 8, "WATCHDOG_TIMEOUT_S");
int main(){ std::printf("all PRD section-8 constants verified\n"); return 0; }
EOF
if g++ -std=c++17 -I "$ROOT/firmware/jug_filler" -o "$TMP/config_check" "$TMP/config_check.cpp" 2>"$TMP/config_cc.err" \
   && "$TMP/config_check" >"$TMP/config_check.log" 2>&1; then
  ok "$(cat "$TMP/config_check.log")"
else
  bad "config.h contract mismatch:"; sed 's/^/    /' "$TMP/config_cc.err" | head -20
fi

# ---------------------------------------------------------------------------
phase 4 "Native unit tests (Unity)"
cd "$ROOT"
if pio test -e native >"$TMP/native.log" 2>&1; then
  SUM=$(grep -E "test cases:.*succeeded" "$TMP/native.log" | tail -1)
  ok "pio test -e native — ${SUM:-all suites passed}"
else
  bad "pio test -e native FAILED:"; grep -E "FAILED|Error|error" "$TMP/native.log" | head -15 | sed 's/^/    /'
fi

# ---------------------------------------------------------------------------
phase 5 "Device compile (arduino-pico / Waveshare RP2040-Zero)"
cd "$ROOT"
if pio run -e pico >"$TMP/pico.log" 2>&1; then
  SZ=$(grep -A2 "Advanced Memory" "$TMP/pico.log" | grep -E "RAM:|Flash:" | tr '\n' ' ')
  ok "pio run -e pico — SUCCESS — $SZ"
else
  bad "pio run -e pico FAILED:"; grep -iE "error|failed" "$TMP/pico.log" | head -15 | sed 's/^/    /'
fi

# ---------------------------------------------------------------------------
phase 6 "E2E user-journey simulation (independent harness over the real logic)"
cat > "$TMP/e2e.cpp" <<'EOF'
// Independent E2E harness: compiles the REAL sketch logic (unguarded region)
// on the host and drives the PRD's documented user journeys end-to-end.
#include "jug_filler.ino"
#include <cstdio>
#include <vector>

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, msg) do { ++g_checks; if (!(cond)) { std::printf("  E2E-FAIL: %s (line %d)\n", msg, __LINE__); ++g_fail; } } while (0)

// ---------------- full device simulator (mirrors the loop() wiring) --------
struct Sim {
    ReadingAggregator agg;
    ButtonClassifier  btn;
    FillEngine        eng;
    uint32_t t = 0;
    uint16_t s1 = 3500, s2 = 3490;          // dry 12-bit counts
    uint16_t curA1 = 0, curA2 = 0;
    bool pressed = false;
    long valveOpenSteps = 0;
    std::vector<FillLogEvent> events;
    bool sawValveOpenLog = false;

    FillEngineOutputs step() {
        agg.addSample(0, s1, t);
        agg.addSample(1, s2, t);
        bool due = false;
        if (agg.readingDue(t)) { agg.takeReading(t, curA1, curA2); due = true; }
        ButtonClassifier::Event ev = btn.update(t, pressed);
        FillEngineOutputs o = eng.step(t, ev, due, curA1, curA2);
        if (o.valveOpen) ++valveOpenSteps;
        for (uint8_t i = 0; i < o.logCount; ++i) {
            if (o.logs[i].kind == FillLogEvent::Kind::ValveOpen) sawValveOpenLog = true;
            events.push_back(o.logs[i]);
        }
        t += 3;  // 3 ms sim step (matches ADC sample pace)
        return o;
    }
    void run_for(uint32_t ms) { uint32_t end = t + ms; while (t < end) step(); }
    // Press-and-release (raw level), duration in ms; returns after release.
    void press(uint32_t durMs) {
        pressed = true;  run_for(durMs);
        pressed = false; run_for(60);   // release acceptance margin
    }
    bool lastEventIs(FillLogEvent::Kind k) const {
        return !events.empty() && events.back().kind == k;
    }
    int countEvents(FillLogEvent::Kind k) const {
        int n = 0; for (const auto& e : events) if (e.kind == k) ++n; return n;
    }
    const FillLogEvent* findEvent(FillLogEvent::Kind k) const {
        for (const auto& e : events) if (e.kind == k) return &e; return nullptr;
    }
    const FillLogEvent* findLastEvent(FillLogEvent::Kind k) const {
        for (size_t i = events.size(); i-- > 0;) if (events[i].kind == k) return &events[i];
        return nullptr;
    }
};

static void journey_boot_idle() {
    std::printf("  J1: boot -> IDLE, valve closed, LED off, no events\n");
    Sim s; s.run_for(5000);
    CHECK(s.eng.state() == FillState::Idle, "J1 boots into IDLE");
    CHECK(s.valveOpenSteps == 0, "J1 valve never opened");
    FillEngineOutputs o = s.eng.step(s.t, ButtonClassifier::Event::None, false, 3500, 3490);
    CHECK(!o.valveOpen && o.led == LedPatternEngine::Mode::Off && o.faultCode == 0, "J1 levels: valve LOW / LED Off");
    CHECK(s.events.empty(), "J1 no spurious engine events");
}

static void journey_flow1_abort_restart() {
    std::printf("  J2 (PRD 5.2 Flow 1): start -> abort mid-fill -> restart with FRESH RECOMPUTED timer\n");
    Sim s; s.run_for(1000);
    s.press(200);                                       // press 1: start fill
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Filling && s.eng.valveOpen(), "J2 valve opened after baseline (~2.5 s)");
    const FillLogEvent* fs = s.findEvent(FillLogEvent::Kind::FillStart);
    CHECK(fs && fs->a == 30 && fs->b == 0, "J2 fresh-boot backstop = 30 min, recovery = 0");
    CHECK(s.countEvents(FillLogEvent::Kind::ValveOpen) == 1, "J2 exactly one VALVE open");
    s.run_for(8000);                                    // fill for a while
    long openedAt = s.valveOpenSteps;
    s.press(200);                                       // press 2: abort
    CHECK(s.eng.state() == FillState::Idle && !s.eng.valveOpen(), "J2 abort -> IDLE, valve closed");
    CHECK(s.countEvents(FillLogEvent::Kind::Abort) == 1, "J2 ABORT logged once");
    (void)openedAt;
    s.run_for(200);                                     // ~immediate restart
    s.press(200);                                       // press 3: start again
    const FillLogEvent* fs2 = s.findLastEvent(FillLogEvent::Kind::FillStart);
    CHECK(fs2 && fs2->a == 44, "J2 restarted fill recomputed: ~0 min recovery -> 44 min (clamp)");
    CHECK(s.eng.clock().hasHistory(), "J2 abort restarted the recovery clock");
    guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    CHECK(s.eng.valveOpen(), "J2 second fill opened the valve");
}

static void journey_plus5_stacking() {
    std::printf("  J3: FILLING long-press = +5 min, stacking, LED ack, valve stays open\n");
    Sim s; s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    CHECK(s.eng.deadline().budgetMinutes() == 30, "J3 initial budget 30 min");
    s.press(1200);                                      // long press 1
    CHECK(s.eng.deadline().budgetMinutes() == 35, "J3 +5 -> budget 35 min");
    CHECK(s.eng.state() == FillState::Filling && s.eng.valveOpen(), "J3 still FILLING, valve open");
    s.press(1200);                                      // long press 2
    CHECK(s.eng.deadline().budgetMinutes() == 40, "J3 second +5 stacks -> 40 min");
    CHECK(s.countEvents(FillLogEvent::Kind::Plus5) == 2, "J3 two PLUS5 log events");
    // IDLE long-press does nothing (PRD 5.1)
    s.press(200);                                       // abort to IDLE
    CHECK(s.eng.state() == FillState::Idle, "J3 abort to IDLE");
    uint32_t before = s.eng.clock().hasHistory() ? 1 : 0;
    s.press(1200);                                      // IDLE long press: nothing
    CHECK(s.eng.state() == FillState::Idle && before == 1, "J3 IDLE long-press does nothing");
    CHECK(s.countEvents(FillLogEvent::Kind::Plus5) == 2, "J3 no PLUS5 from IDLE");
}

static void journey_flow2_success() {
    std::printf("  J4 (PRD 5.2 Flow 2): fill -> trip -> confirm -> DONE blink -> ack -> next fill\n");
    Sim s; s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    const uint32_t openedAt = s.t;
    // keep dry until 60.5 s of valve-open time (TRIP_MIN_ELAPSED_S), then wet s1
    s.run_for(60500);
    s.s1 = 1500;
    guard = s.t + 5000;
    while (s.eng.state() == FillState::Filling && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Confirm, "J4 first trip -> CONFIRM");
    CHECK(!s.eng.valveOpen(), "J4 valve closed at first trip");
    const uint32_t closedAt = s.t;
    const FillLogEvent* trip = s.findEvent(FillLogEvent::Kind::Trip);
    CHECK(trip && trip->sensor == 0 && trip->a >= 60000 && trip->a < 63000, "J4 TRIP fillms in [60,63) s");
    // second sensor wets 2 s into the 12 s window
    s.run_for(2000); s.s2 = 1500;
    guard = s.t + 5000;
    while (s.eng.state() == FillState::Confirm && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Done, "J4 second trip -> DONE");
    const FillLogEvent* done = s.findEvent(FillLogEvent::Kind::Done);
    CHECK(done && done->b >= 60000, "J4 DONE openms >= 60 s");
    // LED contract through the lifecycle
    // acknowledge so recovery lands at exactly 27 min after the valve-CLOSE instant
    while (s.t < closedAt + 27u * 60u * 1000u) s.step();
    s.press(200);
    CHECK(s.eng.state() == FillState::Idle, "J4 ack -> IDLE");
    CHECK(s.countEvents(FillLogEvent::Kind::Ack) == 1, "J4 ACK logged");
    s.run_for(200);
    s.press(200);                                       // next fill
    const FillLogEvent* fs2 = s.findLastEvent(FillLogEvent::Kind::FillStart);
    // recovery = 27 min exactly -> between knots (20,38) and (35,34): drop=ceil(4*7/15)=2 -> 36
    CHECK(fs2 && fs2->a == 36, "J4 next fill backstop = 36 min @ 27 min recovery (concave interp)");
    CHECK(fs2 && fs2->b / 60000u == 27, "J4 recovery measured from valve-close instant");
    (void)openedAt;
}

static void journey_baseline_fault() {
    std::printf("  J5: wet sensor at start -> baseline fault code 1, valve NEVER opens, latched\n");
    Sim s; s.s1 = 1500;                                 // sensor 1 wet from boot
    s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (s.eng.state() == FillState::Filling && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Fault && s.eng.faultCode() == 1, "J5 FAULT code 1 (sensor 1 baseline)");
    CHECK(!s.sawValveOpenLog && s.valveOpenSteps == 0, "J5 valve NEVER opened");
    const FillLogEvent* f = s.findEvent(FillLogEvent::Kind::BaselineFault);
    CHECK(f && f->sensor == 0 && f->a == 1 && f->b == 1500, "J5 baseline fault payload: code=1 count=1500");
    s.press(200); s.press(1200);                        // BOOT ignored while faulted
    CHECK(s.eng.state() == FillState::Fault, "J5 fault latches, BOOT ignored");
    // fresh instance = reset semantics: IDLE, no history -> 30 min (PRD 5.3)
    FillEngine fresh;
    CHECK(fresh.state() == FillState::Idle && !fresh.clock().hasHistory(), "J5 reset clears fault+history");
}

static void journey_early_wet_fault() {
    std::printf("  J6: wet reading inside 5 s early window -> earlywet fault code 2\n");
    Sim s; s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    s.run_for(2000); s.s2 = 1500;                       // wet at ~2 s of valve-open
    guard = s.t + 5000;
    while (s.eng.state() == FillState::Filling && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Fault && s.eng.faultCode() == 2, "J6 FAULT code 2 (early wet s2)");
    const FillLogEvent* f = s.findEvent(FillLogEvent::Kind::EarlyWetFault);
    CHECK(f && f->sensor == 1 && f->a == 2 && f->b > 0 && f->b < 5000, "J6 earlywet payload: fillms in (0,5) s");
    CHECK(!s.eng.valveOpen(), "J6 valve closed on fault");
}

static void journey_confirm_timeout() {
    std::printf("  J7: first trip only, second never -> confirm-timeout fault code 2 @ 12 s\n");
    Sim s; s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    s.run_for(60500); s.s1 = 1500;                      // only sensor 1 ever wets
    guard = s.t + 5000;
    while (s.eng.state() == FillState::Filling && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Confirm, "J7 entered CONFIRM");
    guard = s.t + 20000;
    while (s.eng.state() == FillState::Confirm && s.t < guard) s.step();
    CHECK(s.eng.state() == FillState::Fault && s.eng.faultCode() == 2, "J7 FAULT code 2 (sensor 2 never tripped)");
    const FillLogEvent* f = s.findEvent(FillLogEvent::Kind::ConfirmTimeout);
    CHECK(f && f->sensor == 1 && f->a == 2 && f->b >= 12000 && f->b <= 12010, "J7 confirm timeout at exactly 12 s");
}

static void journey_backstop_expiry() {
    std::printf("  J8: dry sensors, backstop expires -> fault code 3 (+5 extends it)\n");
    Sim s; s.run_for(1000);
    s.press(200);
    uint32_t guard = s.t + 10000;
    while (!s.eng.valveOpen() && s.t < guard) s.step();
    CHECK(s.eng.deadline().budgetMinutes() == 30, "J8 fresh-boot budget 30 min");
    s.run_for(30u * 60u * 1000u - 2000);                // 29:58 of valve-open
    CHECK(s.eng.state() == FillState::Filling, "J8 still filling at 29:58");
    s.run_for(4000);                                    // cross the 30-min deadline
    CHECK(s.eng.state() == FillState::Fault && s.eng.faultCode() == 3, "J8 FAULT code 3 (timer, no trip)");
    const FillLogEvent* f = s.findEvent(FillLogEvent::Kind::BackstopExpired);
    CHECK(f && f->a == 30, "J8 expiry payload budget=30 min");
    // ... and with two +5 extensions the deadline moves to 40 min
    Sim s2; s2.run_for(1000);
    s2.press(200);
    guard = s2.t + 10000;
    while (!s2.eng.valveOpen() && s2.t < guard) s2.step();
    const uint32_t openedAt2 = s2.t;                   // anchor for the 40-min deadline
    s2.press(1200); s2.press(1200);
    CHECK(s2.eng.deadline().budgetMinutes() == 40, "J8b budget 40 min after two +5s");
    while (s2.t < openedAt2 + 40u * 60u * 1000u - 2000) s2.step();
    CHECK(s2.eng.state() == FillState::Filling, "J8b still filling just before 40-min deadline");
    while (s2.t < openedAt2 + 40u * 60u * 1000u + 4000) s2.step();
    CHECK(s2.eng.state() == FillState::Fault && s2.eng.faultCode() == 3, "J8b FAULT code 3 at ~40 min");
    const FillLogEvent* f2 = s2.findEvent(FillLogEvent::Kind::BackstopExpired);
    CHECK(f2 && f2->a == 40, "J8b expiry payload budget=40 min (incl. +5 stacks)");
}

static void journey_led_patterns() {
    std::printf("  J9: LED pattern timings (PRD 4/7) straight from the pattern engine\n");
    LedPatternEngine e;
    e.setMode(LedPatternEngine::Mode::DonePulse);
    struct { uint32_t t; bool want; } dp[] = {
        {0,1},{199,1},{200,0},{399,0},{400,1},{599,1},{600,0},{799,0},{800,1},{999,1},
        {1000,0},{1199,0},{1200,0},{2199,0},{2200,1},{2399,1},{2400,0},
    };
    for (auto& c : dp) CHECK(e.tick(c.t) == c.want, "J9 DonePulse boundary timing");
    e.setMode(LedPatternEngine::Mode::FaultBlink, 3);
    struct { uint32_t t; bool want; } fb[] = {
        {0,1},{249,1},{250,0},{499,0},{500,1},{749,1},{750,0},{999,0},{1000,1},
        {1249,1},{1250,0},{1499,0},{1500,0},{3499,0},{3500,1},{3749,1},
    };
    for (auto& c : fb) CHECK(e.tick(c.t) == c.want, "J9 FaultBlink(3) boundary timing");
    // ack overlay: 3x(100 on/100 off) over Solid, then solid resumes
    LedPatternEngine a;
    a.setMode(LedPatternEngine::Mode::Solid);
    a.triggerAck();
    struct { uint32_t t; bool want; } ak[] = {
        {0,1},{99,1},{100,0},{199,0},{200,1},{299,1},{300,0},{399,0},{400,1},
        {499,1},{500,0},{599,0},{600,1},{700,1},
    };
    for (auto& c : ak) CHECK(a.tick(c.t) == c.want, "J9 ack overlay then Solid resumes");
    CHECK(!a.ackActive(), "J9 ack one-shot completed");
    // mode change mid-pattern resets phase cleanly
    e.setMode(LedPatternEngine::Mode::Solid);
    e.setMode(LedPatternEngine::Mode::DonePulse);
    CHECK(e.tick(0) == true, "J9 re-setMode restarts pattern at t=0 (LED on)");
    LedPatternEngine o;
    CHECK(o.tick(123456) == false, "J9 default mode Off");
}

static void journey_button_semantics() {
    std::printf("  J10: button classifier (PRD 5.1) at 1 ms resolution\n");
    // short press: 200 ms hold
    {
        ButtonClassifier b; int ev=0;
        for (uint32_t t=0;t<1000;++t){ auto e=b.update(t, t>=100 && t<300); if(e!=ButtonClassifier::Event::None){ev=e;} }
        CHECK(ev==ButtonClassifier::Event::ShortPress, "J10 200 ms hold -> Short on release");
    }
    // exactly 1000 ms hold -> Long ; 999 ms -> Short
    {
        ButtonClassifier b; int ev=0;
        for (uint32_t t=0;t<2000;++t){ auto e=b.update(t, t>=100 && t<1100); if(e!=ButtonClassifier::Event::None){ev=e;} }
        CHECK(ev==ButtonClassifier::Event::LongPress, "J10 1000 ms hold -> Long");
    }
    {
        ButtonClassifier b; int ev=0;
        for (uint32_t t=0;t<2000;++t){ auto e=b.update(t, t>=100 && t<1099); if(e!=ButtonClassifier::Event::None){ev=e;} }
        CHECK(ev==ButtonClassifier::Event::ShortPress, "J10 999 ms hold -> Short");
    }
    // no event while still held
    {
        ButtonClassifier b; int n=0;
        for (uint32_t t=0;t<5000;++t){ auto e=b.update(t, t>=100); if(e!=ButtonClassifier::Event::None)++n; }
        CHECK(n==0, "J10 nothing fires while held");
    }
    // contact bounce: 5 ms spikes ignored
    {
        ButtonClassifier b; int ev=0;
        for (uint32_t t=0;t<1000;++t){
            bool raw = (t>=100&&t<105)||(t>=115&&t<120)||(t>=200&&t<400); // bounce then a real press
            auto e=b.update(t,raw); if(e!=ButtonClassifier::Event::None){ev=e;}
        }
        CHECK(ev==ButtonClassifier::Event::ShortPress, "J10 bounce spikes ignored, one clean Short");
    }
    // back-to-back presses
    {
        ButtonClassifier b; int n=0;
        for (uint32_t t=0;t<3000;++t){
            bool raw=(t>=100&&t<300)||(t>=600&&t<800);
            auto e=b.update(t,raw); if(e!=ButtonClassifier::Event::None)++n;
        }
        CHECK(n==2, "J10 two back-to-back Shorts");
    }
}

static void journey_backstop_table() {
    std::printf("  J11: backstop table knots, clamps, concavity (PRD 6.2)\n");
    CHECK(backstopMinutes(0)==44 && backstopMinutes(1)==44 && backstopMinutes(2)==44, "J11 low clamp <=2 -> 44");
    CHECK(backstopMinutes(10)==41 && backstopMinutes(20)==38 && backstopMinutes(35)==34 && backstopMinutes(60)==30, "J11 all knots exact");
    CHECK(backstopMinutes(120)==30 && backstopMinutes(1440)==30, "J11 high clamp >=60 -> 30");
    CHECK(backstopMinutes(6)==42, "J11 interp 6 min -> 42 (exact 42.5, floored)");
    CHECK(backstopMinutes(15)==39, "J11 interp 15 min -> 39");
    CHECK(backstopMinutes(30)==35, "J11 interp 30 min -> 35");
    uint32_t prev=99;
    for (uint32_t r=0;r<=120;++r){ uint32_t v=backstopMinutes(r); CHECK(v<=prev && v>=30 && v<=44, "J11 monotone non-increasing"); prev=v; }
}

static void journey_fuzz_invariants() {
    std::printf("  J12: seeded randomized invariant fuzz (150 k steps)\n");
    Sim s;
    uint32_t rng=0xDEADBEEFu;
    auto rnd=[&](){ rng=rng*1664525u+1013904223u; return rng>>8; };
    FillState prev=s.eng.state();
    int autoOpens=0, badOrder=0, faultUnlatch=0, badLevels=0, resets=0, fillStarts=0, valveOpenTotal=0;
    uint32_t wetUntil1=0, wetUntil2=0;
    for (long i=0;i<150000;++i){
        if (i>0 && i%25000==0) { s = Sim(); prev = s.eng.state(); wetUntil1 = 0; wetUntil2 = 0; ++resets; }  // power cycle (PRD 5.3)
        // random user behavior: occasionally press/hold/release
        if (!s.pressed && (rnd()%3000)==0) s.pressed=true;
        else if (s.pressed && (rnd()%400)==0) s.pressed=false;
        // random sensor world: mostly dry, occasional wet bursts
        if (s.t>=wetUntil1) s.s1 = (rnd()%4000==0)? 1500 : 3500;
        else s.s1=1500;
        if (s.t>=wetUntil2) s.s2 = (rnd()%4000==0)? 1500 : 3490;
        else s.s2=1500;
        if (s.s1==1500 && s.t>=wetUntil1) wetUntil1=s.t+1500+rnd()%2500;
        if (s.s2==1500 && s.t>=wetUntil2) wetUntil2=s.t+1500+rnd()%2500;
        FillEngineOutputs o=s.step();
        valveOpenTotal += o.valveOpen ? 1 : 0;
        if (o.logCount>0 && o.logs[0].kind==FillLogEvent::Kind::FillStart) ++fillStarts;
        FillState cur=s.eng.state();
        // INV-1: valve high only in FILLING
        if (o.valveOpen && cur!=FillState::Filling) ++autoOpens;
        // INV-2: entering FILLING only from IDLE (auto-open guard is structural; log-check here)
        if (cur==FillState::Filling && prev!=FillState::Idle && prev!=FillState::Filling) ++autoOpens;
        // INV-3: every exit from FILLING/CONFIRM logs ValveClosed first
        if ((prev==FillState::Filling||prev==FillState::Confirm) && cur!=prev){
            if (o.logCount==0 || o.logs[0].kind!=FillLogEvent::Kind::ValveClosed) ++badOrder;
        }
        // INV-4: FAULT latches until reset
        if (prev==FillState::Fault && cur!=FillState::Fault) ++faultUnlatch;
        // INV-5: level coherence
        if ((cur==FillState::Idle   && (o.valveOpen||o.led!=LedPatternEngine::Mode::Off)) ||
            (cur==FillState::Done   && o.led!=LedPatternEngine::Mode::DonePulse) ||
            (cur==FillState::Fault  && (o.led!=LedPatternEngine::Mode::FaultBlink||o.faultCode<1||o.faultCode>3))) ++badLevels;
        prev=cur;
    }
    CHECK(autoOpens==0,  "J12 no path ever opens the valve outside IDLE+press");
    CHECK(badOrder==0,   "J12 every FILLING/CONFIRM exit closes the valve FIRST");
    CHECK(faultUnlatch==0,"J12 faults latch (no spontaneous exit)");
    CHECK(badLevels==0,  "J12 state->valve/LED level coherence");
    std::printf("    (fuzz: %d resets, %d fill starts, %d valve-open steps, end state %d, %zu events)\n",
                resets, fillStarts, valveOpenTotal, (int)s.eng.state(), s.events.size());
}

int main() {
    std::printf("E2E user journeys (PRD sections 4-7, 10):\n");
    journey_boot_idle();
    journey_flow1_abort_restart();
    journey_plus5_stacking();
    journey_flow2_success();
    journey_baseline_fault();
    journey_early_wet_fault();
    journey_confirm_timeout();
    journey_backstop_expiry();
    journey_led_patterns();
    journey_button_semantics();
    journey_backstop_table();
    journey_fuzz_invariants();
    std::printf("\nE2E RESULT: %d checks, %d failures\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
EOF
if g++ -std=c++17 -I "$ROOT/firmware/jug_filler" -o "$TMP/e2e" "$TMP/e2e.cpp" -O1 2>"$TMP/e2e_cc.err"; then
  ok "E2E harness compiled (real sketch logic, host g++)"
  if "$TMP/e2e" >"$TMP/e2e.log" 2>&1; then
    ok "$(grep 'E2E RESULT' "$TMP/e2e.log")"
    grep '^  J' "$TMP/e2e.log" | sed 's/^/    /'
  else
    bad "E2E harness FAILED:"; grep -E "E2E-FAIL" "$TMP/e2e.log" | head -30 | sed 's/^/    /'
    tail -3 "$TMP/e2e.log" | sed 's/^/    /'
  fi
else
  bad "E2E harness did not compile:"; sed 's/^/    /' "$TMP/e2e_cc.err" | head -30
fi

# ---------------------------------------------------------------------------
phase 7 "Documentation & plan bookkeeping"
grep -q "pio run -e pico" "$ROOT/README.md" && grep -q "pio test -e native" "$ROOT/README.md" \
  && ok "README documents both build/test commands" || bad "README build/test commands missing"
grep -q "Waveshare RP2040-Zero" "$ROOT/README.md" \
  && ok "README Arduino IDE board instructions present" || warn "README board instructions not found"
# Drift: README calls docs/COMMISSIONING.md "(planned)" though it exists & is committed
if grep -n "(planned)" "$ROOT/README.md" >/dev/null 2>&1 && [ -f "$ROOT/docs/COMMISSIONING.md" ]; then
  warn "README says docs/COMMISSIONING.md is '(planned)' but the file exists ($(grep -c '' "$ROOT/docs/COMMISSIONING.md") lines, committed)"
fi
# Drift: tasks.json statuses vs implemented evidence
TASKS="$ROOT/plan/001_53984f832581/tasks.json"
if [ -f "$TASKS" ]; then
  N_DONE=$(grep -c '"status": "Complete"' "$TASKS" || true)
  N_TODO=$(grep -c '"status": "Planned"' "$TASKS" || true)
  N_RES=$(grep -c '"status": "Researching"' "$TASKS" || true)
  N_TESTDIRS=$(find "$ROOT/test" -mindepth 1 -maxdepth 1 -type d | wc -l)
  echo "    tasks.json: $N_DONE Complete / $N_TODO Planned / $N_RES Researching; $N_TESTDIRS native test suites exist"
  if grep -q '"status": "Planned"' <(python3 - "$TASKS" <<'PY'
import json,sys
d=json.load(open(sys.argv[1]))
def walk(o):
    if isinstance(o,dict):
        if o.get("type")=="Subtask": yield o
        for v in o.values(): yield from walk(v)
    elif isinstance(o,list):
        for v in o: yield from walk(v)
for s in walk(d):
    if s["id"] in ("P1.M6.T2.S1","P1.M7.T1.S1","P2.M1.T2.S1") and s.get("status")!="Complete":
        print('"status": "Planned"  # '+s["id"]+" is "+s.get("status","?"))
PY
  ); then
    warn "plan tasks.json statuses are stale: implemented+committed work (fill-engine wiring P1.M6.T2.S1, invariant suite P1.M7.T1.S1, commissioning runbook P2.M1.T2.S1) still shows Planned/Researching"
  fi
else
  warn "plan tasks.json not found"
fi
if ! git -C "$ROOT" diff --quiet -- firmware test platformio.ini README.md 2>/dev/null; then
  warn "uncommitted changes in firmware/test/README (working tree differs from HEAD)"
fi

# ---------------------------------------------------------------------------
echo
echo "================= VALIDATION SUMMARY ================="
echo "  checks passed: $PASS"
echo "  checks FAILED: $FAIL"
echo "  warnings:      $WARN"
if [ "$FAIL" -gt 0 ]; then echo "  VERDICT: ISSUES FOUND"; else echo "  VERDICT: ALL CHECKS PASSED (see warnings above, if any)"; fi
echo "======================================================"
[ "$FAIL" -gt 125 ] && exit 125
exit "$FAIL"
