# Waypoint choice FUN_10394e80: why a unit funnels through one corner point (x86)

Phase 11 Task D (static), 2026-10-14. Motivation: run `data/runs/20261014_203329`
(`analysis_v6.txt`): 55..62 of 96 infantry soldiers share the exact current
waypoint (164.6, 503.4) at a building corner while their slots stay 11.7 m
apart; a 20-man knot forms there. Inputs read: x86 decompiles in
`docs/walk_decompiles/x86/` (FUN_10394e80, FUN_10393000, FUN_103a0080,
FUN_103930e0, FUN_103a0180, FUN_103a0490, FUN_103a0500, FUN_1036f140,
FUN_103ccd00, FUN_1035fdf0, FUN_103dfc40, FUN_10385fd0, FUN_103878d0,
FUN_1035fbf0, FUN_1035fd50, FUN_103b2240, FUN_10385d20, FUN_1039ee90,
FUN_10396b10), x64 `docs/x64_decompiles/FUN_10394e80.c` (= FUN_1804d0620) and
`FUN_103df3f0.c` (= FUN_180524980); listings `docs/listings/phase11d_*.txt`;
logs `logs/phase11d_*.log`. Ghidra: read-only throughout, no project change.

## Answer

FUN_10394e80 is a funnel ("string pulling") walk over the navmesh portals of the
soldier's path. While the soldier sees its goal it returns the goal (the slot);
when the funnel closes on one side it returns that side's APEX, which is the
portal vertex moved along the portal edge by a fixed distance

    r = 0.5 * [R + 0x50] = [[e+0x20C]+0x54] * [e+0x58] + 0.25    (metres)

where R is the soldier's path result and e the soldier. r is the soldier's own
radius plus 0.25 m. It depends only on the soldier type and scale, not on the
soldier's place in the formation. Every soldier of a unit whose straight line
to its slot is blocked by the same corner therefore gets the same vertex and
the same r, so the same waypoint to the bit. That is the measured shared point.
There is no lateral, per-file or per-soldier term anywhere in the choice.
CONFIRMED (x64 FUN_1804d0620 has the same structure: r = [result+0x90] *
0.5, with DAT_182174b0c = 0.5 read from the file, the same two endpoint
helpers with clamp flag 0, and the same goal / apex returns).

## W1. FUN_10394e80 (x86 0x10394e80, 7307 bytes, RET 0x1C)

Call (only soldier caller): FUN_103df3f0 states 3 / 4 at 0x103df6bb.
`ECX = EDI = entity + 0x3C0` (the path FOLLOWER F, `LEA EDI,[EBX+0x3C0]` at
0x103df3ff); EBX = entity (the soldier: same object, see v10). Stack arguments:

| Arg | Value at the call | Meaning |
|-----|-------------------|---------|
| 1 param_2 | caller stack buffer (EAX after the call points to it) | out: waypoint {x, y, z, u16 heading at +0x0C, float at +0x10} |
| 2 param_3 | &local in the caller, zeroed first | out: the portal of the returned apex, 0 when the goal is returned |
| 3 param_4 | location from FUN_103df250 (e+0x524..+0x534) | soldier's current location {x, y, z, region, aux} |
| 4 param_5 | float [e+0x58] | the soldier's size scale (same factor as in the radius) |
| 5 param_6 | u16 [e+0x442] | heading for the goal case |
| 6 param_7 | byte (e+0x446 or e+0x444) | 1: keep the given heading at the goal |
| 7 param_8 | byte (FUN_10140150() == [e+0x2C]) | debug-draw switch (the FUN_100a1110 line drawing, 0.3 m up) |

Body:
1. Locates the soldier (FUN_103b2470 with [R+0x5C], the request's layer)
   and advances the follower cursor F+0x10 along the portal list to the
   portal of the soldier's region (F+0x10 / F+0x14 list nodes: +0x04 next,
   +0x08 portal).
2. `[decompiled statement removed]` (0.5). For each following portal the
   left and right endpoints come from FUN_10393000 / FUN_103a0080(portal, out,
   local_b0, 0, [R+0x5C]): edge E = [portal+0x20]; left = E.vertex(+0x0C,
   +0x10) + E.dir(+0x24, +0x28) * r; right = twin [E+8].vertex - E.dir * r.
   The 4th argument 0 skips the clamp to the edge length E+0x30, so r is not
   limited by the portal width. Portals with a special link (portal+0x10, e.g.
   a door or ladder object) return that link's point (link+0x28 -> +8 / +0xC /
   +0x10) directly.
