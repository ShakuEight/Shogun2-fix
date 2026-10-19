# pathfix

The first fix of [Shogun2-fix](../../README.md). In vanilla Total War: Shogun 2, units ordered
through gates, doorways and round corners stop in a stretched block and file through one man at
a time. With this fix they run through as a column and re-form beyond the gap, and men round
corners on wider lines.

| Build | Where | What |
|---|---|---|
| **Public release 1.0** | `release\Shogun2-PathFix-1.0\` at the repository root, built by `release.bat` | Variant G with the park release, always on. No keys, log, recording or crash handler, and only the five hooks G needs. Copy in to install, delete to uninstall. See section 7 |
| **Dev build v43** | `build\dinput8.dll`, built by `build.bat` | I toggles G (v43: with its yari wall part, section 3.10), P starts and stops a recording. Sections 1 to 6 describe it unless stated otherwise |

How the fix was found is in [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 1. What the pathfix does

### The problem

In vanilla Shogun 2 a unit that is ordered through a corner, a gate or a doorway does not move
as a formation. Two things happen:

1. **The corner knot.** Each soldier plans his own navmesh path to his own formation slot. Every
   shortest path round a convex corner passes the same inner vertex, so 50 to 110 men of a unit
   receive the identical waypoint at the corner post. They queue there one at a time, and up to
   100 men stand within 2.5 m of one point.
2. **The formation crawl.** Once per game second the unit's formation controller lays out the
   slot block 11 m ahead of the leader and lays its rows *backwards* along the path, each row
   as wide as the passage allows. In a passage narrower than the frontage the block stretches
   back over the whole column, every man's slot is only a few metres ahead of him, and the
   game's pacing rule (`speed = min(5.08, distance / (3.14 s - elapsed))`) makes him walk at
   1 to 2 m/s. The unit crawls through the choke and only runs once the block front reaches
   the destination.

### What the build changes

The path fix is switched on and off with the **I** key, at any time during play. It is off at
launch. One high tone means on, one low tone means off.

When on, the build runs **variant G**, which combines four mechanisms:

| Part | What it does |
|---|---|
| **Release beyond the choke** | When the unit's next slot block would be laid inside a narrow stretch of its path, the block goes past the end of that stretch instead. |
| **Radius lanes** | Men queueing for the same corner take wider lines by rank. |
| **Lead the turn** | Men near a corner aim further round it, at a free point, instead of at the corner post. |
| **No crowd damping** | Men at a corner are not slowed when pressed from all sides. |

The release is the main effect. Every man gets a far slot, the pacing rule gives him full
running speed, and the unit runs through the choke as a column and re-forms on the far side.
On open ground G changes nothing.

Up to v40 the I key cycled through the intermediate variants C (lead the turn and no damping)
and RC (C plus radius lanes). v41 dropped them from the key because G was the best in every
test. Their code stays, since G is built from them.

### Measured effect

These measurements were made with v40, whose variant G is identical to v41's. All times are
wall seconds at 3x game speed, until 90 percent of the men stand within 3 m of their final
slot. Lower is better.

Hairpin ramp, run 20261016_213500, 200-man unit:

| Direction | G | RC | C |
|---|---|---|---|
| down the ramp | 8.4 / 7.7 / 9.4 | 11.6 | 12.1 / 12.7 |
| up the ramp | 11.9 / 9.7 / 10.3 | 14.3 / 14.1 | 15.8 / 14.9 |

Obstacle course, run 20261016_221116, one pass per mode:

| Leg | C | RC | G | off |
|---|---|---|---|---|
| 30 m gate with a right-angle exit | 15.5 | 16.6 | **8.8** | 15.7 |
| 6 m doorway | 16.2 | 13.9 | **7.4** | 9.0 |
| two gates in an S | 13.9 | 13.2 | **11.2** | 17.3 |
| open ground | 19.8 | 19.8 | 19.2 | 19.5 |

Through the 30 m gate G passed 87 men per second, against 20 to 31 in the other modes. Stuck
men with G: 0 to 4 per pass, against up to 54 with C and 40 with RC.

---

## 2. Install and use

**Requirements:** the original Shogun 2 (32-bit `Empire.Retail.dll`; the 2026 Complete Edition
is a different x64 binary and is not supported), single player or replays only. An injected
movement fix desyncs multiplayer.

**Install:** close the game, then copy `build\dinput8.dll` into the game folder
(`...\steamapps\common\Total War SHOGUN 2\`). Check that `Shogun2.exe` is not running first:
Windows locks a loaded DLL, and replacing it under a running game corrupts the install.

**Uninstall:** delete `dinput8.dll` from the game folder.

**In battle:** press **I** to switch the path fix on (one high tone) or off (one low tone), as
often as you like. The key only counts while the game window has the focus.

**Log.** Each launch rewrites `<game>\pathfix_log.txt`, a few kilobytes: install checks, key
presses, self-check failures. Every line starts with the local time `[hh:mm:ss.mmm]`. If the
game crashes, `crash.dmp` is written next to the log (into the recording folder while a
recording is on).

**Recording (v42).** Press **P** to start a recording (three rising notes) and P again to stop it
(three falling notes), as often as you like in one session:

- each recording writes into its own folder `<game>\pathfix_runs\<YYYYMMDD_HHMMSS>\`: the CSV
  files of section 3.9 and a `pathfix_log.txt` that copies the session log while it runs;
- `timestamp_ms` 0 is the P press, whose local time heads the folder's log;
- files are flushed every 10 s and closed at the second P, or when the game closes;
- the buffers (about 220 MB) are allocated at the first P and reused; soldier rows fill first,
  after a few minutes of movement;
- O puts a marker into the running recording; three long low tones on P mean it was refused.

`python tools\runs\collect_runs.py` copies new run folders into `fixes\pathfix\data\runs\`, never
overwriting.

**Capture mode, for the harness.** With the environment variable `PATHFIX_CAPTURE=1` the v40
capture runs instead: one run folder per launch, a 900 s window from the first hook hit, O
marker and P finish.

**Safety.** If anything about the game binary differs from the analysed one, the build installs
nothing and says so in the log. If a self-check fails at run time, the fix switches itself off
for the session and sounds three long low tones. See section 3.8.

---

## 3. How it works

### 3.1 Loading

Windows resolves `dinput8.dll` from the application folder before `System32`. The proxy
(`proxy/`) loads the real `System32\dinput8.dll`, forwards its five exports, and on the game's
first `DirectInput8Create` call installs every compiled-in fix through the registry in
`proxy/fixes.cpp`. That first call comes after `empire.retail.dll` is loaded, so its code can be
patched.

### 3.2 Hook engine

All addresses are Ghidra names in the analysed image (base `0x10000000`). At install the module
base is read and every address is rebased.

Each hook replaces whole instructions at its target with a `JMP` to a naked stub. The stub saves
all registers and flags, calls the handler with a pointer to the saved context, restores them
and jumps to a trampoline holding the stolen instructions, which jumps back into the game.
Before patching anything, **all 17 targets are byte-checked** against the bytes recorded from
the analysed binary. One mismatch and nothing is installed. Stack offsets inside the context
were generated with `tools/re/stack_calc.py`, never by hand, and are checked live on every hit.

| # | Address | Game function | Role in the build |
|---|---|---|---|
| 0..7 | various | parser, move order, command processor, setup, UI | Capture only, from the research phases. |
| 8 | `0x101A9750` | per-tick cursor loop | Capture: unit cursor. |
| 9 | `0x1039BB40` | path builder | Capture: the unit path's nodes and widths. |
| 10 | `0x101BC880` | per-soldier orders, once per game second per unit | Capture. **G:** builds the narrow-run plan. Reads the game clock. |
| 11 | `0x103DBBF0` | soldier go-to-slot | Capture: each man's order and slot. |
| 12 | `0x103DAFB0` | slot off the navmesh | Capture. |
| 13 | `0x103DF6C0` | inside `FUN_103df3f0`, after the funnel `FUN_10394e80` | **C:** lead the turn, rewrites the waypoint. |
| 14 | `0x10367600` | per-tick motion integrator | **C:** no crowd damping for corner men. |
| 15 | `0x103DF4D1` | inside `FUN_103df3f0`, the replan request | **RC:** request radius by lane. |
| 16 | `0x103670A0` | cursor advance | **G:** release beyond the choke. |

### 3.3 The game's movement pipeline, as decoded

```
mouse order
  -> path builder FUN_1039bb40: unit path = linked nodes, each with a corridor width
     (+0x74 / +0x78, clamped to the local passage) and follow points (+0x80 / +0x88)
  -> once per game second, FUN_101bc880 per unit:
       copy the leader's cursor on the path, advance the copy by D = 11 m (FUN_103670a0),
       lay the slot block with its FRONT there and its rows backwards along the path,
       give soldier i slot i: a go-to order (pathfinding) or a formation move (no pathfinding)
  -> per soldier FUN_103df3f0: navmesh path to the slot, funnel FUN_10394e80 picks the
     waypoint (the corner vertex offset by his radius)
  -> steering FUN_103942f0, speed FUN_103aa090 with the pacing rule,
     integrator FUN_10367600: soft collision, crowd damping, hard collision, position += D
