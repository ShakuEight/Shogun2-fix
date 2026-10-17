# pathfix runbook (v42)

## Install

1. Close the game. Check that `Shogun2.exe` is not in the task list.
2. Build and test: `cmd /c build.bat`, then `cmd /c test.bat`. All runs must pass.
3. Copy `build\dinput8.dll` into the game folder as `dinput8.dll`. Keep the previous one as a
   rollback (`<game>\dinput8.dll.vNN.bak`, or `fixes\pathfix\archive\installed_dlls\`).

v42 is installed as of 2026-10-17. The v40 DLL it replaced is `<game>\dinput8.dll.v40.bak`;
v41 is still in the game folder as `dinput8.dll.41`.

## Play

- The path fix is **off** at launch. Press **I** in battle to switch it on: one high tone. Press
  I again to switch it off: one low tone. The key works at any time and only while the game
  window has the focus.
- Three long low tones mean a self-check failed and the fix switched itself off for the session.
  The reason is in `<game>\pathfix_log.txt`.
- Every line of `<game>\pathfix_log.txt` starts with the local time `[hh:mm:ss.mmm]`. The file is
  rewritten at each launch.

## Recording

- Press **P** to start a recording: three rising notes. Press P again to stop it: three falling
  notes. Start and stop as often as you like in one session, in battle or in a replay.
- Each recording gets its own folder `<game>\pathfix_runs\<YYYYMMDD_HHMMSS>\` with the CSV files
  (`pathfix_capture_v42_*.csv`) and a `pathfix_log.txt` that copies the session log while the
  recording runs. `timestamp_ms` 0 in the CSV files is the P press; its local time is in the
  folder's log header.
- **O** puts a marker into the running recording (O without a recording is only logged).
- Three long low tones on P mean the recording was refused (no memory for the buffers or no
  folder); the reason is in the log.
- Closing the game while recording stops the recording and writes its files.
- Long recordings fill the buffers (soldier rows first, after a few minutes of movement); the log
  then warns and later rows are dropped. Stop and start a new recording for the next test.
- Collect the folders with `python tools\runs\collect_runs.py`.

## What to look for

A unit ordered through a gate, a doorway or round a hairpin should run through as a column and
re-form beyond it, instead of standing in a stretched block before the choke and filing through.
On open ground nothing changes. Compare by switching I off and on between orders.

v42 test: replay `4 kyoto police running.replay` with the fix on (I before the first order). All
four units should reach the gatehouse spot and then start their queued second order. Release rows
with reason `park` show the new rule acting.

## If something looks wrong

`pathfix_log.txt` lists the install checks, every I and P press and any self-check failure.
`PATHFIX_CAPTURE=1` (the harness) still gives the v40 capture mode: one window from the first
hook hit, O marker, P finish.

## Rollback

Close the game and copy `<game>\dinput8.dll.v40.bak` (or `dinput8.dll.41`) over
`<game>\dinput8.dll`. To remove the mod, delete `<game>\dinput8.dll`.
