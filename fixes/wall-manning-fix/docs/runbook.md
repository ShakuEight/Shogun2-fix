# wallfix runbook

How to build, test and measure the wall manning fix. The engine decode is
`docs/re/wall_manning_x86.md`; the design is in `../ARCHITECTURE.md`.

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

`test.bat` runs every fix's harness; wallfix's runner is `tests/run_tests.py` (five modes: fix,
off, maxdist, probe, corrupt). `check_prologues.py` must print `all N MATCH` and
`no overlapping sites`.

## Replay runs (autorun)

Install the dev `dinput8.dll` in the game folder first (keep the user's own one under another
name and put it back afterwards). The test replay is `wall_manning_pathin`; it was recorded with
siegefix, artfix and fbrfix on and pathfix off, so leave those at their defaults for a run that
stays in sync with the recording (fix off).

```
python -u tools\autorun\autorun.py "wall_manning_pathin" --speed 1 --env WALLFIX_PROBE=1 --env WALLFIX_OFF=1
python -u tools\autorun\autorun.py "wall_manning_pathin" --speed 1 --env WALLFIX_PROBE=1
```

| Variable | Effect |
|---|---|
| `WALLFIX_OFF=1` | start with the fix off (I toggles) |
| `WALLFIX_MAX_DIST=<m>` | keep the staging march beyond this distance (0..5000, default 0 = no limit) |
| `WALLFIX_PROBE=1` | write `<game>\wallfix_runs\<stamp>\` (events, slots, stacks, A* searches, men) |
| `PATHFIX_ON=1` | pathfix dev build starts with variant G on (for interaction runs) |

A run with the fix on changes the men's routes, so the replay drifts from the recording
(`replay_desync.txt`). Compare per unit and per order, never by the battle's result. Add
`--capture --collect` for pathfix's soldier trace; it records only units that get formation
slot orders, so with the fix on the wall units are missing there and `wall_men.csv` is the
source.

## Analysis (local, `analysis/`)

```
python -I fixes\wall-manning-fix\analysis\wall_metrics.py "<game>\wallfix_runs\<off>" "<game>\wallfix_runs\<on>"
python -I fixes\wall-manning-fix\analysis\wall_timeline.py "<game>\wallfix_runs\<stamp>"
python -I fixes\wall-manning-fix\analysis\plot_men.py "<game>\wallfix_runs\<stamp>" <out.png> -80,150,460,600 4
```

Log every run (stamp, replay, variables, result) in `docs/research/research_log.md`.