3. Funnel test per side (FUN_103a0490 / FUN_103a0500: dot and cross product of
   the new edge against the current side direction). When a side crosses the
   other, the apex of that side (local_4c / local_48 left, local_44 / local_40
   right) is returned via FUN_1036f140 (height from the region plane, or the
   building surface), `[decompiled statement removed]`, F+0x14 = next node.
4. No crossing until the end: the goal is returned (FUN_10399f80 on the
   follower's goal, float [R+0x4C] at +0x10), F+0x18 = 1 (end reached).

Why identical for all men:
- Each soldier has its OWN path result. In the live run 96 soldiers carried
  96 distinct `s_path` pointers (+0x3CC), so this is measured.
- The query FUN_10385fd0 first looks up a 10-entry per-region cache
  (FUN_103878d0). It reuses a stored result when the end region matches
  (R+0x70), the result is valid (R+0x76), and FUN_103b2240 accepts the stored
  request: same layer, flags and class, new radius <= stored radius, start and
  end each within 50.0 m (0x42480000). On a hit, FUN_1035fbf0 builds a new
  per-soldier result whose +0x0C points at the CACHED corridor (FUN_10385d20 ->
  FUN_103892c0).
- So the men of a unit run the funnel over the same portal list (the shared
  corridor at [R+0x0C], CANDIDATE: not dumped yet), with the same r. The only
  per-soldier input is the start location, which decides WHEN the funnel
  closes, not WHERE the apex lies.
- The radius: FUN_103ccd00 (the request builder of replan state 1) passes
  FUN_103dfc40() = 2 * [[e+0x20C]+0x54] * [e+0x58] to FUN_1035fdf0, which
  stores it + 0.5 at request+0x2C. FUN_1035fd50 copies the request to R+0x24,
  so R+0x50 = request+0x2C. x86 read; the x64 offset of the same field is
  result+0x90.

Second caller FUN_10396b10 (another agent kind: radius [obj+0x111C] * 0.5,
not a soldier). An ENTRY hook on FUN_10394e80 would also see those calls.

## W2. Layout for a path dump (x86)

| Object | Field | Meaning |
|--------|-------|---------|
| follower F = e+0x3C0 | +0x0C | R, the soldier's path result (= e+0x3CC, already in v16 as s_path) |
| | +0x10 / +0x14 | current / saved portal-list node (node+0x04 next, node+0x08 portal) |
| | +0x18 | byte, goal reached |
| | +0x1C | special link taken |
| | +0x20..+0x3C, +0x40..+0x5C | two small vectors (4 inline + heap: count +0x30 / +0x50, heap count +0x38 / +0x58, heap pointer +0x3C / +0x5C) of portal side objects collected during the walk |
| result R | +0x0C | the corridor (shared via the cache) |
| | +0x24..+0x6B | copy of the request: +0x28 start {x, y, z, region, aux}, +0x3C end {x, y, z, region, aux}, +0x50 radius (2 * r0 * scale + 0.5), +0x5C layer / class |
| | +0x20 / +0x21 | ready flags (FUN_103df3f0 waits on them) |
| | +0x70 | end region key (cache), +0x76 valid |
| portal | +0x10 | special link, else 0 |
| | +0x14 | region (region+0x24 used for the height) |
| | +0x20 | half-edge E: left vertex +0x0C / +0x10 (x, z), direction +0x24 / +0x28, length +0x30; twin [E+8] gives the right vertex |

To dump a soldier's whole path: walk F+0x10 through node+0x04 and record per
portal the two raw vertices (E+0x0C/+0x10, [E+8]+0x0C/+0x10), E+0x30 and
portal+0x10. Also record R+0x50 and [R+0x0C]: one corridor pointer for all
soldiers would confirm the cache sharing. CANDIDATE: node and portal fields
are as used by FUN_10394e80; the list is not yet dumped live.

## W3. Fix designs (not built), x86

(a) Post-process the returned waypoint, per soldier. This is the least
invasive and fully x86.
- Patch point: 0x103df6c0, the instruction after `CALL 0x10394e80` at
  0x103df6bb: `CMP byte ptr [EBX+0x471],0x1` (80 BB 71 04 00 00 01, 7 bytes,
  no relative operand, so RELOCATABLE).
- No branch targets 0x103df6c0..0x103df6c6 (listing checked). The JNZ at
  0x103df6d9 uses this CMP's flags, so the detour must save and restore the
  flags (pushfd / popfd, then run the stolen CMP last, as the existing detours
  do).
- At the patch:
  - EAX = the waypoint buffer (copied right after, at 0x103df6c7..0x103df6d6,
    into e+0x454..+0x464);
  - EBX = soldier;
  - the apex portal (out-arg 2) is the caller local at ESP+0x14;
  - the buffer is at ESP+0x44 (EAX must equal it: a live self-check).
  - These two offsets were read from the push sequence 0x103df696..0x103df6b6
    and RET 0x1C. They are NOT yet generated with tools/stack_calc.py: generate
    and self-check them before use.
- Mid-function patch, so no return-address detour and no per-thread state.
  Only the soldier caller is affected; FUN_10396b10 is not.
- Handler, only when the apex portal != 0 (a corner, not the goal):
  1. Recompute both raw portal vertices from the portal's edge E to learn
     which side the apex is on and the portal direction and width (E+0x30).
  2. Move the waypoint along the portal to the soldier's own lateral position:
     t = clamp(dot(soldier position - vertex, edge direction), r, E+0x30 - r),
     where r = 0.5 * [R+0x50].
  3. Optionally use the soldier's slot lateral offset (from the order rows)
     instead of its position.
  4. Keep the result only if it is walkable: FUN_103b24a0(loc, 3), or the
     region test FUN_10397c30.
- Each man then keeps his lane through the corner instead of converging on
  the vertex.
- Fix candidate CONFIRMED structurally (the x64 caller copies the same 0x18
  bytes to e+0x620 and has the same apex-portal test).

(b) Per-soldier radius. Setting [R+0x50] per soldier, at the entry of
FUN_10394e80 or once after the result is stored at e+0x3CC, moves each apex
by a different r along the same portal.
- FUN_10394e80 entry: 6 stolen bytes `81 EC 6C 01 00 00`, RELOCATABLE
  (`docs/listings/phase11d_prologues_x86.txt`).
- Works with the clamp-free endpoints, but r must be kept below the portal
  width by hand, or the point leaves the portal.
- It changes the cache compatibility test (a larger stored radius lets more
  requests reuse the path), so it is riskier than (a).
- FUN_1035fdf0 (7 bytes, RELOCATABLE) and FUN_103ccd00 (5 bytes,
  RELOCATABLE) set the radius before the query. Inflating it there also
  changes path feasibility through narrow gaps; not recommended.

(c) No simpler lever. There is no corner-offset parameter in this code.
WAYPOINT_PATH_WIDTH is a registered float (FUN_1009f4e0 at 0x10017C63,
ECX = 0x1192BDE0, default 2.5), but its description is
"width_of_the_waypoint_arrow" (EntityDisplay.cpp). It is read only by the UI
functions FUN_102bf760 and FUN_102d0ac0. COLLISION_AVOIDANCE has no code
reference by address.

## W4. Config floats

FUN_10394e80 and its callees read no FUN_1009f4e0-registered value
(obj+0x5C). The terms are:
- per-entity data: [[e+0x20C]+0x54], [e+0x58];
- the shared .rdata constant 0x115B183C = 0.5, used by many functions
  (request radius, cache, FUN_10396b10), so not patchable safely;
- 0x115C2654 = 0.3 (debug-draw height);
- bss scratch (0x119336E8 / EC initial vectors, 0x11A4058C.. work arrays).

Patching the unit record radius ([e+0x20C]+0x54) would move every man's
apex by the same amount and change collision and pathing: no spread.

## Pairs

| x86 | x64 | Status |
|-----|-----|--------|
| FUN_10394e80 | FUN_1804d0620 | CONFIRMED (radius * 0.5, endpoint helpers with clamp 0, goal / apex returns) |
| FUN_10393000 / FUN_103a0080 | FUN_1804ce390 / FUN_1804de330 | CONFIRMED at the call sites (same arguments: portal, out, r, 0, [result+layer]) |
| FUN_103df3f0 call at 0x103df6bb | FUN_180524980 line 179 | CONFIRMED (follower e+0x3C0 / e+0x538, waypoint e+0x454 / e+0x620) |
| FUN_103dfc40, FUN_1035fdf0, FUN_103878d0, FUN_103b2240, FUN_1035fbf0 | not read | x86 only |

## Correction 2026-10-14 (run 20261014_221943; the text above is left as written)

W1 step 2 and W2's "direction +0x24 / +0x28, length +0x30" are WRONG. Live and
re-read (`docs/ghidra_analysis_v11.md` F2): the endpoint helpers branch on the
side object's type at +0x08. For type 0, left = E.pos + E.m * r and right =
[E+8].pos + [E+8].m * r, where +0x24 / +0x28 of each vertex record is a
per-vertex offset (miter) vector, |m| = 1 / sin(half the corner angle), not
the edge direction; the right side uses the other record's own m. E+0x30 is
0.0 live and is only a clamp limit. The apex therefore sits r away from both
walls of the corner, off the A-B line. W3 (a) stands with the lane redefined
as the shrunk portal pA -> pB.
