# pathfix: history and working notes

Moved here from the root CLAUDE.md on 2026-10-16 (second restructure), verbatim. Paths in the
notes below describe the layout at the time they were written: `src/`, `tools/<script>`, `sim/`,
`docs/<file>`, `data/runs/`, `archive/` and `build_vNN` now live under `fixes/pathfix/` or
`docs/re/`; `fixes/pathfix/archive/MOVES.csv` (first restructure) and `MOVES_2.csv` (second)
map every old path to its new one. Shared rules, paths and engine knowledge stay in the root
CLAUDE.md.

Since 2026-10-17 `archive/`, `analysis/`, `sim/` and `docs/research/` are local only (ignored
by git, absent from the public history, which is one squashed commit). GitHub holds the current dev
build (v42 since 2026-10-17), the public release source (1.0.1), the harness, the runbook and the READMEs.

## Original introduction

Reverse-engineering and DLL-proxy project to fix the corner-funneling bug in
Total War: Shogun 2 battles: soldiers collapse into a single-file line through
chokepoints instead of keeping formation width.

This file is the bridge from the claude.ai "Shogun2 mod" Project into Claude
Code. The full docs live in that Project (ghidra_analysis_v5.md,
SHOGUN2_PATHFINDING_PROJECT.md, PHASE6_v2.md, hooks.cpp, analyze_capture.py,
CE Lua scripts). Keep local copies of the ones you need in `docs/`.

## Root cause (confirmed, ghidra_analysis_v5.1)

Formation width is plumbed end to end but never consumed by movement code.

| Location        | Value          | Set by        | Read by          |
|-----------------|----------------|---------------|------------------|
| moveOrder+0x60  | width (float)  | FUN_102557e0  | serializer only  |
| unit[0x542]     | width * 0.5    | FUN_1035ae20  | nothing          |
| pathObj+0x38    | max(halfWidth) | FUN_1041c5d0  | nothing          |

Pipeline: FUN_101f6bf0 (move cmd parser) -> FUN_102557e0 (move order) ->
FUN_1010bb50 (unit cmd processor) -> FUN_1035ae20 (unit setup, once) ->
FUN_1037a850 (collision setup, once) -> FUN_1042ea90 (center-line path
sampler, no width). Per tick: FUN_101B7B70 -> FUN_10366E40 (segment search)
-> FUN_10369940 -> FUN_103B24A0 -> FUN_10399F80 (position write).

Path object (0x848 bytes): +0x38 max half-width, +0xB8/+0xBC entity ID
count/array, +0x7E4 ptr to move order, +0x7E8..0x7FC path start/end xyz.
Unit struct (0x16D4 bytes): [0x542] half-width, [0x53C]/[0x53D] entity
count/array.

## Open options (v11 of the DLL reached here)

1. Hook higher: FUN_10366E40 (path segment search), inject lateral offset
   before sampling.
2. Hook the consumer of the temp buffers that FUN_10399F80 fills.
3. Direct write to validated entity addresses captured by the hook (not by
   scanning).
4. Complete Edition (x64 rebuild by MATABOO/CA): import into Ghidra, use
   unwind tables for clean function bounds, Diaphora/BinDiff against the
   annotated x86 project to transfer labels; first test whether the funneling
   bug still reproduces there. If fixed in CE, diff the matched path-sampling
   functions and backport.

## First restructure (2026-10-16), as written then

The project is now a git repository for several game fixes in one dinput8.dll.
v40 is the main build. Everything below this section that names `src/`,
`tools/<script>`, `sim/`, `docs/<file>` or `build_vNN` describes the layout
BEFORE the restructure; `archive/MOVES.csv` maps every old path to its new one.

- `README.md` what the build does and how; `ARCHITECTURE.md` the journey.
- `proxy/` dinput8 proxy core + fix registry (`fixes.cpp`: each fix exposes
  `<Name>_Install` / `<Name>_Remove`, compiled in by CMake option `FIX_<NAME>`).
- `fixes/pathfix/src/pathfix.cpp` = the former `src/hooks_v40.cpp`
  (entry points renamed to `Pathfix_Install` / `Pathfix_Remove`), plus
  `stack_offsets_v16/v17/v34.h`. Harness `fixes/pathfix/tests/hooktest.cpp`
  (former `tools/hooktest40.cpp`), runner `tests/run_tests.py`.
- `fixes/pathfix/analysis/` live run analysis (v40_*, v39_ov_pass, plots,
  formup, tip_pivot ...); select a run with env PATHFIX_RUN / PATHFIX_UNIT /
  PATHFIX_CTRL. `fixes/pathfix/sim/` the crowd model (run from the repo root,
  paths `fixes/pathfix/sim/...`). `fixes/pathfix/docs/runbook.md` (former
  capture_v40_runbook.md), `docs/research/` the research log (former docs/
  ghidra_analysis_v*.md, PHASE*.md, capture_v*_*.md/txt).
- `docs/re/` game-binary reference (function maps, decompiles, listings,
  prologues, functions_x86.csv, *_x86.md decodes).
- `tools/ghidra/` headless scripts + `headless_decompile.bat`, `ghx86.bat`
  (-scriptPath is now `E:\_SHOGUN\Shogun2_fixes\tools\ghidra`); `tools/re/`
  stack_calc.py etc.; `tools/runs/collect_runs.py`.
- `archive/` v12..v39 sources, harnesses, build scripts, generators, build
  folders, old installed DLLs (from the game folder), backups.
- Build: `cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"` -> `build\dinput8.dll`.
  Test: `cmd /c "E:\_SHOGUN\Shogun2_fixes\test.bat"` (full 122/122 x2, geo 32/32,
  crash dump). Git replaces the `.bak` rule for tracked files; untracked
  files still follow it.

## Status 2026-10-13

