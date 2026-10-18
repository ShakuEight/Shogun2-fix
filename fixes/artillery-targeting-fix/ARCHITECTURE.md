# artillery-targeting-fix (artfix): how it was found and how it works

Function names are Ghidra names in the original 32-bit `Empire.Retail.dll` at image base
`0x10000000`. The engine decode is in [docs/re/artillery_aim_x86.md](../../docs/re/artillery_aim_x86.md);
this file is the route to it and the design of the fix.

## 1. The report

User (2026-10-18): artillery ordered to fire at an enemy unit targets the edge of the unit
instead of its centre of mass; spread sometimes puts a shot in the middle anyway. Test material:
the replay `Artilery_shooting` (three units of Armstrong guns, four guns each, each unit firing at
one unit of approaching line infantry), played unattended with `tools/autorun` (138 s of battle,
about 3 minutes per run).

## 2. Finding the fire path (static)

- The binary's strings give the engine module (`EngineActionShoot.cpp`, `Engine.cpp`,
  `ProjectileLaunch.cpp`) and a table of engine action states (TURN, GET_AMMO, MISFIRE, AIM, FIRE,
  RELOAD, ...). Their descriptors lead to the shoot action's member functions around
  `FUN_103d3xxx` / `FUN_103d4xxx`.
- Those functions turn and fire towards a position from `FUN_103a6850`. That turned out to be the
  generic getter of a 0x18-byte target record (offset, entity, object, transform): the aim point is
  one entity's position plus an offset. So artillery aims at one soldier, and the question became
  who picks him.

## 3. Probing the live game

A small research build of this fix (`ARTFIX_PROBE=1`, autorun `--env`) hooked the getter and
logged every call from the engine code, the target record, the chosen soldier, his unit
(`[entity+0x3B4]`, found by searching the entity for a pointer to a unit whose soldier array
holds him) and that unit's soldier positions once per game second.

- Probe 1: none of the `FUN_103d3xxx` state functions ran. All 60 calls came from one place,
  `FUN_10420d00` (launch check), once per shot: 12 guns, 5 volleys. Each shot aims 1.2 m above one
  soldier; in the enemy's 53 m line the soldiers were mostly in the front rank, biased to one end,
  and one battery aimed every shot at the flank soldier.
- Probe 2: a hardware write watchpoint (Dr0..Dr3 on the record's entity field, armed on every
  thread by a background thread, vectored handler) caught the writer: `FUN_1045a8a0`, which copies
  a whole record into the gun at `gun+0x71C`, called from `FUN_1047fd90`, the update of an engine
  fire action. The record really belongs to the gun (`FUN_10488c40`, the gun's fire routine,
  passes `gun+0x71C` to the launch check), which also explained a wrong gun position in probe 1.
- Probe 3: a hook on `FUN_1045a8a0` logged the shooter and the caller's action (vtable
  `0x115E7D94`, constructor `FUN_10409980`, created by `FUN_1045a2e0` from a fire order). With the
  gun positions, the pick is not "nearest soldier to the gun": each gun keeps a lateral offset in
  the enemy line that follows its place in the battery (docs/re note, "Which soldier the game
  picks").
- Probe 4: the guns are entities of their own class and the members of their unit's soldier array;
  the infantry soldier class is different, and only six classes carry the engine fire-order method
  `FUN_1045a2e0` (vtable slot `+0x1B0`). The script `is_artillery` test reads another object, not
  the battle unit, so it was not used.

## 4. The fix

Where: the entry of `FUN_1047fd90` (stolen bytes `PUSH EBX; PUSH ESI; MOV ESI,ECX; CMP byte
[ESI+0x7C],0`, 8 bytes, no branch into them). There the action still holds the record it is
about to hand to the gun, through either copy function (`FUN_1045a8a0` or `FUN_1044fb60`), so one
hook covers both, and every later use (turning, launch) sees the changed record. Changing the
record before the copy follows the repository rule "change the game's inputs, not its outputs".

What: when `[action+0x7C] == 0`, the gun `[action+0x60]` is of an engine class (its vtable slot
`+0x1B0` is `FUN_1045a2e0`) and the record at `action+0x20` targets an entity with no object and
no transform: take that soldier's unit `[entity+0x3B4]`, require that its soldier array holds him,
compute the centroid (x / z) of all members, and write the member nearest to it into the record's
entity field. Offset (aim height), object and transform stay.

Why the soldier nearest to the centroid and not the centroid itself: the record keeps pointing at
a real soldier, so the engine's own handling of an entity target (following a moving target, the
launch check on the entity) is unchanged; a point record would be a different code path.

Why every gun gets the same soldier: that is what "aim at the centre" means; the game's spread
distributes the impacts.

Self-check: a soldier not found in his unit's array is left alone and counted; if the first 20
artillery targets all fail that check, the fix switches off for the session. Reads and the one
write are guarded (SEH); the hook runs on game worker threads and takes no locks.

Switch: the fix state is one flag the hook reads on entry. It starts on (`ARTFIX_OFF=1` starts it
off); a small thread polls the U key every 50 ms, only while a window of the game process has the
focus, and flips the flag. After the self-check switch-off, U is refused.

## 5. Result

Same replay, fix off and on: 60 shots each; aim point median 22.0 m from the target's centroid
off, 0.4 m on; shots at the outer tenth of the line 47 % off, 0 % on; enemy losses 247 off, 320 on
(details and caveats in README.md). Offline harness: 18 + 18 + 7 checks.

## 6. Open

- No public release source yet (`release/`), and so no release equivalence run.
- Tested on one replay: Armstrong guns at line infantry in line formation. Other engine classes
  (mortars, rockets, other guns), fire at will, units in column or square, and targets on walls are
  covered by the same code path but not measured.
- The game's own target choice above the fire action (the unit's missile behaviour) was not
  decoded; the fix corrects its result.
