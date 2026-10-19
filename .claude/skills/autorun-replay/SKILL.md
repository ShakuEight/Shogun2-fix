---
name: autorun-replay
description: Play a Total War Shogun 2 battle replay unattended (game started minimized, closed when the battle is decided) with tools/autorun, optionally recording a pathfix capture. Use whenever a task needs the game run, a replay played, a capture or test run in the live game, or checking a fix build in battle, instead of asking the user to click through the game.
---

# Unattended replay runs (tools/autorun)

The tool starts `shogun2.exe` minimized straight into a replay (no frontend), waits until the
engine marks the battle as decided, closes the game and restores everything it changed. No
clicks, no focus stealing. Details: `tools/autorun/README.md`, `tools/autorun/docs/runbook.md`,
`docs/re/console_commands_x86.md`.

## Before a run

1. The game must not be running and Steam must be running (the driver checks both and stops
   with `ERROR ...` otherwise). Never kill a game the user started; ask instead.
2. The proxy must be built and installed:
   ```
   cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"
   python tools\autorun\autorun.py --install
   ```
   Re-run `--install` after every rebuild (the driver warns `WARNING the game folder's
   version.dll differs`). `--install` keeps a different existing `version.dll` as `.bak`.
3. If a fix build is under test, install that `dinput8.dll` first, following the fix's own
   rules (fixes/<name>/CLAUDE.md); the autorun tool never touches `dinput8.dll`.
4. Pick the replay: a name in `%APPDATA%\The Creative Assembly\Shogun2\replays\` (with or
   without `.replay`) or a full path. The pathfix test replay is `4 kyoto police running`.

## Run

Runs take minutes, so start them in the background and wait for the completion notice
(do not poll or sleep in a loop):

```
python -u tools\autorun\autorun.py "<replay>" [--capture] [--speed N] [--collect]
```

| Option | Use |
|---|---|
| `--capture` | `PATHFIX_CAPTURE=1`: the dev dinput8 records into `<game>\pathfix_runs\<stamp>\` |
| `--collect` | with `--capture`: copy the run into `fixes/pathfix/data/runs/` afterwards |
| `--speed N` | battle N times faster (4 = 40 ticks/s). Captures get N times fewer samples per game second; use 1 for measurements, 3..4 for smoke tests |
| `--timeout N` | battle limit in seconds (default 3600); `--load-timeout` launch to battle (300) |
| `--no-minimize`, `--keep-open` | only when the user wants to watch or inspect the end screen |
| `--script-line "cmd args;"` | extra `user.script.txt` line for this run only |
| `--env NAME=VALUE` | extra environment variable for the game (repeatable): `ARTFIX_OFF=1` artillery targeting fix off, `ARTFIX_PROBE=1` its capture into `<game>\artfix_runs\` (fixes/artillery-targeting-fix/docs/runbook.md); `FBRFIX_OFF=1` / `FBRFIX_PROBE=1` the same for the fire-by-rank fix (`<game>\fbrfix_runs\`, fixes/fire-by-rank-fix/docs/runbook.md); `WALLFIX_OFF=1` / `WALLFIX_PROBE=1` the same for the wall manning fix (`<game>\wallfix_runs\`, fixes/wall-manning-fix/docs/runbook.md); `ZOMBIEFIX_OFF=1` / `ZOMBIEFIX_PROBE=1` the same for the zombie yari wall fix (`<game>\zombiefix_runs\`, fixes/zombie-yari-wall-fix/docs/runbook.md); `PATHFIX_ON=1` starts the pathfix dev build with its fix on; `PATHFIX_TOGGLE_TICKS=t1,t2,...` flips it at those game ticks (a replay recorded with I presses), `PATHFIX_YARI=0` keeps G without its yari wall part (v43) |
| `--verbose` | include the 5 s heartbeats (tick, phase) in the transcript |

Timing at speed 1: about 45 s loading, then the battle in real time (the test replay: 470 s of
battle, 518 s in total).

## Reading the result

Exit code and the transcript's last lines tell what happened:

| Exit | Meaning | Next |
|---|---|---|
| 0 | `battle ended: battle_over` (or `stalled`), `game exited (code 0)` | use the run |
| 1 | setup error (`ERROR ...` lines) | fix the precondition, rerun |
| 2 | `TIMEOUT in phase load` or `battle` | load: replay path wrong (the game falls back to the menu) or unsupported game build (see `autorun_log.txt`); battle: raise `--timeout` |
| 3 | game exited or pipe broke before the battle ended | crash? check `pathfix_log.txt` / `crash.dmp` in the run folder |

Every run leaves `tools/autorun/data/runs/<stamp>/driver.txt` (transcript) and
`autorun_log.txt` (proxy log). With `--capture` the transcript names the pathfix run folder,
which also gets copies of both files; pathfix's log there must end with `SHUTDOWN`.

Expected state sequence: `STARTING`, `FRONTEND` (window up), `LOADING` (phase 0, tick 0),
`BATTLE` (phase 3 deployment, then 5), `BATTLE_OVER` (phase 7, 9, 10 or 11), `EXITING`.

Report to the user: the replay, speed, battle length, how it ended, exit code, and the run
folder. A replay played with a movement fix active desyncs from the recording
(`%APPDATA%\...\Shogun2\logs\replay_desync.txt`), so its outcome may differ from the original
battle; say so when comparing runs.

## If something is off

- A run that died leaves `user.script.txt.autorun_bak`; the next run restores it first. By
  hand: rename it back to `user.script.txt` (never delete either file).
- `event stalled` ended the run: the clock stopped for 30 s without a decided phase. Rerun with
  `--no-stall-end` if the battle should continue.
- After a game update the proxy refuses to read memory (SizeOfImage check) and runs time out.
  Re-find the pointer path with `python tools\autorun\discover_chain.py` (needs a pathfix dev
  log naming the clock object) and update `tools/autorun/src/game_offsets.h`.
- The pipe accepts `ping`, `state`, `minimize`, `restore`, `keep-minimized on|off`, `close`
  for manual control (`\\.\pipe\s2autorun_<pid>`); runtime console commands are not supported.
