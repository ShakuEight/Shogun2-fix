# siegefix runbook

How to build, test and measure the siege responsiveness fix. The engine decode is
`docs/re/path_queue_x86.md`; the design is in `../ARCHITECTURE.md`.

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

`test.bat` runs every fix's harness; siegefix's runner is `tests/run_tests.py` (seven modes).
`check_prologues.py` must print `all N MATCH` and `no overlapping sites`.

## Replay runs (autorun)

The game folder normally holds no active `dinput8.dll` (the user plays vanilla). For a run, copy
`build\dinput8.dll` into the game folder, and afterwards rename it to `dinput8.dll.siegefix`.

```
python -u tools\autorun\autorun.py "wall siege bug" --speed 1 --env SIEGEFIX_PROBE=1 --env SIEGEFIX_OFF=1 --env ARTFIX_OFF=1 --env FBRFIX_OFF=1
```

| Variable | Effect |
|---|---|
| `SIEGEFIX_OFF=1` | start with the fix off (J toggles) |
| `SIEGEFIX_PROBE=1` | write `<game>\siegefix_runs\<stamp>\siege_ticks.csv` and `siege_requests.csv` |
| `SIEGEFIX_BUDGET=<n>` | queued-request node budget per tick, 500..20000 (default 2000; the game 500) |
| `SIEGEFIX_INCR=<n>` | incremental search cap per tick, 200..20000 (default 200, unchanged) |
| `SIEGEFIX_PLAYER_SYNC=0` | leave the local side's requests in the queue |

Pathfix starts off in the dev DLL; `ARTFIX_OFF=1` and `FBRFIX_OFF=1` keep the other two fixes out
of a siegefix measurement. A probe-only run (fix off) stays in sync with a vanilla recording; a run
with the fix on changes path timing, so the replay drifts from the recording
(`replay_desync.txt`) and its outcome may differ. Compare runs per request (wait in ticks, queue
length), never by the battle's result.

## Analysis

```
python fixes\siege-responsiveness-fix\analysis\queue_wait.py "<game>\siegefix_runs\<stamp>"
```

Log every run (stamp, replay, variables, result) in `docs/research/research_log.md`.
