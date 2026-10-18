# fire-by-rank-fix (fbrfix): how it was found and how it works

Function names are Ghidra names in the original 32-bit `Empire.Retail.dll` at image base
`0x10000000`. The engine decode is in
[docs/re/missile_fire_x86.md](../../docs/re/missile_fire_x86.md); this file is the route to it
and the design of the fix.

## 1. The report

User (2026-10-18), four points:

- Matchlock units are slow to start shooting once an enemy is in range.
- Fire by rank (FBR) makes it worse; the ranks seem to cycle animations that block shooting.
- For Hattori and Ikko Ikki, FBR fires the first 3 ranks and then nobody reloads.
- Theory: their special officer model breaks the reload animation.

Test material: three replays, `firebyrank_generic`, `firebyrank_ikkoikki` and
`firebyrank_hattori`. Each has two FBR units and two fire-at-will units of matchlock ashigaru
against advancing infantry, and lasts about 2.5 minutes with the autorun tool.

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

## 5. The fix

Three entry hooks on functions that only the group fire task uses: `FUN_1017b980` and
`FUN_1017b9d0` have one caller each (the two wait states), and `FUN_10188290` is a slot of the
task's vtable.

1. **`FUN_10188290`** gives one man his ready / reload order. For a man whose + 0x48C differs
   from his unit's most common value, it returns at once, with the function's own `RET 8`. So the
   sword officer never gets an order he cannot finish. The "most common value" rule needs no
   constant for the shooter class: in a working unit every man, the officer included, shares it,
   and nothing changes.
2. **`FUN_1017b980`** answers "is the current group loaded and ready". The fix answers it for
   the game:
   - yes, when every man of the group in the shooter class is loaded and ready;
   - yes, when they are all loaded and the wait has lasted the grace time (15 ticks, 1.5 game s,
     counted from consecutive calls for the same group);
   - no, while one of them is still reloading.

   Men outside the shooter class are not counted. The ready test itself is the game's own
   `FUN_103cbe40`, called directly, after a byte check of its entry.

Why a grace time and not "loaded only": the fire order that follows cancels a pending ready order
and makes the man aim and fire anyway. Most men are ready within 1 to 2 s, so the grace time only
cuts off the long tail. The measured fire-completed waits (5 to 7 s) are the aim and shot
themselves and stay untouched.

3. **`FUN_1017b9d0`** (part 2, early reload) answers "has every man of the current group
   finished his fire order". The question came from the user: does rank 1 reload while the
   others fire? The probe's dumps showed that a man's reload time is set at his shot but frozen
   until the fire-completed exit (`FUN_101be8f0`) gives the group its reload orders, and that
   this wait holds for the men who get no shot. The hook gives each shooter who has fired
   (reload time above 0; the wait before the volley required everyone loaded) and whose own
   fire order is done his reload order at once, through the game's `FUN_10188290` with the
   exit's after-fire flag, so the kneel decision stays the game's. It then counts him as done,
   although his new order is open. A per-soldier table keyed by the turn (task, group, first
   tick of the wait) makes it one order per shot; `FUN_10188290`'s hook skips the exit's second
   order to a man who had his early one this turn and is still reloading. Option
   `FBRFIX_FIRE_GRACE` (off) also ends the turn a set time after the group's last shot.

Option `FBRFIX_ALL_RANKS=1` raises the group count before the first volley to the number of
ranks present (at most 8). It is off by default, because it changes the game's design (see
README.md).

Self-check: 20 group tasks in a row without the decoded layout (and no answer given yet) switch
the fix off for the session. All reads and the one write are SEH-guarded. The hooks run on game
threads and take no locks beyond a short spin lock on the wait table.

Switch: one flag read on entry. It starts on (`FBRFIX_OFF=1` starts it off); Y flips it through
the shared key poll thread.

## 6. Result

| Unit | First shot after the enemy is in range |
|---|---|
| FBR, fix off | 9.5 to 16.4 s |
| FBR, fix on | 2.1 to 8.1 s |
| Fire at will (unchanged) | 1.9 to 5.8 s |

The first wait fell from 9.1 to 10.6 s to 1.6 s. The Hattori units no longer hang (20 s of hang
off, none on) and fire 6 volleys instead of 3. Offline harness: 20 + 17 + 5 + 4 + 26 checks.

Part 2 (early reload), same replays, fix on throughout: the 90th percentile of the time from a
shot to the reload starting fell from 7 to 9 s to 2.5 s (fire at will 2.2 s), and in the Hattori
replay a man's median time from shot to shot from 29 to 32 s to 23 to 24 s (fire at will 25 to
26 s). Shot totals are within the replay drift; with every rank firing as well, the Hattori units
fired about 30 % more. The table is in README.md. Harness: 28 + 22 + 5 + 4 + 5 + 27 checks.

## 7. Open

- No public release source yet (`release/`), so no release equivalence run.
- The game's own remaining waits: one man still reloading holds his group, and a restarted task
  (the unit re-forms) always begins with group 0, even when group 0 has just fired. A fix would
  let the next loaded group fire instead.
- Not decided: whether every rank should fire (`FBRFIX_ALL_RANKS`), and the grace time.
- Measured on matchlock ashigaru only. The other gun units with fire by rank (matchlock samurai,
  including the Hattori samurai with the same sword officer, and warrior monks) were not run.
  The row fire task has no test replay.
- Fire-at-will acquisition takes 2 to 6 s. It was left alone, because the can-shoot test reacts
  within ticks and most of that time is aiming.
