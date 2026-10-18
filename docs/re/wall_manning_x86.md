# Manning a wall: the order, the staging march and the walk onto the wall (x86)

2026-10-18, static (s2x86, `empire.retail.dll`, image base 0x10000000; read-only Ghidra runs and
capstone over the on-disk DLL) plus the wall-manning-fix probe (`fixes/wall-manning-fix/`,
`WALLFIX_PROBE=1`) on the replay `wall_manning_pathin` (three 150-man yari ashigaru units of the
defender ordered onto two wall sections). Prose only; the decompiles stay local in
`fixes/wall-manning-fix/analysis/decompiles/`.

Names: the unit is the object a unit move is built for (the soldier list at +0x18 count, +0x1C
array; its behaviour stack owner sits 0x20 above it). The wall task is the squad task described
below.

## The wall task

- Class: vtable 0x115BF91C, constructor `FUN_1010d810` (callers `FUN_10186640`, `FUN_10172220`),
  destructor `FUN_10134f30`. The machine context sits at task +0x34 (vtable 0x115BFA4C); state
  methods get the context and work on the task 0x34 bytes below it.
- Fields seen: +0x40 and +0x44 / +0x48 from the constructor's arguments (the target), +0x4C..+0x5C
  a position built from them, +0x64..+0x74 the staging point C {x, y, z, region, aux}, +0x78 byte
  "staging march needed", +0x7C the unit, +0xAC byte (passed on to the move), +0xAE byte (set
  once the slots are being taken), +0xB0 / +0xC4 the slot assignment, +0xBC / +0xC0 count and
  array of the wall positions the unit is to hold (objects with a position getter at vtable
  slot +0x0C / +0x10).
- Five states; their getters are task vtable slots +0x11C..+0x12C (state objects 0x11796DE4,
  0x11796DE8, 0x11796DEC, 0x11796DF0, 0x11796DF4). The probe resolves their names at run time
  (slot +0x10 of a state's vtable, as in `missile_fire_x86.md`).

### State 1 (enter `FUN_101b0b60`, update thunk 0x100FA4B0)

1. Takes the bounding box {min x, min z, max x, max z} of the wall positions (+0xBC / +0xC0).
2. Asks `FUN_1016af90(box)` whether the unit is far: true when the unit's position is farther
   from the box centre than half the box diagonal plus the unit's arrival radius (unit +0x670).
   During deployment (`FUN_10180760`) it is always false. The call is at 0x101B0CA8 (return
   0x101B0CAD); the function has one other caller, `FUN_10197e00`.
3. If far: +0x78 = 1 and C = `FUN_10175f10(box)`, then `FUN_101542e0` places the formation at C
   (the unit's formation heading and width, unit record +0x440 +0x28 / +0x24). Else +0x78 = 0.
4. The update: no wall positions left (+0xBC == 0) -> finished; +0x78 set -> state 2; else ->
   state 3.

### The staging point `FUN_10175f10`

- When the region of the target position (task +0x58, the region field of the position at
  +0x4C) passes its vfunc +0x28 test: with battle +0x190 zero, or no zone found for the region
  (`FUN_10369940`, `FUN_10464a00`), C is the target position itself. Otherwise C lies on the
  line from the box centre towards the zone's point (`FUN_101fe800`), about half the box
  diagonal plus half the formation depth out. In the measured castle that point lies inside the
  castle, 38 m from the wall section's centre, whatever side the unit comes from (which branch
  produced it was not logged; CANDIDATE: the zone line).
- Otherwise: from the box edge towards the unit, in 2 m steps, the first spot where the whole
  formation rectangle (width x depth) fits on clear navmesh (`FUN_1034ec50` + `FUN_103699f0`,
  the same test the unit path uses for a straight corridor); the box edge if none fits.

### State 2 (enter `FUN_1017e5a0`, update `FUN_1017e620`)

- Enter: an ordinary unit move to C with the formation's width and a heading (`FUN_103be120`
  builds the parameters, `FUN_103db410` creates the unit's move action).
- Update: done when the unit's formation stands in its placement rectangle (unit +0x508 / +0x498
  flags and the rectangle at unit +0x644..+0x664). Measured: the formation then re-forms at C for
  5 to 9 s before state 3.

### State 3 (enter `FUN_101b2010`)

- Marks +0xAE, assigns the men to the wall positions (`FUN_101505d0` over +0xB0 / +0xC4) and
  starts the per-man form-up (below). The men walk there one by one, each with his own route,
  at the pace of the order (walk or run kept).

## The unit move action (used by state 2 and by many other tasks)

- `FUN_103db410(ECX = unit, arg = parameters)`: parameters +0 path result (may be null), +4
  destination {x, y, z, region, aux}, +0x18 byte heading given, +0x1A heading, +0x20 width,
  +0x24 / +0x25 bytes. Outside deployment it builds the action in place at unit +0x488
  (`FUN_103be080`, vtable 0x115E0884, machine context at action +0x1C).
- The action's states (objects 0x1179A098..0x1179A0AC, getters at its vtable +0x0C..+0x20):
  arrived test (`FUN_103e5560`: within unit +0x670 of the destination) -> done; no path ->
  unit path search (`FUN_103d2620` -> `FUN_10385e90`, incremental, formation width); placement
  at the destination (`FUN_103d2690`); then the move controller (`FUN_103e1250` ->
  `FUN_10128570`, the controller in `unit_finish_x86.md`).
- The unit path search `FUN_10385e90` queues an incremental search on the pathfinder (list
  P+0x114, the 200-work cap per tick in `path_queue_x86.md`); `FUN_1039c4b0` starts it and builds a
  straight path at once when the formation rectangle fits from start to end; `FUN_1039add0` steps
  it. Both end in the unit path builder `FUN_1039bb40`.

## The per-man form-up action

- Constructor `FUN_103be5d0` (vtable 0x115E012C, machine context at +0x18), created by
  `FUN_101a53f0`, `FUN_101a5500`, `FUN_101a5600` (re-form) and `FUN_101a56e0` from task states.
- States 0x11799D00..0x11799D08: stop every man (`FUN_103d4c30`, each man's slot-off-navmesh
  order), send every man to his slot (`FUN_103e1780`: per man `FUN_103da190` with the slot record),
  then the stance per slot type (`FUN_103c1c70`).
- Slots: array at action +0x44, 0x24 bytes per man: +4 position {x, y, z}, +0x18 heading, +0x20
  type (1..5; 3..5 are the special stances).

## Measured (replay `wall_manning_pathin`, wall task with the game's behaviour)

| Unit | Start | Wall positions (box centre) | C | Staging |
|---|---|---|---|---|
| east | (104, 542) | (78, 496) | (55, 526) | 37 m west, 9 s re-form, then 33 m back |
| west | (-18, 516) | (-43, 562) | (-9, 576) | 51 m north, 5 s re-form, then 41 m west |
| third | (24, 537) | | (14, 528) | the move found the unit already within its arrival radius: no march |

The men's pace: in formation 2.4 m/s (walk) and 4.2 m/s (run), on the walk to the wall 2.1 and
3.9 m/s.
