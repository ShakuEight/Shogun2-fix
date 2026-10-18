# autorun: unattended replay runs

Developer tool: start Total War: Shogun 2 minimized, play one battle replay to its end, quit the
game, without touching the game's UI. Built for repeatable pathfix captures, usable for any fix.
It is a separate proxy DLL (`version.dll`) next to the fixes' `dinput8.dll`, so tools and fixes
stay independent; it is never part of a public release.

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"     build\version.dll + forwarding test
python tools\autorun\autorun.py --install                          copy it into the game folder
python tools\autorun\autorun.py "4 kyoto police running" --capture --collect
```

Claude Code uses the project skill `.claude/skills/autorun-replay/SKILL.md` to run replays with this
tool. Steam must be running. A run takes the replay's battle length (divided by `--speed`) plus about
45 s of loading.

## How it works

| Step | Who | What |
|---|---|---|
| Load the replay | game, via `user.script.txt` | The driver appends `game_startup_mode replay "<file>";` to `%APPDATA%\The Creative Assembly\Shogun2\scripts\user.script.txt` (backed up first, restored afterwards). The engine's own replay startup mode skips the frontend and opens the replay directly. |
| Start minimized | driver + proxy | `shogun2.exe` starts with `SW_SHOWMINNOACTIVE`; with `AUTORUN_MINIMIZE=1` the proxy re-minimizes the window whenever the game restores it. The battle runs at full speed while minimized. |
| Detect the end | proxy | The proxy reads the battle tick and the engine's battle-over byte through a fixed pointer path (`src/game_offsets.h`, guarded reads, vtable check) and reports `BATTLE`, `BATTLE_OVER` or a 30 s tick stall. |
| Quit | proxy | On `close` it posts `WM_CLOSE`; the game shuts down cleanly in about 3 s (pathfix writes its `SHUTDOWN` line). |

Reverse-engineering notes behind this: `docs/re/console_commands_x86.md`. Day-to-day use and the
discovery record: `docs/runbook.md`.

## Driver options (`autorun.py`)

| Option | Effect |
|---|---|
| `<replay>` | Path to a `.replay`, or a name in `%APPDATA%\...\Shogun2\replays\` (with or without `.replay`) |
| `--capture` | Start with `PATHFIX_CAPTURE=1`: the dev `dinput8.dll` records into `<game>\pathfix_runs\<stamp>\`; the driver copies its transcript and the proxy log there |
| `--collect` | With `--capture`: run `tools/runs/collect_runs.py` afterwards |
| `--no-minimize` | Show the window normally (for watching a run) |
| `--keep-open` | Leave the game running at the end-of-battle panel |
| `--timeout N` | Battle phase limit, default 3600 s; `--load-timeout` launch-to-battle, default 300 s |
| `--no-stall-end` | Do not treat a 30 s tick stall as the end |
| `--speed N` | Adds `replay_speed N;`: the battle runs N times faster (measured: 4 gives 40 ticks per second instead of 10). pathfix's fix windows run on game time, but its capture samples on the wall clock, so captures get N times fewer samples per game second |
| `--script-line "x y;"` | Any extra `user.script.txt` line for this run |
| `--env NAME=VALUE` | Extra environment variable for the game process (repeatable), e.g. `ARTFIX_OFF=1` or `ARTFIX_PROBE=1` for the artillery targeting fix's A/B and probe runs |
| `--verbose` | Also print the proxy's 5 s heartbeats |

Exit code 0 = battle played to its end and the game closed, 1 = setup error, 2 = timeout,
3 = the game exited before the battle ended. Every run leaves `driver.txt` (timestamped
transcript) and `autorun_log.txt` (proxy log) in `tools/autorun/data/runs/<stamp>/` (local only).

## Proxy (`build\version.dll`)

- Forwards all 17 exports of `C:\Windows\SysWOW64\version.dll` with jump stubs. In any process
  other than `shogun2.exe` / `shogun2.retail.exe` (the CEF and Awesomium helpers in the game folder
  load version.dll too) it does nothing else.
- In the game: writes `<game>\autorun_log.txt` (shared `proxy/log.cpp`, own file name), starts a
  watcher thread (250 ms) and a pipe server on `\\.\pipe\s2autorun_<pid>`.
- Installs no code hooks, no exception filter and reads no keys; it cannot conflict with the
  fixes in `dinput8.dll`.
- Refuses to read game memory on another build (SizeOfImage check); the state then stays
  `FRONTEND`, and the driver times out.

Pipe protocol (one client, UTF-8 lines):

| Client sends | Proxy answers |
|---|---|
| `ping` | `ok ping` |
| `state` | `ok state <STATE> tick=<n> over=<0/1> iconic=<0/1> hwnd=0x...` |
| `minimize`, `restore` | `ok minimize`, `ok restore` (restore also stops keep-minimized) |
| `keep-minimized on` / `off` | `ok keep-minimized on` / `off` |
| `close` | `ok close` (posts WM_CLOSE) |
| `cmd <console line>` | `err cmd ...`: console commands at runtime are not supported; startup commands go into `user.script.txt` |

Unsolicited: `event hello pid=.. hwnd=.. state=.. build=..` on connect, `event state <STATE>
tick=<n>` on every change (`STARTING`, `FRONTEND`, `BATTLE`, `BATTLE_OVER`, `EXITING`),
`event stalled tick=<n> secs=30`, `event minimized`, `event heartbeat ...` every 5 s.

## Tests

`build\autorun_forward_test.exe build\version.dll` (also run by `test.bat`): loads the proxy in a
non-game process and checks that all exports exist, that version queries give byte-identical
results through the proxy and the real DLL, and that no log file is written (8 checks).

## Files

| File | Content |
|---|---|
| `autorun.cmake` | Build wiring (root `CMakeLists.txt`, option `TOOL_AUTORUN`) |
| `src/version.def`, `src/version_proxy.cpp` | Export forwarding |
| `src/dllmain.cpp`, `src/autorun.cpp` | Host check, watcher, pipe server |
| `src/game_offsets.h` | The pointer path to the battle tick object (RVAs only) |
| `autorun.py` | The driver |
| `discover_chain.py` | Read-only pointer-path search in the running game (how the path was found; re-run it after a game update) |
| `tests/forward_test.cpp` | Offline forwarding test |
| `docs/runbook.md` | Use, troubleshooting, discovery record |
