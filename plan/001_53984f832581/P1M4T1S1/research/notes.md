# Research Notes — P1.M4.T1.S1: backstopMinutes() piecewise-linear interpolation with clamps (pure)

## §1 Contract → codebase mapping

| Contract element | Codebase fact (verified this session) |
| --- | --- |
| `BACKSTOP_TABLE_MIN` from config.h | `firmware/jug_filler/config.h` §3 "Fill backstop model": `constexpr BackstopTableEntry BACKSTOP_TABLE_MIN[] = { {2,44},{10,41},{20,38},{35,34},{60,30} }` + `constexpr uint8_t BACKSTOP_TABLE_LEN` — the helper's own comment says "helper for P1.M4.T1.S1 iteration". `BackstopTableEntry { uint16_t recoveryMin; uint16_t backstopMin; }` also lives in config.h. ZERO new constants needed. |
| Pure function `uint32_t backstopMinutes(uint32_t recoveryMin)` | Free function in the UNGUARDED region of `firmware/jug_filler/jug_filler.ino`. House seam rule #3: free functions DO get an explicit prototype in the prototype block (unlike classes) — `medianOf` / `formatBootBanner` precedent. |
| Output consumed by P1.M4.T2.S1 | Downstream PRP not yet written (task Planned). Freeze: signature `uint32_t backstopMinutes(uint32_t recoveryMin)`, returns whole minutes, floored. FRESH_BOOT_BACKSTOP_MIN is the CALLER's no-history path — this function is table-only. |
| Native tests | New Unity project `test/test_backstop/test_backstop.cpp`, explicit `int main()` runner (PIO 6.2.0), relative `.ino` include — `test_trip`/`test_guards` precedent. |

## §2 The rounding trap (THE design decision of this task)

Contract: "Define rounding explicitly (floor to whole minutes)". Naive integer
interpolation from the LEFT knot — `drop = (b0-b1)*(r-r0)/(r1-r0)` with plain
truncating `/` — floors the DROP, which CEILS the RESULT on a falling segment
(`floor(b0-x) != b0-floor(x)`). At r=6 (exact 42.5) the naive form returns 43,
not 42. Rounding the result UP = a longer valve-open grant = the exact
"padding" PRD §6 forbids ("never padded").

Correct integer form (no floats): for integer `b0` and falling segments,
`floor(b0 - x) == b0 - ceil(x)`, so compute the drop with a ceiling division of
strictly non-negative integers:

```cpp
drop = ((b0 - b1) * (r - r0) + (r1 - r0) - 1) / (r1 - r0);   // ceil(a/b), a,b >= 0
return b0 - drop;
```

All quantities non-negative inside a segment (knots are ascending in recovery,
descending in backstop); max product ≈ 96 → no overflow. uint16_t knot fields
are widened to uint32_t locals once, up front — keeps every comparison and
subtraction unsigned 32-bit (avoids -Wsign-compare under -Wall -Wextra -Werror).

Segment-search loop: after both clamps, `2 < recoveryMin < 60` holds, so the
`while (recoveryMin >= r[i+1]) ++i;` scan stops at `i == BACKSTOP_TABLE_LEN-2`
at the latest — `i+1` cannot go out of bounds. No early-exit tricks needed.

## §3 Machine verification (merged-copy method, this session)

Method: copied the real `firmware/` to /tmp, inserted the PRP's verbatim block
above the `// ---- #ifdef ARDUINO REGION` marker (preceding block verified as the
false-trip-guards end-banner at line 552 — **P1.M3.T3.S2 HAS LANDED**), added the
prototype line after `medianOf`, compiled the test file against the merged .ino:

```
g++ -std=c++17 -Wall -Wextra -Werror -I<project>/.pio/libdeps/native/Unity/src \
    test/test_backstop/test_backstop.cpp <Unity>/src/unity.c -o run && ./run
→ 7 Tests 0 Failures 0 Ignored / OK
```

(Note for repro: unity.c must be compiled+linked too, else `undefined reference
to UnityAssertEqualNumber` at link time.)

### Hand-derived exact values (all machine-confirmed)

Segments (floor of exact result):
- `r ∈ (2,10]`: 44 − 3·(r−2)/8   → 3→43, 4→43, 5→42, 6→42(42.5!), 7→42, 8→41, 9→41, 10→41
- `r ∈ (10,20]`: 41 − 3·(r−10)/10 → 11→40, 12→40, 13→40, 14→39, 15→39(39.5!), 16→39, 17→38, 18→38, 19→38, 20→38
- `r ∈ (20,35]`: 38 − 4·(r−20)/15 → 21→37, 24→36, 27→36(36.13), 28→35, 31→35, 32→34, 34→34(34.27), 35→34
- `r ∈ (35,60)`: 34 − 4·(r−35)/25 → 36→33(33.84), 41→33, 42→32, 47→32(32.08), 48→31, 53→31, 54→30, 59→30(30.16)
- Clamps: 0/1/2→44; 60→30 (also knot); 120, 1440, 4294967295→30.

The two half-integer midpoints (6→42.5, 15→39.5) are the floor-vs-ceil
DISCRIMINATORS — a truncating-drop implementation returns 43/40 there and fails
exactly two tests. Monotone non-increasing + bounded to [30,44] verified for
every r in 0..180. Composition pinned: `elapsedMs(now, lastEnd)/60000 → 15 → 39`.

## §4 Downstream API freeze (for P1.M4.T2.S1's PRP)

- `uint32_t backstopMinutes(uint32_t recoveryMin)` — call ONCE at fill start
  (PRD §6.2), with recovery in whole minutes since previous fill end.
- Caller owns: FRESH_BOOT_BACKSTOP_MIN when no history; converting ms → min;
  the deadline timestamp math (elapsedMs/deadlinePassed); PLUS5_MIN stacking.
- r1-1 values equal their floor (e.g. 59→30, 34→34): knots bracket correctly.

## §5 Formatting pin

The merged-copy insertion left TWO blank lines at the seam (guards banner →
block). House style is exactly one blank line banner-to-banner — the PRP's Task
1 pins one blank line above and below the block.
