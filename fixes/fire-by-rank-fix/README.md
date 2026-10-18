# fire-by-rank-fix: matchlock fire by rank fires sooner, every rank fires, Hattori / Ikko Ikki keep firing

Code name `fbrfix` (identifiers, environment variables, log prefix).

**Problem.** Matchlock units with the Fire by Rank ability take a long time to open fire, much
longer than with fire at will, and the ranks do not keep up a steady fire. The Hattori and Ikko
Ikki gun units are worse: they fire one volley per rank for three ranks and then never fire or
reload again.

**Cause** (decode in [docs/re/missile_fire_x86.md](../../docs/re/missile_fire_x86.md)). Fire by
rank is the game's group fire task. It puts the men in groups by formation rank (at most 3),
waits until every man of the current group is "loaded and ready", orders the group to fire,
waits until every man has finished his fire order, gives the group its kneeling reload orders,
and moves on to the next group.

- **The sword officer.** The Hattori and Ikko Ikki gun units have the officer `officer_katana`
  (katana animations, no gun) instead of `officer_teppo`. He stands in the front group and can
  never finish its kneeling reload order, so when the rotation comes back to the front group the
  wait for him never ends and the whole unit stops.
- **The wait counts every man.** Before the first volley the group waits about 10 s for its
  slowest man's ready pose, although everyone is loaded; one man still reloading holds his group
  in every later wait.
- **The reload starts late.** A man's reload time is set at his shot but only runs once he has
  his reload order, which the group gets when every man has finished his fire order, including
  men who never get a shot: 2 to 9 s with an empty gun after each volley.
- **Three groups, strict order.** In a formation deeper than 3 ranks the rear ranks never fire,
  and a group that has reloaded waits while the group whose turn it is still reloads (for
  example after a re-form, when the new front ranks hold men who have just fired).

**Fix.** Four entry hooks on the group fire task, all on together:

- **Every rank fires**: the group count is raised to the ranks of the formation (at most 64).
- **Front rank first**: the front-most rank with 75 % of its men loaded takes the turn,
  whichever rank's turn it was; it first gets the ready order the game gives a rank at the start
  of its own turn. A rank whose last turn had shots from fewer than a quarter of its men (no line
  of fire, target out of reach) only fires on its own turn.
- **Fire without the stragglers**: a rank fires as soon as all its shooters are loaded and
  ready, after 1.5 s when they are loaded but some are still in their ready pose, or 1 s after
  75 % of them are loaded and ready. Men still reloading get no fire order; they keep
  reloading and fire with their rank next time.
