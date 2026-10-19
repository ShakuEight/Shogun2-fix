# Architecture and history

This document tells how the pathfix came to be, from the first wrong hypothesis to variant G,
and why the build looks the way it does. What the build does today is described in
[README.md](README.md). The working notes behind every step (the
`ghidra_analysis_v5.md` to `v12.md` series, one runbook and one analysis per capture) and every
earlier build are kept locally in `docs/research/` and `archive/`; they are not in the public
repository.

Function names such as `FUN_101bc880` are Ghidra names in the original 32-bit
`Empire.Retail.dll` at image base `0x10000000`. Offsets such as `e+0x110` are fields of a
soldier entity, `ctrl+0x34` of a unit's formation controller.

---

## Part 1. The system

```
Shogun2.exe
  loads dinput8.dll from its own folder          proxy/          (this project)
    -> real System32\dinput8.dll                                  forwarded
    -> first DirectInput8Create: Fixes_InstallAll  proxy/fixes.cpp
         -> Pathfix_Install                        fixes/pathfix/src/pathfix.cpp
              byte-check 17 targets in empire.retail.dll, patch, start the capture thread

battle, every game tick
  game code -> JMP into a hook stub -> handler(context) -> trampoline -> game code
                                         |
                                         +-- capture rows into ring buffers -> run folder CSV
                                         +-- fix: rewrite a waypoint, a radius, a byte, a stack argument
```

Three design rules hold everywhere:

1. **Change the game's inputs, not its outputs.** Every working fix edits a value the game is
   about to consume: a waypoint before the soldier steers to it, a request radius before the
   funnel uses it, a damping flag before the integrator reads it, a distance argument before the
   formation is laid out. The game then does everything else itself, including collision.
2. **Use the game's own helpers.** Where the fix needs geometry it calls the engine's functions
   rather than re-implementing them, and the offline harness calls the same functions in the
   real mapped DLL.
3. **Refuse rather than guess.** All patch sites are byte-checked before anything is written,
   stack layouts are checked on every hit, and every fix path has a self-check that switches it
   off for the session.

The capture code is as large as the fix. It exists because every decision in Part 2 was made
from recorded data, not from watching the game.

---

## Part 2. The journey

### 2.1 The first hypothesis: a dead width parameter (early 2026, phases 1 to 6)

The project started from a common observation: Shogun 2 infantry and cavalry collapse into a
single file when they round a corner or pass a gate. Static analysis in Ghidra (documents v3 to
v5) followed the move command from the script parser `FUN_101f6bf0` through the move order
constructor `FUN_102557e0` to a path object, and found that the formation width is stored at
`moveOrder+0x60`, halved into the unit at `unit[0x542]`, and maximised into `pathObj+0x38`, but
never read by any movement code. The conclusion of v5.1 was that the width was dead code and
that the per-tick path segment search `FUN_10366E40` projected every soldier onto the same
centre line.

The first DLLs, first a `d3d9.dll` proxy and then `dinput8.dll`, hooked the bottom of that
per-tick chain. Three rounds of caller tracing went from the position writer `FUN_10399F80` up to
`FUN_101B7B70`. Two findings ended this line:

- `FUN_10399F80` is a position **copier** into a temporary query buffer, called about 1400 times
  a frame from more than 20 callers. Changing it changed only copies.
- A background thread that wrote soldier positions at 30 fps lost to the game's 60 fps writer.

### 2.2 A second compilation and a fresh start (2026-10-13)

The Complete Edition, an x64 rebuild of the game, was released and the bug reproduced in it.
The decision was to fix the **original x86** binary and to use the x64 module only as a second
compilation of the same source: cleaner function bounds from its unwind tables, register
arguments, and a second opinion wherever the x86 decompile was ambiguous. The old Ghidra
project turned out to be unanalysed, so the x86 DLL was re-imported and fully analysed. String
and constant anchors that survive recompilation (the move-command usage string, `65536 / 360`,
config parameter names) paired 10 functions across the two builds at first, more in later
phases, each confirmed only when both decompiles agreed on a source-level fact
(`docs/re/function_map_v4.md`).

### 2.3 Measuring instead of reading: capture v12 to v14 (phases 7 to 9)

Capture v12 was a ring-buffer recorder on the functions of the v5 pipeline. Its result was
"neither": the path-object registration `FUN_1041c5d0` ran 0 times, and the "path sampler"
`FUN_1042ea90` ran only at battle load, over static map polylines. **The v5 root cause did not
describe mouse orders at all.**

