# zombiefix runbook

How to build, test and measure the zombie yari wall fix. The engine decode is
`docs/re/rout_x86.md`; the design is in `../ARCHITECTURE.md`.

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

`test.bat` runs every fix's harness; zombiefix's runner is `tests/run_tests.py` (four modes: fix,
off, probe, corrupt). `check_prologues.py` must print `all N MATCH` and `no overlapping sites`.

## Replay runs (autorun)

Install the dev `dinput8.dll` in the game folder first (keep the user's own one under another
name and put it back afterwards). The test replay is `zombie_yariwall`. It was recorded with the
dev build of 2026-10-18 (artfix, fbrfix, siegefix, wallfix on, pathfix off), so use the normal dev
build with every fix at its default: a build with only this fix compiled in desyncs at 65.6 s.

```
python -u tools\autorun\autorun.py "zombie_yariwall" --speed 1 --env ZOMBIEFIX_PROBE=1 --env ZOMBIEFIX_OFF=1
python -u tools\autorun\autorun.py "zombie_yariwall" --speed 1 --env ZOMBIEFIX_PROBE=1
```

| Variable | Effect |
|---|---|
| `ZOMBIEFIX_OFF=1` | start with the fix off (I toggles) |
| `ZOMBIEFIX_PROBE=1` | write `<game>\zombiefix_runs\<stamp>\` (states, morale, groups, positions, men, raw dumps, kills, tasks, fix) |

The probe writes about 200 MB per run of this replay (the transition log is most of it). A run
with the fix on changes what the units do after they break, so the replay drifts from the
recording; compare per unit. The battle ends when it is decided (about 120 s in this replay); the
replay does not go on after that.

## Analysis (local, `analysis/`)

```
python -I fixes\zombie-yari-wall-fix\analysis\kills_after_rout.py "<game>\zombiefix_runs\<off>" "<game>\zombiefix_runs\<on>"
python -I fixes\zombie-yari-wall-fix\analysis\zombie_timeline.py "<game>\zombiefix_runs\<stamp>" --states --unit <hex>
python -I fixes\zombie-yari-wall-fix\analysis\find_kills_field.py "<game>\zombiefix_runs\<stamp>"
```

`kills_after_rout.py` is the user's zombie test: per routed unit the kill count (unit +0xD88)
when it broke and at its last dump, the men left, the distance moved in the 20 s after the rout,
and the fix's re-arms.

Log every run in `docs/research/research_log.md`.
