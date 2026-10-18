# artillery-targeting-fix runbook: A/B runs in a replay

Everything runs unattended with the autorun tool (`tools/autorun/README.md`, Claude skill
`autorun-replay`). The game must be closed and Steam running.

## Install

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"
python tools\autorun\autorun.py --install
```

Copy `build\dinput8.dll` into the game folder. Keep the previous one first (the rule for untracked
files): `dinput8.dll.<name>.bak`; on 2026-10-18 the path fix v42 that was installed is kept as
`dinput8.dll.v42.bak`. The dev DLL holds the path fix (off at launch, I key) and the aim fix (on at
launch, U key).

## Runs

```
python -u tools\autorun\autorun.py "Artilery_shooting" --env ARTFIX_PROBE=1 --env ARTFIX_OFF=1
python -u tools\autorun\autorun.py "Artilery_shooting" --env ARTFIX_PROBE=1
python fixes\artillery-targeting-fix\tests\compare_runs.py "<game>\artfix_runs\<off stamp>" "<game>\artfix_runs\<on stamp>"
```

Each run takes about 3 minutes (30 s loading, 138 s of battle). The probe folder is named in
`<game>\pathfix_log.txt` (`artfix: probe on, writing ...`). Without `ARTFIX_PROBE` only the fix hook
is installed and nothing but the log is written.

## What to check

- `pathfix_log.txt`: `artfix: fix ON` (or OFF), one hook line per installed hook, and at exit the
  counter line; `unit check failed` should be 0 and there must be no `DISABLED` line.
- `compare_runs.py`: the same number of shots off and on (the fix must not delay or stop the
  guns), the aim point within a couple of metres of the target's centroid with the fix on.
- Reference (2026-10-18): off 60 shots, median 22.0 m, 47 % at an end; on 60 shots, median
  0.4 m, 0 % at an end.

## Probe files (`<game>\artfix_runs\<stamp>\`)

| File | One row per |
|---|---|
| `aim_calls.csv` | shot: target record, chosen soldier, gun (`rec - 0x71C`), target unit, centroid, soldiers nearest / farthest from the gun |
| `aim_settarget.csv` | target record a gun takes (`FUN_1045a8a0`), with the caller's action, its vtable and return-address candidates |
| `aim_units.csv` | soldier of a target unit, once per game second per unit |
| `aim_fix.csv` | change the fix made: gun, unit, centroid, old and new soldier |
| `aim_watch.csv` | write to a watched record (`ARTFIX_WATCH=1` only) |
| `aim_unitdiff.csv` | raw words of the first battery's unit, the target unit and their records (one-time) |
