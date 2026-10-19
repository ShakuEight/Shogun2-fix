# Routing: morale state, the rout interrupt and the group's rout task (x86)

2026-10-19, static (s2x86, `empire.retail.dll`, image base 0x10000000; read-only Ghidra runs and
capstone over the on-disk DLL) plus the zombie-yari-wall-fix probe
(`fixes/zombie-yari-wall-fix/`, `ZOMBIEFIX_PROBE=1`) on the replay `zombie_yariwall` (an army of
150-man yari ashigaru in yari wall sent into the enemy until every unit broke). Prose only; the
decompiles stay local in `fixes/zombie-yari-wall-fix/analysis/decompiles/`.

Names: the **battle unit** is the object a soldier points at with +0x214 (it has a vtable whose
slot +0x10 returns the unit id). A **group** is the object a soldier points at with +0x3B4 and a
squad task with task +4: the soldier list (+0x18 count, +0x1C array) and the behaviour stack
(owner at group +0x20: entries of 8 bytes at owner +0x400, count +0x440, current +0x454). In the
other notes the group is called "the unit".

## The battle unit

- Groups: list A at +0xB90 (count +0xB8C), list B at +0xB74 (count +0xB70); +0xB9C is the main
  group, re-chosen by `FUN_1016b180` (the listed group with the most men, or an attached
  object's when +0xBA8 is set). Most units have one group.
- Order queue: a ring of ten 0x90-byte entries at +0x454 (count +0x9EC, head +0x9F0), cleared by
  `FUN_101601c0` with ECX = unit +0x44C. The unit's own task stack sits at +0xA00 (entries at
  +0xAF8, count +0xB20), cleared by `FUN_1019e400`.
- Morale block at +0xCC4 (methods take ECX = unit +0xCC4):
  - +0xCE8 morale level. Seen in the replay: 1 and 2 for fresh units, 3 steady, 4 shaken,
    5 wavering (the script condition "wavering" tests 5), 6 broken, 7 shattered.
  - +0xCEC rout state. 0 normal, 2 while broken or shattered. The script condition "routing"
    (`FUN_101821e0` on the morale block) is true for 2 or 3 when a battle flag is set; 3 was not
    seen. `FUN_101821d0` is the same test from an object holding the unit at +0x10.
  - +0xD10 rally byte (script condition "rallying").
  - The unit's melee attack and defence values (`FUN_104339a0`, `FUN_104347f0`) are 0 while the
    rout state is 3 or the level is 7; level 6 (broken) keeps them.
- +0xD88 (and +0xD80): the unit's kill count. Found from the probe's unit dumps: it is the field
  that rises with every killer unit's logged kills.
- The interrupt machine's context sits at unit +0xE78; context +0x10 is the unit.

## The interrupt machine (unit +0xE78)

State vtables of the land unit's machine (the state objects the probe sees as 0x115B4Dxx):

| State | vtable | update / enter / tick / exit |
|---|---|---|
| NO_INTERRUPT_STATE | 0x115B4D40 | |
| ROUT_INTERRUPT_STATE | 0x115B4D68 | 0x100F8CA0 / 0x100F8C40 / 0x100F8C50 / 0x100F8C70 |
| MOVING_TO_RALLY_POSITION_STATE | 0x115B4D90 | 0x100F8D40 / 0x100F8D20 / - / - |
| MELEE_INTERRUPT_STATE | 0x115B4DB8 | 0x100F8E30 / 0x100F8DD0 / 0x100F8DE0 / 0x100F8E00 |

Three more machines have a ROUT_INTERRUPT_STATE (vtables 0x115B5180, 0x115B9368, 0x115DA7C4)
with empty ticks; the replay only used the one above.

- Melee interrupt update: routing -> the rout state; no melee targets left -> no interrupt.
- Rout interrupt update: stays while the unit is routing; leaves for the rally state when the
  unit no longer routs and may rally, else for no interrupt.
- **Rout enter** (`FUN_10170cc0`, ECX = context): sets unit +0xBB0 to 1.0; an attached unit
  (+0xBA8) passes the call on; otherwise it clears the order queue, zeroes +0x9F8 and clears the
  unit's own task stack. It does not touch the groups.
- **Rout tick** (`FUN_101bd3b0`, ECX = context, reached through the thunk 0x100F8C50): for every
  group in list A whose byte +0x2781 is 0 it calls `FUN_101a9bf0`.
- **Group rout set-up** (`FUN_101a9bf0`, ECX = group): byte +0x2781 = 1, +0x2774 = 1.0, cancels
  the group's move action (+0x488, flag +0x508), clears the behaviour stack, then allocates and
  pushes the rout task: `FUN_103be670` (vtable **0x115E01F4**), or `FUN_10529820` (vtable
  0x115EDFA8) when two battle-object fields are set. Its other caller is `FUN_101b6490`.
- **Rout exit** (`FUN_101a46d0`): for every group `FUN_101a4690` (cancels the move, clears
  +0x509, pops the stack, zeroes the word +0x2781), then the unit's formation reset and a
  message to the battle.

So a group is set up for the rout once per rout episode; byte +0x2781 is the only memory of it.

## What happens in a normal rout (probe)

Melee interrupt -> rout interrupt; the next tick the group's stack is emptied and the rout task
0x115E01F4 pushed; every man's movement machines (at soldier +0xF8 and +0x7C0) start moving.

## The zombie (probe, replay in sync with the recording)

Units that broke while still under a charge order (stack: intercept task 0x115C82B4 with a direct
intercept 0x115C0A48): in the same tick as the rout transition a melee attack task 0x115C0DFC
starts on the group (START -> INTERCEPT) and its direct intercept finishes at once. The rout task
is not on the stack afterwards; three ticks later the group's default re-form task 0x115C0644
runs (REFORM, then the halt task 0x115E012C: HALT -> REFORM -> ADOPT_STANCES, then IDLE ->
READY). Byte +0x2781 stays 1, so the rout tick never sets the group up again. The unit stays
broken (level 6, rout state 2), its men stand in their yari wall stance, and its kill count keeps
rising: 34 and 40 kills after breaking for the two units the user saw, 19 for a third that was
wiped out standing. Later some of these groups also get direct intercept tasks.

The squad task constructors seen: re-form 0x115C0644 `FUN_101283a0`, direct intercept 0x115C0A48
`FUN_101283f0`, melee attack 0x115C0DFC `FUN_101273f0` (thiscall: task, group, owner, params).

## Kill paths (for reference)

- Pike contact: `FUN_10435410` (thiscall, ECX = resolver; result {outcome, who}, attacker,
  defender; RET 0xC), called from two soldier actions (vtables 0x115E0E54 and 0x115E0E90, update
  in slot +8: `FUN_103ed950`, `FUN_103ee240`). Outcome 5 = kill.
- Collision knockdown: `FUN_10430cf0` (thiscall; attacker, defender, outcome, flag; RET 0x10)
  from `FUN_103c2350`: a soldier pushed into a man who faces him is knocked down or killed, the
  facing man counts as the attacker. Outcome 5 is applied by `FUN_10368d50`, which may still
  turn it into a knockdown.
- Hit points: soldier +0x680, set by `FUN_103a3080` (from `FUN_103a6140`).