Capture v13 traced what does run on a mouse order. Only the order constructor fired, then
16 to 140 ms later a worker thread rebuilt the unit's path in `FUN_1039bb40`. A hardware write
watchpoint inside the DLL, armed on every thread on the unit's cursor, found the writer. The
path is a linked list of 0xA0-byte nodes, and each node carries a corridor **width**
(`+0x74 / +0x78`).

Capture v14 showed the builder clamps each node's width to the local passage, and that the
order width is the length of the player's drag, capped at the unit's maximum. The width was
not dead after all. It reached the path.

### 2.4 The corridor is not the lever (phase 10, v15)

Static reading showed the node width reaches soldier placement every tick: the formation lays
its rows along copied path pieces, with files per row computed from the node width. Capture
v15 overrode every node width to 49 m. Nothing visible changed. A corner order on nodes 7 to 27 m
wide still went single file. Overriding to 2 m crashed the game at battle start.

The same phase found where the soldiers actually go. About once a second `FUN_101bc880` sends
soldier `i` to slot `i`, and **each soldier plans his own navmesh path to his slot**
(`FUN_103df3f0`, waypoints from the funnel `FUN_10394e80`), never seeing the unit corridor.

### 2.5 The mechanism, confirmed (phase 11, v16)

Capture v16 recorded every soldier's position, slot and current waypoint. Soldiers walk
straight at their slots until the slots move round a building. Then 55 to 62 of 96 men receive
the **identical** waypoint, the building's corner vertex, while their slots stay 11.7 m apart,
and a knot of up to 20 men within 1 m forms there. Shortest paths round a convex corner all
touch its inner vertex. The single file is per-soldier path convergence, not a formation
problem.

From v16 on, every launch writes into its own run folder, crashes leave a dump, and tests never
require copying or editing files. Test keys are I, O and P, because F-keys collide with the
game and `Beep()` is inaudible on the test machine.

### 2.6 Moving the waypoint (phase 12, v17 to v22)

The first fix patched mid-function at `0x103DF6C0`, right after the funnel returns, to slide the
corner waypoint along the portal edge into each soldier's own lane. It took four versions to get
the geometry right:

- v17 failed its own self-check: the apex is offset from the vertex by a per-vertex **miter
  vector** times the soldier's radius, not along the edge.
- v18 matched the game's apex 200 of 200 times, but its test run barely converged even with the
  fix off.
- v19 reproduced the funnel at a walled gate (110 of 120 men on one gate post), but gate side
  objects are types 1 to 3, which v18 skipped.
- v20 called the game's own helpers `FUN_10393000 / FUN_103a0080` for every side-object type.

v20 worked exactly as built. Gate crossings spread from a 1 m band to 8 m, and the user saw no
improvement: the unit still funnelled and sometimes split round both posts. v21 added a turn
limit and a stuck release after men walked into walls. v22 ranked men across the flow. The
conclusion: **spreading waypoints along a portal spreads the crossing but does not speed the
unit up.** Flow through a gatehouse even dropped 14 to 17 percent.

### 2.7 How a soldier actually moves (phases 13 and 14)

Phase 13 decoded per-tick motion. `FUN_103942f0` stores the waypoint as the steering target,
`FUN_103aa090` turns it into a desired displacement, and soft collision `FUN_103ac9c0` pushes
neighbours apart. The integrator `FUN_10367600` applies the push, then a **crowd damping**: when a
man is pushed from all sides, his move is scaled down to as little as 10 percent. There is no
formation or corridor term anywhere in per-tick motion.

Phase 14 built an offline crowd model of the hairpin gate (`sim/`, local only). Its map came
from recorded positions, its laws from the decode, and its slot schedules from captures. It
matched the game at the corner but misjudged v20 and v21, so from then on it was used to rank
ideas, never to accept them.

### 2.8 A new goal: the unit is ready when every man is in his slot (phases 15 and 16, v23 to v25)

The user redefined success: a unit rounds a corner as a cohesive blob and is ready sooner. The
measured cause of slow form-up was the turn: a man nearly stops when his waypoint is more than
20 degrees off his motion, and at a corner every man gets a 90 to 180 degree turn at the apex.

- **v23, lead the turn.** Aim 120 degrees further round the corner vertex, at a free point. Men
  rounded the corner wider (1.7 to 2.0 m against 0.9 to 1.1), but form-up moved only within noise.
- **v24, look-ahead steering.** It shrank the knot but cut ramp flow by a third and slowed form-up
  6 to 9 s.
