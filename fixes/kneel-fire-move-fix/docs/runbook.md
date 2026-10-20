# kneelfix runbook

How to build, test and check the kneel fire move fix. The engine decode is
`docs/re/kneel_fire_x86.md`; the design is in `../ARCHITECTURE.md`.

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

`test.bat` runs every fix's harness; kneelfix's runner is `tests/run_tests.py` (five modes: fix,
off, probe, corrupt, corrupt_head). `check_prologues.py` must print `all N MATCH` and
`no overlapping sites`.

## Replay runs (autorun)

Install the dev `dinput8.dll` in the game folder first (keep the user's own one under another
name and put it back afterwards). The replay `kneel fire` was recorded with every dev fix at its
default (pathfix off): use the normal dev build.

```
python -u tools\autorun\autorun.py "kneel fire" --env KNEELFIX_PROBE=1 --env KNEELFIX_OFF=1
python -u tools\autorun\autorun.py "kneel fire" --env KNEELFIX_PROBE=1
```

| Variable | Effect |
|---|---|
| `KNEELFIX_OFF=1` | start with the fix off (I toggles) |
| `KNEELFIX_ON=1` | start a `KNEELFIX_START_OFF` build with the fix on |
| `KNEELFIX_PROBE=1` | write `<game>\kneelfix_runs\<stamp>\kneel_calls.csv` |

A move order the game refused is never sent, so a replay recorded without the fix cannot show
the change; it can only show that the fix leaves its orders alone (both runs in sync). The
change itself needs a live check, or a replay recorded with the fix on.

## Live check

Build with only this fix, off at launch (a separate build tree):

```
cmake -S . -B build_live_kneelfix -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
      -DFIX_PATHFIX=OFF -DFIX_ARTILLERY_TARGETING=OFF -DFIX_FIRE_BY_RANK=OFF
      -DFIX_SIEGE_RESPONSIVENESS=OFF -DFIX_WALL_MANNING=OFF -DFIX_ZOMBIE_YARI_WALL=OFF
      -DFIX_WALL_COLLAPSE=OFF -DFIX_SINGLE_UNIT_GROUP=OFF -DFIX_GUARD_MODE_RANGED=OFF
      -DFIX_KNEEL_FIRE_MOVE=ON -DKNEELFIX_START_OFF=ON
cmake --build build_live_kneelfix
```

(from a VS x86 developer prompt; `build.bat` shows the vcvars and cmake paths). In a battle with
Fall of the Samurai line infantry that have Kneel Fire: switch Kneel Fire on, then click-move,
drag-move (with width) and, in deployment, drag-place the unit, alone and in a selection with
other units. With the fix off (as at launch) the kneeling unit stays; I (one high tone) turns
the fix on and the unit moves with the ability still on. After the move it should kneel-fire
again. The game-folder `pathfix_log.txt` ends with the kneelfix counters.
