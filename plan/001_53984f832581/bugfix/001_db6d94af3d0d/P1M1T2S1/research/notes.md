# Research Notes — P1.M1.T2.S1 (bugfix): README BTN example value (BUG-002)

## Ground truth chain
- spec/SPEC.md §5.1 (lines 117-126, read in full): short press = hold < 1000 ms; long ≥ 1000 ms
  (`BUTTON_LONG_PRESS_MS`, contact debounce 40 ms). Classification source of truth.
- config.h:28 `BUTTON_LONG_PRESS_MS = 1000`. ButtonClassifier (jug_filler.ino:150) returns
  LongPress when holdMs >= 1000; BTN line emitted at jug_filler.ino:1685 with the raw
  edge-to-edge hold (tracks debounced hold within ~tens of ms bounce). ⇒ any 1234 ms hold
  logs `BTN long 1234ms`; `BTN short 1234ms` is impossible. (system_context.md §2/§4.)

## Occurrences of `BTN short 1234ms` (verified by grep, exactly two in shipped docs)
1. README.md:49 — the serial-log table BTN row:
   `| \`BTN short 1234ms\` / \`BTN long 1500ms\` | BOOT press, classified on release (hold = raw edge-to-edge, diagnostic) |`
2. docs/COMMISSIONING.md:90 — prose (continues L91: \`BTN long 1500ms\`; the ms value is the raw hold time, diagnostic only).)

Sibling P1.M1.T1.S1 (parallel, COMMISSIONING.md Stage 5a recmin fix, lines 434/436 only)
explicitly does NOT touch the BTN examples → **COMMISSIONING.md:90 must be fixed by THIS
item**, otherwise this item's own §6 verification gate fails:
`grep -n 'BTN short\|BTN long' README.md docs/COMMISSIONING.md` must show every short
n < 1000 and every long n >= 1000 **everywhere**. Different lines than the sibling's —
no textual conflict; "Change nothing else in the row or table" governs the README table
only (COMMISSIONING.md:90 is prose, not a table row).

Valid existing examples (must remain valid): COMMISSIONING.md:153 `BTN short 350ms`,
:154 / :91 / README:49 `BTN long 1500ms` (1500 >= 1000 ✓). Replacement value 340 ms is
the item's suggested value, ASCII-only, not yet used anywhere (grep -c = 0 both files).
README:49 is plain ASCII (no em-dash/arrow traps like the sibling's L434/L436).

## Verification gate (system_context.md §6 recipe, made executable)
```bash
# TDD: fails before the fix (1234), passes after (340)
grep -oE 'BTN (short|long) [0-9]+ms' README.md docs/COMMISSIONING.md \
 | awk '{n=substr($3,1,length($3)-2)+0; if(($2=="short"&&n>=1000)||($2=="long"&&n<1000)){print "FAIL: "$0;bad=1}} END{exit bad}'
```
Hand-checked against current+post-fix content: before → FAIL on the two 1234 rows;
after → 6 matches (short 340×2, 350; long 1500×3), exit 0.
