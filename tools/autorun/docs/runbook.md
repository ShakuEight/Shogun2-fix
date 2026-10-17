# autorun runbook

## One-time setup

1. `cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"` (CMake caches the option; later
   `build.bat` calls keep building `build\version.dll`).
2. `python tools\autorun\autorun.py --install` copies `build\version.dll` into the game folder
   (an existing different `version.dll` is kept as `version.dll.bak`, `.bak2`, ...). Run it again
   after every rebuild; the driver warns when the installed copy differs from the build.
3. Steam must be running and logged in. The game folder's `steam_appid.txt` lets `shogun2.exe` start
   directly.

Uninstall: delete `version.dll` from the game folder. Nothing else is changed permanently.

## A run

```
python tools\autorun\autorun.py "4 kyoto police running" --capture
```

Expected transcript (times from run 20261017_230617):

| Time after launch | Line | Meaning |
|---|---|---|
| 0.5 s | `connected to \\.\pipe\s2autorun_<pid>` | proxy loaded in the game |
| 2 s | `event state FRONTEND` | game window exists (no frontend is shown in replay mode) |
| ~18 s | `event phase 0`, `event state LOADING` | battle objects exist, clock at tick 0 |
| ~43 s | `event phase 3`, `event state BATTLE`, `event phase 5` | clock running (deployment phase 3, then fighting phase 5), 10 ticks per second |
| end | `event phase 10` (decided = 7, 9, 10 or 11), `event state BATTLE_OVER` | battle decided (tick 5132 in this run, 470 s of battle) |
| +2 s | `> close`, `ok close`, `event state EXITING` | WM_CLOSE posted |
| +3 s | `game exited (code 0)` | clean shutdown |

Afterwards `user.script.txt` is restored, and `tools/autorun/data/runs/<stamp>/` holds
`driver.txt` and `autorun_log.txt`. With `--capture` the pathfix run folder named in the transcript
also gets `autorun_driver.txt` and `autorun_log.txt`.

## Troubleshooting

| Symptom | Cause / action |
|---|---|
| `ERROR ... version.dll missing` | `autorun.py --install` |
| `no proxy pipe after 60 s` | proxy not loaded: check `<game>\autorun_log.txt`; a non-game process never logs |
| stays `FRONTEND`, then `TIMEOUT in phase load` | replay did not open (bad path: the engine falls back to the frontend, which nobody clicks), or the log says `unsupported empire.retail.dll build` |
| `event stalled` ends the run | the clock stopped for 30 s without a decided phase (pause, or a replay that ends undecided); `--no-stall-end` to wait for the timeout instead |
| a previous run died | the next run restores `user.script.txt.autorun_bak` first; by hand: rename it back to `user.script.txt` |
| game window pops up at the end-of-battle panel | expected without the proxy; with it (`AUTORUN_MINIMIZE=1`) it is minimized again within a second |

After a game update: `SizeOfImage` changes, the proxy refuses to read memory, runs time out. Re-find
the path with `discover_chain.py` (below) and update `src/game_offsets.h`.

## Discovery record (2026-10-17)

How the mechanism was found, so it can be repeated. Details: `docs/re/console_commands_x86.md`.

1. **Command dump.** `docudemon;` appended to `user.script.txt` wrote `<game>\docudemon.txt`, the
   command library (no replay loader in it). `help <file>;` wrote nothing.
2. **Window.** Started with `SW_MINIMIZE` the game stayed minimized and never took the focus; it
   used one core at the frontend. A posted `WM_CLOSE` quit cleanly in 2.9 s from the frontend.
3. **Replay startup mode.** The strings `replay` and `loop_replay` led in Ghidra
   (`tools/ghidra/find_string_refs.py`, new) to the `game_startup_mode` handler and the startup
   function's `replay` branch. `game_startup_mode replay "<path>";` in `user.script.txt` started
   the game straight into the replay (launch 2, 22:37): battle clock at about 40 s, real-time
   speed, battle decided at tick about 5086 (509 game s), then the end-of-battle panel ("Close
   Defeat") waiting for a click. `force_exit_after_battle true;` (the real name behind the
   `exit_after_battle` usage text) did not dismiss it. `WM_CLOSE` at the panel quit cleanly
   (2.9 s, pathfix `SHUTDOWN`).
4. **Minimized battle.** Minimizing mid-battle: 101 ticks in 10 s, so the battle runs at full
   speed minimized. In launch 2 the window was restored at some point before the panel; in
   launch 3 it stayed minimized to the end. The proxy re-minimizes it either way.
5. **Battle-end signal.** The pathfix log names the live clock object; `discover_chain.py`
   searched forward from the game-shell global (found in the startup function) for it: 12 paths
   at depth 3..4, the shortest through an object with a vtable in the module (used as a check).
   Valid unchanged in the next process. A first proxy build used the tick object's byte +0x58
   as "battle over" and stopped a run 26 s into the battle (run 20261017_230406): that byte is
   also set on frames without a tick. The phase of the battle state machine ([tick object +
   0xB4] + 0x14, decided = 7, 9, 10, 11) replaced it.
6. **Speed.** `replay_speed 4;` in `user.script.txt` ran the battle at 40 ticks per second
   (run 20261017_231514, 40 s of battle to tick 1973), so `--speed` works.
7. **Not used.** Runtime console commands (would need the engine's command executor, not
   reverse-engineered), Lua (`LoadBattleReplay` exists for the frontend UI), menu automation.
