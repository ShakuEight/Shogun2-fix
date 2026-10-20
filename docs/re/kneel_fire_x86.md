# Kneel fire and the move-order gate (x86)

Decoded 2026-10-20 for kneel-fire-move-fix (`fixes/kneel-fire-move-fix/`). Ghidra names at image
base 0x10000000. Quotes of the decompiles stay local (`fixes/kneel-fire-move-fix/analysis/`).

## The ability

- Fall of the Samurai's Kneel Fire is `unit_abilities` key `kneel_fire_ability` (`requires_effect_enabling`
  true, `stationary_for_turn` false; its tooltip says "This ability works when the unit is
  stationary"). Line infantry get it from `unit_to_unit_abilities_junctions`; the technology effect
  `enable_ability_kneel_fire` maps to ability number **0x39** (`FUN_10f2ef00`, the effect-to-ability
  table that also gives stand_and_fight 0x2B).
- The battle unit's active-ability set is the object embedded at unit **+0xED4** (vfunc +4 = test a
  bit, see `soldier_steering_x86.md` S1). Kneel fire on = bit 0x39.
- The firing drill is unit **+0xED0**, names in `FUN_10178690` / parser `FUN_101b15c0`:
  0 fire_volley, 1 mass_fire, 2 platoon_fire_dispersed, 3 platoon_fire_grouped,
  4 platoon_fire_column, 5 rank_fire, 6 kneel_fire.

## Switching it on and off

- `FUN_101ac250` (ECX = unit, ability number, ...) is the ability toggle handler, a switch on the
  ability number. When the ability is off it first asks `FUN_1015be80` (may this ability be
  switched on now) and refuses if not.
- Fire by rank (7) and kneel fire (0x39) share one case: switching on sets the drill to 5 or 6
  and activates the ability's effect (`FUN_1014acc0`); switching off restores the unit type's
  default drill and deactivates it (`FUN_10167a60`). Nothing else differs between the two.
- `FUN_1015be80` allows 7 and 0x39 to be switched on only while `FUN_10180730` is false: the
  behaviour on top of the unit's stack (unit +0xB9C, count +0xBA8) is asked through its vfunc
  +0x100.
- The AI switches both off and on around movement: the HOLD_THE_LINE state (vtable 0x115C3618,
  one of the AI line states ADVANCING_TOWARDS_LINE / FORM_ON_LINE / CREEP_FORWARD /
  HOLD_THE_LINE) turns abilities 7 and 0x39 back on in one slot (`FUN_101ff6c0`, through
  `FUN_101872a0` with the ability number) and off in another (`FUN_101ff5e0`); a second pair
  (`FUN_101efaa0` / `FUN_101ef990`, also ability 6) sits in the state vtable near 0x115C4AA0.

## What the drill does

The drill is read only when firing starts: the missile attack task's constructor
(`FUN_10127460`) and its FIRE enter (`FUN_10177780`) call the drill getter `FUN_10178620`. The
FIRE enter builds the volley task from the drill; drill 5 builds the group fire task
(`FUN_101295d0`, `missile_fire_x86.md`), drill 6 a kneel-fire volley task (`FUN_10128460`). A move
order replaces the attack task, so nothing in the firing code needs the unit to stand still.

## The move-order gate

`FUN_1015c3f0` (ECX = unit, returns AL) is "can this unit take a move order". Its first step
answers no when bit 0x39 is set; fire by rank has no such step. Then: in battle phases 5..8
(`FUN_10180600` on the battle object reached through unit +0x14 -> +8 -> +0xB4), the behaviour
on top of the unit's stack must accept (vfunc +0xDC) and `FUN_1015cb40` must pass (no rout or
similar, unit +0xF18 not 0x10, ability 0x3A off). Outside those phases the answer is yes.

Callers:

| Call | Function | Role |
|---|---|---|
| 0x104008EA, 0x10400993, 0x10400ED3 | `FUN_10400870` | click move placement builder: the one selected unit, then the selection filter (twice) |
| 0x10401D4F, 0x10402469 | `FUN_10401c50` | drag move (orientation and width) placement builder: selection filter |
| 0x1048C817 | `FUN_1048c7d0` (called by `FUN_10401c50`) | drag move: the one selected unit |
| 0x1043682B, 0x10436872 | `FUN_104367a0` | any unit of a selection can move (no direct caller found) |
| 0x10185E8A | `FUN_10185da0` | the unit's own target step (25 callers: AI, attack orders): a unit that cannot move takes a missile target only when it is in range |

The builders run both for the UI and inside the battle command executors:

| Battle command (name holder) | Executor | Builder |
|---|---|---|
| `BCQ_MULTIPLE_SELECTION_ORDER_MOVE` (`FUN_1001ab40`) | `FUN_102a8be0` -> `FUN_1045ebf0` -> `FUN_1042dde0` | `FUN_10400870` |
| `BCQ_MULTIPLE_SELECTION_ORDER_MOVE_ORIENTATION` (`FUN_1001ad40`) | `FUN_102a8e90` -> `FUN_1045e930` -> `FUN_1042df70` | `FUN_10400870` |
| `BCQ_MULTIPLE_SELECTION_ORDER_MOVE_ORIENTATION_AND_WIDTH` (`FUN_1001adc0`) | `FUN_102a8f60` -> `FUN_1045e830` -> `FUN_1042de50` | `FUN_10401c50` |
| `BCQ_MULTIPLE_SELECTION_ORDER_TELEPORT_TO_LOCATION` (`FUN_1001b0c0`, deployment placement) | `FUN_102a9290` -> `FUN_10460b20` -> `FUN_1042de50` | `FUN_10401c50` |

The kneel step comes before the battle-phase step, so a kneeling unit cannot be repositioned
during deployment either.

The UI side reaches the same builders through `FUN_10264120` / `FUN_102642c0`. So a kneeling
unit is dropped from a move order when the command executes, in single player, multiplayer and
replays alike.

`FUN_1015c3f0`'s first 26 bytes are whole instructions with no branch into them
(`prologues_kneelfix_skip.txt`); entering at +0x1A with the function's own frame (PUSH ESI;
MOV ESI,ECX) runs the rest of the test without the kneel step.
