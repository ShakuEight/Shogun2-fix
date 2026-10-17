# Per-tick steering, soft collision and integration (x86, Phase 13)

Static read of the original Empire.Retail.dll (project `s2x86`), 2026-10-15.
x86 only: no x64 pairs were made for these functions, so every row is a
single-build reading (STATIC), not CONFIRMED in the function-map sense.
Decompiles: `docs/walk_decompiles/x86/<name>.c`. Logs `logs/phase13a..j.log`.
Field sweeps: `data/phase13_*.csv` from the new `tools/sweep_fieldsets.py`
(per function and base register: which of a set of [REG+OFF] fields are
read, written or address-taken).

All offsets are on the soldier entity e (= soldier s; [s+0x2F0] is s itself,
v16). The tick is 0.1 s (DAT_115b2448 = 0.1, DAT_115b2454 = 10.0).

## Chain

| Step | Function | What it does |
|---|---|---|
| 1 | FUN_103df3f0 (replan, states 3/4) | after FUN_10394e80 picked the waypoint e+0x454, calls FUN_103942f0 with ECX = e, &e+0x454, speed e+0x438, flags e+0x468 (listing `phase11d_wp_x86.txt` 0x103df577 / 0x103df933) |
| 2 | FUN_103942f0 (set steering target) | copies the 5-dword location to e+0x16C..+0x17C, speed * FUN_100a0b60() to e+0x110, flags e+0x118, steering state e+0x10C = 0; clears e+0x144 / e+0x1C0 when the target changed (FUN_10361a50 compare). FUN_103a5f00 is the matching stop (state 2). |
| 3 | FUN_103aa090 (desired move, vtable method; callers FUN_103dda00, FUN_103f59e0) | target point = FUN_10399f80(&e+0x16C) stored at e+0x124 / +0x128; direction and heading change vs e+0x64; speed with slope (FUN_1017c0a0) and turn limits; writes the desired displacement e+0xD8 / e+0xE0 = dir * speed * 0.1 (on a path follower e+0x18: along its segment). With e+0x11E set it projects the move onto a line (FUN_1008dac0). |
| 4 | FUN_101b2be0 (soft-collision manager) -> FUN_103ac9c0 (per object) | pair pushes, below |
| 5 | FUN_10367600 (integrator, vtable method; callers FUN_10366a10, FUN_103c1d90) | applies pushes, crowd damping, hard collision, moves e+0x48 / +0x50, clears the accumulators |
| side | FUN_10374580 (+ FUN_10374d60, FUN_10375310) | clip the displacement against one wall segment (stop or slide along it) |
| side | FUN_103a6650 | "is my steering target inside entity B" using B's radius +0x58 and angular shape table +0x60 |

## Soft collision, FUN_103ac9c0 (STATIC)

The soft-collision object is embedded at e+0x30, and the grid stores
pointers to it, so a neighbour's field X is e'+0x30+X. For this object A
and each neighbour B in the touched grid cells (each pair once per tick):

- Collide only if the flag masks agree: same group (unit) needs bit 1 of
  A's flags e+0x38, different group bit 2, no group bit 4 (bit 7 overrides).
  Height overlap within e+0x54 / B+0x24.
- R = rA + rB (radius e+0x58), or per-direction radii from the shape tables
  e+0x60 (FUN_103ac8a0, indexed by the bearing relative to heading e+0x64).
- d = B.pos - A.pos; if |d|^2 < R^2: s = R^2 / |d|^2 - 1, k = A[e+0x6C] *
  B[e+0x68] (1 if B is pinned, byte e+0x66), with facing multipliers for
  different groups (charge / brace flags e+0x5C..+0x5E).
- A.push (e+0x70 / +0x74) -= min(k,1) * s * d; B.push += s / max(k,1) * d.
  Gross magnitudes |.| are summed into e+0x78 / +0x7C on both.
- Different groups, closing on each other: velocities e+0x80 / +0x84 are
  replaced by the mass-weighted mean (impact).

The push grows as 1/dist^2: at half the contact distance s = 3, i.e. 1.5 R.

## Integrator, FUN_10367600 (STATIC)

Per tick, normal ground branch (e+0xA3 == 0, e+0xA4 == 0, e+0xA2 == 0):

1. displacement D (e+0xD8 / +0xE0) += push * 0.1.
2. Crowd damping: if |gross| / |push| > 3.0 (DAT_115c26d4) and the byte
   e+0xD6 is 1 (set to 1 at the end of every tick), D is scaled by
   max(3 / ratio, 0.1) and e+0xB8 = 1. A man pushed from all sides (large
   gross, small net) keeps only 10..100 % of his own move.
3. D += e+0x6C * 0.1 * 0.7 * (last tick's intended - actual move), a
   catch-up term.
4. |D| is capped at 2 m per tick.
5. Hard collision: FUN_10384570 collects obstacles in the swept box, and
   FUN_1039a9a0 resolves D against them.
6. Position e+0x48 / +0x4C / +0x50 += D; height from the ground; then
   e+0x70..+0x7C (push, gross) and D are cleared.

So a soldier's motion per tick is: straight at his own steering target
(the waypoint), plus 1/dist^2 separation from overlapping men of his own
unit, minus crowd damping, clipped by walls. Nothing in this chain knows
the formation, the slot neighbours or the unit corridor; there is no
lateral "spread out" term other than the separation push.

## Consequences for the fix

- Once waypoints are spread (v20), what keeps men in a column is (a) the
  geometry of their straight lines to those waypoints and (b) the passage
  width; soft collision only spaces them at about the contact distance
  (measured nearest-neighbour 0.61..0.66 m in run 23:20).
- Crowd damping makes a dense knot slow to dissolve (men in the middle move
  at down to 10 %), which explains long-lived knots where waypoints are
  shared (fix off) but is not a cause of the narrow column by itself.
- Candidate levers, none tested: the separation radius e+0x58 (wider
  spacing = wider column for the same men); the damping threshold 3.0 and
  floor 0.1 (shared constants, would need a mid-function patch, not a
  global edit); a lateral term added to D in FUN_103aa090's output for men
  whose waypoint is a funnel apex.

## Tools added

- `tools/sweep_fieldsets.py` (Ghidra headless): functions that touch a set
  of [REG+OFF] fields through one base register; LEA counts as "a".
- Runner used: `launch.bat fg jdk Ghidra-Headless 12G ... AnalyzeHeadless
  E:\_SHOGUN\GHIDRA s2x86 -process empire.retail.dll -noanalysis -readOnly
  -scriptPath E:\_SHOGUN\Shogun2_fixes\tools -postScript <script> <args>` (the
  program name in s2x86 is lower case).