- The user supplied the transcript of the StarCraft II movement talk (GDC 2011). SC2 avoidance
  ignores same-direction movers, and units push. Shogun's crowd damping did the opposite.
- **v25, no crowd damping for corner men.** One byte, `e+0xD6 = 0`, for men within 12 m of the
  corner. This was **the first fix that sped the unit up.** Confirmed with interleaved passes:
  90 percent in slot 14.2 to 17.5 s against 19.3 to 22.2 s off, gate flow 27 to 31 against
  15 to 19 men/s, stuck men 0 to 1 against 30 to 44, no overlap.

### 2.9 Variants on a theme (phases 17 to 19, v26 to v30)

The I key began to cycle variants, so one battle could compare them. **C** (lead the turn plus no
damping within 30 m) beat the v25 zones. **D** added gathering rear men towards the queue
corner. It helped outward orders, but v27 sent men already inside back out; v28 fixed that. **E**
filled the passage width with lanes, spreading the ramp to 7.4 m but cutting flow from 36 to 23
men/s, and was rejected. The user judged D congested in practice, and C remained the reference
for the next eight builds.

### 2.10 The hairpin, and a rule about rules (phases 20 and 21, v31 to v35)

A thin-wall hairpin map put up to 198 of 198 men on the wall tip's waypoint. v31's concentric
"tip lanes" changed nothing, because the tip apex is a 0.7 m end cap where lanes were refused.
v32 allowed them there, and **the user rejected it untested**. From then on every build needed
research, offline evidence and pass marks fixed before the run, and a variant that misses its
marks is reported as failed rather than tweaked.

- **v33, look-ahead avoidance.** Thirteen candidate directions, Moussaid cost, wall memory. It
  missed its marks: it fired only at contact and turned men 10 to 20 degrees, and inside 4 m of
  the tip there was no room.
- **Phase AF.** Congestion-aware routing was studied and not built. There is no alternative route
  at the busy corners, and the soldier's request radius was decoded instead.
- **v34, dynamic radius lanes (R).** This was the user's idea. A larger request radius makes the
  game's own funnel produce a wider line, and men followed about two thirds of the designed
  offset. On down passes the lanes came too late, because men replan rarely mid-corridor.
- **v35 (R2)** keyed the lane to the unit's busy corner instead of the man's own last apex. It
  **passed** its down-pass marks: knot 65 percent of C, closest approach +0.5 m, no stuck men.

The user then pointed out that every test since the beginning had run at **3x game speed**.
All durations in the research log are wall time, and every time window in the DLL was 3.6 times
longer in game time than its name.

### 2.11 Game time, and the formation that waits (phase 22, v36)

v36 combined R2 with C as **RC**, and moved every fix window onto the game's own tick counter at
`[[ctrl+0x14]+8]+0x50`, keeping the tested 3x lengths. RC missed the down knot mark by 3 points
and was otherwise equal to R.

The user's next observation changed the direction of the project: *the unit tries to stay in
formation until some men have passed the choke, then breaks up.* Measured at a gate, 196 of 200
slots sat in a 20 m block before the gate for 5 to 7 s, then all jumped through within 2 s. A
"taper" that narrowed the approach was studied against the literature (gate throughput does not
depend on the approach width) and the crowd model, and dropped. The lever had to be the
formation controller itself.

### 2.12 The formation controller decoded (phase 23, v37)

Static reading of `FUN_101b7b70 / FUN_101bc880` in both binaries and capture v37 of the speed
fields pinned the mechanism:

- The unit's cursor is the **leader's** projection on the unit path.
- Once per game second `FUN_101bc880` copies the cursor, advances the copy by
  `D = ranks x spacing + 2v = 11 m` with `FUN_103670a0`, and lays the slot block with its
  **front** at that point and its rows **backwards** along the path, each as wide as the node.
- Each soldier's maximum speed is `e+0x110 = min(5.08, distance to slot / (3.14 s - elapsed))`,
  the pacing branch of the go-to order. This matched 91 percent of rows within 0.1 m/s.

In a passage narrower than the frontage the block therefore stretches back over the whole
column, depth 35 to 60 m. Every man's slot is a few metres ahead, he paces at 1 to 2 m/s, and
the unit crawls for 6.5 to 7.6 s of a 13 to 16 s pass. The release comes only when the block
front reaches the path end. Then the block is laid at the destination and the men run, with
channel flow jumping from 15 to 19 to 38 to 61 men/s.

