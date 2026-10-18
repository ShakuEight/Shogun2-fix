# Artillery aiming (x86)

How an artillery piece chooses where to aim and hands that to its fire routine, in the original
32-bit `Empire.Retail.dll` (Ghidra names at image base `0x10000000`). Found for the aim fix
(`fixes/artillery-targeting-fix/`, named aimfix at the time) on 2026-10-18 with a probe DLL, a hardware write watchpoint and replay runs of a
Fall of the Samurai battle (3 batteries of 4 Armstrong guns against 3 units of line infantry).
Everything marked measured was seen live; the rest is read from the decompile.

## The target record

Every aimed action uses the same 0x18-byte target record:

| Offset | Field |
|---|---|
| +0x00 .. +0x08 | offset x, y, z (floats) added to the target's position |
| +0x0C | target entity (a soldier, a gun, ...) or 0 |
| +0x10 | target object (a building or similar), used when there is no entity |
| +0x14 | transform: when neither entity nor object is set, the offset is a local point mapped through this matrix |

`FUN_103a6850` (__thiscall, ECX = record, arg = out xyz) is the getter: entity position
(entity +0x48 / +0x4C / +0x50) plus the offset; else the object's position plus the offset; else
the transformed or plain point. It has about 30 callers. Measured: artillery shots at infantry use
an entity record with offset (0, 1.2, 0), i.e. 1.2 m above the chosen soldier's feet.

## Who is the shooter

The guns are entities of their own class (vtable `0x115E65F4` for the Armstrong gun) and are the
members of their unit's soldier array (`[unit+0x1C]`, count `[unit+0x18]`; 4 per battery,
measured). Like soldiers they reach their unit through `[entity+0x3B4]` (measured for both guns and
infantry). Six entity classes (vtables `0x115E65F4`, `0x115E684C`, `0x115E6AB0`, `0x115E6D14`,
`0x115E6F1C`, `0x115E801C`) hold `FUN_1045a2e0` at vtable slot `+0x1B0`; the infantry soldier class
(`0x115DFEC8`) does not. `FUN_1045a2e0` is the engine classes' "take a fire order" method: it builds
an engine fire action (below) from a 0x44-byte order that carries the target record.

The script function `is_artillery` (binding `0x101C1EA0`, test `FUN_10180440`) works on a different
object than the battle unit: its fields read 0 on both the battery and the infantry unit.

## From the fire order to the shot

1. **Engine fire action.** Constructor `FUN_10409980`, vtable `0x115E7D94`, created by
   `FUN_1045a2e0` inside the gun (`gun+0x24C`). It copies the order: the target record ends up at
   action +0x20, the gun is at action +0x60, a "handed over" byte at +0x7C starts at 0.
2. **Hand-over.** `FUN_1047fd90` is the action's update. On its first run (`[+0x7C] == 0`) it copies
   the record into the gun, with `FUN_1045a8a0` or, for the broadside "fire now" case (config
   `fire_now`, `EngineActionShoot.cpp`, and a flag on the action), `FUN_1044fb60`, then sets +0x7C.
   Both copy the 0x18 bytes to `gun+0x71C` and set a few aim parameters beside it.
3. **Fire routine.** `FUN_10488c40` (gun, per tick) turns towards and fires at its own record
   `gun+0x71C`: `FUN_10420d00(gun+0x71C)` gets the position through `FUN_103a6850` and computes the
   launch (`FUN_10420050`), then the projectile is created. Measured: exactly one getter call per
   shot from there (return address `0x10420D16`), 60 shots in the test replay.

Measured cadence: a new fire action reaches each gun about every 0.4 game s (every 4 ticks) while
the battery engages, 57 hand-overs per gun in the test replay; the soldier in the record changes
between some of them. The orders come from higher up (the unit's missile behaviour); that level
was not decoded.

The engine shoot-action state machine around `FUN_103d3xxx` / `FUN_103d4xxx` (states TURN,
GET_AMMO, MISFIRE, AIM, AIM_RELOAD, AIM_FINISHED, FIRE, RELOAD, WATCH; descriptor table at
`0x115DF8A8`) did not run for these guns: no getter call came from that range.

## Which soldier the game picks

Measured over 60 shots (fix off): the chosen soldier is not the one nearest to the gun (its rank by
distance from the gun is spread over the whole unit), but each gun keeps a characteristic lateral
offset along the enemy line, matching the gun's own position in its battery. In battery A the guns
from right to left aimed 3, 5, 13 and 21 m from the enemy unit's centre. Battery B stood off to one
side of its target, and all four of its guns aimed at the flank soldier, 26 m from the centre of a
53 m line, on every volley. Overall: median 22 m from the centroid, 47 % of the shots at the outer
tenth of the line, almost all in the front rank. The selection itself happens above the fire
action (the order already carries the soldier) and was not read.

## Where the aim fix acts

At the entry of `FUN_1047fd90` while `[action+0x7C] == 0`, for a gun whose vtable slot `+0x1B0` is
`FUN_1045a2e0`, and a record whose target is an entity and has no object or transform: the entity
is replaced by the member of `[entity+0x3B4]`'s soldier array nearest to the array's centroid
(x / z). Both copy paths and every later read (turning, launch) then use that soldier.
