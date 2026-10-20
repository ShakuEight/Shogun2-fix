# guard-mode-ranged-fix: how it works and how it was found

## The user's report

Guard mode keeps a unit in place: melee units in guard mode do not chase. Missile units in guard
mode still chase a routing unit to shoot at it. Wanted (user decisions, 2026-10-20): a missile
unit in guard mode never walks after any target that has left its range; it never shoots at a
shattered unit and switches to another enemy in range, or holds fire when there is none. Units
not in guard mode keep the game's behaviour. In the replay the chase followed explicit attack
orders.

Replay `guard mode` (2026-10-20 08:16): two groups of one yari, one matchlock and two bow units,
one group in guard mode; both engage and the enemy routs.

## Getting the replay in sync

The replay was recorded with the dev `dinput8.dll` of 2026-10-19 (all eight fixes, pathfix off;
the game folder log of the recording, kept as `data/recording_pathfix_log_20261020_0806.txt`).
The current dev build with this fix's probe and `GUARDFIX_OFF=1` stays in sync. The battle is
decided 9 s after the first rout, which ends an autorun run by default; the chase happens after
that. With `--linger 90` the replay plays on for another 31 s of game time (to tick 1262).

## Finding the cause

1. **The guard flag** (static). The battle command for guard mode has one executor; per unit it
   calls `FUN_10186130`, which stores the value at battle unit **+0xEB1**. Only four places of
   the battle code read that byte. One is the melee attack task's "keep pursuing" test
   (`FUN_101af030`, used by its PURSUE_STATE): that is the melee no-chase. Nothing on the missile
   side reads it.
2. **The chase** (static, then the probe). A missile unit's attack order builds the missile
   attack squad task (vtable 0x115C0F24). Its FIRE, FIRE_UNFORMED and PERFORM_CAN_SHOOT_TEST
   updates ask `FUN_101aefc0` whether the last can-shoot test failed while the order allows
   intercepting (task byte +0x2D clear). If so they go to INTERCEPT_STATE, which pushes a direct
   intercept task towards the target. In the fix-off run the three guard-mode missile units do
   exactly that at ticks 985..989, from FIRE, against their shattered target 139..166 m away, and
   walk 32..47 m.
3. **Shattered targets.** The same updates first ask `FUN_101b2460` whether the target is still
   valid. A broken or shattered unit stays valid, so the units shot at it while in range.
4. **Fire at will.** With only the missile task changed (first fix-on run), the attack order
   ended, but fire at will at once started a new unit attack task against the next shattered
   enemy, every 0.2 s (442 attack commands in the run). Probe hooks on the attack command post
   and the unit attack task constructor led to the unit interrupt machine's
   FIRE_AT_WILL_INTERRUPT_STATE. Its target comes from `FUN_101752d0`, which scores the enemies
   in missile range by distance with only a penalty for routing ones. An entry hook on the
   awareness classification (`FUN_10169390`) did not help: fire at will does not take its target
   from there. That hook was dropped.

## The fix

Seven calls in the missile attack task's state updates are redirected to the proxy. The whole
CALL instruction is byte-checked and only its displacement is written. The proxy calls the
game's own test first and changes the answer only for a unit in guard mode. One entry hook
filters fire at will's list.

| Site | Game test | Change for a missile unit in guard mode |
|---|---|---|
| FIRE, FIRE_UNFORMED, PERFORM_CAN_SHOOT_TEST updates | `FUN_101aefc0`: out of range, intercept? | no, once the unit had this target in range |
| FIRE, FIRE_UNFORMED, PERFORM_CAN_SHOOT_TEST updates | `FUN_101b2460`: target invalid? | yes, when the target is shattered |
| INTERCEPT update | `FUN_101b2460` | yes, when the target is shattered, or was in range (stops a chase under way when guard mode is switched on) |
| `FUN_101752d0` entry | fire at will's choice | shattered enemies taken out of the list |

- "In range" means the unit's missile task was in FIRE or FIRE_UNFORMED against that target.
  The pair (unit, target) is remembered for the battle, so an attack order on a target that was
  never in range still walks into range.
- When "intercept" is answered no, the update falls through to its other tests and ends in
  CANNOT_SHOOT_STATE: the task ends as it does for an order that may not move. The order queue
  is then empty and fire at will takes over.
- A missile unit is one whose missile attack task has passed a redirected call in this battle.
  Fire at will's list of a guard unit not yet known as a missile unit is left alone.
- The INTERCEPT update is shared with the naval missile task (vtable 0x115C1418). The wrappers
  act only on the land missile task's vtable.
- The fix's tables (in-range pairs, missile units) are cleared when the battle tick goes back.
- Self-check: a guard byte that is neither 0 nor 1 is counted; after 10 the fix switches itself
  off for the session, with the three-tone cue. I is then refused.

## Probe files (`GUARDFIX_PROBE=1`)

| File | Rows |
|---|---|
| `guard_states.csv` | transitions of unit tasks and of the missile attack, intercept, group / row fire and melee attack tasks; guard byte, the missile task's target with its morale and distance |
| `guard_calls.csv` | the redirected calls: each change of answer per task and site, and each answer the fix changed |
| `guard_pos.csv` | every unit's main group twice per game second: men, centre, guard, morale, order queue, behaviour stack |
| `guard_orders.csv` | each change of a unit's order queue head, as hex |
| `guard_posts.csv` | group attack command posts and unit attack task constructions, with a rough backtrace and the target |

Analysis: `analysis/guard_report.py <run>` (local) prints per unit how far it moved after the
first rout, its INTERCEPT and FIRE entries, FIRE entries against shattered targets, and the
answers the fix changed.
