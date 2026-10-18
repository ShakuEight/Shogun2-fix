// pathfix.cpp -- dev build v42: variant G + park release, I toggles the path fix, P toggles a recording (2026-10-17)
//
// v42 (runs 20261017_104106 and 20261017_114804, a replay of four 200-man units sent through
// a gatehouse to one spot with a shift-queued second order; fix off: all four go on; G: one to
// three units stay for good):
// - Park release (AdvanceHit). A unit finishes an order when every man is ready
//   (FUN_103cbe40 over the soldier list, popped by the behaviour stack FUN_101b7e20). A man on
//   a formation move counts as ready, a man on a go-to order only once he is in his slot. The
//   laying call re-orders only men who are not ready; once cursor + D reaches the path end
//   (FUN_10381c20) every man still out gets a formation move to the destination layout (the
//   game's release). G's anchor put the leader's slot short of the end; he arrived, was never
//   re-ordered, so the cursor (his projection) stopped and cursor + D stayed 0.7..4 m short of
//   the end: no release, and go-to men who could not reach slots occupied by the other stacked
//   units kept the unit from finishing, so the queued order never started. Now, after an anchor
//   on this path, with the leader arrived (s+0x384 == 1 and s+0x308 == 1), no narrow run ahead
//   and at most D + block depth + G_MARGIN_M of path left, the front goes G_PARK_PAST_M past
//   the end and stays there (latched) until a new order or a new path. Rows reason "park",
//   counters release_adv_park / release_adv_park_latched.
// - Recording in play: P starts a recording into a new folder pathfix_runs\<date_time>\ (CSV
//   files as v41's capture, named pathfix_capture_v42_*, and a pathfix_log.txt copy of the
//   session log while it runs), P stops it; any number per session. Capture buffers are
//   allocated at the first P. Tones: three rising notes = recording on, three falling = off,
//   three long low tones = refused. O marks a moment while recording. PATHFIX_CAPTURE=1
//   (the harness) keeps v41's capture mode unchanged.
// - Log lines start with the local time [hh:mm:ss.mmm] (proxy/log.cpp).
//
// --- v41 header follows ---
// pathfix.cpp -- main build v41: variant G only, I toggles the path fix, no recording (2026-10-16)
//
// v41 changes nothing in what variant G does (v40, below). It changes how the
// build is driven:
// - I toggles the path fix between off and G (mode 10) at any time during the
//   game; off at launch. One high tone = on, one low tone = off. The other
//   variants (C, RC and the older modes) are no longer selectable by key; their
//   code stays because G is built from C and RC (lead the turn, no crowd
//   damping, radius lanes), and the harness selects them directly through the
//   test-only export PathfixTestVariant to check those parts on their own.
// - No recording in normal play: the capture window never opens, no capture
//   buffers are allocated and no CSV file or run folder is written. The log is
//   <game>\pathfix_log.txt, rewritten at every launch; a crash still writes
//   crash.dmp next to it. O and P are not read.
// - Capture mode (environment variable PATHFIX_CAPTURE=1, set by the offline
//   harness; kept for later work on other fixes): everything as in v40, with
//   per-launch run folders, O marker and P finish.
//
// --- v40 header follows ---
// pathfix.cpp (was src/hooks_v40.cpp) -- variant G: the anchor never reaches the path end (Phase 24c, v12 section AR)
//
// Run 20261016_183851 (v39): with the destination only ~12 m past the choke,
// the release anchor (run exit + depth + 3 m) went PAST THE PATH END, which
// is the game's own end-of-path state: every man then got a formation-move
// command instead of a go-to order, and formation moves steer straight at
// the slot without pathfinding, so the whole unit pushed into the thin wall
// at the hairpin and crawled (go-to fraction 0.00 from the release, 44..95
// men below 0.3 m/s at the corner, 187 stuck; the passes with the anchor
// inside the path kept go-to orders, 5.08 m/s, and were the best measured).
// v40 caps the anchor at path end - G_END_GAP_M, so the advance never marks
// the end reached and the men keep their go-to orders; the game's own
// release still happens when the leader himself gets there.
//
// --- v39 header follows ---
// hooks_v39.cpp -- variant G with the anchor held (Phase 24b, v12 section AQ)
//
// Run 20261016_181741 (v38): G released the block beyond the choke at every
// order and the passes were the fastest measured, but in every G pass the
// block front RETREATED 5..9.5 m for one call: once the leader's 11 m point
// passed the exit of the narrow run the rule stopped and the vanilla point
// (leader + 11 m) lay behind the anchor (exit + depth + 3 m), so the front men
// got slots behind them and walked back for about a second (the user's
// "men in front walk back briefly"). v39 holds the anchor: after a release
// anchor was applied on a path, a later vanilla point that lies behind it is
// moved up to the anchor (the front never retreats) until the vanilla point
// passes the anchor; the hold is dropped at a new order (leader index 0 at
// the first slot-laying call), when the cursor's distance to the path end
// grows (a new path) or when the laying calls stop for a game second.
// Rows: reason "hold" (applied 2); counters release_adv_held /
// release_adv_hold_end / release_hold_reset.
//
// --- v38 header follows ---
// hooks_v38.cpp -- variant G: release the formation beyond the choke (Phase 24, v12 section AO)
//
// Measured in run 20261016_171016 (v37): once per game second FUN_101bc880 copies
// the controller's cursor ctrl+0x34/0x38, advances the copy by D = 11 m along
// the unit path (FUN_103670a0 at 0x101BC983) and lays the slot block with its
// FRONT at that point and the rows BACKWARDS along the path with each node's
// width. In a passage narrower than the frontage the block stretches back over
// the whole column, every man's slot is a few metres ahead of him and the
// pacing law e+0x110 = min(5.08, |pos - slot| / (3.14 s - elapsed)) makes him
// walk: the crawl. When the front reaches the path end the whole block is laid
// at the destination and everybody runs.
//
// G (mode 10, I cycles off -> C -> RC -> G -> off; three high tones) = RC plus:
// at every FUN_101bc880 hit the orders hook walks the unit path from the cursor
// node (next +0x08, follow points +0x80/+0x88, widths +0x74/+0x78), finds the
// runs of nodes narrower than G_NARROW_FRAC x the path's widest node and the
// block depth at that width, and leaves a plan for the thread. The new entry
// hook on FUN_103670a0 (stolen SUB ESP,0x10; PUSH ESI; PUSH EDI), filtered on
// the return address 0x101BC988, reads the game's D from the stack argument:
// if cursor + D falls inside a narrow run, the argument becomes the distance
// to the run's exit + block depth + G_MARGIN_M (or past the path end, which
// is the game's own release), otherwise it is left alone. The order's time
// field (3.14 s) is untouched, so every man's slot is far away and the pacing
// law gives running speed through the choke; the formation re-forms beyond it
// and then follows the leader as before. Open ground: no narrow node, nothing
// changes. Rows: pathfix_capture_v42_release.csv (one per G advance call in
// the window); counters release_* in fix_totals.csv.
//
// --- v37 header follows ---
// hooks_v37.cpp -- v36 fix unchanged; capture of the soldier speed law and the unit leader (Phase 23)
//
// Section AM/AN: at a gate the slot block is laid 11 m + its depth ahead of the
// leader's path projection and the men form into it at a speed proportional to
// their distance from the slot (k ~ 0.1 per game s), the formation front sits
// still until the leader's projection advances, then the block jumps through.
// The static chain (FUN_101b7b70 / FUN_101bc880 / FUN_103df3f0 / FUN_103942f0 /
// FUN_103aa090 / FUN_10366a10) leaves two candidates for the slow approach:
// the pacing branch (speed = min(vmax, dist / time left), needs e+0x440 == 1)
// and the kinematic target speed e+0x150 with the multiplier e+0x1BC. v37
// records both per soldier sample, plus the unit leader index and the
// "all ready" byte per FUN_101bc880 hit, so one gate pass decides.
//
// soldiers.csv gains: spd_max (e+0x110), spd_cur (e+0x130), spd_tgt (e+0x150),
// spd_cmd (e+0x1AC), spd_mul (e+0x1BC), ord_spd (e+0x438), ord_t (e+0x43C),
// ord_mode (e+0x440), elapsed (e+0x46C), moving (e+0xF0), direct (e+0x198),
// f384 (e+0x384), f308 (e+0x308). orders.csv gains leader_idx ([ctrl+0x30]),
// ready ([ctrl+0x0C]), speed_mode ([ctrl+0x24]). Nothing else changes: the
// RC / C variants, keys and the game clock are v36.
//
// --- v36 header follows ---
// hooks_v36.cpp -- variant RC: radius lanes + lead the turn, fix windows in game time (Phase 22)
//
// Run 20261016_121912 (v35 R2, v12 section AJ): R passed its down-pass marks
// (knot 65 % of C, closest approach +0.5..0.6 m) and equals C on the way up,
// where C's lead the turn puts every man at 1.9..2.2 m while R's lane-0 third
// stays at 0.7..1.0 m. C never acts at the down-pass tip (end cap "narrow"),
// R acts there, and the lead point is built from the game's apex, which
// already carries the lane radius, so the two combine without overlap.
//
// v36 RC (mode 9) = v35 R (radius lanes keyed to the unit's busy corner) +
// variant C (lead the turn within 6 m + no crowd damping within 30 m).
// I cycles off -> C (one high tone) -> RC (two high tones) -> off.
//
// Game time (user, 2026-10-16: every test so far ran at the game's 3x speed):
// all fix windows were GetTickCount wall ms and therefore 3.6x longer in game
// time than named (3.6 game s per wall s measured from FUN_101bc880's once
// per game second cadence). v36 reads the engine's tick counter
// ([[ctrl+0x14]+8]+0x50, DAT_115b2448 = 0.1 s per tick; FUN_101b7b70) at the
// FUN_101bc880 hook and measures every fix window in game ms at the tested
// length (wall ms * 3.6), so the fix behaves the same at 1x and 3x. Capture
// timestamps, row rate limits, sampling and keys stay on the wall clock.
// Counters gt_* in fix_totals; the clock object is logged once.
//
// --- v35 header follows ---
// hooks_v35.cpp -- variant R2: radius lanes keyed to the unit's busy corner (Phase 21b)
//
// Run 20261016_102617 (v34): the game's apex moved out exactly with the
// request radius (0.86 / 1.94 / 3.02 m for w 1.2 / 2.7 / 4.2) on both
// directions, men followed the lanes about 2/3, but on the way down the lane
// was set 0.8..2.4 m before the tip: a lane is applied at a replan, replans
// are rare mid-corridor (docs/replan_cadence.txt), and v34 needed the man's
// OWN fresh apex at the tip. Keyed to the unit's busy corner the lane would
// come at ~27 m (docs/lane_timing_check.txt).
//
// v35 R (mode 8) = v34 R with:
//   - corner candidates = the man's own fresh apex vertex plus the unit's
//     busy corners: members' apex vertices (vTick within RQ_UNIT_MS) within
//     30 m of him and ahead of him (towards his slot), clustered by 1.5 m,
//     >= RQ_MIN_MEN members; the candidate with the largest fresh queue
//     (FixLateral n) wins, ties to the own vertex;
//   - a vertex he has passed (behind him, > 2 m) is never used;
//   - the lane is kept for RQ_KEEP_MS = 8 s (was 3 s) so it survives the
//     walk to the corner without replans.
// Rows get a "key" column (own / unit); counters radius_key_own,
// radius_key_unit, radius_passed, radius_candidates.
//
// --- v34 header follows ---
// hooks_v34.cpp -- variant R: dynamic radius lanes (Phase 21)
//
// User idea: when too many men of a unit take one corner line (radius X),
// put the next ones on Y, then Z. Section AF / docs/request_radius_x86.md:
// the soldier's path-request radius w (request+0x2C, FUN_103dfc40() + 0.5)
// sets his funnel apex vertex + m * w/2, inside the game's own path; too
// narrow portals never fail (re-admitted at +100000 cost).
//
// I cycles off -> C -> R -> off (tones: off one low, C one high, R two high).
//   C (mode 4) unchanged.
//   R (mode 8) = radius lanes + C's no crowd damping within 30 m, waypoints
//     untouched (C's lead-the-turn would override the funnel apex).
//   Mid-function patch at 0x103df4d1 (FUN_103df3f0 replan, after CALL
//   FUN_103ccd00; stolen CMP byte [ESP+0xF],1 replayed last by the
//   trampoline, so the JZ sees its flags). EBX = soldier, request Q at
//   hook-point ESP+0x58 (src/stack_offsets_v34.h, tools/stack_calc.py --mid).
//   Per call: frame check (Q+0x18 / +0x20 == soldier slot e+0x420 / +0x428,
//   w plausible; > 10 % failing in the first 200 calls disables the fix).
//   If the man's last apex vertex V is fresh (1.5 s) and >= 8 men of his unit
//   queue for V (FixLateral: n, his rank q across the flow): lane k =
//   floor(q * 3) (kept per man for V within 1.5 m, 3 s); if lane k holds more
//   than 1.3 * n / 3 of the queue, he moves to the ADJACENT lane with fewer
//   men (never two lanes away: no crossing). w = w0 + k * 1.5 m, at most
//   4.2 m, never below w0. Stuck release (FixStuckHold) -> w0.
// Rows: pathfix_capture_v42_radius.csv (5 / s per man); counters radius_*.
//
// --- v33 header follows ---
// hooks_v33.cpp -- variant L: look-ahead avoidance (Phase 20)
//
// Shogun 2 soldiers have no look-ahead: each man walks straight at his
// waypoint and is pushed apart only after bodies overlap (steering_x86.md).
// At a corner the man on the shared waypoint is slow (turning) and the men
// behind walk into his back. Run 20261015_233944 (fix off): men still moving
// 4..16 m before the tip and heading at a stalled man had a free walkable 4 m
// path past him in 92 % of samples; inside the knot 92 % had none. So the
// avoidance acts upstream, for every moving man.
//
// I cycles off -> C -> L -> off (tones: off one low, C one high, L two high).
//   C (mode 4) = v28 C unchanged (lead the turn + no damping within 30 m).
//   L (mode 7) = C + look-ahead avoidance on the integrator entry
//     (FUN_10367600, D = e+0xD8 / +0xE0), every listed soldier:
//   - unit snapshots (positions, velocities from snapshot to snapshot, EMA
//     0.5), rebuilt at most every 100 ms by the first integrator call that
//     finds them stale; per-unit SRW lock, one builder at a time;
//   - only men following a path (waypoint call within 1.5 s, |D| >= 0.05 m);
//   - obstacles: stalled men (< 0.8 m/s, static circles) and men moving
//     another way (moving circles, time to contact); men moving the same way
//     (within 45 deg, >= half my speed) are ignored (SC2: push them); men
//     farther than my own slot are ignored; other units' men count when their
//     snapshot box overlaps;
//   - 13 directions 0, +-10, +-20, +-30, +-45, +-60, +-75 deg; horizon
//     H = min(5 m, distance to my slot); only when the straight line is
//     blocked within H: pick the direction minimising
//     H^2 + f^2 - 2 H f cos(a) (Moussaid 2011), f = free distance;
//   - |D| kept (no braking); no direction with f >= 2 r: D unchanged (push);
//   - side held 0.5 s unless its free distance drops below 0.5 m; an
//     oncoming blocker: pass clockwise (negative angle), both men alike;
//   - wall memory: turned last tick but moved < 40 % of the turned D: that
//     side is blocked for 1 s.
// Rows: pathfix_capture_v42_avoid.csv (5 / s per man, window only);
// counters avoid_* in fix_totals.csv.
//
// --- v31 header follows ---
// hooks_v31.cpp -- variant F "tip lanes" (Phase 19)
//
// User diagram (data/diagram.png): round a sharp or pointy obstacle every man
// is routed through the tip, so all funnel into one point; instead give them
// concentric lanes round the tip. Run 20261015_231130 (v30): at a thin wall
// end (0.7 m thick, corners (-26.0, -31.7) / (-26.0, -31.0)) up to 198 of 198
// men had the identical waypoint; C never touched it (outside its 6 m zone,
// and the 0.7 m end-cap portal is "narrow").
//
// I cycles off -> C -> F -> off (tones: off one low, C one high, F two high).
//   C (mode 4) = v28 C unchanged.
//   F (mode 6) = C plus tip lanes. On an apex call within 30 m of the corner
//     vertex V, if at least TIP_MIN_MEN men of the unit queue for V (fresh apex
//     calls, v22 FixLateral), the man gets a lane: radius R = rP + q * W round
//     V (rP = |apex - V|, the game's own clearance; q = his rank across the
//     flow, 0 inner .. 1 outer; W = min(portal lane, TIP_W_M)). The lane is
//     stored with V, the turning sign and the corner's free sector, and kept
//     while his apex calls stay within TIP_MERGE_M of V (so both corners of a
//     thin wall end count as one tip, including the "narrow" end cap).
//     Waypoint: the point of his circle that lies delta beyond the tangent
//     point seen from him (delta 30 deg when 3 m or more outside the circle,
//     60 deg on or inside it), clamped to the free sector; past the sector
//     the game's waypoint is kept. Concentric arcs keep the rank order, so
//     lanes do not cross. Stuck release as before.
// Counters tip_new, tip_cont, tip_far, tip_near, tip_past, tip_no_queue,
// tip_short_lane, tip_stuck (fix_totals.csv); fix rows: clamped bit 16,
// t = q, tmax = R, tmin = sign, scale = sector half angle.
//
// --- v30 header follows ---
// hooks_v30.cpp -- v28 with I cycling off -> A -> C -> D -> off (Phase 18b)
//
// Run 20261015_221831 (v29): user found D worse than before and E bad; v29 E is
// dropped. v30 is v28 with one more variant on the key, for a new user test:
//   A (mode 1) = v25: lead the turn within 6 m + no crowd damping within 12 m;
//   C (mode 4) and D (mode 5) exactly as in v28.
// Tones: off one low, A one high, C two high, D three high.
//
// --- v28 header follows ---
// hooks_v28.cpp -- v27 with the gather rule fixed (Phase 17c)
//
// Run 20261015_203944 (v27): on INWARD orders with D, men already inside the
// inner yard were sent back towards the gatehouse ("some men were confused and
// try to run outside to join the main blob"). The v27 "has he passed the queue
// corner" test compared only his LAST apex vertex with the queue vertex; the
// queue was at the ramp foot while those men had last rounded the gatehouse
// corner. v28: a man may gather only if he has had NO apex call at all since
// his unit's last move order (he has not started round any corner yet).
// Replayed on the v27 rows: keeps 93 % of the outward gathers, removes the
// inward ones (151 / 168 / 52 men -> 24 / 0 / 1).
//
// --- v27 header follows ---
// hooks_v27.cpp -- variant C as the base, plus D "gather at the queue" (Phase 17b)
//
// I cycles off -> C -> D -> off (tones: off one low, C one high, D two high).
//   C (mode 4) = lead the turn within 6 m + no crowd damping within 30 m of the
//     vertex of a man's current apex (best variant of run 20261015_201332).
//   D (mode 5) = C, plus gather: on a GOAL call (the man walks straight to his
//     slot) while his unit is passing a corner, if his slot is more than
//     GATHER_BEHIND_M farther from that corner's vertex than he is, he has not
//     rounded that vertex since his unit's last move order, and he is within
//     GATHER_ZONE_M of it, his waypoint becomes a point GATHER_STEP_M towards
//     the corner apex the unit queues for (instead of walking back to a slot
//     behind him). The "queue corner" is the vertex shared by most of the
//     unit's men with an apex call in the last FIX_GROUP_MS (at least
//     GATHER_MIN_MEN). The unit's last move order time comes from the
//     FUN_10128570 hook (ECX + 0x60 holds a plausible width only on real
//     orders). v21 stuck release kept.
// Why (run 20261015_201332, user: "men at the back wander further away from
// the gate while waiting"): on outward passes about 150 of 200 men wait; they
// walk a median 8..15 m more than the straight distance to the gate, 20..40
// men move more than 1 m away from it, always walking to their own slot, which
// the re-laid column puts about 3 m farther from the gate than they stand.
// Counters gather_* (fix_totals.csv). Variants A (mode 1) and B (mode 3) of
// v26 are no longer on the key.
//
// --- v26 header follows ---
// hooks_v26.cpp -- v25 plus two tuning variants, chosen live with I (Phase 17)
//
// I cycles the fix: off -> A -> B -> C -> off (tones: off one low, A one high,
// B two high, C three high). The mode number in the events file tells the
// variant of each order (mode 1 = A, 3 = B, 4 = C).
//   A (mode 1) = v25 unchanged: lead the turn within FIX_LEAD_ZONE_M (6 m) of
//     the corner vertex; no crowd damping for men within STEER_ZONE_M (12 m)
//     of the vertex of their current apex.
//   B (mode 3) = A, plus (1) "far lanes": a man between FIX_LEAD_ZONE_M and
//     FIX_FAR_ZONE_M (30 m) from his apex vertex keeps his own lane: the
//     waypoint slides from the apex along its portal lane to the projection of
//     his position (v18 mode 2 geometry, capped at FIX_FAR_LANE_M, v21 turn
//     limit and stuck release kept); and (2) no crowd damping up to
//     STEER_ZONE_FAR_M (30 m) from the vertex.
//   C (mode 4) = A plus (2) only, to tell the two changes apart.
// Why (run 20261015_193946, v12 section V): at the ramp foot a third of the
// men in the dense spot already aim at the NEXT corner (gatehouse, 27 m
// away), outside both v25 zones: they all head for one apex point (the end of
// a 9.9 m portal across the ramp top) and are still crowd-damped. The fix-on
// ramp column used 65..74 % of the ramp's 4.7 m.
// Fix rows: clamped bit 8 marks a far-lane waypoint. Counters far_lane,
// fix_variant (fix_totals.csv).
//
// --- v25 header follows ---
// hooks_v25.cpp -- v23 plus "keep pushing": no crowd damping near corners (Phase 16b)
//
// v25 = v24 with the look-ahead avoidance switched OFF (STEER_AVOID false) and
// the crowd damping switched off instead, for the same men (fix on, waypoint
// a corner apex within STEER_APEX_MS, within STEER_ZONE_M of its vertex).
// Source: the StarCraft 2 talk (J. Anhalt, GDC 2011; user transcript
// data/runs/GDC Vault - AI Navigation- It's Not a Solved Problem - Yet.txt):
// avoidance ignores units moving in the same general direction and idle
// units; units push each other, because moving in a straight line is the
// fastest way. Shogun 2's integrator FUN_10367600 scales a man's own move by
// max(3 / ratio, 0.1) when gross / net push > 3 and the byte e+0xD6 is 1 (set
// to 1 at the end of every tick, no other reader found). At the integrator
// entry v25 writes e+0xD6 = 0 for the selected men, so the damping is skipped
// for them on that tick; pushes, the 2 m cap and hard collision are unchanged.
// Run 20261015_172654 (v24): avoidance between men of one unit cut the ramp
// flow 18 -> 12 men/s and slowed form-up 6..9 s although the knot shrank.
// Counter: steer_nodamp (fix_totals.csv), log line "Steering: ...".
//
// --- v24 header follows ---
// hooks_v24.cpp -- v23 plus StarCraft-2-style lookahead steering (Phase 16)
//
// v24 = v23 (mode 1 "lead the turn" waypoint rule, unchanged) plus a per-tick
// steering layer while the fix is on (any mode):
//   - entry hook on the integrator FUN_10367600 (fastcall, ECX = soldier e;
//     prologue PUSH EBP / MOV EBP,ESP / AND ESP,-16, docs/prologues_v24.txt).
//     At its entry e+0xD8 / +0xE0 hold the soldier's own desired move for this
//     tick (FUN_103aa090 adds it), before the soft-collision push is added
//     (docs/steering_x86.md);
//   - only soldiers whose current waypoint is a corner apex (fix table: apex
//     call within STEER_APEX_MS) and who stand within STEER_ZONE_M of that
//     corner vertex;
//   - candidate directions = D rotated by 0, +-15 .. +-75 deg; free = distance
//     along the ray before it meets another man of his unit (disc of radius
//     R = 2 * e+0x58), capped at STEER_H_M; score = free * cos(angle) +
//     STEER_BIAS * (u . o), o = unit vector from the corner vertex to the
//     man, made perpendicular to D (the outside of the turn);
//   - D is replaced by the best direction with the same length. No wall
//     test: the game's hard collision slides along walls (the offline model,
//     sim/steer.py "H corner nowall", gave the same result as with one).
//   Model: sim/run_gate2.py on the gatehouse ramp (docs/ghidra_analysis_v12.md
//   section Q). Totals: steer_* rows in pathfix_capture_v42_fix_totals.csv.
//
// --- v23 header follows ---
// hooks_v23.cpp -- "lead the turn" corner rule (Phase 15)
//
// v23 = v22 with mode 1 replaced by the rule chosen for unit cohesion (time
// until every man is in his slot; docs/ghidra_analysis_v12.md sections L..N,
// sim rule "R12 v23 final"):
//   - within FIX_LEAD_ZONE_M of the corner vertex V, the waypoint is a point
//     on a circle round V, FIX_LEAD_DEG further round the corner than the
//     man stands (so he curves through the turn instead of stopping at the
//     shared corner point and turning on the spot);
//   - turning direction fixed per man and corner when he first gets a lead
//     point there (side of V seen from the man looking at the game's point P;
//     on P itself, the side of the corner bisector), kept while the corner is
//     unchanged (FIX_LEAD_KEEP_MS);
//   - the point stays inside the corner's free sector: half-span =
//     90 + acos(1 / |m|) deg round the bisector (m = the game's vertex offset
//     vector, (P - V) / r), minus FIX_LEAD_MARGIN_DEG; a man already beyond
//     the far edge keeps the game's point;
//   - radius = his own distance from V, within [|P - V|, |P - V| + min(4 m,
//     portal lane)]; if another man of the unit stands within FIX_LEAD_OCC_M
//     of the point, radii +-0.7 / +-1.4 m are tried and the first free one is
//     used (distinct, unoccupied targets: stragglers were measured stalling at
//     an occupied shared corner point);
//   - the v21 stuck release stays; outside the zone the game's point.
//   Fix rows, mode 1: t = turn added (deg), t_i = man's angle from the
//   bisector (deg), t_min = turning sign, t_max = radius used, scale = sector
//   half-span used (deg); clamped bit 1 sector clamp, bit 2 no free point,
//   bit 4 radius cut by the lane. New statuses outside_zone, past_sector.
//
// --- v22 header follows ---
// hooks_v22.cpp -- lateral-rank corner rule (Phase 14)
//
// v22 = v21 with mode 1 replaced by the rule chosen in the offline crowd
// model (sim/, docs/ghidra_analysis_v12.md section H, "lateral rank"):
//   - every apex call stores the soldier's game apex P and corner vertex V
//     (side 1: vertex A, side 2: vertex B) with a timestamp;
//   - group = the soldier and the men of his unit whose stored corner is within
//     FIX_GROUP_V_M of V, stored within FIX_GROUP_MS, and who stand within
//     FIX_GROUP_ZONE_M of V;
//   - flow d = normalised mean of the members' unit vectors X -> P; lane u =
//     perpendicular to d, pointing away from the corner ((P - V) . u > 0);
//   - lambda = (X - V) . u; q = rank of the soldier's lambda in the group /
//     (n - 1); waypoint = P + u * q * Lc, Lc = min(FIX_LAT_LANE_M, portal lane
//     length) (no navmesh ray cast: the portal at the apex bounds the width);
//   - no turn limit in mode 1 (the lane is across the flow; the model showed
//     the limit removes most of the benefit); the v21 stuck release stays.
//   Mode 2 (own position) is unchanged from v21 (with the turn limit).
//   Fix rows, mode 1: t_i = lambda, t_min = q, t_max = group size, scale = Lc,
//   t = shift; clamped bit 1 = Lc below FIX_LAT_LANE_M.
//
// --- v21 header follows ---
// hooks_v21.cpp -- v20 corner waypoint fix plus two guards (Phase 13)
//
// v21 = v20 with:
//   - turn limit: the moved waypoint may point at most FIX_TURN_DEG away from
//     the game's own waypoint as seen from the soldier. The allowed shift is
//     tan(t) * |a|^2 / (|a x u| - tan(t) * (a . u)), a = apex - soldier,
//     u = lane direction, so it shrinks to zero as the soldier reaches the
//     corner. Run 20261015_094109: men at the corner were sent 15 m back
//     along a 24 m fan edge, into the oncoming unit, and stood 10..28 s.
//   - stuck release: a soldier whose position has not moved FIX_STUCK_MOVE_M
//     within FIX_STUCK_MS of continuous apex calls gets the game's waypoint
//     back for FIX_HOLD_MS (status stuck_release).
//   - clamped column: bit 1 lane end, bit 2 turn limit.
//
// --- v20 header follows ---
// hooks_v20.cpp -- corner waypoint fix for every side-object type (Phase 12d)
//
// Built on hooks_v19.cpp (keys I / O / P, tones). Run 20261014_230445 (two
// battles, infantry through a gate): 110 of 120 men got the identical gate
// waypoint with the fix off, and 97 of 120 still did with it on, because the
// gate's side objects are types 1 / 2, which v18 / v19 skipped (type_other
// 4812 of 10416 rows). v20 takes both candidates from the game's own helpers
// FUN_10393000 (left) and FUN_103a0080 (right), called as FUN_10394e80 calls
// them, so every type is handled with the game's exact points; for type 0 the
// v18 formula is kept as a cross-check (first 200 calls, > 2 % disagreeing
// disables the fix). A type-0 other end with a zero offset vector is pulled
// in by r along the edge instead of being skipped. Statuses: type_other is
// now no_helpers (the helpers failed the install byte check), other_end is
// unused (counter other_pulled instead).
//
// Phase 12c text of hooks_v19.cpp follows.
//
// hooks_v19.cpp -- corner waypoint fix v18 with letter keys and audible tones (Phase 12c)
//
// Built on hooks_v18.cpp; the fix itself is unchanged. Changes (user
// instruction after run 20261014_225015: no F-number keys, use I, O, P; no
// sound was heard from Beep()):
//   Keys: I = corner fix on / off, O = marker, P = finish. Real keys count
//   only while a window of this process has the focus (GetAsyncKeyState
//   sees keys typed into other programs). The lane mode and the window
//   extension have no key any more (harness bits only); lane mode stays 1.
//   Sound: tones are played with PlaySound from in-memory WAVs on the normal
//   audio device (Beep() was silent): one low tone = fix off (also played
//   once when the capture window opens, as a sound check), one high tone =
//   fix on, three long low tones = ALARM (fix disabled, or I refused).
//   Window 900 s (no extension key); soldier row buffer 512 K, go-to 256 K.
//
// Phase 12b text of hooks_v18.cpp follows.
//
// hooks_v18.cpp -- corner waypoint fix, second prototype + v16 capture (Phase 12b)
//
// Phase 12b (docs/PHASE12b_fix_plan.md; run 20261014_221943 showed the v17
// handler's portal model was wrong, docs/ghidra_analysis_v11.md F2). Built on
// hooks_v17.cpp; changes:
//
//   Fix geometry (FUN_10393000 / FUN_103a0080, side-object type 0): the
//   left candidate is pA = A + mA * r and the right one pB = B + mB * r,
//   A = E+0x0C/+0x10, mA = E+0x24/+0x28, B / mB the same fields of [E+8],
//   E = [portal+0x20], type = [portal+0x08]. m is a per-vertex offset (miter)
//   vector, |m| >= 1; E+0x30 is a clamp limit, not the length. The lane is
//   the segment from the apex to the other candidate (the portal shrunk by r
//   at both ends, both ends exactly as the game computes them): the
//   waypoint slides along it by s in [0, L], mode 1 by the slot offset across
//   the unit (compressed when the unit is wider than L), mode 2 to the
//   projection of the soldier's position. Types other than 0 are skipped.
//   Self-checks: frame on every call; on the first 200 type-0 apex calls the
//   apex side's |m| in [0.95, 12] and r in (0, 25) (geometry, > 2 % failing
//   disables) and the waypoint within 2 cm of pA or pB (apex match, > 10 %).
//   Keys: F6 fix on / off (OFF at launch), F7 lane mode 1 <-> 2 (the
//   watchpoint key is gone; the watchpoint stays off). Disabling, and every
//   refused key, sounds an alarm of three long low beeps.
//   Capture: fix rows carry type, mA, mB, E+0x30, pA, pB, L; the first 64
//   distinct apex portals are dumped raw (pathfix_capture_v42_fixraw.csv);
//   soldier samples are also taken from FUN_101bc880 and the fix hook, so a
//   unit is sampled even when FUN_101a9750 is silent (R1 of run 22:19).
//
// Phase 12 text of hooks_v17.cpp follows.
//
// hooks_v17.cpp -- corner waypoint fix prototype + v16 capture (Phase 12)
//
// Phase 12 (docs/PHASE12_fix_plan.md). Built on hooks_v16.cpp; everything of
// v16 is kept except the width override presets (removed; F6 now switches the
// fix) and the file names (pathfix_capture_v42_*).
//
//   Fix: a mid-function patch at 0x103df6c0 (FUN_103df3f0, the instruction
//   after CALL FUN_10394e80; stolen CMP byte ptr [EBX+0x471],1, 7 bytes).
//   There EAX = the waypoint buffer {x, y, z, u16, float} that is copied to
//   soldier+0x454 right after, EBX = soldier, [ESP+0x14] = the portal of the
//   returned apex (0 when the slot itself was returned) and ESP+0x44 = the
//   buffer (src/stack_offsets_v17.h, tools/stack_calc.py --mid). When the
//   waypoint is a corner apex (portal != 0, no special link), it is moved
//   along the same portal edge to the soldier's lane:
//     mode 1 (slot lane): by the soldier's slot offset across the unit,
//       measured along the portal from the innermost slot, compressed when
//       the unit is wider than the portal;
//     mode 2 (own position): to the projection of the soldier's position.
//   The point is clamped to [r, len - r] on the portal, so it stays on the
//   navmesh edge the follower expects to cross. Only x and z of the buffer
//   are written. F6 cycles 0 off (at launch) -> 1 -> 2 -> 0, with beeps.
//   Live self-checks (frame: EAX == ESP+0x44 on every call; portal geometry
//   and apex match on the first 200 apex calls) switch the fix off for the
//   session when they fail ("FIX DISABLED" in the log).
//   Capture: pathfix_capture_v42_fix.csv (one row per apex call, at most
//   10 Hz per soldier, while the window is open), fix_totals.csv, FIXMODE
//   rows in the events file, and soldiers.csv gains s_corridor = [R+0x0C]
//   and s_radius = [R+0x50] (R = [s+0x3CC]).
//
// Phase 11 text of hooks_v16.cpp follows.
//
// hooks_v16.cpp -- soldier trace, per-run folders, crash capture (Phase 11 capture)
//
// Phase 11 (docs/PHASE11_steering_plan.md, section 4). Built on hooks_v15.cpp;
// all v15 hooks and records are kept, except as listed here.
//
//   A0 (no file handling by the user):
//   - Every launch writes into its own new folder pathfix_runs\<stamp>\ next to
//     the game exe (src/log.cpp, PATHFIX_RUN_FOLDERS); the log is there too.
//   - Record files are appended every 10 s while the window is open (only
//     completed records; nothing is rewritten), summary files (totals,
//     watch_totals, faults) are rewritten through <name>.tmp + rename. A crash
//     therefore leaves at most the last 10 s unwritten.
//   - Crash capture: an unhandled-exception filter (re-asserted every second,
//     chained to whatever filter the game installs) logs the fault, writes
//     crash.dmp (MiniDumpWriteDump, dbghelp loaded at install) and flushes all
//     files. A first-chance handler keeps the last 256 access violations and
//     other fatal codes raised inside empire.retail.dll (faults.csv).
//   - No config file is needed. F6 cycles the width override preset
//     (off -> set 49.0 all -> set 2.0 all -> off; an optional
//     pathfix_presets.txt written by Claude replaces the list), F7 toggles the
//     node watchpoint (OFF at start; pathfix_watch.txt may still set its
//     target). pathfix_override.txt is ignored. Each change is a PRESET /
//     WATCH row in the events file.
//   A1 (soldier trace):
//   - FUN_101bc880 entry (per-soldier orders, ~1/s per controller): ECX =
//     controller, unit = [ctrl+4], the soldier array [unit+0x1C] (count
//     [unit+0x18]) in its order at entry (soldier i is sent to slot i), and
//     the controller cursor [ctrl+0x34] / [ctrl+0x38].
//   - FUN_103dbbf0 entry (soldier go-to-slot): ECX = soldier, [ESP+4] = the
//     0x2C-byte order {x, y, z, region, aux, ushort heading at +0x14, ...}.
//   - FUN_103dafb0 entry (slot off the navmesh): ECX = soldier.
//   - From the FUN_101a9750 hook, per unit = [this+4], every 100 ms while any
//     soldier moved since the last row, else every 5 s: per soldier s the
//     position s+0x48..+0x50, heading (u16) s+0x64, location s+0x524 / +0x52C,
//     and the go-to fields on s and on e = [s+0x2F0]: move state +0x3BC, path
//     result +0x3CC, destination +0x420 / +0x428, current waypoint +0x454 /
//     +0x45C. Static reading (docs/soldier_steering_x86.md) and the replan
//     code FUN_103df3f0 suggest e == s; both are recorded and the log reports
//     which held.
//   - Window 600 s (F9 +600 s).
//   - Second build (2026-10-14, after run 20261014_203329): pointer checks go up
//     to the system's highest user-mode address (Shogun2.exe is
//     large-address-aware); the first build stopped at 0x7FFEFFFF and lost every
//     soldier allocated above 2 GB.
//
// Phase 10 text of hooks_v15.cpp follows.
//
// Phase 10 (docs/PHASE10_placement_plan.md, section 4). Built on hooks_v14.cpp;
// all v14 behaviour is kept except:
//
//   - Override (A1): pathfix_override.txt is `set <float> [all|player]`,
//     default `all`. `all` overwrites +0x74 / +0x78 of every walked node of
//     every build while the window is open (AI builds included). `player`
//     keeps the v14 rule (builds within 500 ms after a constructor hit).
//   - Watchpoint (A2): default target `node+0x74 rw` (read or write, 4 bytes).
//     A node-based slot follows the cursor node of its mover: when the
//     per-tick hook sees the cursor node change, the slot is re-pointed at the
//     new node + offset and the watch thread re-arms at once (event, not the
//     500 ms period). Stale traps on a previous address are swallowed without
//     a row. Traps whose EIP lies inside this proxy DLL (its own dumps and
//     reads) are dropped (counted). Rows carry is_read: value unchanged and
//     EIP never seen changing a watched value. Slots are keyed by the
//     FUN_101a9750 this pointer, de-duplicated by address; a slot not
//     refreshed for 1000 ms may be taken over by another mover (the v14 run
//     showed 9 this pointers for 2 units, so "one slot per this" would leave
//     the later movers unwatched).
//   - Builder node rows (A3): side (+0x0C), base points (+0x14..+0x20) and
//     followed points (+0x80..+0x8C) decoded into columns.
//   - Files are named pathfix_capture_v15_*; new pathfix_capture_v15_slots.csv
//     (every slot assignment / re-point).
//
// Phase 9 text of hooks_v14.cpp follows.
//
// Phase 9 (docs/PHASE9_builder_hook_plan.md). Built on hooks_v13.cpp; all v13
// behaviour is kept (group 1 entry hooks, per-tick cursor hook, F8/F9/F10,
// watchpoint). Changes:
//
//   - Group 1 rows record the first EIGHT stack dwords (offsets from
//     src/stack_offsets_v14.h, generated by tools/stack_calc.py with --args 8)
//     and dword [ECX+0x60] at entry. For FUN_102557e0 (move order constructor)
//     the decompile stores param_5 (stack argument 4) at moveOrder+0x60: the
//     requested width is arg4.
//   - Builder hook, entry of FUN_1039bb40 (__thiscall, RET 0xC):
//         ECX = builder, arg1 = P (cursor block), arg2 = node list head, arg3
//     It walks the list from the head through +0x08 and writes one row per node
//     (address, next, +0x74, +0x78) with a 0xA0-byte dump. Stops on null, a
//     pointer outside 0x00010000..0x7FFEFFFF, an unreadable node, a node already
//     visited in this walk, or 32 nodes. Rate limited like group 1.
//   - Per-tick cursor hook: node dumps are 0xA0 bytes (the node size); the P
//     dump stays 0x100.
//   - Width override (Task E), OFF unless pathfix_override.txt exists next to
//     the game exe: on a build within 500 ms after a move order constructor
//     hit, +0x74 and +0x78 of every walked node are set to the configured value
//     before FUN_1039bb40 reads them; old and new values are recorded.
//
// Without pathfix_override.txt nothing here writes to game memory other than
// the entry patches.

#include "log.h"
#include "hook.h"       // proxy/: shared hook engine
#include "gamemem.h"    // proxy/: guarded reads and writes, user-mode ceiling
#include "keys.h"       // proxy/: key primitives and tones (dev build)
#include "stack_offsets_v16.h"
#include "stack_offsets_v17.h"
#include "stack_offsets_v34.h"
#include <dbghelp.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <cstdlib>
#include <cmath>
#include <float.h>

static uint32_t g_moduleBaseVal = 0;
static uint32_t g_moduleSize = 0;            // SizeOfImage, to tell in-module callers from others
static uint32_t g_selfStart = 0, g_selfEnd = 0;   // this proxy DLL's image range
// Highest user-mode address: Mem_UserMax() (proxy/gamemem.h, GetSystemInfo at
// install). Shogun2.exe is large-address-aware, so on 64-bit Windows the heap
// reaches 0xFFFEFFFF; the first v16 build (20:22) assumed 0x7FFEFFFF and
// skipped every soldier above 2 GB (about 20 percent of the rows of run
// 20261014_203329).

// ---------------------------------------------------------------------------
// Hook table
// ---------------------------------------------------------------------------

enum {
    H_PARSER = 0,      // FUN_101f6bf0
    H_MOVEORDER,       // FUN_102557e0
    H_CMDPROC,         // FUN_1010bb50
    H_UNITSETUP,       // FUN_1035ae20
    H_COLLISION,       // FUN_1037a850
    H_UI_WIDTH,        // FUN_1020caa0
    H_UI_ANGLE,        // FUN_1020cc30
    H_TICKCALLER,      // FUN_10128570
    H_CURSOR,          // FUN_101a9750 (group 2)
    H_BUILDER,         // FUN_1039bb40 (path builder, list walk)
    H_ORDERS,          // FUN_101bc880 (per-soldier orders, v16)
    H_GOTO,            // FUN_103dbbf0 (soldier go-to-slot, v16)
    H_OFFMESH,         // FUN_103dafb0 (slot off the navmesh, v16)
    H_WAYPOINT,        // 0x103df6c0 in FUN_103df3f0 (waypoint post-process, v17 fix)
    H_STEER,           // FUN_10367600 (per-tick integrator, v24 steering)
    H_RADIUS,          // 0x103df4d1 in FUN_103df3f0 (replan request radius, v34 R)
    H_ADVANCE,         // FUN_103670a0 entry (cursor advance before the slot layout, v38 G)
    HOOK_COUNT
};
static const uint8_t HOOK_MARKER = 0xFF;
static const uint8_t HOOK_PRESET = 0xFE;     // v16 F6 override preset change (not written by v17)
static const uint8_t HOOK_FIXMODE = 0xFC;    // v17 F6 fix mode change / fix disabled (events file)
static const uint8_t HOOK_WATCHTOGGLE = 0xFD; // F7 watchpoint on / off (events file)

// HookDef / HookSlot and the install steps: proxy/hook.h (shared hook engine).
// relocOff = offset of an absolute address inside the stolen bytes (the loader
// rebases it), or -1.

// Bytes from docs/prologues_v16.txt (tools/dump_prologue.py on s2x86, image
// base 0x10000000; checked against the on-disk DLL by tools/check_prologues.py).
// All fourteen: RELOCATABLE yes, no branch into the range.
static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_101f6bf0", "script move parser",           0x1F6BF0, 6,
      { 0x81, 0xEC, 0xAC, 0x00, 0x00, 0x00 }, -1 },                       // SUB ESP,0xAC
    { "FUN_102557e0", "move order constructor",       0x2557E0, 5,
      { 0x56, 0x8B, 0x74, 0x24, 0x08 }, -1 },                             // PUSH ESI; MOV ESI,[ESP+8]
    { "FUN_1010bb50", "unit command processor",       0x10BB50, 11,
      { 0x83, 0xEC, 0x60, 0xC7, 0x44, 0x24, 0x5C, 0x00, 0x00, 0x00, 0x00 }, -1 },
    { "FUN_1035ae20", "unit setup",                   0x35AE20, 6,
      { 0x81, 0xEC, 0x50, 0x01, 0x00, 0x00 }, -1 },                       // SUB ESP,0x150
    { "FUN_1037a850", "collision setup",              0x37A850, 8,
      { 0xF3, 0x0F, 0x10, 0x25, 0xC8, 0xC9, 0x5A, 0x11 }, 4 },            // MOVSS XMM4,[0x115AC9C8]
    { "FUN_1020caa0", "UI Move Orientation Width",    0x20CAA0, 9,
      { 0x83, 0xEC, 0x34, 0x56, 0x68, 0xCE, 0x7C, 0x5C, 0x11 }, 5 },      // ...; PUSH 0x115C7CCE
    { "FUN_1020cc30", "UI Move Rotation Angle",       0x20CC30, 9,
      { 0x83, 0xEC, 0x34, 0x56, 0x68, 0xE3, 0x7C, 0x5C, 0x11 }, 5 },      // ...; PUSH 0x115C7CE3
    { "FUN_10128570", "caller of FUN_101a9750",       0x128570, 5,
      { 0x83, 0xEC, 0x60, 0x56, 0x57 }, -1 },
    { "FUN_101a9750", "per-tick cursor loop",         0x1A9750, 6,
      { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, -1 },                       // PUSH EBP; MOV EBP,ESP; AND ESP,-8
    { "FUN_1039bb40", "path builder (list walk)",     0x39BB40, 6,
      { 0x81, 0xEC, 0x8C, 0x01, 0x00, 0x00 }, -1 },                       // SUB ESP,0x18C (docs/prologues_v15.txt)
    { "FUN_101bc880", "per-soldier orders",           0x1BC880, 6,
      { 0x81, 0xEC, 0xC8, 0x00, 0x00, 0x00 }, -1 },                       // SUB ESP,0xC8
    { "FUN_103dbbf0", "soldier go-to-slot",           0x3DBBF0, 5,
      { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, -1 },                             // PUSH EBX/ESI/EDI; MOV EDI,ECX
    { "FUN_103dafb0", "slot off the navmesh",         0x3DAFB0, 6,
      { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x28 }, -1 },                       // PUSH ESI; MOV ESI,ECX; MOV EAX,[ESI+0x28]
    // v17: mid-function, not an entry (docs/prologues_v17.txt,
    // docs/listings/prologues_phase12_x86.txt: RELOCATABLE yes, no branch in).
    { "FUN_103df3f0+0x2D0", "waypoint post-process (fix)", 0x3DF6C0, 7,
      { 0x80, 0xBB, 0x71, 0x04, 0x00, 0x00, 0x01 }, -1 },                 // CMP byte ptr [EBX+0x471],0x1
    // v24: integrator entry (docs/prologues_v24.txt: RELOCATABLE yes, no branch in).
    { "FUN_10367600", "integrator (v25 no damping)", 0x367600, 6,
      { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 }, -1 },                       // PUSH EBP; MOV EBP,ESP; AND ESP,-16
    // v34: mid-function after CALL FUN_103ccd00 (docs/request_radius_x86.md section 6;
    // docs/listings/reqradius_listing_a_x86.txt: no branch into 0x103df4d2..d5).
    { "FUN_103df3f0+0xE1", "replan request radius (v34 R)", 0x3DF4D1, 5,
      { 0x80, 0x7C, 0x24, 0x0F, 0x01 }, -1 },                             // CMP byte ptr [ESP+0xF],0x1
    // v38: FUN_103670a0 entry (scratchpad prologue_103670a0.txt from tools/dump_prologue.py:
    // SUB ESP,0x10; PUSH ESI; PUSH EDI; RELOCATABLE yes, no branch into the range).
    { "FUN_103670a0", "cursor advance (v38 G release)", 0x3670A0, 5,
      { 0x83, 0xEC, 0x10, 0x56, 0x57 }, -1 },
};

static HookSlot g_hookSlots[HOOK_COUNT] = {};

// Trampoline pointers, one per hook. The naked detours jump through these.
extern "C" uint8_t* g_trampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Capture buffers
// ---------------------------------------------------------------------------

enum {
    REASON_BURST    = 1,   // event: one of the first 20 hits after 2 s of silence
    REASON_QUOTA    = 2,   // event: within the 50 per second quota
    REASON_NEW_THIS = 1,   // cursor: first call seen for this this
    REASON_NODE     = 2,   // cursor: node differs from the last record for this this
    REASON_PERIODIC = 4,   // cursor: every 60th call for this this
};

enum {
    READ_P_OK       = 1,   // [this+0x18]
    READ_NODE_OK    = 2,   // [P+0xA8], [P+0xAC]
    READ_FLAG_OK    = 4,   // byte [this+0x74]
    READ_NEXT_OK    = 8,   // [node+0x8]
    READ_DUMP_FULL  = 16,  // a dump was requested and no dump slot was free
};

struct EventRecord {           // group 1 hits and F8 markers
    volatile LONG valid;
    uint8_t  hook;
    uint8_t  reason;
    uint16_t markerNo;
    uint32_t seq;
    uint32_t timestamp;        // ms since window open
    uint32_t threadId;
    uint32_t ecx;
    uint32_t args[8];          // stack arguments 1..8 at entry ([ESP+4] ..)
    uint32_t ecx60;            // dword [ECX+0x60] at entry (pre-value)
    uint8_t  ecx60ok;
    uint32_t retAbs;           // absolute return address (RVA computed at write time)
    uint32_t hitNo;            // per-hook hit counter (1-based) at this hit
};

struct CursorRecord {          // group 2
    volatile LONG valid;
    uint8_t  reason;
    uint8_t  readFlags;
    uint8_t  flag74;
    uint8_t  pad;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t thisPtr;
    uint32_t arg1;
    uint32_t retAbs;           // absolute return address (RVA computed at write time)
    uint32_t callNo;           // per-this call counter (1-based)
    uint32_t P;
    uint32_t node;
    uint32_t progressBits;
    uint32_t nodeNext;
    int32_t  dumpIdx;          // -1 when no dump stored
};

static const int NODE_BYTES = 0xA0;    // path node size (pool stride, Phase 9 Task 0)
static const int P_BYTES = 0x100;      // cursor block stride
struct DumpSlot {
    uint32_t nodeLen;
    uint32_t pLen;
    uint8_t  node[NODE_BYTES];
    uint8_t  p[P_BYTES];
};

static const int EVENT_SIZE  = 32 * 1024;    // v15: 128 K; v15 runs used under 100
static const int CURSOR_SIZE = 32 * 1024;    // v15: 128 K; v15 runs used under 100
static const int DUMP_SIZE   = 4096;

static EventRecord*  g_events = nullptr;
static CursorRecord* g_cursor = nullptr;
static DumpSlot*     g_dumps  = nullptr;
static volatile LONG g_eventIdx = 0;
static volatile LONG g_cursorIdx = 0;
static volatile LONG g_dumpIdx = 0;
static volatile LONG g_seq = 0;

static volatile DWORD g_captureStart = 0;
static volatile DWORD g_captureEnd = 0;       // ms offset at which the window closes
static volatile LONG  g_state = 0;            // 0 idle, 1 armed, 2 capturing, 3 done
static bool g_captureOn = false;              // v41: PATHFIX_CAPTURE=1 at process start (LogCaptureMode); else no recording
static volatile LONG g_recActive = 0;         // v42: a P recording is on (play mode, no PATHFIX_CAPTURE)
static int  g_recNumber = 0;                  // v42: recordings started this session
static bool g_recBuffers = false;             // v42: capture buffers allocated (at the first P, kept for the session)
static volatile LONG  g_layoutBad = 0;        // stack layout self-check failed
static volatile LONG  g_eventOverflow = 0;
static volatile LONG  g_cursorOverflow = 0;
static volatile LONG  g_markerCount = 0;

static const DWORD CAPTURE_MS = 900000;       // v19: 15 minutes, no extension key (v15: 300 s, v16..v18: 600 s)

// ---------------------------------------------------------------------------
// Per-hook rate limiter and counters
// ---------------------------------------------------------------------------

static const DWORD SILENCE_MS = 2000;
static const int   BURST_HITS = 20;
static const int   PER_SECOND = 50;

struct RateState {
    volatile LONG lock;
    bool     everHit;
    DWORD    lastHit;          // GetTickCount of the previous hit
    int      burstLeft;
    DWORD    secBucket;
    int      secCount;
    uint32_t totalHits;        // since install
    uint32_t windowHits;       // while the window was open
    uint32_t recorded;
};
static RateState g_rate[HOOK_COUNT] = {};

static void SpinLock(volatile LONG* l)   { while (InterlockedExchange(l, 1) != 0) { YieldProcessor(); } }
static void SpinUnlock(volatile LONG* l) { InterlockedExchange(l, 0); }

// Counts the hit and decides whether a group 1 hit is recorded.
// Returns 0 (count only), REASON_BURST or REASON_QUOTA; *hitNo gets the counter.
static int RateDecide(int h, DWORD now, bool inWindow, uint32_t* hitNo) {
    RateState& s = g_rate[h];
    int result = 0;
    SpinLock(&s.lock);
    s.totalHits++;
    *hitNo = s.totalHits;
    if (!s.everHit || now - s.lastHit > SILENCE_MS) s.burstLeft = BURST_HITS;
    s.everHit = true;
    s.lastHit = now;
    if (inWindow) {
        s.windowHits++;
        DWORD bucket = now / 1000;
        if (bucket != s.secBucket) { s.secBucket = bucket; s.secCount = 0; }
        if (s.burstLeft > 0) {
            s.burstLeft--;
            s.secCount++;
            result = REASON_BURST;
        } else if (s.secCount < PER_SECOND) {
            s.secCount++;
            result = REASON_QUOTA;
        }
        if (result) s.recorded++;
    }
    SpinUnlock(&s.lock);
    return result;
}

// ---------------------------------------------------------------------------
// Per-this state for the cursor hook
// ---------------------------------------------------------------------------

static const int THIS_TABLE_SIZE = 64;
struct ThisState {
    uint32_t thisPtr;
    uint32_t lastNode;
    uint32_t calls;
    uint32_t P;                // P at first sight
    uint32_t nodeAtFirst;
    volatile LONG announced;   // logged by the dump thread
    int      watchSlot;        // watchpoint slot for this unit, -1 none yet
};
static ThisState g_this[THIS_TABLE_SIZE] = {};
static int g_thisUsed = 0;
static volatile LONG g_thisLock = 0;
static volatile LONG g_thisOverflow = 0;

// ---------------------------------------------------------------------------
// Guarded reads. Every memory read a handler makes goes through one of the
// shared helpers in proxy/gamemem.h: ReadDword / ReadByte (SEH, no range
// check, 0 on a fault), MemcpyTry (range-checked), and for the 0x100-byte
// dumps RangeReadable / CopyGuarded (every page committed, readable and not a
// guard page first: a swallowed guard-page fault would break that page's
// purpose). Writes: WriteByteTry / WriteFloatTry (range-checked).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Hardware watchpoint (docs/PHASE8_addendum_watchpoint.md; Phase 10 A2)
//
// Up to four slots. Target = base + offset, base being P = [this+0x18]
// (the v13 forms, sticky: assigned once per unit, never moved) or the cursor
// node [P+0xA8] (default node+0x74: the slot FOLLOWS the cursor node; when the
// per-tick hook sees a new node, the slot is re-pointed and the watch thread
// re-arms at once). Access: read or write (Dr7 RW = 11b, `rw`) or write only
// (RW = 01b, `w`); LEN = 4. A background thread arms Dr0..Dr3 / Dr7 on every
// thread of the process. A first-position vectored exception handler records
// each access: EIP after the accessing instruction, the registers, old and new
// value, is_read, and the first eight dwords on the stack that fall inside
// empire.retail.dll .text (candidate return addresses).
//
// Re-entrancy: with RW the handler's own read of the watched dword traps again
// (nested single-step, EIP inside this DLL). Every trap whose EIP lies inside
// this DLL is dropped before the handler touches any watched memory, so the
// nesting is one level deep. The same rule drops the hooks' own node dumps.
// The handler takes no locks, does no I/O and allocates nothing.
// ---------------------------------------------------------------------------

static const int WATCH_SLOTS = 4;

enum { WATCH_BASE_P = 0, WATCH_BASE_NODE = 1 };
static int      g_watchBase = WATCH_BASE_NODE;
static uint32_t g_watchOffset = 0x74;
static int      g_watchRW = 1;               // 1: read or write (RW=11b), 0: write only (RW=01b)
static char     g_watchConfigText[96] = "node+0x74 rw (default, no pathfix_watch.txt)";

static const int SLOT_HISTORY = 64;          // previous addresses per slot (stale traps)
static const DWORD SLOT_STALE_MS = 1000;     // a slot not refreshed this long may be taken over

struct WatchSlot {
    volatile LONG used;        // 1 once assigned
    volatile LONG announced;   // logged by the watch thread
    volatile LONG addr;        // current target
    volatile LONG unitThis;    // FUN_101a9750 this pointer that owns the slot
    uint32_t threadId;         // thread that ran FUN_101a9750 at the last (re)point
    volatile LONG cached;      // last value seen (old_value of the next record)
    volatile LONG gen;         // incremented on every re-point
    volatile LONG lastSeen;    // GetTickCount when any mover last showed this target
    volatile LONG histIdx;
    volatile LONG hist[SLOT_HISTORY];
};
static WatchSlot g_slots[WATCH_SLOTS] = {};
static volatile LONG g_slotCount = 0;
static volatile LONG g_watchRecording = 0;   // records accepted while 1
static volatile LONG g_watchEnabled = 0;     // v16: F7 toggles; 0 = no slot is armed or re-pointed
static HANDLE g_watchWake = nullptr;         // set when a slot is assigned or re-pointed
static HANDLE g_watchStopReq = nullptr;      // set to request disarm
static HANDLE g_watchDisarmed = nullptr;     // set by the watch thread when done
static HANDLE g_watchThread = nullptr;
static PVOID  g_veh = nullptr;
static volatile LONG g_watchShutdown = 0;
static volatile LONG g_watchSelf = 0;        // traps with EIP inside this DLL (dropped)
static volatile LONG g_watchStale = 0;       // traps on a previous address of a slot (dropped)

static uint32_t g_textStart = 0, g_textEnd = 0;   // absolute .text bounds

// Slot (re)assignments, written to pathfix_capture_v42_slots.csv.
enum { SLOT_ASSIGN = 1, SLOT_NODE_CHANGE = 2, SLOT_TAKEOVER = 3 };
struct SlotEvent {
    volatile LONG valid;
    uint8_t  slot;
    uint8_t  kind;
    uint16_t pad;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t unitThis;
    uint32_t prevThis;
    uint32_t P;
    uint32_t oldAddr;
    uint32_t newAddr;
    uint32_t value;            // dword at the new address when re-pointed
};
static const int SLOTEV_SIZE = 64 * 1024;
static SlotEvent*    g_slotEv = nullptr;
static volatile LONG g_slotEvIdx = 0;
static volatile LONG g_slotEvOverflow = 0;

// Per (slot, EIP) counters for rate limiting and totals. Open
// addressing, inserted with InterlockedCompareExchange; never removed.
static const int EIP_TABLE = 512;
struct EipStat {
    volatile LONG eip;         // 0 = empty
    volatile LONG total;
    volatile LONG recorded;
    volatile LONG changes;
    volatile LONG reads;       // hits flagged is_read
    volatile LONG secBucket;
    volatile LONG secCount;
};
static EipStat g_eipStats[WATCH_SLOTS][EIP_TABLE] = {};
static volatile LONG g_eipTableFull = 0;

static EipStat* EipLookup(int slot, uint32_t eip) {
    uint32_t h = (eip * 2654435761u) % EIP_TABLE;
    for (int i = 0; i < EIP_TABLE; i++) {
        EipStat& e = g_eipStats[slot][(h + i) % EIP_TABLE];
        LONG cur = InterlockedCompareExchange(&e.eip, (LONG)eip, 0);
        if (cur == 0 || cur == (LONG)eip) return &e;
    }
    InterlockedExchange(&g_eipTableFull, 1);
    return nullptr;
}

// EIPs that have changed a watched value on any slot ("known stores").
static const int STORE_TABLE = 256;
static volatile LONG g_storeEips[STORE_TABLE] = {};
static bool StoreKnown(uint32_t eip, bool insert) {
    uint32_t h = (eip * 2654435761u) % STORE_TABLE;
    for (int i = 0; i < STORE_TABLE; i++) {
        volatile LONG* e = &g_storeEips[(h + i) % STORE_TABLE];
        LONG cur = insert ? InterlockedCompareExchange(e, (LONG)eip, 0) : *e;
        if (cur == (LONG)eip) return true;
        if (cur == 0) return insert;   // inserted (insert) or absent (lookup)
    }
    return false;
}

struct WatchRecord {
    volatile LONG valid;
    uint8_t  slot;
    uint8_t  changed;
    uint8_t  reason;           // 1 value changed, 2 first 20 hits of this eip, 4 within 50/s
    uint8_t  nStack;
    uint8_t  isRead;           // value unchanged and EIP not a known store
    uint8_t  pad[3];
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t addr;
    uint32_t unitThis;         // slot owner at the time of the hit
    uint32_t slotGen;
    uint32_t eip;
    uint32_t oldValue;
    uint32_t newValue;
    uint32_t regs[8];          // eax ebx ecx edx esi edi ebp esp
    uint32_t stack[8];         // absolute candidate return addresses
    uint32_t hitsThisEip;
};
static const int WATCH_SIZE = 64 * 1024;
static WatchRecord*  g_watch = nullptr;
static volatile LONG g_watchIdx = 0;
static volatile LONG g_watchOverflow = 0;
static volatile LONG g_watchForeign = 0;     // single-steps with a foreign Dr6 bit

// Sweep statistics, written to pathfix_capture_v42_sweeps.csv.
struct SweepStat {
    volatile LONG valid;       // v16: set last (incremental writer)
    uint32_t timestamp;
    uint16_t threads;          // threads of this process in the snapshot
    uint16_t armed;            // threads carrying our current slot mask after the sweep
    uint16_t changed;          // threads (re)armed in this sweep
    uint16_t failed;           // OpenThread / context calls that failed
    uint8_t  mask;             // slot mask applied
    uint8_t  disarm;           // 1 for the disarm sweep
};
static const int SWEEP_SIZE = 16384;
static SweepStat g_sweeps[SWEEP_SIZE];
static volatile LONG g_sweepIdx = 0;

static void SlotEventAdd(int slot, int kind, uint32_t unitThis, uint32_t prevThis, uint32_t P,
                         uint32_t oldAddr, uint32_t newAddr, uint32_t value) {
    if (!g_slotEv || g_state != 2) return;
    LONG idx = InterlockedIncrement(&g_slotEvIdx) - 1;
    if (idx >= SLOTEV_SIZE) { InterlockedExchange(&g_slotEvOverflow, 1); return; }
    SlotEvent& e = g_slotEv[idx];
    e.slot = (uint8_t)slot;
    e.kind = (uint8_t)kind;
    e.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    e.timestamp = GetTickCount() - g_captureStart;
    e.threadId = GetCurrentThreadId();
    e.unitThis = unitThis;
    e.prevThis = prevThis;
    e.P = P;
    e.oldAddr = oldAddr;
    e.newAddr = newAddr;
    e.value = value;
    InterlockedExchange(&e.valid, 1);
}

// Points slot i at addr. The previous address goes into the slot history
// first, so a trap that still arrives on it (threads not yet re-armed) is
// recognised as stale and dropped.
static void SlotPoint(int i, int kind, uint32_t unitThis, uint32_t P, uint32_t addr, DWORD now) {
    WatchSlot& s = g_slots[i];
    uint32_t oldAddr = (uint32_t)s.addr;
    uint32_t prevThis = (uint32_t)s.unitThis;
    if (kind != SLOT_ASSIGN) {
        LONG h = InterlockedIncrement(&s.histIdx) - 1;
        InterlockedExchange(&s.hist[h % SLOT_HISTORY], (LONG)oldAddr);
    }
    uint32_t v = 0;
    ReadDword(addr, &v);
    InterlockedExchange(&s.cached, (LONG)v);
    InterlockedExchange(&s.unitThis, (LONG)unitThis);
    s.threadId = GetCurrentThreadId();
    InterlockedExchange(&s.lastSeen, (LONG)now);
    InterlockedExchange(&s.addr, (LONG)addr);
    InterlockedIncrement(&s.gen);
    if (kind == SLOT_ASSIGN) {
        InterlockedExchange(&s.used, 1);
        InterlockedIncrement(&g_slotCount);
    }
    SlotEventAdd(i, kind, unitThis, kind == SLOT_ASSIGN ? 0 : prevThis, P,
                 kind == SLOT_ASSIGN ? 0 : oldAddr, addr, v);
    if (g_watchWake) SetEvent(g_watchWake);
}

static bool SlotHistoryHas(int i, uint32_t addr) {
    for (int k = 0; k < SLOT_HISTORY; k++)
        if ((uint32_t)g_slots[i].hist[k] == addr) return true;
    return false;
}

// Called from the cursor hook (game thread) under g_thisLock on every call of
// FUN_101a9750. *ownSlot is the per-this slot index (-1 none, -2 never again).
// No I/O; the watch thread logs and re-arms.
static void WatchTrack(int* ownSlot, uint32_t unitThis, uint32_t P, uint32_t node, DWORD now) {
    uint32_t base = (g_watchBase == WATCH_BASE_NODE) ? node : P;
    if (base == 0) return;
    uint32_t addr = base + g_watchOffset;
    if (addr & 3) return;                    // LEN=4 needs 4-byte alignment

    if (g_watchBase == WATCH_BASE_P) {       // v13 rule: sticky, one per unit
        if (*ownSlot != -1) return;
        for (int i = 0; i < WATCH_SLOTS; i++)
            if (g_slots[i].used && (uint32_t)g_slots[i].addr == addr) { *ownSlot = i; return; }
        for (int i = 0; i < WATCH_SLOTS; i++) {
            if (g_slots[i].used) continue;
            SlotPoint(i, SLOT_ASSIGN, unitThis, P, addr, now);
            *ownSlot = i;
            return;
        }
        *ownSlot = -2;
        return;
    }

    // Node base: already watched (by this or another mover) -> refresh only.
    for (int i = 0; i < WATCH_SLOTS; i++) {
        if (g_slots[i].used && (uint32_t)g_slots[i].addr == addr) {
            InterlockedExchange(&g_slots[i].lastSeen, (LONG)now);
            return;
        }
    }
    // Own slot: the cursor node changed -> follow it.
    int own = *ownSlot;
    if (own >= 0 && g_slots[own].used && (uint32_t)g_slots[own].unitThis == unitThis) {
        SlotPoint(own, SLOT_NODE_CHANGE, unitThis, P, addr, now);
        return;
    }
    // A free slot.
    for (int i = 0; i < WATCH_SLOTS; i++) {
        if (g_slots[i].used) continue;
        SlotPoint(i, SLOT_ASSIGN, unitThis, P, addr, now);
        *ownSlot = i;
        return;
    }
    // Take over the least recently refreshed slot if it is stale.
    int oldest = -1;
    DWORD oldestAge = 0;
    for (int i = 0; i < WATCH_SLOTS; i++) {
        DWORD age = now - (DWORD)g_slots[i].lastSeen;
        if (age > SLOT_STALE_MS && age > oldestAge) { oldest = i; oldestAge = age; }
    }
    if (oldest >= 0) {
        SlotPoint(oldest, SLOT_TAKEOVER, unitThis, P, addr, now);
        *ownSlot = oldest;
    }
}

// Collect up to eight dwords between esp and the thread's stack base that
// point into empire.retail.dll .text. Candidates only, not a call chain.
static int StackCandidates(uint32_t esp, uint32_t* out) {
    uint32_t stackBase = (uint32_t)__readfsdword(4);
    int n = 0;
    if (esp == 0 || stackBase <= esp) return 0;
    uint32_t end = stackBase;
    if (end - esp > 0x10000) end = esp + 0x10000;   // scan at most 64 KB
    __try {
        for (uint32_t a = esp; a + 4 <= end && n < 8; a += 4) {
            uint32_t v = *(const uint32_t*)(uintptr_t)a;
            if (v >= g_textStart && v < g_textEnd) out[n++] = v;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return n;
}

static LONG CALLBACK WatchVeh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    DWORD dr6 = c->Dr6;
    if ((dr6 & 0xF) == 0) return EXCEPTION_CONTINUE_SEARCH;

    const uint32_t eip = c->Eip;
    const bool self = eip >= g_selfStart && eip < g_selfEnd;
    const DWORD drs[4] = { c->Dr0, c->Dr1, c->Dr2, c->Dr3 };
    DWORD ours = 0;
    for (int i = 0; i < WATCH_SLOTS; i++) {
        if (!(dr6 & (1u << i))) continue;
        if (!g_slots[i].used) continue;
        uint32_t addr = (uint32_t)g_slots[i].addr;
        if (drs[i] != addr) {
            // Not the current target: a previous one (thread not re-armed yet)
            // is ours and dropped; anything else belongs to someone else.
            if (SlotHistoryHas(i, drs[i])) { ours |= 1u << i; InterlockedIncrement(&g_watchStale); }
            continue;
        }
        ours |= 1u << i;
        // Our own code (node dumps, guarded reads, and this handler's read
        // below): drop before touching the watched dword.
        if (self) { InterlockedIncrement(&g_watchSelf); continue; }

        uint32_t newValue = *(volatile const uint32_t*)(uintptr_t)addr;   // nested trap, dropped as self
        uint32_t oldValue = (uint32_t)InterlockedExchange(&g_slots[i].cached, (LONG)newValue);
        bool changed = newValue != oldValue;
        if (changed) StoreKnown(eip, true);
        bool isRead = !changed && !StoreKnown(eip, false);

        EipStat* e = EipLookup(i, eip);
        LONG total = 0;
        int reason = 0;
        if (e) {
            total = InterlockedIncrement(&e->total);
            if (changed) InterlockedIncrement(&e->changes);
            if (isRead) InterlockedIncrement(&e->reads);
            LONG sec = (LONG)(GetTickCount() / 1000);
            if (e->secBucket != sec) {
                InterlockedExchange(&e->secBucket, sec);
                InterlockedExchange(&e->secCount, 0);
            }
            LONG inSec = InterlockedIncrement(&e->secCount);
            if (changed) reason |= 1;
            if (total <= 20) reason |= 2;
            if (inSec <= 50) reason |= 4;
        } else {
            reason = changed ? 1 : 0;
        }
        if (!reason || !g_watchRecording) continue;
        if (e) InterlockedIncrement(&e->recorded);

        LONG idx = InterlockedIncrement(&g_watchIdx) - 1;
        if (idx >= WATCH_SIZE) { InterlockedExchange(&g_watchOverflow, 1); continue; }
        WatchRecord& r = g_watch[idx];
        r.slot = (uint8_t)i;
        r.changed = changed ? 1 : 0;
        r.isRead = isRead ? 1 : 0;
        r.reason = (uint8_t)reason;
        r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
        r.timestamp = GetTickCount() - g_captureStart;
        r.threadId = GetCurrentThreadId();
        r.addr = addr;
        r.unitThis = (uint32_t)g_slots[i].unitThis;
        r.slotGen = (uint32_t)g_slots[i].gen;
        r.eip = eip;
        r.oldValue = oldValue;
        r.newValue = newValue;
        r.regs[0] = c->Eax; r.regs[1] = c->Ebx; r.regs[2] = c->Ecx; r.regs[3] = c->Edx;
        r.regs[4] = c->Esi; r.regs[5] = c->Edi; r.regs[6] = c->Ebp; r.regs[7] = c->Esp;
        r.nStack = (uint8_t)StackCandidates(c->Esp, r.stack);
        r.hitsThisEip = (uint32_t)total;
        InterlockedExchange(&r.valid, 1);
    }
    if (!ours) return EXCEPTION_CONTINUE_SEARCH;
    if ((dr6 & 0xF) & ~ours) {
        // A foreign breakpoint fired together with ours: leave it to its owner.
        InterlockedExchange(&g_watchForeign, 1);
        c->Dr6 = dr6 & ~ours;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    c->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Dr7 bits for our slot i: L_i (bit 2i), RW_i at 16+4i (01 write, 11 read or
// write), LEN_i = 11 (4 bytes) at 18+4i.
static DWORD Dr7Field(int i)  { return (1u << (2 * i)) | (3u << (16 + 4 * i)) | (3u << (18 + 4 * i)); }
static DWORD Dr7Ours(int i)   { return (1u << (2 * i)) | ((g_watchRW ? 3u : 1u) << (16 + 4 * i)) | (3u << (18 + 4 * i)); }
static DWORD Dr7Enable(int i) { return 3u << (2 * i); }   // L_i | G_i

struct ArmedThread {
    DWORD tid;
    DWORD mask;                // our slots currently set on that thread
    DWORD foreign;             // debug registers owned by someone else there
    DWORD foreignLogged;
    LONG  gen[WATCH_SLOTS];    // slot generation applied there
};
static const int ARMED_MAX = 1024;
static ArmedThread g_armed[ARMED_MAX];
static int g_armedUsed = 0;

static ArmedThread* ArmedFind(DWORD tid, bool create) {
    for (int i = 0; i < g_armedUsed; i++) if (g_armed[i].tid == tid) return &g_armed[i];
    if (!create || g_armedUsed >= ARMED_MAX) return nullptr;
    ArmedThread* a = &g_armed[g_armedUsed++];
    a->tid = tid;
    a->mask = 0;
    a->foreign = 0;
    a->foreignLogged = 0;
    for (int i = 0; i < WATCH_SLOTS; i++) a->gen[i] = -1;
    return a;
}

// Pending log lines from a sweep (written after all threads are resumed).
static char g_sweepLog[16][160];
static int  g_sweepLogN = 0;

// Applies `want` (slot mask) to every thread of the process except this one.
// A thread is touched when its mask differs or a slot it carries was
// re-pointed since. Between SuspendThread and ResumeThread nothing allocates,
// logs or locks.
static void Sweep(DWORD want, bool disarm) {
    DWORD self = GetCurrentThreadId();
    DWORD pid = GetCurrentProcessId();
    LONG gens[WATCH_SLOTS];
    uint32_t addrs[WATCH_SLOTS];
    for (int i = 0; i < WATCH_SLOTS; i++) {   // generation first, then address
        gens[i] = g_slots[i].gen;
        addrs[i] = (uint32_t)g_slots[i].addr;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    int threads = 0, changed = 0, failed = 0, armedNow = 0;
    g_sweepLogN = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        threads++;
        if (te.th32ThreadID == self) continue;
        ArmedThread* a = ArmedFind(te.th32ThreadID, !disarm);
        if (!a) continue;
        bool current = a->mask == (want & ~a->foreign);
        for (int i = 0; current && i < WATCH_SLOTS; i++)
            if (((a->mask >> i) & 1) && a->gen[i] != gens[i]) current = false;
        if (current) { if (want) armedNow++; continue; }
        HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                              FALSE, te.th32ThreadID);
        if (!h) { failed++; continue; }
        DWORD applied = a->mask;
        DWORD foreign = 0;
        bool okCtx = false;
        if (SuspendThread(h) != (DWORD)-1) {
            CONTEXT ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(h, &ctx)) {
                DWORD dr7 = ctx.Dr7;
                DWORD* dr[4] = { &ctx.Dr0, &ctx.Dr1, &ctx.Dr2, &ctx.Dr3 };
                applied = 0;
                for (int i = 0; i < WATCH_SLOTS; i++) {
                    bool usedByUs = (a->mask >> i) & 1;
                    bool enabled = (dr7 & Dr7Enable(i)) != 0;
                    if (enabled && !usedByUs) {            // owned by someone else: leave it
                        foreign |= 1u << i;
                        continue;
                    }
                    dr7 &= ~Dr7Field(i);
                    if ((want >> i) & 1) {
                        *dr[i] = addrs[i];
                        dr7 |= Dr7Ours(i);
                        applied |= 1u << i;
                    } else if (usedByUs) {
                        *dr[i] = 0;
                    }
                }
                ctx.Dr7 = dr7;
                ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                okCtx = SetThreadContext(h, &ctx) != 0;
            }
            ResumeThread(h);
        }
        CloseHandle(h);
        if (!okCtx) { failed++; continue; }
        a->mask = applied;
        a->foreign = foreign;
        for (int i = 0; i < WATCH_SLOTS; i++) a->gen[i] = gens[i];
        changed++;
        if (want && applied == (want & ~foreign)) armedNow++;
        if (foreign && !a->foreignLogged && g_sweepLogN < 16) {
            a->foreignLogged = 1;
            sprintf(g_sweepLog[g_sweepLogN++],
                    "watch: thread %lu already had Dr7 bits set for register mask 0x%lX "
                    "(another tool); our slots there were skipped", te.th32ThreadID, foreign);
        }
    }
    CloseHandle(snap);

    LONG si = InterlockedIncrement(&g_sweepIdx) - 1;
    if (si < SWEEP_SIZE) {
        SweepStat& s = g_sweeps[si];
        s.timestamp = (g_state >= 2) ? GetTickCount() - g_captureStart : 0;
        s.threads = (uint16_t)threads;
        s.armed = (uint16_t)armedNow;
        s.changed = (uint16_t)changed;
        s.failed = (uint16_t)failed;
        s.mask = (uint8_t)want;
        s.disarm = disarm ? 1 : 0;
        InterlockedExchange(&s.valid, 1);
    }
    for (int i = 0; i < g_sweepLogN; i++) Log("%s", g_sweepLog[i]);
    // Re-points make sweeps frequent: log the first 20 that changed something,
    // then every 200th, and the disarm sweep. The sweeps CSV has all of them.
    if ((changed && (si < 20 || si % 200 == 0)) || disarm)
        Log("watch sweep %ld%s: %d threads in process, %d carry slot mask 0x%lX, %d changed, %d failed",
            (long)si, disarm ? " (DISARM)" : "", threads, armedNow, want, changed, failed);
}

static void AnnounceSlots() {
    for (int i = 0; i < WATCH_SLOTS; i++) {
        WatchSlot& s = g_slots[i];
        if (!s.used || s.announced) continue;
        s.announced = 1;
        Log("watch slot %d = 0x%08X (unit this 0x%08X, thread %lu), value now 0x%08X",
            i, (uint32_t)s.addr, (uint32_t)s.unitThis, s.threadId, (uint32_t)s.cached);
    }
}

// v16: nothing is armed while the watch is disabled (F7). Disabling keeps the
// slots `used`, so a trap still in flight on a thread not yet cleared is
// recognised as ours and swallowed by the handler (an unclaimed single-step
// would be fatal).
static DWORD WINAPI WatchThread(LPVOID) {
    HANDLE waits[2] = { g_watchStopReq, g_watchWake };
    DWORD lastWant = 0;
    for (;;) {
        DWORD w = WaitForMultipleObjects(2, waits, FALSE, 500);
        if (w == WAIT_OBJECT_0) break;
        AnnounceSlots();
        DWORD want = 0;
        if (g_watchEnabled)
            for (int i = 0; i < WATCH_SLOTS; i++) if (g_slots[i].used) want |= 1u << i;
        if (want || lastWant) Sweep(want, false);
        lastWant = want;
    }
    InterlockedExchange(&g_watchRecording, 0);
    Sweep(0, true);
    SetEvent(g_watchDisarmed);
    return 0;
}

// pathfix_watch.txt, first non-comment line: `<base>+<offset> [rw|w]` with
// base `node` (follows the cursor node) or `P` (sticky, v13). Access defaults
// to rw for node and w for P (the v13 forms keep their meaning).
static void LoadWatchConfig() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* sl = strrchr(path, '\\');
    if (sl) *(sl + 1) = 0;
    strcat(path, "pathfix_watch.txt");
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[128];
    bool parsed = false;
    while (!parsed && fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || *p == 0) continue;
        int base = WATCH_BASE_P;
        if (_strnicmp(p, "node+", 5) == 0) { base = WATCH_BASE_NODE; p += 5; }
        else if (_strnicmp(p, "P+", 2) == 0) { p += 2; }
        char* end = nullptr;
        unsigned long off = strtoul(p, &end, 0);
        if (end == p || off > 0x10000) {
            Log("pathfix_watch.txt: cannot parse \"%s\", keeping node+0x74 rw", line);
            break;
        }
        int rw = (base == WATCH_BASE_NODE) ? 1 : 0;
        while (*end == ' ' || *end == '\t') end++;
        if (_strnicmp(end, "rw", 2) == 0) rw = 1;
        else if (_strnicmp(end, "w", 1) == 0) rw = 0;
        g_watchBase = base;
        g_watchOffset = (uint32_t)off;
        g_watchRW = rw;
        parsed = true;
    }
    fclose(f);
    if (parsed)
        sprintf(g_watchConfigText, "%s+0x%X %s (from pathfix_watch.txt)",
                g_watchBase == WATCH_BASE_NODE ? "node" : "P", g_watchOffset, g_watchRW ? "rw" : "w");
}

static bool FindTextSection(const uint8_t* base) {
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            g_textStart = (uint32_t)(uintptr_t)base + sec[i].VirtualAddress;
            g_textEnd = g_textStart + sec[i].Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

static void WatchInstall(const uint8_t* base) {
    LoadWatchConfig();
    if (!FindTextSection(base)) {
        Log("  Watch:       no .text section found; watchpoint disabled");
        return;
    }
    g_watch = (WatchRecord*)VirtualAlloc(nullptr, sizeof(WatchRecord) * WATCH_SIZE,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_watchWake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    g_watchStopReq = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_watchDisarmed = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_slotEv = (SlotEvent*)VirtualAlloc(nullptr, sizeof(SlotEvent) * SLOTEV_SIZE,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    // This proxy's own image range: traps with EIP inside it are dropped.
    HMODULE selfMod = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&WatchVeh, &selfMod);
    if (selfMod) {
        const IMAGE_DOS_HEADER* sd = (const IMAGE_DOS_HEADER*)selfMod;
        const IMAGE_NT_HEADERS32* sn = (const IMAGE_NT_HEADERS32*)((const uint8_t*)selfMod + sd->e_lfanew);
        g_selfStart = (uint32_t)(uintptr_t)selfMod;
        g_selfEnd = g_selfStart + sn->OptionalHeader.SizeOfImage;
    }
    if (!g_watch || !g_watchWake || !g_watchStopReq || !g_watchDisarmed || !g_slotEv || !g_selfStart) {
        Log("  Watch:       allocation failed; watchpoint disabled");
        g_watch = nullptr;
        return;
    }
    g_veh = AddVectoredExceptionHandler(1, WatchVeh);
    if (!g_veh) {
        Log("  Watch:       AddVectoredExceptionHandler failed; watchpoint disabled");
        g_watch = nullptr;
        return;
    }
    InterlockedExchange(&g_watchRecording, 0);   // v16: off until F7
    g_watchThread = CreateThread(nullptr, 0, WatchThread, nullptr, 0, nullptr);
    Log("  Watch:       %s, up to %d slots, 4 bytes, %s; .text 0x%08X..0x%08X; proxy 0x%08X..0x%08X dropped",
        g_watchConfigText, WATCH_SLOTS, g_watchRW ? "read or write" : "write only",
        g_textStart, g_textEnd, g_selfStart, g_selfEnd);
    if (g_watchBase == WATCH_BASE_NODE)
        Log("  Watch:       node slots follow the cursor node (re-armed on change); "
            "a slot unseen for %d ms may be taken over", (int)SLOT_STALE_MS);
    Log("  Watch:       OFF (v19: no key; nothing is armed)");
}

// Disarm every thread, then remove the handler. Idempotent. The disarm sweep
// runs on the watch thread, which can also reach the thread calling this.
static void WatchShutdown() {
    if (InterlockedExchange(&g_watchShutdown, 1) != 0) return;
    if (!g_veh) return;
    InterlockedExchange(&g_watchRecording, 0);
    bool done = false;
    if (g_watchThread && WaitForSingleObject(g_watchThread, 0) == WAIT_TIMEOUT) {
        SetEvent(g_watchStopReq);
        done = WaitForSingleObject(g_watchDisarmed, 3000) == WAIT_OBJECT_0;
    }
    if (!done) {
        // Watch thread gone (process exit) or not answering: disarm from here;
        // this thread itself cannot be changed and is reported.
        Sweep(0, true);
        Log("watch: disarm ran on thread %lu (watch thread not available)", GetCurrentThreadId());
    }
    Sleep(50);   // let a trap that was already in flight reach the handler
    RemoveVectoredExceptionHandler(g_veh);
    g_veh = nullptr;
    Log("watch: disarmed, handler removed");
}

// ---------------------------------------------------------------------------
// Common handler front end
// ---------------------------------------------------------------------------

// ctx points at the saved EFLAGS; everything else is at the CTX_OFF_* offsets
// generated by tools/stack_calc.py for the sequence pushad, pushfd.
static inline uint32_t CtxDword(const uint8_t* ctx, int off) {
    return *(const uint32_t*)(ctx + off);
}

// Returns true while the window is open. Capture mode (PATHFIX_CAPTURE=1) opens it on the
// first hit; in play (v42) only P opens it (RecStart) and P closes it again (RecStop).
static bool WindowOpen(DWORD now, DWORD* elapsed) {
    if (!g_captureOn && !g_recActive) return false;   // every fix path runs before this gate
    LONG state = g_state;
    if (state == 1 && g_captureOn) {
        if (InterlockedCompareExchange(&g_state, 2, 1) == 1) {
            g_captureStart = now;
            g_captureEnd = CAPTURE_MS;
        }
        state = g_state;
    }
    if (state != 2) return false;
    *elapsed = now - g_captureStart;
    if (*elapsed > g_captureEnd) {
        if (g_captureOn) InterlockedExchange(&g_state, 3);
        return false;
    }
    return true;
}

static volatile DWORD g_lastCtorTick = 0;     // GetTickCount of the last FUN_102557e0 hit

// v27: last move order per unit (FUN_10128570 with a plausible width at
// [ECX + 0x60]; ARG1 = unit). Open addressing, never shrinks.
struct UnitOrder { volatile LONG unit; volatile LONG tick; };
static const int UNIT_ORDER_TABLE = 512;      // power of two
static UnitOrder g_unitOrders[UNIT_ORDER_TABLE] = {};
static volatile LONG g_unitOrderNotes = 0;

// v36: game-time clock. Every fix window (apex freshness, lane keep, stuck
// release, turning-sign keep, no-damping freshness) is measured in GAME
// milliseconds read from the engine's tick counter at [[ctrl+0x14]+8]+0x50
// (FUN_101b7b70: tick * DAT_115b2448 (0.1 s) > ctrl+0x2C + 1.0 runs
// FUN_101bc880 once per game second), so the fix behaves the same at 1x and
// 3x game speed. All tests so far ran at 3x (3.6 game s per wall s measured),
// so the windows keep their tested game-time length: wall ms * GT_PER_WALL.
// Capture timestamps, row rate limits, sampling and the keys stay on
// GetTickCount. Until the first FUN_101bc880 hit provides the clock, GameNow()
// falls back to the wall clock (counted in gt_fallback).
#define GT_PER_WALL 3.6
#define GT_MS(wallMs) ((DWORD)((wallMs) * GT_PER_WALL + 0.5))
static const DWORD GT_TICK_MS = 100;           // DAT_115b2448 = 0.1 s per tick
static volatile LONG g_gtClock = 0;            // clock object ([ctrl+0x14]+8), 0 unknown
static volatile LONG g_gtReads = 0, g_gtFail = 0, g_gtFallback = 0, g_gtObjChanges = 0, g_gtNotes = 0;
static volatile LONG g_gtLastTick = 0;


static void GameClockNote(uint32_t ctrl) {
    uint32_t p1 = 0, obj = 0, tick = 0;
    if (!ReadDword(ctrl + 0x14, &p1) || !PtrPlausible(p1) || !ReadDword(p1 + 0x08, &obj) || !PtrPlausible(obj) ||
        !ReadDword(obj + 0x50, &tick))
        return;
    LONG prev = InterlockedExchange(&g_gtClock, (LONG)obj);
    InterlockedIncrement(&g_gtNotes);
    if (prev == 0) {
        Log("v36 game clock: object 0x%08X from controller 0x%08X, tick %u (%.1f game s); fix windows run on game time "
            "(%.1f game s per wall s assumed for the tested lengths)", obj, ctrl, tick, tick * 0.1, GT_PER_WALL);
    } else if ((uint32_t)prev != obj) {
        if (InterlockedIncrement(&g_gtObjChanges) <= 3)
            Log("v36 game clock: object changed 0x%08X -> 0x%08X (controller 0x%08X, tick %u)", (uint32_t)prev, obj,
                ctrl, tick);
    }
}

// Game time in ms (tick * 100). Falls back to the wall clock until the clock is known.
static DWORD GameNow() {
    uint32_t obj = (uint32_t)g_gtClock, tick = 0;
    if (obj && ReadDword(obj + 0x50, &tick)) {
        InterlockedIncrement(&g_gtReads);
        InterlockedExchange(&g_gtLastTick, (LONG)tick);
        return tick * GT_TICK_MS;
    }
    if (obj) InterlockedIncrement(&g_gtFail);
    InterlockedIncrement(&g_gtFallback);
    return GetTickCount();
}

static void UnitOrderNote(uint32_t unit, DWORD now) {
    if (!unit) return;
    uint32_t h = ((unit >> 2) * 2654435761u) & (UNIT_ORDER_TABLE - 1);
    for (int i = 0; i < UNIT_ORDER_TABLE; i++) {
        UnitOrder& u = g_unitOrders[(h + i) & (UNIT_ORDER_TABLE - 1)];
        LONG cur = u.unit;
        if (cur == 0) cur = InterlockedCompareExchange(&u.unit, (LONG)unit, 0);
        if (cur == 0 || (uint32_t)cur == unit) {
            InterlockedExchange(&u.tick, (LONG)(now ? now : 1));
            InterlockedIncrement(&g_unitOrderNotes);
            return;
        }
    }
}

static DWORD UnitOrderTick(uint32_t unit) {
    uint32_t h = ((unit >> 2) * 2654435761u) & (UNIT_ORDER_TABLE - 1);
    for (int i = 0; i < UNIT_ORDER_TABLE; i++) {
        UnitOrder& u = g_unitOrders[(h + i) & (UNIT_ORDER_TABLE - 1)];
        if ((uint32_t)u.unit == unit) return (DWORD)u.tick;
        if (u.unit == 0) return 0;
    }
    return 0;
}

// Offline harness only: note a move order for a unit (the game never calls it).
extern "C" __declspec(dllexport) void __stdcall PathfixTestOrder(uint32_t unit) {
    UnitOrderNote(unit, GameNow());
}
// Offline harness only: the game-time clock as the fix sees it (ms).
extern "C" __declspec(dllexport) DWORD __stdcall PathfixTestGameNow() {
    return GameNow();
}

static void EventHit(const uint8_t* ctx, int h) {
    DWORD now = GetTickCount();
    if (h == H_MOVEORDER) g_lastCtorTick = now ? now : 1;
    if (h == H_TICKCALLER) {   // v27: real move orders carry the width at [ECX + 0x60]
        uint32_t w60 = 0;
        if (ReadDword(CtxDword(ctx, CTX_OFF_ECX) + 0x60, &w60)) {
            float w;
            memcpy(&w, &w60, 4);
            if (_finite(w) && w >= 1.0f && w <= 200.0f) UnitOrderNote(CtxDword(ctx, CTX_OFF_ARG1), GameNow());   // v36 game time
        }
    }
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    uint32_t hitNo = 0;
    int reason = RateDecide(h, now, inWindow, &hitNo);
    if (!reason) return;

    LONG idx = InterlockedIncrement(&g_eventIdx) - 1;
    if (idx >= EVENT_SIZE) { InterlockedExchange(&g_eventOverflow, 1); return; }
    EventRecord& r = g_events[idx];
    r.hook = (uint8_t)h;
    r.reason = (uint8_t)reason;
    r.markerNo = 0;
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.ecx = CtxDword(ctx, CTX_OFF_ECX);
    r.args[0] = CtxDword(ctx, CTX_OFF_ARG1);
    r.args[1] = CtxDword(ctx, CTX_OFF_ARG2);
    r.args[2] = CtxDword(ctx, CTX_OFF_ARG3);
    r.args[3] = CtxDword(ctx, CTX_OFF_ARG4);
    r.args[4] = CtxDword(ctx, CTX_OFF_ARG5);
    r.args[5] = CtxDword(ctx, CTX_OFF_ARG6);
    r.args[6] = CtxDword(ctx, CTX_OFF_ARG7);
    r.args[7] = CtxDword(ctx, CTX_OFF_ARG8);
    r.ecx60ok = ReadDword(r.ecx + 0x60, &r.ecx60) ? 1 : 0;
    r.retAbs = CtxDword(ctx, CTX_OFF_RET);
    r.hitNo = hitNo;
    InterlockedExchange(&r.valid, 1);
}

static void SoldierSample(uint32_t ctrl, DWORD now, DWORD elapsed);   // soldier trace, below

static void CursorHit(const uint8_t* ctx) {
    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    uint32_t hitNo = 0;
    RateDecide(H_CURSOR, now, inWindow, &hitNo);   // counters only; own record rule below
    if (!inWindow) return;

    uint32_t thisPtr = CtxDword(ctx, CTX_OFF_ECX);
    SoldierSample(thisPtr, now, elapsed);
    uint8_t readFlags = 0;
    uint32_t P = 0, node = 0, prog = 0;
    uint8_t flag74 = 0;
    if (ReadDword(thisPtr + 0x18, &P)) readFlags |= READ_P_OK;
    if ((readFlags & READ_P_OK) && ReadDword(P + 0xA8, &node) && ReadDword(P + 0xAC, &prog))
        readFlags |= READ_NODE_OK;
    if (ReadByte(thisPtr + 0x74, &flag74)) readFlags |= READ_FLAG_OK;

    // Decide under the per-this lock.
    int reason = 0;
    uint32_t callNo = 0;
    SpinLock(&g_thisLock);
    ThisState* st = nullptr;
    for (int i = 0; i < g_thisUsed; i++) {
        if (g_this[i].thisPtr == thisPtr) { st = &g_this[i]; break; }
    }
    if (!st) {
        if (g_thisUsed < THIS_TABLE_SIZE) {
            st = &g_this[g_thisUsed++];
            st->thisPtr = thisPtr;
            st->lastNode = node;
            st->calls = 0;
            st->P = P;
            st->nodeAtFirst = node;
            st->announced = 0;
            st->watchSlot = -1;
            reason = REASON_NEW_THIS | REASON_NODE;
        } else {
            InterlockedExchange(&g_thisOverflow, 1);
        }
    }
    if (st) {
        st->calls++;
        callNo = st->calls;
        if (node != st->lastNode) reason |= REASON_NODE;
        if (st->calls % 60 == 0) reason |= REASON_PERIODIC;
        if (reason) st->lastNode = node;
        // Watch slot: node-based slots follow the cursor node on every call;
        // P-based slots are assigned once (v13 rule).
        if (g_watch && g_watchEnabled && (readFlags & READ_P_OK) && ((readFlags & READ_NODE_OK) || g_watchBase == WATCH_BASE_P))
            WatchTrack(&st->watchSlot, thisPtr, P, node, now);
    }
    SpinUnlock(&g_thisLock);
    if (!reason) return;

    LONG idx = InterlockedIncrement(&g_cursorIdx) - 1;
    if (idx >= CURSOR_SIZE) { InterlockedExchange(&g_cursorOverflow, 1); return; }
    CursorRecord& r = g_cursor[idx];
    r.reason = (uint8_t)reason;
    r.flag74 = flag74;
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.thisPtr = thisPtr;
    r.arg1 = CtxDword(ctx, CTX_OFF_ARG1);
    r.retAbs = CtxDword(ctx, CTX_OFF_RET);
    r.callNo = callNo;
    r.P = P;
    r.node = node;
    r.progressBits = prog;
    r.nodeNext = 0;
    r.dumpIdx = -1;
    if (node != 0 && ReadDword(node + 0x8, &r.nodeNext)) readFlags |= READ_NEXT_OK;

    if (reason & REASON_NODE) {
        LONG d = InterlockedIncrement(&g_dumpIdx) - 1;
        if (d < DUMP_SIZE) {
            DumpSlot& s = g_dumps[d];
            s.nodeLen = node ? CopyGuarded(node, s.node, NODE_BYTES) : 0;
            s.pLen = P ? CopyGuarded(P, s.p, P_BYTES) : 0;
            r.dumpIdx = d;
        } else {
            readFlags |= READ_DUMP_FULL;
        }
    }
    r.readFlags = readFlags;
    InterlockedExchange(&r.valid, 1);
}

// ---------------------------------------------------------------------------
// Builder hook: FUN_1039bb40 entry (__thiscall, RET 0xC)
//   ECX = builder object, arg1 = P (cursor block), arg2 = node list head,
//   arg3 = param_4. Walk the list from the head through +0x08.
// ---------------------------------------------------------------------------

enum {
    STOP_NULL = 1,        // next pointer 0: end of list
    STOP_RANGE = 2,       // pointer outside 0x00010000..g_userMax (v15 and earlier: 0x7FFEFFFF)
    STOP_UNREADABLE = 3,  // node not fully readable
    STOP_CYCLE = 4,       // node already visited in this walk
    STOP_LIMIT = 5,       // 32 nodes walked
};
static const int WALK_LIMIT = 32;
static const DWORD PLAYER_WINDOW_MS = 500;   // build counted as player-ordered within this after a constructor hit

struct BuildRecord {
    volatile LONG valid;
    uint8_t  reason;           // REASON_BURST / REASON_QUOTA
    uint8_t  stop;             // STOP_*
    uint8_t  nodes;            // rows written for this build
    uint8_t  overridden;       // 1 if the width override was applied
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t ecx;
    uint32_t args[4];
    uint32_t retAbs;
    uint32_t hitNo;
    int32_t  msSinceCtor;      // -1 if no constructor hit seen yet
    int32_t  firstRow;         // index of the first NodeRow, -1 if none
};

struct NodeRow {
    volatile LONG valid;       // v16: set last (incremental writer)
    uint32_t buildSeq;
    uint8_t  index;
    uint8_t  dumpLen;          // 0xA0 or 0
    uint8_t  overridden;
    uint8_t  pad;
    uint32_t addr;
    uint32_t next;
    uint32_t w74, w78;         // raw bits as read (before any override)
    uint32_t new74, new78;     // values written by the override, else equal to w74 / w78
    uint8_t  dump[NODE_BYTES];
};

static const int BUILD_SIZE = 8 * 1024;
static const int NODEROW_SIZE = 32 * 1024;
static BuildRecord* g_builds = nullptr;
static NodeRow*     g_nodeRows = nullptr;
static volatile LONG g_buildIdx = 0;
static volatile LONG g_nodeRowIdx = 0;
static volatile LONG g_buildOverflow = 0;
static volatile LONG g_nodeRowOverflow = 0;

// Width override (Phase 10 A1). v16: set by the F6 preset cycle (off at start).
static volatile LONG g_overrideOn = 0;
static volatile LONG g_overrideAll = 1;      // 1 `all` builds, 0 `player` (v14 500 ms rule)
static volatile LONG g_overrideBits = 0;     // float bits written to +0x74 / +0x78
static float    g_overrideValue = 0.0f;
static char     g_overrideText[96] = "off (preset 0)";
static volatile LONG g_overrideBuilds = 0;
static volatile LONG g_overrideNodes = 0;

static bool WriteDwordGuarded(uint32_t addr, uint32_t v) {
    __try {
        *(volatile uint32_t*)(uintptr_t)addr = v;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    // The trap for this write is dropped (EIP inside this DLL) without reading
    // the value, so keep the slot's cached value in step here; otherwise the
    // next game read would look like a value change.
    for (int i = 0; i < WATCH_SLOTS; i++)
        if (g_slots[i].used && (uint32_t)g_slots[i].addr == addr)
            InterlockedExchange(&g_slots[i].cached, (LONG)v);
    return true;
}

static void BuilderHit(const uint8_t* ctx) {
    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    uint32_t hitNo = 0;
    int reason = RateDecide(H_BUILDER, now, inWindow, &hitNo);
    DWORD ctorTick = g_lastCtorTick;
    int32_t sinceCtor = ctorTick ? (int32_t)(now - ctorTick) : -1;
    bool player = ctorTick && (DWORD)(now - ctorTick) <= PLAYER_WINDOW_MS;
    bool doOverride = g_overrideOn && inWindow && (g_overrideAll || player);
    uint32_t ovBits = (uint32_t)g_overrideBits;
    if (!reason && !doOverride) return;

    uint32_t head = CtxDword(ctx, CTX_OFF_ARG2);
    BuildRecord* br = nullptr;
    if (reason) {
        LONG idx = InterlockedIncrement(&g_buildIdx) - 1;
        if (idx < BUILD_SIZE) br = &g_builds[idx];
        else InterlockedExchange(&g_buildOverflow, 1);
    }
    uint32_t seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);

    uint32_t visited[WALK_LIMIT];
    int n = 0;
    int stop = STOP_NULL;
    int firstRow = -1;
    uint32_t node = head;
    for (;;) {
        if (node == 0) { stop = STOP_NULL; break; }
        if (node < 0x00010000 || node > Mem_UserMax()) { stop = STOP_RANGE; break; }
        if (n >= WALK_LIMIT) { stop = STOP_LIMIT; break; }
        bool seen = false;
        for (int k = 0; k < n; k++) if (visited[k] == node) { seen = true; break; }
        if (seen) { stop = STOP_CYCLE; break; }
        if (!RangeReadable(node, NODE_BYTES)) { stop = STOP_UNREADABLE; break; }
        visited[n] = node;

        uint32_t next = 0, w74 = 0, w78 = 0;
        ReadDword(node + 0x08, &next);
        ReadDword(node + 0x74, &w74);
        ReadDword(node + 0x78, &w78);
        uint32_t n74 = w74, n78 = w78;
        uint8_t ov = 0;
        if (doOverride) {
            if (WriteDwordGuarded(node + 0x74, ovBits)) n74 = ovBits;
            if (WriteDwordGuarded(node + 0x78, ovBits)) n78 = ovBits;
            ov = 1;
            InterlockedIncrement(&g_overrideNodes);
        }
        if (br) {
            LONG ri = InterlockedIncrement(&g_nodeRowIdx) - 1;
            if (ri < NODEROW_SIZE) {
                NodeRow& row = g_nodeRows[ri];
                if (firstRow < 0) firstRow = ri;
                row.buildSeq = seq;
                row.index = (uint8_t)n;
                row.overridden = ov;
                row.addr = node;
                row.next = next;
                row.w74 = w74;
                row.w78 = w78;
                row.new74 = n74;
                row.new78 = n78;
                row.dumpLen = (uint8_t)CopyGuarded(node, row.dump, NODE_BYTES);
                InterlockedExchange(&row.valid, 1);
            } else {
                InterlockedExchange(&g_nodeRowOverflow, 1);
            }
        }
        n++;
        node = next;
    }
    if (doOverride) InterlockedIncrement(&g_overrideBuilds);

    if (br) {
        br->reason = (uint8_t)reason;
        br->stop = (uint8_t)stop;
        br->nodes = (uint8_t)n;
        br->overridden = doOverride ? 1 : 0;
        br->seq = seq;
        br->timestamp = elapsed;
        br->threadId = GetCurrentThreadId();
        br->ecx = CtxDword(ctx, CTX_OFF_ECX);
        br->args[0] = CtxDword(ctx, CTX_OFF_ARG1);
        br->args[1] = head;
        br->args[2] = CtxDword(ctx, CTX_OFF_ARG3);
        br->args[3] = CtxDword(ctx, CTX_OFF_ARG4);
        br->retAbs = CtxDword(ctx, CTX_OFF_RET);
        br->hitNo = hitNo;
        br->msSinceCtor = sinceCtor;
        br->firstRow = firstRow;
        InterlockedExchange(&br->valid, 1);
    }
}

// ---------------------------------------------------------------------------
// v17: the v16 width override presets are removed (the corridor width is not
// the lever, and `set 2.0 all` crashed the game in E2). g_overrideOn stays 0,
// so BuilderHit never writes. F6 switches the corner fix instead (below).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Soldier trace (v16 A1)
//
// Orders: FUN_101bc880 entry. ECX = controller, unit = [ctrl+4]
// (FUN_100dbce0), soldier array [unit+0x1C], count [unit+0x18]. Soldier i of
// the array at entry is sent to slot i (docs/soldier_steering_x86.md S3).
// Go-to: FUN_103dbbf0 entry, ECX = soldier, [ESP+4] = order (0x2C bytes,
// FUN_10351130 copies ten dwords and two bytes); FUN_103dafb0 entry, ECX =
// soldier (its slot is off the navmesh; no target).
// Samples: from the FUN_101a9750 hook (this = controller), per unit.
// ---------------------------------------------------------------------------

static const int ORDER_BYTES = 0x2C;
static const int MAX_SOLDIERS = 256;          // per unit, rows and order lists

struct OrderRecord {
    volatile LONG valid;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t ctrl;
    uint32_t unit;
    uint32_t count;            // [unit+0x18]
    uint32_t cursorNode;       // [ctrl+0x34]
    uint32_t cursorT;          // [ctrl+0x38] (float bits)
    uint32_t retAbs;
    int32_t  listStart;        // index into g_soldierPool, -1 none
    uint32_t listed;           // soldier pointers stored
    uint32_t leaderIdx;        // v37: [ctrl+0x30], the soldier whose path projection is the unit cursor
    uint32_t ready;            // v37: byte [ctrl+0x0C], all soldiers ready (FUN_103cbe40)
    uint32_t speedMode;        // v37: [ctrl+0x24], speed mode passed to FUN_1018b310 (0 walk, 1 run, 2 charge)
};

enum { GOTO_GOTO = 1, GOTO_OFFMESH = 2 };
struct GotoRecord {
    volatile LONG valid;
    uint8_t  kind;
    uint8_t  orderLen;         // bytes copied (0 or ORDER_BYTES)
    uint16_t pad;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t soldier;
    uint32_t retAbs;
    uint32_t orderPtr;
    uint8_t  order[ORDER_BYTES];
};

struct SampleRecord {
    volatile LONG valid;
    uint8_t  reason;           // 1 first, 2 moving, 3 periodic (idle)
    uint8_t  pad[3];
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t sampleNo;
    uint32_t ctrl;
    uint32_t unit;
    uint32_t count;            // [unit+0x18] as read
    int32_t  firstRow;
    uint32_t rows;
};

enum {
    SR_POS_OK  = 1,    // s+0x48..+0x64 read
    SR_LOC_OK  = 2,    // s+0x524 / +0x52C read
    SR_S_OK    = 4,    // go-to fields on s read
    SR_E_SELF  = 8,    // [s+0x2F0] == s
    SR_E_OK    = 16,   // [s+0x2F0] != s and its go-to fields read
    SR_E_BAD   = 32,   // [s+0x2F0] null, out of range or unreadable
};
struct SoldierRow {
    uint32_t sampleNo;
    uint16_t index;
    uint8_t  flags;            // SR_*
    uint8_t  pad;
    uint32_t soldier;
    uint32_t ent;              // [s+0x2F0]
    float    pos[3];           // s+0x48, +0x4C, +0x50
    uint16_t heading;          // s+0x64
    uint16_t pad2;
    float    loc[2];           // s+0x524, s+0x52C
    uint32_t sState, sPath;    // s+0x3BC, s+0x3CC
    float    sDest[2];         // s+0x420, s+0x428
    float    sWp[2];           // s+0x454, s+0x45C
    uint32_t eState, ePath;    // the same on e when e != s
    float    eDest[2];
    float    eWp[2];
    uint32_t corridor;         // v17: [R+0x0C], R = s+0x3CC (0 if unread)
    float    radius;           // v17: [R+0x50] (2 * r0 * scale + 0.5)
    // v37: speed law fields (section AN)
    float    spdMax, spdCur, spdTgt, spdCmd, spdMul;   // s+0x110, +0x130, +0x150, +0x1AC, +0x1BC
    float    ordSpd, ordT, elapsed;                   // s+0x438, +0x43C, +0x46C
    uint8_t  ordMode, moving, direct, f384, f308;     // s+0x440, +0xF0, +0x198, +0x384, +0x308
    uint8_t  pad3[3];
};

static const int ORDERREC_SIZE = 32 * 1024;
static const int POOL_SIZE     = 512 * 1024;  // soldier pointers listed by order records
static const int GOTO_SIZE     = 256 * 1024;   // v19 (v18: 160 K)
static const int SAMPLE_SIZE   = 64 * 1024;
static const int SROW_SIZE     = 1024 * 1024;  // v27: run 20261015_201332 filled 512 K at ~410 s (v19..v26: 512 K)

static OrderRecord*  g_orders = nullptr;
static uint32_t*     g_soldierPool = nullptr;
static GotoRecord*   g_gotos = nullptr;
static SampleRecord* g_samples = nullptr;
static SoldierRow*   g_srows = nullptr;
static volatile LONG g_orderIdx = 0, g_poolIdx = 0, g_gotoIdx = 0, g_sampleIdx = 0, g_srowIdx = 0;
static volatile LONG g_orderOverflow = 0, g_poolOverflow = 0, g_gotoOverflow = 0;
static volatile LONG g_sampleOverflow = 0, g_srowOverflow = 0;
static volatile LONG g_entSelf = 0, g_entOther = 0, g_entBad = 0;   // soldier rows by [s+0x2F0] kind

// Per-unit sampling state.
static const int UNIT_TABLE = 128;
static const DWORD SAMPLE_MOVING_MS = 100;
static const DWORD SAMPLE_IDLE_MS = 5000;
struct UnitState {
    uint32_t unit;
    DWORD    lastCheck;        // GetTickCount of the last position check
    DWORD    lastRow;          // GetTickCount of the last recorded sample, 0 none yet
    float    lastSum;          // sum of |x| + |z| over the soldiers at the last check
    uint32_t lastCount;
    volatile LONG busy;        // a thread is sampling this unit
    volatile LONG announced;   // logged by the dump thread
    volatile LONG firstDone;   // first sample stored (fields below valid)
    uint32_t firstSoldier;
    uint32_t firstEnt;
    uint32_t firstCount;
    uint32_t firstCtrl;
};
static UnitState g_units[UNIT_TABLE] = {};
static int g_unitsUsed = 0;
static volatile LONG g_unitLock = 0;
static volatile LONG g_unitOverflow = 0;

// PtrPlausible and MemcpyTry: proxy/gamemem.h.

static void FixTablesUpdate(uint32_t unit, const uint32_t* ptrs, uint32_t n);   // fix, below
static void SoldierSampleUnit(uint32_t unit, uint32_t ctrl, DWORD now, DWORD elapsed);   // below

static void GPlan(uint32_t ctrl, uint32_t unit, uint32_t n, DWORD now);   // v38 G (defined with AdvanceHit)

static void OrdersHit(const uint8_t* ctx) {
    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    uint32_t hitNo = 0;
    RateDecide(H_ORDERS, now, inWindow, &hitNo);   // counters only; every hit is recorded

    // v17: the soldier -> unit table and the unit's order batch are kept up
    // to date on every hit, window or not (the fix runs after the window too).
    uint32_t ctrl = CtxDword(ctx, CTX_OFF_ECX);
    GameClockNote(ctrl);   // v36: the engine's tick counter hangs off the controller
    uint32_t unit = 0, count = 0, arr = 0, n = 0;
    uint32_t ptrs[MAX_SOLDIERS];
    bool listOk = false;
    if (ReadDword(ctrl + 0x04, &unit) && PtrPlausible(unit) &&
        ReadDword(unit + 0x18, &count) && ReadDword(unit + 0x1C, &arr) && PtrPlausible(arr)) {
        n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
        if (n && MemcpyTry(ptrs, arr, n * 4)) {
            listOk = true;
            FixTablesUpdate(unit, ptrs, n);
        }
    }
    GPlan(ctrl, unit, n, now);   // v38 G: walk the path from the cursor, note the narrow runs for AdvanceHit
    if (!inWindow) return;
    if (listOk) SoldierSampleUnit(unit, ctrl, now, elapsed);   // v18, rate limited per unit

    LONG idx = InterlockedIncrement(&g_orderIdx) - 1;
    if (idx >= ORDERREC_SIZE) { InterlockedExchange(&g_orderOverflow, 1); return; }
    OrderRecord& r = g_orders[idx];
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.ctrl = ctrl;
    r.retAbs = CtxDword(ctx, CTX_OFF_RET);
    r.unit = unit;
    r.count = count;
    r.cursorNode = r.cursorT = 0;
    r.listStart = -1;
    r.listed = 0;
    ReadDword(r.ctrl + 0x34, &r.cursorNode);
    ReadDword(r.ctrl + 0x38, &r.cursorT);
    r.leaderIdx = 0xFFFFFFFF; r.ready = 0xFF; r.speedMode = 0xFFFFFFFF;   // v37
    ReadDword(r.ctrl + 0x30, &r.leaderIdx);
    { uint8_t rb = 0xFF; if (MemcpyTry(&rb, r.ctrl + 0x0C, 1)) r.ready = rb; }
    ReadDword(r.ctrl + 0x24, &r.speedMode);
    if (listOk) {
        LONG start = InterlockedExchangeAdd(&g_poolIdx, (LONG)n);
        if (start + (LONG)n <= POOL_SIZE) {
            memcpy(&g_soldierPool[start], ptrs, n * 4);
            r.listStart = start;
            r.listed = n;
        } else {
            InterlockedExchange(&g_poolOverflow, 1);
        }
    }
    InterlockedExchange(&r.valid, 1);
}

static void GotoHit(const uint8_t* ctx, int kind) {
    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    uint32_t hitNo = 0;
    RateDecide(kind == GOTO_GOTO ? H_GOTO : H_OFFMESH, now, inWindow, &hitNo);
    if (!inWindow) return;
    LONG idx = InterlockedIncrement(&g_gotoIdx) - 1;
    if (idx >= GOTO_SIZE) { InterlockedExchange(&g_gotoOverflow, 1); return; }
    GotoRecord& r = g_gotos[idx];
    r.kind = (uint8_t)kind;
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.soldier = CtxDword(ctx, CTX_OFF_ECX);
    r.retAbs = CtxDword(ctx, CTX_OFF_RET);
    r.orderPtr = CtxDword(ctx, CTX_OFF_ARG1);
    r.orderLen = 0;
    if (kind == GOTO_GOTO && MemcpyTry(r.order, r.orderPtr, ORDER_BYTES)) r.orderLen = ORDER_BYTES;
    InterlockedExchange(&r.valid, 1);
}

// Reads one soldier into a row. Returns false only if the position is unreadable.
static bool ReadSoldier(uint32_t s, SoldierRow& w) {
    w.soldier = s;
    w.flags = 0;
    w.ent = 0;
    uint8_t blk[0x20];
    if (MemcpyTry(blk, s + 0x48, 0x20)) {
        memcpy(w.pos, blk, 12);
        memcpy(&w.heading, blk + 0x1C, 2);   // s+0x64
        w.flags |= SR_POS_OK;
    } else {
        w.pos[0] = w.pos[1] = w.pos[2] = 0.0f;
        w.heading = 0;
    }
    float loc[3];
    if (MemcpyTry(loc, s + 0x524, 12)) { w.loc[0] = loc[0]; w.loc[1] = loc[2]; w.flags |= SR_LOC_OK; }
    else w.loc[0] = w.loc[1] = 0.0f;

    // go-to fields, one read of +0x3BC .. +0x460: +0x3BC state, +0x3CC path
    // result, +0x420 / +0x428 destination, +0x454 / +0x45C current waypoint
    uint8_t g[0xA4];
    w.sState = w.sPath = 0;
    w.sDest[0] = w.sDest[1] = w.sWp[0] = w.sWp[1] = 0.0f;
    if (MemcpyTry(g, s + 0x3BC, sizeof(g))) {
        memcpy(&w.sState, g + 0x00, 4);
        memcpy(&w.sPath, g + 0x10, 4);
        memcpy(&w.sDest[0], g + 0x64, 4);
        memcpy(&w.sDest[1], g + 0x6C, 4);
        memcpy(&w.sWp[0], g + 0x98, 4);
        memcpy(&w.sWp[1], g + 0xA0, 4);
        w.flags |= SR_S_OK;
    }
    // v37: speed law fields, two block reads (+0xF0..+0x1BF and +0x430..+0x46F) and two bytes
    w.spdMax = w.spdCur = w.spdTgt = w.spdCmd = w.spdMul = 0.0f;
    w.ordSpd = w.ordT = w.elapsed = 0.0f;
    w.ordMode = w.moving = w.direct = w.f384 = w.f308 = 0;
    {
        uint8_t k[0xD0];
        if (MemcpyTry(k, s + 0xF0, sizeof(k))) {
            w.moving = k[0];
            memcpy(&w.spdMax, k + (0x110 - 0xF0), 4);
            memcpy(&w.spdCur, k + (0x130 - 0xF0), 4);
            memcpy(&w.spdTgt, k + (0x150 - 0xF0), 4);
            w.direct = k[0x198 - 0xF0];
            memcpy(&w.spdCmd, k + (0x1AC - 0xF0), 4);
            memcpy(&w.spdMul, k + (0x1BC - 0xF0), 4);
        }
        uint8_t q[0x40];
        if (MemcpyTry(q, s + 0x430, sizeof(q))) {
            memcpy(&w.ordSpd, q + 0x08, 4);
            memcpy(&w.ordT, q + 0x0C, 4);
            w.ordMode = q[0x10];
            memcpy(&w.elapsed, q + 0x3C, 4);
        }
        uint8_t b1 = 0;
        if (MemcpyTry(&b1, s + 0x308, 1)) w.f308 = b1;
        if (MemcpyTry(&b1, s + 0x384, 1)) w.f384 = b1;
    }
    w.corridor = 0;
    w.radius = 0.0f;
    if (PtrPlausible(w.sPath)) {
        uint32_t c = 0;
        float rad = 0.0f;
        if (ReadDword(w.sPath + 0x0C, &c)) w.corridor = c;
        if (MemcpyTry(&rad, w.sPath + 0x50, 4)) w.radius = rad;
    }
    w.eState = w.ePath = 0;
    w.eDest[0] = w.eDest[1] = w.eWp[0] = w.eWp[1] = 0.0f;
    uint32_t e = 0;
    if (!ReadDword(s + 0x2F0, &e) || !PtrPlausible(e)) {
        w.flags |= SR_E_BAD;
    } else if (e == s) {
        w.flags |= SR_E_SELF;
    } else if (MemcpyTry(g, e + 0x3BC, sizeof(g))) {
        memcpy(&w.eState, g + 0x00, 4);
        memcpy(&w.ePath, g + 0x10, 4);
        memcpy(&w.eDest[0], g + 0x64, 4);
        memcpy(&w.eDest[1], g + 0x6C, 4);
        memcpy(&w.eWp[0], g + 0x98, 4);
        memcpy(&w.eWp[1], g + 0xA0, 4);
        w.flags |= SR_E_OK;
    } else {
        w.flags |= SR_E_BAD;
    }
    w.ent = e;
    return (w.flags & SR_POS_OK) != 0;
}

static void SampleStore(LONG si, int reason, uint32_t ctrl, uint32_t unit, uint32_t count,
                        LONG first, uint32_t rows, DWORD elapsed) {
    SampleRecord& sr = g_samples[si];
    sr.reason = (uint8_t)reason;
    sr.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    sr.timestamp = elapsed;
    sr.threadId = GetCurrentThreadId();
    sr.sampleNo = (uint32_t)si;
    sr.ctrl = ctrl;
    sr.unit = unit;
    sr.count = count;
    sr.firstRow = first;
    sr.rows = rows;
    InterlockedExchange(&sr.valid, 1);
}

// Called from the FUN_101a9750 hook (game thread) while the window is open.
// One unit is sampled by one thread at a time (busy flag); several
// controllers of the same unit share its state.
static void SoldierSampleUnit(uint32_t unit, uint32_t ctrl, DWORD now, DWORD elapsed);

static void SoldierSample(uint32_t ctrl, DWORD now, DWORD elapsed) {
    uint32_t unit = 0;
    if (!ReadDword(ctrl + 0x04, &unit) || !PtrPlausible(unit)) return;
    SoldierSampleUnit(unit, ctrl, now, elapsed);
}

// v18: also called from FUN_101bc880 (OrdersHit) and the fix hook with the
// unit known (ctrl 0 from the fix hook), so a unit is sampled even when
// FUN_101a9750 is silent (run 20261014_221943, R1).
static void SoldierSampleUnit(uint32_t unit, uint32_t ctrl, DWORD now, DWORD elapsed) {
    if (!g_samples || !g_srows) return;
    uint32_t count = 0, arr = 0;

    UnitState* u = nullptr;
    bool go = false;
    SpinLock(&g_unitLock);
    for (int i = 0; i < g_unitsUsed; i++) if (g_units[i].unit == unit) { u = &g_units[i]; break; }
    if (!u) {
        if (g_unitsUsed < UNIT_TABLE) {
            u = &g_units[g_unitsUsed++];
            u->unit = unit;
            u->lastCheck = now - SAMPLE_IDLE_MS;
            u->lastRow = 0;
            u->lastSum = 0.0f;
            u->lastCount = 0;
            u->busy = 0;
            u->announced = 0;
            u->firstDone = 0;
            u->firstCtrl = ctrl;
        } else {
            InterlockedExchange(&g_unitOverflow, 1);
        }
    }
    if (u && !u->busy && now - u->lastCheck >= SAMPLE_MOVING_MS) {
        u->busy = 1;
        u->lastCheck = now;
        go = true;
    }
    SpinUnlock(&g_unitLock);
    if (!go) return;

    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0) {
        InterlockedExchange(&u->busy, 0);
        return;
    }
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) { InterlockedExchange(&u->busy, 0); return; }

    // Movement test: sum of |x| + |z| over the soldiers (any move changes it).
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float xz[3];
        if (MemcpyTry(xz, ptrs[i] + 0x48, 12)) sum += fabsf(xz[0]) + fabsf(xz[2]);
    }
    int reason = 0;
    if (u->lastRow == 0) reason = 1;
    else if (fabsf(sum - u->lastSum) > 0.05f || count != u->lastCount) reason = 2;
    else if (now - u->lastRow >= SAMPLE_IDLE_MS) reason = 3;
    u->lastSum = sum;
    u->lastCount = count;
    if (!reason) { InterlockedExchange(&u->busy, 0); return; }
    u->lastRow = now ? now : 1;

    LONG si = InterlockedIncrement(&g_sampleIdx) - 1;
    if (si >= SAMPLE_SIZE) { InterlockedExchange(&g_sampleOverflow, 1); InterlockedExchange(&u->busy, 0); return; }
    LONG first = InterlockedExchangeAdd(&g_srowIdx, (LONG)n);
    if (first + (LONG)n > SROW_SIZE) {
        InterlockedExchange(&g_srowOverflow, 1);
        SampleStore(si, reason, ctrl, unit, count, -1, 0, elapsed);
        InterlockedExchange(&u->busy, 0);
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        SoldierRow& w = g_srows[first + i];
        w.sampleNo = (uint32_t)si;
        w.index = (uint16_t)i;
        ReadSoldier(ptrs[i], w);
        if (w.flags & SR_E_SELF) InterlockedIncrement(&g_entSelf);
        else if (w.flags & SR_E_OK) InterlockedIncrement(&g_entOther);
        else InterlockedIncrement(&g_entBad);
    }
    if (reason == 1) {
        u->firstSoldier = ptrs[0];
        u->firstEnt = g_srows[first].ent;
        u->firstCount = count;
        InterlockedExchange(&u->firstDone, 1);
    }
    SampleStore(si, reason, ctrl, unit, count, first, n, elapsed);
    InterlockedExchange(&u->busy, 0);
}

// ---------------------------------------------------------------------------
// Corner waypoint fix (v18; docs/PHASE12b_fix_plan.md, geometry corrected in
// docs/ghidra_analysis_v11.md F2 and docs/waypoint_choice_x86.md, correction)
//
// Hook point 0x103df6c0 in FUN_103df3f0, right after CALL FUN_10394e80
// (RET 0x1C). Frame (src/stack_offsets_v17.h, tools/stack_calc.py --mid):
//   EAX              waypoint buffer {x, y, z, u16 heading, float}, copied to
//                    soldier+0x454..+0x467 by the next instructions
//   EBX              soldier (FUN_103df3f0's this)
//   [ESP+0x14]       side object (portal) of the returned apex, 0 when the
//                    goal (slot) was returned (out-argument 2)
//   ESP+0x44         the buffer itself (EAX must equal it: frame check)
// Side object: +0x08 type, +0x10 special link (door / ladder object), +0x20
// vertex record E. Vertex record: position +0x0C / +0x10 (x, z), offset
// (miter) vector m +0x24 / +0x28, next record +0x08; +0x30 is only a clamp
// limit (0.0 live). For type 0 FUN_10393000 returns the left candidate
// pA = A + mA * r (A, mA from E) and FUN_103a0080 the right one
// pB = B + mB * r (B, mB from [E+8]); r = 0.5 * [R+0x50], R = [soldier+0x3CC].
// The lane is the segment apex -> other candidate.
// Only x and z of the buffer are ever written, and only after every read of
// the call succeeded.
// ---------------------------------------------------------------------------

enum { FIX_OFF = 0, FIX_SLOT_LANE = 1, FIX_OWN_POS = 2, FIX_V26_B = 3, FIX_V26_C = 4, FIX_V27_D = 5, FIX_V31_F = 6,
       FIX_V33_L = 7, FIX_V34_R = 8, FIX_V36_RC = 9, FIX_V38_G = 10, FIX_MODES = 11 };
static const char* const kFixModeText[FIX_MODES] = {
    "off (waypoints untouched)",
    "lead the turn (120 deg round the corner, free point) + no crowd damping near corners",
    "own position (projection of the soldier's position) + no crowd damping near corners",
    "lead the turn + far lanes (6..30 m) + no crowd damping within 30 m",
    "lead the turn + no crowd damping within 30 m",
    "lead the turn + no crowd damping within 30 m + gather at the queue",
    "lead the turn + no crowd damping within 30 m + tip lanes (concentric lanes round a funnel corner)",
    "lead the turn + no crowd damping within 30 m + look-ahead avoidance (go round stalled and crossing men)",
    "radius lanes keyed to the unit's busy corner (request radius by rank, overfull lane -> neighbour lane) + no crowd damping within 30 m",
    "radius lanes keyed to the unit's busy corner (request radius by rank, overfull lane -> neighbour lane) + lead the turn + no crowd damping within 30 m",
    "RC + release beyond the choke (the slot block is laid past the narrow run ahead instead of inside it; the anchor is held until the leader's point passes it)" };
static const char* const kFixVariantName[4] = { "off", "C", "RC", "G" };
static inline bool FixLeadMode(LONG m) {
    return m == FIX_SLOT_LANE || m == FIX_V26_B || m == FIX_V26_C || m == FIX_V27_D || m == FIX_V31_F ||
           m == FIX_V33_L || m == FIX_V36_RC || m == FIX_V38_G;
}
static const int   TIP_MIN_MEN = 8;           // v31 F: men queueing for the corner (self included)
static const float TIP_W_M = 4.0f;            // v31 F: lane band width cap (outermost lane rP + W)
static const float TIP_MERGE_M = 1.5f;        // v31 F: apex vertices this close to the stored tip are the same tip
static const DWORD TIP_KEEP_MS = GT_MS(3000);        // v31 F: lane kept this long after the last tip call
static const float TIP_MIN_LANE_M = 1.0f;     // v31 F: a new lane needs a portal lane at least this long
static const float TIP_AHEAD_FAR_DEG = 30.0f; // v31 F: beyond the tangent point, 3 m or more outside the circle
static const float TIP_AHEAD_NEAR_DEG = 60.0f;   // v31 F: ... on or inside the circle
// v34 R radius lanes
static const int   RQ_LANES = 3;
static const float RQ_STEP_M = 1.5f;          // request radius w per lane step (corner point + 0.75 * |m| m)
static const float RQ_W_MAX = 4.2f;
static const float RQ_BALANCE = 1.3f;         // a lane is overfull above 1.3 * n / 3 of the queue
static const int   RQ_MIN_MEN = 8;
static const DWORD RQ_APEX_MS = GT_MS(1500);
static const DWORD RQ_KEEP_MS = GT_MS(8000);         // v35: survives the walk to the corner without replans
static const DWORD RQ_UNIT_MS = GT_MS(3000);         // v35: members' apex vertices this fresh define the unit's busy corners
static const float RQ_AHEAD_MIN_M = 1.0f;     // v35: slot at least this far away for the ahead test
static const float RQ_PASSED_M = 2.0f;        // v35: a vertex behind the man by more than this is passed
static const int   RQ_MAX_CAND = 4;
static const int   RQ_MAX_CLUSTER = 8;
static const float RQ_MERGE_M = 1.5f;
static const DWORD RQ_ROW_MS = 200;
static const int   RQ_FRAME_PERMILLE = 100;
static const float GATHER_BEHIND_M = 1.0f;    // v27 D: slot this much farther from the vertex than the man
static const float GATHER_ZONE_M = 30.0f;     // v27 D: man within this distance of the queue vertex
static const float GATHER_STEP_M = 4.0f;      // v27 D: waypoint this far towards the queue apex
static const int   GATHER_MIN_MEN = 8;        // v27 D: men with a fresh apex call on the queue vertex
static const float FIX_FAR_ZONE_M = 30.0f;    // v26 B: far lanes for men up to this far from the apex vertex
static const float FIX_FAR_LANE_M = 10.0f;    // v26 B: far-lane slide cap along the portal lane

enum {
    FS_ADJUSTED = 0,   // waypoint moved along the lane (s may be 0)
    FS_OFF,            // fix off: recorded only
    FS_LINK,           // portal with a special link: not a vertex apex
    FS_READ,           // a read failed or a value was not finite
    FS_GEOMETRY,       // apex side: |m| outside [0.95, 12] or r outside (0, 25)
    FS_MISMATCH,       // waypoint is neither pA nor pB
    FS_NARROW,         // lane shorter than 0.1 m, or pA / pB crossed over
    FS_NOUNIT,         // mode 1: soldier not in the soldier -> unit table yet
    FS_UNITREAD,       // mode 1: no slot of the unit readable
    FS_DISABLED,       // a self-check switched the fix off
    FS_TYPE,           // v20: the endpoint helpers failed the install byte check
    FS_OTHEREND,       // v20: unused (a zero other end is pulled in by r instead)
    FS_STUCK,          // v21: no progress, the game's waypoint is kept for FIX_HOLD_MS
    FS_ZONE,           // v23: farther than FIX_LEAD_ZONE_M from the corner: the game's point
    FS_PAST,           // v23: already beyond the far edge of the corner's free sector
    FS_COUNT
};
static const char* const kFixStatus[FS_COUNT] = {
    "adjusted", "mode_off", "special_link", "read_fail", "geometry", "vertex_mismatch",
    "narrow", "no_unit", "unit_read_fail", "disabled", "no_helpers", "other_end", "stuck_release",
    "outside_zone", "past_sector" };

static const float FIX_MATCH_M = 0.02f;       // apex candidate tolerance (2 cm)
static const float FIX_NARROW_M = 0.1f;       // shortest usable lane
static const float FIX_M_MIN = 0.95f;         // |m| range of a vertex offset vector
static const float FIX_M_MAX = 12.0f;         // (1 / sin(half angle): 12 = a 9.6 degree spike)
static const int   FIX_CHECK_CALLS = 200;     // type-0 apex calls with structural checks
static const int   FIX_GEO_PERMILLE = 20;     // geometry: more than 2 % failing disables
static const int   FIX_MATCH_PERMILLE = 100;  // apex match: more than 10 % failing disables
static const DWORD FIX_ROW_MS = 100;          // fix rows: at most 10 Hz per soldier
static const float FIX_TURN_DEG = 25.0f;      // v21 turn limit
static const float FIX_TAN_TURN = 0.46630766f;   // tan(25 degrees)
static const float FIX_STUCK_MOVE_M = 0.5f;   // v21 stuck release: progress needed ...
static const DWORD FIX_STUCK_MS = GT_MS(1500);       // ... within this time
static const DWORD FIX_HOLD_MS = GT_MS(4000);        // game waypoint kept this long after a release
static const DWORD FIX_SEEN_GAP_MS = GT_MS(1000);    // apex calls further apart restart the progress clock
static const float FIX_LAT_LANE_M = 4.0f;     // v22 lateral rank: lane length cap
static const float FIX_GROUP_ZONE_M = 30.0f;  // members within this distance of the corner
static const float FIX_GROUP_V_M = 1.0f;      // same corner: stored vertex within this distance
static const DWORD FIX_GROUP_MS = GT_MS(1500);       // stored apex younger than this (replans about every 0.5 s)
static const float FIX_LEAD_ZONE_M = 6.0f;    // v23 lead the turn: zone round the corner vertex
static const float FIX_LEAD_DEG = 120.0f;     // lead angle round the vertex
static const float FIX_LEAD_RMAX_M = 4.0f;    // radius at most |P - V| + this (and the portal lane)
static const float FIX_LEAD_OCC_M = 0.7f;     // another man this close to the point: occupied
static const float FIX_LEAD_STEP_M = 0.7f;    // radius steps when occupied
static const float FIX_LEAD_MARGIN_DEG = 15.0f;   // kept off the free sector's edge
static const DWORD FIX_LEAD_KEEP_MS = GT_MS(3000);   // turning sign kept this long for the same corner
static const float FIX_PI = 3.14159265f;

struct FixRecord {
    volatile LONG valid;
    uint8_t  mode;             // effective mode (0 when the fix is off)
    uint8_t  status;           // FS_*
    uint8_t  side;             // 0 none, 1 apex pA (left), 2 apex pB (right)
    uint8_t  clamped;          // v21: bit 1 lane end, bit 2 turn limit
    uint32_t seq;
    uint32_t timestamp;
    uint32_t threadId;
    uint32_t soldier;
    uint32_t unit;             // from the soldier -> unit table, 0 unknown
    uint32_t portal;
    uint32_t type;             // [portal+0x08]
    uint32_t edge;             // E = [portal+0x20]
    uint32_t twin;             // [E+8]
    uint32_t batch;            // unit's order batch (FUN_101bc880 hits)
    float    len;              // |B - A|
    float    r;
    float    e30;              // E+0x30 (clamp limit)
    float    a[2], b[2];       // vertex positions
    float    ma[2], mb[2];     // vertex offset vectors
    float    pa[2], pb[2];     // candidates A + mA * r, B + mB * r
    float    lane;             // L = |other - apex|
    float    before[3];        // buffer x, y, z as FUN_10394e80 returned it
    float    after[2];         // buffer x, z after the handler
    float    t, ti, tmin, tmax, scale;   // t = s, the shift from the apex along the lane
    float    pos[2];           // soldier +0x48 / +0x50
    float    dest[2];          // soldier +0x420 / +0x428 (slot)
};
static const int FIXREC_SIZE = 256 * 1024;
static FixRecord* g_fixRecs = nullptr;
static volatile LONG g_fixRecIdx = 0;
static volatile LONG g_fixRecOverflow = 0;

// Raw bytes of the first FIXRAW_SIZE distinct apex side objects (window only),
// so the layout can be re-derived offline if a check fails.
struct FixRawRecord {
    volatile LONG valid;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t portal, edge, twin;
    uint32_t plen, elen, tlen;
    uint8_t  pbytes[0x40];
    uint8_t  ebytes[0x40];
    uint8_t  tbytes[0x40];
};
static const int FIXRAW_SIZE = 64;
static FixRawRecord g_fixRaw[FIXRAW_SIZE] = {};
static volatile LONG g_fixRawIdx = 0;

static volatile LONG g_fixOn = 0;             // F6; OFF at launch
static volatile LONG g_fixLane = FIX_SLOT_LANE;   // F7: 1 slot lane <-> 2 own position
static volatile LONG g_fixMode = FIX_OFF;     // effective: g_fixOn ? g_fixLane : 0
static volatile LONG g_fixVariant = 0;        // v38: I cycles 0 off, 1 C, 2 RC, 3 G
static volatile LONG g_fixFarLane = 0;        // v26: far-lane waypoints written
static volatile LONG g_fixDisabled = 0;
static const char* volatile g_fixDisabledWhy = "";
static volatile LONG g_fixDisabledLogged = 0;
static volatile LONG g_fixCalls = 0, g_fixGoal = 0, g_fixApex = 0, g_fixFaults = 0;
static volatile LONG g_fixFrameChecked = 0, g_fixFrameFail = 0;
static volatile LONG g_fixGeoChecked = 0, g_fixGeoFail = 0;
static volatile LONG g_fixMatchChecked = 0, g_fixMatchFail = 0;
static volatile LONG g_fixStatus[FS_COUNT] = {};
static volatile LONG g_fixLaneMiss = 0;
static volatile LONG g_fixSamples = 0;        // soldier samples triggered from the fix / orders hooks

// Soldier -> unit table (open addressing, never shrinks; a reused soldier
// address is re-keyed by the next FUN_101bc880 hit of its new unit), unit
// order batches and the lane cache, all under one SRW lock. Game memory is
// never read while the lock is held.
struct FixSoldier {
    volatile LONG soldier;     // key, 0 empty
    volatile LONG unit;
    volatile LONG lastRow;     // GetTickCount of this soldier's last fix row
    // v21 stuck release (written only from the fix hook; races are benign)
    float    ax, az;           // anchor position
    DWORD    moveTick;         // last time the soldier was FIX_STUCK_MOVE_M from the anchor, 0 none
    DWORD    seenTick;         // last apex call
    DWORD    holdUntil;        // game waypoint kept until this tick, 0 none
    // v22 lateral rank: last game apex and its corner vertex
    float    apx, apz, vx, vz;
    volatile LONG apexTick;    // GetTickCount of the last apex call, 0 none (goal call clears it)
    // v23 lead the turn: turning sign for the current corner
    float    leadVx, leadVz, leadSgn;
    DWORD    leadTick;
    DWORD    vTick;            // v27: last apex call (vx / vz), never cleared by goal calls
    // v31 F tip lane
    float    tipVx, tipVz, tipR, tipSgn, tipBeta, tipHalf, tipQ;
    DWORD    tipTick;          // last tip call, 0 none
    // v33 L look-ahead avoidance
    volatile LONG wpTick;      // last waypoint call (goal or apex), 0 none
    float    avPx, avPz;       // position at the last integrator call
    DWORD    avPosTick;
    float    avDx, avDz;       // the D written on that call (when turned)
    LONG     avTurned;
    LONG     avSide;           // +1 / -1 side held, 0 none
    DWORD    avHoldUntil;
    LONG     avWallSide;
    DWORD    avWallUntil;
    volatile LONG avLastRow;
    // v34 R radius lane
    LONG     rLane;
    float    rVx, rVz;
    DWORD    rTick;            // last lane decision, 0 none
    volatile LONG rRow;
};
static const int FIX_SOLDIER_TABLE = 16384;   // power of two
static FixSoldier* g_fixSoldiers = nullptr;
static volatile LONG g_fixSoldierUsed = 0, g_fixSoldierFull = 0;

struct FixUnit { uint32_t unit; uint32_t batch; };
static const int FIX_UNIT_TABLE = 1024;       // power of two
static FixUnit g_fixUnits[FIX_UNIT_TABLE] = {};
static volatile LONG g_fixUnitFull = 0;

struct LaneEntry { uint32_t unit, portal, batch, side; float tmin, tmax; };
static const int LANE_CACHE = 512;            // power of two, direct mapped
static LaneEntry g_lane[LANE_CACHE] = {};

static SRWLOCK g_fixLock = SRWLOCK_INIT;

static inline uint32_t FixHash(uint32_t p) { return (p >> 2) * 2654435761u; }

static void AddControlEvent(uint8_t hook, uint16_t no, uint32_t a0, uint32_t a1, DWORD elapsed);   // below

static void FixDisable(const char* why) {
    if (InterlockedExchange(&g_fixDisabled, 1) == 0) {
        g_fixDisabledWhy = why;
        InterlockedExchange(&g_fixOn, 0);
        InterlockedExchange(&g_fixVariant, 0);
        InterlockedExchange(&g_fixMode, FIX_OFF);
    }
}

// Lock held exclusively by the caller.
static uint32_t* FixUnitBatchSlot(uint32_t unit, bool create) {
    uint32_t h = FixHash(unit) & (FIX_UNIT_TABLE - 1);
    for (int i = 0; i < FIX_UNIT_TABLE; i++) {
        FixUnit& u = g_fixUnits[(h + i) & (FIX_UNIT_TABLE - 1)];
        if (u.unit == unit) return &u.batch;
        if (u.unit == 0) {
            if (!create) return nullptr;
            u.unit = unit;
            u.batch = 0;
            return &u.batch;
        }
    }
    InterlockedExchange(&g_fixUnitFull, 1);
    return nullptr;
}

// FUN_101bc880 hit: a new order batch for the unit, and every listed soldier
// is (re)keyed to it.
static void FixTablesUpdate(uint32_t unit, const uint32_t* ptrs, uint32_t n) {
    if (!g_fixSoldiers) return;
    AcquireSRWLockExclusive(&g_fixLock);
    uint32_t* batch = FixUnitBatchSlot(unit, true);
    if (batch) (*batch)++;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t s = ptrs[k];
        if (!PtrPlausible(s)) continue;
        uint32_t h = FixHash(s) & (FIX_SOLDIER_TABLE - 1);
        bool placed = false;
        for (int i = 0; i < 64; i++) {
            FixSoldier& e = g_fixSoldiers[(h + i) & (FIX_SOLDIER_TABLE - 1)];
            if ((uint32_t)e.soldier == s) { e.unit = (LONG)unit; placed = true; break; }
            if (e.soldier == 0) {
                e.unit = (LONG)unit;
                e.lastRow = 0;
                e.moveTick = 0;
                e.seenTick = 0;
                e.holdUntil = 0;
                e.apexTick = 0;
                e.leadTick = 0;
                InterlockedExchange(&e.soldier, (LONG)s);
                InterlockedIncrement(&g_fixSoldierUsed);
                placed = true;
                break;
            }
        }
        if (!placed) InterlockedExchange(&g_fixSoldierFull, 1);
    }
    ReleaseSRWLockExclusive(&g_fixLock);
}

// Returns the table entry of the soldier (or null) and its unit / batch.
static FixSoldier* FixLookup(uint32_t s, uint32_t* unit, uint32_t* batch) {
    *unit = 0;
    *batch = 0;
    if (!g_fixSoldiers) return nullptr;
    FixSoldier* found = nullptr;
    AcquireSRWLockShared(&g_fixLock);
    uint32_t h = FixHash(s) & (FIX_SOLDIER_TABLE - 1);
    for (int i = 0; i < 64; i++) {
        FixSoldier& e = g_fixSoldiers[(h + i) & (FIX_SOLDIER_TABLE - 1)];
        if ((uint32_t)e.soldier == s) { found = &e; break; }
        if (e.soldier == 0) break;
    }
    if (found) {
        *unit = (uint32_t)found->unit;
        uint32_t* b = FixUnitBatchSlot(*unit, false);   // read only with create = false
        if (b) *batch = *b;
    }
    ReleaseSRWLockShared(&g_fixLock);
    return found;
}

// Lane range of a unit across one apex: t_j = dot(slot_j - apex, u) over the
// unit's soldiers (slot = soldier+0x420 / +0x428), cached per (unit, portal,
// side, batch). Returns false when no slot is readable.
static bool FixLaneRange(uint32_t unit, uint32_t batch, uint32_t portal, uint32_t side,
                         const float apex[2], const float u[2], float* tmin, float* tmax) {
    uint32_t slot = (FixHash(unit) ^ FixHash(portal) ^ side) & (LANE_CACHE - 1);
    bool hit = false;
    AcquireSRWLockShared(&g_fixLock);
    const LaneEntry& c = g_lane[slot];
    if (c.unit == unit && c.portal == portal && c.side == side && c.batch == batch) {
        *tmin = c.tmin;
        *tmax = c.tmax;
        hit = true;
    }
    ReleaseSRWLockShared(&g_fixLock);
    if (hit) return true;

    InterlockedIncrement(&g_fixLaneMiss);
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0)
        return false;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return false;
    float lo = 0.0f, hi = 0.0f;
    int got = 0;
    for (uint32_t i = 0; i < n; i++) {
        float d3[3];
        if (!PtrPlausible(ptrs[i]) || !MemcpyTry(d3, ptrs[i] + 0x420, 12)) continue;
        float tj = (d3[0] - apex[0]) * u[0] + (d3[2] - apex[1]) * u[1];
        if (!_finite(tj)) continue;
        if (!got || tj < lo) lo = tj;
        if (!got || tj > hi) hi = tj;
        got++;
    }
    if (!got) return false;
    *tmin = lo;
    *tmax = hi;
    AcquireSRWLockExclusive(&g_fixLock);
    LaneEntry& w = g_lane[slot];
    w.unit = unit;
    w.portal = portal;
    w.side = side;
    w.batch = batch;
    w.tmin = lo;
    w.tmax = hi;
    ReleaseSRWLockExclusive(&g_fixLock);
    return true;
}

// One structural check result; switches the fix off once the failures exceed
// the allowance for the first FIX_CHECK_CALLS checks.
static void FixCheck(volatile LONG* checked, volatile LONG* failed, bool ok, int permille, const char* why) {
    if (*checked >= FIX_CHECK_CALLS) return;
    InterlockedIncrement(checked);
    if (ok) return;
    LONG f = InterlockedIncrement(failed);
    if ((LONGLONG)f * 1000 > (LONGLONG)FIX_CHECK_CALLS * permille) FixDisable(why);
}

static inline float Dist2(float ax, float az, float bx, float bz) {
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

// v20: the two candidates come from the game's own endpoint helpers, called
// exactly as FUN_10394e80 calls them (ECX = side object; out, r, clamp 0,
// layer; RET 0x10), so every side-object type is covered: type 0 vertex
// offset vectors, types 1..3 the vectors at +0x38 / +0x44 (FUN_1037f370,
// rotated by the owning object), others the edge direction. Both helpers are
// leaf code (plus FUN_1037f370 / FUN_1036e5f0 math) and are byte-checked at
// install; if the check fails the fix refuses to switch on.
typedef void (__fastcall *EndpointFn)(uint32_t ecx, uint32_t edx, float* out, float r, int clamp, int layer);
static EndpointFn g_endLeft = nullptr;    // FUN_10393000
static EndpointFn g_endRight = nullptr;   // FUN_103a0080
static volatile LONG g_fixCrossChecked = 0, g_fixCrossFail = 0, g_fixOtherPulled = 0;
static volatile LONG g_fixTurnCapped = 0, g_fixStuckReleases = 0;
static volatile LONG g_fixLatCalls = 0, g_fixLatGroupSum = 0, g_fixLatSolo = 0, g_fixLatLaneCut = 0;
static volatile LONG g_fixLeadCalls = 0, g_fixLeadSectorClamp = 0, g_fixLeadNoFree = 0, g_fixLeadMoved = 0,
                     g_fixLeadRadiusCut = 0;
static const float FIX_CROSS_M = 0.001f;  // type 0: own formula vs the game helper

// The whole computation for one apex call. Every read and both helper calls
// are inside this __try; the buffer is written last.
// v21 stuck release: true while the soldier should keep the game's waypoint.
static bool FixStuckHold(FixSoldier* e, const float pos[2], DWORD now) {
    if (!e) return false;
    bool fresh = e->seenTick == 0 || now - e->seenTick > FIX_SEEN_GAP_MS;
    e->seenTick = now ? now : 1;
    if (fresh || e->moveTick == 0 || Dist2(pos[0], pos[1], e->ax, e->az) >= FIX_STUCK_MOVE_M) {
        e->ax = pos[0];
        e->az = pos[1];
        e->moveTick = now ? now : 1;
    }
    if (e->holdUntil) {
        if ((LONG)(now - e->holdUntil) < 0) return true;
        e->holdUntil = 0;
    }
    if (now - e->moveTick >= FIX_STUCK_MS) {
        e->holdUntil = (now + FIX_HOLD_MS) ? now + FIX_HOLD_MS : 1;
        e->moveTick = now ? now : 1;
        e->ax = pos[0];
        e->az = pos[1];
        InterlockedIncrement(&g_fixStuckReleases);
        return true;
    }
    return false;
}

// v22 lateral rank. Reads the unit's soldier array and positions (game
// memory, outside the lock), then the members' stored apexes (table, under the
// shared lock). Returns false when the unit is unreadable.
static bool FixLateral(uint32_t self, uint32_t unit, const float V[2], const float P[2], const float selfPos[2],
                       DWORD now, float u[2], float* lamOut, float* qOut, int* nOut) {
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0)
        return false;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return false;
    float px[MAX_SOLDIERS], pz[MAX_SOLDIERS];
    bool okp[MAX_SOLDIERS];
    for (uint32_t i = 0; i < n; i++) {
        float xyz[3];
        okp[i] = ptrs[i] != self && PtrPlausible(ptrs[i]) && MemcpyTry(xyz, ptrs[i] + 0x48, 12) &&
                 _finite(xyz[0]) && _finite(xyz[2]);
        px[i] = okp[i] ? xyz[0] : 0.0f;
        pz[i] = okp[i] ? xyz[2] : 0.0f;
    }
    // members; slot 0 is the soldier himself
    float mx[MAX_SOLDIERS + 1], mz[MAX_SOLDIERS + 1], ax[MAX_SOLDIERS + 1], az[MAX_SOLDIERS + 1];
    int m = 1;
    mx[0] = selfPos[0]; mz[0] = selfPos[1]; ax[0] = P[0]; az[0] = P[1];
    AcquireSRWLockShared(&g_fixLock);
    for (uint32_t i = 0; i < n; i++) {
        if (!okp[i]) continue;
        if (Dist2(px[i], pz[i], V[0], V[1]) > FIX_GROUP_ZONE_M) continue;
        uint32_t h = FixHash(ptrs[i]) & (FIX_SOLDIER_TABLE - 1);
        for (int k = 0; k < 64; k++) {
            FixSoldier& e = g_fixSoldiers[(h + k) & (FIX_SOLDIER_TABLE - 1)];
            if ((uint32_t)e.soldier == ptrs[i]) {
                DWORD t = (DWORD)e.apexTick;
                if (t && now - t <= FIX_GROUP_MS && Dist2(e.vx, e.vz, V[0], V[1]) <= FIX_GROUP_V_M &&
                    _finite(e.apx) && _finite(e.apz)) {
                    mx[m] = px[i]; mz[m] = pz[i]; ax[m] = e.apx; az[m] = e.apz; m++;
                }
                break;
            }
            if (e.soldier == 0) break;
        }
    }
    ReleaseSRWLockShared(&g_fixLock);
    float dx = 0.0f, dz = 0.0f;
    for (int j = 0; j < m; j++) {
        float ex = ax[j] - mx[j], ez = az[j] - mz[j];
        float l = sqrtf(ex * ex + ez * ez);
        if (l > 1e-3f) { dx += ex / l; dz += ez / l; }
    }
    float dl = sqrtf(dx * dx + dz * dz);
    if (dl < 1e-3f) return false;
    dx /= dl; dz /= dl;
    u[0] = -dz; u[1] = dx;
    if ((P[0] - V[0]) * u[0] + (P[1] - V[1]) * u[1] < 0.0f) { u[0] = -u[0]; u[1] = -u[1]; }
    float lam0 = (mx[0] - V[0]) * u[0] + (mz[0] - V[1]) * u[1];
    int below = 0;
    for (int j = 1; j < m; j++)
        if ((mx[j] - V[0]) * u[0] + (mz[j] - V[1]) * u[1] < lam0) below++;
    *lamOut = lam0;
    *qOut = m > 1 ? (float)below / (float)(m - 1) : 0.0f;
    *nOut = m;
    return true;
}

// v23: positions of the unit's other men within `range` of V (game memory,
// no lock). Returns the count written to px / pz.
static int FixNearbyMen(uint32_t self, uint32_t unit, const float V[2], float range, float* px, float* pz, int cap) {
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0)
        return -1;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return -1;
    int m = 0;
    for (uint32_t i = 0; i < n && m < cap; i++) {
        float xyz[3];
        if (ptrs[i] == self || !PtrPlausible(ptrs[i]) || !MemcpyTry(xyz, ptrs[i] + 0x48, 12)) continue;
        if (!_finite(xyz[0]) || !_finite(xyz[2])) continue;
        if (Dist2(xyz[0], xyz[2], V[0], V[1]) > range) continue;
        px[m] = xyz[0]; pz[m] = xyz[2]; m++;
    }
    return m;
}

static inline float WrapPi(float a) {
    while (a > FIX_PI) a -= 2.0f * FIX_PI;
    while (a < -FIX_PI) a += 2.0f * FIX_PI;
    return a;
}

// v31 F: tip lanes. Returns an FS_ status when it handled the call, -1 to
// leave the call to the C rules. laneOK: the apex -> other lane points the
// right way (not crossed); L its length.
static volatile LONG g_fixTipNew = 0, g_fixTipCont = 0, g_fixTipFar = 0, g_fixTipNear = 0, g_fixTipPast = 0,
                     g_fixTipNoQueue = 0, g_fixTipShort = 0, g_fixTipStuck = 0;
static int FixTipLanes(uint32_t soldier, FixRecord& r, FixSoldier* ent, int side, const float apex[2], float L,
                       bool laneOK, DWORD now, float* wp) {
    const float* V = side == 1 ? r.a : r.b;
    const float* mvec = side == 1 ? r.ma : r.mb;
    bool cont = ent->tipTick && now - ent->tipTick <= TIP_KEEP_MS &&
                Dist2(ent->tipVx, ent->tipVz, V[0], V[1]) <= TIP_MERGE_M;
    if (!cont) {
        if (!laneOK || L < TIP_MIN_LANE_M) { InterlockedIncrement(&g_fixTipShort); return -1; }
        if (Dist2(r.pos[0], r.pos[1], V[0], V[1]) > FIX_FAR_ZONE_M) return -1;
        float u[2], lam = 0.0f, q = 0.0f;
        int n = 0;
        if (!FixLateral(soldier, r.unit, V, apex, r.pos, now, u, &lam, &q, &n)) return -1;
        if (n < TIP_MIN_MEN || !_finite(q)) { InterlockedIncrement(&g_fixTipNoQueue); return -1; }
        float bx = apex[0] - V[0], bz = apex[1] - V[1];
        float rP = sqrtf(bx * bx + bz * bz);
        if (rP < 1e-3f) return -1;
        float mlen = sqrtf(mvec[0] * mvec[0] + mvec[1] * mvec[1]);
        float halfDeg = 90.0f + (mlen > 1.0f ? acosf(1.0f / mlen) * 180.0f / FIX_PI : 0.0f) - FIX_LEAD_MARGIN_DEG;
        if (halfDeg < 0.0f) halfDeg = 0.0f;
        float beta = atan2f(bz, bx);
        float sgn;
        float ax = apex[0] - r.pos[0], az = apex[1] - r.pos[1];
        if (sqrtf(ax * ax + az * az) > 0.3f) {
            float c = ax * (V[1] - r.pos[1]) - az * (V[0] - r.pos[0]);
            sgn = c > 0.0f ? 1.0f : -1.0f;
        } else {
            sgn = WrapPi(atan2f(r.pos[1] - V[1], r.pos[0] - V[0]) - beta) > 0.0f ? -1.0f : 1.0f;
        }
        float W = L < TIP_W_M ? L : TIP_W_M;
        ent->tipVx = V[0]; ent->tipVz = V[1];
        ent->tipR = rP + q * W;
        ent->tipSgn = sgn;
        ent->tipBeta = beta;
        ent->tipHalf = halfDeg * FIX_PI / 180.0f;
        ent->tipQ = q;
        InterlockedIncrement(&g_fixTipNew);
    } else {
        InterlockedIncrement(&g_fixTipCont);
    }
    ent->tipTick = now ? now : 1;
    if (FixStuckHold(ent, r.pos, now)) { InterlockedIncrement(&g_fixTipStuck); return FS_STUCK; }
    float Tx = ent->tipVx, Tz = ent->tipVz, R = ent->tipR, sgn = ent->tipSgn;
    float rx = r.pos[0] - Tx, rz = r.pos[1] - Tz;
    float d = sqrtf(rx * rx + rz * rz);
    if (d < 1e-3f || !_finite(R) || R <= 0.0f) return FS_GEOMETRY;
    float relth = WrapPi(atan2f(rz, rx) - ent->tipBeta);
    r.ti = relth * 180.0f / FIX_PI;
    r.t = ent->tipQ;
    r.tmax = R;
    r.tmin = sgn;
    r.scale = ent->tipHalf * 180.0f / FIX_PI;
    if (sgn * relth > ent->tipHalf) { InterlockedIncrement(&g_fixTipPast); return FS_PAST; }
    float f = (d - R) / 3.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    float delta = (TIP_AHEAD_NEAR_DEG + (TIP_AHEAD_FAR_DEG - TIP_AHEAD_NEAR_DEG) * f) * FIX_PI / 180.0f;
    float tang = d > R ? acosf(R / d) : 0.0f;
    float tgt = relth + sgn * (tang + delta);
    uint8_t flags = 16;
    if (tgt > ent->tipHalf) { tgt = ent->tipHalf; flags |= 1; }
    if (tgt < -ent->tipHalf) { tgt = -ent->tipHalf; flags |= 1; }
    float th = ent->tipBeta + tgt;
    float nx = Tx + R * cosf(th), nz = Tz + R * sinf(th);
    if (!_finite(nx) || !_finite(nz)) return FS_READ;
    InterlockedIncrement(d > R ? &g_fixTipFar : &g_fixTipNear);
    r.clamped = flags;
    wp[0] = nx;
    wp[2] = nz;
    r.after[0] = nx;
    r.after[1] = nz;
    return FS_ADJUSTED;
}

static int FixCompute(uint32_t soldier, uint32_t portal, float* wp, int mode, FixRecord& r,
                      FixSoldier* ent, DWORD now) {
    __try {
        r.before[0] = wp[0];
        r.before[1] = wp[1];
        r.before[2] = wp[2];
        r.after[0] = wp[0];
        r.after[1] = wp[2];
        uint32_t link = *(const uint32_t*)(uintptr_t)(portal + 0x10);
        if (link) return FS_LINK;
        r.type = *(const uint32_t*)(uintptr_t)(portal + 0x08);
        uint32_t E = *(const uint32_t*)(uintptr_t)(portal + 0x20);
        if (!PtrPlausible(E)) return FS_READ;
        r.edge = E;
        uint32_t twin = *(const uint32_t*)(uintptr_t)(E + 8);
        if (!PtrPlausible(twin)) return FS_READ;
        r.twin = twin;
        const float* ef = (const float*)(uintptr_t)E;
        const float* tf = (const float*)(uintptr_t)twin;
        r.a[0] = ef[0x0C / 4];  r.a[1] = ef[0x10 / 4];
        r.b[0] = tf[0x0C / 4];  r.b[1] = tf[0x10 / 4];
        r.e30 = ef[0x30 / 4];
        uint32_t R = *(const uint32_t*)(uintptr_t)(soldier + 0x3CC);
        if (!PtrPlausible(R)) return FS_READ;
        r.r = 0.5f * *(const float*)(uintptr_t)(R + 0x50);
        r.pos[0] = *(const float*)(uintptr_t)(soldier + 0x48);
        r.pos[1] = *(const float*)(uintptr_t)(soldier + 0x50);
        r.dest[0] = *(const float*)(uintptr_t)(soldier + 0x420);
        r.dest[1] = *(const float*)(uintptr_t)(soldier + 0x428);
        if (!_finite(r.a[0]) || !_finite(r.a[1]) || !_finite(r.b[0]) || !_finite(r.b[1]) ||
            !_finite(r.r) || !_finite(wp[0]) || !_finite(wp[2]))
            return FS_READ;
        r.len = Dist2(r.a[0], r.a[1], r.b[0], r.b[1]);
        if (!g_endLeft || !g_endRight) return FS_TYPE;   // helpers failed the install byte check
        bool geoOk = r.r > 0.0f && r.r < 25.0f;
        FixCheck(&g_fixGeoChecked, &g_fixGeoFail, geoOk, FIX_GEO_PERMILLE,
                 "geometry: r = 0.5 * [R+0x50] outside (0, 25)");
        if (!geoOk) return FS_GEOMETRY;

        // The game's own candidates.
        float gl[2] = { 0.0f, 0.0f }, gr[2] = { 0.0f, 0.0f };
        g_endLeft(portal, 0, gl, r.r, 0, 0);
        g_endRight(portal, 0, gr, r.r, 0, 0);
        r.pa[0] = gl[0]; r.pa[1] = gl[1];
        r.pb[0] = gr[0]; r.pb[1] = gr[1];
        if (!_finite(gl[0]) || !_finite(gl[1]) || !_finite(gr[0]) || !_finite(gr[1])) return FS_READ;
        // Offset vectors as the game used them: (candidate - vertex) / r.
        r.ma[0] = (gl[0] - r.a[0]) / r.r; r.ma[1] = (gl[1] - r.a[1]) / r.r;
        r.mb[0] = (gr[0] - r.b[0]) / r.r; r.mb[1] = (gr[1] - r.b[1]) / r.r;

        // Type 0 cross-check: v18's own formula (confirmed live) must give the
        // same points, else the helper call itself is wrong.
        if (r.type == 0) {
            float oa0 = ef[0x24 / 4] * r.r + r.a[0], oa1 = ef[0x28 / 4] * r.r + r.a[1];
            float ob0 = tf[0x24 / 4] * r.r + r.b[0], ob1 = tf[0x28 / 4] * r.r + r.b[1];
            bool same = Dist2(oa0, oa1, gl[0], gl[1]) <= FIX_CROSS_M && Dist2(ob0, ob1, gr[0], gr[1]) <= FIX_CROSS_M;
            if (g_fixCrossChecked < FIX_CHECK_CALLS) {
                InterlockedIncrement(&g_fixCrossChecked);
                if (!same) {
                    LONG f = InterlockedIncrement(&g_fixCrossFail);
                    if ((LONGLONG)f * 1000 > (LONGLONG)FIX_CHECK_CALLS * FIX_GEO_PERMILLE)
                        FixDisable("cross-check: the game endpoint helpers disagree with the type-0 vertex formula");
                }
            }
            if (!same) return FS_GEOMETRY;
        }

        float da = Dist2(wp[0], wp[2], gl[0], gl[1]);
        float db = Dist2(wp[0], wp[2], gr[0], gr[1]);
        int side = 0;
        if (da <= FIX_MATCH_M && da <= db) side = 1;
        else if (db <= FIX_MATCH_M) side = 2;
        FixCheck(&g_fixMatchChecked, &g_fixMatchFail, side != 0, FIX_MATCH_PERMILLE,
                 "apex match: the waypoint is neither endpoint the game helpers give for the side object");
        r.side = (uint8_t)side;
        if (!side) return FS_MISMATCH;

        // Lane: apex -> the other candidate. A zero offset vector on the other
        // end (type 0, a vertex without an obstacle: 627 rows in run 23:04)
        // would put the end on the vertex itself; pull it in by r along the
        // edge instead.
        float apex[2], other[2], vdir[2];
        const float* vo;
        if (side == 1) {
            apex[0] = gl[0]; apex[1] = gl[1]; other[0] = gr[0]; other[1] = gr[1];
            vdir[0] = r.b[0] - r.a[0]; vdir[1] = r.b[1] - r.a[1]; vo = r.b;
        } else {
            apex[0] = gr[0]; apex[1] = gr[1]; other[0] = gl[0]; other[1] = gl[1];
            vdir[0] = r.a[0] - r.b[0]; vdir[1] = r.a[1] - r.b[1]; vo = r.a;
        }
        // v22: remember this apex for the other men's group (any mode)
        if (ent) {
            const float* vv = side == 1 ? r.a : r.b;
            ent->apx = apex[0]; ent->apz = apex[1]; ent->vx = vv[0]; ent->vz = vv[1];
            ent->vTick = now ? now : 1;
            InterlockedExchange(&ent->apexTick, (LONG)(now ? now : 1));
        }
        const float* mo = side == 1 ? r.mb : r.ma;
        if (sqrtf(mo[0] * mo[0] + mo[1] * mo[1]) < FIX_M_MIN && r.len > 0.0f) {
            other[0] = vo[0] - vdir[0] / r.len * r.r;
            other[1] = vo[1] - vdir[1] / r.len * r.r;
            InterlockedIncrement(&g_fixOtherPulled);
        }
        float lx = other[0] - apex[0], lz = other[1] - apex[1];
        float L = sqrtf(lx * lx + lz * lz);
        r.lane = L;
        if (mode == FIX_V31_F && !g_fixDisabled && r.unit && ent) {   // v31 F: tip lanes
            int ts = FixTipLanes(soldier, r, ent, side, apex, L, L >= FIX_NARROW_M && lx * vdir[0] + lz * vdir[1] > 0.0f,
                                 now, wp);
            if (ts >= 0) return ts;
        }
        if (L < FIX_NARROW_M || lx * vdir[0] + lz * vdir[1] <= 0.0f) return FS_NARROW;
        float u[2] = { lx / L, lz / L };
        if (g_fixDisabled) return FS_DISABLED;
        if (mode == FIX_OFF) return FS_OFF;
        if (mode == FIX_V34_R) return FS_OFF;              // v34 R: waypoints untouched (radius lanes instead)
        if (FixStuckHold(ent, r.pos, now)) return FS_STUCK;

        float s;
        // v26 B: far lanes between the lead zone and FIX_FAR_ZONE_M
        bool farLane = false;
        if (mode == FIX_V26_B) {
            const float* Vf = side == 1 ? r.a : r.b;
            float fx = r.pos[0] - Vf[0], fz = r.pos[1] - Vf[1];
            float fd = sqrtf(fx * fx + fz * fz);
            if (fd > FIX_LEAD_ZONE_M && fd <= FIX_FAR_ZONE_M) farLane = true;
        }
        if (FixLeadMode(mode) && !farLane) {
            // v23 lead the turn
            if (!r.unit) return FS_NOUNIT;
            const float* V = side == 1 ? r.a : r.b;
            const float* mvec = side == 1 ? r.ma : r.mb;
            float rx = r.pos[0] - V[0], rz = r.pos[1] - V[1];
            float dist = sqrtf(rx * rx + rz * rz);
            if (dist > FIX_LEAD_ZONE_M || dist < 1e-3f) return FS_ZONE;
            float bx = apex[0] - V[0], bz = apex[1] - V[1];
            float rP = sqrtf(bx * bx + bz * bz);
            if (rP < 1e-3f) return FS_GEOMETRY;
            float beta = atan2f(bz, bx);
            float relth = WrapPi(atan2f(rz, rx) - beta);
            float sgn;
            if (ent && ent->leadTick && now - ent->leadTick <= FIX_LEAD_KEEP_MS &&
                Dist2(ent->leadVx, ent->leadVz, V[0], V[1]) <= 0.1f) {
                sgn = ent->leadSgn;
            } else {
                float ax = apex[0] - r.pos[0], az = apex[1] - r.pos[1];
                if (sqrtf(ax * ax + az * az) > 0.3f) {
                    float c = ax * (V[1] - r.pos[1]) - az * (V[0] - r.pos[0]);
                    sgn = c > 0.0f ? 1.0f : -1.0f;
                } else {
                    sgn = relth > 0.0f ? -1.0f : 1.0f;
                }
            }
            if (ent) {
                ent->leadVx = V[0]; ent->leadVz = V[1]; ent->leadSgn = sgn;
                ent->leadTick = now ? now : 1;
            }
            float mlen = sqrtf(mvec[0] * mvec[0] + mvec[1] * mvec[1]);
            float halfDeg = 90.0f + (mlen > 1.0f ? acosf(1.0f / mlen) * 180.0f / FIX_PI : 0.0f) - FIX_LEAD_MARGIN_DEG;
            if (halfDeg < 0.0f) halfDeg = 0.0f;
            float half = halfDeg * FIX_PI / 180.0f;
            r.ti = relth * 180.0f / FIX_PI;
            r.tmin = sgn;
            r.scale = halfDeg;
            if (sgn * relth > half) return FS_PAST;
            float tgt = relth + sgn * FIX_LEAD_DEG * FIX_PI / 180.0f;
            uint8_t flags = 0;
            if (tgt > half) { tgt = half; flags |= 1; }
            if (tgt < -half) { tgt = -half; flags |= 1; }
            float th = beta + tgt;
            float rspan = L < FIX_LEAD_RMAX_M ? L : FIX_LEAD_RMAX_M;
            if (L < FIX_LEAD_RMAX_M) flags |= 4;
            float rho = dist < rP ? rP : (dist > rP + rspan ? rP + rspan : dist);
            float px[MAX_SOLDIERS], pz[MAX_SOLDIERS];
            int nm = FixNearbyMen(soldier, r.unit, V, rP + rspan + 2.0f, px, pz, MAX_SOLDIERS);
            if (nm < 0) return FS_UNITREAD;
            static const float kSteps[5] = { 0.0f, 1.0f, -1.0f, 2.0f, -2.0f };
            float useR = -1.0f, firstR = -1.0f;
            for (int i = 0; i < 5; i++) {
                float rr = rho + kSteps[i] * FIX_LEAD_STEP_M;
                if (rr < rP - 1e-4f || rr > rP + rspan + 1e-4f) continue;
                if (firstR < 0.0f) firstR = rr;
                float wx = V[0] + rr * cosf(th), wz = V[1] + rr * sinf(th);
                bool occupied = false;
                for (int j = 0; j < nm; j++)
                    if (Dist2(px[j], pz[j], wx, wz) < FIX_LEAD_OCC_M) { occupied = true; break; }
                if (!occupied) { useR = rr; break; }
            }
            if (useR < 0.0f) { useR = firstR < 0.0f ? rho : firstR; flags |= 2; }
            InterlockedIncrement(&g_fixLeadCalls);
            if (flags & 1) InterlockedIncrement(&g_fixLeadSectorClamp);
            if (flags & 2) InterlockedIncrement(&g_fixLeadNoFree);
            if (flags & 4) InterlockedIncrement(&g_fixLeadRadiusCut);
            if (fabsf(useR - rho) > 1e-3f) InterlockedIncrement(&g_fixLeadMoved);
            float nx = V[0] + useR * cosf(th);
            float nz = V[1] + useR * sinf(th);
            if (!_finite(nx) || !_finite(nz)) return FS_READ;
            r.t = (tgt - relth) * 180.0f / FIX_PI;
            r.tmax = useR;
            r.clamped = flags;
            wp[0] = nx;
            wp[2] = nz;
            r.after[0] = nx;
            r.after[1] = nz;
            return FS_ADJUSTED;
        } else {
            s = (r.pos[0] - apex[0]) * u[0] + (r.pos[1] - apex[1]) * u[1];
        }
        float sc = s;
        if (sc < 0.0f) sc = 0.0f;
        if (sc > L) sc = L;
        if (farLane && sc > FIX_FAR_LANE_M) sc = FIX_FAR_LANE_M;
        r.clamped = fabsf(sc - s) > 1e-4f ? 1 : 0;   // float rounding at the ends is not a clamp
        // v21 turn limit: angle(apex + u*s - pos, apex - pos) <= FIX_TURN_DEG.
        {
            float ax = apex[0] - r.pos[0], az = apex[1] - r.pos[1];
            float a2 = ax * ax + az * az;
            float cr = fabsf(ax * u[1] - az * u[0]);
            float dt = ax * u[0] + az * u[1];
            float den = cr - FIX_TAN_TURN * dt;
            if (den > 1e-6f) {
                float smax = FIX_TAN_TURN * a2 / den;
                if (sc > smax) {
                    sc = smax > 0.0f ? smax : 0.0f;
                    r.clamped |= 2;
                    InterlockedIncrement(&g_fixTurnCapped);
                }
            }
        }
        r.t = sc;
        if (farLane) { r.clamped |= 8; InterlockedIncrement(&g_fixFarLane); }
        float nx = apex[0] + u[0] * sc;
        float nz = apex[1] + u[1] * sc;
        if (!_finite(nx) || !_finite(nz)) return FS_READ;
        wp[0] = nx;
        wp[2] = nz;
        r.after[0] = nx;
        r.after[1] = nz;
        return FS_ADJUSTED;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedIncrement(&g_fixFaults);
        return FS_READ;
    }
}

// Install: byte-check the two helpers (their first 9 bytes, docs/listings/
// phase12c_endpoints_x86.txt) before the fix may use them.
static void FixEndpointsInit(uint8_t* base) {
    static const uint8_t left[]  = { 0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x08, 0x85, 0xC0, 0x0F, 0x84, 0x80 };
    static const uint8_t right[] = { 0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x08, 0x85, 0xC0, 0x0F, 0x84, 0x8E };
    bool okL = memcmp(base + 0x393000, left, sizeof(left)) == 0;
    bool okR = memcmp(base + 0x3A0080, right, sizeof(right)) == 0;
    if (okL && okR) {
        g_endLeft = (EndpointFn)(base + 0x393000);
        g_endRight = (EndpointFn)(base + 0x3A0080);
    } else {
        FixDisable("install: the endpoint helpers FUN_10393000 / FUN_103a0080 differ from the analysed bytes");
    }
    Log("  Endpoints:   FUN_10393000 %s, FUN_103a0080 %s (the fix computes both candidates with them%s)",
        okL ? "PASS" : "FAIL", okR ? "PASS" : "FAIL", okL && okR ? "" : "; FIX UNAVAILABLE");
}

// First FIXRAW_SIZE distinct apex side objects: raw bytes (window only).
static void FixRawDump(uint32_t portal, uint32_t edge, uint32_t twin, DWORD elapsed) {
    if (g_fixRawIdx >= FIXRAW_SIZE) return;
    AcquireSRWLockExclusive(&g_fixLock);
    bool seen = false;
    LONG n = g_fixRawIdx;
    for (LONG i = 0; i < n; i++) if (g_fixRaw[i].portal == portal) { seen = true; break; }
    LONG idx = -1;
    if (!seen && n < FIXRAW_SIZE) {
        idx = n;
        g_fixRaw[idx].portal = portal;   // claims the slot for the dedupe scan
        InterlockedIncrement(&g_fixRawIdx);
    }
    ReleaseSRWLockExclusive(&g_fixLock);
    if (idx < 0) return;
    FixRawRecord& w = g_fixRaw[idx];
    w.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    w.timestamp = elapsed;
    w.edge = edge;
    w.twin = twin;
    w.plen = MemcpyTry(w.pbytes, portal, sizeof(w.pbytes)) ? sizeof(w.pbytes) : 0;
    w.elen = PtrPlausible(edge) && MemcpyTry(w.ebytes, edge, sizeof(w.ebytes)) ? sizeof(w.ebytes) : 0;
    w.tlen = PtrPlausible(twin) && MemcpyTry(w.tbytes, twin, sizeof(w.tbytes)) ? sizeof(w.tbytes) : 0;
    InterlockedExchange(&w.valid, 1);
}

static void SoldierSampleUnit(uint32_t unit, uint32_t ctrl, DWORD now, DWORD elapsed);   // above

// v27 D: gather at the queue (see the header). Goal calls only.
enum { GS_ADJUSTED = 0, GS_NO_ORDER, GS_READ, GS_NO_QUEUE, GS_SLOT_AHEAD, GS_PASSED, GS_FAR, GS_STUCK, GS_COUNT };
static const char* const kGatherName[GS_COUNT] = { "adjusted", "no_order", "read_fail", "no_queue", "slot_ahead",
                                                    "passed", "far", "stuck_release" };
static volatile LONG g_gather[GS_COUNT] = {};

static int FixGather(uint32_t soldier, uint32_t unit, float* wp, FixSoldier* ent, DWORD now) {
    DWORD ot = UnitOrderTick(unit);
    if (!ot) return GS_NO_ORDER;
    float xyz[3];
    if (!MemcpyTry(xyz, soldier + 0x48, 12) || !_finite(xyz[0]) || !_finite(xyz[2])) return GS_READ;
    float slot[2] = { wp[0], wp[2] };
    if (!_finite(slot[0]) || !_finite(slot[1])) return GS_READ;
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0)
        return GS_READ;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return GS_READ;
    // queue corners: vertices of the members' fresh apex calls (up to 8)
    const int KC = 8;
    float cvx[KC], cvz[KC], cax[KC], caz[KC];
    int cn[KC], nc = 0;
    AcquireSRWLockShared(&g_fixLock);
    for (uint32_t i = 0; i < n; i++) {
        if (ptrs[i] == soldier || !PtrPlausible(ptrs[i])) continue;
        uint32_t h = FixHash(ptrs[i]) & (FIX_SOLDIER_TABLE - 1);
        for (int k = 0; k < 64; k++) {
            FixSoldier& e = g_fixSoldiers[(h + k) & (FIX_SOLDIER_TABLE - 1)];
            if ((uint32_t)e.soldier == ptrs[i]) {
                DWORD t = (DWORD)e.apexTick;
                if (t && now - t <= FIX_GROUP_MS && _finite(e.vx) && _finite(e.vz) && _finite(e.apx) && _finite(e.apz)) {
                    int c = 0;
                    while (c < nc && Dist2(cvx[c], cvz[c], e.vx, e.vz) > FIX_GROUP_V_M) c++;
                    if (c == nc && nc < KC) { cvx[c] = e.vx; cvz[c] = e.vz; cax[c] = 0; caz[c] = 0; cn[c] = 0; nc++; }
                    if (c < nc) { cax[c] += e.apx; caz[c] += e.apz; cn[c]++; }
                }
                break;
            }
            if (e.soldier == 0) break;
        }
    }
    ReleaseSRWLockShared(&g_fixLock);
    int best = -1;
    for (int c = 0; c < nc; c++) if (best < 0 || cn[c] > cn[best]) best = c;
    if (best < 0 || cn[best] < GATHER_MIN_MEN) return GS_NO_QUEUE;
    float V[2] = { cvx[best], cvz[best] };
    float Pq[2] = { cax[best] / cn[best], caz[best] / cn[best] };
    float pos[2] = { xyz[0], xyz[2] };
    float dP = Dist2(pos[0], pos[1], V[0], V[1]);
    if (dP > GATHER_ZONE_M) return GS_FAR;
    if (Dist2(slot[0], slot[1], V[0], V[1]) <= dP + GATHER_BEHIND_M) return GS_SLOT_AHEAD;
    if (ent->vTick && (LONG)(ent->vTick - ot) >= 0)   // v28: any apex call since the order
        return GS_PASSED;
    if (FixStuckHold(ent, pos, now)) return GS_STUCK;
    float dx = Pq[0] - pos[0], dz = Pq[1] - pos[1];
    float L = sqrtf(dx * dx + dz * dz);
    if (L < 0.3f) return GS_SLOT_AHEAD;
    float step = L < GATHER_STEP_M ? L : GATHER_STEP_M;
    float nx = pos[0] + dx / L * step, nz = pos[1] + dz / L * step;
    if (!_finite(nx) || !_finite(nz)) return GS_READ;
    wp[0] = nx;
    wp[2] = nz;
    return GS_ADJUSTED;
}

static void WaypointHit(const uint8_t* ctx) {
    InterlockedIncrement(&g_fixCalls);
    // per-hook totals (no rate limiter on this hot path; the counters only)
    InterlockedIncrement((volatile LONG*)&g_rate[H_WAYPOINT].totalHits);
    if (g_state == 2) InterlockedIncrement((volatile LONG*)&g_rate[H_WAYPOINT].windowHits);
    uint32_t eax = CtxDword(ctx, CTX_OFF_EAX);
    uint32_t soldier = CtxDword(ctx, CTX_OFF_EBX);
    uint32_t portal = CtxDword(ctx, WPF_OFF_APEX);
    float* wp = (float*)(ctx + WPF_OFF_WPBUF);

    // Frame check on every call: FUN_10394e80 returns its first argument,
    // the caller's buffer at hook-point ESP + 0x44.
    InterlockedIncrement(&g_fixFrameChecked);
    if (eax != (uint32_t)(uintptr_t)wp) {
        InterlockedIncrement(&g_fixFrameFail);
        FixDisable("frame: EAX != hook-point ESP + 0x44 (the waypoint buffer)");
        return;
    }
    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    bool inWindow = WindowOpen(now, &elapsed);
    DWORD gnow = GameNow();   // v36: fix windows on game time (rows, sampling and the window on the wall clock)

    if (portal == 0) {
        InterlockedIncrement(&g_fixGoal);
        // v22: heading for the slot, no corner any more
        if (PtrPlausible(soldier)) {
            uint32_t gu = 0, gb = 0;
            FixSoldier* ge = FixLookup(soldier, &gu, &gb);
            if (ge) InterlockedExchange(&ge->apexTick, 0);
            if (ge) InterlockedExchange(&ge->wpTick, (LONG)(gnow ? gnow : 1));   // v33 (game time since v36)
            // v27 D: gather at the queue
            if (ge && gu && g_fixMode == FIX_V27_D && !g_fixDisabled) {
                int gs = FixGather(soldier, gu, wp, ge, gnow);
                InterlockedIncrement(&g_gather[gs]);
            }
        }
        // v18: the soldier's unit is sampled from here as well (rate limited
        // per unit), so a unit is traced even when FUN_101a9750 is silent.
        if (inWindow && PtrPlausible(soldier)) {
            uint32_t unit = 0, batch = 0;
            if (FixLookup(soldier, &unit, &batch) && unit) {
                InterlockedIncrement(&g_fixSamples);
                SoldierSampleUnit(unit, 0, now, elapsed);
            }
        }
        return;
    }
    InterlockedIncrement(&g_fixApex);

    FixRecord r;
    memset(&r, 0, sizeof(r));
    r.soldier = soldier;
    r.portal = portal;
    int mode = (int)g_fixMode;
    r.mode = (uint8_t)mode;
    FixSoldier* ent = PtrPlausible(soldier) ? FixLookup(soldier, &r.unit, &r.batch) : nullptr;
    if (ent) InterlockedExchange(&ent->wpTick, (LONG)(gnow ? gnow : 1));   // v33 (game time since v36)
    int status;
    if (!PtrPlausible(portal) || !PtrPlausible(soldier)) {
        FixCheck(&g_fixGeoChecked, &g_fixGeoFail, false, FIX_GEO_PERMILLE,
                 "frame: [ESP+0x14] or EBX is not a pointer on an apex call");
        status = FS_READ;
    } else {
        status = FixCompute(soldier, portal, wp, mode, r, ent, gnow);
    }
    r.status = (uint8_t)status;
    InterlockedIncrement(&g_fixStatus[status]);

    if (!g_fixRecs || !inWindow) return;
    if (r.edge) FixRawDump(portal, r.edge, r.twin, elapsed);
    if (r.unit) {
        InterlockedIncrement(&g_fixSamples);
        SoldierSampleUnit(r.unit, 0, now, elapsed);
    }
    // Row, at most 10 Hz per soldier.
    if (ent) {
        LONG last = ent->lastRow;
        if (last && now - (DWORD)last < FIX_ROW_MS) return;
        if (InterlockedCompareExchange(&ent->lastRow, (LONG)(now ? now : 1), last) != last) return;
    }
    LONG idx = InterlockedIncrement(&g_fixRecIdx) - 1;
    if (idx >= FIXREC_SIZE) { InterlockedExchange(&g_fixRecOverflow, 1); return; }
    FixRecord& w = g_fixRecs[idx];
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.valid = 0;
    memcpy((void*)&w, &r, sizeof(r));
    InterlockedExchange(&w.valid, 1);
}

// Tones (v19): Beep() was not audible for the user, so the patterns are
// in-memory 16-bit mono WAVs played with PlaySound(SND_MEMORY | SND_ASYNC)
// on the normal audio device. Built once at install; a new tone cuts the
// previous one off.
// Patterns: 0 one low tone (fix off); 1 / 2 that many high tones (fix on in
// mode 1 / 2); 11 / 12 that many high tones, then a low one (lane mode
// selected while the fix is off); 3 ALARM, three long low tones (the fix
// disabled itself, or a key was refused because it is disabled).
// v42: 5 recording ON, three rising notes (C5 E5 G5); 6 recording OFF, the same falling.
// The WAV synthesis and playback are shared (proxy/keys.h: ToneSeg, Tone_Make, Tone_Play).
static uint8_t* g_tones[13] = {};

static uint8_t* MakeTone(const ToneSeg* seg, int n) { return Tone_Make(seg, n); }

static void FixTonesInit() {
    static const ToneSeg low[] = { { 330, 300 } };
    static const ToneSeg high1[] = { { 880, 160 } };
    static const ToneSeg high2[] = { { 880, 160 }, { 0, 100 }, { 880, 160 } };
    static const ToneSeg high3[] = { { 880, 160 }, { 0, 100 }, { 880, 160 }, { 0, 100 }, { 880, 160 } };
    static const ToneSeg high1low[] = { { 880, 160 }, { 0, 100 }, { 330, 300 } };
    static const ToneSeg high2low[] = { { 880, 160 }, { 0, 100 }, { 880, 160 }, { 0, 100 }, { 330, 300 } };
    static const ToneSeg alarm[] = { { 200, 450 }, { 0, 150 }, { 200, 450 }, { 0, 150 }, { 200, 450 } };
    static const ToneSeg recOn[] = { { 523, 90 }, { 0, 30 }, { 659, 90 }, { 0, 30 }, { 784, 160 } };
    static const ToneSeg recOff[] = { { 784, 90 }, { 0, 30 }, { 659, 90 }, { 0, 30 }, { 523, 160 } };
    g_tones[5] = MakeTone(recOn, 5);
    g_tones[6] = MakeTone(recOff, 5);
    g_tones[0] = MakeTone(low, 1);
    g_tones[1] = MakeTone(high1, 1);
    g_tones[2] = MakeTone(high2, 3);
    g_tones[3] = MakeTone(alarm, 5);
    g_tones[4] = MakeTone(high3, 5);
    g_tones[11] = MakeTone(high1low, 3);
    g_tones[12] = MakeTone(high2low, 5);
}

static void FixBeep(int pattern) {
    if (pattern < 0 || pattern > 12 || !g_tones[pattern]) return;
    if (!Tone_Play(g_tones[pattern]))
        Log("WARNING: PlaySound failed for tone pattern %d", pattern);
}

// Dump thread: F6 (on / off), F7 (lane mode) and the one-time "disabled"
// report. The effective mode g_fixMode is only written here and by
// FixDisable.
static void FixKeyF6(DWORD elapsed) {
    if (g_fixDisabled) {
        Log("I refused: the corner fix is DISABLED for this session (%s)", g_fixDisabledWhy);
        FixBeep(3);
        return;
    }
    // v41: off <-> G (any other state, e.g. a harness-selected variant, goes to off)
    LONG var = g_fixMode == FIX_V38_G ? 0 : 3;
    InterlockedExchange(&g_fixVariant, var);
    LONG on = var != 0 ? 1 : 0;
    InterlockedExchange(&g_fixOn, on);
    LONG eff = on ? FIX_V38_G : FIX_OFF;
    InterlockedExchange(&g_fixMode, eff);
    AddControlEvent(HOOK_FIXMODE, (uint16_t)eff, (uint32_t)var, 0, elapsed);
    Log("I: path fix %s (mode %ld: %s)", on ? "ON, variant G" : "OFF", (long)eff, kFixModeText[eff]);
    FixBeep(on ? 1 : 0);
}

// Offline harness only (the game never calls it): select variant 0 off, 1 C, 2 RC or 3 G
// directly, as the v38..v40 I cycle did, so the parts G is built from can be checked alone.
extern "C" __declspec(dllexport) void __stdcall PathfixTestVariant(LONG var) {
    if (var < 0 || var > 3 || g_fixDisabled) return;
    InterlockedExchange(&g_fixVariant, var);
    InterlockedExchange(&g_fixOn, var != 0 ? 1 : 0);
    LONG eff = var == 0 ? FIX_OFF : var == 1 ? FIX_V26_C : var == 2 ? FIX_V36_RC : FIX_V38_G;
    InterlockedExchange(&g_fixMode, eff);
    Log("TEST variant %s: mode %ld: %s", kFixVariantName[var], (long)eff, kFixModeText[eff]);
}

static void FixKeyF7(DWORD elapsed) {
    if (g_fixDisabled) {
        Log("Lane key refused: the corner fix is DISABLED for this session (%s)", g_fixDisabledWhy);
        FixBeep(3);
        return;
    }
    LONG lane = g_fixLane == FIX_SLOT_LANE ? FIX_OWN_POS : FIX_SLOT_LANE;
    InterlockedExchange(&g_fixLane, lane);
    if (g_fixOn && g_fixVariant == 1) {
        InterlockedExchange(&g_fixMode, lane);
        AddControlEvent(HOOK_FIXMODE, (uint16_t)lane, 0, 0, elapsed);
        Log("Lane: lane mode %ld selected, fix ON, mode %ld: %s", (long)lane, (long)lane, kFixModeText[lane]);
        FixBeep((int)lane);
    } else {
        Log("Lane: lane mode %ld selected (%s); the fix is OFF, I switches it on",
            (long)lane, kFixModeText[lane]);
        FixBeep(10 + (int)lane);
    }
}

static void FixReportDisabled(DWORD elapsed) {
    if (!g_fixDisabled || g_fixDisabledLogged) return;
    InterlockedExchange(&g_fixDisabledLogged, 1);
    AddControlEvent(HOOK_FIXMODE, 0, 1, 0, elapsed);
    Log("FIX DISABLED: %s", g_fixDisabledWhy);
    Log("  checks so far: frame %ld/%ld failed, geometry %ld/%ld, apex match %ld/%ld. Mode set to 0; "
        "capture continues. ALARM sounded (three long low tones).", (long)g_fixFrameFail,
        (long)g_fixFrameChecked, (long)g_fixGeoFail, (long)g_fixGeoChecked, (long)g_fixMatchFail,
        (long)g_fixMatchChecked);
    FixBeep(3);
}

// ---------------------------------------------------------------------------
// v24 lookahead steering (see the header). Runs on whatever thread integrates
// the soldier; reads game memory with SEH guards and holds no lock while doing
// so (FixLookup takes the shared lock only for the table).
// ---------------------------------------------------------------------------
static const float STEER_H_M = 2.5f;          // look-ahead horizon
static const float STEER_BIAS = 0.3f;         // outside-of-the-turn preference
static const float STEER_ZONE_M = 12.0f;      // only within this distance of the corner vertex
static const float STEER_ZONE_FAR_M = 30.0f;  // v26 variants B and C
static const DWORD STEER_APEX_MS = GT_MS(1500);      // waypoint is an apex: apex call this recent
static const float STEER_R_DEFAULT = 0.86f;
static const bool STEER_AVOID = false;        // v25: no avoidance between men of one unit (SC2: same direction ignored)   // 2 * 0.43 (sim calibration) when e+0x58 is implausible
static const int STEER_NA = 11;
static const float kSteerCos[STEER_NA] = { 1.0f, 0.96592583f, 0.96592583f, 0.86602540f, 0.86602540f,
    0.70710678f, 0.70710678f, 0.5f, 0.5f, 0.25881905f, 0.25881905f };
static const float kSteerSin[STEER_NA] = { 0.0f, 0.25881905f, -0.25881905f, 0.5f, -0.5f,
    0.70710678f, -0.70710678f, 0.86602540f, -0.86602540f, 0.96592583f, -0.96592583f };
static const char* const kSteerName[STEER_NA] = { "0", "p15", "m15", "p30", "m30", "p45", "m45", "p60", "m60",
    "p75", "m75" };
static volatile LONG g_stHits = 0, g_stTracked = 0, g_stTurned = 0, g_stNoApex = 0, g_stFar = 0,
    g_stNoUnit = 0, g_stZeroD = 0, g_stRadiusDefault = 0, g_stNeighSum = 0, g_stFaults = 0, g_stBlocked0 = 0,
    g_stNoDamp = 0;
static volatile LONG g_stAngle[STEER_NA] = {};

// WriteFloatTry / WriteByteTry: proxy/gamemem.h (range-checked, SEH-guarded).

// ---------------------------------------------------------------------------
// v33 L: look-ahead avoidance (see the header). Runs on whatever thread
// integrates the soldier. Game memory is read with SEH guards outside the
// snapshot locks; the locks only cover the snapshot arrays.
// ---------------------------------------------------------------------------
static const float  AV_H_M = 5.0f;              // look-ahead horizon
static const float  AV_STALL_MPS = 0.8f;        // slower = stalled (static obstacle)
static const float  AV_SAME_COS = 0.70710678f;  // same-direction mover: within 45 deg of my move ...
static const float  AV_SAME_FRAC = 0.5f;        // ... and at least half my speed
static const float  AV_R_DEFAULT = 0.43f;       // e+0x58 when implausible
static const DWORD  AV_WP_MS = GT_MS(1500);            // path-following: a waypoint call this recent
static const float  AV_MIN_D = 0.05f;           // m per tick
static const float  AV_TICK_S = 0.1f;           // game tick (DAT_115b2448)
static const double AV_SNAP_MS = 100.0;         // own unit snapshot rebuilt when older
static const double AV_SNAP_STALE_MS = 400.0;   // other units' snapshots older than this are skipped
static const double AV_VEL_MIN_MS = 30.0, AV_VEL_MAX_MS = 600.0;   // snapshot gap usable for velocities
static const float  AV_VMAX_MPS = 15.0f;        // faster = teleported / spawned: velocity unknown, EMA reset
static const DWORD  AV_HOLD_MS = GT_MS(500);
static const float  AV_HOLD_MIN_F = 0.5f;
static const DWORD  AV_WALL_MS = GT_MS(1000);
static const float  AV_WALL_FRAC = 0.4f;
static const DWORD  AV_WALL_CHECK_MS = GT_MS(250);
static const bool   AV_OTHER_UNITS = true;
static const DWORD  AV_ROW_MS = 200;            // at most 5 rows / s per man
static const int    AV_MAX_OB = 128;
static const int    AV_NA = 13;
static const int   kAvDeg[AV_NA] = { 0, 10, -10, 20, -20, 30, -30, 45, -45, 60, -60, 75, -75 };
static const float kAvCos[AV_NA] = { 1.0f, 0.98480775f, 0.98480775f, 0.93969262f, 0.93969262f, 0.86602540f,
    0.86602540f, 0.70710678f, 0.70710678f, 0.5f, 0.5f, 0.25881905f, 0.25881905f };
static const float kAvSin[AV_NA] = { 0.0f, 0.17364818f, -0.17364818f, 0.34202014f, -0.34202014f, 0.5f, -0.5f,
    0.70710678f, -0.70710678f, 0.86602540f, -0.86602540f, 0.96592583f, -0.96592583f };

struct AvUnit {
    volatile LONG unit;        // key, 0 empty
    volatile LONG building;
    SRWLOCK  lock;             // zero = SRWLOCK_INIT
    double   tMs;              // snapshot time (QPC ms), 0 none
    int      n;
    float    minx, minz, maxx, maxz;
    float    vmax;
    uint32_t ptr[MAX_SOLDIERS];
    float    x[MAX_SOLDIERS], z[MAX_SOLDIERS], vx[MAX_SOLDIERS], vz[MAX_SOLDIERS], r[MAX_SOLDIERS];
    uint8_t  vok[MAX_SOLDIERS];
};
static const int AV_UNIT_TABLE = 256;         // power of two
static AvUnit* g_avUnits = nullptr;

struct AvoidRecord {
    volatile LONG valid;
    uint8_t  kind;             // 0 straight best, 1 turned, 2 none free, 3 wall flag
    int8_t   side;             // chosen side
    uint8_t  hold;             // 1 side filter from the hold, 2 from the oncoming rule
    uint8_t  wall;             // 1 a wall-blocked side was excluded
    int16_t  deg;
    uint16_t nStalled, nMover, nIgnored, nOther, nBeyond;
    uint32_t seq, timestamp, threadId, soldier, unit, blocker;
    float    pos[2], din[2], dout[2];
    float    f0, fbest, H, speed;
    float    bv[2];            // blocker's measured velocity
};
static const int AVREC_SIZE = 256 * 1024;
static AvoidRecord* g_avRecs = nullptr;
static volatile LONG g_avRecIdx = 0, g_avRecOverflow = 0;

static volatile LONG g_avChecked = 0, g_avTriggered = 0, g_avTurned = 0, g_avStraight = 0, g_avNoneFree = 0,
    g_avHeld = 0, g_avHoldBroken = 0, g_avOncoming = 0, g_avWallFlags = 0, g_avWallExcluded = 0,
    g_avNotFollowing = 0, g_avSmallD = 0, g_avNearSlot = 0, g_avRebuilds = 0, g_avContended = 0,
    g_avNoTable = 0, g_avOtherObs = 0, g_avStalledObs = 0, g_avMoverObs = 0, g_avIgnoredObs = 0,
    g_avBeyondSlot = 0, g_avObFull = 0, g_avFaults = 0, g_avClear = 0, g_avNoVel = 0;
static volatile LONG g_avAngle[AV_NA] = {};

static double AvNowMs() {
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static AvUnit* AvFind(uint32_t unit, bool create) {
    if (!g_avUnits || !unit) return nullptr;
    uint32_t h = FixHash(unit) & (AV_UNIT_TABLE - 1);
    for (int i = 0; i < AV_UNIT_TABLE; i++) {
        AvUnit& u = g_avUnits[(h + i) & (AV_UNIT_TABLE - 1)];
        LONG k = u.unit;
        if ((uint32_t)k == unit) return &u;
        if (k == 0) {
            if (!create) return nullptr;
            LONG prev = InterlockedCompareExchange(&u.unit, (LONG)unit, 0);
            if (prev == 0 || (uint32_t)prev == unit) return &u;
        }
    }
    return nullptr;
}

// Reads the unit's men (game memory, no lock), then swaps them into the
// snapshot with velocities against the previous snapshot.
static void AvBuild(AvUnit* u, uint32_t unit, double now) {
    if (InterlockedCompareExchange(&u->building, 1, 0) != 0) { InterlockedIncrement(&g_avContended); return; }
    uint32_t count = 0, arr = 0;
    uint32_t raw[MAX_SOLDIERS], ptrs[MAX_SOLDIERS];
    float x[MAX_SOLDIERS], z[MAX_SOLDIERS], r[MAX_SOLDIERS], vx[MAX_SOLDIERS], vz[MAX_SOLDIERS];
    uint8_t vok[MAX_SOLDIERS];
    int n = 0;
    if (ReadDword(unit + 0x18, &count) && ReadDword(unit + 0x1C, &arr) && PtrPlausible(arr) && count) {
        uint32_t m = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
        if (MemcpyTry(raw, arr, m * 4)) {
            for (uint32_t i = 0; i < m; i++) {
                if (!PtrPlausible(raw[i])) continue;
                float b[(0x5C - 0x48) / 4];
                if (!MemcpyTry(b, raw[i] + 0x48, sizeof(b))) continue;
                float px = b[0], pz = b[(0x50 - 0x48) / 4], rad = b[(0x58 - 0x48) / 4];
                if (!_finite(px) || !_finite(pz)) continue;
                ptrs[n] = raw[i];
                x[n] = px;
                z[n] = pz;
                r[n] = (_finite(rad) && rad >= 0.15f && rad <= 1.0f) ? rad : AV_R_DEFAULT;
                n++;
            }
        }
    }
    AcquireSRWLockExclusive(&u->lock);
    double dt = u->tMs > 0.0 ? now - u->tMs : 0.0;
    bool velOk = dt >= AV_VEL_MIN_MS && dt <= AV_VEL_MAX_MS;
    float minx = 1e30f, minz = 1e30f, maxx = -1e30f, maxz = -1e30f, vmax = 0.0f;
    for (int k = 0; k < n; k++) {
        int j = -1;
        if (velOk) {
            if (k < u->n && u->ptr[k] == ptrs[k]) j = k;
            else for (int q = 0; q < u->n; q++) if (u->ptr[q] == ptrs[k]) { j = q; break; }
        }
        float nvx = 0.0f, nvz = 0.0f;
        if (j >= 0) {
            nvx = (float)((x[k] - u->x[j]) / (dt / 1000.0));
            nvz = (float)((z[k] - u->z[j]) / (dt / 1000.0));
            if (nvx * nvx + nvz * nvz > AV_VMAX_MPS * AV_VMAX_MPS) j = -1;
        }
        if (j >= 0) {
            if (u->vok[j]) { nvx = 0.5f * nvx + 0.5f * u->vx[j]; nvz = 0.5f * nvz + 0.5f * u->vz[j]; }
            vx[k] = nvx; vz[k] = nvz; vok[k] = 1;
            float sp = sqrtf(nvx * nvx + nvz * nvz);
            if (sp > vmax) vmax = sp;
        } else {
            vx[k] = 0.0f; vz[k] = 0.0f; vok[k] = 0;
        }
        if (x[k] < minx) minx = x[k];
        if (x[k] > maxx) maxx = x[k];
        if (z[k] < minz) minz = z[k];
        if (z[k] > maxz) maxz = z[k];
    }
    memcpy(u->ptr, ptrs, n * sizeof(uint32_t));
    memcpy(u->x, x, n * sizeof(float));
    memcpy(u->z, z, n * sizeof(float));
    memcpy(u->vx, vx, n * sizeof(float));
    memcpy(u->vz, vz, n * sizeof(float));
    memcpy(u->r, r, n * sizeof(float));
    memcpy(u->vok, vok, n);
    u->n = n;
    u->minx = minx; u->minz = minz; u->maxx = maxx; u->maxz = maxz; u->vmax = vmax;
    u->tMs = now;
    ReleaseSRWLockExclusive(&u->lock);
    InterlockedExchange(&u->building, 0);
    InterlockedIncrement(&g_avRebuilds);
}

struct AvOb { float px, pz, vx, vz, R; uint32_t who; uint8_t mover, other; };

struct AvScan {
    AvOb ob[AV_MAX_OB];
    int  m;
    int  nStalled, nMover, nIgnored, nOther, nBeyond;
};

// Collects the obstacles of one snapshot (shared lock held by the caller).
static void AvCollect(const AvUnit* u, bool other, uint32_t self, float px, float pz, float ux, float uz,
                      float s, float ri, float H, float dSlot, AvScan& sc) {
    float T = s > 0.1f ? H / s : 0.0f;
    for (int j = 0; j < u->n; j++) {
        if (u->ptr[j] == self) continue;
        float ax = u->x[j] - px, az = u->z[j] - pz;
        float R = ri + u->r[j];
        float vxj = u->vx[j], vzj = u->vz[j];
        float sp = sqrtf(vxj * vxj + vzj * vzj);
        float reach = H + R + sp * T;
        float d2 = ax * ax + az * az;
        if (d2 > reach * reach) continue;
        if (!u->vok[j]) { InterlockedIncrement(&g_avNoVel); continue; }
        float d = sqrtf(d2);
        if (d > dSlot && d > R) { sc.nBeyond++; continue; }     // a man I am about to join
        uint8_t mover;
        if (sp < AV_STALL_MPS) {
            mover = 0;
            sc.nStalled++;
        } else if (vxj * ux + vzj * uz >= AV_SAME_COS * sp && sp >= AV_SAME_FRAC * s) {
            sc.nIgnored++;                                     // same way: push, do not dodge
            continue;
        } else {
            mover = 1;
            sc.nMover++;
        }
        if (sc.m >= AV_MAX_OB) { InterlockedIncrement(&g_avObFull); continue; }
        AvOb& o = sc.ob[sc.m++];
        o.px = ax; o.pz = az; o.vx = vxj; o.vz = vzj; o.R = R; o.who = u->ptr[j];
        o.mover = mover; o.other = other ? 1 : 0;
        if (other) sc.nOther++;
    }
}

// Free distance along unit direction (cx, cz) at speed s, up to H; *who the
// first obstacle hit (0 none).
static float AvFree(const AvScan& sc, float cx, float cz, float s, float H, int* who) {
    float f = H;
    *who = -1;
    for (int k = 0; k < sc.m; k++) {
        const AvOb& o = sc.ob[k];
        float R2 = o.R * o.R;
        float d2 = o.px * o.px + o.pz * o.pz;
        float d;
        if (!o.mover) {
            float sp = o.px * cx + o.pz * cz;
            if (d2 < R2) { if (sp > 0.0f && f > 0.0f) { f = 0.0f; *who = k; } continue; }
            if (sp <= 0.0f) continue;
            float l = cx * o.pz - cz * o.px;
            if (l * l >= R2) continue;
            d = sp - sqrtf(R2 - l * l);
        } else {
            float wx = s * cx - o.vx, wz = s * cz - o.vz;       // closing velocity
            float a = wx * wx + wz * wz;
            if (a < 1e-6f) continue;
            float pw = o.px * wx + o.pz * wz;
            float cc = d2 - R2;
            if (cc < 0.0f) { if (pw > 0.0f && f > 0.0f) { f = 0.0f; *who = k; } continue; }
            if (pw <= 0.0f) continue;
            float disc = pw * pw - a * cc;
            if (disc < 0.0f) continue;
            float t = (pw - sqrtf(disc)) / a;
            d = s * t;
        }
        if (d < 0.0f) d = 0.0f;
        if (d < f) { f = d; *who = k; }
    }
    return f;
}

static int AvPick(const float* f, float H, float minFree, int sideFilter, int wallSide, int prefSide, bool* wallHit) {
    int best = 0;
    float bestC = (H - f[0]) * (H - f[0]);
    for (int k = 1; k < AV_NA; k++) {
        int side = kAvDeg[k] > 0 ? 1 : -1;
        if (wallSide && side == wallSide) { *wallHit = true; continue; }
        if (sideFilter && side != sideFilter) continue;
        if (f[k] < minFree) continue;
        float c = H * H + f[k] * f[k] - 2.0f * H * f[k] * kAvCos[k];
        if (side != prefSide) c += 1e-3f;
        if (c < bestC - 1e-6f) { bestC = c; best = k; }
    }
    return best;
}

static void AvRow(const AvoidRecord& r, FixSoldier* fe, DWORD now) {
    if (!g_avRecs) return;
    DWORD elapsed = 0;
    if (!WindowOpen(now, &elapsed)) return;
    LONG last = fe->avLastRow;
    if (last && now - (DWORD)last < AV_ROW_MS) return;
    if (InterlockedCompareExchange(&fe->avLastRow, (LONG)(now ? now : 1), last) != last) return;
    LONG idx = InterlockedIncrement(&g_avRecIdx) - 1;
    if (idx >= AVREC_SIZE) { InterlockedExchange(&g_avRecOverflow, 1); return; }
    AvoidRecord& w = g_avRecs[idx];
    memcpy((void*)&w, &r, sizeof(r));
    w.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    w.timestamp = elapsed;
    w.threadId = GetCurrentThreadId();
    InterlockedExchange(&w.valid, 1);
}

static void AvoidStep(uint32_t e, uint32_t unit, FixSoldier* fe, DWORD now) {
    double nowMs = AvNowMs();
    AvUnit* own = AvFind(unit, true);
    if (!own) { InterlockedIncrement(&g_avNoTable); return; }
    if (nowMs - own->tMs >= AV_SNAP_MS) AvBuild(own, unit, nowMs);

    float b[(0xE4 - 0x48) / 4];
    float slot[3];
    if (!MemcpyTry(b, e + 0x48, sizeof(b)) || !MemcpyTry(slot, e + 0x420, sizeof(slot))) {
        InterlockedIncrement(&g_avFaults);
        return;
    }
    float px = b[0], pz = b[(0x50 - 0x48) / 4], rad = b[(0x58 - 0x48) / 4];
    float Dx = b[(0xD8 - 0x48) / 4], Dz = b[(0xE0 - 0x48) / 4];
    if (!_finite(px) || !_finite(pz) || !_finite(Dx) || !_finite(Dz)) { InterlockedIncrement(&g_avFaults); return; }

    AvoidRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.soldier = e;
    rec.unit = unit;
    rec.pos[0] = px; rec.pos[1] = pz;
    rec.din[0] = Dx; rec.din[1] = Dz;

    // wall memory: last tick we turned him; did he move?
    if (fe->avTurned && fe->avPosTick && now - fe->avPosTick <= AV_WALL_CHECK_MS) {
        float ax = px - fe->avPx, az = pz - fe->avPz;
        float want = sqrtf(fe->avDx * fe->avDx + fe->avDz * fe->avDz);
        if (want > 1e-4f && sqrtf(ax * ax + az * az) < AV_WALL_FRAC * want) {
            fe->avWallSide = fe->avSide;
            fe->avWallUntil = now + AV_WALL_MS;
            if (!fe->avWallUntil) fe->avWallUntil = 1;
            InterlockedIncrement(&g_avWallFlags);
            rec.kind = 3;
            rec.side = (int8_t)fe->avSide;
            AvRow(rec, fe, GetTickCount());   // rows on the wall clock (v36)
        }
    }
    fe->avPx = px; fe->avPz = pz;
    fe->avPosTick = now ? now : 1;
    fe->avTurned = 0;

    DWORD wt = (DWORD)fe->wpTick;
    if (!wt || now - wt > AV_WP_MS) { InterlockedIncrement(&g_avNotFollowing); return; }
    float L = sqrtf(Dx * Dx + Dz * Dz);
    if (L < AV_MIN_D || L > 2.0f) { InterlockedIncrement(&g_avSmallD); return; }
    float s = L / AV_TICK_S;
    float ux = Dx / L, uz = Dz / L;
    float dSlot = AV_H_M;
    if (_finite(slot[0]) && _finite(slot[2])) dSlot = Dist2(px, pz, slot[0], slot[2]);
    float H = dSlot < AV_H_M ? dSlot : AV_H_M;
    if (H < 0.5f) { InterlockedIncrement(&g_avNearSlot); return; }
    float ri = (_finite(rad) && rad >= 0.15f && rad <= 1.0f) ? rad : AV_R_DEFAULT;
    InterlockedIncrement(&g_avChecked);

    AvScan sc;                                             // ~3.6 KB on the integrating thread's stack
    sc.m = 0; sc.nStalled = 0; sc.nMover = 0; sc.nIgnored = 0; sc.nOther = 0; sc.nBeyond = 0;
    AcquireSRWLockShared(&own->lock);
    AvCollect(own, false, e, px, pz, ux, uz, s, ri, H, dSlot, sc);
    ReleaseSRWLockShared(&own->lock);
    if (AV_OTHER_UNITS) {
        for (int i = 0; i < AV_UNIT_TABLE; i++) {
            AvUnit* u = &g_avUnits[i];
            if (u == own || u->unit == 0 || u->tMs <= 0.0 || nowMs - u->tMs > AV_SNAP_STALE_MS) continue;
            float pad = H + 2.0f + u->vmax * (s > 0.1f ? H / s : 0.0f);
            if (px + pad < u->minx || px - pad > u->maxx || pz + pad < u->minz || pz - pad > u->maxz) continue;
            AcquireSRWLockShared(&u->lock);
            AvCollect(u, true, e, px, pz, ux, uz, s, ri, H, dSlot, sc);
            ReleaseSRWLockShared(&u->lock);
        }
    }
    InterlockedExchangeAdd(&g_avStalledObs, sc.nStalled);
    InterlockedExchangeAdd(&g_avMoverObs, sc.nMover);
    InterlockedExchangeAdd(&g_avIgnoredObs, sc.nIgnored);
    InterlockedExchangeAdd(&g_avOtherObs, sc.nOther);
    InterlockedExchangeAdd(&g_avBeyondSlot, sc.nBeyond);
    rec.nStalled = (uint16_t)sc.nStalled; rec.nMover = (uint16_t)sc.nMover; rec.nIgnored = (uint16_t)sc.nIgnored;
    rec.nOther = (uint16_t)sc.nOther; rec.nBeyond = (uint16_t)sc.nBeyond;
    rec.H = H;
    rec.speed = s;
    if (sc.m == 0) { InterlockedIncrement(&g_avClear); return; }

    float f[AV_NA];
    int who[AV_NA];
    for (int k = 0; k < AV_NA; k++) {
        float cx = ux * kAvCos[k] - uz * kAvSin[k], cz = ux * kAvSin[k] + uz * kAvCos[k];
        f[k] = AvFree(sc, cx, cz, s, H, &who[k]);
    }
    rec.f0 = f[0];
    if (f[0] >= H - 1e-4f) { InterlockedIncrement(&g_avClear); return; }
    InterlockedIncrement(&g_avTriggered);
    if (who[0] >= 0) {
        rec.blocker = sc.ob[who[0]].who;
        rec.bv[0] = sc.ob[who[0]].vx;
        rec.bv[1] = sc.ob[who[0]].vz;
    }

    // side preferences: hold, oncoming rule, wall memory, away from the corner vertex on ties
    bool holdOn = fe->avSide != 0 && fe->avHoldUntil && (LONG)(fe->avHoldUntil - now) > 0;
    bool wallOn = fe->avWallSide != 0 && fe->avWallUntil && (LONG)(fe->avWallUntil - now) > 0;
    int wallSide = wallOn ? (int)fe->avWallSide : 0;
    bool oncoming = false;
    if (who[0] >= 0) {
        const AvOb& o = sc.ob[who[0]];
        float sp = sqrtf(o.vx * o.vx + o.vz * o.vz);
        oncoming = o.mover && o.vx * ux + o.vz * uz < -0.5f * sp;
    }
    int prefSide = 1;
    DWORD vt = fe->vTick;
    if (vt && now - vt <= AV_WP_MS && _finite(fe->vx) && _finite(fe->vz)) {
        float lat = (px - fe->vx) * -uz + (pz - fe->vz) * ux;   // + side lateral = (-uz, ux)
        prefSide = lat >= 0.0f ? 1 : -1;
    }
    float minFree = 2.0f * ri;
    int sideFilter = 0;
    if (holdOn) { sideFilter = (int)fe->avSide; rec.hold = 1; }
    else if (oncoming) { sideFilter = -1; rec.hold = 2; InterlockedIncrement(&g_avOncoming); }
    if (sideFilter && sideFilter == wallSide) sideFilter = 0;
    bool wallHit = false;
    int best = AvPick(f, H, minFree, sideFilter, wallSide, prefSide, &wallHit);
    if (sideFilter && (best == 0 || f[best] < AV_HOLD_MIN_F)) {
        if (holdOn) InterlockedIncrement(&g_avHoldBroken);
        best = AvPick(f, H, minFree, 0, wallSide, prefSide, &wallHit);
        rec.hold = 0;
    } else if (holdOn) {
        InterlockedIncrement(&g_avHeld);
    }
    if (wallHit) { InterlockedIncrement(&g_avWallExcluded); rec.wall = 1; }
    rec.deg = (int16_t)kAvDeg[best];
    rec.fbest = f[best];
    InterlockedIncrement(&g_avAngle[best]);
    if (best == 0) {
        bool anyFree = false;
        for (int k = 1; k < AV_NA; k++) if (f[k] >= minFree) anyFree = true;
        if (!anyFree && f[0] < minFree) { InterlockedIncrement(&g_avNoneFree); rec.kind = 2; }
        else { InterlockedIncrement(&g_avStraight); rec.kind = 0; }
        rec.dout[0] = Dx; rec.dout[1] = Dz;
        AvRow(rec, fe, GetTickCount());   // rows on the wall clock (v36)
        return;
    }
    float nx = (ux * kAvCos[best] - uz * kAvSin[best]) * L, nz = (ux * kAvSin[best] + uz * kAvCos[best]) * L;
    if (!WriteFloatTry(e + 0xD8, nx) || !WriteFloatTry(e + 0xE0, nz)) { InterlockedIncrement(&g_avFaults); return; }
    int side = kAvDeg[best] > 0 ? 1 : -1;
    fe->avTurned = 1;
    fe->avDx = nx; fe->avDz = nz;
    fe->avSide = side;
    fe->avHoldUntil = now + AV_HOLD_MS;
    if (!fe->avHoldUntil) fe->avHoldUntil = 1;
    InterlockedIncrement(&g_avTurned);
    rec.kind = 1;
    rec.side = (int8_t)side;
    rec.dout[0] = nx; rec.dout[1] = nz;
    AvRow(rec, fe, GetTickCount());   // rows on the wall clock (v36)
}

static void SteerHit(const uint8_t* ctx) {
    if (g_fixMode == FIX_OFF) return;
    InterlockedIncrement(&g_stHits);
    uint32_t e = CtxDword(ctx, CTX_OFF_ECX);
    if (!PtrPlausible(e)) return;
    uint32_t unit = 0, batch = 0;
    FixSoldier* fe = FixLookup(e, &unit, &batch);
    if (!fe) return;                                       // not a soldier of a listed unit
    DWORD now = GameNow();   // v36: game time (apex freshness, avoidance memory)
    if (g_fixMode == FIX_V33_L && unit && !g_fixDisabled) AvoidStep(e, unit, fe, now);   // v33 L
    DWORD t = (DWORD)fe->apexTick;
    if (!t || now - t > STEER_APEX_MS) { InterlockedIncrement(&g_stNoApex); return; }
    if (!unit) { InterlockedIncrement(&g_stNoUnit); return; }
    float Vx = fe->vx, Vz = fe->vz;
    float b[(0xE4 - 0x48) / 4];
    if (!MemcpyTry(b, e + 0x48, sizeof(b))) { InterlockedIncrement(&g_stFaults); return; }
    float px = b[0], pz = b[(0x50 - 0x48) / 4];
    float rad = b[(0x58 - 0x48) / 4];
    float Dx = b[(0xD8 - 0x48) / 4], Dz = b[(0xE0 - 0x48) / 4];
    if (!_finite(px) || !_finite(pz) || !_finite(Dx) || !_finite(Dz) || !_finite(Vx) || !_finite(Vz)) {
        InterlockedIncrement(&g_stFaults);
        return;
    }
    float ox = px - Vx, oz = pz - Vz;
    float od = sqrtf(ox * ox + oz * oz);
    LONG smode = g_fixMode;
    float szone = (smode == FIX_V26_B || smode == FIX_V26_C || smode == FIX_V27_D || smode == FIX_V31_F ||
                   smode == FIX_V33_L || smode == FIX_V34_R || smode == FIX_V36_RC || smode == FIX_V38_G)
                  ? STEER_ZONE_FAR_M : STEER_ZONE_M;
    if (od > szone) { InterlockedIncrement(&g_stFar); return; }
    // v25: this man keeps his own move when crowded (crowd damping skipped this tick)
    if (!WriteByteTry(e + 0xD6, 0)) { InterlockedIncrement(&g_stFaults); return; }
    InterlockedIncrement(&g_stNoDamp);
    if (!STEER_AVOID) return;
    float L = sqrtf(Dx * Dx + Dz * Dz);
    if (L < 1e-5f || L > 2.0f) { InterlockedIncrement(&g_stZeroD); return; }
    float R = STEER_R_DEFAULT;
    if (_finite(rad) && rad >= 0.15f && rad <= 1.0f) R = 2.0f * rad;
    else InterlockedIncrement(&g_stRadiusDefault);

    // neighbours of the same unit within reach (relative positions)
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0) {
        InterlockedIncrement(&g_stNoUnit);
        return;
    }
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) { InterlockedIncrement(&g_stFaults); return; }
    float rx[MAX_SOLDIERS], rz[MAX_SOLDIERS];
    int m = 0;
    float reach2 = (STEER_H_M + R) * (STEER_H_M + R);
    for (uint32_t i = 0; i < n; i++) {
        if (ptrs[i] == e || !PtrPlausible(ptrs[i])) continue;
        float xyz[3];
        if (!MemcpyTry(xyz, ptrs[i] + 0x48, 12) || !_finite(xyz[0]) || !_finite(xyz[2])) continue;
        float ax = xyz[0] - px, az = xyz[2] - pz;
        float d2 = ax * ax + az * az;
        if (d2 >= reach2 || d2 < 1e-8f) continue;
        rx[m] = ax; rz[m] = az; m++;
    }
    InterlockedExchangeAdd(&g_stNeighSum, m);
    float ux0 = Dx / L, uz0 = Dz / L;
    float qx = 0.0f, qz = 0.0f;                            // outside of the turn, perpendicular to D
    if (od > 1e-3f) {
        qx = ox / od; qz = oz / od;
        float dp = qx * ux0 + qz * uz0;
        qx -= dp * ux0; qz -= dp * uz0;
    }
    int best = 0;
    float bestS = -1e30f, bestFree = 0.0f;
    for (int a = 0; a < STEER_NA; a++) {
        float c = kSteerCos[a], s = kSteerSin[a];
        float cx = ux0 * c - uz0 * s, cz = ux0 * s + uz0 * c;
        float fr = STEER_H_M;
        for (int j = 0; j < m; j++) {
            float sp = rx[j] * cx + rz[j] * cz;
            if (sp <= 0.0f) continue;
            float l = fabsf(cx * rz[j] - cz * rx[j]);
            if (l >= R) continue;
            float d = sp - sqrtf(R * R - l * l);
            if (d < fr) fr = d;
        }
        if (fr < 0.0f) fr = 0.0f;
        if (a == 0 && fr < STEER_H_M) InterlockedIncrement(&g_stBlocked0);
        float score = fr * c + STEER_BIAS * (cx * qx + cz * qz);
        if (score > bestS + 1e-6f) { bestS = score; best = a; bestFree = fr; }
    }
    (void)bestFree;
    InterlockedIncrement(&g_stTracked);
    InterlockedIncrement(&g_stAngle[best]);
    if (best == 0) return;
    float c = kSteerCos[best], s = kSteerSin[best];
    float nx = (ux0 * c - uz0 * s) * L, nz = (ux0 * s + uz0 * c) * L;
    if (!WriteFloatTry(e + 0xD8, nx) || !WriteFloatTry(e + 0xE0, nz)) { InterlockedIncrement(&g_stFaults); return; }
    InterlockedIncrement(&g_stTurned);
}

// v38 G constants (handlers: GPlan / AdvanceHit before HandleHit)
static const float G_NARROW_FRAC = 0.6f;    // node narrower than this x the path's widest node = choke (stretch >= 1.67x)
static const float G_MIN_WIDE_M = 10.0f;    // the widest node must be at least this wide for the rule to apply
static const float G_FILE_M = 1.4f;         // metres per file in a row (36.6 m -> 26 files, section AO)
static const float G_ROW_M = 0.85f;         // metres per row (200 men at 36 m: 8 rows, 6.8 m deep, section AO)
static const float G_MARGIN_M = 3.0f;       // beyond the block depth: command ranks and slack
static const float G_MAX_D_M = 100.0f;      // the game's D must be plausible (11 m measured)
static const int   G_MAX_NODES = 96;
static const int   G_MAX_RUNS = 8;
static const int   G_PENDING = 32;
static const DWORD G_PLAN_AGE_MS = 100;     // wall ms between the orders hook and its advance call (same call)
static const uint32_t G_CALL_SITE_RVA = 0x1BC988;   // return address of CALL FUN_103670a0 at 0x101BC983
static uint32_t g_gCallSite = 0;            // rebased at install
static const DWORD G_HOLD_AGE_MS = GT_MS(1000);     // v39: game ms without a laying call that end a hold
static const float G_END_GAP_M = 2.0f;              // v40: the anchor stays this far before the path end (never "end reached")
static const int   G_HOLDS = 64;                    // v39: controllers with a held anchor
static const float G_PARK_PAST_M = 0.5f;            // v42: the park release lays the front this far past the path end

static void FixInstallLog() {
    Log("  Fix:         corner waypoint fix at RVA 0x3DF6C0, OFF at launch (I cycles off / C / RC / G)");
    Log("  v38 G:       mode 10 = RC + release beyond the choke. Orders hook walks the unit path from ctrl+0x34 "
        "(next +0x08, follow points +0x80/+0x88, widths +0x74/+0x78, up to %d nodes); narrow = node width < %.2f x "
        "the widest node (widest >= %.0f m); block depth = ceil(n / floor(width / %.1f m)) x %.2f m. FUN_103670a0 "
        "entry hook (RVA 0x3670A0), only the call from 0x1BC983 (return 0x%08X): if cursor + D falls in a narrow run, "
        "D becomes run exit + depth + %.0f m - cursor (past the path end = the game's own release); order time untouched",
        G_MAX_NODES, G_NARROW_FRAC, G_MIN_WIDE_M, G_FILE_M, G_ROW_M, g_gCallSite, G_MARGIN_M);
    Log("               v39: the anchor is held (the front is never laid behind the last applied anchor of the same "
        "path) until the leader's point passes it; dropped at a new order (leader index 0), a longer path, or %lu game ms "
        "without a laying call", (unsigned long)G_HOLD_AGE_MS);
    Log("               v40: the anchor is capped at path end - %.1f m (an anchor at the end gives every man a "
        "formation-move command instead of a go-to order, measured in run 183851)", G_END_GAP_M);
    Log("               v42 park release: after an anchor on this path, with the leader arrived at his slot "
        "(s+0x384 == 1, s+0x308 == 1), no narrow run ahead and at most D + block depth + %.1f m of path left, the front "
        "goes %.1f m past the path end (the game's own release) and stays there until a new order or a new path",
        G_MARGIN_M, G_PARK_PAST_M);
    Log("  v37 capture:soldier rows add spd_max e+0x110, spd_cur +0x130, spd_tgt +0x150, spd_cmd +0x1AC, spd_mul +0x1BC, "
        "ord_spd +0x438, ord_t +0x43C, ord_mode +0x440, elapsed +0x46C, moving +0xF0, direct +0x198, f384, f308; "
        "order rows add leader_idx [ctrl+0x30], ready [ctrl+0x0C], speed_mode [ctrl+0x24]");
    Log("  Variant R:   mode 8 = radius lanes at the replan request (0x103DF4D1, request at hook ESP+0x%02X, "
        "w at +0x2C): >= %d men queueing for a corner -> lane floor(q * %d), overfull (> %.1f * n / %d) -> "
        "adjacent lane; w = w0 + lane * %.1f m (max %.1f); waypoints untouched; no damping within 30 m",
        RQF_OFF_REQ - RQF_BYTES_BELOW_ENTRY, RQ_MIN_MEN, RQ_LANES, RQ_BALANCE, RQ_LANES, RQ_STEP_M, RQ_W_MAX);
    Log("               v35: corner = own fresh apex vertex or the unit's busy corner ahead (members' vertices "
        "within %lu ms, clustered %.1f m, largest fresh queue wins); passed vertices skipped; lane kept %lu ms",
        (unsigned long)RQ_UNIT_MS, RQ_MERGE_M, (unsigned long)RQ_KEEP_MS);
    Log("  v36 RC:      mode 9 = R + lead the turn (C). Fix windows in GAME ms from the engine tick "
        "([[ctrl+0x14]+8]+0x50, %lu ms per tick), tested lengths kept at %.1f game s per wall s: group %lu, "
        "stuck %lu / hold %lu / gap %lu, lead keep %lu, steer apex %lu, lane apex %lu / unit %lu / keep %lu; "
        "wall clock until the first FUN_101bc880 hit",
        (unsigned long)GT_TICK_MS, GT_PER_WALL, (unsigned long)FIX_GROUP_MS, (unsigned long)FIX_STUCK_MS,
        (unsigned long)FIX_HOLD_MS, (unsigned long)FIX_SEEN_GAP_MS, (unsigned long)FIX_LEAD_KEEP_MS,
        (unsigned long)STEER_APEX_MS, (unsigned long)RQ_APEX_MS, (unsigned long)RQ_UNIT_MS, (unsigned long)RQ_KEEP_MS);
    Log("  Variant L:   mode 7 = C + look-ahead avoidance on the integrator entry: men with a waypoint call "
        "within %lu ms; obstacles = stalled men (< %.1f m/s) and men moving another way, same-way movers ignored, "
        "men beyond the own slot ignored, other units %s; 13 directions +-75 deg, horizon min(%.0f m, slot), "
        "Moussaid cost, |D| kept, side held %lu ms, oncoming -> clockwise, wall memory %lu ms",
        (unsigned long)AV_WP_MS, AV_STALL_MPS, AV_OTHER_UNITS ? "included" : "excluded", AV_H_M,
        (unsigned long)AV_HOLD_MS, (unsigned long)AV_WALL_MS);
    Log("               unit snapshots every %.0f ms (%s); rows: pathfix_capture_v42_avoid.csv, %lu per s per man",
        AV_SNAP_MS, g_avUnits ? "table allocated" : "TABLE MISSING, avoidance inert",
        (unsigned long)(1000 / AV_ROW_MS));
    Log("  Variants:    C mode 4 = lead the turn within %.0f m + no damping within %.0f m; D mode 5 = C + gather "
        "(slot > %.1f m farther from the queue vertex than the man, within %.0f m, step %.0f m towards the queue apex, "
        "queue >= %d men, no apex call since the unit's last order)", FIX_LEAD_ZONE_M, STEER_ZONE_FAR_M,
        GATHER_BEHIND_M, GATHER_ZONE_M, GATHER_STEP_M, GATHER_MIN_MEN);
    Log("               frame from stack_offsets_v17.h: apex portal [ESP+0x%02X], buffer ESP+0x%02X "
        "(ctx +0x%02X / +0x%02X)", WPF_OFF_APEX - WPF_BYTES_BELOW_ENTRY,
        WPF_OFF_WPBUF - WPF_BYTES_BELOW_ENTRY, WPF_OFF_APEX, WPF_OFF_WPBUF);
    Log("               geometry: both candidates from the game helpers FUN_10393000 / FUN_103a0080 (every "
        "side-object type); lane apex -> other candidate; type 0 cross-checked against A + mA*r / B + mB*r");
    Log("               self-checks: frame on every call; geometry (> %d permille) and apex match "
        "(> %d permille) on the first %d type-0 apex calls; ALARM (three long low tones) when disabled",
        FIX_GEO_PERMILLE, FIX_MATCH_PERMILLE, FIX_CHECK_CALLS);
    Log("               rows: pathfix_capture_v42_fix.csv, at most %d per s per soldier, window only; "
        "%d rows max; raw dump of the first %d apex side objects (fixraw.csv)",
        (int)(1000 / FIX_ROW_MS), FIXREC_SIZE, FIXRAW_SIZE);
    Log("  Steering:    v25 on the integrator FUN_10367600 while the fix is on: corner men within %.0f m of the "
        "vertex skip the crowd damping (e+0xD6 = 0); look-ahead avoidance %s",
        STEER_ZONE_M, STEER_AVOID ? "ON" : "OFF (SC2: same-direction movers are not avoided)");
    Log("  Override:    none (v16 width presets removed); watchpoint off (no key)");
}

// ---------------------------------------------------------------------------
// v34 R: dynamic radius lanes (see the header). Runs on the thread that
// replans the soldier; game memory read with SEH guards outside the lock.
// ---------------------------------------------------------------------------

struct RadiusRecord {
    volatile LONG valid;
    uint8_t  lane, prevLane, reason;           // reason 0 assigned, 1 balanced, 2 kept
    uint8_t  pad;
    uint16_t n, cnt[RQ_LANES];
    uint32_t seq, timestamp, threadId, soldier, unit;
    float    q, w0, w, vx, vz, pos[2];
    uint8_t  key;              // v35: 0 own vertex, 1 unit's busy corner
    uint8_t  ncand;
};
static const int RQREC_SIZE = 256 * 1024;
static RadiusRecord* g_rqRecs = nullptr;
static volatile LONG g_rqRecIdx = 0, g_rqRecOverflow = 0;
static volatile LONG g_rqCalls = 0, g_rqActive = 0, g_rqFrameChecked = 0, g_rqFrameFail = 0, g_rqFrameBad = 0,
    g_rqNoUnit = 0, g_rqNoCorner = 0, g_rqNoQueue = 0, g_rqStuck = 0, g_rqBalanced = 0, g_rqRaised = 0,
    g_rqKept = 0, g_rqReadFail = 0, g_rqKeyOwn = 0, g_rqKeyUnit = 0, g_rqPassed = 0, g_rqCandSum = 0;
static volatile LONG g_rqLane[RQ_LANES] = {};

// Lanes already held by the unit's other men for corner V (table, shared lock).
struct RqCand { float V[2], A[2]; int own, count; };

// v35: the unit's busy corners ahead of the man: members' stored apex
// vertices (vTick within RQ_UNIT_MS, table under the shared lock) within
// FIX_GROUP_ZONE_M of him and, when his slot is known, not behind him;
// clustered by RQ_MERGE_M; clusters with >= RQ_MIN_MEN members, largest
// first. Returns the number written to out.
static int RqUnitCorners(uint32_t self, uint32_t unit, const float P[2], const float* slot, DWORD now,
                         RqCand* out, int maxOut) {
    uint32_t count = 0, arr = 0;
    if (maxOut <= 0) return 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0) return 0;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return 0;
    float cv[RQ_MAX_CLUSTER][2], ca[RQ_MAX_CLUSTER][2];
    DWORD ct[RQ_MAX_CLUSTER];
    int cc[RQ_MAX_CLUSTER];
    int m = 0;
    AcquireSRWLockShared(&g_fixLock);
    for (uint32_t i = 0; i < n; i++) {
        if (ptrs[i] == self || !PtrPlausible(ptrs[i])) continue;
        uint32_t h = FixHash(ptrs[i]) & (FIX_SOLDIER_TABLE - 1);
        for (int k = 0; k < 64; k++) {
            FixSoldier& e = g_fixSoldiers[(h + k) & (FIX_SOLDIER_TABLE - 1)];
            if ((uint32_t)e.soldier == ptrs[i]) {
                DWORD t = e.vTick;
                if (t && now - t <= RQ_UNIT_MS && _finite(e.vx) && _finite(e.vz) && _finite(e.apx) && _finite(e.apz) &&
                    Dist2(e.vx, e.vz, P[0], P[1]) <= FIX_GROUP_ZONE_M) {
                    bool ahead = true;
                    if (slot) {
                        float dx = e.vx - P[0], dz = e.vz - P[1];
                        ahead = (dx * (slot[0] - P[0]) + dz * (slot[1] - P[1]) >= 0.0f) ||
                                sqrtf(dx * dx + dz * dz) <= RQ_PASSED_M;
                    }
                    if (ahead) {
                        int j = 0;
                        for (; j < m; j++) if (Dist2(cv[j][0], cv[j][1], e.vx, e.vz) <= RQ_MERGE_M) break;
                        if (j == m && m < RQ_MAX_CLUSTER) {
                            cv[m][0] = e.vx; cv[m][1] = e.vz; ca[m][0] = e.apx; ca[m][1] = e.apz; ct[m] = t; cc[m] = 0;
                            m++;
                        }
                        if (j < m) {
                            cc[j]++;
                            if ((LONG)(t - ct[j]) > 0) { ct[j] = t; ca[j][0] = e.apx; ca[j][1] = e.apz; }
                        }
                    }
                }
                break;
            }
            if (e.soldier == 0) break;
        }
    }
    ReleaseSRWLockShared(&g_fixLock);
    int written = 0;
    bool used[RQ_MAX_CLUSTER] = {};
    while (written < maxOut) {
        int best = -1;
        for (int j = 0; j < m; j++)
            if (!used[j] && cc[j] >= RQ_MIN_MEN && (best < 0 || cc[j] > cc[best])) best = j;
        if (best < 0) break;
        used[best] = true;
        out[written].V[0] = cv[best][0]; out[written].V[1] = cv[best][1];
        out[written].A[0] = ca[best][0]; out[written].A[1] = ca[best][1];
        out[written].own = 0; out[written].count = cc[best];
        written++;
    }
    return written;
}

static void RqCount(uint32_t self, uint32_t unit, const float V[2], DWORD now, int* cnt) {
    for (int k = 0; k < RQ_LANES; k++) cnt[k] = 0;
    uint32_t count = 0, arr = 0;
    if (!ReadDword(unit + 0x18, &count) || !ReadDword(unit + 0x1C, &arr) || !PtrPlausible(arr) || count == 0) return;
    uint32_t n = count < (uint32_t)MAX_SOLDIERS ? count : (uint32_t)MAX_SOLDIERS;
    uint32_t ptrs[MAX_SOLDIERS];
    if (!MemcpyTry(ptrs, arr, n * 4)) return;
    AcquireSRWLockShared(&g_fixLock);
    for (uint32_t i = 0; i < n; i++) {
        if (ptrs[i] == self || !PtrPlausible(ptrs[i])) continue;
        uint32_t h = FixHash(ptrs[i]) & (FIX_SOLDIER_TABLE - 1);
        for (int k = 0; k < 64; k++) {
            FixSoldier& e = g_fixSoldiers[(h + k) & (FIX_SOLDIER_TABLE - 1)];
            if ((uint32_t)e.soldier == ptrs[i]) {
                if (e.rTick && now - e.rTick <= RQ_KEEP_MS && Dist2(e.rVx, e.rVz, V[0], V[1]) <= RQ_MERGE_M &&
                    e.rLane >= 0 && e.rLane < RQ_LANES)
                    cnt[e.rLane]++;
                break;
            }
            if (e.soldier == 0) break;
        }
    }
    ReleaseSRWLockShared(&g_fixLock);
}

static void RqRow(const RadiusRecord& r, FixSoldier* fe, DWORD now) {
    if (!g_rqRecs) return;
    DWORD elapsed = 0;
    if (!WindowOpen(now, &elapsed)) return;
    LONG last = fe->rRow;
    if (last && now - (DWORD)last < RQ_ROW_MS) return;
    if (InterlockedCompareExchange(&fe->rRow, (LONG)(now ? now : 1), last) != last) return;
    LONG idx = InterlockedIncrement(&g_rqRecIdx) - 1;
    if (idx >= RQREC_SIZE) { InterlockedExchange(&g_rqRecOverflow, 1); return; }
    RadiusRecord& w = g_rqRecs[idx];
    memcpy((void*)&w, &r, sizeof(r));
    w.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    w.timestamp = elapsed;
    w.threadId = GetCurrentThreadId();
    InterlockedExchange(&w.valid, 1);
}

static void RadiusHit(const uint8_t* ctx) {
    InterlockedIncrement(&g_rqCalls);
    if ((g_fixMode != FIX_V34_R && g_fixMode != FIX_V36_RC && g_fixMode != FIX_V38_G) || g_fixDisabled) return;
    InterlockedIncrement(&g_rqActive);
    uint32_t e = CtxDword(ctx, RQF_OFF_EBX);
    float* wq = (float*)(ctx + RQF_OFF_REQ + 0x2C);          // request radius w (game stack)
    const float* qend = (const float*)(ctx + RQF_OFF_REQ + 0x18);
    float slot[3], pos[3];
    bool read = PtrPlausible(e) && MemcpyTry(slot, e + 0x420, sizeof(slot)) && MemcpyTry(pos, e + 0x48, sizeof(pos));
    if (!read) { InterlockedIncrement(&g_rqReadFail); return; }
    float w0 = *wq;
    // frame: FUN_1035fdf0 copied the destination e+0x420.. into Q+0x18..
    bool ok = qend[0] == slot[0] && qend[2] == slot[2] && _finite(w0) && w0 > 0.3f && w0 < 6.0f;
    FixCheck(&g_rqFrameChecked, &g_rqFrameFail, ok, RQ_FRAME_PERMILLE,
             "radius frame: request end != soldier slot, or the radius is implausible");
    if (!ok) { InterlockedIncrement(&g_rqFrameBad); return; }
    uint32_t unit = 0, batch = 0;
    FixSoldier* fe = FixLookup(e, &unit, &batch);
    if (!fe || !unit) { InterlockedIncrement(&g_rqNoUnit); return; }
    DWORD now = GetTickCount();
    DWORD gnow = GameNow();   // v36: lane and apex windows on game time (rows on the wall clock)
    float P[2] = { pos[0], pos[2] };
    float SL[2] = { slot[0], slot[2] };
    bool slotOk = _finite(SL[0]) && _finite(SL[1]) && Dist2(P[0], P[1], SL[0], SL[1]) > RQ_AHEAD_MIN_M;
    // v35: candidates = own fresh apex vertex (not passed) + the unit's busy corners ahead
    RqCand cand[RQ_MAX_CAND];
    int nc = 0;
    DWORD vt = fe->vTick;
    if (vt && gnow - vt <= RQ_APEX_MS && _finite(fe->vx) && _finite(fe->vz) && _finite(fe->apx) && _finite(fe->apz)) {
        bool passed = false;
        if (slotOk) {
            float dx = fe->vx - P[0], dz = fe->vz - P[1];
            passed = dx * (SL[0] - P[0]) + dz * (SL[1] - P[1]) < 0.0f && sqrtf(dx * dx + dz * dz) > RQ_PASSED_M;
        }
        if (passed) InterlockedIncrement(&g_rqPassed);
        else {
            cand[nc].V[0] = fe->vx; cand[nc].V[1] = fe->vz; cand[nc].A[0] = fe->apx; cand[nc].A[1] = fe->apz;
            cand[nc].own = 1; cand[nc].count = 0; nc++;
        }
    }
    nc += RqUnitCorners(e, unit, P, slotOk ? SL : nullptr, gnow, cand + nc, RQ_MAX_CAND - nc);
    InterlockedExchangeAdd(&g_rqCandSum, nc);
    if (nc == 0) { fe->rTick = 0; InterlockedIncrement(&g_rqNoCorner); return; }
    if (FixStuckHold(fe, P, gnow)) { fe->rTick = 0; InterlockedIncrement(&g_rqStuck); return; }
    float V[2] = { 0.0f, 0.0f }, A[2] = { 0.0f, 0.0f }, u[2], lam = 0.0f, q = 0.0f;
    int n = 0, best = -1;
    for (int c = 0; c < nc; c++) {
        float uu[2], ll = 0.0f, qq = 0.0f;
        int nn = 0;
        if (!FixLateral(e, unit, cand[c].V, cand[c].A, P, gnow, uu, &ll, &qq, &nn) || nn < RQ_MIN_MEN || !_finite(qq))
            continue;
        if (nn > n) {
            n = nn; q = qq; lam = ll; u[0] = uu[0]; u[1] = uu[1]; best = c;
            V[0] = cand[c].V[0]; V[1] = cand[c].V[1]; A[0] = cand[c].A[0]; A[1] = cand[c].A[1];
        }
    }
    if (best < 0) { fe->rTick = 0; InterlockedIncrement(&g_rqNoQueue); return; }
    InterlockedIncrement(cand[best].own ? &g_rqKeyOwn : &g_rqKeyUnit);
    (void)lam; (void)u; (void)A;
    int cnt[RQ_LANES];
    RqCount(e, unit, V, gnow, cnt);
    int k0 = (int)(q * RQ_LANES);
    if (k0 < 0) k0 = 0;
    if (k0 >= RQ_LANES) k0 = RQ_LANES - 1;
    bool have = fe->rTick && gnow - fe->rTick <= RQ_KEEP_MS && Dist2(fe->rVx, fe->rVz, V[0], V[1]) <= RQ_MERGE_M &&
                fe->rLane >= 0 && fe->rLane < RQ_LANES;
    int prev = have ? (int)fe->rLane : -1;
    int k = have ? prev : k0;
    uint8_t reason = have ? 2 : 0;
    float capN = RQ_BALANCE * (float)n / (float)RQ_LANES;
    if ((float)(cnt[k] + 1) > capN) {                        // too many on this lane: adjacent lane with fewer
        int best = k, bc = cnt[k];
        for (int d = -1; d <= 1; d += 2) {
            int j = k + d;
            if (j < 0 || j >= RQ_LANES) continue;
            if (cnt[j] < bc) { bc = cnt[j]; best = j; }
        }
        if (best != k) { k = best; reason = 1; InterlockedIncrement(&g_rqBalanced); }
    }
    if (reason == 2) InterlockedIncrement(&g_rqKept);
    float w = w0 + (float)k * RQ_STEP_M;
    if (w > RQ_W_MAX) w = RQ_W_MAX;
    if (w < w0) w = w0;
    *wq = w;
    fe->rLane = k;
    fe->rVx = V[0]; fe->rVz = V[1];
    fe->rTick = gnow ? gnow : 1;
    InterlockedIncrement(&g_rqLane[k]);
    if (w > w0) InterlockedIncrement(&g_rqRaised);
    RadiusRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.lane = (uint8_t)k; rec.prevLane = (uint8_t)(prev < 0 ? 255 : prev); rec.reason = reason;
    rec.n = (uint16_t)n;
    for (int j = 0; j < RQ_LANES; j++) rec.cnt[j] = (uint16_t)cnt[j];
    rec.soldier = e; rec.unit = unit; rec.q = q; rec.w0 = w0; rec.w = w; rec.vx = V[0]; rec.vz = V[1];
    rec.key = cand[best].own ? 0 : 1; rec.ncand = (uint8_t)nc;
    rec.pos[0] = P[0]; rec.pos[1] = P[1];
    RqRow(rec, fe, now);
}


// ---------------------------------------------------------------------------
// v38 G: release the formation beyond the choke (see the header).
//
// GPlan runs on the orders hook (FUN_101bc880 entry, once per game second per
// unit) on the thread that will call FUN_103670a0 a few instructions later; it
// walks the unit path from the cursor node ctrl+0x34 (t at +0x38) and stores a
// per-thread plan: the cursor's arc length, the path length, the widest node,
// the block depth at that width and the narrow runs. AdvanceHit runs on the
// FUN_103670a0 entry, keeps only the call from 0x101BC983 (return address
// 0x101BC988), checks that ECX is the same cursor and rewrites the distance
// argument on the game stack when cursor + D lies in a narrow run.
// ---------------------------------------------------------------------------
struct GPending {
    volatile LONG tid;          // owner thread, 0 = free
    volatile LONG stamp;        // GetTickCount at the plan (0 = consumed / invalid)
    uint32_t ctrl, unit, node, n;
    float    t, s0, pathEnd, wmax, depth;
    int32_t  nNodes, nRuns;
    float    runs[G_MAX_RUNS][2];   // [start, end) arc lengths of the narrow runs
    uint8_t  newOrder;          // v39: leader index ctrl+0x30 was 0 (first laying call after an order)
    uint8_t  leaderParked;      // v42: the leader has arrived at his slot (s+0x384 == 1 and s+0x308 == 1)
    uint8_t  pad[2];
};
static GPending g_gp[G_PENDING] = {};

// v39: per-controller anchor hold; v42: anchor applied on this path / park release latched
struct GHold {
    volatile LONG ctrl;         // 0 = free
    DWORD    stamp;             // GameNow at the last advance call
    float    remFront;          // path end - block front laid last time (valid while active)
    float    remCursor;         // path end - cursor at the last call
    uint8_t  active;
    uint8_t  anchored;          // v42: a release anchor was applied since the last order / new path
    uint8_t  latched;           // v42: the park release fired; the front stays past the path end
    uint8_t  pad;
};
static GHold g_gh[G_HOLDS] = {};
static volatile LONG g_gAdvHeld = 0, g_gAdvHoldEnd = 0, g_gHoldReset = 0, g_gAdvPark = 0, g_gAdvParkLatched = 0;

static GHold* GHoldFor(uint32_t ctrl) {
    for (int i = 0; i < G_HOLDS; i++)
        if ((uint32_t)g_gh[i].ctrl == ctrl) return &g_gh[i];
    for (int i = 0; i < G_HOLDS; i++)
        if (InterlockedCompareExchange(&g_gh[i].ctrl, (LONG)ctrl, 0) == 0) {
            g_gh[i].stamp = 0; g_gh[i].active = 0; g_gh[i].anchored = 0; g_gh[i].latched = 0;
            return &g_gh[i];
        }
    GHold* oldest = &g_gh[0];
    for (int i = 1; i < G_HOLDS; i++)
        if (g_gh[i].stamp < oldest->stamp) oldest = &g_gh[i];
    InterlockedExchange(&oldest->ctrl, (LONG)ctrl);
    oldest->stamp = 0; oldest->active = 0; oldest->anchored = 0; oldest->latched = 0;
    return oldest;
}

struct ReleaseRecord {
    volatile LONG valid;
    uint32_t seq, timestamp, threadId, ctrl, unit, n, node;
    float    t, s0, pathEnd, wmax, depth, D, Dp, P, A, runStart, runEnd;
    int32_t  nNodes, nRuns, run;
    uint8_t  applied, toEnd, reason, pad;   // reason 0 vanilla, 1 past_run, 2 end_gap, 3 hold, 4 park (v42)
};
static const int RELREC_SIZE = 16 * 1024;
static ReleaseRecord* g_relRecs = nullptr;
static volatile LONG g_relRecIdx = 0, g_relRecOverflow = 0;
static volatile LONG g_gPlanCalls = 0, g_gPlanBadCursor = 0, g_gPlanWalkFail = 0, g_gPlanNoWide = 0,
    g_gPlanNoRuns = 0, g_gPlanWithRuns = 0, g_gPlanNodesSum = 0, g_gAdvCalls = 0, g_gAdvOtherSite = 0,
    g_gAdvNoPlan = 0, g_gAdvMismatch = 0, g_gAdvVanilla = 0, g_gAdvApplied = 0, g_gAdvToEnd = 0, g_gAdvNotFarther = 0;

static GPending* GPendingFind(DWORD tid) {
    for (int i = 0; i < G_PENDING; i++)
        if ((DWORD)g_gp[i].tid == tid) return &g_gp[i];
    return nullptr;
}

static GPending* GPendingSlot(DWORD tid) {
    GPending* have = GPendingFind(tid);
    if (have) return have;
    for (int i = 0; i < G_PENDING; i++)
        if (InterlockedCompareExchange(&g_gp[i].tid, (LONG)tid, 0) == 0) return &g_gp[i];
    GPending* oldest = &g_gp[0];       // table full: take the oldest (plans live for one call)
    for (int i = 1; i < G_PENDING; i++)
        if ((DWORD)g_gp[i].stamp < (DWORD)oldest->stamp) oldest = &g_gp[i];
    InterlockedExchange(&oldest->tid, (LONG)tid);
    return oldest;
}

static void GPlan(uint32_t ctrl, uint32_t unit, uint32_t n, DWORD now) {
    if (g_fixMode != FIX_V38_G || g_fixDisabled) return;
    InterlockedIncrement(&g_gPlanCalls);
    GPending* gp = GPendingSlot(GetCurrentThreadId());
    InterlockedExchange(&gp->stamp, 0);
    uint32_t node = 0;
    float t = 0.0f;
    if (!ReadDword(ctrl + 0x34, &node) || !MemcpyTry(&t, ctrl + 0x38, 4) || !PtrPlausible(node) || !_finite(t)) {
        InterlockedIncrement(&g_gPlanBadCursor);
        return;
    }
    uint32_t visited[G_MAX_NODES];
    float start[G_MAX_NODES], len[G_MAX_NODES], wid[G_MAX_NODES];
    int nn = 0;
    float arc = 0.0f, s0 = 0.0f, wmax = 0.0f;
    uint32_t cur = node;
    bool fail = false;
    while (cur) {
        if (nn >= G_MAX_NODES || !PtrPlausible(cur) || !RangeReadable(cur, 0x90)) { fail = true; break; }
        for (int k = 0; k < nn; k++) if (visited[k] == cur) { fail = true; break; }
        if (fail) break;
        uint32_t next = 0;
        float w74 = 0, w78 = 0, f[4] = { 0, 0, 0, 0 };
        if (!ReadDword(cur + 0x08, &next) || !MemcpyTry(&w74, cur + 0x74, 4) || !MemcpyTry(&w78, cur + 0x78, 4) ||
            !MemcpyTry(f, cur + 0x80, 16)) { fail = true; break; }
        float L = sqrtf((f[2] - f[0]) * (f[2] - f[0]) + (f[3] - f[1]) * (f[3] - f[1]));
        if (!_finite(L) || L > 2000.0f || !_finite(w74) || !_finite(w78)) { fail = true; break; }
        float w = w74 < w78 ? w74 : w78;
        float wm = w74 < w78 ? w78 : w74;
        if (w < 0.0f) w = 0.0f;
        visited[nn] = cur; start[nn] = arc; len[nn] = L; wid[nn] = w;
        if (nn == 0) {
            float tc = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
            s0 = arc + tc * L;
        }
        if (L > 0.01f && wm > wmax) wmax = wm;
        arc += L;
        nn++;
        cur = next;
    }
    if (fail || nn == 0) { InterlockedIncrement(&g_gPlanWalkFail); return; }
    InterlockedExchangeAdd(&g_gPlanNodesSum, nn);
    gp->ctrl = ctrl; gp->unit = unit; gp->node = node; gp->n = n; gp->t = t;
    gp->s0 = s0; gp->pathEnd = arc; gp->wmax = wmax; gp->nNodes = nn; gp->nRuns = 0;
    {   // v39: the leader index is 0 at the first laying call after an order (measured, section AO)
        uint32_t li = 0xFFFFFFFF;
        ReadDword(ctrl + 0x30, &li);
        gp->newOrder = li == 0 ? 1 : 0;
        // v42: has the leader (soldier li of the unit [ctrl+4]) arrived at his slot? The laying call
        // re-orders only men who are not ready (FUN_103cbe40), so an arrived leader stays where he is
        // and the unit cursor (his path projection) stops.
        gp->leaderParked = 0;
        uint32_t u = 0, cnt = 0, arr = 0, s = 0;
        uint8_t b384 = 0, b308 = 0;
        if (ReadDword(ctrl + 0x04, &u) && PtrPlausible(u) && ReadDword(u + 0x18, &cnt) && ReadDword(u + 0x1C, &arr) &&
            li < cnt && cnt <= (uint32_t)MAX_SOLDIERS && PtrPlausible(arr) && ReadDword(arr + 4 * li, &s) &&
            PtrPlausible(s) && ReadByte(s + 0x384, &b384) && ReadByte(s + 0x308, &b308))
            gp->leaderParked = (b384 == 1 && b308 == 1) ? 1 : 0;
    }
    float files = floorf(wmax / G_FILE_M + 0.001f);
    if (files < 1.0f) files = 1.0f;
    gp->depth = ceilf((float)(n ? n : 1) / files) * G_ROW_M;
    if (wmax >= G_MIN_WIDE_M) {
        float limit = G_NARROW_FRAC * wmax;
        int r = -1;
        for (int i = 0; i < nn; i++) {
            bool narrow = len[i] > 0.01f && wid[i] < limit;
            if (narrow) {
                if (r >= 0 && gp->runs[r][1] >= start[i] - 0.01f) gp->runs[r][1] = start[i] + len[i];   // extend
                else if (gp->nRuns < G_MAX_RUNS) { r = gp->nRuns++; gp->runs[r][0] = start[i]; gp->runs[r][1] = start[i] + len[i]; }
                else break;
            } else if (len[i] > 0.01f) r = -1;
        }
    } else InterlockedIncrement(&g_gPlanNoWide);
    InterlockedIncrement(gp->nRuns ? &g_gPlanWithRuns : &g_gPlanNoRuns);
    InterlockedExchange(&gp->stamp, (LONG)(now ? now : 1));
}

static void RelRow(const GPending* gp, float D, float Dp, float P, float A, int run, uint8_t applied, uint8_t toEnd,
                   uint8_t reason, DWORD now) {
    if (!g_relRecs) return;
    DWORD elapsed = 0;
    if (!WindowOpen(now, &elapsed)) return;
    LONG idx = InterlockedIncrement(&g_relRecIdx) - 1;
    if (idx >= RELREC_SIZE) { InterlockedExchange(&g_relRecOverflow, 1); return; }
    ReleaseRecord& w = g_relRecs[idx];
    w.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    w.timestamp = elapsed;
    w.threadId = GetCurrentThreadId();
    w.ctrl = gp->ctrl; w.unit = gp->unit; w.n = gp->n; w.node = gp->node; w.t = gp->t;
    w.s0 = gp->s0; w.pathEnd = gp->pathEnd; w.wmax = gp->wmax; w.depth = gp->depth;
    w.D = D; w.Dp = Dp; w.P = P; w.A = A;
    w.nNodes = gp->nNodes; w.nRuns = gp->nRuns; w.run = run;
    w.runStart = run >= 0 ? gp->runs[run][0] : 0.0f;
    w.runEnd = run >= 0 ? gp->runs[run][1] : 0.0f;
    w.applied = applied; w.toEnd = toEnd; w.reason = reason; w.pad = 0;
    InterlockedExchange(&w.valid, 1);
}

static void AdvanceHit(const uint8_t* ctx) {
    InterlockedIncrement(&g_gAdvCalls);
    if (g_fixMode != FIX_V38_G || g_fixDisabled) return;
    if (CtxDword(ctx, CTX_OFF_RET) != g_gCallSite) { InterlockedIncrement(&g_gAdvOtherSite); return; }
    DWORD now = GetTickCount();
    GPending* gp = GPendingFind(GetCurrentThreadId());
    if (!gp || !gp->stamp || now - (DWORD)gp->stamp > G_PLAN_AGE_MS) { InterlockedIncrement(&g_gAdvNoPlan); return; }
    InterlockedExchange(&gp->stamp, 0);      // one advance per plan
    uint32_t cur = CtxDword(ctx, CTX_OFF_ECX);
    uint32_t node = 0;
    float t = 0.0f;
    if (!ReadDword(cur, &node) || !MemcpyTry(&t, cur + 4, 4) || node != gp->node || fabsf(t - gp->t) > 1e-3f) {
        InterlockedIncrement(&g_gAdvMismatch);
        return;
    }
    float* dist = (float*)(ctx + CTX_OFF_ARG1);   // the game stack: FUN_103670a0's distance argument
    float D = *dist;
    if (!_finite(D) || D < 0.0f || D > G_MAX_D_M) { InterlockedIncrement(&g_gAdvMismatch); return; }
    float P = gp->s0 + D;
    int run = -1;
    for (int i = 0; i < gp->nRuns; i++)
        if (P >= gp->runs[i][0] && P < gp->runs[i][1]) { run = i; break; }
    float A = P, Dp = D;
    uint8_t applied = 0, toEnd = 0, reason = 0;
    if (run < 0) {
        InterlockedIncrement(&g_gAdvVanilla);
    } else {
        A = gp->runs[run][1] + gp->depth + G_MARGIN_M;
        for (int i = run + 1; i < gp->nRuns; i++)
            if (A >= gp->runs[i][0] && A < gp->runs[i][1]) A = gp->runs[i][1] + gp->depth + G_MARGIN_M;
        reason = 1;
        if (A > gp->pathEnd - G_END_GAP_M) { A = gp->pathEnd - G_END_GAP_M; reason = 2; toEnd = 1; }   // v40: before the end
        Dp = A - gp->s0;
        if (Dp > D) {
            *dist = Dp;
            applied = 1;
            InterlockedIncrement(&g_gAdvApplied);
            if (toEnd) InterlockedIncrement(&g_gAdvToEnd);
        } else {
            InterlockedIncrement(&g_gAdvNotFarther);
            Dp = D;
        }
    }
    {   // v39: never lay the front behind the anchor applied earlier on this path
        GHold* gh = GHoldFor(gp->ctrl);
        DWORD gnow = GameNow();
        float curRem = gp->pathEnd - gp->s0;
        bool newPath = gp->newOrder || gh->stamp == 0 || curRem > gh->remCursor + 1.0f;
        if (newPath) { gh->anchored = 0; gh->latched = 0; }   // v42
        if (newPath || gnow - gh->stamp > G_HOLD_AGE_MS) {
            if (gh->active) InterlockedIncrement(&g_gHoldReset);
            gh->active = 0;
        }
        if (applied) {
            gh->remFront = gp->pathEnd - A;
            gh->active = 1;
            gh->anchored = 1;
        } else if (gh->active) {
            float remP = gp->pathEnd - P;
            if (remP > gh->remFront + 0.05f) {
                float Ah = gp->pathEnd - gh->remFront;
                float Dh = Ah - gp->s0;
                if (Dh > D) {
                    *dist = Dh;
                    A = Ah; Dp = Dh; applied = 2; reason = 3;
                    InterlockedIncrement(&g_gAdvHeld);
                }
            } else {
                gh->active = 0;
                InterlockedIncrement(&g_gAdvHoldEnd);
            }
        }
        // v42 park release (run 20261017_114804): the anchor put the leader's slot short of the
        // path end, he arrived there and, being ready, is never re-ordered, so the cursor (his
        // projection) stops and cursor + D never reaches the end. The game's own release (front
        // at the end: every man still out of his slot gets a formation move to the destination
        // layout) then never comes, and men on go-to orders that cannot reach their slots (units
        // stacked on one spot) keep the unit from ever finishing its order. When the leader is
        // parked behind such an anchor, no narrow run is ahead and the rest of the path is no
        // longer than D + block depth + margin (the most the anchor moved him ahead of the game's
        // own creep), the front goes past the end; it stays there (latched) until a new order or
        // a new path, as the game's front does once it reached the end.
        bool runAhead = false;
        for (int i = 0; i < gp->nRuns; i++)
            if (gp->runs[i][1] > gp->s0 + 0.01f) { runAhead = true; break; }
        if (!runAhead && !gp->newOrder && gp->s0 + Dp < gp->pathEnd &&
            (gh->latched || (gh->anchored && gp->leaderParked && curRem <= D + gp->depth + G_MARGIN_M))) {
            Dp = curRem + G_PARK_PAST_M;
            A = gp->pathEnd + G_PARK_PAST_M;
            *dist = Dp;
            applied = 3; toEnd = 1; reason = 4;
            InterlockedIncrement(gh->latched ? &g_gAdvParkLatched : &g_gAdvPark);
            gh->latched = 1;
            gh->active = 0;
        }
        gh->remCursor = curRem;
        gh->stamp = gnow ? gnow : 1;
    }
    RelRow(gp, D, Dp, P, A, run, applied, toEnd, reason, now);
}

static void HandleHit(const uint8_t* ctx, int h) {
    if (g_state == 0 || g_layoutBad) return;

    // Self-check of the generated offsets, on live data: PUSHAD saved the ESP
    // it had on entry, which is exactly the address of the return-address
    // slot (entry hooks) or the hook-point ESP (the v17 mid-function patch,
    // WPF_BYTES_BELOW_ENTRY above ctx). If this does not hold, the offsets are
    // wrong and nothing is recorded (the dump thread reports it).
    uint32_t entryEsp = (uint32_t)(uintptr_t)(ctx + (h == H_WAYPOINT ? WPF_BYTES_BELOW_ENTRY :
                                                     h == H_RADIUS ? RQF_BYTES_BELOW_ENTRY : CTX_OFF_RET));
    if (CtxDword(ctx, CTX_OFF_ESP_BEFORE_PUSHAD) != entryEsp) {
        InterlockedExchange(&g_layoutBad, 1);
        return;
    }
    if (h == H_STEER)        SteerHit(ctx);
    else if (h == H_ADVANCE) AdvanceHit(ctx);
    else if (h == H_RADIUS)  RadiusHit(ctx);
    else if (h == H_WAYPOINT) WaypointHit(ctx);
    else if (h == H_CURSOR)  CursorHit(ctx);
    else if (h == H_BUILDER) BuilderHit(ctx);
    else if (h == H_ORDERS)  OrdersHit(ctx);
    else if (h == H_GOTO)    GotoHit(ctx, GOTO_GOTO);
    else if (h == H_OFFMESH) GotoHit(ctx, GOTO_OFFMESH);
    else                     EventHit(ctx, h);
}

// ---------------------------------------------------------------------------
// Handlers and naked detours. The detour sequence is exactly the one given to
// tools/stack_calc.py:   pushad, pushfd, mark:ctx, push:ctxptr
// "push esp" pushes the value ESP had before the push, i.e. the ctx pointer.
// The handlers are __stdcall and remove that one argument.
// ---------------------------------------------------------------------------

// The naked stub is HOOK_DETOUR (proxy/hook.h), the same sequence.
#define DEFINE_HOOK(i)                                                        \
    extern "C" void __stdcall Handler##i(const uint8_t* ctx) { HandleHit(ctx, i); } \
    HOOK_DETOUR(Detour##i, Handler##i, g_trampolines, i)

DEFINE_HOOK(0)
DEFINE_HOOK(1)
DEFINE_HOOK(2)
DEFINE_HOOK(3)
DEFINE_HOOK(4)
DEFINE_HOOK(5)
DEFINE_HOOK(6)
DEFINE_HOOK(7)
DEFINE_HOOK(8)
DEFINE_HOOK(9)
DEFINE_HOOK(10)
DEFINE_HOOK(11)
DEFINE_HOOK(12)
DEFINE_HOOK(13)
DEFINE_HOOK(14)
DEFINE_HOOK(15)
DEFINE_HOOK(16)

static const HookDetour g_detours[HOOK_COUNT] = {
    Detour0, Detour1, Detour2, Detour3, Detour4, Detour5, Detour6, Detour7, Detour8, Detour9,
    Detour10, Detour11, Detour12, Detour13, Detour14, Detour15, Detour16
};

// ---------------------------------------------------------------------------
// Output (v16). Every file lives in this launch's run folder (LogRunDir(),
// src/log.cpp). Record files are opened once and APPENDED: each flush writes
// the completed records after the last one written (a record still being
// filled stops the pass; the final pass skips and counts it). Summary files
// (totals, watch_totals, units, faults) are rewritten through <name>.tmp and
// a rename. Flushes run every 10 s on the dump thread, at the end of the
// window, at process detach and from the crash filter, under g_flushCS.
// ---------------------------------------------------------------------------

static void BuildPath(char* out, const char* fileName) {
    snprintf(out, MAX_PATH, "%s%s", LogRunDir(), fileName);
}

// The run folder is new, so a name can only be taken by an earlier file of
// this launch; still never overwrite blind: move it to <name>.bak(N).
static void MoveExistingAside(const char* path) {
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;
    char bak[MAX_PATH + 16];
    for (int n = 1; n < 1000; n++) {
        if (n == 1) sprintf(bak, "%s.bak", path);
        else        sprintf(bak, "%s.bak%d", path, n);
        if (GetFileAttributesA(bak) == INVALID_FILE_ATTRIBUTES) break;
    }
    if (MoveFileA(path, bak)) Log("Previous %s kept as %s", path, bak);
}

// RVA text for a return address: the RVA inside empire.retail.dll, or
// "outside" when the caller is in another module.
static void RetText(char* out, uint32_t abs) {
    if (abs == 0) { strcpy(out, ""); return; }
    if (abs >= g_moduleBaseVal && abs - g_moduleBaseVal < g_moduleSize)
        sprintf(out, "0x%06X", abs - g_moduleBaseVal);
    else
        strcpy(out, "outside");
}

static const char* HookName(uint8_t h) {
    if (h == HOOK_MARKER) return "MARKER";
    if (h == HOOK_PRESET) return "PRESET";
    if (h == HOOK_FIXMODE) return "FIXMODE";
    if (h == HOOK_WATCHTOGGLE) return "WATCH";
    if (h < HOOK_COUNT) return g_hookDefs[h].func;
    return "?";
}

static void HexInto(char* out, const uint8_t* p, uint32_t n) {
    static const char* digits = "0123456789ABCDEF";
    for (uint32_t i = 0; i < n; i++) {
        out[2 * i]     = digits[p[i] >> 4];
        out[2 * i + 1] = digits[p[i] & 15];
    }
    out[2 * n] = 0;
}

struct OutFile {
    const char* name;
    const char* header;
    FILE*       f;
    LONG        next;          // next record index to write
    LONG        written;
    LONG        lost;          // never completed (final pass only)
    bool        failed;
    bool        closed;        // closed by the final flush; never reopened
};

static bool OutReady(OutFile& o) {
    if (o.f) return true;
    if (o.failed || o.closed) return false;
    char path[MAX_PATH];
    BuildPath(path, o.name);
    MoveExistingAside(path);
    o.f = fopen(path, "w");
    if (!o.f) {
        o.failed = true;
        Log("ERROR: cannot open %s", path);
        return false;
    }
    fputs(o.header, o.f);
    return true;
}

template <typename R, typename W>
static void Append(OutFile& o, R* recs, volatile LONG* idx, LONG size, bool final, W writeRow) {
    if (!recs || !OutReady(o)) return;
    LONG count = *idx;
    if (count > size) count = size;
    LONG i = o.next;
    for (; i < count; i++) {
        if (!recs[i].valid) {
            if (!final) break;
            o.lost++;
            continue;
        }
        writeRow(o.f, recs[i]);
        o.written++;
    }
    o.next = i;
    fflush(o.f);
}

static void OutClose(OutFile& o, const char* what) {
    o.closed = true;
    if (!o.f) return;
    fclose(o.f);
    o.f = nullptr;
    char path[MAX_PATH];
    BuildPath(path, o.name);
    if (o.lost) Log("Wrote %s (%ld %s; %ld claimed but never completed)", path, (long)o.written, what, (long)o.lost);
    else        Log("Wrote %s (%ld %s)", path, (long)o.written, what);
}

// Summary files: write <name>.tmp, then replace <name>.
static FILE* SummaryOpen(const char* name, char* tmpPath, char* finalPath) {
    BuildPath(finalPath, name);
    snprintf(tmpPath, MAX_PATH + 8, "%s.tmp", finalPath);
    return fopen(tmpPath, "w");
}
static void SummaryClose(FILE* f, const char* tmpPath, const char* finalPath) {
    fclose(f);
    if (!MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING))
        Log("ERROR: cannot replace %s (error %lu)", finalPath, GetLastError());
}

static OutFile g_outEvents = { "pathfix_capture_v42_events.csv",
    "seq,timestamp_ms,thread_id,hook_id,hook_name,ecx,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,"
    "arg4_as_float,ecx_60,ecx_60_as_float,ecx_60_ok,ret_abs,ret_rva,hit_no,record_reason,marker_no\n" };
static OutFile g_outCursor = { "pathfix_capture_v42_cursor.csv",
    "seq,timestamp_ms,thread_id,this_ptr,arg1,ret_abs,ret_rva,call_no,reason,p_ptr,cursor_addr,"
    "node,progress,progress_hex,flag_74,node_next,read_flags,"
    "node_dump_len,node_dump_hex,p_dump_len,p_dump_hex\n" };
static OutFile g_outWatch = { "pathfix_capture_v42_watch.csv",
    "seq,timestamp_ms,thread_id,hook_id,slot,slot_gen,unit_this,watch_addr,watch_base,watch_offset,"
    "eip,eip_rva,eip_in_module,old_value,new_value,old_as_float,new_as_float,value_changed,is_read,"
    "eax,ebx,ecx,edx,esi,edi,ebp,esp,"
    "stack_rva_0,stack_rva_1,stack_rva_2,stack_rva_3,stack_rva_4,stack_rva_5,"
    "stack_rva_6,stack_rva_7,hits_this_eip_total,record_reason\n" };
static OutFile g_outSlots = { "pathfix_capture_v42_slots.csv",
    "seq,timestamp_ms,thread_id,slot,kind,unit_this,prev_this,p_ptr,old_addr,new_addr,new_node,value_hex,value_as_float\n" };
static OutFile g_outSweeps = { "pathfix_capture_v42_sweeps.csv",
    "timestamp_ms,threads,armed,changed,failed,slot_mask,disarm\n" };
static OutFile g_outBuilds = { "pathfix_capture_v42_builds.csv",
    "seq,timestamp_ms,thread_id,ret_abs,ret_rva,ecx,p_ptr,head,arg3,arg4,nodes,stop_reason,"
    "ms_since_ctor,overridden,hit_no,record_reason\n" };
static OutFile g_outNodes = { "pathfix_capture_v42_build_nodes.csv",
    "build_seq,index,node,next,w74,w78,w74_hex,w78_hex,overridden,new74,new78,"
    "side,clearance,base0_x,base0_z,base1_x,base1_z,follow0_x,follow0_z,follow1_x,follow1_z,"
    "dump_len,dump_hex\n" };
static OutFile g_outOrders = { "pathfix_capture_v42_orders.csv",
    "seq,timestamp_ms,thread_id,ctrl,unit,soldier_count,cursor_node,cursor_t,ret_abs,ret_rva,listed,leader_idx,ready,speed_mode,soldiers\n" };
static OutFile g_outGotos = { "pathfix_capture_v42_gotos.csv",
    "seq,timestamp_ms,thread_id,kind,soldier,ret_abs,ret_rva,order_ptr,order_len,"
    "x,y,z,region,aux,heading,d18,d1c,f20,f24,b28,b29,order_hex\n" };
static OutFile g_outSoldiers = { "pathfix_capture_v42_soldiers.csv",
    "seq,timestamp_ms,thread_id,sample_no,reason,ctrl,unit,soldier_count,index,soldier,ent,ent_kind,flags,"
    "pos_x,pos_y,pos_z,heading,loc_x,loc_z,"
    "s_state,s_path,s_dest_x,s_dest_z,s_wp_x,s_wp_z,"
    "e_state,e_path,e_dest_x,e_dest_z,e_wp_x,e_wp_z,s_corridor,s_radius,"
    "spd_max,spd_cur,spd_tgt,spd_cmd,spd_mul,ord_spd,ord_t,ord_mode,elapsed,moving,direct,f384,f308\n" };

static OutFile g_outFix = { "pathfix_capture_v42_fix.csv",
    "seq,timestamp_ms,thread_id,soldier,unit,mode,status,side,clamped,portal,type,edge,twin,len,r,e30,"
    "a_x,a_z,b_x,b_z,ma_x,ma_z,mb_x,mb_z,pa_x,pa_z,pb_x,pb_z,lane,"
    "before_x,before_y,before_z,after_x,after_z,t,t_i,t_min,t_max,scale,"
    "pos_x,pos_z,dest_x,dest_z,batch\n" };
static OutFile g_outRadius = { "pathfix_capture_v42_radius.csv",
    "seq,timestamp_ms,thread_id,soldier,unit,lane,prev_lane,reason,n,cnt0,cnt1,cnt2,q,w0,w,vertex_x,vertex_z,pos_x,pos_z,"
    "key,candidates\n" };
static OutFile g_outAvoid = { "pathfix_capture_v42_avoid.csv",
    "seq,timestamp_ms,thread_id,soldier,unit,kind,deg,side,hold,wall,n_stalled,n_mover,n_ignored,n_other,n_beyond,"
    "blocker,pos_x,pos_z,din_x,din_z,dout_x,dout_z,f0,fbest,horizon,speed,blk_vx,blk_vz\n" };
static OutFile g_outRelease = { "pathfix_capture_v42_release.csv",
    "seq,timestamp_ms,thread_id,ctrl,unit,n,node,t,cursor_arc,path_end,nodes,max_width,narrow_runs,run,run_start,run_end,"
    "depth,D,D_new,P,A,applied,to_end,reason\n" };
static OutFile g_outFixRaw = { "pathfix_capture_v42_fixraw.csv",
    "seq,timestamp_ms,portal,edge,twin,portal_len,portal_hex,edge_len,edge_hex,twin_len,twin_hex\n" };

static void RowFix(FILE* f, const FixRecord& r) {
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,%u,%s,%u,%u,0x%08X,%u,0x%08X,0x%08X,%.4f,%.4f,%.4f,"
               "%.3f,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,"
               "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,"
               "%.3f,%.3f,%.3f,%.3f,%u\n",
        r.seq, r.timestamp, r.threadId, r.soldier, r.unit, (unsigned)r.mode,
        r.status < FS_COUNT ? kFixStatus[r.status] : "?", (unsigned)r.side, (unsigned)r.clamped,
        r.portal, r.type, r.edge, r.twin, r.len, r.r, r.e30, r.a[0], r.a[1], r.b[0], r.b[1],
        r.ma[0], r.ma[1], r.mb[0], r.mb[1], r.pa[0], r.pa[1], r.pb[0], r.pb[1], r.lane,
        r.before[0], r.before[1], r.before[2], r.after[0], r.after[1],
        r.t, r.ti, r.tmin, r.tmax, r.scale, r.pos[0], r.pos[1], r.dest[0], r.dest[1], r.batch);
}

static void RowRadius(FILE* f, const RadiusRecord& r) {
    static const char* const reason[3] = { "assigned", "balanced", "kept" };
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,%u,%d,%s,%u,%u,%u,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%u\n",
        r.seq, r.timestamp, r.threadId, r.soldier, r.unit, (unsigned)r.lane,
        r.prevLane == 255 ? -1 : (int)r.prevLane, r.reason < 3 ? reason[r.reason] : "?", (unsigned)r.n,
        (unsigned)r.cnt[0], (unsigned)r.cnt[1], (unsigned)r.cnt[2], r.q, r.w0, r.w, r.vx, r.vz, r.pos[0], r.pos[1],
        r.key ? "unit" : "own", (unsigned)r.ncand);
}

static void RowRelease(FILE* f, const ReleaseRecord& r) {
    static const char* const reason[5] = { "vanilla", "past_run", "end_gap", "hold", "park" };
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,%u,0x%08X,%.4f,%.2f,%.2f,%d,%.2f,%d,%d,%.2f,%.2f,%.2f,%.3f,%.3f,%.2f,%.2f,%u,%u,%s\n",
        r.seq, r.timestamp, r.threadId, r.ctrl, r.unit, r.n, r.node, r.t, r.s0, r.pathEnd, r.nNodes, r.wmax, r.nRuns,
        r.run, r.runStart, r.runEnd, r.depth, r.D, r.Dp, r.P, r.A, (unsigned)r.applied, (unsigned)r.toEnd,
        r.reason < 5 ? reason[r.reason] : "?");
}

static void RowAvoid(FILE* f, const AvoidRecord& r) {
    static const char* const kind[4] = { "straight", "turned", "none_free", "wall_flag" };
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,%s,%d,%d,%u,%u,%u,%u,%u,%u,%u,0x%08X,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,"
               "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        r.seq, r.timestamp, r.threadId, r.soldier, r.unit, r.kind < 4 ? kind[r.kind] : "?", (int)r.deg,
        (int)r.side, (unsigned)r.hold, (unsigned)r.wall, (unsigned)r.nStalled, (unsigned)r.nMover,
        (unsigned)r.nIgnored, (unsigned)r.nOther, (unsigned)r.nBeyond, r.blocker, r.pos[0], r.pos[1],
        r.din[0], r.din[1], r.dout[0], r.dout[1], r.f0, r.fbest, r.H, r.speed, r.bv[0], r.bv[1]);
}

static void RowFixRaw(FILE* f, const FixRawRecord& r) {
    char ph[2 * 0x40 + 1], eh[2 * 0x40 + 1], th[2 * 0x40 + 1];
    HexInto(ph, r.pbytes, r.plen);
    HexInto(eh, r.ebytes, r.elen);
    HexInto(th, r.tbytes, r.tlen);
    fprintf(f, "%u,%u,0x%08X,0x%08X,0x%08X,%u,%s,%u,%s,%u,%s\n", r.seq, r.timestamp, r.portal,
            r.edge, r.twin, r.plen, ph, r.elen, eh, r.tlen, th);
}

static void WriteFixTotals(bool logIt) {
    char tmp[MAX_PATH + 8], fin[MAX_PATH];
    FILE* f = SummaryOpen("pathfix_capture_v42_fix_totals.csv", tmp, fin);
    if (f) fprintf(f, "counter,value\n");
    struct { const char* name; LONG v; } rows[] = {
        { "handler_calls", g_fixCalls }, { "goal_returns", g_fixGoal }, { "apex_returns", g_fixApex },
        { "frame_checks", g_fixFrameChecked }, { "frame_failures", g_fixFrameFail },
        { "geometry_checked", g_fixGeoChecked }, { "geometry_failures", g_fixGeoFail },
        { "apex_match_checked", g_fixMatchChecked }, { "apex_match_failures", g_fixMatchFail },
        { "seh_faults", g_fixFaults }, { "disabled", g_fixDisabled }, { "mode_now", g_fixMode },
        { "soldier_table_entries", g_fixSoldierUsed }, { "soldier_table_full", g_fixSoldierFull },
        { "unit_table_full", g_fixUnitFull }, { "lane_cache_misses", g_fixLaneMiss },
        { "fix_on", g_fixOn }, { "lane_mode", g_fixLane }, { "raw_dumps", g_fixRawIdx },
        { "samples_from_fix_hook", g_fixSamples },
        { "cross_checked", g_fixCrossChecked }, { "cross_failures", g_fixCrossFail },
        { "other_pulled", g_fixOtherPulled },
        { "turn_capped", g_fixTurnCapped }, { "stuck_releases", g_fixStuckReleases },
        { "lateral_calls", g_fixLatCalls }, { "lateral_group_sum", g_fixLatGroupSum },
        { "lateral_solo", g_fixLatSolo }, { "lateral_lane_cut", g_fixLatLaneCut },
        { "far_lane", g_fixFarLane }, { "tip_new", g_fixTipNew }, { "tip_cont", g_fixTipCont },
        { "tip_far", g_fixTipFar }, { "tip_near", g_fixTipNear }, { "tip_past", g_fixTipPast },
        { "tip_no_queue", g_fixTipNoQueue }, { "tip_short_lane", g_fixTipShort }, { "tip_stuck", g_fixTipStuck }, { "fix_variant", g_fixVariant }, { "unit_order_notes", g_unitOrderNotes },
        { "gt_notes", g_gtNotes }, { "gt_reads", g_gtReads }, { "gt_read_fail", g_gtFail },
        { "gt_fallback", g_gtFallback }, { "gt_object_changes", g_gtObjChanges }, { "gt_last_tick", g_gtLastTick },
        { "gather_adjusted", g_gather[GS_ADJUSTED] }, { "gather_no_order", g_gather[GS_NO_ORDER] },
        { "gather_read_fail", g_gather[GS_READ] }, { "gather_no_queue", g_gather[GS_NO_QUEUE] },
        { "gather_slot_ahead", g_gather[GS_SLOT_AHEAD] }, { "gather_passed", g_gather[GS_PASSED] },
        { "gather_far", g_gather[GS_FAR] }, { "gather_stuck_release", g_gather[GS_STUCK] },
        { "lead_calls", g_fixLeadCalls }, { "lead_sector_clamp", g_fixLeadSectorClamp },
        { "lead_no_free", g_fixLeadNoFree }, { "lead_radius_moved", g_fixLeadMoved },
        { "lead_radius_cut", g_fixLeadRadiusCut },
        { "steer_hits", g_stHits }, { "steer_tracked", g_stTracked }, { "steer_turned", g_stTurned },
        { "steer_blocked_ahead", g_stBlocked0 }, { "steer_no_apex", g_stNoApex }, { "steer_far", g_stFar },
        { "steer_no_unit", g_stNoUnit }, { "steer_zero_d", g_stZeroD },
        { "steer_radius_default", g_stRadiusDefault }, { "steer_neighbour_sum", g_stNeighSum },
        { "steer_faults", g_stFaults }, { "steer_nodamp", g_stNoDamp },
        { "avoid_checked", g_avChecked }, { "avoid_triggered", g_avTriggered }, { "avoid_turned", g_avTurned },
        { "avoid_straight_best", g_avStraight }, { "avoid_none_free", g_avNoneFree }, { "avoid_clear", g_avClear },
        { "avoid_held", g_avHeld }, { "avoid_hold_broken", g_avHoldBroken }, { "avoid_oncoming", g_avOncoming },
        { "avoid_wall_flags", g_avWallFlags }, { "avoid_wall_excluded", g_avWallExcluded },
        { "avoid_not_following", g_avNotFollowing }, { "avoid_small_d", g_avSmallD },
        { "avoid_near_slot", g_avNearSlot }, { "avoid_snapshot_rebuilds", g_avRebuilds },
        { "avoid_snapshot_contended", g_avContended }, { "avoid_no_table", g_avNoTable },
        { "avoid_obstacles_stalled", g_avStalledObs }, { "avoid_obstacles_mover", g_avMoverObs },
        { "avoid_ignored_same_way", g_avIgnoredObs }, { "avoid_obstacles_other_unit", g_avOtherObs },
        { "avoid_beyond_slot", g_avBeyondSlot }, { "avoid_no_velocity", g_avNoVel },
        { "avoid_obstacle_list_full", g_avObFull }, { "avoid_faults", g_avFaults },
        { "avoid_rows", g_avRecIdx }, { "avoid_rows_overflow", g_avRecOverflow },
        { "radius_calls", g_rqCalls }, { "radius_active", g_rqActive }, { "radius_frame_checked", g_rqFrameChecked },
        { "radius_frame_failures", g_rqFrameFail }, { "radius_frame_bad", g_rqFrameBad },
        { "radius_read_fail", g_rqReadFail }, { "radius_no_unit", g_rqNoUnit }, { "radius_no_corner", g_rqNoCorner },
        { "radius_no_queue", g_rqNoQueue }, { "radius_stuck", g_rqStuck }, { "radius_balanced", g_rqBalanced },
        { "radius_kept", g_rqKept }, { "radius_raised", g_rqRaised }, { "radius_lane0", g_rqLane[0] },
        { "radius_key_own", g_rqKeyOwn }, { "radius_key_unit", g_rqKeyUnit }, { "radius_passed", g_rqPassed },
        { "radius_candidates", g_rqCandSum },
        { "radius_lane1", g_rqLane[1] }, { "radius_lane2", g_rqLane[2] }, { "radius_rows", g_rqRecIdx },
        { "radius_rows_overflow", g_rqRecOverflow },
        { "release_plan_calls", g_gPlanCalls }, { "release_plan_bad_cursor", g_gPlanBadCursor },
        { "release_plan_walk_fail", g_gPlanWalkFail }, { "release_plan_no_wide", g_gPlanNoWide },
        { "release_plan_no_runs", g_gPlanNoRuns }, { "release_plan_with_runs", g_gPlanWithRuns },
        { "release_plan_nodes_sum", g_gPlanNodesSum }, { "release_adv_calls", g_gAdvCalls },
        { "release_adv_other_site", g_gAdvOtherSite }, { "release_adv_no_plan", g_gAdvNoPlan },
        { "release_adv_mismatch", g_gAdvMismatch }, { "release_adv_vanilla", g_gAdvVanilla },
        { "release_adv_applied", g_gAdvApplied }, { "release_adv_to_end", g_gAdvToEnd },
        { "release_adv_not_farther", g_gAdvNotFarther }, { "release_adv_held", g_gAdvHeld },
        { "release_adv_hold_end", g_gAdvHoldEnd }, { "release_hold_reset", g_gHoldReset },
        { "release_adv_park", g_gAdvPark }, { "release_adv_park_latched", g_gAdvParkLatched }, { "release_rows", g_relRecIdx },
        { "release_rows_overflow", g_relRecOverflow },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
        if (f) fprintf(f, "%s,%ld\n", rows[i].name, (long)rows[i].v);
    for (int k = 0; k < FS_COUNT; k++)
        if (f) fprintf(f, "status_%s,%ld\n", kFixStatus[k], (long)g_fixStatus[k]);
    for (int k = 0; k < STEER_NA; k++)
        if (f) fprintf(f, "steer_angle_%s,%ld\n", kSteerName[k], (long)g_stAngle[k]);
    for (int k = 0; k < AV_NA; k++)
        if (f) fprintf(f, "avoid_angle_%d,%ld\n", kAvDeg[k], (long)g_avAngle[k]);
    if (logIt) {
        Log("Radius (R): %ld replans, %ld while R on (%ld frame failures), %ld lanes set (%ld / %ld / %ld; "
            "%ld balanced, %ld raised), %ld no corner, %ld no queue, %ld stuck",
            (long)g_rqCalls, (long)g_rqActive, (long)g_rqFrameBad, (long)(g_rqLane[0] + g_rqLane[1] + g_rqLane[2]),
            (long)g_rqLane[0], (long)g_rqLane[1], (long)g_rqLane[2], (long)g_rqBalanced, (long)g_rqRaised,
            (long)g_rqNoCorner, (long)g_rqNoQueue, (long)g_rqStuck);
        Log("Avoidance (L): %ld checked, %ld blocked ahead, %ld turned, %ld straight best, %ld none free, "
            "%ld held, %ld oncoming, %ld wall flags, %ld snapshots (%ld contended), %ld faults",
            (long)g_avChecked, (long)g_avTriggered, (long)g_avTurned, (long)g_avStraight, (long)g_avNoneFree,
            (long)g_avHeld, (long)g_avOncoming, (long)g_avWallFlags, (long)g_avRebuilds, (long)g_avContended,
            (long)g_avFaults);
        Log("Steering: %ld corner man-ticks without crowd damping (v25); avoidance %s", (long)g_stNoDamp,
            STEER_AVOID ? "on" : "off");
        Log("Steering: %ld integrator hits while on, %ld corner men steered per tick (%ld turned, %ld blocked ahead), "
            "%ld no apex, %ld far, %ld no unit, %ld zero move, %ld default radius, mean neighbours %.1f, %ld faults",
            (long)g_stHits, (long)g_stTracked, (long)g_stTurned, (long)g_stBlocked0, (long)g_stNoApex, (long)g_stFar,
            (long)g_stNoUnit, (long)g_stZeroD, (long)g_stRadiusDefault,
            g_stTracked ? (double)g_stNeighSum / (double)g_stTracked : 0.0, (long)g_stFaults);
        Log("v36 game clock: object 0x%08X, %ld notes, %ld reads, %ld read failures, %ld wall-clock fallbacks, "
            "%ld object changes, last tick %ld (%.1f game s)", (uint32_t)g_gtClock, (long)g_gtNotes, (long)g_gtReads,
            (long)g_gtFail, (long)g_gtFallback, (long)g_gtObjChanges, (long)g_gtLastTick, g_gtLastTick * 0.1);
        Log("Release (G): %ld plans (%ld bad cursor, %ld walk failures, %ld no wide node, %ld without / %ld with narrow runs, "
            "%.1f nodes per plan); advance hook %ld calls, %ld other call sites, %ld no plan, %ld mismatch, "
            "%ld vanilla, %ld moved past the run (%ld to the path end), %ld not farther, %ld held at the anchor "
            "(%ld holds ended, %ld reset), %ld park releases (+%ld latched calls); %ld rows",
            (long)g_gPlanCalls, (long)g_gPlanBadCursor, (long)g_gPlanWalkFail, (long)g_gPlanNoWide, (long)g_gPlanNoRuns,
            (long)g_gPlanWithRuns, g_gPlanCalls ? (double)g_gPlanNodesSum / (double)g_gPlanCalls : 0.0,
            (long)g_gAdvCalls, (long)g_gAdvOtherSite, (long)g_gAdvNoPlan, (long)g_gAdvMismatch, (long)g_gAdvVanilla,
            (long)g_gAdvApplied, (long)g_gAdvToEnd, (long)g_gAdvNotFarther, (long)g_gAdvHeld, (long)g_gAdvHoldEnd,
            (long)g_gHoldReset, (long)g_gAdvPark, (long)g_gAdvParkLatched, (long)g_relRecIdx);
        Log("Fix totals: %ld handler calls, %ld goal, %ld apex; adjusted %ld; mode now %ld%s",
            (long)g_fixCalls, (long)g_fixGoal, (long)g_fixApex, (long)g_fixStatus[FS_ADJUSTED],
            (long)g_fixMode, g_fixDisabled ? " (DISABLED)" : "");
        char line[512];
        int at = 0;
        for (int k = 1; k < FS_COUNT; k++)
            at += snprintf(line + at, sizeof(line) - at, "%s%s %ld", k > 1 ? ", " : "", kFixStatus[k], (long)g_fixStatus[k]);
        Log("  not adjusted: %s", line);
        Log("  self-checks: frame %ld/%ld failed, geometry %ld/%ld, apex match %ld/%ld; SEH faults %ld",
            (long)g_fixFrameFail, (long)g_fixFrameChecked, (long)g_fixGeoFail, (long)g_fixGeoChecked,
            (long)g_fixMatchFail, (long)g_fixMatchChecked, (long)g_fixFaults);
    }
    if (f) SummaryClose(f, tmp, fin);
}

static void RowEvent(FILE* f, const EventRecord& r) {
    char ret[16];
    RetText(ret, r.retAbs);
    float a4, e60;
    memcpy(&a4, &r.args[3], 4);
    memcpy(&e60, &r.ecx60, 4);
    const char* why = r.hook == HOOK_MARKER ? "marker" : r.hook == HOOK_PRESET ? "preset" :
                      r.hook == HOOK_FIXMODE ? "fixmode" :
                      r.hook == HOOK_WATCHTOGGLE ? "watch" : (r.reason == REASON_BURST ? "burst" : "quota");
    fprintf(f, "%u,%u,%u,%u,%s,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,"
               "%.6g,0x%08X,%.6g,%u,0x%08X,%s,%u,%s,%u\n",
        r.seq, r.timestamp, r.threadId, (unsigned)r.hook, HookName(r.hook),
        r.ecx, r.args[0], r.args[1], r.args[2], r.args[3], r.args[4], r.args[5], r.args[6], r.args[7],
        a4, r.ecx60, e60, (unsigned)r.ecx60ok, r.retAbs, ret, r.hitNo, why, (unsigned)r.markerNo);
}

static void RowCursor(FILE* f, const CursorRecord& r) {
    static char hexNode[2 * NODE_BYTES + 1];
    static char hexP[2 * P_BYTES + 1];
    char ret[16];
    RetText(ret, r.retAbs);
    float prog;
    memcpy(&prog, &r.progressBits, 4);
    uint32_t nl = 0, pl = 0;
    hexNode[0] = 0;
    hexP[0] = 0;
    if (r.dumpIdx >= 0 && r.dumpIdx < DUMP_SIZE) {
        const DumpSlot& s = g_dumps[r.dumpIdx];
        nl = s.nodeLen;
        pl = s.pLen;
        HexInto(hexNode, s.node, nl);
        HexInto(hexP, s.p, pl);
    }
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,0x%08X,%s,%u,%u,0x%08X,0x%08X,"
               "0x%08X,%.6g,0x%08X,%u,0x%08X,%u,%u,%s,%u,%s\n",
        r.seq, r.timestamp, r.threadId, r.thisPtr, r.arg1, r.retAbs, ret, r.callNo,
        (unsigned)r.reason, r.P, r.P ? r.P + 0xA8 : 0,
        r.node, prog, r.progressBits, (unsigned)r.flag74, r.nodeNext, (unsigned)r.readFlags,
        nl, hexNode, pl, hexP);
}

static void RowWatch(FILE* f, const WatchRecord& r) {
    bool inMod = r.eip >= g_moduleBaseVal && r.eip - g_moduleBaseVal < g_moduleSize;
    uint32_t rva = inMod ? r.eip - g_moduleBaseVal : 0;
    float of, nf;
    memcpy(&of, &r.oldValue, 4);
    memcpy(&nf, &r.newValue, 4);
    fprintf(f, "%u,%u,%u,%u,%u,%u,0x%08X,0x%08X,0x%08X,0x%X,0x%08X,0x%06X,%u,0x%08X,0x%08X,%.6g,%.6g,%u,%u",
        r.seq, r.timestamp, r.threadId, 0xE0u + r.slot, (unsigned)r.slot, r.slotGen, r.unitThis,
        r.addr, r.addr - g_watchOffset, g_watchOffset, r.eip, rva, inMod ? 1u : 0u,
        r.oldValue, r.newValue, of, nf, (unsigned)r.changed, (unsigned)r.isRead);
    for (int k = 0; k < 8; k++) fprintf(f, ",0x%08X", r.regs[k]);
    for (int k = 0; k < 8; k++) {
        if (k < r.nStack) fprintf(f, ",0x%06X", r.stack[k] - g_moduleBaseVal);
        else              fprintf(f, ",");
    }
    fprintf(f, ",%u,%u\n", r.hitsThisEip, (unsigned)r.reason);
}

static void RowSlot(FILE* f, const SlotEvent& e) {
    static const char* kinds[] = { "?", "assign", "node_change", "takeover" };
    float vf;
    memcpy(&vf, &e.value, 4);
    fprintf(f, "%u,%u,%u,%u,%s,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,%.6g\n",
        e.seq, e.timestamp, e.threadId, (unsigned)e.slot, e.kind <= 3 ? kinds[e.kind] : "?",
        e.unitThis, e.prevThis, e.P, e.oldAddr, e.newAddr,
        g_watchBase == WATCH_BASE_NODE ? e.newAddr - g_watchOffset : 0, e.value, vf);
}

static void RowSweep(FILE* f, const SweepStat& w) {
    fprintf(f, "%u,%u,%u,%u,%u,0x%X,%u\n", w.timestamp, w.threads, w.armed, w.changed,
            w.failed, (unsigned)w.mask, (unsigned)w.disarm);
}

static void RowBuild(FILE* f, const BuildRecord& r) {
    static const char* stopNames[] = { "?", "null", "range", "unreadable", "cycle", "limit" };
    char ret[16];
    RetText(ret, r.retAbs);
    fprintf(f, "%u,%u,%u,0x%08X,%s,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,%u,%s,%d,%u,%u,%s\n",
        r.seq, r.timestamp, r.threadId, r.retAbs, ret, r.ecx, r.args[0], r.args[1], r.args[2],
        r.args[3], (unsigned)r.nodes, r.stop <= 5 ? stopNames[r.stop] : "?", r.msSinceCtor,
        (unsigned)r.overridden, r.hitNo, r.reason == REASON_BURST ? "burst" : "quota");
}

// Decoded from the entry dump (before any override): side = +0x0C, base
// points +0x14 / +0x1C (x, z), followed points +0x80 / +0x88 (x, z),
// clearance +0x7C (docs/path_node_layout.md).
static void RowNode(FILE* f, const NodeRow& r) {
    static char hex[2 * NODE_BYTES + 1];
    float a, b, c, d;
    memcpy(&a, &r.w74, 4); memcpy(&b, &r.w78, 4);
    memcpy(&c, &r.new74, 4); memcpy(&d, &r.new78, 4);
    HexInto(hex, r.dump, r.dumpLen);
    fprintf(f, "%u,%u,0x%08X,0x%08X,%.6g,%.6g,0x%08X,0x%08X,%u,%.6g,%.6g,",
        r.buildSeq, (unsigned)r.index, r.addr, r.next, a, b, r.w74, r.w78,
        (unsigned)r.overridden, c, d);
    if (r.dumpLen >= 0x90) {
        uint32_t side;
        float cl, pt[8];
        memcpy(&side, r.dump + 0x0C, 4);
        memcpy(&cl, r.dump + 0x7C, 4);
        memcpy(pt, r.dump + 0x14, 16);
        memcpy(pt + 4, r.dump + 0x80, 16);
        fprintf(f, "%u,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,",
            side, cl, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7]);
    } else {
        fprintf(f, ",,,,,,,,,,");
    }
    fprintf(f, "%u,%s\n", (unsigned)r.dumpLen, hex);
}

static void RowOrder(FILE* f, const OrderRecord& r) {
    char ret[16];
    RetText(ret, r.retAbs);
    float t;
    memcpy(&t, &r.cursorT, 4);
    fprintf(f, "%u,%u,%u,0x%08X,0x%08X,%u,0x%08X,%.6g,0x%08X,%s,%u,%u,%u,%u,",
        r.seq, r.timestamp, r.threadId, r.ctrl, r.unit, r.count, r.cursorNode, t, r.retAbs, ret, r.listed,
        r.leaderIdx, r.ready, r.speedMode);
    if (r.listStart >= 0)
        for (uint32_t i = 0; i < r.listed; i++)
            fprintf(f, i ? ";%08X" : "%08X", g_soldierPool[r.listStart + i]);
    fprintf(f, "\n");
}

static void RowGoto(FILE* f, const GotoRecord& r) {
    char ret[16];
    RetText(ret, r.retAbs);
    fprintf(f, "%u,%u,%u,%s,0x%08X,0x%08X,%s,0x%08X,%u,",
        r.seq, r.timestamp, r.threadId, r.kind == GOTO_GOTO ? "goto" : "offmesh",
        r.soldier, r.retAbs, ret, r.orderPtr, (unsigned)r.orderLen);
    if (r.orderLen == ORDER_BYTES) {
        float x, y, z, f20, f24;
        uint32_t region, aux, d18, d1c;
        uint16_t heading;
        memcpy(&x, r.order + 0x00, 4);
        memcpy(&y, r.order + 0x04, 4);
        memcpy(&z, r.order + 0x08, 4);
        memcpy(&region, r.order + 0x0C, 4);
        memcpy(&aux, r.order + 0x10, 4);
        memcpy(&heading, r.order + 0x14, 2);
        memcpy(&d18, r.order + 0x18, 4);
        memcpy(&d1c, r.order + 0x1C, 4);
        memcpy(&f20, r.order + 0x20, 4);
        memcpy(&f24, r.order + 0x24, 4);
        char hex[2 * ORDER_BYTES + 1];
        HexInto(hex, r.order, ORDER_BYTES);
        fprintf(f, "%.4f,%.4f,%.4f,0x%08X,0x%08X,%u,0x%08X,0x%08X,%.6g,%.6g,%u,%u,%s\n",
            x, y, z, region, aux, (unsigned)heading, d18, d1c, f20, f24,
            (unsigned)r.order[0x28], (unsigned)r.order[0x29], hex);
    } else {
        fprintf(f, ",,,,,,,,,,,,\n");
    }
}

static void RowSample(FILE* f, const SampleRecord& s) {
    if (s.firstRow < 0 || s.rows == 0) {
        fprintf(f, "%u,%u,%u,%u,%u,0x%08X,0x%08X,%u,-1,,,,,,,,,,,,,,,,,,,,,,,,\n",
            s.seq, s.timestamp, s.threadId, s.sampleNo, (unsigned)s.reason, s.ctrl, s.unit, s.count);
        return;
    }
    for (uint32_t i = 0; i < s.rows; i++) {
        const SoldierRow& w = g_srows[s.firstRow + i];
        const char* kind = (w.flags & SR_E_SELF) ? "self" : (w.flags & SR_E_OK) ? "other" : "bad";
        fprintf(f, "%u,%u,%u,%u,%u,0x%08X,0x%08X,%u,%u,0x%08X,0x%08X,%s,%u,"
                   "%.3f,%.3f,%.3f,%u,%.3f,%.3f,"
                   "%u,0x%08X,%.3f,%.3f,%.3f,%.3f,",
            s.seq, s.timestamp, s.threadId, s.sampleNo, (unsigned)s.reason, s.ctrl, s.unit, s.count,
            (unsigned)w.index, w.soldier, w.ent, kind, (unsigned)w.flags,
            w.pos[0], w.pos[1], w.pos[2], (unsigned)w.heading, w.loc[0], w.loc[1],
            w.sState, w.sPath, w.sDest[0], w.sDest[1], w.sWp[0], w.sWp[1]);
        if (w.flags & SR_E_OK)
            fprintf(f, "%u,0x%08X,%.3f,%.3f,%.3f,%.3f,",
                w.eState, w.ePath, w.eDest[0], w.eDest[1], w.eWp[0], w.eWp[1]);
        else
            fprintf(f, ",,,,,,");
        fprintf(f, "0x%08X,%.4f,%.3f,%.3f,%.3f,%.3f,%.4f,%.3f,%.3f,%u,%.2f,%u,%u,%u,%u\n", w.corridor, w.radius,
                w.spdMax, w.spdCur, w.spdTgt, w.spdCmd, w.spdMul, w.ordSpd, w.ordT, (unsigned)w.ordMode, w.elapsed,
                (unsigned)w.moving, (unsigned)w.direct, (unsigned)w.f384, (unsigned)w.f308);
    }
}

static void WriteTotals(bool logIt) {
    char tmp[MAX_PATH + 8], fin[MAX_PATH];
    FILE* f = SummaryOpen("pathfix_capture_v42_totals.csv", tmp, fin);
    if (logIt) Log("Per-hook totals (hits since install / hits in window / records written):");
    if (f) fprintf(f, "hook_id,hook_name,role,rva,total_hits,window_hits,recorded\n");
    for (int h = 0; h < HOOK_COUNT; h++) {
        const RateState& s = g_rate[h];
        if (logIt) Log("  %-13s %-28s %10u %10u %8u", g_hookDefs[h].func, g_hookDefs[h].role,
                       s.totalHits, s.windowHits, s.recorded);
        if (f) fprintf(f, "%d,%s,%s,0x%06X,%u,%u,%u\n", h, g_hookDefs[h].func, g_hookDefs[h].role,
                       g_hookDefs[h].rva, s.totalHits, s.windowHits, s.recorded);
    }
    if (logIt) Log("  ('recorded' is unused for the cursor, orders, go-to, off-mesh and fix hooks; see their files)");
    if (f) SummaryClose(f, tmp, fin);
}

static void WriteWatchTotals(bool logIt) {
    char tmp[MAX_PATH + 8], fin[MAX_PATH];
    FILE* f = SummaryOpen("pathfix_capture_v42_watch_totals.csv", tmp, fin);
    if (logIt) Log("Watch totals per slot and EIP (EIP is the instruction after the access):");
    if (f) fprintf(f, "slot,watch_addr_last,eip,eip_rva,eip_in_module,total_hits,recorded,value_changes,reads\n");
    for (int sl = 0; sl < WATCH_SLOTS; sl++) {
        if (!g_slots[sl].used) continue;
        for (int i = 0; i < EIP_TABLE; i++) {
            const EipStat& e = g_eipStats[sl][i];
            if (!e.eip) continue;
            uint32_t eip = (uint32_t)e.eip;
            bool inMod = eip >= g_moduleBaseVal && eip - g_moduleBaseVal < g_moduleSize;
            uint32_t rva = inMod ? eip - g_moduleBaseVal : 0;
            if (logIt) Log("  slot %d  eip 0x%08X  rva 0x%06X  hits %ld  recorded %ld  value changes %ld  reads %ld",
                           sl, eip, rva, e.total, e.recorded, e.changes, e.reads);
            if (f) fprintf(f, "%d,0x%08X,0x%08X,0x%06X,%u,%ld,%ld,%ld,%ld\n",
                           sl, (uint32_t)g_slots[sl].addr, eip, rva, inMod ? 1u : 0u,
                           e.total, e.recorded, e.changes, e.reads);
        }
    }
    if (f) SummaryClose(f, tmp, fin);
}

static void WriteUnits() {
    char tmp[MAX_PATH + 8], fin[MAX_PATH];
    FILE* f = SummaryOpen("pathfix_capture_v42_units.csv", tmp, fin);
    if (!f) return;
    fprintf(f, "unit,first_ctrl,first_soldier,first_ent,ent_kind,first_count\n");
    int n = g_unitsUsed;
    for (int i = 0; i < n; i++) {
        const UnitState& u = g_units[i];
        if (!u.firstDone) {
            fprintf(f, "0x%08X,0x%08X,,,,\n", u.unit, u.firstCtrl);
            continue;
        }
        const char* kind = !PtrPlausible(u.firstEnt) ? "bad" : u.firstEnt == u.firstSoldier ? "self" : "other";
        fprintf(f, "0x%08X,0x%08X,0x%08X,0x%08X,%s,%u\n", u.unit, u.firstCtrl, u.firstSoldier,
                u.firstEnt, kind, u.firstCount);
    }
    SummaryClose(f, tmp, fin);
}

// ---------------------------------------------------------------------------
// First-chance faults inside empire.retail.dll (v16). The handler only fills a
// ring entry (no I/O, no locks); faults.csv is rewritten at each flush.
// ---------------------------------------------------------------------------

struct FaultRecord {
    volatile LONG valid;
    uint32_t n;
    uint32_t timestamp;        // ms since window open, 0 before
    uint32_t threadId;
    uint32_t code;
    uint32_t eip;
    uint32_t avKind;           // ExceptionInformation[0] for an access violation
    uint32_t avAddr;           // ExceptionInformation[1]
    uint32_t regs[8];
    uint32_t stack[8];
    uint8_t  nStack;
};
static const int FAULT_RING = 256;
static FaultRecord g_faults[FAULT_RING];
static volatile LONG g_faultCount = 0;
static PVOID g_faultVeh = nullptr;

static bool FatalCode(DWORD code) {
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
           code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
           code == EXCEPTION_IN_PAGE_ERROR || code == EXCEPTION_STACK_OVERFLOW ||
           code == 0xC0000409;   // STATUS_STACK_BUFFER_OVERRUN
}

static LONG CALLBACK FaultVeh(PEXCEPTION_POINTERS ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (!FatalCode(code)) return EXCEPTION_CONTINUE_SEARCH;
    const CONTEXT* c = ep->ContextRecord;
    uint32_t eip = c->Eip;
    if (!(eip >= g_moduleBaseVal && eip - g_moduleBaseVal < g_moduleSize)) return EXCEPTION_CONTINUE_SEARCH;
    LONG n = InterlockedIncrement(&g_faultCount) - 1;
    FaultRecord& f = g_faults[n % FAULT_RING];
    InterlockedExchange(&f.valid, 0);
    f.n = (uint32_t)n;
    f.timestamp = (g_state >= 2) ? GetTickCount() - g_captureStart : 0;
    f.threadId = GetCurrentThreadId();
    f.code = code;
    f.eip = eip;
    f.avKind = ep->ExceptionRecord->NumberParameters >= 2 ? (uint32_t)ep->ExceptionRecord->ExceptionInformation[0] : 0;
    f.avAddr = ep->ExceptionRecord->NumberParameters >= 2 ? (uint32_t)ep->ExceptionRecord->ExceptionInformation[1] : 0;
    f.regs[0] = c->Eax; f.regs[1] = c->Ebx; f.regs[2] = c->Ecx; f.regs[3] = c->Edx;
    f.regs[4] = c->Esi; f.regs[5] = c->Edi; f.regs[6] = c->Ebp; f.regs[7] = c->Esp;
    f.nStack = code == EXCEPTION_STACK_OVERFLOW ? 0 : (uint8_t)StackCandidates(c->Esp, f.stack);
    InterlockedExchange(&f.valid, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void WriteFaults() {
    LONG total = g_faultCount;
    if (total == 0) return;
    char tmp[MAX_PATH + 8], fin[MAX_PATH];
    FILE* f = SummaryOpen("pathfix_capture_v42_faults.csv", tmp, fin);
    if (!f) return;
    fprintf(f, "n,timestamp_ms,thread_id,code,eip,eip_rva,av_kind,av_addr,eax,ebx,ecx,edx,esi,edi,ebp,esp,"
               "stack_rva_0,stack_rva_1,stack_rva_2,stack_rva_3,stack_rva_4,stack_rva_5,stack_rva_6,stack_rva_7\n");
    LONG from = total > FAULT_RING ? total - FAULT_RING : 0;
    for (LONG k = from; k < total; k++) {
        const FaultRecord& r = g_faults[k % FAULT_RING];
        if (!r.valid || (LONG)r.n != k) continue;
        fprintf(f, "%u,%u,%u,0x%08X,0x%08X,0x%06X,%u,0x%08X", r.n, r.timestamp, r.threadId, r.code,
                r.eip, r.eip - g_moduleBaseVal, r.avKind, r.avAddr);
        for (int i = 0; i < 8; i++) fprintf(f, ",0x%08X", r.regs[i]);
        for (int i = 0; i < 8; i++) {
            if (i < r.nStack) fprintf(f, ",0x%06X", r.stack[i] - g_moduleBaseVal);
            else              fprintf(f, ",");
        }
        fprintf(f, "\n");
    }
    SummaryClose(f, tmp, fin);
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------

static CRITICAL_SECTION g_flushCS;
static volatile LONG g_flushInit = 0;

static void FlushLocked(bool final) {
    Append(g_outEvents, g_events, &g_eventIdx, (LONG)EVENT_SIZE, final, RowEvent);
    Append(g_outCursor, g_cursor, &g_cursorIdx, (LONG)CURSOR_SIZE, final, RowCursor);
    Append(g_outBuilds, g_builds, &g_buildIdx, (LONG)BUILD_SIZE, final, RowBuild);
    Append(g_outNodes, g_nodeRows, &g_nodeRowIdx, (LONG)NODEROW_SIZE, final, RowNode);
    Append(g_outOrders, g_orders, &g_orderIdx, (LONG)ORDERREC_SIZE, final, RowOrder);
    Append(g_outGotos, g_gotos, &g_gotoIdx, (LONG)GOTO_SIZE, final, RowGoto);
    Append(g_outSoldiers, g_samples, &g_sampleIdx, (LONG)SAMPLE_SIZE, final, RowSample);
    Append(g_outFix, g_fixRecs, &g_fixRecIdx, (LONG)FIXREC_SIZE, final, RowFix);
    Append(g_outFixRaw, g_fixRaw, &g_fixRawIdx, (LONG)FIXRAW_SIZE, final, RowFixRaw);
    Append(g_outAvoid, g_avRecs, &g_avRecIdx, (LONG)AVREC_SIZE, final, RowAvoid);
    Append(g_outRadius, g_rqRecs, &g_rqRecIdx, (LONG)RQREC_SIZE, final, RowRadius);
    Append(g_outRelease, g_relRecs, &g_relRecIdx, (LONG)RELREC_SIZE, final, RowRelease);
    if (g_watch) {
        Append(g_outWatch, g_watch, &g_watchIdx, (LONG)WATCH_SIZE, final, RowWatch);
        Append(g_outSlots, g_slotEv, &g_slotEvIdx, (LONG)SLOTEV_SIZE, final, RowSlot);
        Append(g_outSweeps, g_sweeps, &g_sweepIdx, (LONG)SWEEP_SIZE, final, RowSweep);
        WriteWatchTotals(final);
    }
    WriteTotals(final);
    WriteFixTotals(final);
    WriteUnits();
    WriteFaults();
}

// Returns false if the lock could not be taken within waitMs (a thread killed
// at process exit, or the crashing thread, may hold it).
static bool FlushAll(bool final, DWORD waitMs) {
    if (!g_flushInit) return false;
    DWORD t0 = GetTickCount();
    while (!TryEnterCriticalSection(&g_flushCS)) {
        if (GetTickCount() - t0 >= waitMs) return false;
        Sleep(10);
    }
    FlushLocked(final);
    LeaveCriticalSection(&g_flushCS);
    return true;
}

static void CloseAllOutputs() {
    OutClose(g_outEvents, "event records");
    OutClose(g_outCursor, "cursor records");
    OutClose(g_outBuilds, "builds");
    OutClose(g_outNodes, "node rows");
    OutClose(g_outOrders, "order records");
    OutClose(g_outGotos, "go-to records");
    OutClose(g_outSoldiers, "samples");
    OutClose(g_outFix, "fix rows");
    OutClose(g_outFixRaw, "raw apex side objects");
    OutClose(g_outAvoid, "avoidance rows");
    OutClose(g_outRadius, "radius rows");
    OutClose(g_outRelease, "release rows");
    OutClose(g_outWatch, "watch records");
    OutClose(g_outSlots, "slot events");
    OutClose(g_outSweeps, "sweeps");
}

// ---------------------------------------------------------------------------
// Crash capture (v16): unhandled-exception filter, chained.
// ---------------------------------------------------------------------------

typedef BOOL (WINAPI* MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
static MiniDumpWriteDumpFn g_miniDump = nullptr;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevUef = nullptr;
static volatile LONG g_crashDepth = 0;
static volatile LONG g_crashed = 0;
static int g_uefChangesLogged = 0;

static void CrashCapture(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    const CONTEXT* c = ep->ContextRecord;
    uint32_t eip = c->Eip;
    char where[64];
    if (eip >= g_moduleBaseVal && eip - g_moduleBaseVal < g_moduleSize)
        sprintf(where, "empire.retail.dll RVA 0x%06X", eip - g_moduleBaseVal);
    else if (eip >= g_selfStart && eip < g_selfEnd)
        sprintf(where, "THIS PROXY DLL + 0x%X", eip - g_selfStart);
    else
        sprintf(where, "outside both modules");
    DWORD el = g_state >= 2 ? GetTickCount() - g_captureStart : 0;
    Log("CRASH: unhandled exception 0x%08X at EIP 0x%08X (%s), thread %lu, %.1f s after window open",
        er->ExceptionCode, eip, where, GetCurrentThreadId(), el / 1000.0);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        Log("CRASH: access violation, %s of address 0x%08X",
            er->ExceptionInformation[0] == 0 ? "read" : er->ExceptionInformation[0] == 1 ? "write" : "execute",
            (uint32_t)er->ExceptionInformation[1]);
    Log("CRASH: eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    if (er->ExceptionCode != EXCEPTION_STACK_OVERFLOW) {
        uint32_t cand[8];
        uint32_t esp = c->Esp;
        char line[200];
        for (int round = 0; round < 4; round++) {   // up to 32 candidates, 8 per line
            int n = StackCandidates(esp, cand);
            if (n == 0) break;
            line[0] = 0;
            for (int i = 0; i < n; i++) sprintf(line + strlen(line), " 0x%06X", cand[i] - g_moduleBaseVal);
            Log("CRASH: stack candidates (RVA, not a call chain):%s", line);
            if (n < 8) break;
            // continue the scan just above the last candidate's slot
            uint32_t a = esp, last = cand[7];
            __try {
                while (*(const uint32_t*)(uintptr_t)a != last) a += 4;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                break;
            }
            esp = a + 4;
        }
    }
    if (g_miniDump) {
        char path[MAX_PATH];
        BuildPath(path, "crash.dmp");
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = ep;
            mei.ClientPointers = FALSE;
            MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs |
                MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo);
            BOOL ok = g_miniDump(GetCurrentProcess(), GetCurrentProcessId(), h, type, &mei, nullptr, nullptr);
            CloseHandle(h);
            Log("CRASH: %s %s", ok ? "wrote" : "FAILED to write", path);
        } else {
            Log("CRASH: cannot create %s (error %lu)", path, GetLastError());
        }
    } else {
        Log("CRASH: no dbghelp, no dump");
    }
    bool flushed = FlushAll(true, 2000);
    if (flushed) {
        if (TryEnterCriticalSection(&g_flushCS)) { CloseAllOutputs(); LeaveCriticalSection(&g_flushCS); }
        Log("CRASH: capture files flushed and closed");
    } else {
        Log("CRASH: capture files NOT flushed (lock busy); the last periodic flush is on disk");
    }
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    if (InterlockedIncrement(&g_crashDepth) > 1) {   // re-entered through a chained filter
        InterlockedDecrement(&g_crashDepth);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedExchange(&g_crashed, 1) == 0) CrashCapture(ep);
    LONG r = EXCEPTION_CONTINUE_SEARCH;
    LPTOP_LEVEL_EXCEPTION_FILTER prev = g_prevUef;
    if (prev && prev != CrashFilter) r = prev(ep);
    InterlockedDecrement(&g_crashDepth);
    return r;
}

// Keeps our filter on top; a filter installed after ours becomes the one we
// chain to.
static void AssertCrashFilter() {
    LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(CrashFilter);
    if (cur != CrashFilter) {
        g_prevUef = cur;
        if (g_uefChangesLogged < 5) {
            g_uefChangesLogged++;
            Log("crash filter: another filter (0x%p) was installed after ours; ours is back on top and chains to it", cur);
        }
    }
}

// ---------------------------------------------------------------------------
// Dump thread: logs new unit cursors and units, polls F6..F10, flushes every
// 10 s, closes the window. Afterwards it keeps the crash filter on top until
// the process exits. The hooks stay installed (handlers return immediately
// once done); they are only removed at process detach.
// ---------------------------------------------------------------------------

static HANDLE g_dumpThread = nullptr;
static volatile LONG g_quit = 0;
static volatile LONG g_written = 0;
static const DWORD FLUSH_MS = 10000;

static void AnnounceNewCursors() {
    for (int i = 0; i < THIS_TABLE_SIZE; i++) {
        ThisState& st = g_this[i];
        if (st.thisPtr == 0 || st.announced) continue;
        st.announced = 1;
        Log("unit cursor address: this 0x%08X  P 0x%08X  P+0xA8 = 0x%08X  (node at first sight 0x%08X)",
            st.thisPtr, st.P, st.P ? st.P + 0xA8 : 0, st.nodeAtFirst);
    }
}

static void AnnounceUnits() {
    int n = g_unitsUsed;
    for (int i = 0; i < n; i++) {
        UnitState& u = g_units[i];
        if (u.announced || !u.firstDone) continue;
        u.announced = 1;
        const char* kind = !PtrPlausible(u.firstEnt) ? "null or unreadable" :
                           u.firstEnt == u.firstSoldier ? "the soldier itself" : "ANOTHER object";
        Log("unit 0x%08X (controller 0x%08X): %u soldiers; soldier[0] 0x%08X, [s+0x2F0] = 0x%08X (%s)",
            u.unit, u.firstCtrl, u.firstCount, u.firstSoldier, u.firstEnt, kind);
    }
}

static void CaptureSummaryLog() {
    Log("Capture complete: %d event records, %d cursor records, %d node dumps, %d markers",
        (int)min(g_eventIdx, (LONG)EVENT_SIZE), (int)min(g_cursorIdx, (LONG)CURSOR_SIZE),
        (int)min(g_dumpIdx, (LONG)DUMP_SIZE), (int)g_markerCount);
    Log("Soldier trace: %d order records, %d go-to records, %d samples, %d soldier rows, %d units",
        (int)min(g_orderIdx, (LONG)ORDERREC_SIZE), (int)min(g_gotoIdx, (LONG)GOTO_SIZE),
        (int)min(g_sampleIdx, (LONG)SAMPLE_SIZE), (int)min(g_srowIdx, (LONG)SROW_SIZE), g_unitsUsed);
    Log("Soldier rows by [s+0x2F0]: %ld the soldier itself, %ld another object, %ld null or unreadable",
        (long)g_entSelf, (long)g_entOther, (long)g_entBad);
    if (g_eventOverflow)  Log("WARNING: event buffer full, later events dropped");
    if (g_cursorOverflow) Log("WARNING: cursor buffer full, later cursor records dropped");
    if (g_dumpIdx > DUMP_SIZE) Log("WARNING: node dump slots exhausted (read_flags 16 marks the rows)");
    if (g_thisOverflow)   Log("WARNING: more than %d distinct FUN_101a9750 this pointers; extra ones not tracked", THIS_TABLE_SIZE);
    if (g_buildOverflow)   Log("WARNING: build buffer full, later builds dropped");
    if (g_nodeRowOverflow) Log("WARNING: node row buffer full, later node rows dropped");
    if (g_orderOverflow)  Log("WARNING: order buffer full, later order records dropped");
    if (g_poolOverflow)   Log("WARNING: soldier list pool full, later order records carry no list");
    if (g_gotoOverflow)   Log("WARNING: go-to buffer full, later go-to records dropped");
    if (g_sampleOverflow) Log("WARNING: sample buffer full, later samples dropped");
    if (g_srowOverflow)   Log("WARNING: soldier row buffer full, later samples carry no rows");
    if (g_unitOverflow)   Log("WARNING: more than %d units; extra ones not sampled", UNIT_TABLE);
    Log("Fix rows: %d (window only)", (int)min(g_fixRecIdx, (LONG)FIXREC_SIZE));
    if (g_fixRecOverflow) Log("WARNING: fix row buffer full, later fix rows dropped (the fix kept working)");
    if (g_faultCount) Log("First-chance faults inside empire.retail.dll: %ld (last %d in faults.csv)",
                          (long)g_faultCount, FAULT_RING);
}

static void WriteAllOnce() {
    if (InterlockedExchange(&g_written, 1) != 0) return;
    InterlockedExchange(&g_state, 3);
    WatchShutdown();   // disarm every thread before the buffers are read
    Sleep(200);        // let a handler that is mid-record finish it
    CaptureSummaryLog();
    if (!g_events || !g_cursor) {
        Log("No buffers, nothing to write.");
        return;
    }
    AnnounceNewCursors();
    AnnounceUnits();
    if (!FlushAll(true, 3000)) {
        Log("ERROR: flush lock busy for 3 s; files hold the last periodic flush");
        return;
    }
    if (TryEnterCriticalSection(&g_flushCS)) { CloseAllOutputs(); LeaveCriticalSection(&g_flushCS); }
    if (g_watch) {
        Log("Watch traps dropped: %ld inside this proxy DLL (own reads), %ld on a previous slot address",
            (long)g_watchSelf, (long)g_watchStale);
        if (g_watchOverflow) Log("WARNING: watch buffer full, later watch records dropped");
        if (g_eipTableFull)  Log("WARNING: more than %d EIPs on one slot; extra ones not counted", EIP_TABLE);
        if (g_watchForeign)  Log("NOTE: at least one single-step had our and a foreign Dr6 bit set together");
    }
    Log("DONE. Files are in %s (nothing to copy; Claude collects them).", LogRunDir());
}

// Marker, preset and watch rows in the events file (window open only).
static void AddControlEvent(uint8_t hook, uint16_t no, uint32_t a0, uint32_t a1, DWORD elapsed) {
    if (g_state != 2) return;
    LONG idx = InterlockedIncrement(&g_eventIdx) - 1;
    if (idx >= EVENT_SIZE) { InterlockedExchange(&g_eventOverflow, 1); return; }
    EventRecord& r = g_events[idx];
    r.hook = hook;
    r.reason = 0;
    r.markerNo = no;
    r.seq = (uint32_t)(InterlockedIncrement(&g_seq) - 1);
    r.timestamp = elapsed;
    r.threadId = GetCurrentThreadId();
    r.ecx = 0;
    memset(r.args, 0, sizeof(r.args));
    r.args[0] = a0;
    r.args[1] = a1;
    r.ecx60 = 0;
    r.ecx60ok = 0;
    r.retAbs = 0;
    r.hitNo = 0;
    InterlockedExchange(&r.valid, 1);
}

static void AddMarker(DWORD elapsed) {
    LONG no = InterlockedIncrement(&g_markerCount);
    AddControlEvent(HOOK_MARKER, (uint16_t)no, 0, 0, elapsed);
    Log("O: marker %d at %.1f s", (int)no, elapsed / 1000.0);
}

// ---------------------------------------------------------------------------
// v42 recording in play (no PATHFIX_CAPTURE): P starts a recording into a new run folder, P
// stops it; any number of recordings per session. The record writers stay behind WindowOpen,
// which is open only between RecStart and RecStop. Each recording starts from empty buffers
// (timestamp_ms 0 = the P press, logged with the local time in the folder's pathfix_log.txt).
// ---------------------------------------------------------------------------
static bool AllocCaptureBuffers();   // with Pathfix_Install, below

template <typename R>
static void ClearUsed(R* buf, volatile LONG* idx, LONG size) {
    LONG n = *idx;
    if (n > size) n = size;
    if (buf && n > 0) memset((void*)buf, 0, sizeof(R) * (size_t)n);
    InterlockedExchange(idx, 0);
}

static void OutReset(OutFile& o) {
    if (o.f) { fclose(o.f); o.f = nullptr; }
    o.next = 0; o.written = 0; o.lost = 0; o.failed = false; o.closed = false;
}

// After RecStop has flushed and closed everything: empty buffers, files and per-recording
// counters for the next recording. Session counters (fix totals, hits since install) stay.
static void ResetCaptureState() {
    ClearUsed(g_events, &g_eventIdx, (LONG)EVENT_SIZE);
    ClearUsed(g_cursor, &g_cursorIdx, (LONG)CURSOR_SIZE);
    ClearUsed(g_dumps, &g_dumpIdx, (LONG)DUMP_SIZE);
    ClearUsed(g_builds, &g_buildIdx, (LONG)BUILD_SIZE);
    ClearUsed(g_nodeRows, &g_nodeRowIdx, (LONG)NODEROW_SIZE);
    ClearUsed(g_orders, &g_orderIdx, (LONG)ORDERREC_SIZE);
    ClearUsed(g_soldierPool, &g_poolIdx, (LONG)POOL_SIZE);
    ClearUsed(g_gotos, &g_gotoIdx, (LONG)GOTO_SIZE);
    ClearUsed(g_samples, &g_sampleIdx, (LONG)SAMPLE_SIZE);
    ClearUsed(g_srows, &g_srowIdx, (LONG)SROW_SIZE);
    ClearUsed(g_fixRecs, &g_fixRecIdx, (LONG)FIXREC_SIZE);
    ClearUsed(g_fixRaw, &g_fixRawIdx, (LONG)FIXRAW_SIZE);
    ClearUsed(g_avRecs, &g_avRecIdx, (LONG)AVREC_SIZE);
    ClearUsed(g_rqRecs, &g_rqRecIdx, (LONG)RQREC_SIZE);
    ClearUsed(g_relRecs, &g_relRecIdx, (LONG)RELREC_SIZE);
    ClearUsed(g_watch, &g_watchIdx, (LONG)WATCH_SIZE);
    ClearUsed(g_slotEv, &g_slotEvIdx, (LONG)SLOTEV_SIZE);
    InterlockedExchange(&g_sweepIdx, 0);
    volatile LONG* flags[] = { &g_eventOverflow, &g_cursorOverflow, &g_buildOverflow, &g_nodeRowOverflow,
        &g_orderOverflow, &g_poolOverflow, &g_gotoOverflow, &g_sampleOverflow, &g_srowOverflow, &g_fixRecOverflow,
        &g_avRecOverflow, &g_rqRecOverflow, &g_relRecOverflow, &g_watchOverflow, &g_slotEvOverflow,
        &g_seq, &g_markerCount, &g_entSelf, &g_entOther, &g_entBad };
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) InterlockedExchange(flags[i], 0);
    OutFile* files[] = { &g_outEvents, &g_outCursor, &g_outWatch, &g_outSlots, &g_outSweeps, &g_outBuilds,
        &g_outNodes, &g_outOrders, &g_outGotos, &g_outSoldiers, &g_outFix, &g_outRadius, &g_outAvoid,
        &g_outRelease, &g_outFixRaw };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) OutReset(*files[i]);
    for (int i = 0; i < THIS_TABLE_SIZE; i++) InterlockedExchange(&g_this[i].announced, 0);
    for (int i = 0; i < UNIT_TABLE; i++) InterlockedExchange(&g_units[i].announced, 0);
    for (int h = 0; h < HOOK_COUNT; h++) {
        SpinLock(&g_rate[h].lock);
        g_rate[h].windowHits = 0;
        g_rate[h].recorded = 0;
        SpinUnlock(&g_rate[h].lock);
    }
}

static void RecStart() {
    if (g_recActive) return;
    if (!AllocCaptureBuffers()) {
        Log("P refused: cannot allocate the recording buffers; nothing is recorded (ALARM)");
        FixBeep(3);
        return;
    }
    char folder[MAX_PATH];
    if (!LogRecordingStart(g_recNumber + 1, folder, sizeof(folder))) {
        Log("P refused: cannot create a recording folder in pathfix_runs (ALARM)");
        FixBeep(3);
        return;
    }
    g_recNumber++;
    Log("PATHFIX BUILD v42 recording %d, build id %s %s", g_recNumber, __DATE__, __TIME__);
    Log("P: recording %d ON (path fix %s) -> %s", g_recNumber,
        g_fixMode == FIX_V38_G ? "ON, variant G" : g_fixMode == FIX_OFF ? "OFF" : kFixModeText[g_fixMode], folder);
    g_captureStart = GetTickCount();
    g_captureEnd = 0xFFFFFFFFu;
    InterlockedExchange(&g_recActive, 1);
    InterlockedExchange(&g_state, 2);
    AddControlEvent(HOOK_FIXMODE, (uint16_t)g_fixMode, (uint32_t)g_fixVariant, 0, 0);   // the fix mode at t = 0
    FixBeep(5);
}

// atExit: called from Pathfix_Remove (process detach): no tone, short grace period.
static void RecStop(bool atExit) {
    if (InterlockedCompareExchange(&g_recActive, 0, 1) != 1) return;
    InterlockedExchange(&g_state, 1);      // writers see the window closed
    DWORD elapsed = GetTickCount() - g_captureStart;
    Sleep(atExit ? 50 : 200);              // let a handler that is mid-record finish it
    Log("P: recording %d OFF after %.1f s%s", g_recNumber, elapsed / 1000.0, atExit ? " (game closed)" : "");
    CaptureSummaryLog();
    AnnounceNewCursors();
    AnnounceUnits();
    if (FlushAll(true, 3000)) {
        if (TryEnterCriticalSection(&g_flushCS)) { CloseAllOutputs(); LeaveCriticalSection(&g_flushCS); }
    } else {
        Log("ERROR: flush lock busy for 3 s; the files hold the last periodic flush");
    }
    Log("RECORDING STOPPED: recording %d, files in %s", g_recNumber, LogRunDir());
    LogRecordingStop();
    ResetCaptureState();
    if (!atExit) FixBeep(6);
}

// Key functions (v19). Real keys: I = fix on / off, O = marker, P = finish;
// the lane mode and the window extension have no real key. Offline harness
// only (tools/hooktest19.cpp): functions pressed through this export instead
// of real keystrokes; bit k = function k. The game never calls it.
enum { KEY_FIX = 0, KEY_LANE = 1, KEY_MARK = 2, KEY_MORE = 3, KEY_FINISH = 4, KEY_FUNCS = 5 };
static const int kKeyVk[KEY_FUNCS] = { 'I', 0, 'O', 0, 'P' };
static volatile LONG g_testKeys = 0;
extern "C" __declspec(dllexport) void __stdcall PathfixTestKeys(LONG bits) {
    InterlockedExchange(&g_testKeys, bits);
}

// Real keys count only while a window of this process (the game) has the
// focus: GetAsyncKeyState also sees keys typed into other programs
// (Keys_RealDown, proxy/keys.h).
static bool KeyDown(int fn) {
    if (fn < 0 || fn >= KEY_FUNCS) return false;
    if ((g_testKeys >> fn) & 1) return true;
    return Keys_RealDown(kKeyVk[fn]);
}

static bool KeyEdge(int fn, bool* down) {
    return Keys_Edge(KeyDown(fn), down);
}

// v42 play (no PATHFIX_CAPTURE): I fix on / off, P recording on / off, O marker while recording.
static void PlayLoop() {
    bool iDown = false, lDown = false, pDown = false, oDown = false;
    DWORD lastUef = 0, lastFlush = 0, lastLog = 0, lastTotals = GetTickCount();
    LONG lastCalls = g_fixCalls;
    while (!g_quit) {
        Sleep(50);
        if (g_layoutBad) {
            RecStop(false);
            Log("ERROR: stack layout self-check failed (saved ESP != address of return slot).");
            Log("       The fix does not run and nothing more is recorded. Regenerate the stack offset headers");
            Log("       with tools/re/stack_calc.py.");
            return;
        }
        DWORD tick = GetTickCount();
        if (tick - lastUef >= 1000) { lastUef = tick; AssertCrashFilter(); }
        DWORD elapsed = g_recActive ? tick - g_captureStart : 0;
        if (KeyEdge(KEY_FIX, &iDown)) FixKeyF6(elapsed);
        FixReportDisabled(elapsed);
        if (KeyEdge(KEY_LANE, &lDown)) FixKeyF7(elapsed);   // harness only
        if (KeyEdge(KEY_FINISH, &pDown)) {
            if (g_recActive) RecStop(false);
            else { RecStart(); lastFlush = 0; lastLog = 0; }
            continue;
        }
        bool oEdge = KeyEdge(KEY_MARK, &oDown);
        if (!g_recActive) {
            if (oEdge) Log("O ignored: not recording (P starts a recording)");
            if (tick - lastTotals >= 60000) {
                lastTotals = tick;
                if (g_fixCalls != lastCalls) { lastCalls = g_fixCalls; WriteFixTotals(true); }
            }
            continue;
        }
        AnnounceNewCursors();
        AnnounceUnits();
        if (oEdge) AddMarker(elapsed);
        if (elapsed - lastFlush >= FLUSH_MS) {
            lastFlush = elapsed;
            FlushAll(false, 1000);
        }
        if (elapsed - lastLog >= 10000) {
            lastLog = elapsed;
            Log("Recording %d... %d s, %d orders, %d go-to, %d samples / %d soldier rows, %d fix rows, %d release rows, "
                "fix mode %d%s", g_recNumber, (int)(elapsed / 1000), (int)g_orderIdx, (int)g_gotoIdx, (int)g_sampleIdx,
                (int)g_srowIdx, (int)g_fixRecIdx, (int)g_relRecIdx, (int)g_fixMode, g_fixDisabled ? " DISABLED" : "");
        }
    }
}

static DWORD WINAPI DumpThread(LPVOID) {
    if (!g_captureOn) { PlayLoop(); return 0; }   // v42
    bool announced = false;
    bool f6Down = false, f7Down = false, f8Down = false, f9Down = false;
    DWORD lastLog = 0, lastFlush = 0, lastUef = 0;

    while (g_state != 3 && !g_quit) {
        Sleep(50);
        if (g_layoutBad) break;
        DWORD tick = GetTickCount();
        if (tick - lastUef >= 1000) { lastUef = tick; AssertCrashFilter(); }

        DWORD elapsed = g_state == 2 ? tick - g_captureStart : 0;
        if (KeyEdge(KEY_FIX, &f6Down)) FixKeyF6(elapsed);
        FixReportDisabled(elapsed);
        if (KeyEdge(KEY_LANE, &f7Down)) FixKeyF7(elapsed);   // harness only
        bool f8Edge = g_captureOn && KeyEdge(KEY_MARK, &f8Down);   // v41: O and P only in capture mode
        if (g_state != 2) {
            if (f8Edge) Log("O ignored: the capture window is not open yet (no hook has fired)");
            continue;
        }

        if (!announced) {
            announced = true;
            lastFlush = elapsed;
            Log("CAPTURE WINDOW OPEN: first hook hit seen. %d s window. I = corner fix on/off, "
                "O = marker, P = finish now. Sound check: one low tone.", (int)(CAPTURE_MS / 1000));
            FixBeep(0);
        }
        AnnounceNewCursors();
        AnnounceUnits();
        if (f8Edge) AddMarker(elapsed);

        if (KeyEdge(KEY_MORE, &f9Down)) {   // harness only
            g_captureEnd = g_captureEnd + CAPTURE_MS;
            Log("More: window extended, now closes at %d s", (int)(g_captureEnd / 1000));
        }
        if (KeyDown(KEY_FINISH)) {
            Log("P: finishing capture at %d s", (int)(elapsed / 1000));
            FixBeep(g_fixOn ? 1 : 0);
            InterlockedExchange(&g_state, 3);
            break;
        }
        // The window must also close when no hook fires any more.
        if (elapsed > g_captureEnd) {
            Log("Window closed at %d s", (int)(elapsed / 1000));
            InterlockedExchange(&g_state, 3);
            break;
        }
        if (elapsed - lastFlush >= FLUSH_MS) {
            lastFlush = elapsed;
            FlushAll(false, 1000);
        }
        if (elapsed - lastLog >= 10000) {
            lastLog = elapsed;
            Log("Capturing... %d events, %d cursor, %d builds, %d orders, %d go-to, %d samples / %d soldier rows, "
                "%d fix rows, fix mode %d%s (%ld apex, %ld adjusted), watch %s, %d/%d s",
                (int)g_eventIdx, (int)g_cursorIdx, (int)g_buildIdx, (int)g_orderIdx, (int)g_gotoIdx,
                (int)g_sampleIdx, (int)g_srowIdx, (int)g_fixRecIdx, (int)g_fixMode,
                g_fixDisabled ? " DISABLED" : "", (long)g_fixApex, (long)g_fixStatus[FS_ADJUSTED],
                g_watchEnabled ? "on" : "off", (int)(elapsed / 1000), (int)(g_captureEnd / 1000));
        }
    }

    if (g_layoutBad) {
        Log("ERROR: stack layout self-check failed (saved ESP != address of return slot).");
        Log("       Nothing was recorded and the fix does not run. Regenerate src/stack_offsets_v16.h /");
        Log("       src/stack_offsets_v17.h with stack_calc.py.");
        InterlockedExchange(&g_state, 3);
        return 0;
    }

    if (g_quit) return 0;   // process is going away: RemoveHooks writes the files
    WriteAllOnce();
    // After the window: the fix keeps running and F6 keeps switching it; the
    // crash filter stays on top until the process ends. Nothing more is
    // recorded; the fix counters go to the log once a minute when they move.
    DWORD lastTotals = GetTickCount(), lastF = 0;
    LONG lastCalls = g_fixCalls;
    f6Down = false;
    f7Down = false;
    while (!g_quit) {
        Sleep(50);
        DWORD tick = GetTickCount();
        if (tick - lastF >= 1000) { lastF = tick; AssertCrashFilter(); }
        if (KeyEdge(KEY_FIX, &f6Down)) FixKeyF6(0);
        if (KeyEdge(KEY_LANE, &f7Down)) FixKeyF7(0);
        FixReportDisabled(0);
        if (tick - lastTotals >= 60000) {
            lastTotals = tick;
            if (g_fixCalls != lastCalls) { lastCalls = g_fixCalls; WriteFixTotals(true); }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static void HexBytes(char* out, const uint8_t* p, int n) {
    out[0] = 0;
    for (int i = 0; i < n; i++) sprintf(out + strlen(out), i ? " %02X" : "%02X", p[i]);
}

template <typename T>
static T* AllocRecords(int n) {
    return (T*)VirtualAlloc(nullptr, sizeof(T) * (size_t)n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

// Capture buffers: at install in capture mode, at the first P in play (v42). Kept for the session.
static bool AllocCaptureBuffers() {
    if (g_recBuffers) return true;
    if (!g_events) g_events = AllocRecords<EventRecord>(EVENT_SIZE);
    if (!g_cursor) g_cursor = AllocRecords<CursorRecord>(CURSOR_SIZE);
    if (!g_dumps) g_dumps = AllocRecords<DumpSlot>(DUMP_SIZE);
    if (!g_builds) g_builds = AllocRecords<BuildRecord>(BUILD_SIZE);
    if (!g_nodeRows) g_nodeRows = AllocRecords<NodeRow>(NODEROW_SIZE);
    if (!g_orders) g_orders = AllocRecords<OrderRecord>(ORDERREC_SIZE);
    if (!g_soldierPool) g_soldierPool = AllocRecords<uint32_t>(POOL_SIZE);
    if (!g_gotos) g_gotos = AllocRecords<GotoRecord>(GOTO_SIZE);
    if (!g_samples) g_samples = AllocRecords<SampleRecord>(SAMPLE_SIZE);
    if (!g_srows) g_srows = AllocRecords<SoldierRow>(SROW_SIZE);
    if (!g_fixRecs) g_fixRecs = AllocRecords<FixRecord>(FIXREC_SIZE);
    if (!g_avRecs) g_avRecs = AllocRecords<AvoidRecord>(AVREC_SIZE);
    if (!g_rqRecs) g_rqRecs = AllocRecords<RadiusRecord>(RQREC_SIZE);
    if (!g_relRecs) g_relRecs = AllocRecords<ReleaseRecord>(RELREC_SIZE);   // v38; null = no release rows (G still acts)
    if (!g_events || !g_cursor || !g_dumps || !g_builds || !g_nodeRows ||
        !g_orders || !g_soldierPool || !g_gotos || !g_samples || !g_srows || !g_fixRecs)
        return false;
    g_recBuffers = true;
    size_t total = sizeof(EventRecord) * EVENT_SIZE + sizeof(CursorRecord) * CURSOR_SIZE +
                   sizeof(DumpSlot) * DUMP_SIZE + sizeof(BuildRecord) * BUILD_SIZE +
                   sizeof(NodeRow) * NODEROW_SIZE + sizeof(OrderRecord) * ORDERREC_SIZE +
                   sizeof(uint32_t) * POOL_SIZE + sizeof(GotoRecord) * GOTO_SIZE +
                   sizeof(SampleRecord) * SAMPLE_SIZE + sizeof(SoldierRow) * SROW_SIZE +
                   sizeof(FixRecord) * FIXREC_SIZE + sizeof(FixSoldier) * FIX_SOLDIER_TABLE;
    Log("  Buffers:     %d events, %d cursor, %d node dumps, %d builds, %d node rows", EVENT_SIZE,
        CURSOR_SIZE, DUMP_SIZE, BUILD_SIZE, NODEROW_SIZE);
    Log("               %d orders, %d go-to, %d samples, %d soldier rows (%d MB total before the watch)",
        ORDERREC_SIZE, GOTO_SIZE, SAMPLE_SIZE, SROW_SIZE, (int)(total / 1024 / 1024));
    Log("  Rate limit:  group 1 and builder: first %d hits after %d ms silence, else %d records/s per hook",
        BURST_HITS, (int)SILENCE_MS, PER_SECOND);
    Log("  Soldiers:    per unit every %d ms while any soldier moves, else every %d ms; up to %d soldiers",
        (int)SAMPLE_MOVING_MS, (int)SAMPLE_IDLE_MS, MAX_SOLDIERS);
    Log("  Output:      record files appended every %d s; summaries rewritten (tmp + rename)", (int)(FLUSH_MS / 1000));
    return true;
}

void Pathfix_Install() {   // registered in proxy/fixes.cpp (was InstallHooks in hooks_v40.cpp)
    Log("------------------------------------------------------------");
    g_captureOn = LogCaptureMode();
    Log("PATHFIX BUILD v42 (variant G = release beyond the choke + park release + radius lanes + lead the turn + "
        "no crowd damping; I toggles the path fix on / off, off at launch; P toggles a recording), build id %s %s",
        __DATE__, __TIME__);
    Log("------------------------------------------------------------");
    Log("  Capture:     %s", g_captureOn ? "ON (PATHFIX_CAPTURE=1): v40 capture, run folder, O marker, P finish"
                                         : "OFF at launch: P starts / stops a recording (v42)");
    Log("  Folder:      %s", LogRunDir());
    Mem_Init();
    Log("  Address:     user mode up to 0x%08X (pointer checks use this)", Mem_UserMax());

    uint8_t* base = Hook_GameModule(false, &g_moduleSize);
    if (!base) {
        Log("REFUSED: empire.retail.dll is not loaded in this process. No hooks installed.");
        return;
    }
    g_moduleBaseVal = (uint32_t)(uintptr_t)base;
    uint32_t delta = Hook_RebaseDelta(base);
    Log("  Module:      empire.retail.dll at 0x%08X, size 0x%08X (rebase delta 0x%08X)",
        g_moduleBaseVal, g_moduleSize, delta);

    // Byte-check every target before touching any. An absolute address inside
    // the stolen bytes is compared after adding the rebase delta (Hook_Expected).
    bool allMatch = true;
    for (int h = 0; h < HOOK_COUNT; h++) {
        const HookDef& d = g_hookDefs[h];
        uint8_t* target = base + d.rva;
        uint8_t expect[HOOK_MAX_STOLEN];
        Hook_Expected(d, delta, expect);
        bool match = memcmp(target, expect, d.len) == 0;
        char seen[64], want[64];
        HexBytes(seen, target, d.len);
        HexBytes(want, expect, d.len);
        Log("  Hook %d %s  %s", h, d.func, d.role);
        Log("    RVA 0x%06X  byte check: %s  (%s)%s", d.rva, match ? "PASS" : "FAIL", seen,
            d.relocOff >= 0 ? "  [contains a rebased address]" : "");
        if (!match) {
            Log("    expected  %s", want);
            allMatch = false;
        }
    }
    if (!allMatch) {
        Log("REFUSED: prologue bytes differ from the analysed binary. No hooks installed.");
        return;
    }

    // v41: without capture only the fix's own tables are allocated (every record writer is
    // behind WindowOpen, which never opens, or checks its buffer for null).
    g_fixSoldiers = AllocRecords<FixSoldier>(FIX_SOLDIER_TABLE);
    g_avUnits = AllocRecords<AvUnit>(AV_UNIT_TABLE);       // v33; null = avoidance inert
    if (!g_fixSoldiers) {
        Log("FATAL: cannot allocate the fix tables. No hooks installed.");
        return;
    }
    if (g_captureOn) {
        if (!AllocCaptureBuffers()) {
            Log("FATAL: cannot allocate capture buffers. No hooks installed.");
            return;
        }
        Log("  Window:      %d seconds after the first hit of any hook", (int)(CAPTURE_MS / 1000));
    } else {
        Log("  Recording:   P starts a recording into a new folder pathfix_runs\\<date_time>\\ and P stops it, "
            "any number of times (buffers allocated at the first P); O marks a moment while recording");
    }

    InitializeCriticalSection(&g_flushCS);
    InterlockedExchange(&g_flushInit, 1);

    // Build all trampolines first: stolen bytes (as they are in memory, i.e.
    // already rebased) + JMP back.
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, nullptr, g_trampolines, g_hookSlots)) {
        Log("FATAL: trampoline alloc failed. No hooks installed.");
        return;
    }

    g_gCallSite = (uint32_t)(uintptr_t)(base + G_CALL_SITE_RVA);   // v38
    FixTonesInit();
    FixInstallLog();
    FixEndpointsInit(base);
    Log("  Builder:     FUN_1039bb40 list walk, up to %d nodes, 0x%X bytes each; %d builds / %d node rows",
        WALK_LIMIT, NODE_BYTES, BUILD_SIZE, NODEROW_SIZE);

    // Armed before patching, so a hit right after the patch is recorded.
    InterlockedExchange(&g_state, 1);

    Hook_Patch(g_detours, g_hookSlots, HOOK_COUNT);

    WatchInstall(base);
    if (!g_selfStart) {   // WatchInstall failed before finding our own range
        HMODULE selfMod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&FaultVeh, &selfMod);
        if (selfMod) {
            const IMAGE_DOS_HEADER* sd = (const IMAGE_DOS_HEADER*)selfMod;
            const IMAGE_NT_HEADERS32* sn = (const IMAGE_NT_HEADERS32*)((const uint8_t*)selfMod + sd->e_lfanew);
            g_selfStart = (uint32_t)(uintptr_t)selfMod;
            g_selfEnd = g_selfStart + sn->OptionalHeader.SizeOfImage;
        }
    }

    // Crash capture.
    char sysDir[MAX_PATH], dbgPath[MAX_PATH + 16];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    snprintf(dbgPath, sizeof(dbgPath), "%s\\dbghelp.dll", sysDir);
    HMODULE dbg = LoadLibraryA(dbgPath);
    if (dbg) g_miniDump = (MiniDumpWriteDumpFn)GetProcAddress(dbg, "MiniDumpWriteDump");
    g_faultVeh = AddVectoredExceptionHandler(1, FaultVeh);
    g_prevUef = SetUnhandledExceptionFilter(CrashFilter);
    Log("  Crash:       filter installed (previous 0x%p, chained; re-asserted every second), %s, "
        "first-chance faults in empire.retail.dll %s",
        g_prevUef, g_miniDump ? "crash.dmp via dbghelp" : "NO dbghelp (no dump)",
        g_faultVeh ? "recorded (last 256)" : "NOT recorded");

    // Unattended runs (tools/autorun --env PATHFIX_ON=1): start with variant G on instead of off,
    // as one I press would (the other fixes' interaction tests need pathfix on without a key).
    {
        char on[8] = "";
        if (GetEnvironmentVariableA("PATHFIX_ON", on, sizeof(on)) > 0 && on[0] == '1' && !g_fixDisabled) {
            InterlockedExchange(&g_fixVariant, 3);
            InterlockedExchange(&g_fixOn, 1);
            InterlockedExchange(&g_fixMode, FIX_V38_G);
            Log("PATHFIX_ON=1: path fix ON at launch, variant G (mode %d)", (int)FIX_V38_G);
        }
    }

    if (g_captureOn) {
        Log("All %d hooks installed. The window opens at the first hit of any hook.", HOOK_COUNT);
        Log("Keys (only while the game has the focus): I path fix on/off (off at launch), O marker, "
            "P finish. Window %d s.", (int)(CAPTURE_MS / 1000));
    } else {
        Log("All %d hooks installed. Path fix OFF, not recording.", HOOK_COUNT);
        Log("Keys (only while the game has the focus): I path fix on / off at any time (one high tone on, "
            "one low tone off; three long low tones = disabled by a self-check); P recording on / off at any time "
            "(three rising notes on, three falling notes off; three long low tones = refused); O marker while "
            "recording.");
    }
    Log("");

    g_dumpThread = CreateThread(nullptr, 0, DumpThread, nullptr, 0, nullptr);
}

void Pathfix_Remove() {    // registered in proxy/fixes.cpp (was RemoveHooks in hooks_v40.cpp)
    InterlockedExchange(&g_quit, 1);

    Hook_Remove(g_hookSlots, HOOK_COUNT);

    if (g_dumpThread) {
        WaitForSingleObject(g_dumpThread, 2000);
        CloseHandle(g_dumpThread);
    }

    // If the game is closed before the window ends, the dump thread is already
    // gone (process exit kills it before DLL detach), so write here instead.
    if (g_recActive) RecStop(true);   // v42: a recording still on when the game closes
    if (g_captureOn && !g_layoutBad && g_state != 0) WriteAllOnce();
    else if (g_layoutBad) Log("Layout self-check had failed; nothing written.");
    WatchShutdown();   // no-op if WriteAllOnce already ran it
    if (g_faultVeh) { RemoveVectoredExceptionHandler(g_faultVeh); g_faultVeh = nullptr; }
    if (g_prevUef != CrashFilter) SetUnhandledExceptionFilter(g_prevUef);

    Log("SHUTDOWN");
}
