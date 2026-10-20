# single-unit-group-fix (groupfix): history and working notes

Feature request: groups (G soft, Ctrl+G / Ctrl+number locked) of a single unit. Shared rules,
paths and engine knowledge are in the root CLAUDE.md; the engine decode is
`docs/re/unit_groups_x86.md`; the design is in `ARCHITECTURE.md`. Local only (git-ignored):
`docs/research/` (research log), `analysis/`.

## Session 1, 2026-10-19 (dev build, not released)

- Static: shortcut names in `data/text/default_keys.xml`; dispatcher `FUN_10fa37e0`; G
  `FUN_10f9de40`, Ctrl+number `FUN_10f91e40`; both end in `FUN_10f89a40`, whose first test
  (selected count at manager +0x28 against 1) is the only refusal on the player's path. Engine
  executor of BCQ_ARMY_CREATE_UNIT_GROUP `FUN_102a5e40`, unit add / remove / destroy: no count
  test. The "2 or more units" strings are the Lua unit_controller's (`FUN_101c4a60`,
  `FUN_10202520`).
- Fix: immediate patch at RVA 0xF89A46 (1 to 0), whole 6-byte compare + branch checked
  (`docs/re/prologues_groupfix.txt`, local). Dev log hook at RVA 0xF89A49 (after the test, one
  push done: `src/stack_offsets_group_mid.h`). Probe-only hooks on G, Ctrl+number, the lock
  button handler 0x11211d20, group finalise, unit add, unit remove, group destroy.
- Harness `tests/groupfix_test.cpp`: fix 26, off 21, probe 29, corrupt 3; test.bat all passed.
  The harness rebases the absolute addresses in two sites' stolen bytes before comparing (the
  mapped DLL is relocated).
- Replay `control group.replay` (user, 22:21, repeated group / ungroup of two units, recorded with
  the collapsefix session build), speed 4, probe on, fix on: run 20261019_224722, 30 rows, every
  group had two units; some groups' units sit in the group's second list (+0x54, count +0x60)
  rather than the first (+0x14, count +0x20); not decoded (possibly soft against locked). Battle
  ended normally after 8 s of battle, no new replay desync log.
- Game folder: at the start `dinput8.dll` was the collapsefix session build (SHA-256
  AAD7496D..., kept as `dinput8.dll.pre_groupfix`); now the session build with groupfix (all
  eight fixes; SHA-256 96668DAE...), copy `dinput8.dll.groupfix`. `version.dll` unchanged.

## Session 1, part 2, 2026-10-19 (user live check of the first build)

- User: Ctrl+number and Ctrl+G make a locked group of one, Ctrl+G again makes it soft; G does
  nothing; the yellow frame with the number around the card is missing. Replay
  `control group 2.replay` (22:54) has the single-unit groups. The game folder log of the live
  session shows two `group of one unit made` lines (slots 1 and 0).
- Replay `control group 2` with the probe: four one-unit groups made by the engine, add / remove /
  destroy fine, ended normally. The selection manager singleton is null in replay playback: the
  UI side cannot be studied from a replay.
- G routing (static): G maps to `button_group`; its shortcut slot `FUN_11065720` runs the click
  only with its third argument set; click and Ctrl+G-on-ungrouped both end in `FUN_11100f50`,
  which runs the G handler. So the group button refuses G for one unit (presumably inactive).
  State setter not found (scans for count compares with 1 / 2, getters, the button classes, the
  "Inactive" users). Probe added: group button constructor (component at arg1), its shortcut
  (arg3), `FUN_10f9bf20` state-by-name and `FUN_10f9c360` visibility on its components, UI
  groups sampler, frame rebuild, UI events, card manager shortcuts.
- Frame (static): `FUN_11223470` hides the frame for a count of 1 or less (CMP [EBP+4],1 / JA at
  +0xF). Fix: second immediate patch (RVA 0x1223482) + entry hook clearing the two-row flag for
  a one-unit group (the two-row odd-count branch reads a second card).
- Harness: fix 47, off 38, probe 56, corrupt 3 (frame scenes with a stub on the drawing path
  recording the flag). test.bat all passed.
- Live-check DLL: built in the scratchpad with `GROUPFIX_PROBE_DEFAULT=ON` (all fixes), installed
  as `dinput8.dll` (copy `dinput8.dll.groupfix_live`); earlier session builds kept as
  `dinput8.dll.groupfix`, `.groupfix_v1`, `.groupfix_v2`, `.groupfix_v3`, `.groupfix_live_a`.
  Smoke run on `control group 2`: probe on without variables, both group buttons tracked.

## Session 1, part 3, 2026-10-19 (second live check)

- User: the frame and number show for a one-unit group; G still nothing. Probe run
  20261019_232226 (live): `group_button_state_inactive` at sel=1, `_active` at sel=2,
  `_selected` on a group, all from `FUN_110e22f0` called by `FUN_111024e0`; no `key_G_button`
  row with one unit (the inactive button swallows the key). Ctrl+1 made a one-unit locked group
  (engine finalise 1); G on two units made a UI-only group (slot 1, no engine command).
- `FUN_111016d0` (the panel's state table) entry +0x240: active only if the selected count
  `[ESP+0x38]` > 1 and room for a group. Third fix site: imm at RVA 0x1101A3A (offset 4 in the
  7-byte CMP / JBE). `docs/re/prologues_groupfix_fix3.txt`.
- Harness unchanged in count (the immediates check covers all three): 47 / 38 / 56 / 3.
- Replay `control group 3` (Ctrl+1 on one, G on one, G on two, G on one), third build: one
  engine group, in sync. Live DLL `dinput8.dll` = `dinput8.dll.groupfix_live` (SHA-256
  09F872B6...); previous live DLLs `.groupfix_live_b`, `.groupfix_live_b2`.

## Session 2, 2026-10-20 (third live check)

- User: works as wanted on land and at sea; three long low tones heard once in the naval battle.
  Probe run 20261020_074853: 20 groups from the selection (17 of one unit), G button key rows
  with one unit, group button "selected" after G, no faults.
- The tones were pathfix's self-check alarm, not groupfix: `FIX DISABLED: apex match` (110 of
  110 type-0 apex calls failed) in the naval battle, while pathfix was off anyway. Reproduced by
  replay `control group 4 naval` (speed 4, exit 0, no group commands in the replay). Noted in
  fixes/pathfix/CLAUDE.md.

Pitfalls met:
- Bash heredocs containing an apostrophe break ("unexpected EOF"); write Python edit scripts to
  the scratchpad with the Write tool and run them from there.
- Python edits fed through a Bash heredoc silently failed to match strings with backslashes
  (`test.bat` paths, C escapes); use the Edit tool or byte escapes (`\x5c`).

Next:
- Trim the probe for the release, public release source and the combined package
  (fixes/README.md).
- Public release source and the combined package (fixes/README.md).
