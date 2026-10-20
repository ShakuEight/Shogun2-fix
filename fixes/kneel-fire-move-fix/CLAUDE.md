# kneel-fire-move-fix (kneelfix): history and working notes

Fall of the Samurai units with Kneel Fire on refuse move orders. Shared rules, paths and engine
knowledge are in the root CLAUDE.md; the engine decode is `docs/re/kneel_fire_x86.md`; the design
is in `ARCHITECTURE.md`. Local only (git-ignored): `analysis/` (scan_ability_bit.py, walk_up.py,
x86q.py, Ghidra decompiles and string / scan outputs), `data/` (the recording's game log),
`docs/research/` (research log).

## Session 1, 2026-10-20 (dev build, not released)

User report: see ARCHITECTURE.md. Replay `kneel fire.replay` (10:19), recorded with the all-fixes
dev build (pathfix off, the eight others on; game folder log saved as
`data/recording_pathfix_log_20261020_1016.txt`).

- Static: ability 0x39 = bit 0x39 of unit +0xED4; toggle handler `FUN_101ac250` (case 7 / 0x39:
  drill +0xED0 = 5 / 6); drill read only by the missile task (`FUN_10127460`, `FUN_10177780`:
  drill 6 -> kneel volley task `FUN_10128460`); the gate is the first step of `FUN_1015c3f0`;
  its builder callers run in the UI and in the move / teleport command executors (table in the
  docs/re note). `FUN_10185da0` (unit target step) left alone on purpose.
- Fix: eight CALL rel32 redirects (`docs/re/prologues_kneelfix.txt`) to wrappers that, for a
  kneeling unit the game refused, re-ask from `FUN_1015c3f0`+0x1A through a naked entry stub
  (PUSH ESI; MOV ESI,ECX; JMP); the function's first 32 bytes are byte-checked
  (`prologues_kneelfix_skip.txt`). Kneel fire stays on.
- Harness `tests/kneelfix_test.cpp`: fix 20, off 6, probe 5, corrupt 3, corrupt_head 3 (passed on
  the first build).
- Runs: 103830 (fix off, probe: in sync, 17 can-move tests, all drag, 0 for a kneeling unit) and
  104004 (fix on, probe: identical, in sync). The replay holds no move of a kneeling unit (the UI
  never sends a refused order), so the change itself needs the live check.
- Game folder: the user's `dinput8.dll` (09:38) was set aside as `.pre_kneelfix` during the runs
  and is active again; the all-fixes dev build sits as `dinput8.dll.kneelfix`, the live-check
  build (this fix only, off at launch, I turns it on) as `dinput8.dll.kneelfix_live`
  (build tree `build_live_kneelfix/`, not in git).

Pitfalls met:
- `sed -i 'a ...'` turns `\k`, `\t`, `\r` in a Windows path into escapes; edit .bat lines with
  Python.

Next (not started):
- User live check (docs/runbook.md, Live check): click, drag and deployment drag of a kneeling
  unit, alone and in a selection; fix off and on (I); kneel-fire volley after the move.
- Open question for the user: should an attack order on a target out of range also make a
  kneeling unit walk into range (the unit's target step `FUN_10185da0`, AI included)?
- Public release source and the combined package (fixes/README.md).