```

The formation width reaches the path nodes but never the per-soldier steering. There is no
formation term in per-tick motion. Each soldier is an independent pathfinder chasing his slot.

### 3.4 Lead the turn and no crowd damping (the former variant C)

**Lead the turn** (hook 13). When the funnel returns a corner apex for a man within 6 m of the
corner vertex, the waypoint is moved 120 degrees further round the vertex, inside the free
sector, at a radius no larger than his distance plus 4 m. If another man stands within 0.7 m of
that point, the radius steps out by 0.7 m. The turning direction is kept per corner for 3 game
seconds. Men take a rounder line instead of all touching the same post.

**No crowd damping** (hook 14). The integrator scales a man's move down to as little as 10
percent when he is pushed from all sides (gross push more than 3 times the net push), gated by
byte `e+0xD6`. For men whose waypoint is a corner apex within 30 m, the build writes
`e+0xD6 = 0`. The queue at a corner keeps flowing instead of freezing.

**Stuck release.** A man who makes less than 0.5 m of progress in 1.5 s of corner calls gets
the game's own waypoint back for 4 s.

### 3.5 Radius lanes (the former variant RC)

Hook 15 sits where a soldier asks for a new navmesh path with his clearance radius. The game's
funnel offsets the corner apex in proportion to that radius, so a larger request radius gives a
wider line round the same corner.

The build keys the lane to the **unit's** busy corner ahead: corner vertices that members
received as apex within the last 3 game seconds, clustered within 1.5 m, largest queue first,
corners already passed skipped. Men queueing for that corner are ranked across the flow into
three lanes: the default radius, +1.5 m and +3.0 m, capped at 4.2 m. A lane holding more than
1.3 times its share moves the man to the neighbour lane. A lane is kept for 8 game seconds,
which covers the walk to the corner without a replan.

### 3.6 Release beyond the choke

**Plan** (hook 10). At every slot-laying call the build walks the unit path from the leader's
cursor (`ctrl+0x34`), at most 96 nodes, following `next` at `+0x08` and the node lengths from
the follow points. With `W` the widest node width:

- a node is **narrow** if `min(w74, w78) < 0.6 W`, and only when `W >= 10 m`;
- consecutive narrow nodes form a **run** with an exit position along the path;
- the block depth at the passage is `ceil(n / floor(W / 1.4)) x 0.85 m` for `n` men.

The plan is stored per thread with the cursor it was made for.

**Advance** (hook 16). The game calls the cursor advance from many places. The hook acts only
when its return address is `0x101BC988`, the call inside the slot-laying function, and a fresh
plan for this thread matches the cursor exactly. Let `P = cursor + D`, the game's block front:

```
if P lies inside a narrow run:
    A = run exit + block depth + 3 m            (chained through any run A falls into)
    A = min(A, path end - 2 m)                   (v40: never the path end itself)
    if A > P: rewrite the stack argument D to A - cursor
