# single-unit-group-fix runbook

## Build and offline test

```
cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat -DTOOL_AUTORUN=ON"
cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"
python -I tools\re\check_prologues.py
```

The harness runs in four modes (fix 47, off 38, probe 56, corrupt 3 checks). It runs the game's
own group-of-the-selection function on a fake selection manager with 0, 1 and 2 selected units,
and the game's own frame count test on groups of 0 to 3 units (the frame drawing is a stub that
records the layout flag it got).
check_prologues.py must print `all N MATCH` and `no overlapping sites`. Stolen bytes come from
`docs/re/prologues_groupfix.txt`, `prologues_groupfix_ui.txt` and `prologues_groupfix_fix2.txt`
(local; regenerate with `tools\ghidra\ghx86.bat <log> dump_prologue.py <out> 5 <addresses>`, the
addresses as in those files).
Stack offsets: `src/stack_offsets_group.h` (entry hooks) and `src/stack_offsets_group_mid.h`
(the site after the count test, one push done), both from `tools/re/stack_calc.py`.

## Variables (dev build)

| Variable | Effect |
|---|---|
| `GROUPFIX_OFF=1` | fix off at launch (I turns it on) |
| `GROUPFIX_ON=1` | fix on at launch in a `GROUPFIX_START_OFF` build |
| `GROUPFIX_PROBE=1` | research probe into `<game>\groupfix_runs\<stamp>\group_events.csv` |

I toggles the fix: one high tone = on, one low tone = off, three long low tones = refused (at
install, or when the code page cannot be written).

## Replay runs (autorun)

Install `build\dinput8.dll` into the game folder first (keep the old one as a copy).

```
python -u tools\autorun\autorun.py "control group" --speed 4 --env GROUPFIX_PROBE=1
```

A replay re-issues the recorded battle commands, not the key presses: the probe sees the
engine's group create, add, remove and destroy, not the UI shortcuts. The fix itself changes no
battle command, so a replay stays in sync with it on or off.

## Live check build (all fixes, probe on at launch)

A game started from Steam gets no environment variables, so the live-check DLL has the probe on
by default. Build it outside `build/` (the harness expects the probe off), for example in a
scratch folder:

```
cmake -S . -B <scratch>\build_live -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DGROUPFIX_PROBE_DEFAULT=ON
cmake --build <scratch>\build_live
```

(from a VS x86 developer prompt, or with `vcvarsall.bat x86` first). The log must show
`groupfix: probe on, writing ...`.

## Live check (the player's path)

Custom battle, single player, land then naval. The game folder log `pathfix_log.txt` shows the
install (`groupfix: fix ON: G, Ctrl+G and Ctrl+number accept a single unit, its group frame is
shown`), for every group made from a single unit `groupfix: a group of one unit made from the
selection (slot N)`, and `groupfix: first frame of a one-unit group placed` once.

1. One unit selected, G: a group card with a number.
2. G again on that unit: the group is dissolved.
3. One unit, Ctrl+3: group 3; 3 selects it.
4. One unit, Ctrl+G: locked group.
5. A group of one: move, rotate, change the group formation, add a second unit, remove it again.
6. I: low tone, one unit is refused again; I: high tone.

With the probe on every step leaves a row in `group_events.csv` (event, selected count, the
group's unit counts, caller and stack). For G, compare a G press with two units selected and
with one: rows `key_G_button` (column arg2 = the flag that lets the click run),
`group_button_state_<name>` and `group_button_visible` (the button's state changes and their
callers), `key_G` (the G handler reached), `ui_groups` (the UI groups and their slots).

## Live check build (only this fix, off at launch)

```
cmake -S . -B build_probe -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ^
  -DFIX_PATHFIX=OFF -DFIX_ARTILLERY_TARGETING=OFF -DFIX_FIRE_BY_RANK=OFF -DFIX_SIEGE_RESPONSIVENESS=OFF ^
  -DFIX_WALL_MANNING=OFF -DFIX_ZOMBIE_YARI_WALL=OFF -DFIX_WALL_COLLAPSE=OFF -DFIX_SINGLE_UNIT_GROUP=ON ^
  -DGROUPFIX_START_OFF=ON
cmake --build build_probe
```

(from a VS x86 developer prompt, or with `vcvarsall.bat x86` first). The log must show
`Fixes compiled in: 1` and `OFF at launch`.

Log every run in `docs/research/research_log.md`.
