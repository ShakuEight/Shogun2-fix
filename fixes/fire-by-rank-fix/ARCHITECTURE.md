# fire-by-rank-fix (fbrfix): how it was found and how it works

Function names are Ghidra names in the original 32-bit `Empire.Retail.dll` at image base
`0x10000000`. The engine decode is in
[docs/re/missile_fire_x86.md](../../docs/re/missile_fire_x86.md); this file is the route to it
and the design of the fix.

## 1. The report

User (2026-10-18), four points, then two questions during the work (does rank 1 reload while
the others fire; what happens when the unit re-forms into a different depth):

- Matchlock units are slow to start shooting once an enemy is in range.
- Fire by rank (FBR) makes it worse; the ranks seem to cycle animations that block shooting.
- For Hattori and Ikko Ikki, FBR fires the first 3 ranks and then nobody reloads.
- Theory: their special officer model breaks the reload animation.

Test material: three replays, `firebyrank_generic`, `firebyrank_ikkoikki` and
`firebyrank_hattori`. Each has two FBR units and two fire-at-will units of matchlock ashigaru
against advancing infantry, and lasts about 2.5 minutes with the autorun tool. A fourth,
`firebyrank_reform_deep`, re-forms a firing unit into a deep formation.

## 2. The data

The unit tables were extracted with RPFM from `data.pack` and the enabled mods
(`analysis/extract_db.py`, local only) and compared side by side (`analysis/diff_units.py`).
The Hattori and Ikko Ikki matchlock units differ from every working one in a single column:
`officer = officer_katana` (katana animations, no gun) instead of `officer_teppo`. The mods
change none of these rows. The user's theory pointed at the right column.

## 3. Finding the machines (static)

- The state-name strings (`WAIT_FOR_CURRENT_ROW_LOADED_AND_READY_STATE`, ...) lead to name
  getters. The getters lead to five-slot state vtables, then to static state objects, then to
  the owning task's vtable of next-state getters (`analysis/states.py`).
- That decoded the row fire task (`SquadTaskFireVolley`) and the shared runner `FUN_11374cf0`.
  Its transition point became the probe's main hook: every state change of every machine, at
  no cost between transitions.

## 4. Probing the live game

- **Probe 1** hooked the runner entry, the transition, and the two soldier order functions. It
  showed that the row fire task never runs. FBR units run a different task, the group fire task
  (vtable `0x115C1048`). The timing confirmed the report: FBR fires 9.5 to 16.4 s after the enemy
  is in range, fire at will 1.9 to 5.8 s. The first group wait alone takes about 10 s. The Hattori
  units end the battle stuck in a group wait.
- **The group task decode** (`FUN_101295d0` and its states) showed the cap of 3 groups, the
  ready / reload order `FUN_10188290` with a kneel flag, and the group test `FUN_1017b980`.
- **Probe 2** dumped every soldier once per game second while a group waits. In both Hattori
  units, after the wrap to group 0, every man is loaded and ready except soldier 0. He is of the
  officer class (vtable `0x115DBFB4`), with an open order that never completes.
- **The order stream** set the sword officer against the gun officer of the generic replay. Both
  complete the standing ready order and the fire order. Only the gun officer completes the
  kneeling reload order that follows the volley; the sword officer never does.
- **Probe 3** dumped raw soldier objects. Soldier + 0x48C reads 8 for every gun man and the gun
  officer, and 0x10 for the sword officer.
