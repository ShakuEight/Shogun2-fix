# pathfix runbook (v41)

## Install

1. Close the game. Check that `Shogun2.exe` is not in the task list.
2. Build and test: `cmd /c build.bat`, then `cmd /c test.bat`. All runs must pass.
3. Copy `build\dinput8.dll` into the game folder as `dinput8.dll`. Keep the previous one as a
   rollback in `fixes\pathfix\archive\installed_dlls\dinput8.dll.vNN.bak`.

v41 is installed as of 2026-10-16. The v40 DLL it replaced is
`fixes\pathfix\archive\installed_dlls\dinput8.dll.v40.bak`.

## Play

- The path fix is **off** at launch. Press **I** in battle to switch it on: one high tone. Press
  I again to switch it off: one low tone. The key works at any time and only while the game
  window has the focus.
- Three long low tones mean a self-check failed and the fix switched itself off for the session.
  The reason is in `<game>\pathfix_log.txt`.
- O and P do nothing. Nothing is recorded; the log is rewritten at each launch.

## What to look for

A unit ordered through a gate, a doorway or round a hairpin should run through as a column and
re-form beyond it, instead of standing in a stretched block before the choke and filing through.
On open ground nothing changes. Compare by switching I off and on between orders.

## If something looks wrong

`pathfix_log.txt` lists the install checks, every I press and any self-check failure. For
measurements, start the game with the environment variable `PATHFIX_CAPTURE=1`. That restores
the v40 recording into `<game>\pathfix_runs\<stamp>\`, with O as marker and P to finish. Collect
the run with `python tools\runs\collect_runs.py` and analyse it as in `fixes/pathfix/README.md`.
The v40 capture protocol and pass marks are in `research/capture_v40_runbook.md` (local research
log, not in the public repository).

## Rollback

Close the game and copy `fixes\pathfix\archive\installed_dlls\dinput8.dll.v40.bak` over
`<game>\dinput8.dll`. To remove the mod, delete `<game>\dinput8.dll`.