### 2.13 Variant G: release beyond the choke (phase 24, v38 to v40)

**v38.** The one-call lever is the advance at `0x101BC983`. Hook `FUN_103670a0`'s entry, act only
when the return address is `0x101BC988`, and when the 11 m point falls inside a narrow run of
nodes, rewrite the distance argument to put the front past the run's exit plus the block depth
plus 3 m. The harness entered the real advance function through the real call site on fake node
lists. In the live run G was the fastest variant measured, and the user: "looks really good".
The front men sometimes walked back briefly.

**v39.** The walk-back was measured. Once per pass, when the leader's 11 m point left the narrow
run, the rule stopped applying and the game's point lay 5 to 9.5 m behind the last anchor. v39
holds the anchor until the game's point passes it. Down passes became the best ever, at 8.3 to
9.8 s against RC 11.0. Up passes regressed and one never formed.

**v40.** The up destination of that run was only 12 m past the channel, so the anchor went past
the path end. Decompiling the per-man decision `FUN_10173680` explained the result: an advanced
cursor at the end is the game's end-of-path state, and every man then gets a **formation move**
instead of a go-to order. Formation moves do not pathfind, and the whole unit pushed into the
thin wall. v40 caps the anchor 2 m before the end.

v40 was validated twice:

- **Hairpin, run 213500.** G fastest both ways (down 7.7 to 9.4 s, up 9.7 to 11.9 s against RC
  11.6 and 14.1 to 14.3), go-to orders kept after every release, 0 front retreats in 232 advance
  rows. Six of seven pass marks were met. The tip mark was missed once, on the shortest approach.
- **Obstacle course, run 221116.** G fastest on every leg with a choke: the 30 m gate 8.8 s
  against 15.5 to 16.6, the doorway 7.4 against 9.0 to 16.2, two gates 11.2 against 13.2 to
  17.3. It equalled the other modes on open ground. The end gap fired as designed at the doorway.

v40 became the main build.

**v41** made it a play build without touching variant G. The I key toggles between off and G at
any time, off at launch. The intermediate variants C and RC left the key. Recording is off
unless the game starts with `PATHFIX_CAPTURE=1`, in which case the v40 capture, O and P come
back. That mode is kept for work on later fixes. The capture never allocates or writes
anything without that variable. Every handler runs its fix code before the capture gate, so
G cannot tell the difference. The harness gained a play run (nothing written, G acts) and a
key run (keys after P).

**Public release 1.0** splits the player's DLL from the dev DLL (first packaged from v41,
repackaged with v42's park release before it was published). The release source
(`fixes/pathfix/release/pathfix_release.cpp`) contains only variant G's code path: five hooks,
the soldier tables, lead the turn, no damping, radius lanes, the release with its hold and end
gap, and the self-checks. G is always on. It has no keys, log, capture, crash handler or test
exports. Its correctness argument is an equivalence test, since it was not re-measured in battle:
the harness runs the same scenarios through the dev DLL in G and through the release DLL, and
every result must match.

**v42 and the published release 1.0.** The first bug report from play: four 200-man units sent through a
gatehouse to one spot with a shift-queued second order. With the fix off all four went on; with
G one to three stayed at the spot for good, live and in the replay. Two recorded replays and a
decode of how a unit finishes an order explained it. The unit's behaviour stack pops the move
behaviour when every man is ready. A man on a go-to order is ready only in his slot, a man on a
formation move at once, and the laying call re-orders only men who are not ready. G's anchor
put the leader's slot short of the path end; he arrived, was never re-ordered, the cursor (his
projection) stopped, and `cursor + D` stayed 0.7 to 4 m short of the end. So the game's release,
formation moves for everyone once the front reaches the end, never came, and go-to men pinned
in the stacked crowd kept the units unfinished. The **park release** sends the front past the end
in exactly that state: anchor on this path, leader arrived, no narrow run ahead, at most D +
depth + 3 m left; it stays latched until a new order. v42 also brought recording back into
play: P starts and stops a recording into its own folder, any number of times, with its own
tones, and every log line carries the local time.

