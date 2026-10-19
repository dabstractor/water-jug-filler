# jug-filler

Firmware for an automatic water-jug filler: two analog water-level sensors, one
solenoid valve, one status LED — on a Waveshare RP2040-Zero (RP2040) board.
All tunables live in [`firmware/jug_filler/config.h`](firmware/jug_filler/config.h).

## Repo layout

```
firmware/jug_filler/   Arduino sketch (jug_filler.ino + config.h) — the only firmware files
test/                  Native host unit tests (Unity), run without any hardware
platformio.ini         PlatformIO build (board) + host-test (native) configuration
```

## Build in Arduino IDE

1. Install Arduino IDE 2.x.
2. **File → Preferences → Additional boards manager URLs**, add:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
3. **Tools → Board → Boards Manager…**, search for "Raspberry Pi Pico/RP2040"
   (Earle Philhower's arduino-pico core) and install it.
4. **Tools → Board** → select a **Waveshare RP2040-Zero** entry under the RP2040 core.
5. **File → Open** → `firmware/jug_filler/jug_filler.ino` (the folder name must match
   the sketch name — don't move files out of `firmware/jug_filler/`).
6. **Sketch → Upload**.

## Build with PlatformIO

```sh
pio run -e pico        # compile-only check (no upload); first run downloads the toolchain
```

## Run native tests

```sh
pio test -e native     # host g++ + Unity; no board or Arduino toolchain needed
```

First run downloads the native platform and the Unity library — expected, one-time.

## Safety note

The **first executable line of `setup()`** drives the valve pin
(`PIN_VALVE`, GP15) LOW so the valve is closed before anything else runs.
Never reorder that line. `setup()`/`loop()` contain no `delay()` and no
`while(!Serial)` anywhere — see the seam-contract comment at the top of
`firmware/jug_filler/jug_filler.ino`.
