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

- **A man who has fired waits for his whole group before he starts reloading.** His reload time
  is set at the shot but only runs once he has his reload order, and the game gives a group its
  reload orders when every man of the group has finished his fire order, including the men who
  never get a shot. Measured: 2 to 9 s with an empty gun after a volley.

- **Three groups at most, and one reloader holds all.** In a formation deeper than 3 ranks the
  rear ranks never fire, and a single man still reloading keeps his whole group from firing.

**Fix.** Four entry hooks on the group fire task.

- No ready or reload order for a man outside his unit's shooter class (the sword officer is the
  only such man seen), and the group does not wait for him.
- The group fires once every shooter of it is loaded and either ready or 1.5 game seconds into
  the wait, instead of waiting for the last man's ready pose.
- **Early reload** (part 2): a man who has fired gets his kneeling reload order (the game's own)
  as soon as his fire order is done, instead of when his whole group is done.
- **Every rank fires** (part 3): the group count is raised from the game's cap of 3 to the ranks
  of the formation (at most 8) before the first volley.
- **75 % quorum** (part 3): once 75 % of a group's shooters are loaded and ready (a loaded man
  counts as ready after the 1.5 s grace time), the group fires 1 s later. The men still
  reloading get no fire order: they go on reloading and sit that volley out, and the end of the
  group's turn does not wait for them.

State: dev build only (2026-10-18), not in a public release yet.

## Part 2: does rank 1 reload while the other ranks fire?

Yes, but it is not ready when the rotation comes back to it. With 3 groups of about 6 s each,
group 0 gets about 12 s of reload before its next turn, and a matchlock reload is 19.8 s
(Hattori) to 20.9 s (Ikko Ikki). Early reload removes the wait before the reload starts; more
groups (every rank firing, then an option, now the default) make the rotation long enough. Runs with the probe, two
fire-by-rank units per replay, all with the part-1 fix ("A" = part 1 only, "E" = early reload,
"F" = fire grace 20 ticks, "R" = every rank fires):

| Replay | Variant | FBR shots | Idle after a shot, 90 % | Shot to shot per man (median) | Reload left at the wrap |
|---|---|---|---|---|---|
| Hattori (4 ranks) | A | 198 | 9.1, 7.0 s | 32.3, 29.1 s | 7.3, 7.0 s |
| | E | 183 | 2.5, 2.5 s | 24.5, 23.4 s | 19.8, 5.7 s |
| | E + F | 185 | 2.5, 2.5 s | 28.3, 27.9 s | 19.8, 19.3 s |
| | E + R | 251 | 2.7, 2.5 s | 22.0, 35.7 s | 2.9, 0.0 s |
| | E + F + R | 260 | 2.5, 2.5 s | 28.9, 25.3 s | 0.0, 19.8 s |
| Ikko Ikki (5 ranks) | A | 205 | | | 12.3, 14.7 s |
| | E | 208 | 2.5, 2.5 s | | 12.5, 13.8 s |
| | E + R | 206 | 2.5, 2.5 s | | 11.5 s |
| | E + F + R | 232 | 2.5, 2.5 s | | 12.1 s |
| Generic (3 ranks) | A | 239 | | | 11.8 s |
| | E | 244 | 2.5, 2.5 s | | 6.9, 9.2 s |

Fire-at-will men in the same runs: idle 2.2 s, shot to shot 24.8 to 26.2 s. The battles are
short (one to three rotations), and a replay drifts from its recording once the simulation
changes: the fire-at-will totals in the Hattori runs range from 114 to 153 shots, so shot totals
differ by up to about 20 % from noise alone. Early reload is the per-man effect the user asked
about: the idle with an empty gun falls to the fire-at-will value and a man fires about as often
as a fire-at-will man. Every rank firing adds about 30 % shots in the 4-rank Hattori units; in
the 5-rank Ikko Ikki units ranks 3 to 5 often had no line of fire (their fire orders ended
without a shot). The fire grace (`FBRFIX_FIRE_GRACE`) showed no gain and stays off.

Two waits remained, both the game's own: one man still reloading holds his whole group, and when
the task restarts (the unit re-forms, seen late in the Hattori runs) it always begins with group
0, even when group 0 has just fired and the others are loaded. Part 3 answers the first.

## Part 3: every rank fires, 75 % quorum (the defaults)

User decision: every rank fires; a group fires 1 s after 75 % of its men are ready. Runs with the
new defaults against part 2 with every rank firing (E + R above):

