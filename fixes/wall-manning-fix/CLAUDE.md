# wall-manning-fix (wallfix): history and working notes

Units ordered to man a wall march to a staging point first. Shared rules, paths and engine
knowledge are in the root CLAUDE.md; the engine decode is `docs/re/wall_manning_x86.md`; the
design is in `ARCHITECTURE.md`; numbers in `README.md`. Local only (git-ignored): `analysis/`
(wall_load.py, wall_timeline.py, wall_metrics.py, plot_men.py, plot_wall_scene.py, disasm.py /
finddw.py / states.py capstone helpers, Ghidra decompiles in `analysis/decompiles/`), `data/`
(PNGs, pathfix logs of the interaction runs), `docs/research/` (research log).

## Session 1, 2026-10-18 (dev build, not released)

User report: see ARCHITECTURE.md. Replay `wall_manning_pathin.replay` (recorded 22:05 with the dev
build of 21:04: siegefix, artfix, fbrfix on, pathfix off; pathfix was switched on in the last
seconds but adjusted nothing). User decisions: the fix applies to all units; C was open ground.

- Capture 20261018_222832 (pathfix capture, in sync): the detour is a formation march to C, not
  a soldier route; the direct unit is missing from the soldier trace (no formation orders).
- Static: unit move action (`FUN_103db410` -> `FUN_103be080`, states, unit path search
  `FUN_10385e90` incremental), per-man form-up action (`FUN_103be5d0`, slots 0x24 bytes), wall
  task (vtable 0x115BF91C) with the staging test `FUN_1016af90` and staging point `FUN_10175f10`.
  State code in 0x100Fxxxx / 0x103B2xxx is not defined as functions in s2x86: capstone helpers
  in `analysis/` instead of define_function.py (which would modify the Ghidra project).
- Probe runs (game folder `wallfix_runs\`): 224937 (probe only, in sync): moves from
  `FUN_1017e5a0` for all three units; 230205 (first fix-on run).
- Fix: answer of `FUN_1016af90` at return 0x101B0CAD; runs 230357 (off) / 230535 (on): east
  median 38.8 -> 21.6 s, west 26.3 -> 17.3 s, routes 85 -> 43 m and 117 -> 67 m, third unit
  unchanged.
- pathfix dev got `PATHFIX_ON=1` (variant G at launch) for unattended interaction runs.
- Interaction runs 230958 (pathfix on, wallfix off) / 231136 (both on): wallfix-on figures identical
  with and without pathfix; pathfix set radius lanes only on the staging march (no effect) and
  has nothing to act on once it is skipped.
- Game folder: the user's active `dinput8.dll` (the 21:04 build) was set aside as
  `dinput8.dll.pre_wallfix` during the runs and put back at the end; the session build sits there
  as `dinput8.dll.wallfix`.
- 2026-10-19: wallfix build installed as the active `dinput8.dll` for the user's live check (copy
  of `dinput8.dll.wallfix`, SHA-256 451ACC90...); the 21:04 build set aside again as
  `dinput8.dll.pre_wallfix`.

Next (not started):
- User live check: order units onto walls in a custom siege, fix on and off (I).
- Other castles and wall types (the zone branch of the staging point vs the formation-fit
  branch); the AI as defender; very distant units (WALLFIX_MAX_DIST if a mob arrival looks bad).
- Public release source and the combined package (fixes/README.md).

Pitfalls met:
- Bash heredocs turn `\\n` inside embedded Python into a real newline (siegefix's pitfall
  again); write edit scripts to the scratchpad with the Write tool and run them.
- The harness must pass a real IID to DirectInput8Create, or the system dinput8 crashes.
- The move creator's ECX (the unit) and the behaviour stack owner differ by 0x20.
