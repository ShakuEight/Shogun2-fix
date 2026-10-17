# From slot to soldier: formation type, slot assignment, per-soldier steering (x86)

Phase 10 Task D, second branch (after slot generation). Written 2026-10-14 by a
static pass only (Ghidra headless, `-readOnly -noanalysis`, projects s2x86 and
s2ce unchanged). It was started because of three live results:

- In E1, the per-tick readers of the node width were the record copy and the
  corner piece, as predicted.
- In E3, a 49 m width reached those readers, yet the user saw no change.
- In E1, the corner order went single file on plain nodes 7.3 m or wider.

The user also confirmed that cavalry (the test unit "general") and infantry
both go single file round corners and through bent gates.

Status words:
- CONFIRMED: the x86 and x64 decompiles agree on the fact.
- x86 / x64 only: read in one build.
- CANDIDATE: inferred and not compared.

Decompiles are in `docs/walk_decompiles/x86/` and `docs/x64_decompiles/`. x64
files are named after their x86 counterpart, or `x64_*` / `form0_vt*` /
`x64_goto_*` / `x64_entity_*` when there is no x86 file.

Listings and sweeps are in `docs/listings/phase10b_*` and
`prologues_phase10b.txt`. Logs are `logs/phase10b_*.log`.

New read-only Ghidra scripts in `tools/`:

| Script | What it does |
|--------|--------------|
| `find_disp.py` | Functions using a set of struct displacements |
| `find_vcall.py` | Virtual calls on an object loaded from a field |
| `read_vtable.py` | Vtable entries |
| `vtables_with_slot.py` | Every vtable long enough to have slot X, and the function in that slot |

## Answer in one paragraph

The slot chain is real, and soldiers are sent to their slots. The problem is
how they get there: each soldier navigates to its slot with its OWN navmesh
path query, unrelated to the unit's corridor.

- About once per second (FUN_101bc880), the unit's movement controller:
  - lays the formation's slots along the unit path, starting from a point
    2 x speed ahead of the leader;
  - gives soldier i the slot i (the order comes from the distance-based
    assignment FUN_101537d0);
  - issues each soldier a "go to this point" order (FUN_103dbbf0).
- The soldier entity then pathfinds from its own position to that point:
  - the replan state machine is FUN_103df3f0, with the path query
    FUN_10385fd0;
  - it follows the resulting waypoints (FUN_10394e80).
- That pathfinder is a separate function family. It does not reach the unit
  corridor builder FUN_1039bb40 / FUN_1804d8d00 (call-graph BFS, depth 5, no
  path found).

A shortest navmesh path to a point around a convex corner passes that corner's
vertex. So every soldier whose slot lies past the corner is routed through the
same vertex, and the column forms there whatever width the slots have.

This explains all three live results:
- The 49 m corridor override (E3) changes only where the slots are, not the
  route to them.
- The E1 corner order went single file on 7 to 27 m plain nodes.
- Cavalry and infantry behave the same: the same entity go-to slot,
  FUN_18051cdb0 (x64), sits at +0x3A0 in 26 entity vtables; x86 FUN_103d9860
  sits at +0x1D0 in 26 vtables.

The hypothesis is CONFIRMED in structure: each soldier makes its own path query
to its slot. The claim that the per-soldier path hugs the inner vertex is
CANDIDATE: FUN_10394e80 (7.3 KB, waypoint selection) was not read line by line.

## S1. Formation type (cavalry and infantry)

The formation object [unit+0x624] (x64 +0x6D8) is created by FUN_1014e9e0:
`switch(type)` over 8 classes (row 8 of `placement_chain_x86.md`). The type
argument comes from FUN_101672b0 (x86; 7 of the 11 call sites of FUN_1014e990
pass its result). Its own name table is FUN_10179c70 (strings):

| Type | Name (FUN_10179c70) | Chosen by FUN_101672b0 when bit set in [unit+0xED4] |
|------|---------------------|------------------------------------------------------|
| 0 | block_formation | none of the bits below (default) |
| 1 | diamond_formation | bit 3 |
| 2 | pike_square_fomation | bit 0 |
| 3 | pike_wall_fomation | bit 6 |
| 4 | square_formation | bit 1 |
| 5 | wedge_formation | bit 2 |
| 6 | light_infantry_formation | bit 4 |
| 7 | we_stand_and_fight_formation | bit 0x2B |

[unit+0xED4] is the set of active special-formation abilities (vfunc +4 =
test bit). The remaining call sites pass 0 or a stored value: FUN_101fc9d0 /
FUN_101f4bc0 for merged or split units, and the unit constructor
FUN_10126a20 = param_2[2].

