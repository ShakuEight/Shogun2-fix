# Guard mode and the missile attack task's chase (x86)

2026-10-20, static (s2x86, `empire.retail.dll`, image base 0x10000000; read-only Ghidra runs and
capstone over the on-disk DLL) plus the guard-mode-ranged-fix probe
(`fixes/guard-mode-ranged-fix/`, `GUARDFIX_PROBE=1`) on the replay `guard mode` (two groups of one
yari, one matchlock and two bow units, one group in guard mode, attack orders on enemy units that
rout). Prose only; the decompiles stay local in `fixes/guard-mode-ranged-fix/analysis/decompiles/`.

Names as in `rout_x86.md`: the **battle unit** is what a soldier points at with +0x214, a
**group** what it points at with +0x3B4 and what a squad task holds at task +4. A group's battle
unit is at group +0x510.

## The guard flag

- Guard mode is the byte **+0xEB1** of the battle unit (1 = on).
- Set by `FUN_10186130` (ECX = unit; stack: a word, then the new value). It does nothing while
  the unit counts as routing; otherwise it stores the byte and posts a unit voice event (on / off).
- Its only caller is `FUN_1045d0b0`, which walks the units of a selection; that is reached from
  the battle command `BCQ_MULTIPLE_SELECTION_ORDER_CHANGE_GUARD_MODE` (name holder `FUN_1001a2c0`,
  stream creator and executor `FUN_102a8340`, registered with the name string). The Lua
  `unit_controller` command `guard_mode` (`FUN_101c4410`) builds the same command object
  (`FUN_10254e10`, vtable 0x115CFFE8, the value at +0x48) and posts it, so the interface, scripts
  and replays all pass through the one executor.
- Unit setup `FUN_10150ac0` copies the unit's default from +0x17A (and other defaults into +0xEB0,
  +0xEAD, +0xEB2).

### Readers

Only four places of the battle code read +0xEB1:

| Function | Reading |
|---|---|
| `FUN_101af030` | the melee attack task's "keep pursuing" test: answers no when the guard byte of the task's unit is set (other conditions aside) |
| `FUN_10169390` | classification of an enemy unit in the unit's awareness list: for a guard unit an enemy that is not a valid non-routing unit is ignored unless it is the target of the unit's current order |
| `FUN_101b8740` | builds that awareness list (enemies within a radius); the guard byte changes how many candidates are kept (half instead of a quarter of the formation's count) |
| `FUN_104390e0` | picks one entry from a list of units with orders, guard units first (purpose not decoded) |

(`FUN_1122a090` in the interface reads it too.)

So the melee no-pursuit is a single test in the melee attack task (0x115C0DFC): its
PURSUE_STATE update (thunk 0x100F9CF0, `FUN_1019fa80`) keeps chasing only while `FUN_101af030`
says yes. Nothing on the missile side reads the guard byte.

## The missile attack squad task

- Vtable 0x115C0F24, constructor `FUN_10127460`, built by the deferred attack command start
  `FUN_103d0470` when the command's missile byte (+0x28) is set (otherwise the melee attack task,
  or the yari wall attack task when that ability is on). The unit controller's
  MISSILE_ATTACK_STATE enter (`FUN_1018a4d0`) and FIRE_AT_WILL_INTERRUPT_STATE enter
  (`FUN_101773c0`) post such commands through the group attack post `FUN_103daa80`.
- Fields: +0x24 the **target battle unit**, +0x28 a range value, +0x2C and +0x2D two bytes of
  the order; **+0x2D set = never intercept**. +0x170 / +0x171 cache the last can-shoot test
  (done / passed; written by `FUN_10157500`, read by `FUN_1015d130`).
- States (getters at task vtable +0x58 .. +0x80): START, INTERCEPT, HALT, FACE_TARGET,
  BUILD_REFORM_SPINE, FIRE, FIRE_UNFORMED, WAIT_FOR_CAN_SHOOT_TEST, PERFORM_CAN_SHOOT_TEST,
  REFORM, CANNOT_SHOOT; the finished state is the shared object 0x117C0810.

### The chase

- `FUN_101aefc0` (ECX = task): true when +0x2D is clear and the cached can-shoot test failed.
  The FIRE (thunk 0x100FA100), FIRE_UNFORMED (0x100FA1D0) and PERFORM_CAN_SHOOT_TEST (0x100FA300)
  updates go to **INTERCEPT_STATE** when it is true.
