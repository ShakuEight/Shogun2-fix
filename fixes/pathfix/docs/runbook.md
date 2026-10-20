# pathfix runbook (v43)

## Install

1. Close the game. Check that `Shogun2.exe` is not in the task list.
2. Build and test: `cmd /c build.bat`, then `cmd /c test.bat`. All runs must pass.
3. Copy `build\dinput8.dll` into the game folder as `dinput8.dll`. Keep the previous one as a
   rollback (`<game>\dinput8.dll.vNN.bak`, or `fixes\pathfix\archive\installed_dlls\`).

As of 2026-10-19 the game folder's `dinput8.dll` is the dev build with every fix compiled in
(pathfix v43 and the five other fixes, `build\dinput8.dll`, SHA-256 F339B42E...). The
pathfix-only v43 build used for the replay runs is `<game>\dinput8.dll.pathfix43`; the older
copies (`.artifix`, `.fbr`, `.manningfix`, `.wallfix`, `.zombie`) are untouched.

## v43: yari wall

v43 adds the yari wall part of G (Y): a unit in yari wall is laid along the path, like a block,
wherever its straight wall would not fit (a node narrower than the wall, or a bend inside it);
the wall itself forms as the game lays it at the destination. For a yari wall, G does not anchor
the front 2 m before the path end when a narrow run reaches the end (the straight wall laid there
early sent men up parallel passages). G's block depth for a yari wall comes from its own files
(the player's drag sets them). It is part of the I toggle; nothing else to press. Comparison
runs: `PATHFIX_YARI=0` keeps G exactly as v42 (all of Y off).

For replays recorded with I presses: `PATHFIX_TOGGLE_TICKS=t1,t2,...` (ascending game ticks)
flips the fix at each tick, as the I key would, so the replay plays with the same fix states.
The ticks come from the recording's `pathfix_log.txt` (the minute summaries give the tick, the I
lines the time). Each flip is logged as `PATHFIX_TOGGLE_TICKS: flip n of N at tick ...`.

New capture columns: orders.csv `gtick, form_vt, form_f0..f3, form_x1c, yari, groups,
req_width`; new `yari.csv` (one row per decided slot call: width, files, ranks, block span,
narrowest node, sharpest turn, laid by the block generator or not); release.csv `depth_kind`;
events.csv rows `FUN_1014e870` (formation change, ECX = the army unit). Analysis:
`fixes/pathfix/analysis/yariwall_passes.py <run>` (local).

FUN_10154180 is not hooked by pathfix (zombie-yari-wall-fix's probe site).

## Play

- The path fix is **off** at launch. Press **I** in battle to switch it on: one high tone. Press
  I again to switch it off: one low tone. The key works at any time and only while the game
  window has the focus.
- Three long low tones mean a self-check failed and the fix switched itself off for the session.
  The reason is in `<game>\pathfix_log.txt`. A naval battle triggers this every time (apex match;
  README "Known limitations"); it is not a fault of the land battle that follows.
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