So a unit with no special formation active uses type 0, block_formation. That
holds for infantry and for a general's cavalry bodyguard alike. Cavalry uses
type 5 (wedge) only while the wedge ability is on.

Status:
- The switch, the four floats and the vtable are a CONFIRMED pair (earlier
  phase).
- The type getter and the names are x86 only, backed by the name strings.
- That the test unit had no formation active is CANDIDATE (not captured).

Type 0 vtable 0x115e05e0 (x64 0x1821c0980). Slot 9 = FUN_103c4720 (x64
FUN_180503950), the piece/row chain that uses the node width. Further entries,
read in x64 (`form0_vt*.c`):

| Slot | Function | What it does |
|------|----------|--------------|
| +0x20 / x64 +0x20 | vt04 | files for a width = max(1, round((W + 0.001 - f0) / f2) + 1), clamped to the men count. The same formula as the row placer. |
| +0x28 / +0x50 | vt10 | file index of man i, filled centre-out with alternating sides |
| +0x2C / +0x58 | vt11 | rank index = i / files |
| +0x08 / +0x10 | vt02 | depth = (ranks - 1) * f3 + f1 |

The piece/row chain therefore applies to cavalry and infantry alike, with the
node width entering through the row width. No cavalry-specific code was found
in the steering path (S3); the only special cases in FUN_101bc880 are slot
types 3 / 4 / 5 -> animation codes 0x51..0x53.

## S2. Slot assignment FUN_101537d0 (x64 FUN_18021aa00)

Called as FUN_101537d0(unit, slot list, flag, out = unit+0x518 vector {cap,
count, data}, leader index).

1. Builds 12-byte triples {soldier, slot*, cost}:
   - a. soldiers whose vfunc +0x114 is true come first (or last when bit 1 of
     [unit+0xED4] = square formation is set);
   - b. then the leader index;
   - c. then the rest in soldier-array order.

   Slot k goes to the k-th triple.
2. FUN_10105470 = pairwise swap optimiser on the triples:
   - Cost FUN_10140700(soldier, slot) = d^2 + 0.5 * (1 - min(1, cos)).
     - d = straight-line distance from soldier +0x48 / +0x50 to the slot point
       (FUN_10399f80).
     - cos = soldier facing (ushort +0x64 into the sin/cos table) dotted with
       the direction to the slot.
   - Repeatedly takes the worst triple below a threshold and swaps it with a
     partner if that lowers the sum of the two costs.
   - Settings by flag:

     | Flag | Threshold | Passes |
     |------|-----------|--------|
     | 0 | 16.0 (4 m) | one pass |
     | 1 | FLT_MAX | until no swap |

   - The cost is purely Euclidean: walls and corners are invisible to it.
3. Persistent result:
   - The unit's soldier array [unit+0x1C] is rebuilt in triple order by
     FUN_10179b10 (x64 FUN_1802467d0), so soldier index i <-> slot i.
   - FUN_1017c9c0 sets unit+0x2778 ("formed up") when the mean
     soldier-to-slot distance < 4.0.
   - The slot list itself is a local and is freed at the end of FUN_10179b10
     (CONFIRMED: both builds free the local data pointer). The slot pointers
     left in unit+0x518 dangle.

When it runs:
- FUN_101a9750 calls FUN_101afcd0 -> FUN_10179b10 -> FUN_101537d0 on its first
  call (this+0x74 latch).
- After that, only while [unit+0x734] == 0 (FUN_1014f770; x64 [unit+0x838]).
  CONFIRMED, both builds have the same gate.
- The early return in FUN_101a9750 (param 0 and FUN_101b3c60 false) is x86
  only.
- FUN_101bc880 also runs the assignment itself once per order:
  - x64 calls FUN_18021aa00(unit, ctrl+0x70, flag 1, ...) under the
    ctrl+0x68 latch;
  - it stores slot pointers per soldier (soldier +0x2A0 index) in table
    ctrl+0x98;
  - the x86 counterpart is FUN_101afab0 under ctrl+0x44.

Consequence for E1 / E3: the reads of node +0x74 by FUN_10351690 are piece
copies made by FUN_1036b980 on every call of FUN_101a9750. The row chain that
consumes the width runs only:
- where the formation's slot 9 is called: FUN_10179b10 (gated as above),
  FUN_101bc880 (about 1/s), FUN_1017c6a0 (deployment / teleport) and
  FUN_1017c960;
- so "read per tick" does not mean "used per tick".

## S3. What a soldier steers to

