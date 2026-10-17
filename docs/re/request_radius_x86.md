# Soldier path request radius (x86): built, used, and what happens when it does not fit

2026-10-16, static, x86 only (`s2x86`, program `empire.retail.dll`, every run
`-readOnly -noanalysis`). Logs `logs/radius_a..g.log`. New decompiles in
`docs/walk_decompiles/x86/` (added only where no file existed). Listings in
`docs/listings/reqradius_*_x86.txt`. Every statement is STATIC (one build read)
unless it says MEASURED. Nothing was checked against x64 `s2ce`.

Naming: e = soldier entity (the object FUN_103df3f0 runs on, ECX = EBX there),
Q = request (0x48 bytes), R = path result, P = pathfinder =
[[e+0x28]+8]+0xB8, w = request+0x2C.

## 1. FUN_103dfc40 (15 bytes)

```
[5 lines of decompiled / disassembled game code removed from the public repository]
```

- It returns `2 * [[e+0x20C]+0x54] * [e+0x58]`, a diameter.
  - [e+0x20C] is the entity's type record. +0x54 is the base radius r0, and
    +0x20 of the same record is used for go-to records in FUN_103f2db0.
  - [e+0x58] is a per-entity scale.
- MEASURED (run 20261016_091335, v33 `soldiers.csv`): s_radius = [R+0x50] =
  1.2 on 97 % of the rows and 1.5 on the rest. So 2 * r0 * scale is 0.7 for
  infantry and 1.0 for the other kind.

Callers (`reqradius_callers_a_x86.txt`):

| Caller | Use |
|--------|-----|
| FUN_103ccd00 + 0x08 | the request builder (below), the only path use |
| FUN_1014da10 (x3), FUN_10129db0 (x3) | unit-level maxima while soldiers are added: `obj+0x3A8 = max(obj+0x3A8, FUN_103dfc40())` and `obj+0x3A4 = max(.., [s+0x58])`. These are not per request. |

## 2. Request builder FUN_103ccd00 -> FUN_1035fdf0

Replan call (FUN_103df3f0 state 1, 0x103df4b1..0x103df4cc), ECX = EBX = e:
`FUN_103ccd00(Q = ESP+0x58, &e+0x420 (dest), [e+0x448], vfunc_e+0x124(FUN_1038a850(e+0x2F0)==1), [e+0x450])`, RET 0x14.
FUN_103ccd00 calls FUN_103dfc40 and stores the result with FSTP [ESP+0x10]. It
then calls FUN_1035fdf0 (thiscall, ECX = Q, RET 0x24).

Request layout. FUN_1035fdf0 writes it. FUN_1035fd50 copies it to R+0x24, so
the R offset is the Q offset + 0x24.

