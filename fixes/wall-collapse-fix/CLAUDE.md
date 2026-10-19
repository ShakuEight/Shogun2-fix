# wall-collapse-fix (collapsefix): history and working notes

Men deployed on a castle gatehouse die at Start Battle. Shared rules, paths and engine knowledge are
in the root CLAUDE.md; the engine decode is `docs/re/building_destruction_x86.md`; the design is in
`ARCHITECTURE.md`; numbers in `README.md`. Local only (git-ignored): `analysis/` (pe.py scan
helper, q_*.py scans, collapse_timeline.py, Ghidra decompiles in `analysis/decompiles/`),
`docs/research/` (research log).

## Session 1, 2026-10-19 (dev build, not released)

User report: see ARCHITECTURE.md. Replay `broken wall bug.replay` (15:27; not `wall siege bug`,
which is siegefix's), recorded with the dev build of 14:28 (all six other fixes, defaults), "an
old bug from vanilla". User constraint: walls destroyed by artillery in battle must still
collapse and kill.

- Static: building command creators lead to the piece (+0x68 destruct level, +0xEC damage, +0xF1
  destroyed, template +0x18 with level thresholds) and its functions (`FUN_10477f40` apply,
  `FUN_10477ff0` damage set, `FUN_1047b370` damage added, `FUN_10479ca0` collapse effects); the
  campaign fort damage in the battle set-up (`FUN_10174140`, `FUN_10426940`); battle buildings at
  battle +0x114 / +0x118.
- Probe runs (game folder `collapsefix_runs\`): 213723 (probe only): at tick 452, the first
  battle tick, `FUN_101787b0` -> `FUN_1043c870` -> `FUN_1043c8b0` damage every gate piece; two
  intact doors collapse; no soldier hit point set at all; the men census missed deployed units
  (fixed: all group lists). 214244: 10 men at height 8 m over the door leave their group at tick
  453, unit 200 -> 190.
- Gate flag `[tick object +0x5C] +0x9C` = gates open to all (piece set-up builds gates open, gate
  tick keeps them open, passability lets every alliance through).
- Fix: entry hook on `FUN_1043c870` (10 bytes `56 57 8B F9 8B B7 D0 01 00 00`,
  docs/re/prologues_collapsefix_fix.txt) returns at once with the fix on.
- Runs 214507 (fix off: 2 doors destroyed, 200 -> 190, in sync) and 214606 (fix on: 2 doors kept,
  no loss, desync at tick 453). 215553: replay `wall siege bug` (vanilla, every other fix off, fix
  on): 946 calls skipped, no door, no piece damage at all in that battle, in sync.
- Harness `tests/collapsefix_test.cpp`: fix 30, off 23, probe 30, corrupt 3; test.bat all passed.
- Game folder: at the start `dinput8.dll` was the all-fixes dev build of 14:28 (SHA-256
  F339B42E..., identical to build\dinput8.dll then), kept as `dinput8.dll.pre_collapsefix`. At the
  end `dinput8.dll` is the session build with collapsefix (all seven fixes, collapsefix on;
  SHA-256 AAD7496D...), copy `dinput8.dll.collapsefix`. `version.dll` (autorun) unchanged.
  Probe runs take 8 MB.

Pitfalls met:
- Bash heredocs with an apostrophe or `'\\'` in the text fail in this tool ("unexpected EOF");
  write files and edit scripts with the Write tool (scratchpad) instead.
- During deployment a unit's men move to groups that are not in its +0xB90 list; count men over
  the main group and both group lists (+0xB9C, +0xB90, +0xB74).
- The per-piece gate function `FUN_1043c8b0` starts with a short branch: hook its loop
  `FUN_1043c870` instead.
- `from pe import *` does not import names starting with an underscore.

Next (not started):
- User live check: a siege defence with sabotaged / open gates, units on the gatehouse, fix on and
  off (I); and artillery breaching a manned wall or gate in battle (must still kill).
- Pre-battle destroyed wall pieces themselves were never seen to re-collapse; if the user meets a
  case that is not a gate, record a replay and run the probe.
- Public release source and the combined package (fixes/README.md).