CE battle module is `empire.retail.x64.dll`, analysed in Ghidra project
`s2ce` (`E:\_SHOGUN\GHIDRA`). The old `Shogun2` project turned out to be
unanalysed (381 functions, no xrefs), so the original x86 DLL was re-imported
and fully analysed into a new project `s2x86` (same image base, same FUN_
names); `Shogun2` is untouched. `docs/function_map_v1.md` maps the pipeline
x86 -> x64 with 10 CONFIRMED pairs (both decompiles agree on a source-level
fact): FUN_101f6bf0 -> 1802E61F0, FUN_1035ae20 -> 18048EB20, FUN_1041c5d0 ->
18056FA50, FUN_1037a850 -> 1804B2050, FUN_1042ea90 -> 180581F50,
FUN_10366e40 -> 18049B660, FUN_10369940 -> 18049ECA0, FUN_103b24a0 ->
1804F2230, FUN_10399f80 -> 1804D6CB0, FUN_1036e710 -> 1804A43A0; x64
decompiles are in `docs/x64_decompiles/<x86name>.c`. In x64 the segment
search has FUN_10399f80 / FUN_10369940 / FUN_103b24a0 inlined, and
FUN_10399f80 has FUN_1036e710 inlined. Not done: FUN_101B7B70 (vtable-only,
not a function in either fresh analysis) and the consumer of the temp buffer.
Everything else in the map, and all of `docs/walk_pairs_all.csv`, is
CANDIDATE only; the automatic call-graph walk mispaired small helpers twice
and must not be trusted without reading the decompile. Corrections to the
anchor table above: the parser string has no spaces after the commas
(`(location,angle,width,move_fast)`), the angle constant is the literal
0x43360B60, and `NAVAL_UNIT_RECORD` only occurs as a substring. Tooling
notes: `analyzeHeadless.bat` lives in
`ghidra_12.0.3_PUBLIC_20260210\ghidra_12.0.3_PUBLIC\support\` and hard-codes
a 2 GB heap, so call `launch.bat` directly with a larger one; the batch
launcher splits arguments on `=`; all scripts in `tools/` run with
`-readOnly -noanalysis`.

Phase 7 (2026-10-13, later the same day): capture build v12 is built
(`src/hooks_v12.cpp`, `CMakeLists.txt`, `build_v12.bat`, output
`build_v12/dinput8.dll`), passed an offline injection test against the mapped
real DLL, and is installed in the game folder (previous build kept there as
`dinput8.dll.bak`). The live capture has NOT been run yet; steps are in
`docs/capture_v12_runbook.md`, format in `docs/capture_format_v12.md`, parser
is `tools/analyze_capture_v2.py`. `stack_calc.py` did not exist on this
machine and was written new in `tools/`. Results so far are in
`docs/ghidra_analysis_v6.md`: FUN_101a9750 -> 18027C820 and FUN_10179d20 ->
180246A60 are now CONFIRMED (`docs/function_map_v2.md`, 14 confirmed pairs);
the sampler is reached as FUN_1037a850 -> FUN_10409e10 -> FUN_1040a4a0 ->
FUN_1042ea90, and FUN_1040a4a0 copies the floats at +0x10..+0x30 into a new
0x54-byte record first, so the hypothesis sampler.this == pathObj + 0x7D8 is
predicted (not measured) to be false; FUN_101B7B70 is real code reached only
through the vtable slot at 0x115c1744 but is not a function in s2x86. Task 3
(half-width reader sweep) was not run.

Capture v12 (2026-10-14): run once (24-man unit each side; deploy 4 ranks,
plain click, drag 2 ranks, drag 1 rank). Files in `data/capture_v12/`, report
`docs/capture_v12_analysis.txt`, outcome in `docs/ghidra_analysis_v6.md`:
decision table "neither". FUN_1041c5d0 (Hook A) was called 0 times in the
whole window, and FUN_1042ea90 (Hook B) was called 2737 times inside 1.44 s at
window open (callers FUN_1040a4a0, FUN_1040a550, FUN_1040a410, segments
spread over the whole map) and never again during the three orders. Neither
function runs per move command in this setup, so the v5 per-command pipeline
(FUN_1035ae20 -> FUN_1037a850 -> sampler, and FUN_1041c5d0 setting
pathObj+0x38) is not what executes when infantry is ordered to move.

Correction 2026-10-14 (Phase 8; the text above is left as written): the
"Root cause" table and pipeline are UNVERIFIED for mouse orders. The
pathObj+0x38 row does NOT execute in land battles (FUN_1041c5d0 measured 0
calls, capture v12). FUN_1042ea90 is a battle-load pass over static map
polylines, not a per-order path sampler (`docs/function_map_v2.md` relabelled).
FUN_1035ae20 is not per order. What is measured is the per-tick chain:
FUN_101a9750 copies the cursor (node pointer, float progress) from
[this+0x18]+0xA8 and calls FUN_10366e40 on a stack copy. Static x86 node
layout (`docs/path_node_layout.md`): next +0x08, sub-object +0x24, 2D endpoints
+0x80 / +0x88 (x64 +0x10 / +0x30 / +0xC4 / +0xCC). Capture v13 (order trace:
which functions fire on a mouse order, node pointer timeline, node dumps) is
built and installed but not yet run; see `docs/ghidra_analysis_v7.md` and
`docs/capture_v13_runbook.md`. In the game folder the v12 DLL is now
`dinput8.dll.v12.bak`.

Phase 8 addendum (2026-10-14): the Cheat Engine step is replaced by a hardware
write watchpoint inside the same v13 DLL (`docs/PHASE8_addendum_watchpoint.md`):
Dr0..Dr3 armed on every thread at each unit's P+0xA8 (field configurable in
`pathfix_watch.txt` next to the game exe, no rebuild), a vectored exception
handler records writer EIP, registers and stack candidates into
`pathfix_capture_v13_watch.csv`. Tested offline; installed (previous v13 as
`dinput8.dll.v13pre.bak`). One live run now covers Task 1 and Task 2. Do not
attach Cheat Engine or a debugger during a capture: they share the debug
registers. Writer and stack addresses are named from `docs/functions_x86.csv`
(`tools/dump_functions.py`, one row per contiguous function range).

Capture v13 result (2026-10-14 08:41 run; `docs/ghidra_analysis_v7.md`,
`docs/capture_v13_analysis.txt`, files in `data/capture_v13/`): per mouse order
only FUN_102557e0 (move order constructor, main thread) of the v5 chain fires;
FUN_1010bb50 fires only at battle start, the UI width/angle functions never.
16..140 ms later, on a worker thread, the cursor P+0xA8 is zeroed (helper at
0x1008EF30) and set to the new node by FUN_1039bb40 at 0x1039c440 (the path
builder). Path nodes carry a full corridor WIDTH at +0x74 (segment start) /
+0x78 (end): FUN_1039bb40 builds left/right edges as point +- perpendicular
(+0x64 / +0x6C) * width * 0.5. Player unit values 12.38 / 24.62 / 30.75 on
orders 1 / 2 / 4 (ratio o2/o1 1.99), enemy 15.0; whether this is the formation
frontage or a terrain width is not yet settled (the ranks used per order were
not reported). Nodes and cursor blocks are pooled, so addresses recur.

Ranks confirmed later the same day (4 / 2 / 1 / 1 ranks, 6 / 12 / 24 / 24
files, order 4 through the gap): node +0x74 scales with formation frontage on
open-ground orders (12.384 at 6 files, 24.615 at 12 files, about 2.04 m per
file), but the gap order at 24 files holds 30.749 instead of the about 49 m
that line predicts. Either a terrain/passage clamp or a fixed cap near 30.75
limited it; the 1-rank open-ground order (o3) was rebuilt in place at a reused
node address and not dumped, so this run cannot tell which.

Phase 9, 2026-10-14 (`docs/ghidra_analysis_v8.md`): the path node is 0xA0
bytes (links +0x00 P, +0x04 prev, +0x08 next, then a 0x90-byte corridor record
at +0x0C, flag byte +0x9C); the v13 0x100 dumps carried the neighbour block.
Node creator FUN_103592a0, record built by FUN_103baac0: width at +0x74 / +0x78,
clearance +0x7C, and the points the per-tick search follows (+0x80 / +0x88)
are shifted sideways by (width - clearance) / 2 on corner nodes, so the width
does reach the per-tick chain at corners. Plain-node width = length of a
two-point segment from FUN_103703b0 (in FUN_1039add0). Order width = stack
argument 4 of FUN_102557e0 (stored at moveOrder+0x60). Path builder pair
FUN_1039bb40 -> x64 1804D8D00 CONFIRMED (`docs/function_map_v3.md`; v2 has 12
confirmed rows, not 14 as written above). Capture DLL v14 (builder entry hook
with whole-list walk, eight constructor arguments, optional width override via
`pathfix_watch.txt`-style `pathfix_override.txt`, off by default) is built,
offline-tested and installed (v13 kept as `dinput8.dll.v13.bak`); the live run
(`docs/capture_v14_runbook.md`) and the decision table are pending.

Phase 9 run result (2026-10-14 10:15; `data/capture_v14/`,
`docs/capture_v14_analysis.txt`, v8 "Decision table"): the order carries the
formation frontage (FUN_102557e0 arg4: 47.0 at 24 files, 11.18..11.53 at 6
files); click orders do not call the constructor and reuse the unit's current
width. The builder clamps per segment: node width = min(requested width,
local passage width). The test gap measures 5.51 m at both 24 and 6 files;
open segments keep the requested width (47.0). v13's 30.75 was a clamped first
segment, not a cap. Re-paths without a new order run about every 0.5 s per
moving unit, often on the main thread. Width-dependent corner offsets occur on
captured corner nodes. Next: Task E override run (does the corridor width
reach soldier placement; 5.51 m fits about 3 files).

Phase 10, 2026-10-14 (`docs/ghidra_analysis_v9.md`, plan
`docs/PHASE10_placement_plan.md`): correction to the Phase 9 text above, the
order width (FUN_102557e0 arg4) is the DRAG LENGTH capped at the unit's
maximum (47.0 for the test unit), not a fixed frontage. The gap is a gate
about four men wide (user); the engine's 5.51 m corridor there is roughly
honest. FUN_101a9750's `this` is not one per unit (9 for 2 units in v14).
Static Task D (`docs/placement_chain_x86.md`): the corridor width DOES reach
soldier placement every tick, via a copy. FUN_101b7b70 (now a function in
s2x86; x64 FUN_18028e100) -> FUN_101a9750 -> FUN_1036b9b0 walks the path
backwards from the cursor node and copies each node record into a piece
(FUN_10351690). The formation object [unit+0x624] (type 0: slot 9
FUN_103c4720) lays out rows along the pieces, and the row placer FUN_103e0660
uses files = round((min(P+0x6C, node width) + 0.001 - f0) / f2) + 1
(FUN_103ce750). At the gate that is about 3 abreast (formation type 0
assumed). There is no push-to-walkable step in the slot chain. Slot assignment
(FUN_101537d0) and steering were not read. Capture DLL v15 (`src/hooks_v15.cpp`,
`build_v15/`, format `docs/capture_format_v15.md`) is built, offline-tested
(`tools/hooktest15.cpp`) and installed (v14 kept as `dinput8.dll.v14.bak`). It
adds a read-or-write watchpoint on the followed node's +0x74 that follows node
changes, and `pathfix_override.txt` = `set <v> [all|player]`. Analyzer
`tools/analyze_capture_v5.py` adds [10] readers and [11] override effect.
Runs E1 / E2 / E3 (`docs/capture_v15_runbook.md`) are NOT yet run. Predicted
results: E1 per-tick reads by FUN_10351690; E2 (2.0 m) a single-file column
even on open ground. If E1 shows 1 abreast at the gate, the collapse is after
slot generation.

Phase 10 runs, 2026-10-14 (`docs/ghidra_analysis_v9.md` Task C and decision
table; files `data/capture_v15/E1`, `E2`, `E3`; reports
`docs/capture_v15_E1_analysis.txt`, `_E3_analysis.txt`): the target bug is
SINGLE FILE WHEN A UNIT ROUNDS A CORNER (cavalry and infantry alike, also at
narrow or multi-bend gates; the test unit "general" is cavalry; the v14/v15
gate is passed about 4 abreast and is a weak repro). E1: the only per-tick
readers of node+0x74 are FUN_10351690 + 0x53 and FUN_103ba8f0 + 0x38, as
predicted. E3 (`set 49.0 all`): the readers saw 49.0 for the whole run and
nothing visible changed; E1's corner order on plain nodes 7.33..27.24 m wide
still went single file. The corridor width is NOT the lever. E2
(`set 2.0 all`) crashed at battle start (AV at FUN_1036e710 + 4, invalid
[location+0x10] + 0x44; no CSVs, no dump). Static
(`docs/soldier_steering_x86.md`): about once a second FUN_101bc880 sends
soldier i to slot i (FUN_103dbbf0), and each soldier entity plans its own
navmesh path to its slot (FUN_103df3f0 -> FUN_10385fd0, waypoints
FUN_10394e80), never seeing the unit corridor; shortest paths round a convex
corner all pass the inner vertex (CANDIDATE mechanism). User rule: tests must
need no manual file copying, moving or editing. Next: `docs/PHASE11_steering_plan.md`
(capture v16: per-run folders, crash dump, hotkey presets, soldier
destination / waypoint / position trace). Not started.

Phase 11, 2026-10-14 (`docs/ghidra_analysis_v10.md`, plan
`docs/PHASE11_steering_plan.md`): capture DLL v16 (`src/hooks_v16.cpp`,
`build_v16/`, format `docs/capture_format_v16.md`) is built, offline-tested
(`tools/hooktest16.cpp`: full run and a deliberate crash at the E2 fault site
FUN_1036e710 + 4) and installed (v15 kept as `dinput8.dll.v15.bak`; the older
`.bak` files were no longer in the game folder). From v16 on, each launch
writes into its own folder `<game>\pathfix_runs\<stamp>\` (`src/log.cpp` with
`PATHFIX_RUN_FOLDERS`, set by CMake for v16+), files are appended every 10 s,
a crash writes `crash.dmp` plus a CRASH record, and F6 / F7 switch the width
override preset / watchpoint, so the user never copies or edits files;
Claude collects runs with `tools/collect_runs.py` into `data/runs/`. v16 adds
the soldier trace: FUN_101bc880 (per-soldier orders, unit = [ctrl+4], soldiers
[unit+0x1C]), FUN_103dbbf0 (go-to, 0x2C-byte order with the slot location),
FUN_103dafb0 (slot off the navmesh), and per-unit samples of each soldier's
position (+0x48 / +0x50), destination (+0x420 / +0x428) and current waypoint
(+0x454 / +0x45C) on s and on [s+0x2F0] (probably s itself; the analyzer
decides per unit). `tools/analyze_capture_v6.py` reports per corner episode
whether destinations, waypoints or only positions collapse (tested on
`tools/make_synthetic_v16.py`). The run (`docs/capture_v16_runbook.md`) is
NOT done yet.

Phase 11 run, 2026-10-14 20:33 (`data/runs/20261014_203329/`, report
`analysis_v6.txt` there, v10 Task C and decision table): CONFIRMED live that
the corner funnel is per-soldier path convergence. Soldiers walk straight at
their slots (current waypoint +0x454 = slot +0x420) until the slots move round
a building; then 55..62 of 96 infantry soldiers get the IDENTICAL intermediate
waypoint, the building corner (164.6, 503.4), while their slots stay 11.7 m
apart, and a knot of up to 20 men within 1 m forms there (user screenshot
`data/capture_v16/Capture.JPG`). The cavalry general shows the same. The slot
layout is not the problem. [s+0x2F0] is the soldier itself. The first v16
build skipped soldiers above 2 GB (Shogun2.exe is large-address-aware; 20
percent of rows); fixed and reinstalled (first build kept as
`dinput8.dll.v16build1.bak`). Next: how FUN_10394e80 picks that shared corner
waypoint (`docs/waypoint_choice_x86.md`) and a per-soldier corner offset fix.

Phase 12, 2026-10-14 (`docs/ghidra_analysis_v11.md`, plan
`docs/PHASE12_fix_plan.md`): first fix prototype, DLL v17 (`src/hooks_v17.cpp`,
`build_v17/`, format `docs/capture_format_v17.md`), built, offline-tested
(`tools/hooktest17.cpp` 31/31 plus the crash path) and installed (v16 kept as
`dinput8.dll.v16.bak`). A mid-function patch at 0x103df6c0 (FUN_103df3f0, after
CALL FUN_10394e80; stolen `CMP byte ptr [EBX+0x471],1`) sees EAX = waypoint
buffer = ESP+0x44, [ESP+0x14] = apex portal, EBX = soldier
(`src/stack_offsets_v17.h` from `tools/stack_calc.py --mid`, new mode). On a
corner apex it slides x / z along the same portal edge (E = [portal+0x20]:
A E+0x0C/+0x10, d E+0x24/+0x28, len E+0x30, B [E+8]+0x0C/+0x10; r = 0.5 *
[[s+0x3CC]+0x50]) to the soldier's lane: mode 1 slot lane (slot offset across
the unit, compressed to len - 2r), mode 2 own position; clamped to
[r, len - r]. OFF at launch; F6 cycles 0 / 1 / 2 with beeps; self-checks
(frame on every call, portal geometry and apex match on the first 200 apex
calls) disable it for the session. v16 width presets removed. Analyzer
`tools/analyze_capture_v7.py` (off vs on per corner, stalls, corridor sharing;
synthetic `tools/make_synthetic_v17.py`). `docs/function_map_v4.md` adds the
Phase 10/11 CONFIRMED pairs. The live run (`docs/capture_v17_runbook.md`) is
NOT done yet.

Phase 12 run, 2026-10-14 22:19 (`data/runs/20261014_221943/`, v11 section F):
the fix never acted. Its geometry self-check failed 200 / 200 during R1 (fix
still off) and disabled it; the user missed the refusal beeps, so R2..R4 were
the unmodified game (the "unit sweeps a circle round the corner" seen there is
vanilla, from other start points). The frame was right on all 11311 calls.
Correction: the funnel apex is vertex + m * r with m a PER-VERTEX offset
(miter) vector at +0x24 / +0x28 of the vertex record (|m| = 1.455 / 1.31 at the
two corners, 1.0 on a straight wall), the right side uses [E+8]'s own m, the
helpers branch on side-object type +0x08, and E+0x30 is 0.0 (a clamp limit, not
the length); `waypoint_choice_x86.md` W1/W2 corrected. The knot mechanism is
confirmed again from the fix rows: 69 of 120 men got the identical apex within
1 s in R1, but only 36 / 15 in R2 / R3, so the vanilla pass-to-pass spread is
large and one off / on pair cannot decide. Corridors are not shared (median
46 distinct [R+0x0C] per 120 men). Next: v18 with the lane pA -> pB (both ends
from the game's own formula), type and apex-match checks, an alarm on disable,
and a repeated off / on protocol. Not built.

Phase 12b, 2026-10-14 (`docs/PHASE12b_fix_plan.md`, v11 section G): DLL v18
built, offline-tested (`tools/hooktest18.cpp`: full 42/42, geometry-disable
10/10, crash path) and installed (v17 kept as `dinput8.dll.v17.bak`). Fix
geometry as found live: for type-0 side objects the apex is pA = mA * r + A or
pB = mB * r + B (m = vertex offset vector at +0x24 of each vertex record); the
waypoint slides from the apex along the lane to the other candidate (mode 1
slot lane, mode 2 own position). F6 = fix on / off, F7 = lane mode; three long
low beeps = fix disabled or key refused. Fix rows log type / m / candidates /
lane, `fixraw.csv` keeps raw side objects, and samples are also taken from the
orders and fix hooks. Analyzer `tools/analyze_capture_v8.py` adds [8] per
order (F8) game-same / final-same / knot over repeated off / on passes. The
live run (`docs/capture_v18_runbook.md`, 8 orders) is NOT done yet.

Phase 12b run, 2026-10-14 22:50 (`data/runs/20261014_225015/`, v11 section H):
the v18 geometry is CONFIRMED live (apex match 200/200, 350/350 rows), the fix
adjusted 273 apex calls, but the game itself barely converged in that run
(3..11 % of the men on one corner point per order, no single file measured
off or on), so the user saw no difference. Strong convergence needs the start
close to the building and the destination out of sight round the corner
(run 22:19 R1: 69 of 120). Beep() was never audible. User rule: test keys are
I / O / P, never F-number keys. v19 (`src/hooks_v19.cpp`: same fix, keys
I fix on/off, O marker, P finish, only while the game has the focus; tones via
PlaySound; harness `tools/hooktest19.cpp` 45/45) is installed (v18 kept as
`dinput8.dll.v18.bak`); runbook `docs/capture_v19_runbook.md`, not run yet.

Phase 12d, 2026-10-14 23:20 (v11 section I): run 20261014_230445 (v19, two
battles, infantry through a walled gate) reproduced the funnel strongly (110
of 120 men on one gate-post waypoint, fix off) but the fix left 97 of 120 there
because gate side objects are types 1 / 2 / 3, which v18 / v19 skipped; where
it applied (a type-0 building corner) it cut 96 shared men to 15. Static: for
types 1..3 the endpoint offset comes from {x, z, owner} at side object +0x38
(left) / +0x44 (right) via FUN_1037f370; types >= 4 use the edge direction
E+0x14. v20 (`src/hooks_v20.cpp`) computes both candidates by calling the
game's own FUN_10393000 / FUN_103a0080 (byte-checked; type 0 cross-checked
against the v18 formula), so every type is covered; harness
`tools/hooktest20.cpp` 47/47 runs the real helpers. Installed (v19 kept as
`dinput8.dll.v19.bak`); runbook `docs/capture_v20_runbook.md` (gate, off / on
/ off / on). Not run yet.

Phase 12d run, 2026-10-14 23:20 (`data/runs/20261014_232030/`, v11 section
J): v20 worked as built (every apex call adjusted, all checks clean, tones
heard; waypoints spread evenly over the 11.4 m gate lane) but the user saw no
change: the unit still funnels at the gate corner, one man at a time, and
sometimes splits into two streams round both posts. Measured: knots remain
(on 15 / 18 vs off 20 / 14 / 12) of two kinds: at the short wall edges beside
the posts (lanes 1.9 m), and a blob 10 m before the gate whose men aim at
points up to 7 m apart. Spreading the apex along its portal is not enough.
Next (offline): read per-tick steering / soft collision (FUN_103942f0,
SoftCollisionObject.cpp) and move the lane for short edges onto the widest
portal at the same corner vertex.

Phase 13, 2026-10-15 (`docs/ghidra_analysis_v12.md`, static part
`docs/steering_x86.md`, x86 only): per-tick motion read. FUN_103942f0 only
stores the waypoint as steering target e+0x16C; FUN_103aa090 makes the
desired displacement e+0xD8 / +0xE0 (dir * speed * 0.1 s tick);
FUN_101b2be0 -> FUN_103ac9c0 is soft collision (soft object at e+0x30;
push (R^2/d^2 - 1) * d into e+0x70 / +0x74, gross into +0x78 / +0x7C);
FUN_10367600 integrates (push * 0.1, crowd damping to as little as 10 % when
gross / net > 3, |D| <= 2 m, hard collision FUN_10384570 / FUN_1039a9a0,
position += D). No formation or corridor term in per-tick motion. Run 23:20
re-measured from positions (`tools/route_sections.py`,
`tools/plot_route_png.py`, outputs in the run folder): at the gate line the
fix spreads crossings from a 1 m band at the post (off) to about 8 m (on);
upstream a 5.5 m passage limits the column to 3..5 m in every pass, so the
gate scene cannot show more; v11 J2's "blob before the gate" is that
column. The user's "one man at a time" is not located in the positions yet.
Next: clean single-corner scene with v20 as installed, O pressed at the
moment single file is visible, plot that marker time. New Ghidra tool
`tools/sweep_fieldsets.py`; the s2x86 program name is `empire.retail.dll`.

Run 20261015_094109 (v20, hairpin round a wall end into a 6.1 m mouth and a
6.5 m channel; v12 section E): one like-for-like pair. Channel column IQR
0.6..0.9 m with the fix off (single file, measured) vs 1.4..2.1 m on (about
5..6 abreast); throughput unchanged (mouth-limited); the knot at the wall end
remains (19 off / 18 on) and the fix adds approach knots where it spreads
waypoints along a 24 m fan edge. Needs a repeat in the same direction.
User then reported the fix-on order looked worse: men stuck running into
walls. Measured (`tools/stuck_men.py`): on 58 men / ~600 man-s stuck vs off
76..207 man-s; v20 moved waypoints up to 15 m back along a 24 m fan edge
(median 59 deg off the game's direction, 115 deg at the second opening).
v21 (`src/hooks_v21.cpp`, v12 section F): turn limit 25 deg from the game
waypoint as seen from the soldier, and stuck release (no 0.5 m in 1.5 s ->
game waypoint for 4 s). Harness `tools/hooktest21.cpp` 55/55, geo 30/30,
crash OK. Installed (v20 kept as `dinput8.dll.v20.bak`); runbook
`docs/capture_v21_runbook.md` (same gate, off/on/off/on, one direction).
Run 20261015_100000 (v21, v12 section G): no wall-sticking any more, but no
whole-width flow either (channel IQR at best 1.6 m of 6.5 m), and eastbound
with the fix on is worse (72 men / 617 man-s stuck vs 248 off): the fix
slides the target along a corner edge that runs back along the man's own
path, leaving it about 1 m ahead of him. The slide-along-the-apex-portal rule
is structurally wrong (along-track shifts, slot order crosses lanes).
Candidate: keep each man's own clearance from the corner (order-preserving,
sideways only); to be checked in an offline crowd model first.

Phase 14, 2026-10-15 (v12 section H, `sim/`): offline crowd model of the
hairpin gate (map from recorded positions, decoded per-tick laws, recorded
slot schedules; calibrated rs 0.43 m / 3.5 m/s; matches the game at the
corner, more generous in the channel; misjudged v20/v21). Real bottleneck:
a ~4.6 m gap under a thin wall tip, game uses ~1 m of it. Best rule
"lateral rank" (lane at right angles to the flow, men ranked across the
flow, stuck release): pass spread 0.9 -> 1.9 m, stuck -47 %, flow +10 %,
channel column barely wider; buildable variant with a fixed 4 m lane. No
corner rule gives whole-width flow. Report `sim/report/gate_trials.html`.
v22 (v12 section I): mode 1 = lateral rank (lane 4 m, capped by the portal
lane length; stuck release kept; no turn limit in mode 1). Harness
`tools/hooktest22.cpp` 55/55, geo 30/30, crash OK. Installed (v21 kept as
`dinput8.dll.v21.bak`); runbook `docs/capture_v22_runbook.md`. Not run yet.
Run 20261015_111654 (v22, v12 section J): only one comparable fix-on pass
(east, a WALK order; all eastbound fix-off references were runs). Real,
speed-independent effect: men pass the wall tip much wider (spread 3.2 m vs
0.8..1.7 in all eight fix-off passes). Channel column unchanged. Flow equals
the model's no-fix prediction; stuck time higher (494 man-s, a wait below
the mouth) but not decidable without a matched off pass. Always check walk
vs run (free speed) before comparing passes.
Run 20261015_112917 (v22, new map, two matched off/on pairs, v12 section
K, `tools/gate_pairs.py`): with the fix the men use more of a 5.5 m
gatehouse (31 -> 49 %, 39 -> 46 % of its width) and round its corner wider
(0.8 -> 2.4 / 1.9 m), but flow through it drops 14..17 %. No change at the
180-degree turn round a thin wall tip, where every pass jams. Stuck times
were contaminated by orders given before the previous pass cleared the tip.

Cohesion (2026-10-15, v12 section L; user goal: unit ready = every man in
his slot): the formation's slots reach the destination 6..12 s after a run
order and do not wait; the men lag up to 25 m and need 10..13 s more. Cause,
measured: a man nearly stops when his waypoint is more than 20 deg off his
motion (run 7 -> 2.3 -> 1.5 -> ~0.8 m/s at 30 / 50 / >60 deg), and at a
corner every man gets a 90..180 deg turn at the apex, so they stop one by
one. v20..v22 only spread men sideways and gain under 3 s. Model has a
turning model now (90 deg/s fits). Next: decode the turn slowdown in
FUN_103aa090; test "lead the turn" (arc waypoints) and milder turn slowdown
by time-to-formed-up. `tools/formup.py` measures form-up.
Phase 15 (v12 section M): stragglers stall facing a corner point that is
always occupied by another man (0.6..0.8 m/s); re-plans every ~0.5 s;
turn rate ~240 deg/s run. Model: congestion costs ~3.7 s of ~22 s form-up;
best rule "lead the turn" 120 deg recovers 1.1 s, v22 0.6 s; ideal 18.2 s.
Real straggler tail is longer than the model's. Candidate v23: lead the turn
+ distinct unoccupied target points; needs a controlled form-up test (one
direction, run orders, wait for full form-up between orders, 3 off/on pairs).
New tools: `tools/formup.py`, `tools/stragglers.py`, `tools/gate_pairs.py`.
v23 (v12 section N): mode 1 = lead the turn (aim 120 deg further round the
corner vertex within 6 m, free point, sector-bounded, sign kept per corner;
model: ready90 -0.7..-0.9 s, knots -3.5 men). Harness `tools/hooktest23.cpp`
59/59, geo 30/30, crash OK. Installed (v22 kept as `dinput8.dll.v22.bak`);
runbook `docs/capture_v23_runbook.md` (form-up test). Not run yet.
Run 20261015_131431 (v23, gatehouse, proper form-up protocol, v12 section
O): v23 visibly changes corner behaviour (corner passed 1.7..2.0 m out vs
0.9..1.1, knots 21 -> 14..15 going up, no stuck men) but form-up time only
moves within noise (down +0.7 s, up -0.9 s at 90 %; unit formed ~10 s after
the order). Tracked unit had 90 soldiers (user says 120-man infantry).
Run 20261015_162717 (v23, same gatehouse, 200-man unit, v12 section P):
user screenshots show the knot at the ramp-foot hairpin (94 men within 2.5 m
off vs 58 on) and the queue behind it; ramp column IQR 0.8 -> 1.4 m, gate
9.9 -> 12.2 men/s, form-up 38.8 s both. The ON pass is confounded: a re-path
without a constructor call at 203.6 s, then the slots froze 7.3 s at the foot
(first measured formation hold) and the men walked. Next: matched run-order
down pairs with the 200-man unit.
Phase 16, 2026-10-15 (v12 sections P addendum and Q): the user gave no
click in d2 (only focus to the snipping tool), so the 7.3 s slot hold came
from the game. SC2 movement (public summaries of Anhalt, GDC 2011): navmesh +
funnel + boids steering with look-ahead; friendly movers do not jam. Model of
the gatehouse ramp (`sim/map_gatehouse.py`, `sim/extract_moves_gatehouse.py`,
`sim/metrics_gatehouse.py`, `sim/run_gate2.py`, `sim/steer.py`): look-ahead
steering fills the ramp (IQR 1.4 -> 2.8 m) and shrinks the knot 30..40 %, but
does not speed form-up in the model (+0..5 s); only an occupied-waypoint stall
reproduces the game's slow inflow. v24 = v23 + that steering (entry hook on
the integrator FUN_10367600, e+0xD8/+0xE0 turned 0..75 deg for corner men
within 12 m; `src/hooks_v24.cpp`, harness `tools/hooktest24.cpp` 70/70, geo
30/30, crash OK) is installed (v23 kept as `dinput8.dll.v23.bak`); runbook
`docs/capture_v24_runbook.md`. Not run yet.
Phase 16b, 2026-10-15 (v12 section R): run 20261015_172654 (v24, 150 men):
same-unit avoidance shrank the knot (foot 62 -> 27 men) but cut ramp flow
18 -> 12 men/s and slowed form-up 6..9 s. The user's transcript of the SC2
talk (`data/runs/GDC Vault - ...txt`): SC2 avoidance ignores same-direction
movers and idle units; units push. Shogun's crowd damping (FUN_10367600, D
scaled to >= 10 % when pushed from all sides, gated by byte e+0xD6) matches
the 0.6..0.8 m/s corner stall. v25 = v23 + e+0xD6 = 0 for corner men within
12 m (no avoidance); `src/hooks_v25.cpp`, harness 68/68, geo 30/30, crash OK;
installed (v24 kept as `dinput8.dll.v24.bak`); runbook
`docs/capture_v25_runbook.md`. Not run yet.
Run 20261015_173729 (v25, 150 men, v12 section S): first fix that speeds the
unit up. Inward 90 % in slot 17.5..18.4 s off -> 16.4 / 14.9 s on, 100 %
19.7..20.6 -> 15.7 s; ramp-foot crowd 59..62 -> 27 men, gate flow 17 -> 29
men/s, stuck men 8..10 -> 0..2, arrival spread 6.0 -> 3.0 s. On passes all
came after off passes (one I press); confirm with an alternating run.
Run 20261015_192047 (v25, 200 men, v12 section T): inward result
replicated with ON before OFF (gate 90 % 13.9 vs 18.4 s, foot crowd 49 vs
78, gate flow 31 vs 17 men/s, stuck 0 vs 36 men). Outward ON slower once:
the formation crawled ~8 s at the start (path follow 10..20 calls/s instead
of 40). Formation holds seen in 4 of ~16 fix-on passes, 0 of ~19 off
(`tools/slot_holds.py`); cause unknown. P ends the capture for the whole
launch: a second battle in the same session is not recorded.
Run 20261015_193946 (v25, 17 interleaved passes, v12 section U): v25
CONFIRMED on the gatehouse. Inward 90 % in slot 14.2..17.5 s ON vs
19.3..22.2 off, foot crowd 35..66 vs 71..89, ramp column 1.5 vs 0.6 m,
gate flow 27..31 vs 15..19 men/s, stuck men 0..1 vs 30..44; no overlap.
Outward smaller gain (90 % 14.4 vs 15.5 s mean). A formation hold occurred
in a fix-OFF pass: holds are not caused by the fix.
Phase 17 (v12 section V): with v25 the ramp-foot crowd keeps moving (~6 m/s
vs ~1 off); a third of it already aims at the next corner 27 m away (one
apex point, outside the 6 m lead zone and 12 m no-damping zone). v26: I
cycles off -> A (v25) -> B (A + far lanes 6..30 m + no damping within 30 m)
-> C (A + no damping within 30 m); harness 79/79, geo 30/30, crash OK;
installed (v25 kept as dinput8.dll.v25.bak); runbook
docs/capture_v26_runbook.md. The sim does not reproduce v25 (no-damping
barely matters there); not used for tuning.
Run 20261015_201332 (v26, v12 section W): variant C (lead the turn + no
damping within 30 m) best inward (90 % in slot 14.8..15.0 s vs off 16.7..17.4);
B (far lanes) dropped. User saw rear men wander away from the gate on
outward orders: confirmed (column re-laid, rear slots behind them). v27: I
cycles off -> C -> D (C + gather towards the queue corner on goal calls);
harness 76/76, geo 30/30, crash OK; installed (v26 kept as .v26.bak);
soldier rows 1 M; runbook docs/capture_v27_runbook.md.
Run 20261015_203944 (v27, v12 section X): D gather helped outward but sent
men already inside back out on inward orders (bug: "passed" test used only
the last apex vertex). v28: gather only for men with no apex call since the
unit's last order; harness 77/77 (GD5 fails on v27), geo 30/30, crash OK;
installed (v27 kept as .v27.bak); runbook docs/capture_v28_runbook.md.
Run 20261015_205247 (v28, v12 section Y): bug fixed; D = best overall.
Inward 90 % in slot D 14.7..15.5 / C 15.1..15.8 / off 17.9..18.1 s; outward
D 12.6..13.0 / C 13.0..13.3 / off 14.2..14.7 s; no stuck men with C or D.
D misses men routed round an inner-yard corner to rear slots (apex calls).
Phase 18 (v12 section Z): user idea "fill the passage width" -> v29 E = D
+ lanes (rank across the flow among the men queueing for the corner,
offset q * min(portal lane, 4 m): far zone apex + u * s, lead zone radius
rP + s). I cycles off -> D -> E. Harness 81/81, geo 30/30, crash OK;
installed (v28 kept as .v28.bak); runbook docs/capture_v29_runbook.md.

Run 20261015_221831 (v29, v12 section AA): only 104 s recorded (P early).
D code unchanged from v28 (source diff); recorded D passes slower than v28
D, each with a game formation hold (same spot as v28 inC1 with C). E:
ramp spread 7.4 m but flow 23 vs ~36 men/s, 28 stuck; rejected. User: D
worse, congestion; back to C. v28 reinstalled (first I press = C, one high
tone); v29 kept as `dinput8.dll.v29.bak`.

v30 (2026-10-15, for a new user test): v28 with I cycling off -> A (mode 1,
v25: lead the turn + no damping within 12 m; one high tone) -> C (two high)
-> D (three high) -> off (one low); C and D unchanged from v28.
`src/hooks_v30.cpp` (scratchpad `make_v30.py`), harness
`tools/hooktest30.cpp` 80/80 (FC1 / NC1 / L15 added), geo 30/30, crash OK;
installed (v28 kept as `dinput8.dll.v28reinstall.bak`). P still ends the
capture for the whole launch.

Run 20261015_231130 (v30, v12 section AB): thin-wall hairpin map; with C
up to 198/198 men share the tip waypoint, C never acts there (outside 6 m;
end cap "narrow"). v31 F "tip lanes" (user diagram `data/diagram.png`):
I cycles off -> C -> F; per-man concentric lane round a funnel vertex
(>= 8 men queueing), radius rP + rank * min(lane, 4 m), kept across both
corners of a thin wall end. `src/hooks_v31.cpp` (scratchpad
`make_v31.py`), harness `tools/hooktest31.cpp` 87/87, geo 30/30, crash OK;
installed (v30 kept as `dinput8.dll.v30.bak`); runbook
`docs/capture_v31_runbook.md`. New tool `tools/plot_scene_png.py`.

Run 20261015_233944 (v31, F / off / C / F / off / C / F, one round trip
each; v12 section AC): F changed nothing (tip knot 54..76 F vs 59..72 off,
closest approach unchanged); C again faster on down passes (90 % in slot
12.0 / 13.2 vs off 13.9 / 15.2 s). Cause: on the way down the tip apex is
the 0.7 m end cap, where v31 refused to start a lane; on the way up lanes
existed (R ~3 m) but men reached only ~1/3 of the radius. v32 = v31 with
lanes allowed to start on end caps (edge <= 1.5 m, band 4 m);
`src/hooks_v32.cpp`, harness `tools/hooktest32.cpp` 90/90, geo 30/30, crash
OK; installed (v31 kept as `dinput8.dll.v31.bak`); runbook
`docs/capture_v32_runbook.md`. New tool `tools/tip_rounding.py`.

Phase 20, 2026-10-16 (v12 section AD): v32 rejected untested by the user
(research first). Look-ahead avoidance built: v33 variant L = C + avoidance
on the integrator entry for every path-following man (stalled and crossing
men avoided, same-way movers ignored, 13 directions +-75 deg, horizon 5 m,
Moussaid cost, speed kept, 0.5 s side hold, wall memory, unit velocity
snapshots). I cycles off -> C -> L. `src/hooks_v33.cpp` (scratchpad
`make_v33.py`), harness `tools/hooktest33.cpp` 94/94 x3, geo 30/30, crash
OK; installed (v32 kept as `dinput8.dll.v32.bak`); runbook
`docs/capture_v33_runbook.md` (pass marks fixed in advance). New tool
`tools/stalled_gaps.py`. Not run yet.

Run 20261016_091335 (v33, v12 section AE): L FAILED its pass marks (knot at
the tip 58..77 vs C 63..72; men running into stalled men 1.5..2.7/s vs C
0.2..3.3/s; form-up and stuck men same as C). The avoidance fires only at
contact (median free distance 0.2 m) and turns men mostly 10..20 deg; inside
4 m of the tip there is no room. C stays the best measured variant. The
unit's address can change mid-run (same soldiers): merge by soldier set.

AF (2026-10-16, v12 section AF): congestion-aware routing studied, not
built. No alternative route at the busy corners of either test map
(`tools/route_alternatives.py`). Soldier request radius decoded
(`docs/request_radius_x86.md`; hook 0x103df4d1): raising it per man widens
his corner line inside the game's own funnel (no failure, too-narrow portals
at +100000 cost), expected gain ~10 % (corner line only).

Phase 21, 2026-10-16 (v12 section AG): v34 variant R (user's dynamic radius
idea) built: radius lanes at the soldier replan request (patch 0x103DF4D1,
w = w0 / +1.5 / +3.0 m by rank, overfull lane -> neighbour lane), no
damping within 30 m, waypoints untouched. I cycles off -> C -> R. Harness
`tools/hooktest34.cpp` 89/89 x3, geo 30/30, crash OK; installed (v33 kept as
`dinput8.dll.v33.bak`); runbook `docs/capture_v34_runbook.md`. Expected gain
small (knot -10..15 %); pass marks fixed. Not run yet.

Run 20261016_102617 (v34 R; v12 section AH): R failed its marks (knot 96 %
of C, closest approach +0.25 m) but was never worse than C (form-up 0.5..2 s
faster, fewer stuck). Lanes followed 2/3 of the way; on down passes they
were set only 1..2 m before the tip because men replan rarely mid-corridor
(`docs/replan_cadence.txt`): keyed to the unit's busy corner they would
come at ~27 m (`docs/lane_timing_check.txt`). Research on lanes / portal
steering / flow fields in AH. Corridor polygon check
(`tools/corridor_polygon_check.py`, `docs/corridor_check/`): the builder's
per-order corridor is disconnected boxes, 0.7 m slivers at the thin-wall
tip, unusable as a walkable grid. Offline Continuum-Crowds field in the sim
(`sim/flowfield.py`, `sim/report/flowfield_trials.txt`): column +16 %, no
form-up gain, no whole-width flow. Decision: v36 flow field NO-GO (needs
navmesh regions); v35 R2 (unit-keyed radius lanes) GO pending the user.
Nothing built.

Phase 21b, 2026-10-16 (v12 section AI): v35 R2 = v34 R with the lane keyed
to the unit's busy corner ahead (members' fresh apex vertices clustered,
largest queue wins; passed corners skipped; lane kept 8 s). The game's apex
offset scales with the request radius in both directions (0.86 / 1.94 /
3.02 m), so only the timing was wrong. `src/hooks_v35.cpp`, harness
`tools/hooktest35.cpp` 94/94 x3, geo 30/30, crash OK; installed (v34 kept
as `dinput8.dll.v34.bak`); runbook `docs/capture_v35_runbook.md`. Not run.

Run 20261016_121912 (v35 R2; v12 section AJ): R PASSED its down-pass marks
(knot 65 % of C, closest approach +0.5..0.6 m, form-up not slower, no
stuck men, no frame cost) and equals C on the way up; every tip man had a
lane in effect (keyed to the upper corners first, the request radius covers
the whole path); lanes followed ~70 % (0.7 / 1.3 / 2.1 m by lane). The
tip pivoting the user sees (men walk into the wall end and rotate) is
quantified with tools/tip_pivot.py: R cuts it to 3..4 % slow samples vs
24..35 % off. ALL TESTS WERE RUN AT 3x GAME SPEED (measured 3.6 game-s per
wall-s): every speed / duration in the docs is wall time, and every fix
window (GetTickCount ms) is 3.6x longer in game time than its name; the
fix is validated at 3x only. Game tick readable at [[ctrl+0x14]+8]+0x50
(0.1 s per tick). Proposal v36 = R2 + C lead-the-turn + game-time windows;
not built, awaiting the user's go.

Phase 22, 2026-10-16 (v12 section AK): v36 variant RC = v35 radius lanes +
C lead the turn (mode 9; I cycles off -> C -> RC), and every fix window now
runs on the GAME clock read from the engine tick at [[ctrl+0x14]+8]+0x50
(FUN_101bc880 hook; GameNow() = tick * 100 ms; wall clock only until the
first order hit), with the lengths kept at their tested 3x values (x 3.6).
Capture timestamps and row limits stay on the wall clock.
`src/hooks_v36.cpp` (scratchpad `make_v36.py`), `build_v36.bat`; harness
`tools/hooktest36.cpp` drives a fake tick counter (Advance instead of
Sleep; GT1..GT6, RC1) 102/102 x3, geo 32/32, crash OK. Installed (v35 kept
as `dinput8.dll.v35.bak`); runbook `docs/capture_v36_runbook.md` with pass
marks (down as v35, up not worse than C, gt_fallback 0). Not run yet.

Run 20261016_130609 (v36 RC; v12 section AL): RC MISSED the down knot
mark (88 % of C, mark 85 %), met the approach mark (+0.5 m), form-up and
stuck fine; two of four RC up passes jammed from the two shortest
approaches (an off pass from the same distance jammed too); within noise
RC = v35 R, lead the turn adds nothing at the hairpin. Game clock worked
(2.47 M reads, 1 fallback). Fixed landmarks needed for both ends next time.
User observation measured at a gate (z 133.6 on the same map): the slot
block does not taper; 196 of 200 slots sit in a 20 m wide block before
the gate (13 on the gate line) for 5..7 s, then all 200 jump to the far
side within 2 s and re-lay 27..45 m wide; the men fan from a 20 m block
into an 8 m gap. Candidate v37 "taper" (narrow the approach nodes' +0x74
/ +0x78 at the builder hook so the slots form a column before the gate):
not built, research + gatehouse-model check first.

Phase 22b (v12 section AM): taper / column at a gate studied, NO-GO, not
built. Literature (Adrian/Seyfried: gate throughput independent of the
approach corridor width, density falls with a narrow approach; Pottinger:
a formation cannot move until formed) and the crowd model
(sim/run_gate133.py: column +2.6 s, taper +-0.6 s, cutting the forming
wait -6.2 s) agree. Measured mechanism at the gate (run 130609): the
narrow rows (3..4 abreast) stretch the slot block 32 m forward into the
passage at the order; the men WALK into it (1..5 m/s wall, 6..6.5 s) while
the slots sit or creep; when the bulk is within ~2.5 m the block jumps
through the passage and the men run and funnel (4..6 s). The front-row
men are never in slot when it releases. Next lever: the formation
controller's advance rule (cursor P+0xA8 advance in FUN_101b7b70 /
FUN_101a9750, formed test, walk-while-forming), static decode proposed;
cheap alternative probe: node-width floor = order width (v15 override
machinery) to stop the stretch. Free running speed is 12..13 m/s wall
(3.4..3.7 m/s game), not 6.6. New tools: sim/map_gate133.py,
sim/extract_moves_gate133.py, sim/run_gate133.py, tools/lanes_effective.py.

Phase 23, 2026-10-16 (v12 section AN): formation controller decoded
statically (x86 + x64): the unit cursor is the LEADER soldier's projection
on the unit path (leader index ctrl+0x30, clamped to the command-group
size [unit+0x334]); once per game second FUN_101bc880 lays the slot block
from cursor + D forward along the path, D = [unit+0x334]*[unit+0x10C] + 2v
= 11 m (T = D/v = 3.14 s, the order's time field), so narrow rows push the
block front far ahead; each man gets a go-to order (speed v, time T, mode
0) or a formation-move command (FUN_1018abb0, vfunc +0xD0); ready byte
ctrl+0x0C = all FUN_103cbe40. Speed chain: FUN_103df3f0 pacing branch
(min(vmax, dist/time), needs e+0x440 == 1), FUN_103942f0 max e+0x110,
FUN_10366a10 commanded e+0x1AC, FUN_103dda00 terrain multiplier e+0x1BC,
FUN_103aa090 target e+0x1BC * e+0x150 with accel/decel. The measured
approach law (speed ~ 0.1 * distance per game s, waypoint = slot) is not
pinned to one field yet. v37 = v36 fix + capture of those fields per
soldier sample and leader_idx / ready / speed_mode per order row
(`src/hooks_v37.cpp`, `tools/hooktest37.cpp` 106/106 x2, geo 32/32, crash
OK; `tools/headless_decompile.bat` runs decompile_funcs.py). Installed
(v36 kept as `dinput8.dll.v36.bak`); runbook `docs/capture_v37_runbook.md`
(fix off, one gate pass each way, two open-ground orders). Not run yet.

Run 20261016_171016 (v37; v12 section AO; docs/capture_v37_analysis.txt;
user ran C / RC / off x 3 instead of fix-off, which does not matter for
the measurement): the slow approach is PINNED to the pacing branch:
soldier max speed e+0x110 = min(5.08, |pos - slot| / (3.14 s - elapsed
e+0x46C)) in every go-to order (mode byte e+0x440 = 1; AN's mapping was
off by one byte), elapsed resetting each game second; formation-move
orders run at 3.5, first-click walk orders at 1.5. Controller: leader =
soldier index 2, slot order = soldier index, the slot block's FRONT is
9 +- 2 m ahead of the leader and the rows are laid BACKWARDS along the
path with the node widths (AN had them forward), so in a passage
narrower than the frontage the block stretches over the whole column
(rear pinned on the wide ground, depth up to 60 m), every man's slot is
a few metres ahead of him and he paces at 1 .. 2 m/s: the crawl (N
passes 6.5..7.6 wall s of 13..16, S 3..4.5 of 11.5..14.7). Release = the
front reaching the path end -> block re-laid at the destination -> men
run (channel flow 38..61 men/s vs 15..19 in the crawl). Run 130609's 6 s
wait = the same block static (leader's projection clamped at t = 0 of
the gate node) with the men walk-capped (cause unrecorded). Design v38
variant G "release beyond the choke": replace the 11 m advance of the
cursor copy in FUN_101bc880 (FUN_103670a0 call, return-address
filtered) by the distance to the exit of the narrow run + block depth
when the block would fall into narrow nodes; T unchanged; pass marks in
AO. Not built; awaiting the user's go. Tools: tools/v37_*.py.

Phase 24, 2026-10-16 (v12 section AP): v38 variant G = RC + release
beyond the choke, built on the user's go. Lever: the CALL FUN_103670a0
at 0x101BC983 in FUN_101bc880 advances the local cursor copy by D = 11 m
before the slot block is laid (front there, rows backwards); FUN_103670a0
is __thiscall(cursor {node, t}, float distance, char flag), walks nodes
by the follow points +0x80 / +0x88. v38 hooks its entry (stolen 83 EC 10
56 57), keeps only the return address 0x101BC988, and when cursor + D
falls in a run of nodes narrower than 0.6 x the path's widest node
(widest >= 10 m) rewrites the stack argument to the run exit + block
depth (ceil(n / floor(W / 1.4)) x 0.85 m) + 3 m from the cursor, past
the path end if the run reaches it; the plan comes from the orders hook
(path walk from ctrl+0x34, per thread). Order time untouched. I cycles
off -> C -> RC -> G (three high tones). Rows
pathfix_capture_v38_release.csv, counters release_*. `src/hooks_v38.cpp`
(scratchpad make_v38.py), harness `tools/hooktest38.cpp` enters the REAL
advance through the real call site on fake node lists: 119/119 x2, geo
32/32, crash OK. Installed (v37 kept as `dinput8.dll.v37.bak`); runbook
`docs/capture_v38_runbook.md` (G / RC / off round trips, pass marks
against RC). Not run yet.

Run 20261016_181741 (v38 G; v12 section AQ; docs/capture_v38_analysis.txt):
G released the block 45..55 m ahead at the first laying call after every
order and is the fastest variant measured (90 % in slot N 11.9 / 11.8 /
10.6 s vs RC 16.3 / 12.9, S 8.4 / 10.4 / 10.8 vs RC 10.6; stuck men and
100 % not worse; the knot mark missed on S passes and is dropped for G,
the faster flow fills the tip). User: "looks really good", but the front
men sometimes walk back briefly. Measured cause: once per G pass the
block front retreated 5..9.5 m at the laying call where the leader's 11 m
point passed the narrow run's exit (rule off, vanilla point behind the
anchor); walk-back episodes start 0.1..1.3 s after each retreat. v39 = v38
+ anchor hold (never lay the front behind the last applied anchor of the
same path until the vanilla point passes it; reset at a new order =
leader index 0, a longer path, or 1 game s without calls); rows reason
"hold", counters release_adv_held / hold_end / hold_reset.
`src/hooks_v39.cpp` (scratchpad make_v39.py), `tools/hooktest39.cpp`
122/122 x2, geo 32/32, crash OK. Installed (v38 kept as
`dinput8.dll.v38.bak`); runbook `docs/capture_v39_runbook.md`. Not run
yet. Analysis tools: tools/v38_load.py, v38_pass_table.py, v38_anchor.py,
v38_walkback.py (order times from leader_idx == 0 rows when the orders
are clicks).

Run 20261016_183851 (v39 G; v12 section AR; docs/capture_v39_analysis.txt):
down passes the best measured (90 % 8.3 / 9.2 / 9.8 s vs RC 11.0), up
passes a regression (13.8 .. 15.3 s, one never formed, 187 stuck): the
up destination sat ~12 m past the channel top, the anchor went PAST THE
PATH END, and an advanced cursor copy at the end (or on the last node
within ctrl+0x40 of a man) makes FUN_10173680 give every man a
formation-move command instead of a go-to order; formation moves do not
pathfind round the thin wall, so the unit crawled at the hairpin (go-to
fraction 0.00 at the release vs 1.00 when the anchor is inside the path).
The v39 hold worked (no retreats). v40 = v39 with the anchor capped at
path end - 2 m (reason "end_gap"); `src/hooks_v40.cpp` (scratchpad
make_v40.py), `tools/hooktest40.cpp` 122/122 x2, geo 32/32, crash OK;
installed (v39 kept as `dinput8.dll.v39.bak`); runbook
`docs/capture_v40_runbook.md`. Not run yet. Tools: tools/v39_ov_pass.py
(per-second pass overview: order type, slow men, tip), tools/v39_anchor.py.

Run 20261016_213500 (v40 G; v12 section AS; docs/capture_v40_analysis.txt;
user protocol C / RC / G / C / RC / G): G fastest both ways (down 90 % 8.4 /
7.7 / 9.4 s vs RC 11.6; up 11.9 / 9.7 / 10.3 vs RC 14.3 / 14.1; stuck 0..4 vs
RC 25 / 40; go-to orders kept ~6 s after every up release; 0 retreats; 79
past_run, 13 hold, 0 end_gap rows). Six of seven marks met; the tip mark
(<= 20 men < 0.3 m/s within 8 m of the wall end) missed once: 28 men for
0.6 s in pass 221.0, where the leader's projection sat clamped at the end of
a node 41 m along the route for 5 s, so the block anchored past the channel
at the first laying call and all 200 men arrived at the tip within 1 s from
a 24 m approach (RC: 59 / 65 slow men for 4 s). The end-gap cap was NOT
exercised: the up destinations were 8..10 m further out than run 183851's
failing ones, the narrow run ended 21..28 m before the path end, anchors
9..19 m inside. Needs an up order within ~12 m of the channel top. The
walk-back detector is unusable at the hairpin (flags the turn). Tools:
tools/v40_load.py, v40_pass_table.py, v40_anchor.py, v40_walkback.py.

Run 20261016_221116 (v40 G; obstacle course, C / RC / G / off once each;
v12 section AT; docs/capture_v40_course_analysis.txt): G fastest on every
choke leg (gate 8.8 s vs 15.5 .. 16.6; doorway 7.4 vs 9.0 .. 16.2; corridor
10.6 vs 11.6; two gates 11.2 vs 13.2 .. 17.3), equal on open legs, 0 .. 2
stuck men, gate flow 2 .. 3x, 0 retreats. C and RC were WORSE than off at a
straight gate / doorway (30 .. 54 stuck, a 17 s doorway jam). End gap
exercised twice: leg 2 as intended; leg 5 fired on a 19 m wide final
segment (relative rule 0.6 x widest 34 m) and put 65 % of the men on
formation moves on open ground, no cost, no gain; an absolute width would
avoid it, not changed. Tools take PATHFIX_RUN / PATHFIX_UNIT / PATHFIX_CTRL;
tools/v40_paths.py prints the path geometry per build.

Restructure 2026-10-16 (after run 221116): v40 made the main build; the
working dir became a git repo for several fixes (layout at the top of this
file, "Repository layout"). The rebuilt v40 from the new tree (entry points
renamed, fix registry added) passed the harness (122/122 x2, geo 32/32,
crash dump); the game folder still holds the original v40 binary
(byte-identical to archive/builds/build_v40/dinput8.dll), and the old
installed DLLs moved to archive/installed_dlls/. Correction: the README first
said the keys stop with the capture; wrong, v40 has a post-capture loop that
keeps polling I and re-asserting the crash filter.

v41, 2026-10-16 (user: G is best, drop the others; I toggles the path fix
any time, default off; unhook P; no recording): variant G code unchanged.
I toggles FIX_OFF <-> FIX_V38_G (one high tone on, one low off). Capture only
when the process has env PATHFIX_CAPTURE=1 (proxy/log.cpp LogCaptureMode;
WindowOpen returns false otherwise, capture buffers not allocated, no run
folder, log = <game>\pathfix_log.txt rewritten each launch, O / P not read,
crash.dmp still next to the log). C / RC stay in the code (G is built from
them) and are selectable only by the harness-only export PathfixTestVariant.
Harness (fixes/pathfix/tests/hooktest.cpp): capture mode for full / geo /
crash / pkey; real I for G; new play run (18 checks: G acts, nothing
written, P / O ignored) and pkey run (9: keys after P). test.bat: full
124/124 x2, geo 32/32, play 18/18, pkey 9/9, crash dump. Installed (v40 kept
as archive/installed_dlls/dinput8.dll.v40.bak). Runbook
fixes/pathfix/docs/runbook.md rewritten for play; the v40 capture runbook is
research/capture_v40_runbook.md. Not played yet.

Public release 1.0, 2026-10-16 (user: v41 is dev only; the public DLL has the
fix only, always on, install by copy, uninstall by delete, separate folder):
fixes/pathfix/release/pathfix_release.cpp = v41's mode-G code path only (5
hooks: FUN_101bc880, 0x103DF6C0, FUN_10367600, 0x103DF4D1, FUN_103670a0; no
keys, tones, log (proxy/log.h PATHFIX_NO_LOG compiles Log out), capture,
crash filter or test exports; endpoint-helper mismatch = nothing installed).
CMake option PATHFIX_PUBLIC_RELEASE (own tree build_release/), version
resource 1.0.0.0. release.bat = build.bat + release build + run_tests.py
--with-release + fixes/pathfix/release/package.py -> release/Shogun2-PathFix-
1.0/ (dinput8.dll 122880 bytes, README.txt for players, SHA256.txt) + .zip;
build_release/ and release/ are gitignored. Harness mode "release": same
scenarios on dev-in-G (71 checks, reference) and release (78 = 71 + RS1-3 no
test exports / only the 5 hooks / RF1-4 nothing written); the runner requires
all 71 reference result lines identical in the release run (passed). Not
played yet; the game folder still has dev v41.

Second restructure, 2026-10-16 (user: the repo hosts several fixes, aiming and
shooting fixes to come; GitHub ShakuEight/Shogun2-fix): everything pathfix-
specific now lives under fixes/pathfix/ (archive/, data/ with runs and field
sweeps, ARCHITECTURE.md, the full README, this file, pathfix.cmake). The root
holds the shared parts and repo-level README / ARCHITECTURE / CLAUDE. CMake
option renamed PATHFIX_PUBLIC_RELEASE -> PUBLIC_RELEASE; the packager moved to
tools/release/package.py (args version, name, readme). Run paths rewritten
data/runs -> fixes/pathfix/data/runs in analysis, sim scripts and sim JSON
(74 edits, logged in archive/MOVES_2.csv). release.bat rebuilt everything
from the new tree: all tests passed and the public DLL came out
byte-identical (SHA-256 1A15B08A...40AB).

Bug report and v42, 2026-10-17 (user: four 200-man units through a gatehouse to one spot, then a
shift-queued second order; "on the live game with fix off all units move without stopping",
with the fix on one or more units stay at the spot for good, live and in the replay). The live
battle (10:34) ran v41 with the fix OFF (I pressed on then off at 10:26); replay
`4 kyoto police running.replay` played under v40 capture with G: run 20261017_104106 (two units
stuck), run 20261017_114804 (one replay off, one G: off all four go on, G one stuck for good).
Decode (`docs/re/unit_finish_x86.md`): the behaviour stack pops the move controller when every
man is ready (FUN_103cbe40: go-to men only in their slot, formation-move men at once); the
laying call re-orders only men not ready; at the path end (cursor + D) every man gets a
formation move = the game's release. G's anchor put the leader's slot short of the end, he
arrived (s+0x384 1, s+0x308 1), was never re-ordered, the cursor froze, cursor + D stayed
0.7..4 m short: no release, go-to men pinned by the stacked units, unit never finished.
v42 = v41 + park release (AdvanceHit: anchor applied on this path, leader arrived, no narrow run
ahead, path left <= D + depth + 3 m -> front 0.5 m past the end, latched until a new order /
path; rows reason "park", counters release_adv_park / _latched) + recording in play (P on / off
any number of times, folder per recording with its own pathfix_log.txt, CSV prefix v42, tones 5
rising / 6 falling, O marker while recording; PATHFIX_CAPTURE=1 keeps the v40 capture mode) +
local time [hh:mm:ss.mmm] on every log line (proxy/log.cpp LogRecordingStart / Stop).
Harness: ParkSection PR1..PR7 in full and release runs, T14 updated, T17/T18; new run "rec"
(18). test: full 134/134 x2, geo 32/32, play 18/18, pkey 9/9, rec 18/18, crash OK; release
1.0.1 (park release only) equivalence 79/79, release_pub 86/86, package
release/Shogun2-PathFix-1.0.1 (dinput8.dll 123904 bytes, SHA-256 28137AE6...35CE). The old
build trees (CMake cache from E:\_SHOGUN\pathfix, the repo folder before its rename to
E:\_SHOGUN\Shogun2_fixes) moved to archive/builds/build_v41_tree and
build_release_1.0_tree. v42 installed in the game folder (v40 kept there as
dinput8.dll.v40.bak; v41 still dinput8.dll.41). The user accepted the fix ("good fix") the
same day; no P recording of the replay test was collected. v42 and release 1.0.1 committed and
pushed to GitHub on 2026-10-17 (second commit on top of the squashed history; path references
moved to E:\_SHOGUN\Shogun2_fixes in the same commit).
