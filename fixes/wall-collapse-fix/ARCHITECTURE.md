# wall-collapse-fix: design

## The user's report

Defending a siege, you can put units on the walls. Walls can be damaged before the battle and
deployment shows which. Normally a unit deployed on a damaged section is fine, but sometimes the
section "glitches" and is destroyed again when Start Battle is clicked, killing the men deployed
there. Replay "broken wall bug" (2026-10-19 15:27) shows a unit losing its men this way; the user
reports 20 men. The bug is old (vanilla). Goal: any wall position can be used in deployment
without that risk, while walls destroyed by artillery during the battle still collapse and kill.

## Getting the replay in sync

The replay was recorded with the dev build of 2026-10-19 14:28 (all six other fixes, defaults:
pathfix off). Played with that build plus this fix's probe, it stays in sync. With the fix on it
desyncs at tick 453, where the recording's men die: expected, since the fix keeps them alive.

## Finding the cause

1. The scripted building commands (`BCQ_BUILDING_PIECE_*_DESTRUCT_LEVEL`) are registered with a
   creator each. Those lead to the piece's destruct level (+0x68) and the level apply function
   `FUN_10477f40`. From there the piece's damage (+0xEC), damage thresholds per level, the
   destroyed flag (+0xF1), the damage-added function `FUN_1047b370` and the collapse effects
   `FUN_10479ca0` follow.
2. The campaign's fort damage is applied in the battle set-up (`FUN_10174140`
   "finalise_fort_info", `FUN_10426940`), before deployment: what deployment shows is final.
3. Probe run on the replay: at the change to phase 5 (tick 451), the next tick shows five damage
   calls from one caller chain (phase 5 handler, `FUN_101787b0`, `FUN_1043c870`, `FUN_1043c8b0`):
   two pieces that were intact during deployment collapse, three already destroyed ones ignore it.
4. The two pieces are gate doors: two-piece buildings whose piece 1 carries the gate flag
   (+0x6C) and the gate variant. In one building the gate structure (piece 0) was already destroyed
   by the campaign damage. That is the "damaged wall" seen in deployment. Its door was intact.
5. `FUN_101787b0` is the battle start. The phase 5 handler runs it once, on the first fighting
   tick. For every building it calls `FUN_1043c870`, which gives every gate piece one more damage
   than its current threshold when the flag at `[tick object +0x5C] +0x9C` is set. The game reads
   the same flag to build gates open and to let every alliance through them: the castle's gates
   are open to everyone (presumably sabotaged in the campaign). So the start of the battle turns
   "open" into "destroyed".
6. Men: soldier hit points never change at the transition. The second probe run follows every
   soldier within 30 m of a collapsing piece. 97 men of one unit stood near the door. 10 of them,
   in a row at height 8 m right over the door, left their group's men list one tick after the
   collapse. The unit went from 200 to 190 men and nothing else changed.

## The fix

An entry hook on `FUN_1043c870` (10 stolen bytes, `docs/re/prologues_collapsefix_fix.txt`)
returns at once while the fix is on. No gate is damaged at the battle start.

- The gates stay open and passable for every alliance for the whole battle. The gate tick
  (`FUN_10487410`) keeps a flagged gate open and the passability test (`FUN_104362e0`) lets everyone
  through, both from the same flag. So the attacker's gain from the open gates is unchanged.
  Only the rubble and the deaths go.
- Every other way to damage a piece is untouched: `FUN_1047b370` is still called by projectiles,
  rams and fire through its other callers, with the game's own code. A door destroyed that way in
  battle still collapses and still kills.
- Before returning, the hook counts the intact gate doors the game would have destroyed and logs
  each building ("kept" with the fix on, "destroyed by the game" with it off).

Alternatives not taken: destroying the doors at the start of deployment instead (would show the
real state, but rebuilds navigation data while units are being placed); suppressing only the
kill (the kill routine was not traced, and the gate would still turn to rubble under the men).

## Research probe (dev build only, COLLAPSEFIX_PROBE=1)

Wraps the level apply, damage set, damage added and fort damage functions, logs the collapse
effects, phase changes and soldier hit point sets, snapshots every piece at each phase change and
on every change (50 ms sampler), counts every unit's men, and follows the soldiers near a piece
that collapses in battle. Files in `<game>\collapsefix_runs\<stamp>\`; header of
`src/collapsefix.cpp` lists them.
