# guardfix runbook

How to build, test and measure the guard mode ranged fix. The engine decode is
`docs/re/guard_mode_x86.md`; the design is in `../ARCHITECTURE.md`.

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

`test.bat` runs every fix's harness; guardfix's runner is `tests/run_tests.py` (four modes: fix,
off, probe, corrupt). `check_prologues.py` must print `all N MATCH` and `no overlapping sites`.

## Replay runs (autorun)

Install the dev `dinput8.dll` in the game folder first (keep the user's own one under another
name and put it back afterwards). The test replay is `guard mode`, recorded with the dev build of
2026-10-19 (all fixes, pathfix off): use the normal dev build with every fix at its default.
The battle is decided 9 s after the first rout, before the chase: always pass `--linger 90` so the
replay plays on (to tick 1262).

```
python -u tools\autorun\autorun.py "guard mode" --speed 1 --linger 90 --env GUARDFIX_PROBE=1 --env GUARDFIX_OFF=1
python -u tools\autorun\autorun.py "guard mode" --speed 1 --linger 90 --env GUARDFIX_PROBE=1
```

| Variable | Effect |
|---|---|
| `GUARDFIX_OFF=1` | start with the fix off (I toggles) |
| `GUARDFIX_PROBE=1` | write `<game>\guardfix_runs\<stamp>\` (states, calls, positions, orders, posts) |

The probe writes about 3 MB per run of this replay. Its hooks share sites with the fire-by-rank
and zombie probes: never turn on two probes together (the second refuses to install).

A run with the fix on changes what the guard units do from the first rout on, so the replay
drifts from the recording; compare the guard group only.

## Live check build (only this fix, off at launch)

Every dev fix toggles with I, so a live check build holds this fix alone:

```
cmake -S . -B build_probe -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ^
  -DTOOL_AUTORUN=OFF -DFIX_PATHFIX=OFF -DFIX_ARTILLERY_TARGETING=OFF -DFIX_FIRE_BY_RANK=OFF ^
  -DFIX_SIEGE_RESPONSIVENESS=OFF -DFIX_WALL_MANNING=OFF -DFIX_ZOMBIE_YARI_WALL=OFF ^
  -DFIX_WALL_COLLAPSE=OFF -DFIX_SINGLE_UNIT_GROUP=OFF -DFIX_GUARD_MODE_RANGED=ON -DGUARDFIX_START_OFF=ON
cmake --build build_probe
```

From an x86 MSVC environment (`vcvarsall.bat x86`); `build_probe\` is git-ignored. The log
(`<game>\pathfix_log.txt`) must say `Fixes compiled in: 1` and `fix OFF at launch`. I turns the
fix on (high tone), I again off (low tone). The harness's off scenario passes on this DLL (57
checks); its default scenario fails its launch and key checks by design.

## Analysis (local, `analysis/`)

```
python -I fixes\guard-mode-ranged-fix\analysis\guard_report.py "<game>\guardfix_runs\<stamp>"
```

Per unit: guard byte, men, morale, distance moved after the first enemy rout, missile tasks,
INTERCEPT entries, FIRE entries and those against shattered targets; then the answers the fix
changed per site.

Log every run in `docs/research/research_log.md`.
