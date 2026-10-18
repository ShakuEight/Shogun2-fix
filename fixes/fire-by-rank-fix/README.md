# fire-by-rank-fix: matchlock units fire sooner, and the Hattori / Ikko Ikki units keep firing

Code name `fbrfix` (identifiers, environment variables, log prefix).

**Problem.** Matchlock units with the Fire by Rank ability take a long time to open fire, much
longer than with fire at will. The Hattori and Ikko Ikki gun units are worse: with Fire by Rank
they fire one volley per rank for three ranks and then never fire or reload again.

**Cause** (decode in [docs/re/missile_fire_x86.md](../../docs/re/missile_fire_x86.md)). Fire by
rank is the game's group fire task. It puts the men in groups by rank (at most 3), waits until
every man of the current group is "loaded and ready", orders the group to fire, waits until
every man has fired, gives the group reload orders, and moves on to the next group.

- **The wait counts every man.** Before the first volley the group waits for its slowest man to
  finish his ready pose, about 10 s although everyone is loaded. Later waits hang on stragglers
  in the same way.
- **The Hattori and Ikko Ikki gun units have a sword officer.** Their officer is
  `officer_katana` (katana animations, no gun) instead of `officer_teppo`. He stands in the front
  group. After its first volley he gets the group's kneeling reload order, which he can never
  finish. When the rotation comes back to the front group, the wait for him never ends, and the
  whole unit stops.

**Fix.** Two entry hooks on the group fire task.

- No ready or reload order for a man outside his unit's shooter class (the sword officer is the
  only such man seen), and the group does not wait for him.
- The group fires once every shooter of it is loaded and either ready or 1.5 game seconds into
  the wait, instead of waiting for the last man's ready pose.

Reloading itself is untouched: a man still reloading keeps his group waiting, as in the game.

State: dev build only (2026-10-18), not in a public release yet.

## Measured (replays `firebyrank_generic`, `_ikkoikki`, `_hattori`)

Two fire-by-rank units per replay. "Delay" runs from the first enemy soldier within 100 m (the
matchlock's effective range) of the unit to its first shot. "Hang" is a wait still open when the
battle ended. Each replay with the fix off and on, through `tests/compare_runs.py`. "Off" is a
probe build without the fix hooks; the harness's `off` mode checks that `FBRFIX_OFF=1` gives the
game's own answers. A Hattori re-run gave identical numbers.

| Replay | Delay, fix off | Delay, fix on | First wait off / on | Volleys off / on | Hang off / on |
|---|---|---|---|---|---|
| Hattori | 16.4 s, 13.3 s | 8.1 s, 4.3 s | 10.0 / 1.6 s | 3, 3 / 6, 6 | 20.8 s, 20.0 s / none |
| Ikko Ikki | 14.7 s, 9.5 s | 5.5 s, 2.1 s | 10.6 / 1.6 s | 3, 3 / 3, 3 | (battle ends while reloading) |
| Generic | 16.0 s, 12.3 s | 7.2 s, 4.8 s | 9.3 / 1.6 s | 3, 3 / 3, 3 | none |

Fire-at-will units in the same replays: 1.9 to 5.8 s, unchanged.

The battles are short (they are decided 30 to 50 s after contact), so the Ikko Ikki and generic
units only reach their fourth volley after the end of the battle with or without the fix. In the
Hattori replay the fix doubles the volleys, because the units no longer stop. With a changed
simulation the replay no longer follows its recording exactly, so the casualties differ between
the runs.

## Option: every rank fires

The game caps fire by rank at 3 groups. In a formation deeper than 3 ranks the rear ranks never
fire: 40 of 160 men in the 4-rank Hattori units, 65 of 200 in the 5-rank Ikko Ikki units. With
`FBRFIX_ALL_RANKS=1` every rank fires in turn. That makes a longer rotation, but no group waits
for slow reloaders. In the test replays it gave 4 to 6 volleys where 3 groups gave 3 to 6, with
the same shot counts within noise. It is off by default and not decided yet.

## Dev build

Built into the dev `dinput8.dll` with CMake option `FIX_FIRE_BY_RANK` (on by default), next to
the path fix and the artillery targeting fix. The fix is on at launch. **Y** switches it off and
on again at any time, only while the game window has the focus: a system "asterisk" sound for
on, the default beep for off.

| Environment | Effect |
|---|---|
| `FBRFIX_OFF=1` | start with the fix off (hooks installed, the game's own answers), for A/B runs |
| `FBRFIX_GRACE=<ticks>` | grace time of the group wait in game ticks of 0.1 s (default 15) |
| `FBRFIX_ALL_RANKS=1` | every rank fires in turn (see above) |
| `FBRFIX_PROBE=1` | research capture into `<game>\fbrfix_runs\<stamp>\` (four more hooks; see `src/fbrfix.cpp`) |

Log lines (in `<game>\pathfix_log.txt`): `fbrfix: fix ON, grace 15 ticks`, the installed hooks,
and at exit the counters `group tests: ready N, grace N, reloading N, waiting N; men left out N;
ready orders skipped N, given N; bad task N`. If the first 20 group tasks do not have the decoded
layout, the fix switches itself off for the session (`fbrfix: DISABLED ...`, the system error
sound). Y is refused from then on.

Single player and replays only: a changed simulation desyncs multiplayer.

## Tests

`python fixes/fire-by-rank-fix/tests/run_tests.py` (also run by `test.bat`) maps the real
`empire.retail.dll`, loads the proxy and enters the game's own group test `FUN_1017b980` and
group order `FUN_10188290` on fake group tasks. It runs five modes:

| Mode | Checks |
|---|---|
| `fix` | the officer is not waited for and gets no order, a reloading man is waited for, a man slow to his ready pose only until the grace time, other groups do not count, Y switches it, 3 groups kept (20) |
| `off` | the game's own answers until Y (17) |
| `selfcheck` | broken group tasks switch the fix off (5) |
| `allranks` | the option raises a 5-rank unit to 5 groups before the first volley (4) |
| `probe` | the probe's hooks and files (26) |

## Files

| Path | Content |
|---|---|
| `src/fbrfix.cpp` | the fix and the research probe |
| `src/stack_offsets_fbr.h` | hook context offsets, generated by `tools/re/stack_calc.py` |
| `tests/fbrfix_test.cpp`, `tests/run_tests.py` | offline harness and runner |
| `tests/compare_runs.py` | compares probe runs (delay, volleys, waits, hangs, shots) |
| `fire-by-rank-fix.cmake` | build wiring |
| `docs/runbook.md` | A/B runs with the autorun tool |
| `analysis/`, `docs/research/`, `data/` | local only: extraction and analysis scripts, Ghidra output, research log, db tables |