- **Early reload**: a man who has fired gets his kneeling reload order (the game's own) as soon
  as his fire order is done. A turn also ends 5 s after the rank's last shot, so a man who aims
  but never gets a shot does not hold the unit.
- **The sword officer** gets no ready or reload order and is never waited for. The rule needs no
  constant: the shooter class is the most common soldier class in the unit, so in a working unit
  nothing changes.

The ranks come from the game's own formation every time the unit forms up or re-forms; each man
keeps his own reload time. State: dev build only (2026-10-18), not in a public release yet.

## Measured (replays `firebyrank_generic`, `_ikkoikki`, `_hattori`, `_reform_deep`)

Probe runs with the fix off (`FBRFIX_OFF=1`) and with the fix on (the defaults above), through
`tests/compare_runs.py`. "Delay" runs from the first enemy soldier within 100 m (the matchlock's
effective range) to the unit's first shot; "hang" is a wait still open when the battle ended.
The first three replays have two fire-by-rank units each, `_reform_deep` one, which re-forms
into a formation 14 ranks deep while firing.

| Replay | Shots, off / on | Delay to first shot, off / on | Volleys, off / on | Hang, off / on |
|---|---|---|---|---|
| Hattori (4 ranks, sword officer) | 153 / 194 | 16.4, 13.3 / 8.1, 4.3 s | 3, 3 / 7, 6 | 20.8, 20.0 s / none |
| Ikko Ikki (5 ranks, sword officer) | 161 / 206 | 14.7, 9.5 / 5.5, 2.1 s | 3, 3 / 5, 4 | 8.3 s, none / 9.3 s, none (a real reload) |
| Generic (3 ranks) | 230 / 244 | 16.0, 12.3 / 7.2, 4.8 s | 3, 3 / 3, 3 | 2.9 s, none / 0.4, 5.4 s |
| Re-form, 14 ranks deep | 104 / 129 | 11.4 / 4.4 s | 6 / 9 | 1.2 s / none |

Per man, with the fix the time from a shot to the start of his reload falls to 2.5 s (90th
percentile; fire at will 2.2 s; up to 9 s before early reload). Fire-at-will units in the same
replays open fire 1.9 to 5.8 s after contact and are not changed.

A replay with a changed simulation no longer follows its recording: shot totals between runs
with the same fix differ by up to about 20 % (the fire-at-will totals in the Hattori runs ranged
from 114 to 156). Single variants measured against each other (runs in the research log):
front rank first against the game's order gave 129 / 121 shots in the re-form replay, 194 / 240
in the Hattori replay and identical runs in the other two; every rank against 3 ranks added
about 30 % shots in the Hattori replay.

## Dev build

Built into the dev `dinput8.dll` with CMake option `FIX_FIRE_BY_RANK` (on by default), next to
the path fix and the artillery targeting fix. The fix is on at launch. **Y** switches it off and
on again at any time, only while the game window has the focus: a system "asterisk" sound for
on, the default beep for off.

| Environment | Effect (all for A/B runs; the defaults are the fix) |
|---|---|
| `FBRFIX_OFF=1` | start with the fix off (hooks installed, the game's own answers) |
| `FBRFIX_ALL_RANKS=0` | the game's 3 groups |
| `FBRFIX_FRONT_FIRST=0` | the game's order of the ranks |
| `FBRFIX_QUORUM=<percent>` | share of a rank's shooters loaded for it to fire without the rest and to take the turn (default 75; 0 = wait for every man) |
| `FBRFIX_QUORUM_WAIT=<ticks>` | wait after the quorum is reached (default 10 = 1 s) |
| `FBRFIX_GRACE=<ticks>` | wait for the ready pose of loaded men (default 15 = 1.5 s) |
| `FBRFIX_EARLY_RELOAD=0` | reload orders only when the whole rank is done, as in the game |
| `FBRFIX_FIRE_GRACE=<ticks>` | end a turn that long after the rank's last shot (default 50 = 5 s; 0 = off) |
| `FBRFIX_PROBE=1` | research capture into `<game>\fbrfix_runs\<stamp>\` (four more hooks; see `src/fbrfix.cpp`) |

Log lines (in `<game>\pathfix_log.txt`): at launch the installed hooks and one line per rule;
at exit `group tests: ready N, grace N, reloading N, waiting N; ...`, `fire-completed tests:
done N, firing N, cut by the fire grace N ...; early reload orders N, ...` and `quorum: fired N,
waited N ticks; fire orders not given to men reloading N; turns moved to a front-most loaded
group N`. If the first 20 group tasks do not have the decoded layout, the fix switches itself
off for the session (`fbrfix: DISABLED ...`, the system error sound). Y is refused from then on.

Single player and replays only: a changed simulation desyncs multiplayer.

## Tests

`python fixes/fire-by-rank-fix/tests/run_tests.py` (also run by `test.bat`) maps the real
`empire.retail.dll`, loads the proxy and enters the game's own group test `FUN_1017b980`, group
order `FUN_10188290`, fire-completed test `FUN_1017b9d0` and fire order `FUN_101883f0` on fake
group tasks. It runs six modes:

| Mode | Checks |
|---|---|
| `fix` | the officer is not waited for and gets no order; reloading men are waited for below the quorum; the grace time; Y; early reload orders once per shot and not again at the exit; the 75 % quorum fires 1 s later without the men reloading, who do not hold the turn; every rank; front rank first with its ready orders, a rank without shots keeps out of turn (42) |
| `off` | the game's own answers until Y (26) |
| `selfcheck` | broken group tasks switch the fix off (5) |
| `cap3` | `FBRFIX_ALL_RANKS=0 FBRFIX_QUORUM=0 FBRFIX_FRONT_FIRST=0`: the game's 3 groups, one reloader holds his rank, the order is kept (6) |
| `firegrace` | early reload off, the turn ends 20 ticks after the rank's last shot (5) |
| `probe` | the probe's hooks and files (28) |

## Files

| Path | Content |
|---|---|
| `src/fbrfix.cpp` | the fix and the research probe |
| `src/stack_offsets_fbr.h` | hook context offsets, generated by `tools/re/stack_calc.py` |
| `tests/fbrfix_test.cpp`, `tests/run_tests.py` | offline harness and runner |
| `tests/compare_runs.py` | compares probe runs (delay, volleys, waits, hangs, shots, reload timing, rank order) |
| `fire-by-rank-fix.cmake` | build wiring |
| `docs/runbook.md` | A/B runs with the autorun tool |
| `analysis/`, `docs/research/`, `data/` | local only: extraction and analysis scripts, Ghidra output, research log, db tables |
