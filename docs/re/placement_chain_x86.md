# Placement chain x86: from path node to each soldier's target slot (Phase 10 Task D)

Date: 2026-10-14. Plan: `docs/PHASE10_placement_plan.md` section 8 (D1..D5).
Static analysis only (s2x86 / s2ce, headless Ghidra). Addresses are x86
`empire.retail.dll` at image base 0x10000000 unless marked x64. Decompiles:
`docs/walk_decompiles/x86/<name>.c`, x64 pairs `docs/x64_decompiles/<x86 name>.c`
(header names the x64 function). Listings, vtable dumps and probes:
`docs/listings/phase10_taskD_x86.txt`; prologues:
`docs/listings/prologues_phase10_taskD.txt` (tools/dump_prologue.py).

CONFIRMED = both decompiles show the same source-level fact. CANDIDATE = read
in one decompile only, or inferred.

## Result in one paragraph

The corridor width IS consumed per tick by soldier placement, but through a
COPY of the node record, which is why the A3 static sweep (reads of
node+0x74 / +0x78) found nothing. Every tick, for each moving unit,
FUN_101a9750 takes the most advanced soldier's progress along the path, then
FUN_1036b9b0 walks the path from the cursor node BACKWARDS (prev links) and
turns each node record into a "piece" object (0x90-byte record copied to
piece+4). The unit's formation object ([unit+0x624], vtable slot 9) lays the
soldiers' target slots out row by row along those pieces. Each row carries the
piece's width (= node+0x74 at the time of the copy), and the row placer computes

    files_in_row = round((min(W_req, W_row) + 0.001 - f0) / f2) + 1   (at least 1,
                                                                        at most the men left)

where W_req is P+0x6c (the requested formation width) and f0 / f2 are
formation constants. So the slots go min(order width, corridor width) wide: at
the 5.51 m gate the engine itself plans about 3 men abreast (with f2 near 2 m),
not 1. No "push to walkable ground" step exists in the slot chain: slot points
are only LOCATED on the navmesh (region lookup and height), never moved (D5).
If the men still go single file through a corridor the slots say is 3 wide,
the collapse is downstream of slot generation (slot assignment, per-soldier
steering / collision), or the formation type in use is not type 0 (see open
points).

## The chain (per tick)

