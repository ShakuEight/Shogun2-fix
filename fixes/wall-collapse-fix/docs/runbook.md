# wall-collapse-fix runbook

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

The harness runs in four modes (fix 30, off 23, probe 30, corrupt 3 checks). check_prologues.py
must print `all N MATCH` and `no overlapping sites`. Stolen bytes come from
`docs/re/prologues_collapsefix.txt` (probe hooks) and `docs/re/prologues_collapsefix_fix.txt`
(the fix), both local.

## Replay runs (autorun)

Install `build\dinput8.dll` into the game folder first (keep the old one as a `.bak`-style copy).

```
python -u tools\autorun\autorun.py "broken wall bug" --speed 1 --env COLLAPSEFIX_PROBE=1 [--env COLLAPSEFIX_OFF=1]
```

| Variable | Effect |
|---|---|
| `COLLAPSEFIX_OFF=1` | fix off at launch (I turns it on) |
| `COLLAPSEFIX_ON=1` | fix on at launch in a `COLLAPSEFIX_START_OFF` build |
| `COLLAPSEFIX_PROBE=1` | research probe into `<game>\collapsefix_runs\<stamp>\` |

The replay "broken wall bug" was recorded with the dev build of 2026-10-19 14:28 (all six other
fixes at their defaults), so play it with the other fixes at their defaults. With the fix on it
desyncs at tick 453, the tick the men die in the recording. Expected log lines (game folder
`pathfix_log.txt`):

```
collapsefix: battle start, gates open to all: building XXXXXXXX, 1 intact gate door(s) kept (fix ON)
collapsefix: battle start gate damage calls 2887 (skipped 2887); intact gate doors 2, kept 2
```

A vanilla replay ("wall siege bug") needs every other fix off: `ARTFIX_OFF=1 FBRFIX_OFF=1
SIEGEFIX_OFF=1 WALLFIX_OFF=1 ZOMBIEFIX_OFF=1` (pathfix starts off by itself).

## Live check build (only this fix, off at launch)

```
cmake -S . -B build_probe -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ^
  -DFIX_PATHFIX=OFF -DFIX_ARTILLERY_TARGETING=OFF -DFIX_FIRE_BY_RANK=OFF -DFIX_SIEGE_RESPONSIVENESS=OFF ^
  -DFIX_WALL_MANNING=OFF -DFIX_ZOMBIE_YARI_WALL=OFF -DFIX_WALL_COLLAPSE=ON -DCOLLAPSEFIX_START_OFF=ON
cmake --build build_probe
```

(from a VS x86 developer prompt, or with `vcvarsall.bat x86` first). The log must show
`Fixes compiled in: 1` and `OFF at launch`. I toggles (one high tone = on, one low = off).

## Analysis (local, `analysis/`)

- `collapse_timeline.py <run|latest> [--window N]`: phase changes, piece events, piece state
  changes, men count drops and deaths around Start Battle.
- `pe.py`: read-only scan helper over the on-disk DLL (xrefs, callers, disassembly); `q_*.py`
  one-off scans built on it.

Log every run in `docs/research/research_log.md`.