- **The rotation** (the user's question whether rank 1 reloads while the others fire): the
  per-group dumps showed the reload time frozen from the shot until the fire-completed exit
  (`FUN_101be8f0`), 2 to 9 s, and 7 to 15 s of reload left in group 0 when the rotation came
  back with 3 groups. A `counting` event (reload started) in the probe measured it per man.
- **Re-forms** (the user's question about new formations): static only. The group table is
  filled from the formation by `FUN_101540f0` on the task's first tick and on every re-form, and
  a late task restart in the Hattori runs always began at group 0 just after it had fired.
  Then a replay with a re-form into a deep formation (`firebyrank_reform_deep`).

## 5. The fix

Four entry hooks on functions that only the group fire task uses: `FUN_1017b980` and
`FUN_1017b9d0` have one caller each (the two wait states), `FUN_10188290` and `FUN_101883f0`
are slots of the task's vtable (+0x84, +0x80). One turn of a group goes through all four:

1. **Who fires next: `FUN_1017b980`**, the group test of `WAIT_FOR_GROUP_ALL_LOADED_AND_READY`,
   answered by the fix. One pass over the unit counts per group its shooters (men of the unit's
   shooter class), those still reloading (+0x390 above 0) and those loaded but not ready (the
   game's own ready test `FUN_103cbe40`, called directly after a byte check of its entry).
   - Every rank: before the first volley (group 0, count 3, the game's cap) the group count is
     raised to the highest group in the task's table + 1 (at most 64). The rest of the task
     already handles more groups (kneel flag, wrap at the count).
   - Front rank first: the front-most group with 75 % of its shooters loaded takes the turn
     (task +0x40 is written), whichever group's turn it was. After its volley the game advances
     to the next group as usual, and the next test again picks the front-most loaded group, so a
     front rank that has reloaded fires before the rear ranks, and the rear ranks fire while the
     front ones reload. Two refinements came from the runs:
     - The wait's entry (`FUN_1019a9a0`) gives its ready orders (slot +0x84, flag 0) to the
       current and later groups only. A front group taking the turn gets the same orders from
       the fix (the test answers "not yet" for that tick); without them, one man of a kneeling
       front rank aimed for 22 s without a shot in the Hattori replay and held the unit.
     - "Loaded" also means "never got a shot": rear ranks without a line of fire stay loaded and
       took the turn ten times in a row in the Ikko Ikki replay. A per-group memory of the last
       turn (men given fire orders, men who fired, noted by the fire-completed hook) lets a group
       take the turn out of order only if at least a quarter of them fired; otherwise it fires on
       its own turn. A signature of the group table (count and every soldier's group) clears the
       memory when the game rebuilds the ranks.
   - The chosen group fires when all its shooters are loaded and ready; when they are loaded and
     the wait has lasted the grace time (15 ticks, counted from consecutive calls for the same
     group); or 1 s after 75 % of them are loaded and ready (quorum, a timer per wait).
   - Men outside the shooter class (the sword officer) are not counted.
2. **The fire orders: `FUN_101883f0`**, called by the `ORDER_GROUP_FIRE` enter once per man. A
   man still reloading (the group fired on the quorum) gets none (`HOOK_RETURN`, the function's
   own `RET 8`): he keeps his reload order and sits the volley out. A per-soldier table marks
   him (task, tick).
3. **The end of the turn: `FUN_1017b9d0`**, the test of `WAIT_FOR_GROUP_FIRE_COMPLETED`. The game
   sets a man's reload time at his shot but runs it only under a reload order, and gives the
   group its reload orders at the exit of this wait, which waits for the slowest man. The fix
   gives each shooter who has fired (reload time above 0) and finished his fire order the game's
   own kneeling reload order at once (`FUN_10188290` with the exit's after-fire flag, so the
   kneel decision stays the game's), and counts him as done although his new order is open.
   Men sitting out do not count. The per-soldier table, keyed by the turn (task, group, first
   tick), makes it one order per shot. The turn also ends 5 s after the group's last shot
   (`FBRFIX_FIRE_GRACE`, 50 ticks): a safety net for a man who aims and never fires.
4. **The ready and reload orders: `FUN_10188290`.** None for a man outside the shooter class, so
   the sword officer never gets the kneeling order he cannot finish (the "most common +0x48C"
   rule needs no constant: in a working unit every man, the officer included, shares the
   value, and nothing changes). None either for a man who is still reloading under his early
   order or sitting out, when the exit orders the whole group.

Why a grace time and not "loaded only": the fire order cancels a pending ready order and makes
the man aim and fire anyway. Most men are ready within 1 to 2 s, so the grace time only cuts off
the long tail.

A re-form or a new task: the game fills the group table from the formation on the task's first
tick (`FUN_101b6d80`, task slot +4) and on every `REFORM` enter (`FUN_101a5600`), through
`FUN_101540f0` (for each array position, its rank from task slot +0x78). So a new formation gets
new ranks, each man keeps his own reload time, and the front-first rule picks the front-most rank
that has enough men loaded.

Options, for A/B runs: `FBRFIX_ALL_RANKS=0`, `FBRFIX_FRONT_FIRST=0`, `FBRFIX_QUORUM=<percent>`
(0 = off), `FBRFIX_QUORUM_WAIT`, `FBRFIX_GRACE`, `FBRFIX_EARLY_RELOAD=0`, `FBRFIX_FIRE_GRACE`.

Self-check: 20 group tasks in a row without the decoded layout (and no answer given yet) switch
the fix off for the session. All reads and the two writes (group count, current group) are
SEH-guarded. The hooks run on game threads and take no locks beyond short spin locks on their
small tables.

Switch: one flag read on entry. It starts on (`FBRFIX_OFF=1` starts it off); Y flips it through
the shared key poll thread.

## 6. Result

Fix off against the fix on, four replays (table in README.md): the delay from contact to the
first shot falls from 9.5 to 16.4 s to 2.1 to 8.1 s, the Hattori units no longer hang (20 s each
before), the shot totals rise by 6 to 28 %, and the re-form replay gets 9 volleys instead of 6.
Per man, the time from a shot to the start of his reload falls to the fire-at-will value. The
offline harness has 42 + 26 + 5 + 6 + 5 + 28 checks. The run-by-run route (each rule measured on
its own) is in CLAUDE.md and the local research log.

## 7. Open

- No public release source yet (`release/`), so no release equivalence run.
- The depleted test (`FUN_10173550`, task slot +0x74: groups 0..2 thinned out, the task ends and
  the unit re-forms) runs only when the turn passes the last group; with front rank first that
  happens less often.
- Rear ranks of deep formations often have no line of fire (their fire orders end without a
  shot); front rank first makes them fire less.
- Measured on matchlock ashigaru only. The other gun units with fire by rank (matchlock samurai,
  including the Hattori samurai with the same sword officer, and warrior monks) were not run.
  The row fire task has no test replay.
- Fire-at-will acquisition takes 2 to 6 s. It was left alone, because the can-shoot test reacts
  within ticks and most of that time is aiming.
