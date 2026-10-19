# Buildings, wall pieces and their destruction (x86 Empire.Retail.dll)

Found for wall-collapse-fix (2026-10-19). Ghidra names at image base 0x10000000. Decompiles and
the scan scripts stay local in `fixes/wall-collapse-fix/analysis/`.

## Where the buildings are

- The battle object is `[tick object +0xB4]` (the phase machine: phase at +0x14; tick object from
  the pointer path in `console_commands_x86.md`). It lists its buildings at +0x118, count +0x114
  (the same offsets an army uses for its units).
- A building lists its pieces at +0x1D0, count +0x1CC.
- A piece points to its owner at +0x0C, and `[owner +8]` is the tick object again.
  `[tick object +0x5C]` is a settings object; its byte +0x9C means the castle's gates are open
  to everyone (see below).

## A piece

| Offset | Meaning |
|---|---|
| +0x18 | template: level count +0x6C, level models +0x70; each level model has its level index at +0x10 and a damage threshold (int) at +0x90 |
| +0x40 / +0x44 | list of collapse effects (next at +4, kind at +0xC; kinds 2 and 3 are spawned when the piece is destroyed) |
| +0x54 | level derived from the damage |
| +0x64 | current level model |
| +0x68 | destruct level (written from the level model by the apply function) |
| +0x6C | byte: gate piece |
| +0x78 | variant (gate open / closed) |
| +0xB4 / +0xB8 / +0xBC | position |
| +0xEC | damage taken (float); level = first level whose threshold is above it |
| +0xF1 | destroyed |
| +0xF2 | destroyed before the battle (counted when the battle reports fort damage back to the campaign, `FUN_10475b20`) |
| +0xF3 | takes damage |
| +0x1AD | byte read by the campaign damage when it picks pieces to destroy |

Most castle pieces have two or three levels; the last level is the destroyed one.

## Functions

- `FUN_10477f40` (ECX = piece, level, variant; RET 8) applies a level model through
  `FUN_10477100`. That one swaps the piece's model, writes +0x68 and, when the model really changes,
  rebuilds the building's navigation data (`FUN_104399d0`, `FUN_104876f0`). It has no early exit
  for an unchanged level, but a same-model apply skips the rebuild.
- `FUN_10477ff0` (ECX = piece, int damage; RET 4) sets the damage, derives the level, starts the
  collapse effects if the damage equals the destroyed threshold, then applies the level.
- `FUN_1047b370` (ECX = piece, float damage; RET 4) adds damage. It returns 3 at once for a
  destroyed piece. Otherwise `FUN_104757a0` raises the level when the damage passes the current
  threshold: collapse effects (`FUN_10479ca0`), level apply, follow-ups for a destroyed piece, and
  damage passed on to a linked piece (+0x74). This is the path of projectiles, rams and the battle
  start's gate damage.
- `FUN_10479ca0` (ECX = piece) spawns the collapse effects of kinds 2 and 3 through two objects
  embedded in the manager at `[tick object +0x98]` (+0x534 and +0x94, vtable slot +8).
- The scripted commands `BCQ_BUILDING_PIECE_INCREASE_DESTRUCT_LEVEL` and `..._DECREASE_...` step
  +0x68 by one and apply it. `BCQ_BUILDING_SET_MAX_DESTRUCT_LEVEL` marks every piece of a building
  (+0xF0). Their names are registered by static initialisers in a table at 0x115A19AC, each with a
  creator function (0x102A6470, 0x102A64A0, 0x102A64D0).
- Gate state: `FUN_10487410` (each piece with +0x6C; nothing for a destroyed one) keeps the gate
  open while the gates-open flag is set. `FUN_104362e0` lets every alliance through a gate when the
  flag is set or the gate is destroyed. The piece set-up (`FUN_104076e0`) builds a gate open
  (+0x78 = 1) when the flag is set.

## Battle set-up and the battle start

- `FUN_10173ba0` ("Manager finalise: validate buildings") runs `FUN_1044cfc0` for every building.
  When the building's +0x23C is above 0, that sets every piece's damage to that fraction of its
  destroyed threshold. Then `FUN_10174140` ("finalise_fort_info") applies the campaign's fort damage
  with `FUN_10426940`: per-piece damage from the campaign record, then the given number of pieces
  (+0x1AD set) nearest a point are destroyed outright and marked +0xF2 (`FUN_10430c60`). All of this
  happens before deployment, so deployment shows it.
- The battle start is `FUN_101787b0`. The phase 5 handler `FUN_101b9f10` runs it once, on the
  first fighting tick, guarded by battle +0x18. It fires the battle-started event, runs a per-army
  step (`FUN_101788a0`), then calls `FUN_1043c870` for every building. That one calls
  `FUN_1043c8b0` per piece: a gate piece (+0x6C) gets one more damage than the threshold of its
  current level when the gates-open flag is set. An intact gate door is destroyed at that moment;
  a destroyed one ignores it (returns 3).
- Units deployed on the gatehouse top above such a door lose the men standing over it the tick
  after (replay "broken wall bug": 10 men at height 8 m, in a row over the door). None of them
  goes through the soldier hit point setter `FUN_103a3080`: they leave their group's men list
  directly. The kill routine for men on a collapsing piece was not traced.

## Soldier hit points

`FUN_103a3080` (soldier, hp, flag; stdcall RET 0xC) is the only setter of soldier +0x680 found.
It is reached from the melee and collision outcomes (`FUN_103a6140` from `FUN_10368d50`) and from
`FUN_103691d0` / `FUN_104282d0`. Men killed by a collapsing piece do not pass through it, and it
was called only 3 times in a 4.5-minute wall assault (replay "wall siege bug"), so most deaths take
another path.