| # | x86 | x64 | What it does | Status |
|---|-----|-----|--------------|--------|
| 1 | FUN_101b7b70 (vtable slot 0x115c1744; 200 bytes, defined as a function in this phase) | FUN_18028e100 | Per-tick update of the unit movement controller `this`: FUN_101a4810 (clamp leader index this+0x30 to the soldier count), FUN_1014ee60 = FUN_10366e40 on the controller's own cursor this+0x34 with the LEADER soldier (x64 inlines it: FUN_18049b660 on this+0x50 with soldier [min(this+0x4c, n-1)]), then FUN_101a9750(flag), then leader flag +0xF5 (x64 +0x135) and "all soldiers ready" byte this+0x0C (x64 +0x18) | CONFIRMED pair |
| 2 | FUN_101a9750 | FUN_18027C820 | For EVERY soldier of the unit (array [unit+0x1C], count [unit+0x18]; x64 +0x30 / +0x28) a copy of the cursor P+0xA8 is advanced by FUN_10366e40 to that soldier's projection; FUN_10391600 keeps the most advanced cursor. Then FUN_1036b980(P, &best, 0) builds a target object and FUN_101afcd0 hands it to the formation | CONFIRMED (known pair; per-soldier loop and "keep the max" visible in both) |
| 3 | FUN_10366e40 | FUN_18049B660 | Segment search: soldier location from soldier+0x524..+0x534 (FUN_103df250), projects it on each node's followed segment (node+0x80 -> node+0x88, via FUN_1008dac0), keeps the closest reachable projection (FUN_10397c30 + FUN_10369940 + FUN_10361ac0); writes (node, t) | CONFIRMED (known pair) |
| 4 | FUN_10391600 | FUN_1804cc720 | Cursor order: same node -> compare t; else walk from P+0xA0 head through +0x08 | x86 read; x64 call site matches |
| 5 | FUN_1036b980 = FUN_10393530 + FUN_1036b9b0 | FUN_1804a1210 = FUN_1804cec40 + FUN_1804a1260 | FUN_10393530: point at progress t on the cursor node's followed segment (lerp between the two located endpoints), located by FUN_10397c30. FUN_1036b9b0: allocates the target object (x86 0x3C, x64 0x58) with that location and (P+0x6C, 2, P+0x08); then for node = cursor node, node = node.prev (+0x04; x64 +0x08) until null: FUN_103c1b00(target, node+0x0C) appends one piece per node | CONFIRMED pair (both walk prev, both append from node+0x0C / +0x18) |
| 6 | FUN_103c1b00 -> FUN_103cc9f0 -> FUN_103ba8d0 / FUN_103ba8f0 | FUN_180500520 (all inlined) | Piece factory. record side (+0x00 = node+0x0C) == 0: straight piece 0x94 (x64 0xD0), vtable 0x115e00d0 (x64 0x1821bff50), record copied by FUN_10351690 to piece+4. side != 0: corner piece 0xA0 (x64 0xE0), vtable 0x115e00f8 (x64 0x1821bffa0); also reads record +0x68 / +0x6C / +0x70 (node+0x74 width start, +0x78 width end, +0x7C clearance), clamps width against clearance into piece+0x9C, and puts the row point at base point -/+ perp * width_start * 0.5 (piece+0x94 / +0x98) | CONFIRMED pair (x64 reads record[0x28..0x2a] = node+0xB8..+0xC0, same min/median and same +- by side) |
| 7 | FUN_101afcd0 -> FUN_10179b10 | FUN_1802467d0 (FUN_101afcd0 inlined) | this = unit. Calls formation->vfunc slot 9 (x86 +0x24, x64 +0x48) with (out slot list, target, P+0x6C, soldier count); then FUN_101537d0 (x64 FUN_18021aa00) assigns soldiers to slots into unit+0x518 (x64 +0x5C8), rebuilds the unit's soldier order from it, and FUN_1017c9c0 (x64 FUN_180249650) sets unit+0x2778 when the mean soldier-to-slot distance drops below 4.0 ("formed up") | CONFIRMED pair (x64 also passes the soldier count, which the x86 decompile hides) |
| 8 | formation object [unit+0x624] (x64 +0x6D8), created by FUN_1014e9e0 (x64 FUN_180215390) from a type 0..7 switch; FUN_1014e990 passes unit record [unit+0x510] floats +0x3AC / +0x3B0 / +0x3B4 / +0x3B8 | FUN_180215390 | 8 formation classes. Type 0: ctor FUN_103bd900 (x64 FUN_1804fc470), vtable 0x115e05e0 (x64 0x1821c0980), four floats f0..f3 at +0x08..+0x14 | CONFIRMED pair (same switch, same pool, same 4 floats) |
| 9 | type 0 slot 9 = FUN_103c4720 | FUN_180503950 | Allocates the slot list (x86 0x24 bytes per slot, x64 0x38), builds a row generator (x86 FUN_103bd490, vtable 0x115e05d8; x64 inline, vtable 0x1821c0960) with gen.Wreq = P+0x6C, gen.f0, f1, f3, gen.f2, the slot list and the unit; then FUN_103e00b0 (x64 FUN_180525830) feeds it the target's pieces in order: first piece vfunc +0x18 (x64 +0x30) with the target location as start point, later pieces vfunc +0x14 (x64 +0x28); each returns the number of men still to place | CONFIRMED pair for the generator fields (x64 shows them without stack ambiguity) |
| 10 | straight piece +0x18 = FUN_103e0140 | FUN_1805258f0 | Steps along the segment (unit vector piece+0x88 / +0x8C = node+0x90 / +0x94) at rank spacing gen+0x10 / +0x14 carry; per step builds a ROW {point, perp (piece+0x5C / +0x60 = node+0x64 / +0x68), WIDTH = piece+0x6C = node+0x74, heading, men left} and calls the generator's row placer | CONFIRMED pair (x64 row width = piece+0xA8 = record+0xA0 = node+0xB8) |
| 10b | corner piece +0x18 = FUN_103e03a0 | FUN_180525b90 | Rows around the corner point piece+0x94 / +0x98 with width piece+0x9C | x86 read; x64 decompiled (same vtable slot), not compared line by line |
| 11 | row placer = generator slot 0 FUN_103e0660 | FUN_180525ed0 (slot +0x08) | files = FUN_103ce750(min(gen.Wreq, row.width), gen.f0, gen.f2) = round((W + 0.001 - f0) / f2) + 1, clamp to men left, at least 1; then FUN_103b6190 places the row | CONFIRMED pair (x64 inlines FUN_103ce750 with the same 0.001 constant) |
| 12 | FUN_103b6190 | FUN_1804f6af0 | LATERAL OFFSET: starts at row point -/+ perp * width * 0.5 (side chosen by slot parity via FUN_103de950), then each further man at +/- perp * spacing; each slot point is located by FUN_10397c30 (mode 3) and appended (FUN_103c0ca0, 0x24-byte slot) | x86 read; x64 call site matches (args: generator, gen+0x28, row, &files, n, gen.f2) |
| 13 | FUN_10397c30 (this = node+0x24 / piece+0x1C sub-object) | FUN_1804D3A90 | D4, below | CONFIRMED (known pair) |

