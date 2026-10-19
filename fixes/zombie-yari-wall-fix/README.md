# zombie-yari-wall-fix

**Problem.** When yari ashigaru in yari wall break, a unit sometimes does not run. It stands on
the spot, counted as broken by the morale system, its men in their yari wall stance, and keeps
killing whatever runs into it. Its kill count keeps rising after it broke. This can decide a
battle, worst in multiplayer.

**Cause.** When a unit breaks, the game sets up each of its soldier groups for the rout once: it
clears the group's task list, pushes the rout task (the men run) and marks the group as set up.
A unit that breaks in the middle of a charge still has a follow-up attack of that charge
pending. It runs in the same tick and starts a melee attack task on the group before the rout
set-up. Once that is over, the rout task is not on the group's task list; the group falls back to
its default task, re-forms, adopts its stances (the yari wall) and waits. Because the group is
marked as set up, the game never pushes the rout task again. A standing yari wall kills by
contact without any order, so the unit keeps killing. Details: `ARCHITECTURE.md`, engine
reference `docs/re/rout_x86.md`.

**Fix.** While a unit is routing, the game's rout state checks its groups every tick. The fix
looks at the same groups first: a group that is marked as set up but has no rout task gets its
mark cleared, so the game sets it up again at once with its own rout task. Nothing else changes.
Units that rout normally are not touched, and a unit that rallies leaves the rout state first.

**State.** Dev build (2026-10-19), compiled into the same `dinput8.dll` as the other fixes. No
public release yet. Single player and replays only.

## Results

The user's replay `zombie_yariwall`: an army of 150-man yari ashigaru in yari wall sent into the
enemy until every unit broke. Recorded with the dev build of 2026-10-18 (artfix, fbrfix, siegefix,
wallfix on, pathfix off), which these runs keep. Speed 1, 12 units broke in each run. Kills come
from the unit's own kill count, every 0.5 s.

| | Fix off | Fix on |
|---|---|---|
| Units whose kill count rose after they broke and that did not run | 4 | 0 |
| Their kills after breaking | 9, 19, 34, 40 | none |
| Their men left at the end | 12, 0, 16, 14 | (the matching units: 56, 36, 89, 87) |
| Distance moved in the 20 s after breaking | 6 to 24 m | 21 to 66 m |
| Kills more than 5 s after breaking, all routed units | 94 | 0 |
| Groups the fix set up to rout again | | 4, once each |

The units with 34 and 40 kills are the two the user saw. Two to three units in each run gain one
or two kills in the first seconds after breaking while they turn and run (a blow already under
way, a knockdown); that happens with the fix off as well and is not the fault.

The fix-off run stays in sync with the recording (identical in three runs). With the fix on the
replay drifts from the first re-armed unit on (65.5 s), so the units break at slightly different
times; the four re-armed units are the ones that broke in the same moments as the four zombies.
The battle ends when it is decided (about 120 s).

## Interaction with the other fixes

- Shares no patch site with any fix (`tools/re/check_prologues.py`). Its probe's state transition
  hook is the same site as fire-by-rank-fix's probe; the two probes are never on together (the
  second one finds a jump there and refuses).
- All runs above have artfix, fbrfix, siegefix and wallfix on.

## Use (dev build)

| Variable | Effect |
|---|---|
| `ZOMBIEFIX_OFF=1` | start with the fix off; I toggles it (with the game focused) |
| `ZOMBIEFIX_PROBE=1` | research probe: `<game>\zombiefix_runs\<stamp>\` |

Live check build (only this fix, off at launch, I on with the high tone, I again off with the low
tone): CMake `-DZOMBIEFIX_START_OFF=ON` with the other `FIX_*` options OFF; in such a build
`ZOMBIEFIX_ON=1` starts it on instead.

Build, tests and replay runs: `docs/runbook.md`.