| Q off | R off | Value | Source |
|-------|-------|-------|--------|
| +0x00 | +0x24 | agent class object; its +0x10 is the cache bucket index | [e+0x44] |
| +0x04..+0x14 | +0x28 | start {x, y, z, region, aux} | FUN_103df250 = e+0x524..+0x534 |
| +0x18..+0x28 | +0x3C | end {x, y, z, region, aux} | e+0x420..+0x430 |
| **+0x2C** | **+0x50** | **w = FUN_103dfc40() + 0.5** (`ADDSS XMM0,[0x115b183c]`, 0.5; stored at 0x1035fe82 `MOVSS [EDI+0x2c],XMM0`) | radius |
| +0x30 | +0x54 | mode: 0 = synchronous search, otherwise queued | [e+0x448] |
| +0x34 | +0x58 | byte | entity vfunc +0x40 |
| +0x35 | +0x59 | byte; when 1, +0x40 = clock + 4000 (0xFA8) | FUN_103c1f80 |
| +0x38 | +0x5C | layer / class | vfunc +0x124 result |
| +0x3C | +0x60 | byte 1 if start.aux or end.aux is set, or a point is outside the map box [P'+0x7C..+0x88] | FUN_1035fdf0 |
| +0x3D / +0x3E | | bytes, both set to 1 when FUN_1038a850(e+0x2F0) == 1 | FUN_103ccd00 |
| +0x40 | | 0, or the time stamp above | |
| +0x44 | | [e+0x450] (object, triggers a retry in the search) | |

- The soldier slot go-to FUN_103e88b0 -> FUN_103df290 passes record+0x14 = 0
  (-> e+0x448) and +0x1C = 0 (-> e+0x450). Soldier slot orders are therefore
  mode 0 (synchronous) with Q+0x44 = 0.
- The other users of the same builders are not soldiers:
  - FUN_10387190 calls FUN_103ccd00 for a target-reachability test
    (FUN_10382fd0). It never queries a path.
  - FUN_1038ad60 calls FUN_1035fdf0 directly with radius [obj+0x111C] and
    mode 2.

## 3. Query FUN_10385fd0 (ECX = P, arg Q)

1. FUN_10360340 finds the start and end regions for Q+0x04 / Q+0x18 on layer
   Q+0x38.
   - Either region is missing, disabled (region vfunc +8), or on an
     unconnected island (FUN_1039ec40 / FUN_1036b270): return 0
     (counter P+0x6AC).
2. Start region == end region: FUN_1035fca0 makes a direct result. It is
   ready at once (R+0x20/+0x21 = 0x101). The radius plays no part.
3. Cache FUN_103878d0 (ECX = START region).
   - The bucket is `[[start+0x64] + [[Q]+0x10]*4]`: a 10-entry ring per
     (start region, agent class).
   - An entry is a candidate when R+0x76 == 1 and R+0x70 == end region and
     FUN_103b2240(Q, R+0x24, 0) == 1.
   - Among the candidates it takes the one with the best score
     (FUN_103a3260 / (2 * distance)).
   - On a hit, FUN_1035fbf0 makes a NEW per-soldier R. Its +0x24 is a copy of
     the soldier's OWN Q, so R+0x50 is his own w. Its +0x0C is the cached
     corridor (FUN_10385d20). It is ready at once (0x101).
4. The cache inserts happen in FUN_1036bc50(start, R), called after every
   successful search. When the ring is full the oldest entry is dropped.

FUN_103b2240 (ECX = A = new request, B = stored request), listing at 0x103b2240:

| Check | Exact |
|-------|-------|
| equal | dword +0x00, +0x38, +0x40, +0x44; byte +0x34, +0x35, +0x3C, +0x3D, +0x3E |
| mode 0 (cache, own-path reuse) | `COMISS B.w, A.w; JC fail`: **A.w <= B.w**; then FUN_103b21c0 for start and end: \|dx\|, \|dy\|, \|dz\| each <= 50.0 (0x42480000, a box, not a distance) AND the same region (FUN_103b24a0) |
| mode 1 (failure list) | **B.w <= A.w** only, no position test |

- The own-path reuse FUN_10378c70 runs in state 1 before the query, unless
  FUN_1038bfb0 reports that the path was invalidated.
  - It needs the old R ready, successful and valid, the same end region, and
    FUN_103b2240(Q, oldR+0x24, 0).
  - A request whose w is larger than the stored one never reuses a path and
    never gets a cache hit.

Cache miss (still in FUN_10385fd0):
- Failure list FUN_10387860 (ECX = start region). The ring is at region+0x68
  (8 bytes per entry: Q copy, end region), count +0xB8, head +0xBC, tail
  +0xC0.
  - A hit is an entry with the same end region and FUN_103b2240(Q, failed, 1).
  - On a hit the query returns 0 at once (counter P+0x6A8).
- Q+0x30 == 0 runs a synchronous search FUN_10386d90(ECX = P+0x36C, Q,
  start, end).
  - Success: the portal list P+0x444 / count P+0x440 goes through FUN_1035ffb0
    into FUN_1035fca0 (ready 0x101), then FUN_1036bc50 (into the cache).
  - Failure: FUN_1036bb60(Q, end) stores Q in the failure ring, and the query
    returns 0 (counter P+0x6A0).
- Q+0x30 != 0 queues the request. FUN_1035fb40 makes R with +0x20 = 0 in the
  list P+0x148 / +0x14C / +0x150.
  - The per-frame pathfinder update FUN_103aac30 runs FUN_1039a500(R) on the
    queue. That function has the same order: cache, failure list, search.
  - It then sets `R+0x21 = result; R+0x20 = 1`.
  - FUN_103df3f0 state 2 waits for R+0x20. Soldier slot orders (mode 0) never
    enter state 2.

How w enters the search (A* FUN_10386d90 -> FUN_1034d1c0 -> FUN_1034c8e0;
neighbour expansion FUN_10384120, Q = [this+0xE4]):
1. Passability per portal: FUN_10371b60(portal, Q+0, Q+0x3D, Q+0x34,
   this+0x108, Q+0x38, **Q+0x2C**, &narrow), call at 0x10384293. Inside it,
   the width test is FUN_103b0b60(ECX = portal, layer, w) at 0x10371c07.
   - It fails when `w > portal+0x30` (`COMISS w,[ESI+0x30]; JA`): portal+0x30
     is the widest w that fits.
   - It also fails when either inset end point (vertex + m * w/2, with m the
     vertex offset vector at edge+0x24 / +0x28) is closer than w/2 to the
     adjoining wall segments (`d^2 < w^2 * 0.25`, 0x115c264c = 0.25).
   - Links (portal+0x08) and portals with +0x24 set always pass.
   - When the width test fails and &narrow is non-null, it sets `narrow = 1`
     and otherwise continues.
2. In FUN_10384120:
   `if (narrow == 1) { *flag = 1; if (this+0x110 == 0) skip; else penalty = 100000.0 (0x115de868) }`.
3. The crossing point on a plain portal comes from FUN_10393260(portal,
   min(w, portal+0x34)): the portal segment shrunk by 0.5 * that value at each
   end.
   - The point is the start->goal line intersection (FUN_1017f720) or the
     closest point to the goal.
   - The edge cost is distance + penalty (FUN_103661f0 adds node+0x18).
4. The heuristic (FUN_1038f160) is 1.5 * distance until this+0xC8 reaches
   the budget this+0x114 (75, or 250 for mode 2). After that it is
   distance^2. The search turns greedy there; the budget is not a failure.

Answer: w does reach the search. It sets which portals count as wide enough
(portal+0x30 plus the wall-clearance test), and it sets the crossing points
(the inset). Each portal is checked separately; there is no per-edge
clearance field besides portal+0x30.

## 4. When w does not fit, or there is no path

FUN_10386d90 retries:

```
[5 lines of decompiled / disassembled game code removed from the public repository]
```

- When only the width is wrong, the search does NOT fail. The second pass
  takes the narrow portals at a 100000 m cost and returns a normal full
  path.
  - Nothing falls back to a smaller w, and there are no partial paths.
  - The result keeps the soldier's w (R+0x50), so the funnel still offsets by
    w/2.
- A real failure gives a null R, from any of these:
  - the regions are missing, disabled or not connected;
  - a failure-list hit (same start region, end region, class and flags,
    failed w <= new w; the start position is ignored);
  - the open list runs empty after all passes.
- R+0x76 (valid) is set to 1 by all three result builders. What clears it was
  not read (open).

FUN_103df3f0 reaction:
- State 1: `R = FUN_10385fd0(Q); FUN_1039f9f0(R)` (follower e+0x3C0, R at
  e+0x3CC).
  - R == 0: state **7**.
  - R ready and +0x21 == 1: state 3 (follow).
  - R ready and +0x21 != 1: state 7.
  - R not ready: state 2.
- No state of FUN_103df3f0 leaves state 7 by itself. States 3..5 are handled
  only when the state is 3/4/5; for 7 the function goes to the end
  (LAB_103dfbb2).
- The go-to states FUN_103ed580 / FUN_103f2db0 treat 7 like 6 (arrived):
  `+0x18 = 1` / done. The soldier gets no waypoint and stops.
- He replans only when the next slot order arrives (FUN_101bc880 ->
  FUN_103dbbf0 -> FUN_103df290 sets state 1, about once a second, v10). This
  link is a CANDIDATE: the order rate is from v10, not re-measured.
- A failed Q also stays in the start region's failure ring (10 entries). For
  as long as it stays there, later requests to the same end region with equal
  flags and w >= the failed w fail at once. FUN_10382fd0 (target
  reachability) reads the same ring.

## 5. The radius in FUN_10394e80

- The only read is line 171 of the decompile:
  `[decompiled statement removed]`.
  - This is r = 0.5 * R+0x50 = r0 * scale + 0.25, passed to FUN_10393000 /
    FUN_103a0080 with clamp 0.
  - The stack argument [e+0x58] (W1 arg 4) is never read in the body. The
    decompile uses param_5 only in the signature.
- R+0x50 is the soldier's own w. FUN_1035fbf0 / FUN_1035fca0 / FUN_1035fb40
  all copy the caller's own Q, even on a cache hit.
- No unit or formation code writes R or Q.
  - The writes found are FUN_1035fdf0 (Q+0x2C) and FUN_1035fd50 (the copy).
  - The readers found are FUN_10384120, FUN_103b2240, FUN_103b0b60 (through
    the argument) and FUN_10394e80.
  - Per soldier, only [[e+0x20C]+0x54] (type) and [e+0x58] (scale) set it. A
    full sweep for other R+0x50 readers was not done.

## 6. Hook points for ONE soldier's next request

| Site | Bytes (stolen, relocatable) | Context at the patch | Notes |
|------|-----------------------------|----------------------|-------|
| **A. 0x103df4d1** in FUN_103df3f0, right after `CALL FUN_103ccd00` | `80 7C 24 0F 01` CMP byte [ESP+0xF],1 (5 bytes); next `74 1F` JZ at 0x103df4d6 | EBX = e; Q = ESP+0x58 (the same LEA at 0x103df4db / 0x103df4fa with no push between), so w = [ESP+0x58+0x2C] | Replan only. It runs before own-path reuse and before the query. The detour must run the CMP LAST (flags go to the JZ). RECOMMENDED. |
| B. 0x103ccd65 in FUN_103ccd00, after `CALL FUN_1035fdf0` (RET 0x24) | `8D 8F F0 02 00 00` LEA ECX,[EDI+0x2F0] (6 bytes), resume 0x103ccd6b | ESI = Q, EDI = e, caller return at [ESP+0x28] (stack_calc: sub 0x1C + 3 pushes) | Also hit by FUN_10387190 (return 0x103875dd); filter on [ESP+0x28] == 0x103df4d1. |
| C. 0x103ccd0d in FUN_103ccd00, after `CALL FUN_103dfc40` | `8B CF D9 5C 24 10` MOV ECX,EDI; FSTP [ESP+0x10] (6 bytes), resume 0x103ccd13 (CALL rel, not stolen) | ST0 = 2*r0*scale (before +0.5), EDI = e, RET [ESP+0x28] | Same filter as B; x87 state. |
| D. FUN_103dfc40 entry | `8B 81 0C 02 00 00 D9 40 54` (9 bytes; `dump_prologue` RELOCATABLE yes, no branch into it) | ECX = e, [ESP] = return | Also called by the unit maxima FUN_1014da10 / FUN_10129db0; filter [ESP] == 0x103ccd0d. Returns in ST0. |

Detour stack offsets: `tools/stack_calc.py --mid` (A: 0 caller pushes at the
hook point; locals as above).

## Risks of changing w per soldier

- Larger w:
  - It never reuses a path or gets a cache hit (A.w <= B.w), so a synchronous
    A* runs on every replan of that soldier (CPU).
  - The result goes into the start region's cache. Soldiers of the same class
    with a smaller w, within the 50 m box and with the same end region, then
    reuse that wider-w corridor. That corridor may avoid narrow portals or
    take a different route.
- Narrow passages (w > portal+0x30):
  - The first pass skips them. The second pass takes them at +100000 each, so
    the path may detour through wider portals far away when one exists.
  - When no detour exists, the path still goes through and does not fail.
- Funnel: the apex offset is w/2 * m with no clamp. With a large w the apex
  can leave the portal and land in or behind a wall, so the soldier steers
  into it (the v20/v21 wall-sticking kind).
- Smaller w gives tighter corners and easier cache reuse. It is safe for
  feasibility.
- Path failure (null R) means state 7: the soldier stands until the next slot
  order. The failed Q sits in the failure ring and blocks equal-flag requests
  with w >= it to the same end region from that start region. A w raised only
  for the "too narrow" case does not cause failures (the second pass), so the
  ring matters only for unreachable goals.