Corner nodes feed the same chain through 6 and 10b; plain (side 0) nodes, like
the whole v14 gate path, through 10. Both carry the node width into the rows.

## D1: the vtable code at 0x101B7B70

Defined as a function in s2x86 by `tools/define_function.py` (one
CreateFunctionCmd at 0x101b7b70, default name FUN_101b7b70, body
0x101b7b70..0x101b7c37, 200 bytes; saved; log `logs/phase10_d1_define.log`).
It is row 1 above. It does not call FUN_10366e40 or FUN_1008dac0 itself: it
calls FUN_1014ee60 (leader cursor this+0x34, FUN_10366e40 with the leader
soldier) and FUN_101a9750 (all soldiers). The segment FUN_1008dac0 builds from
node+0x80 / +0x88 is used only inside FUN_10366e40 / FUN_10393530 to project a
soldier or to place the target point; FUN_10366e40's output (node, t) becomes
the unit's progress and the start of the target object (rows 2..5). x64 pair:
FUN_18028e100 (caller of 18027C820; the other caller FUN_1801e4df0 is the pair
candidate of FUN_10128570). Slot 9 of 0x115c1700.. is 0x101b7b70 at +0x44.

## D2: direction readers (`tools/sweep_loads.py`, `docs/direction_readers_x86.csv`)

Whole image, 65205 functions: 7015 loads of [reg+0x64/0x68/0x90/0x94] and 3261
of [reg+0x74/0x78] after excluding LEA, ESP and EBP-frame bases; 1168 rows with
node score >= 3 written. The score is a heuristic (accessor calls, cursor/next
source, record-field signature, list walk); the top rows are dominated by large
unrelated functions (FUN_100a2370, 54 KB). Finding: the sweep CANNOT see the
real consumers, because they read the piece copy at record offset + 4
(piece+0x5C / +0x60 / +0x6C / +0x88 / +0x8C), not the node. The consumers were
found by following the per-tick chain instead (rows 6..12). Within the path
module the sweep lists FUN_10355100 and FUN_1037b480 (width-like reads); the
latter was read and is not a node consumer; FUN_10355100 not read. The two
earlier outputs `direction_readers_x86.csv.bak` (0 rows, comma separator split
by the batch launcher) and `.bak2` (loose scoring, 28224 rows) are superseded.

Note for the v15 RW watchpoint (node+0x74 rw): expected per-tick READS of the
cursor node's +0x74 by FUN_10351690 (the 0x90-byte record copy, called from
FUN_103ba8d0 / FUN_103ba8f0 inside FUN_1036b9b0) on every tick of every moving
unit, and by FUN_103ba8f0 at 0x103ba923 / 0x103ba92d / 0x103ba93c for corner
nodes. Nodes before the cursor are read the same way. These are the placement
reads, not noise.

## D3: waypoint handlers

0x101c3010 / 0x101c30e0 / 0x101c3170 are not handlers but 6-byte state-id
getters (`MOV EAX,[0x117979d0]; RET` etc.) in three 5-slot state vtables at
0x115c3a28.. (FIND_WAYPOINT: update 0x101c3070 -> FUN_101e9a00, transition
FUN_101c3090; MOVE_TO_WAYPOINT: 0x101c3180 -> FUN_101dc6a0; ARRIVED: 0x101c31b0
-> FUN_101f8280). The owner is state - 0x598. FIND_WAYPOINT hands entry
[owner+0x5A4] of the 12-byte list owner+0x5E8 to [owner+0x74]->vfunc +0x78.
This is a unit-level waypoint queue (multi-point orders), not the source of a
soldier's slot. CANDIDATE (x86 only; the x64 string anchors 1802a24c0 /
1802a2620 are likewise id getters and were not followed).

## D4: FUN_10397c30 (x64 FUN_1804D3A90)

`this` = the shared sub-object at node+0x24 (piece+0x1C): a set of navmesh
region rings. It locates a 2D point: walks each ring testing point-in-region
(FUN_11372520; x64 FUN_1819af350) first exactly, then with tolerance
DAT_115c897c, then with the caller's tolerance; then FUN_1037c010 builds a
20-byte location {x, y, z, region, region+0x80}. y comes from the region plane
(FUN_101b39d0) or the terrain height when no region contains the point. x and
z are never changed. It is called per soldier slot (row 12), per soldier in the
segment search (row 3) and for the target point (row 5): per call, not per
soldier state. CONFIRMED pair (known); the "x/z unchanged" fact is x86 only
(x64 builder FUN_1804b43e0 not read).

## D5: push-to-walkable step

None in the slot chain (rows 5..13): slot points are located, not clamped, and
a point outside every region gets a null region with its x/z kept. Whether
per-soldier steering towards an off-mesh or blocked slot collapses men onto the
centre line was not examined (outside the slot chain: FUN_101537d0 assignment
and the soldier movement code). CANDIDATE absence.