- INTERCEPT's enter (`FUN_1017d740`) pushes a direct intercept squad task (0x115C0A48,
  `FUN_101283f0`) towards the target onto the group's behaviour stack: the group walks. Its update
  (0x100F8070) ends the task when the target is no longer valid and goes to HALT once the intercept
  task has finished (the missile task is again on top of the stack).
- Without INTERCEPT, PERFORM_CAN_SHOOT_TEST runs its other tests (fire unformed, halt, face
  target, reform spine, reform) and ends with FIRE when the cached test passed, else
  **CANNOT_SHOOT_STATE**, whose update returns the finished state: the task ends.
- `FUN_101b2460` (ECX = task): true when the target unit is no longer a valid target
  (`FUN_10182620` on the target false, or `FUN_10180ba0` on it non-zero). The FIRE, FIRE_UNFORMED,
  PERFORM_CAN_SHOOT_TEST and INTERCEPT updates end the task when it is true. A broken or
  shattered unit stays valid.
- `FUN_10181e50` (ECX = unit): a valid unit that does not count as routing and whose rout state
  is not 4; `FUN_10181e90` is its negation.

The same INTERCEPT update code also serves a second, naval missile task (vtable 0x115C1418,
constructor `FUN_10127690`, states with BOARD_SHIP / THROW_DOWN_BOARDS), whose FIRE and
PERFORM_CAN_SHOOT_TEST updates (0x100F8110, 0x100F8350) use the sibling test `FUN_101aefa0`.

## Fire at will: the unit interrupt machine

- The unit's interrupt machine (context at unit +0xE78, its unit at context +0x10; getter table
  of the context at 0x115BB04C) has NO_INTERRUPT, ROUT_INTERRUPT, MOVING_TO_RALLY_POSITION,
  MELEE_INTERRUPT, **FIRE_AT_WILL_INTERRUPT** (vtable 0x115B4DE0), SKIRMISH_INTERRUPT and
  CELEBRATE_INTERRUPT. (The unit controller's own FIRE_AT_WILL_INTERRUPT_STATE, vtable 0x115B51BC,
  is a different machine.)
- Every few ticks the machine's tick (`FUN_101bdbe0`) stores a candidate from the awareness list
  at context +0x18 (`FUN_1016b350` -> `FUN_1018a140`: the preferred entry if its class is 1, else
  the last class-1 entry) and the fire-at-will target at context +0x1C (`FUN_1016aa00`).
- `FUN_1016aa00` builds the list of enemy units within missile range (`FUN_1018a590` is the
  range) and lets `FUN_10175650` / **`FUN_101752d0`** choose: cdecl (unit, list, current
  target(s), range, extra range); the list is an array of battle unit pointers with its count at
  list +0x2800 (at most 0xA00). Each valid enemy in range is scored: minus its distance, a bonus
  when it is the current target, a penalty when it does not count as a valid non-routing unit.
  The best one is returned. A shattered enemy therefore stays a choice. The other callers are
  `FUN_10178300` and `FUN_10177e30`; every caller rebuilds the list for each call.
- NO_INTERRUPT goes to FIRE_AT_WILL_INTERRUPT when `FUN_101aee90` sees a target at context +0x1C
  (or +0x24) and the order queue is empty. Its enter (thunk 0x100F8EB0, `FUN_10170990`) builds a
  unit attack task (vtable 0x115BF200, constructor `FUN_1012d660`, target at task +0x18) on the
  unit's own task stack (unit +0xA00); that task's first update (`FUN_101b9290`) posts the group
  attack command, which builds the missile attack squad task above.
- An explicit attack order lives in the unit's order queue; when its missile attack task ends
  (CANNOT_SHOOT, or the target test), the order is done and the queue is empty, so fire at will
  takes over.

## The replay `guard mode`

Probe runs 2026-10-20 (`fixes/guard-mode-ranged-fix/`, all dev fixes at their defaults, speed 1,
`--linger 90`; the battle is decided at tick 953 and the replay plays on to tick 1262):

- Fix off: the three guard-mode missile units fire at the target of their attack order; it
  shatters and flees; at ticks 985..989 their FIRE update finds it out of range (139..166 m) and
  goes to INTERCEPT: they walk 32..47 m after it. The order's no-intercept byte +0x2D is 0.
- A first fix that only ended the task on a shattered target made fire at will start a new unit
  attack task against the next shattered enemy every 0.2 s (442 attack commands, each task ended
  at once by the target test): fire at will's choice has to leave shattered enemies out.
