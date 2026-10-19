# Research Notes — P1.M3.T3.S1: TripDetector (pure)

## 1. Codebase contracts consumed (verified in-file this session)

- `firmware/jug_filler/config.h` §5 group (read 2025-10-19, lines 51–59):
  - `constexpr uint16_t TRIP_DROP_COUNTS = 350;` (wet threshold = baseline − this, PRD §7.3)
  - `constexpr uint16_t DEBOUNCE_COUNT = 8;` (consecutive wet readings to accept a trip)
  - `BASELINE_MIN = 2000`, `BASELINE_MAX = 4050` (validated-baseline domain)
- `firmware/jug_filler/jug_filler.ino` (394 lines at research time):
  - UNGUARDED region: lines 40–264; ARDUINO region marker at line 265.
  - `ReadingAggregator` (line 186) emits averaged `uint16_t` pairs via `takeReading(nowMs, a1, a2)` — the feed source. `SENSOR_COUNT = 2`.
  - Aggregator end-banner (line 263) already names this task as consumer.
  - **BaselineCapture NOT yet landed** (P1.M3.T2.S1 status: Implementing at research time). Its PRP is the contract: `baseline(uint8_t sensor) -> uint16_t` (median counts, valid after `done()`, verdict Ok implies 2000..4050). TripDetector consumes only the VALUE — zero code dependency, no name collisions (verified by compile).
- Test conventions (`test/test_reading/test_reading.cpp`, 16 tests): relative `.ino` include, empty setUp/tearDown, derivation comments, `ITEM CASE` tags, explicit `int main()` Unity runner (required on PIO 6.2.0).

## 2. Design decisions (binding for this PRP)

1. **Interface**: item contract says `void feed(uint16_t avg); bool tripped();`. A third member is REQUIRED: `void begin(uint16_t baseline)` — "latched once tripped for the remainder of the fill" demands a re-arm point between fills, and the INPUT (baseline from P1.M3.T2.S1) needs an injection point. Mirrors `BaselineCapture::begin` house pattern (default ctor + begin, one long-lived instance per sensor).
2. **Timeless by design**: feed takes NO `nowMs` — the trip rule is purely count-domain (8 consecutive readings). All time-domain logic (TRIP_MIN_ELAPSED_S, EARLY_WET_WINDOW_S) belongs to guards P1.M3.T3.S2; elapsed-based decisions to the engine. This is the cleanest seam split in the chain.
3. **Inclusive threshold**: `avg <= baseline - TRIP_DROP_COUNTS` — "at or below" (PRD §7.3). Exactly baseline−350 qualifies; baseline−349 resets the streak (`streak_ = 0` only on strictly-above).
4. **Latch semantics**: once `streak_ >= DEBOUNCE_COUNT` sets `tripped_ = true`, `feed()` returns immediately — post-trip readings of any value are inert; `tripped()` can never chatter. Cleared only by `begin()`.
5. **No clamp on threshold** (deliberate): begin() must receive a VALIDATED baseline (≥ BASELINE_MIN = 2000 > 350). An unvalidated baseline < 350 wraps mod-2^16 to a huge threshold → never trips → backstop fault 3 (PRD §7.7) — the fail-safe direction. Clamping to 0 would trip-on-anything = unsafe.
6. **Streak counter**: `uint8_t` (max 8); saturates at latch. State footprint 4 bytes/instance.
7. **One instance per sensor** (item contract): sensor independence is structural — no shared state, no sensor index inside the class. The engine owns the index↔instance mapping (0 == fault code 1, 1 == fault code 2).
8. **No new config constants** (§8 frozen; both tunables already exist by name). Classes get NO prototype-block entry (ButtonClassifier/ReadingAggregator/BaselineCapture precedent).

## 3. Machine verification (2025-10-19, this session)

Merged-copy method: real .ino + the PRP's verbatim class block inserted after the aggregator end-banner (BaselineCapture absent — irrelevant, no code dependency); PRP's verbatim test file with only the include line pointed at the merged copy.

```
g++ -std=c++17 -Wall -Wextra -Werror -I<firmware/jug_filler> -I<.pio/libdeps/native/Unity/src> \
    test_trip.cpp unity.o -o verify && ./verify
→ 14 Tests 0 Failures 0 Ignored — OK
```

Expected-value table (all pinned by tests):
| baseline | threshold (= baseline−350) | behavior |
|---|---|---|
| 3400 | 3050 | 3050 qualifies ×8 → trip; 3051 never; 7× then 3051 resets |
| 2000 (BASELINE_MIN) | 1650 | 1650 ×8 → trip (no wrap: 2000 > 350) |
| 2400 (re-arm) | 2050 | 3050 no longer qualifies; 2050 ×8 → trip |

## 4. External references

- https://docs.platformio.org/en/stable/plus/unit-testing.html — PIO native Unity: test-dir auto-discovery, explicit runner on PIO 6.2.0.
- https://ganssle.com/debouncing.pdf — Ganssle, "A Guide to Debouncing": the consecutive-sample counter (integrator) as the robust debounce form; count-domain streak is the same structure applied to an ADC threshold instead of a switch level.

## 5. Downstream API freeze (public surface, 3 members + default ctor)

- `void begin(uint16_t baseline)` — arm per fill (engine P1.M6.T1.S2 calls with `BaselineCapture::baseline(s)` after verdict Ok)
- `void feed(uint16_t avg)` — one averaged reading per emitted pair
- `bool tripped() const` — latched flag; consumed by P1.M3.T3.S2 (guards) and P1.M6.T1.S2 (valve close → CONFIRM)

## 6. Sequencing / scope safety

- PRECONDITION: P1.M3.T2.S1 (BaselineCapture) must be LANDED before implementation starts — verify by grep, STOP if missing.
- Insert anchor (by TEXT, not line number): AFTER `// ========= end baseline capture (consumed by P1.M3.T3.S1, P1.M6.T1.S2, P1.M6.T1.S3) =========`, BEFORE `// ---- #ifdef ARDUINO REGION: hardware glue ----`.
- No parallel tasks touch the UNGUARDED region at implementation time (T2.S1 completes first; T3.S2 is strictly after). config.h, platformio.ini, prototype block, ARDUINO region: byte-stable.