```

Everything else is untouched. The order's time field stays 3.14 s, so every man's slot is far
away and the pacing rule yields full speed through the choke.

**Anchor hold** (v39). Once an anchor has been applied on a path, later laying calls on the same
path never put the front behind it, until the game's own point passes it. The hold resets at a
new order, when the cursor's distance to the end grows (a new path), or after 1 game second
without laying calls. Without the hold the front retreated 5 to 9.5 m once per pass and the
front men walked back.

**End gap** (v40). An advanced cursor at the path end is the game's own end-of-path state, in
which every man gets a formation move instead of a go-to order. Formation moves do not
pathfind, so a unit sent just past a choke pushed straight into the wall. The anchor therefore
stops 2 m before the end, and the game's own release still happens when the leader arrives.

**Park release** (v42). How a unit finishes an order, decoded from the game: every laying call
re-orders only men who are not yet ready; a man on a go-to order is ready once he is in his slot,
a man on a formation move as soon as he has his order; when every man is ready the move
behaviour is popped and a shift-queued order starts. The unit cursor is the leader's projection
on the path. The anchor can put the leader's slot short of the path end. He arrives, is ready,
is never re-ordered, so the cursor stops and `cursor + D` never reaches the end: the game's
release (front at the end, formation moves to the destination layout) never comes. With several
units stacked on one spot, go-to men that cannot reach occupied slots then keep the unit from
ever finishing. The rule:

```
if an anchor was applied on this path, the leader has arrived (s+0x384 == 1, s+0x308 == 1),
   no narrow run lies ahead, the front is short of the end and
   path end - cursor <= D + block depth + 3 m:
    D = path end - cursor + 0.5 m              (the game's own release)
    latched: the same on every later laying call until a new order or a new path
```

The cap keeps the release to the distance the anchor moved the leader ahead of the game's own
creep, so formation moves (which do not pathfind) never cover a long stretch.

### 3.7 Game clock

All fix time windows run on the game's tick counter, read at `[[ctrl+0x14]+8]+0x50` in
0.1 s ticks, so they scale with the game speed setting. The window lengths were tuned at 3x
speed and are kept at those values, which is 3.6 game seconds per wall second.

### 3.8 Self-checks and crash handling

- **Install:** all-or-nothing byte check of the 17 targets. A different game build is refused.
- **Stack layout:** on every hit the saved `ESP` must equal the expected return slot.
  Otherwise nothing is recorded and no fix runs.
- **Waypoint frame:** checked on every call. Portal geometry and the apex match with the game's
  own helpers are checked on the first 200 corner calls. A failure disables the fix for the
  session, with three long low tones.
- **G:** return-address filter, cursor match, plausibility of `D` (0 to 100 m), plan age under
  100 ms. Each failure leaves the game's value alone and is counted in `fix_totals.csv`.
- **Memory:** every game read goes through structured-exception-guarded copies.
- **Crash filter:** a crash in the game writes `crash.dmp` into the log folder, chaining to
  the game's own filter. The game installs its filter later, so ours is re-asserted every
  second for the whole session.

### 3.9 Capture files

Written into the run folder as `pathfix_capture_v43_<name>.csv` (runs recorded with v40 .. v42
use those prefixes). Without a recording none of the buffers is allocated, and every recording
path stops at one gate that is open only between the two P presses (or for the capture window
in capture mode). The fix code runs before that gate in every handler, so G behaves the same
with and without recording. Columns are documented in the
local research log (`docs/research/capture_format_v17.md` and the later section headers of
`ghidra_analysis_v12.md`), which is not in the public repository.

| File | Content |
|---|---|
| `soldiers` | Per sampled soldier: position, slot, waypoint, speed fields, order type |
| `orders` | One row per slot-laying call: cursor, leader index, ready byte; v43: game tick, formation vtable and floats, yari wall ability, group count, requested width |
| `gotos` | Go-to orders per soldier |
| `builds`, `build_nodes` | Every path build with its node chain |
| `fix`, `fixraw` | Corner fix decisions, raw side objects |
| `radius` | Radius-lane assignments |
| `release` | Every G advance call: plan, `D`, `P`, anchor, reason; v43: reason `yari_end`, `depth_kind` |
| `yari` | v43: every decided yari wall slot call: width, files, ranks, block span, narrowest node, sharpest turn, laid by the block generator or not |
| `events`, `cursor`, `units`, `totals`, `fix_totals` | Mode changes, markers, counters |

### 3.10 Yari wall (v43)

User report (2026-10-19, replay `yariwall_pathing`): a yari wall unit still paths badly through a
gatehouse and round a sharp corner with G on; men wander off, the unit splits into two blobs and
slows down. The yari wall is its own formation class (type 3, `docs/re/formation_types_x86.md`):

- Its slot generator lays one straight grid at the block front, 1 m per file and per rank, with
  the heading of the path piece there. The file count comes only from the requested width (the
  player's drag), so the depth is whatever the player set. It never follows the path pieces and
  never narrows a row to the passage, as the block's generator does.
- Every slot skips the navmesh test, so every man is sent to his slot even when it lies behind a
  building, and each man plans his own route there.

v43 adds three things to G, all part of the I toggle (`PATHFIX_YARI=0` switches them off and
leaves G exactly as v42):

1. **Laid along the path where the wall does not fit.** An entry hook on the yari wall's slot
   generator (0x103C5D30, only the call from the per-second slot laying) checks the block the
   wall would lay: from the front back over its ranks. A path node narrower than the requested
   width minus 1 m, or a turn of more than 30 degrees inside it, and the block's own generator
   (`FUN_103c4720`) lays the slots instead, on the same object. On straight open ground the two
   give the same grid. The choice is kept 3 game seconds. The final wall at the destination is
   always the game's.
2. **No end-gap anchor for a yari wall.** When a narrow run reaches the path end, G (v40) puts
   the front 2 m before the end at once. For a yari wall that laid the whole straight wall at a
   corridor exit while the unit was still at the gatehouse, and men whose slots were nearer by a
   parallel passage went that way. A yari wall keeps the game's front in that case. Release past
   a narrow run, the hold and the park release are unchanged.
3. **Block depth from the wall's own files.** G's anchor beyond a narrow run uses the block depth;
   for a yari wall it is ceil(men / files) x 1 m, from the wall's own files for the widest node.

Measured on the replay (two 150-man yari units, drag widths 10 to 31 m, 5 to 15 ranks; autorun at
speed 1; `analysis/yariwall_passes.py` and `yariwall_summary.py`, local; passes of both yari units
from game tick 860, when the recording switched G on):

| Run | Split (s, summed) | Most strays in a pass | Spread p90 (m, mean) | Median speed (m/s) |
|---|---|---|---|---|
| Fix off | 13.9 | 47 | 18.5 | 2.27 |
| G as v42 (the recording, in sync) | 27.6 | 33 | 18.0 | 2.91 |
| G as v42, on all battle | 27.8 | 37 | 18.3 | 2.82 |
| v43, the recording's I schedule | 0.0 | 1 | 10.2 | 2.29 |
| v43, on all battle | 0.0 | 0 | 10.4 | 2.69 |

Split = seconds with the men in two or more groups of at least 15 % (4 m single linkage); stray =
a man more than 15 m from the largest group. In the corridor pass of the worse unit the split went
from 13 s to 0 and the strays from 31..37 to 0..1; in the hairpin pass that follows, 90 % of the
men were in their slots after 20 s instead of 25..34 s. Every run except the in-sync one desyncs
from the recording at its first change, so their battles differ in detail.

---

## 4. Build and test

Requirements: Visual Studio 2022 Build Tools (x86 MSVC), CMake, Python 3 with numpy.

```
cmd /c build.bat      -> build\dinput8.dll and build\pathfix_hooktest.exe
cmd /c test.bat       -> offline harness, six runs
```

`test.bat` needs the game's `empire.retail.dll`. Set `GAME_DIR` if the game is not in
`E:\SteamLibrary\steamapps\common\Total War SHOGUN 2`. No game process is involved: the harness
maps the real DLL, loads the proxy, and drives the hooks through the game's own helper functions
on fake soldiers, portals and path nodes. It passes when:

- the full run passes 136 of 136 checks, twice. It runs in capture mode, selects C and RC
  through a harness-only export to check G's parts on their own, and switches G with the
  real I key. The park release scenario (PR1 to PR7, T17, T18) is part of it;
- the geometry-disable run passes 32 of 32;
- the play run passes 18 of 18. It runs without capture mode: G acts on I, O alone is ignored,
  and without P no run folder or CSV file is written;
- the key run passes 9 of 9: after P ends a capture, I still switches G on and off;
- the recording run passes 18 of 18: P on / off twice gives two folders, the second starting
  empty, each with its own log; a recording still on when the game closes is written; log
  lines carry the local time;
- the yari run (v43) passes 27 of 27: `PATHFIX_TOGGLE_TICKS` flips G on and off by game tick; a
  fake yari wall object (the real type 3 vtable) is laid through the real slot call site with
  both game generators stubbed to report which one ran (clean span, narrow node, kept, expired,
  bend, final wall, no plan, other object, other call site, fix off), the yari depth in G's
  anchor and the end-gap anchor left to the game;
- the crash run dies on a deliberate access violation and leaves `crash.dmp` in its run folder.

CMake options: `FIX_PATHFIX` (default ON) and `BUILD_TESTS` (default ON).

---

## 5. Folder layout

Everything pathfix-specific lives in this folder. The proxy, the RE tools and the game-binary
reference it uses are shared with the other fixes and live at the repository root.

| Path | Content |
|---|---|
| `src/pathfix.cpp` | Dev build v43: the whole fix and its recording (P toggled, or the capture mode with `PATHFIX_CAPTURE=1`). The header comment keeps every version's notes back to v37 |
| `src/stack_offsets_v16.h`, `_v17.h`, `_v34.h` | Hook context offsets generated by `tools/re/stack_calc.py` |
| `release/pathfix_release.cpp` | The public release: variant G always on, five hooks, nothing else |
| `release/version.rc`, `release/README.txt` | Version resource and the readme for players |
| `pathfix.cmake` | Build wiring, included by the root `CMakeLists.txt` when `FIX_PATHFIX` is ON |
| `tests/hooktest.cpp`, `tests/run_tests.py` | Offline harness and its runner, called by `test.bat` and `release.bat` |
| `docs/runbook.md` | Install, keys, what to look for, rollback |
| `docs/research/`, `analysis/`, `sim/`, `archive/` | Local only, not in the public repository: the research log (`ghidra_analysis_v5.md` to `v12.md`, one runbook and one analysis per capture), the run analysis scripts (section 8), the offline crowd model and every earlier build (v8 to v40) |
| `data/` | Capture runs (`data/runs/<stamp>/`) and field sweeps. About 2.6 GB, local only |
| `ARCHITECTURE.md` | How the fix was found, from the first wrong hypothesis to variant G |

---

## 6. Known limitations

- **The fix starts off.** Each launch needs one I press.
- **Relative narrow rule.** A 19 m wide final stretch counts as narrow next to a 34 m road. G
  then releases on open ground, which costs nothing measurable but gains nothing.
- **Short approaches.** When the leader's path projection is clamped ahead at the order, G
  releases at once. In one hairpin pass that sent 200 men to the corner within a second, and
  28 of them slowed below 0.3 m/s for 0.6 s.
- **Validated at 3x game speed only.** The time windows are game-clocked, so other speeds should
  behave the same, but that is not measured.
- **v42's park release has little in-game data.** It was designed from two recorded replays
  (runs 20261017_104106 and 114804) in which G left units "not finished" at a shared
  destination, so a shift-queued order never started; the harness confirms the rule and the
  user accepted it in game, but no recording of the fixed replay has been analysed. A leader parked with more than D + block depth + 3 m of path left
  is deliberately not released early (formation moves do not pathfind), so such a unit can
  still finish short of its destination layout.
- **Yari wall (v43) tested on one replay.** The rules were checked on two yari units of one
  battle (one gatehouse, one long corridor, one hairpin), not yet live. Other special
  formations (square, wedge and the rest) keep their own slot generators and are untouched.
- **Original 32-bit game only, single player only.**

---

## 7. Public release

`release.bat` builds, tests and packages the end-user DLL:

1. It runs `build.bat`, building the dev DLL and the harness.
2. It builds the release DLL in its own tree, `build_release\`, with CMake option
   `PUBLIC_RELEASE=ON`; `pathfix.cmake` then adds the release source instead of the dev one.
3. It runs every dev test, then the release equivalence runs described below.
4. `tools/release/package.py` packages `release\Shogun2-PathFix-<version>\` at the repository
   root with `dinput8.dll`, `README.txt`, `SHA256.txt`, `LICENSE` and `NOTICE`, and zips it. An older package of the same version is moved to
   `release\_previous\`.

| | Dev build v43 | Public release 1.0 (G as in v42, no yari wall part) |
|---|---|---|
| Source | `fixes/pathfix/src/pathfix.cpp` | `fixes/pathfix/release/pathfix_release.cpp` |
| Fix | off at launch, I toggles G | G always on |
| Hooks | 17 | 5: orders, waypoint, integrator, radius request, advance |
| Log, recording, keys, tones, crash dump, test exports | yes | none |
| Exports | DirectInput + 4 test exports | DirectInput only |
| Size | about 250 KB | 120 KB |
| Self-checks | install byte checks, run-time checks with an alarm | the same checks, silent: the fix switches itself off |

The release source is the dev build's mode-G code path with everything else removed. The
logger compiles to nothing (`PATHFIX_NO_LOG`), and the version resource reads "Shogun 2 Path
Fix 1.0.0.0". Release 1.0 was first packaged from v41 and repackaged with the park release of
dev v42 before it was published.

**Equivalence test.** The harness mode `release` runs the same scenarios twice: lead the turn,
the side-object robustness cases, no crowd damping, radius lanes, release beyond the choke, the
frame self-check and unloading. The first run uses the dev DLL switched to G through its test
export, the reference with 79 checks (71 in the v41 package; the park release scenario added 8). The second
uses the release DLL, with those 79 plus 7 release-only checks. The release-only checks confirm:

- there are no test exports;
- exactly the five G hooks are patched and none of the twelve capture-only functions;
- no log, run folder, CSV file or crash dump is written.

The runner then requires every reference result line, with its computed waypoints, radii and
cursor positions, to appear unchanged in the release run.

The release has passed these offline tests but has not been played itself. Its fix code is the
dev build's, and the dev build's variant G was measured in battle (the park release of v42 was
accepted in game without a recorded run).

---

## 8. Analysing capture runs

Runs are the folders of P recordings (v42) or of capture mode. The analysis scripts below
are in the local `analysis/` folder, which is not in the public repository.

```
python tools\runs\collect_runs.py
set PATHFIX_RUN=E:\_SHOGUN\Shogun2_fixes\fixes\pathfix\data\runs\<stamp>
set PATHFIX_UNIT=<unit from pathfix_capture_v4x_units.csv>
set PATHFIX_CTRL=<its first_ctrl>
python fixes\pathfix\analysis\v40_pass_table.py      per-pass form-up, stuck men, knot
python fixes\pathfix\analysis\v40_anchor.py          G advance rows, retreats, end gaps
python fixes\pathfix\analysis\v40_paths.py           path geometry per build
python fixes\pathfix\analysis\v39_ov_pass.py v40_load T0 T1 [tip_x tip_z]
```
