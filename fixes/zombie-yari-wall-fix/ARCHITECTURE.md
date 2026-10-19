# zombie-yari-wall-fix: how it works and how it was found

## The user's report

When yari ashigaru in yari wall break and shatter, a unit sometimes does not run. It stands on
the spot, out of the fight as far as morale goes, and its yari wall keeps killing whatever
touches it. This can decide a battle, worst in multiplayer. Replay `zombie_yariwall`: an army of
yari ashigaru in yari wall sent into the enemy until every unit broke; two units stayed behind
and kept fighting. The user's test for a zombie: its kill count still rises after it broke.

## Getting the replay in sync

The replay was recorded with the dev `dinput8.dll` of 2026-10-18 23:08 loaded (artfix, fbrfix,
siegefix and wallfix on, pathfix off; the game folder log of the recording shows it). A replay
with only this fix's probe compiled in desynced at 65.6 s (siegefix changes route timing). The
current dev build, with those fixes at their defaults plus this probe, stays in sync: three runs
gave identical per-unit numbers, and `replay_desync.txt` was not rewritten. Every run so far ends
when the battle is decided (about 120 s); the replay does not continue after that.

## Finding the cause

1. **Morale fields** (static, from the script conditions "wavering", "routing", "rallying"): the
   battle unit's morale block at +0xCC4 with the level at +0xCE8 and the rout state at +0xCEC
   (`docs/re/rout_x86.md`). The probe logs every change: level 6 with rout state 2 = broken,
   level 7 = shattered.
2. **Kill counter**: the probe's unit dumps show the field at unit +0xD88 rising with every
   killer unit's kills. Per routed unit, its rise after the rout tick is the user's zombie test.
3. **State machines**: the probe logs every transition of every machine (the shared runner
   `FUN_11374cf0`) with its owner. A normal rout: the unit's interrupt machine (unit +0xE78)
   goes MELEE_INTERRUPT -> ROUT_INTERRUPT; the next tick the group's behaviour stack is cleared
   and the rout task (vtable 0x115E01F4) pushed; the men run.
4. **The zombies** broke while their charge order was still running. In the same tick as the
   rout transition a deferred group command of the charge (`FUN_103d0470`, the follow-up attack)
   builds a melee attack task (0x115C0DFC) on the group, before the rout tick has set the group
   up. The probe never sees the rout task on these groups' stacks (with the fix on, the first
   rout tick after the set-up finds their stack empty); three ticks later the group's default
   re-form task (0x115C0644, built by `FUN_101b6490`) runs: REFORM, HALT, ADOPT_STANCES (the yari
   wall stance), IDLE, READY. No rout task is on the stack, and the group's byte +0x2781, which
   the rout tick sets when it sets a group up, stays 1, so the rout tick never sets it up again.
   The task log of the fix-off run has the attack task at the rout tick for exactly the four
   units whose kill count rose most after breaking.
5. **The exact sequence** (probe hooks on the rout set-up `FUN_101a9bf0`, the stack pop
   `FUN_1019e480`, the group stop `FUN_101604a0`, the stop of every group `FUN_101605f0` and the
   attack command post `FUN_103daa80`; run 20261019_114406, in sync):
   - break tick: `FUN_10154180` stops every group of the unit (all routing units get this);
   - same tick, zombies only: the unit's attack order (unit-level task, vtable 0x115BF200, update
     `FUN_101b9290`) starts a fresh instance and posts a group attack command; the command builds
     a melee attack task;
   - next tick: the rout set-up clears that task and pushes the rout task (correctly);
   - right after, zombies only: `FUN_10154180` stops every group of the unit again, which deletes
     the rout task; two ticks later the group update (`FUN_101b6490`) finds the stack empty and
     pushes the default re-form task.
6. **Why yari wall**: breaking switches yari wall off, so the unit's formation changes from the
   yari wall formation back to the block. The formation change handler `FUN_1014e870` picks the
   formation from the active abilities (`FUN_101672b0`), installs it and re-forms the unit
   (`FUN_10154180`: every group stopped, a new formation task). A probe hook on the re-form
   (run 20261019_122816, in sync) shows it at the break for all 12 yari units; in the user's
   second replay `bow_route` (three bow units charge, melee and rout; recorded with the
   zombie-only build, fix off; run 20261019_123056, in sync) it never happens: no formation to
   drop, no re-form, the rout set-up simply replaces the melee attack task with the rout task
   (0 groups without a rout task, 0 kills after breaking).
   Which yari units become zombies: those whose attack order is still the unit's current task at
   the break. The re-form restarts that order (`FUN_101b9290` posts a new attack command and
   re-forms through `FUN_101543b0`), and the next tick the order re-forms the unit again, right
   after the rout set-up gave the group its rout task: the stop of every group deletes it. The
   other 8 yari units re-formed through `FUN_10154310` (another current task) and ran.
   Why a stuck yari unit is deadly: standing in its yari wall stance it kills by contact (pike
   outcome `FUN_10435410`, collision knockdown `FUN_10430cf0`) without an order, and a broken unit
   (level 6) keeps its melee values. Other special formations that are dropped at a break (yari
   square, wedge, square and the others picked by `FUN_101672b0`) go through the same re-form;
   untested. The fix acts on the result (a routing group without its rout task), so it covers
   them as well.

## The fix

One entry hook, `FUN_101bd3b0`, the rout interrupt state's tick (stolen
`56 57 8B F9 8B 47 10`, ECX = the interrupt context, its unit at +0x10). Before the game's own
loop it looks at every live group of the unit (list at unit +0xB90): if the group's byte +0x2781
says "set up" but no rout task (0x115E01F4, or 0x115EDFA8 from the other constructor) is on its
behaviour stack, it clears the byte. The game's loop right after it then calls its own group set
up (`FUN_101a9bf0`): the move cancelled, the stack cleared, the rout task pushed, the men run.

Only units in the rout state are touched, and only groups that lost their rout task. A unit that
rallies leaves the rout state first (the state's exit resets the byte anyway). Groups without men
are left alone.

## Research probe (dev build only, `ZOMBIEFIX_PROBE=1`)

Probe-only hooks: the state transition site `FUN_11374cf0+0x1B` (shared with fbrfix's probe; the
two are never on together, the second finds a JMP and refuses), the pike and knockdown outcome
functions (wrapped through their trampolines), and the three squad task constructors (re-form,
direct intercept, melee attack). A thread samples every unit met every 50 ms: morale, group
lists and stacks, positions, men, raw dumps. Files: `docs/runbook.md`.
