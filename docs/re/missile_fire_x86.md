# Infantry missile fire and fire by rank (x86)

How a gun infantry unit's fire by rank works in the original 32-bit `Empire.Retail.dll` (Ghidra
names at image base `0x10000000`). Found for the fire-by-rank fix (`fixes/fire-by-rank-fix/`) on
2026-10-18: statically, then measured with the fix's probe (`FBRFIX_PROBE=1`) in three replays
of matchlock ashigaru against advancing infantry. Prose only; the decompiles stay local.

## State machines

Many battle behaviours are small state machines with one shared runner.

- **States** are static objects. Each state's vtable has five slots: update (returns the next
  state, or null to stay), enter, tick, exit, and a name getter that loads a pointer from a name
  table and returns it (for example `WAIT_FOR_GROUP_ALL_LOADED_AND_READY_STATE`).
- **The runner** `FUN_11374cf0` takes the machine context in ECX; the current state sits at
  context + 4. It calls the state's update. On a non-null result it calls the old state's exit,
  stores the new state and calls its enter. Otherwise it calls the state's tick. It reports
  "done" once the current state is the context's finished state (a shared sentinel object). The
  runner has 33 call sites; measured, it runs about 10 000 times per game second in a small
  battle.
- **Squad tasks** (behaviours on the unit's behaviour stack, `docs/re/unit_finish_x86.md`) keep
  their machine context at task + 0x18 and their unit at task + 4. A state method gets the
  context and works on the task 0x18 bytes below it. The task's next-state getters are
  consecutive slots of its vtable; each returns one static state object.

## The missile attack machine

A unit attacking with missiles runs a machine with the states `FACE_TARGET`, `FIRE`,
`FIRE_UNFORMED`, `WAIT_FOR_CAN_SHOOT_TEST`, `PERFORM_CAN_SHOOT_TEST`, `CANNOT_SHOOT`, `REFORM`,
`BUILD_REFORM_SPINE`, `ATTACK`, `HALT` and others. `WAIT_FOR_CAN_SHOOT_TEST` waits until a
can-shoot test for the unit has completed; `PERFORM_CAN_SHOOT_TEST` branches on the result.
Entering `FIRE` (`0x100FA0C0`) creates the unit's volley task.

Measured: from the first target soldier within the matchlock's effective range (100 m,
`projectiles.shogun_matchlock`) the machine reaches `FIRE` within about 3 s, through
`PERFORM_CAN_SHOOT_TEST`, `BUILD_REFORM_SPINE` and `REFORM` in a few ticks. A fire-at-will unit
fires its first shot 2 to 6 s after that point.

## The group fire task: Shogun 2's fire by rank

Vtable `0x115C1048`, constructor `FUN_101295d0` (base `FUN_10127ef0`). The matchlock units with
the fire-by-rank ability run this task.

| Field | Meaning |
|---|---|
| +0x04 | unit |
| +0x18 | machine context |
| +0x38 | number of groups: the formation's rank count, **capped at 3** by the constructor |
| +0x40 | current group (0 = front) |
| +0x44 | byte: the current group was ordered to fire |
| +0x45 | byte: cleared when the group number wraps |
| +0x48 | group per soldier, a dword per soldier index (soldier + 0x218); filled from the formation, one group per rank |

States (getters at vtable + 0x58 .. + 0x70): START, REFORM, then the cycle:

1. **WAIT_FOR_GROUP_ALL_LOADED_AND_READY**. On entry every man of the current and later groups
   gets a ready / reload order (task slot + 0x84 = `FUN_10188290`: order type 5, with a kneel
   flag for every group but the last). Ends when every man of the current group passes the
   ready test (`FUN_1017b980`, its only caller).
2. **ORDER_GROUP_FIRE**: fire orders for the current group (each carries the target record,
   `docs/re/artillery_aim_x86.md`).
3. **WAIT_FOR_GROUP_FIRE_COMPLETED**: ends when every man of the group passes the ready test
   again (his fire order is done). Its exit gives the group's men reload orders (kneeling),
   except for the last group.
4. **ADVANCE_GROUP**: the group number goes up; past the last group it wraps to 0 (or the task
   finishes and the missile machine re-forms the unit and starts a new one).

The ready test (`FUN_103cbe40`, the one decoded for movement): a man with an order (byte
+ 0x384 = 1) that is not completed (byte + 0x308 not 1) is not ready, unless the action at
+ 0x248 reports ready. A soldier is "loaded" when his reload time at + 0x390 is at or below 0
(soldier vtable slot + 0xF8). The group test itself does not look at the reload time: a man
counts as ready once his last order is done.

Measured (three replays, two fire-by-rank units each):

- The first wait lasts 9 to 11 s although every man is loaded: it waits for the last man to
  finish his ready pose (most men are ready within 1 to 2 s, a few take up to 9 s while standing
  still). With the aim before the first shot (about 3.5 s) a fire-by-rank unit fires 9.5 to 16.4
  s after the enemy is in range, against 2 to 6 s with fire at will.
- A fire-completed wait lasts 5 to 7 s (aim, shot, recovery); the next group's wait is then
  short while the reloads overlap.
- With the cap at 3, the rear ranks of a formation deeper than 3 ranks never fire: 40 of 160
  men in a 4-rank Hattori unit, 65 of 200 in a 5-rank Ikko Ikki unit. The code handles more
  groups (kneel flag, wrap at the count); only the constructor caps it.

## The sword officer (Hattori, Ikko Ikki)

`unit_stats_land.officer` references `battle_personalities`. Every working matchlock unit has
the officer `officer_teppo` (musket animations, a gun). The Hattori matchlock ashigaru and
samurai and the Ikko Ikki matchlock ashigaru have `officer_katana` (katana animations, no gun),
as artillery crews do.

The officer is soldier 0 of the unit, in group 0, of his own soldier class (vtable
`0x115DBFB4`, shared by both officer kinds). Soldier + 0x48C reads 8 for every gun man and for
the gun officer, 0x10 for the sword officer. Measured order stream of the officer:

| Order | `officer_teppo` | `officer_katana` |
|---|---|---|
| type 5, standing (first wait) | completes | completes |
| fire | fires, his reload starts | completes without a shot |
| type 5, kneeling (after group 0 fired) | completes | **never completes** |

So after group 0's first volley the sword officer never becomes ready again. When the group
number wraps, the wait for group 0 never ends: the unit fires three volleys (one per group) and
then stops firing and reloading for good. Measured: 20 s of wait until the end of the battle in
both Hattori units, every man loaded and ready except the officer.

## The row fire task (not run by these units)

A second volley task exists (`SquadTaskFireVolley.cpp`, vtable `0x115C0564`, constructor
`FUN_10127d40`, created by `FUN_10177780` from `FIRE`'s enter) with the states
`WAIT_FOR_CURRENT_ROW_LOADED_AND_READY`, `CURRENT_ROW_ADVANCE`, `CURRENT_ROW_FIRE`,
`INCREMENT_CURRENT_ROW` and `WAIT_FOR_FINAL_ROW_LOADED` (fields: + 0x30 current row, + 0x34
rows, + 0x58 (row, column) per soldier index). Its wait gives reload orders only on entry and ends
when every man of the current row is ready. None of the units in the test replays ran it.
