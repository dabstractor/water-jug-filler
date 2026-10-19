# Research Notes — P1.M1.T1.S2 (dual-build skeleton + native test seam)

Session: PRP creation. Method: local empirical verification with PlatformIO Core
6.2.0 in a /tmp scratch project (repo untouched), plus targeted web verification.
Every binding claim below was proven on this machine or cited from a primary source.

## 1. Empirical verification (scratch project in /tmp/seamtest, layout identical to PRP)

Scratch layout = the exact layout shipped in the PRP:

```
firmware/jug_filler/jug_filler.ino   (minimal skeleton: ARDUINO-guarded setup/loop + one pure fn)
firmware/jug_filler/config.h         (byte-copy of the repo's firmware/config.h from S1)
test/test_seam.cpp                   (Unity, includes the .ino, explicit main())
platformio.ini                       ([platformio] src_dir=firmware/jug_filler; [env:pico]; [env:native])
```

Result: `pio test -e native` → `native:* [PASSED]` in 0.45 s. Full command output:

```
Building...
Testing...
test/test_seam.cpp:15: test_config_constants_visible	[PASSED]
--------------------- native:* [PASSED] Took 0.45 seconds --
Environment    Test    Status    Duration
native         *       PASSED    00:00:00.445
```

## 2. Verified binding facts (each one changed the PRP)

1. **PIO 6.2.0 does NOT auto-generate a Unity test runner.** A test file without
   `main()` fails to link: `undefined reference to 'main'` (observed live). Every
   test file MUST end with explicit `UNITY_BEGIN(); RUN_TEST(...); return UNITY_END();`
   inside `main()`. This contradicts older PlatformIO docs/examples that show no main.
2. **`build_src_filter = -<*>` is required in `[env:native]`.** The tests
   `#include` the .ino directly (so its code is compiled inside the test TU); if
   the native env also compiled `src_dir`, non-inline functions would collide
   (duplicate symbols) and the native platform has no `.ino` builder anyway.
   With the filter, the build is clean (observed live).
3. **Unity is auto-fetched on first run** — `Library Manager: Installing
   throwtheswitch/Unity @ ^2.6.1` (observed live). No `lib_deps` entry needed;
   `test_framework = unity` is sufficient.
4. **Board id `waveshare_rp2040_zero` RESOLVES.** After
   `pio pkg install --platform raspberrypi`, `pio boards` lists:
   `waveshare_rp2040_zero   RP2040  133MHz  2MB  256KB  RP2040 Zero`.
   No fallback needed; PRP documents generic fallback ids (`rpipico`, pin-compatible
   for GP14/15/26/27) only as insurance. The raspberrypi platform is NOW installed
   locally (~/.platformio/platforms), so the implementer only lacks the
   toolchain/framework packages that `pio run -e pico` downloads on first run.
5. **Arduino sketch folder naming rule (MANDATORY):** "Every sketch must contain
   a .ino file with a file name matching the sketch root folder name" —
   https://arduino.github.io/arduino-cli/1.4/sketch-specification (also forum:
   "It is mandatory for the filename of the primary .ino file to match the sketch
   folder name"; upheld in arduino-cli issue #948). CONSEQUENCE: the architecture
   doc's `firmware/jug_filler.ino` (folder "firmware" ≠ sketch "jug_filler")
   CANNOT build in Arduino IDE — the IDE shows the "needs to be inside a sketch
   folder named jug_filler" repair dialog (which would move the file and break
   the layout), and arduino-cli refuses outright. **Decision: nest one level —
   `firmware/jug_filler/jug_filler.ino` + `firmware/jug_filler/config.h`.**
   This keeps: PRD §3 two-file contract (both files inside the one sketch folder),
   Arduino IDE direct build, PlatformIO `src_dir = firmware/jug_filler`, native
   tests `#include "../firmware/jug_filler/jug_filler.ino"`. Requires `git mv
   firmware/config.h firmware/jug_filler/config.h` (contents untouched, byte-for-byte).
   Verified the nested layout passes `pio test -e native` (run #2, PASSED).
6. **`[platformio] default_envs = native`** makes bare `pio test` safe (native only;
   no accidental on-device test of env:pico). Document that pico builds use
   explicit `pio run -e pico`.
7. **digitalWrite-before-pinMode order** (valve LOW as literal line 1 of setup,
   then pinMode OUTPUT) is glitch-free on arduino-pico: digitalWrite writes the
   output latch; during the pre-pinMode Hi-Z window the MOSFET module's gate
   pull-down holds the valve closed (external_deps.md §8); pinMode(OUTPUT) then
   drives the pre-latched LOW. Matches firmware_architecture.md §1 ordering.

## 3. Not verified locally (documented as first-run expectations in the PRP)

- Full `pio run -e pico` compile (needs first-run download of gcc-arm-none-eabi
  toolchain + arduino-pico framework; ~hundreds of MB, minutes). Board id + platform
  install verified; board def is standard RP2040 so compile risk is low.
- Arduino IDE 2.x manual flow (human gate; steps + Boards Manager URL in README):
  https://arduino-pico.readthedocs.io/en/latest/install-platformio.html … the
  Boards Manager JSON is https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
  (from arduino-pico docs install page).

## 4. Machine environment (verified)

- PlatformIO Core 6.2.0 at ~/.local/bin/pio; g++ 16.2.1 (host); python3 present.
- `pio board` is NOT a command (it's `pio boards`) — PIO 6.2 CLI surface.

## 5. Sources

- https://arduino.github.io/arduino-cli/1.4/sketch-specification (folder==sketch name rule)
- https://forum.arduino.cc/t/arduino-cli-compile-seems-to-try-wrong-filename/1258678
- https://github.com/arduino/arduino-cli/issues/948 (rule upheld)
- https://docs.platformio.org/en/latest/advanced/unit-testing/ (test runner, Unity)
- https://arduino-pico.readthedocs.io/en/latest/ (core docs; install, analog, bootsel)
- plan/001_53984f832581/architecture/external_deps.md (verified facts + layout rationale)
- plan/001_53984f832581/architecture/firmware_architecture.md (seam rules)
- firmware/config.h (S1 deliverable, read; included verbatim in scratch verification)