| Replay | Variant | FBR shots | Volleys | Hang at the end | Quorum volleys (men sitting out) |
|---|---|---|---|---|---|
| Hattori | E + R | 251 | 4, 6 | 17.8 s, none | - |
| | part 3 | 240 | 5, 7 | none, none | 1 (3 men), 0 |
| Ikko Ikki | E + R | 206 | 5, 4 | 9.3 s, none | - |
| | part 3 | 206 | 5, 4 | 9.3 s, none | 0 |
| Generic | E + R | 244 | 3, 3 | 0.4, 5.4 s | - |
| | part 3 | 244 | 3, 3 | 0.4, 5.4 s | 0 |

In the Hattori run the restarted task found 3 of 39 men still reloading: it fired 2.5 s into the
wait with the other 36 instead of waiting 17.8 s, and the unit got one more volley. In the Ikko
Ikki and generic replays no group ever had a few stragglers: the waits that remain are whole
groups reloading together (the rotation is shorter than a reload), so the runs are identical to
E + R. Shot totals are within the replay drift.

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
the fix every rank fires in turn. That makes a longer rotation, so a group's men are reloaded
when its turn comes back. Without early reload it gave 4 to 6 volleys where 3 groups gave 3 to 6,
with the same shot counts within noise; with early reload it added about 30 % shots in the
Hattori replay (part 2 above). On by default since part 3 (user decision);
`FBRFIX_ALL_RANKS=0` keeps the game's 3 groups.

## Dev build

Built into the dev `dinput8.dll` with CMake option `FIX_FIRE_BY_RANK` (on by default), next to
the path fix and the artillery targeting fix. The fix is on at launch. **Y** switches it off and
on again at any time, only while the game window has the focus: a system "asterisk" sound for
on, the default beep for off.

| Environment | Effect |
|---|---|
| `FBRFIX_OFF=1` | start with the fix off (hooks installed, the game's own answers), for A/B runs |
| `FBRFIX_GRACE=<ticks>` | grace time of the group wait in game ticks of 0.1 s (default 15) |
| `FBRFIX_ALL_RANKS=0` | the game's 3 groups (default: every rank fires) |
| `FBRFIX_QUORUM=<percent>` | share of a group's shooters loaded and ready before it may fire without the rest (default 75; 0 = off: one reloader holds his group) |
| `FBRFIX_QUORUM_WAIT=<ticks>` | wait after the quorum is reached (default 10 = 1 s) |
| `FBRFIX_EARLY_RELOAD=0` | no early reload orders (part 1 behaviour) |
| `FBRFIX_FIRE_GRACE=<ticks>` | also end a group's turn that many game ticks after its last shot (default 0 = off) |
| `FBRFIX_PROBE=1` | research capture into `<game>\fbrfix_runs\<stamp>\` (four more hooks; see `src/fbrfix.cpp`) |

Log lines (in `<game>\pathfix_log.txt`): `fbrfix: fix ON, grace 15 ticks`, the installed hooks,
and at exit the counters `group tests: ready N, grace N, reloading N, waiting N; men left out N;
ready orders skipped N, given N; bad task N` and `fire-completed tests: done N, firing N, cut by
the fire grace N (N men not fired); early reload orders N, second reload orders skipped N` and
`quorum: fired N, waited N ticks; fire orders not given to men reloading N`. If the first 20
group tasks do not have the decoded layout, the fix switches itself off for the session
(`fbrfix: DISABLED ...`, the system error sound). Y is refused from then on.

Single player and replays only: a changed simulation desyncs multiplayer.

## Tests

`python fixes/fire-by-rank-fix/tests/run_tests.py` (also run by `test.bat`) maps the real
`empire.retail.dll`, loads the proxy and enters the game's own group test `FUN_1017b980` and
group order `FUN_10188290`, the fire-completed test `FUN_1017b9d0` and the fire order
`FUN_101883f0` on fake group tasks. It runs six modes:

| Mode | Checks |
|---|---|
| `fix` | the officer is not waited for and gets no order, a reloading man is waited for below the quorum, a man slow to his ready pose only until the grace time, other groups do not count, Y switches it, early reload orders once per shot and not again at the exit, 75 % quorum fires 1 s later, no fire order for men reloading, they do not hold the turn, a 5-rank unit gets 5 groups (35) |
| `off` | the game's own answers until Y (26) |
| `selfcheck` | broken group tasks switch the fix off (5) |
| `cap3` | `FBRFIX_ALL_RANKS=0 FBRFIX_QUORUM=0`: 3 groups kept, one reloader holds his group (5) |
| `firegrace` | early reload off, the turn ends 20 ticks after the group's last shot (5) |
| `probe` | the probe's hooks and files (28) |

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