**v43: the yari wall.** The second bug report: units in yari wall still went wrong through a
gatehouse and round a sharp corner, men wandering off and the unit splitting into two blobs. A
static read found the reason first. The yari wall is its own formation class, and its slot
generator lays one straight grid at the block front: no walk along the path pieces, no row
narrowed to the passage, and every slot exempt from the navmesh test
(`docs/re/formation_types_x86.md`). The first rule (Y1) hands the slot laying to the block's own
generator wherever the wall's block meets a node narrower than the wall or a bend; on straight
open ground the two give the same grid. To measure it on the user's replay, which was recorded
with two I presses mid-battle, v43 got a tick schedule (`PATHFIX_TOGGLE_TICKS`) that replays the
presses, so one run stayed in sync with the recording. Y1 cleaned the gatehouse pass but not a
long corridor that ended at the destination: there the men split even with G as v42, and not with
the fix off. The run showed why. G's end-gap anchor laid the whole straight wall at the corridor
exit while the unit was still at the gatehouse, and the men whose slots lay on the far side took
a parallel passage. A second idea, clearing the slots' "no navmesh test" flag so the game would
hold such men, changed nothing: every one of those slots was on the navmesh, just nearer by the
other passage. It was taken out again. The rule that worked (Y3) keeps the game's front for a
yari wall when a narrow run reaches the path end. With Y1 and Y3 the replay's two yari units
showed no split and at most one stray (README section 3.10).

---

## Part 3. What did not work, and why

| Idea | Versions | Why it failed |
|---|---|---|
| Width is dead code; fix the path sampler | phases 1 to 6 | The sampler runs at battle load; the width does reach the path |
| Hook the position writer | early DLLs | It writes temporary copies |
| Background thread writing positions | phase 6 | 30 fps loses to the game's 60 fps |
| Widen the corridor | v15 | Soldiers ignore the corridor; each pathfinds to his slot |
| Slide the corner waypoint along the portal | v17 to v22 | Spreads crossings, no faster; slides back along the path at some corners |
| Look-ahead steering and avoidance | v24, v33 | Slows the flow; acts only at contact; no room at a tip |
| Gather rear men, fill the width with lanes | v27 to v29 (D, E) | Congestion, lower flow |
| Concentric tip lanes | v31, v32 | Refused at end caps, followed one third; rejected |
| Taper the approach | studied | Gate throughput does not depend on the approach |
| Flow field per order | studied | The builder's corridor is disconnected boxes; no navmesh regions |

The levers that worked all act on the game's own inputs: one damping byte (v25), the request
radius (v35), and one distance argument (v38 to v40).

---

## Part 4. Methods that carried the project

- **Capture first.** Each question became a DLL that records the answer: ring buffers, rate
  limits, per-launch folders, crash dumps, I / O / P keys with tones. The 29 builds from v12 to
  v40 each answered a specific question.
- **Inject known data and watch where it goes.** Width overrides, hardware watchpoints on the
  cursor, and markers at the moment the user saw the problem.
- **An offline harness that runs the game's own code.** From v20 on, the harness maps the real
  `empire.retail.dll` and calls its helpers on fake objects, so a geometry or call-site mistake
  fails on the desk, not in battle.
- **Generated stack offsets.** `stack_calc.py` produces the context offsets after a manual
  calculation once produced garbage caller addresses.
- **Fixed pass marks and interleaved passes.** Vanilla pass-to-pass spread is large (36 to 69
  of 120 men on one apex in consecutive passes), so off and on passes are interleaved and the
  marks are written before the run.
- **The user's eyes.** The decisive observations came from watching the game: one man at a time
  through the gate, the formation waiting before a choke, the front men walking back, the knot at
  the hairpin going up. Each was then measured before anything was changed.

---

## Part 5. Open questions

- The leader's path projection is sometimes clamped ahead of him at an order, which makes G
  release at once on a short approach. Its cause is not decoded.
- The relative narrow rule (0.6 times the widest node) also fires on wide-but-narrower
  stretches. An absolute width threshold would avoid that.
- The ready byte `ctrl+0x0C` is now explained (all men ready, set at the end of each controller
  tick; the behaviour stack pops the controller when it is 1). The per-unit flag at
  `[unit+0x218][ctrl+0x70]` in the go-to versus formation-move decision and the object at
  `[s+0x248]` in the per-man ready test are not.
- A leader parked with more than D + depth + 3 m of path left is not released by the park rule,
  so such a unit could finish at its parked block, short of its destination layout. Not seen yet.
- The fix has only been validated at 3x game speed.
- The park release of v42 / 1.0 has passed the offline harness and was accepted in game, but
  no recording of the fixed replay has been analysed yet.
- v43's yari wall rules are measured on one replay only, and not yet in live play. Whether the
  end-gap rule (G v40) also hurts block units at corridors with a parallel passage is untested.