Per-tick controller FUN_101b7b70 (x64 FUN_18028e100):
- leader cursor (FUN_1014ee60);
- FUN_101a9750 (gated slot regeneration);
- then FUN_101bc880(0) when `tick * 0.1 > ctrl+0x2C + 1.0`:
  - ctrl+0x2C is the last run time;
  - the tick is [[ctrl+0x14]+8]+0x50;
  - x64 uses ctrl+0x48 and FUN_180294d10(ctrl, 0).

That is about once per second of game time. CONFIRMED pair FUN_101bc880 <->
FUN_180294d10: the same call order, the 0x24 / 0x38 slot stride, the same 3 /
4 / 5 special codes, the navmesh test mode 3, and the same branch structure.

FUN_101bc880, per run:
1. Advances a copy of the leader cursor ctrl+0x34 by 2 x speed (+ per-rank
   term) along the unit path (FUN_103670a0, x64 FUN_18049bb90).
2. Builds a target from it (FUN_1036b980) and calls formation slot 9 (+0x24)
   -> slot list (0x24 bytes per slot: +0x00 flag byte, +0x04 location
   {x, y, z, region, region+0x80}, +0x18 ushort heading, +0x20 slot type).
3. For soldier i of [unit+0x1C] and slot i:
   - The slot flag is 0 and the slot location is not on a walkable navmesh
     region (FUN_103b24a0(loc, 3) null, or region vfunc +8 false):
     FUN_103dafb0(soldier). This is x64 FUN_18051f420, CONFIRMED. It puts the
     soldier into the second state (vtable 0x115dd698): no target.
   - Otherwise: FUN_103dbbf0(soldier, order). This is x64 FUN_180520b70,
     CONFIRMED.
     - order = {slot location (5 dwords), heading, speed, ...};
     - placement-new of a "go to point" state at soldier+0x2F8 (x64 +0x410),
       vtable 0x115de144 (x64 0x1821bd678);
     - when the state enters (x64 FUN_1804bbaf0), it calls the entity's
       vfunc +0x3A0 (x86 +0x1D0) with the point.
   - Fixed-assignment branch (ctrl flags): FUN_1018abb0 (x64 FUN_180258c80)
     with the per-order slot table.

The entity is [soldier+0x2F0] (x86; state+8 = soldier+0x2F0, FUN_103506f0).
x64: [soldier+0x400]. CANDIDATE for x86 by analogy with x64.

Entity go-to: x86 FUN_103d9860 / x64 FUN_18051cdb0, the same function in the
vtables of all entity classes found (26 each).
- It installs state vtable 0x115e13e4 (x64 0x1821c2498).
- Enter FUN_103e88b0 (x64 FUN_180532020) -> FUN_103df290 (x64 FUN_180524860)
  stores the destination at entity+0x420..+0x430 (x64 +0x5D8) and sets
  state entity+0x3BC = 1 (x64 +0x530).
- Then calls FUN_103df3f0 (x64 FUN_180524980). CONFIRMED pair: the same
  states 1..7 and the same flag constants 0x10 / 0x18 / 0x200 / 0x20 / 0x400 /
  0xA0 / 0x4040.

Replan state machine FUN_103df3f0:

| State | What happens |
|-------|--------------|
| 1 | Builds a path request: FUN_103ccd00 from the entity's location to the destination +0x420, with a radius from entity vfunc +0x124. If FUN_10378c70 cannot reuse the previous path (same end region and FUN_103b2240), it queries FUN_10385fd0(request) (x64 FUN_1804c0340) and stores the result in the entity's path object +0x3C0. |
| 2 | Waits for the asynchronous result. |
| 3 / 4 | FUN_10394e80 (x64 FUN_1804d0620, 7.3 / 7.9 KB) picks the next waypoint from the path and the current location (entity+0x524 via FUN_103df250) into entity+0x454..+0x464. FUN_103942f0 then steers towards it with the given speed and flags. |

The path query family (FUN_10385fd0 / FUN_1804c0340 -> FUN_1804c1f70,
FUN_1804c1ea0, FUN_1804c1170, FUN_1804a1630) does not reach the unit corridor
builder (x64 BFS, depth 5). Callers of FUN_1804c0340: the entity replan and
FUN_1804704b0 only.

Hypothesis check:
- Each soldier takes its own path to its slot: CONFIRMED (structure, both
  builds).
- "So all men pass the inner corner vertex": CANDIDATE. It is what any
  shortest path does round a convex corner, but the waypoint selection in
  FUN_10394e80 (string pulling, corner offset by radius) was not read.
- The unit corridor width (node +0x74), the corner clearance and the slot width
  do not enter the soldier's route.
- Shared by cavalry and infantry: CONFIRMED by the shared entity go-to.

