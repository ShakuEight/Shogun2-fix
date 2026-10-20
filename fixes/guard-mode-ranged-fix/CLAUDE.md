# guard-mode-ranged-fix (guardfix): history and working notes

Missile units in guard mode chase targets that left their range and keep shooting at shattered
units. Shared rules, paths and engine knowledge are in the root CLAUDE.md; the engine decode is
`docs/re/guard_mode_x86.md`; the design is in `ARCHITECTURE.md`; numbers in `README.md`. Local
only (git-ignored): `analysis/` (guard_report.py, taskstates.py, x86q.py and the zombie helpers;
Ghidra decompiles in `analysis/decompiles/`), `data/` (the recording's game log),
`docs/research/` (research log).

## Session 1, 2026-10-20 (dev build, not released)

User report: see ARCHITECTURE.md. Replay `guard mode.replay` (08:16), recorded with the all-fixes
dev build of 2026-10-19 (8 fixes, pathfix off; game folder log saved as
`data/recording_pathfix_log_20261020_0806.txt`). User decisions: a guard missile unit lets any
target go once it is out of range (routing or not); the shattered rule applies to guard units
only; with only shattered enemies in range it holds fire; the chase followed explicit attack
orders.

- Static: guard byte unit +0xEB1 (setter `FUN_10186130`, four readers; the melee no-chase is
  `FUN_101af030` in the melee attack task's PURSUE_STATE); missile attack task 0x115C0F24
  (target +0x24, no-intercept byte +0x2D), its INTERCEPT decision `FUN_101aefc0` and target test
  `FUN_101b2460`; fire at will through the unit interrupt machine (unit +0xE78,
  FIRE_AT_WILL_INTERRUPT_STATE 0x115B4DE0, choice `FUN_101752d0`).
- Runs (game folder `guardfix_runs\`): 090506 (fix off, battle decided 9 s after the rout,
  before the chase); 090722 (fix off, `--linger 90`: the three guard missile units INTERCEPT
  from FIRE at ticks 985..989 against their shattered target and walk 32..47 m); 091047 (first
  fix, only the missile task's calls: no chase, but fire at will re-targets the next shattered
  enemy every 0.2 s, 444 answers changed); 091846 (+ awareness class hook `FUN_10169390`: no
  effect, dropped); 092344 / 092932 (probe hooks on the attack command post and the unit attack
  task constructor: the restarts come from the interrupt machine's FIRE_AT_WILL_INTERRUPT_STATE);
  093820 (final fix: guard missile units 0.7..1.2 m, 5 orders ended on shattered targets, 265
  shattered enemies left out of fire at will, 2 tasks instead of 442); 094146 (final build, fix
  off: in sync, identical to 090722).
- Harness `tests/guardfix_test.cpp`: fix 83, off 57, probe 32, corrupt 3.
- Game folder: the user's `dinput8.dll` (identical to `dinput8.dll.groupfix_live`) was set aside
  as `.pre_guardfix` during the runs and is active again; the final all-fixes dev build sits as
  `dinput8.dll.guardfix`, the live check build (this fix only, off at launch, I toggles) as
  `dinput8.dll.guardfix_live`. Not committed.

Pitfalls met:
- The battle in this replay is decided before the chase; without `--linger` the runs never show it.
- `cmd //c build.bat` from the Bash tool silently does nothing; build from PowerShell.
- Bash heredocs with Python byte strings mangle backslashes; write edit scripts to the
  scratchpad with the Write tool.
- The unit controller's machine context sits at its object +0x34, not +0x18, so the transition
  probe does not attribute its transitions (kind V rows stay empty); the squad and interrupt
  tasks are attributed.

Next (not started):
- User live check: guard mode missile units with an attack order on a unit that routs, and fire
  at will with a shattered and a valid enemy in range (switch); fix on and off (I).
- The switch to a valid enemy is covered by the harness only; this replay has no valid enemy
  left after the rout.
- Naval missile task (vtable 0x115C1418, sibling test `FUN_101aefa0`) not covered.
- Public release source and the combined package (fixes/README.md).
