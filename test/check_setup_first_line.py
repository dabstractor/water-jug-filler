#!/usr/bin/env python3
"""Structural safety-invariant check (P1.M7.T1.S1, PRD §10).

Checks firmware/jug_filler/jug_filler.ino:
  A1  setup(): first executable statement is digitalWrite(PIN_VALVE, LOW);
  A2  loop():  first executable statement is watchdog_update();   [§10: fed
      every pass, before anything else can hang]
  A3  no delay( token outside comments and string/char literals, in the
      WHOLE file (strict superset of the required ARDUINO loop region — the
      pure region is delay-free by the seam rules anyway);
  A4  no `while (!Serial` gate anywhere (PRD §3: logging never blocks on
      USB presence).

Comments and literals are blanked (newlines preserved) before scanning, so
mentions like "zero delay()" in prose cannot false-positive. Stdlib only.
Exit 0 = pass, 1 = violations (each printed with a line number where known).
"""

import re
import sys
from pathlib import Path

SKETCH = (Path(__file__).resolve().parent.parent
          / "firmware" / "jug_filler" / "jug_filler.ino")


def strip_comments_and_strings(text):
    """Blank out // and /* */ comments and "..." / '...' literals, keeping
    newlines (line numbers survive). Single left-to-right scanner: whichever
    token opens first owns its span, so '"' inside a char literal and "'"
    inside a string are handled correctly."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":                       # line comment
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":                     # block comment
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n
                                 and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
        elif c in ('"', "'"):                             # string/char literal
            quote = c
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                else:
                    out.append("\n" if text[i] == "\n" else " ")
                    i += 1
            if i < n:
                out.append(" ")
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_body(code, name):
    """Lines inside { ... } of the DEFINITION `void name() { ... }`.
    Matches the definition, never the forward declaration `void name();`
    (which ends in ';' and has no brace)."""
    m = re.search(r"void\s+" + name + r"\s*\(\s*\)\s*\{", code)
    if not m:
        return None
    brace = code.find("{", m.start())
    depth, i = 1, brace + 1
    while i < len(code) and depth > 0:
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
        i += 1
    return code[brace + 1:i - 1].splitlines()


def first_executable_line(body_lines):
    for ln in body_lines:
        stripped = ln.strip()
        if stripped:
            return stripped
    return ""


def main():
    failures = []
    code = strip_comments_and_strings(SKETCH.read_text(encoding="utf-8"))

    setup_body = function_body(code, "setup")
    loop_body = function_body(code, "loop")

    # A1 — valve LOW is setup()'s very first executable statement (PRD §10)
    if setup_body is None:
        failures.append("A1: setup() definition not found")
    else:
        first = first_executable_line(setup_body)
        if not re.fullmatch(r"digitalWrite\s*\(\s*PIN_VALVE\s*,\s*LOW\s*\)\s*;", first):
            failures.append(
                "A1: setup() first executable statement is not "
                f"digitalWrite(PIN_VALVE, LOW); — got: {first!r}")

    # A2 — watchdog fed before anything else in loop() (PRD §10)
    if loop_body is None:
        failures.append("A2: loop() definition not found")
    else:
        first = first_executable_line(loop_body)
        if not re.fullmatch(r"watchdog_update\s*\(\s*\)\s*;", first):
            failures.append(
                "A2: loop() first executable statement is not "
                f"watchdog_update(); — got: {first!r}")

    # A3 — zero delay( outside comments/strings, whole file (see docstring)
    for lineno, ln in enumerate(code.splitlines(), start=1):
        if "delay(" in ln:
            failures.append(f"A3: delay( at line {lineno}: {ln.strip()!r}")

    # A4 — zero while (!Serial gates (PRD §3)
    if re.search(r"while\s*\(\s*!\s*Serial", code):
        failures.append("A4: while (!Serial present — logging must not block on USB")

    if failures:
        print("STRUCTURAL SAFETY CHECK: FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("STRUCTURAL SAFETY CHECK: PASS "
          "(setup line 1 valve LOW; loop line 1 watchdog; "
          "no delay(; no !Serial gate)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