## S4. Corner rows FUN_103e03a0 (x64 FUN_180525b90)

CONFIRMED pair, compared line by line:
- The same guard `|cross| >= 0.1` (DAT_115b2448 = 0.1).
- The same negative-projection early return.
- The same loop count round(proj / spacing) + 1.
- The same side flip when piece+4 (x64 +8) == 2.
- The same carry-over write.

Per step along the corner piece's segment (at rank spacing gen+0x10):
- The row direction is the unit vector from the pivot (piece+0x94 / +0x98;
  x64 +0xD0 / +0xD4) to the step point, flipped by side.
- The row centre = pivot + dir * piece+0x9C * 0.5.
- The ROW WIDTH = piece+0x9C (x64 +0xD8), passed to the row placer (x86
  FUN_103e0640 -> generator slot 0; x64 vfunc +8).

So corner rows fan round the pivot like a wheel. Files per corner row =
max(1, round((min(Wreq, piece+0x9C) + 0.001 - f0) / f2) + 1). piece+0x9C is
the width clamped by the clearance +0x7C (row 6 of `placement_chain_x86.md`),
which the v15 override does not touch.

E3's corner node with clearance 0.154 m therefore gives 1 file per corner row
(CANDIDATE: the exact clamp form is from the earlier pass). This is
secondary: after S3, slot width only moves the targets.

## S5. Hook points and v16 dump points

Stolen bytes are from `docs/listings/prologues_phase10b.txt` (dump_prologue,
5-byte minimum):

| x86 function | RVA | Stolen bytes | Relocatable | Use |
|--------------|-----|--------------|-------------|-----|
| FUN_101bc880 (per-soldier orders, ~1/s) | 0x1BC880 | 6 (SUB ESP,0xC8) | yes | ECX = controller; unit = [ctrl+4] (FUN_100dbce0) |
| FUN_103dbbf0 (soldier go-to-slot) | 0x3DBBF0 | 5 (PUSH EBX/ESI/EDI; MOV EDI,ECX) | yes | ECX = soldier, [ESP+4] = order: +0x00..+0x10 location (x, y, z, region, aux), +0x14 ushort heading, then speed |
| FUN_103dafb0 (slot off navmesh) | 0x3DAFB0 | 6 | yes | ECX = soldier |
| FUN_103df3f0 (entity replan) | 0x3DF3F0 | 6 (SUB ESP,0x94) | yes | ECX = entity |
| FUN_10385fd0 (soldier path query) | 0x385FD0 | 5 | yes | arg = request (FUN_103ccd00 layout, not decoded) |
| FUN_10394e80 (next waypoint) | 0x394E80 | 6 | yes | fix candidate: offset the waypoint |
| FUN_101537d0 (assignment) | 0x1537D0 | 6 | yes | fix candidate: path-aware cost |
| FUN_10140700 (assignment cost) | 0x140700 | 7 | yes | fix candidate |
| FUN_10105470 (swap optimiser) | 0x105470 | 6 (MOV EAX,FS:[0x2C]) | yes | the FS read is absolute, OK |
| FUN_103df290 (set destination) | 0x3DF290 | 8 | NO (relative CALL at +3) | do not hook |

v16 capture (to decide the question live without new risk):
- Per run of FUN_101bc880, from its entry hook: for the player unit, walk the
  soldier array [unit+0x1C] (count [unit+0x18]).
- Per FUN_103dbbf0 hit: record (soldier, target x/z from the order).
- Per tick, from the existing FUN_101a9750 hook, for each soldier s of the
  player unit and entity e = [s+0x2F0] (CANDIDATE pointer; validate that
  e+0x48 equals s+0x48 or is near it on the first dump), record:
  - s+0x48 / +0x50 position x / z, +0x64 heading (ushort);
  - e+0x3BC entity move state (1 replan, 2 wait, 3 / 4 follow, 7 fail);
  - e+0x420 / +0x428 destination x / z (the slot);
  - e+0x454 / +0x45C current waypoint x / z;
  - e+0x3CC path result pointer (non-null when a path exists).
- Expected at a corner if S3 holds:
  - the destinations are spread over the slot fan;
  - the current waypoints of most soldiers coincide at one point near the
    corner vertex;
  - the positions collapse onto it.
- Fix candidates follow from which of these is seen:
  - offset each soldier's waypoint by its file offset in FUN_10394e80;
  - or give the soldier an intermediate target on its own lateral line of the
    unit corridor before the corner (FUN_103dbbf0 order rewrite);
  - or a path-aware assignment cost.

Ghidra project changes in this pass: none (all runs `-readOnly`).