## Predictions for the v15 runs (from rows 10..11)

- E2 `set 2.0 all`: every row gets min(W_req, 2.0) = 2.0, so files = 1 if
  f0 > 0.001 (f2 about 2 m): the slots form a single-file column on open ground.
  E2 should show bunching / a column even on open ground.
- E3 `set 49.0 all`: min(47.0, 49.0) = 47.0 on open ground (no change); at the
  gate rows go 24 wide, slots off the mesh.
- E1 control: slots about 3 abreast in the 5.51 m segments. Abreast 1 in the
  run means the collapse is after slot generation.
Caveat: all of this is for formation type 0. Which type a standard infantry
unit uses was not resolved (FUN_1014e990 has 11 callers passing the type).
Slot 9 per type: 0 FUN_103c4720, 1 0x103c4780 (no function in s2x86),
2 FUN_103c4d70, 3 0x103c5d30, 4 FUN_103c6240, 5 FUN_103c63a0, 6 FUN_103c4cb0,
7 0x101e04a0 (types 3 and 6 are built on type 0's constructor but override
slot 9). Types 5 and 6 follow the same pattern with other generators
(FUN_103bd5d0, FUN_103bd560); FUN_103ce750 has 11 callers, so the files formula
is shared across formation code.

## Fix hook point candidates (prologues from dump_prologue.py)

| Function | Role | Stolen | Relocatable |
|----------|------|--------|-------------|
| FUN_103e0660 | row placer (type 0 generator slot 0): row width in, files out | 5: `PUSH ESI; MOV ESI,[ESP+8]` | yes |
| FUN_103ce750 | files-per-row formula (cdecl, 3 floats; 11 callers) | 7: `PUSH ECX; MOVSS XMM0,[ESP+8]` | yes |
| FUN_103e0140 | straight piece: builds rows with the node width | 6: `SUB ESP,0x30; PUSH ESI; MOV ESI,ECX` | yes |
| FUN_103e03a0 | corner piece rows | 7: `SUB ESP,0x50; MOV EAX,[ESP+0x54]` | yes |
| FUN_103b6190 | lateral offsets per soldier | 6: `SUB ESP,0x84` | yes |
| FUN_1036b9b0 | target / piece list build, per tick | 5: `SUB ESP,0x2C; PUSH EBX; PUSH EBP` | yes |
| FUN_10179b10 | formation call + slot assignment, per tick | 8 | yes |
| FUN_103c4720 | type 0 formation slot 9 | 8 | yes |
| FUN_101b7b70 | per-tick controller update | 9, contains `CALL rel32` | NO |

A vtable-slot swap (0x115e05d8 slot 0, 0x115e00d0 / 0x115e00f8 slot 6) is an
alternative to inline hooks for the generator and pieces.

## New pairs (not added to function_map_v3.md)

| x86 | x64 | Basis |
|-----|-----|-------|
| FUN_101b7b70 | FUN_18028e100 | caller of the FUN_101a9750 pair; same leader cursor update, same 0x2780/0x277c (x64 0x28a4/0x28a0) test, same leader byte and ready byte |
| FUN_10179b10 (+ FUN_101afcd0 inlined) | FUN_1802467d0 | formation vfunc slot 9 with (out, target, P width, count); same slot-assignment call and same rebuild of the soldier array from unit+0x518 / +0x5C8 |
| FUN_1036b980 | FUN_1804a1210 | same two calls |
| FUN_1036b9b0 | FUN_1804a1260 | target alloc with P width and 2; prev walk appending pieces from node+0x0C / +0x18 |
| FUN_10393530 | FUN_1804cec40 | lerp at cursor t between the two located endpoints |
| FUN_103c1b00 + FUN_103cc9f0 + FUN_103ba8f0 | FUN_180500520 | side test, two piece sizes, width/clearance clamp, +- perp * width * 0.5 |
| FUN_103e0140 | FUN_1805258f0 | straight piece slot +0x18 / +0x30; row width = node width start |
| FUN_103e0660 + FUN_103ce750 | FUN_180525ed0 | min(gen width, row width); same formula with 0.001 |
| FUN_1014e9e0 | FUN_180215390 | 8-way formation switch into unit+0x624 / +0x6D8 |
| FUN_103c4720 (+ FUN_103bd490) | FUN_180503950 | type 0 slot 9; generator fields |
| FUN_103e03a0 | FUN_180525b90 | same vtable slot of the corner piece (CANDIDATE: not compared in detail) |

## Ghidra project changes

s2x86: one function created at 0x101b7b70 (FUN_101b7b70) by
`tools/define_function.py`, saved. Nothing else written; all other scripts ran
with `-readOnly -noanalysis`. s2ce: unchanged.
